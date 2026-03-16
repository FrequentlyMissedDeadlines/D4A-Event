#!/usr/bin/env python3
"""
WebSocket Bridge Test Script for K10 Bot

Tests the WebSocket UDP bridge functionality including:
- Connection establishment
- Master registration/unregistration
- Servo angle control (0x21)
- Servo speed control (0x22)
- Stop servos (0x23)
- Heartbeat (0x43)
- Ping/latency measurement (0x44)

Usage:
    python3 test_websocket_bridge.py <robot_ip> [master_token] [--verbose]

Examples:
    python3 test_websocket_bridge.py 192.168.1.100
    python3 test_websocket_bridge.py 192.168.1.100 "my-token-1234" --verbose
    python3 test_websocket_bridge.py localhost 0 --verbose
"""

import asyncio
import websockets
import struct
import sys
import time
from datetime import datetime
from typing import Tuple, Optional, List
import argparse


# ─── Constants ────────────────────────────────────────────────────────────────

# UDP Command Codes
UDP_COMMANDS = {
    'MASTER_REGISTER': 0x41,
    'MASTER_UNREGISTER': 0x42,
    'HEARTBEAT': 0x43,
    'PING': 0x44,
    'SET_SERVO_ANGLE': 0x21,
    'SET_SERVO_SPEED': 0x22,
    'STOP_SERVOS': 0x23,
}

# Status Codes in Responses
RESPONSE_STATUS = {
    0x00: 'SUCCESS',
    0x01: 'IGNORED (not master)',
    0x02: 'DENIED',
    0x03: 'ERROR',
}

# Color output
class Color:
    HEADER = '\033[95m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'


# ─── Utility Functions ─────────────────────────────────────────────────────────

def print_section(title: str):
    """Print a test section header"""
    print(f"\n{Color.BOLD}{Color.CYAN}{'─' * 70}")
    print(f"  {title}")
    print(f"{'─' * 70}{Color.ENDC}\n")


def print_test(name: str, status: bool, message: str = "", latency_ms: float = 0):
    """Print test result with status"""
    status_icon = f"{Color.GREEN}✓ PASS{Color.ENDC}" if status else f"{Color.RED}✗ FAIL{Color.ENDC}"
    latency_str = f" ({latency_ms:.1f}ms)" if latency_ms > 0 else ""
    print(f"  {status_icon}  {name}{latency_str}")
    if message:
        print(f"        {Color.YELLOW}→ {message}{Color.ENDC}")


def print_info(message: str):
    """Print informational message"""
    print(f"  {Color.BLUE}ℹ {message}{Color.ENDC}")


def print_error(message: str):
    """Print error message"""
    print(f"  {Color.RED}✗ {message}{Color.ENDC}")


def bytes_to_hex(data: bytes) -> str:
    """Convert bytes to hex string"""
    return ' '.join(f'{b:02X}' for b in data)


def decode_response(data: bytes) -> Tuple[int, str]:
    """Decode response packet
    
    Returns:
        (command_code, status_message)
    """
    if len(data) < 2:
        return 0, "Invalid response (too short)"
    
    cmd_code = data[0]
    status = data[1] if len(data) > 1 else 0
    status_msg = RESPONSE_STATUS.get(status, f"Unknown status 0x{status:02X}")
    
    return cmd_code, status_msg


# ─── WebSocket Test Client ────────────────────────────────────────────────────

class WebSocketBridgeTester:
    """Test client for WebSocket bridge"""
    
    def __init__(self, robot_ip: str, master_token: Optional[str] = None, verbose: bool = False):
        self.robot_ip = robot_ip
        self.master_token = master_token or "0"
        self.verbose = verbose
        self.ws = None
        self.is_connected = False
        self.is_master = False
        self.latencies: List[float] = []
        self.ping_sequence = 0
    
    async def connect(self) -> bool:
        """Connect to WebSocket bridge"""
        ws_url = f"ws://{self.robot_ip}:80/ws"
        
        try:
            print_info(f"Connecting to {ws_url}...")
            self.ws = await websockets.connect(ws_url, ping_interval=None)
            self.is_connected = True
            print_test("Connection established", True)
            return True
        except Exception as e:
            print_error(f"Failed to connect: {e}")
            return False
    
    async def disconnect(self):
        """Disconnect from WebSocket"""
        if self.ws:
            await self.ws.close()
            self.is_connected = False
    
    async def send_command(self, cmd_name: str, data: bytes) -> Optional[bytes]:
        """Send command and receive response
        
        Args:
            cmd_name: Command name for logging
            data: Packet bytes to send
            
        Returns:
            Response bytes or None if failed
        """
        if not self.is_connected or not self.ws:
            print_error("WebSocket not connected")
            return None
        
        try:
            start_time = time.time()
            
            if self.verbose:
                print_info(f"Sending {cmd_name}: {bytes_to_hex(data)}")
            
            await self.ws.send(data)
            response = await asyncio.wait_for(self.ws.recv(), timeout=2.0)
            
            latency_ms = (time.time() - start_time) * 1000
            self.latencies.append(latency_ms)
            
            if self.verbose:
                print_info(f"Received response ({latency_ms:.1f}ms): {bytes_to_hex(response)}")
            
            return response, latency_ms
            
        except asyncio.TimeoutError:
            print_error(f"{cmd_name} timed out (no response in 2s)")
            return None
        except Exception as e:
            print_error(f"{cmd_name} failed: {e}")
            return None
    
    async def test_connection(self):
        """Test 1: Basic connection"""
        print_section("TEST 1: Connection")
        if self.is_connected:
            print_test("WebSocket connected", True)
            return True
        return False
    
    async def test_master_registration(self):
        """Test 2: Master registration"""
        print_section("TEST 2: Master Registration")
        
        # Build registration packet: 0x41 + 4-byte token
        token_value = int(self.master_token) if self.master_token.isdigit() else 0
        token_bytes = struct.pack('<I', token_value)
        packet = bytes([UDP_COMMANDS['MASTER_REGISTER']]) + token_bytes
        
        response_data = await self.send_command("MASTER_REGISTER", packet)
        if not response_data:
            print_test("Master registration", False, "No response")
            return False
        
        response, latency_ms = response_data
        cmd_code, status_msg = decode_response(response)
        
        success = len(response) >= 2 and response[1] == 0x00
        print_test("Master registration", success, status_msg, latency_ms)
        
        self.is_master = success
        return success
    
    async def test_heartbeat(self):
        """Test 3: Heartbeat"""
        print_section("TEST 3: Heartbeat")
        
        if not self.is_master:
            print_test("Heartbeat", False, "Not registered as master")
            return False
        
        packet = bytes([UDP_COMMANDS['HEARTBEAT']])
        response_data = await self.send_command("HEARTBEAT", packet)
        
        if not response_data:
            print_test("Heartbeat", False, "No response")
            return False
        
        response, latency_ms = response_data
        cmd_code, status_msg = decode_response(response)
        
        success = len(response) >= 2 and response[1] == 0x00
        print_test("Heartbeat", success, status_msg, latency_ms)
        return success
    
    async def test_ping(self, count: int = 3):
        """Test 4: Ping/Latency measurement"""
        print_section(f"TEST 4: Ping ({count} samples)")
        
        if not self.is_master:
            print_test("Ping", False, "Not registered as master")
            return False
        
        latencies = []
        successes = 0
        
        for i in range(count):
            self.ping_sequence += 1
            packet = bytes([UDP_COMMANDS['PING'], self.ping_sequence & 0xFF])
            
            response_data = await self.send_command(f"PING {i+1}/{count}", packet)
            if not response_data:
                print_test(f"Ping {i+1}/{count}", False, "No response")
                continue
            
            response, latency_ms = response_data
            latencies.append(latency_ms)
            
            # Verify response
            success = (len(response) >= 2 and 
                      response[0] == UDP_COMMANDS['PING'] and
                      response[1] == self.ping_sequence)
            
            print_test(f"Ping {i+1}/{count}", success, f"{latency_ms:.1f}ms", latency_ms)
            if success:
                successes += 1
            
            await asyncio.sleep(0.1)  # Small delay between pings
        
        if latencies:
            avg_latency = sum(latencies) / len(latencies)
            min_latency = min(latencies)
            max_latency = max(latencies)
            print_info(f"Latency - Min: {min_latency:.1f}ms, Max: {max_latency:.1f}ms, Avg: {avg_latency:.1f}ms")
        
        return successes == count
    
    async def test_set_servo_angles(self):
        """Test 5: Set servo angles (0x21)"""
        print_section("TEST 5: Set Servo Angles")
        
        if not self.is_master:
            print_test("Set servo angles", False, "Not registered as master")
            return False
        
        # Build packet: 0x21 + (channel, angle) pairs
        # Set angles: ch0=45°, ch1=-30°, ch2=60°, ch3=-90°
        angles = [
            (0, 45),
            (1, -30),
            (2, 60),
            (3, -90)
        ]
        
        packet = bytearray([UDP_COMMANDS['SET_SERVO_ANGLE']])
        for ch, angle in angles:
            packet.append(ch)
            packet.append(angle & 0xFF)
        
        response_data = await self.send_command("SET_SERVO_ANGLE", bytes(packet))
        if not response_data:
            print_test("Set servo angles", False, "No response")
            return False
        
        response, latency_ms = response_data
        cmd_code, status_msg = decode_response(response)
        
        success = len(response) >= 2 and response[1] == 0x00
        angle_str = ", ".join([f"ch{ch}={angle}°" for ch, angle in angles])
        print_test("Set servo angles", success, f"{angle_str} → {status_msg}", latency_ms)
        return success
    
    async def test_set_servo_speeds(self):
        """Test 6: Set servo speeds (0x22)"""
        print_section("TEST 6: Set Servo Speeds")
        
        if not self.is_master:
            print_test("Set servo speeds", False, "Not registered as master")
            return False
        
        # Build packet: 0x22 + mask + speeds
        # Mask: which servo channels to update (bit 0=ch4, bit 1=ch5, etc.)
        # Set speeds: ch4=100, ch5=-75, ch6=50, ch7=0
        mask = 0x0F  # Update all 4 channels (bits 0-3)
        speeds = [100, -75, 50, 0]
        
        packet = bytearray([UDP_COMMANDS['SET_SERVO_SPEED'], mask])
        for speed in speeds:
            packet.append(speed & 0xFF)
        
        response_data = await self.send_command("SET_SERVO_SPEED", bytes(packet))
        if not response_data:
            print_test("Set servo speeds", False, "No response")
            return False
        
        response, latency_ms = response_data
        cmd_code, status_msg = decode_response(response)
        
        success = len(response) >= 2 and response[1] == 0x00
        speed_str = ", ".join([f"ch{4+i}={speeds[i]}" for i in range(len(speeds))])
        print_test("Set servo speeds", success, f"{speed_str} → {status_msg}", latency_ms)
        return success
    
    async def test_stop_servos(self):
        """Test 7: Stop all servos (0x23)"""
        print_section("TEST 7: Stop All Servos")
        
        if not self.is_master:
            print_test("Stop servos", False, "Not registered as master")
            return False
        
        packet = bytes([UDP_COMMANDS['STOP_SERVOS']])
        response_data = await self.send_command("STOP_SERVOS", packet)
        
        if not response_data:
            print_test("Stop servos", False, "No response")
            return False
        
        response, latency_ms = response_data
        cmd_code, status_msg = decode_response(response)
        
        success = len(response) >= 2 and response[1] == 0x00
        print_test("Stop all servos", success, status_msg, latency_ms)
        return success
    
    async def test_master_unregistration(self):
        """Test 8: Unregister as master"""
        print_section("TEST 8: Master Unregistration")
        
        if not self.is_master:
            print_test("Unregister master", False, "Not currently master")
            return False
        
        packet = bytes([UDP_COMMANDS['MASTER_UNREGISTER']])
        response_data = await self.send_command("MASTER_UNREGISTER", packet)
        
        if not response_data:
            print_test("Unregister master", False, "No response")
            return False
        
        response, latency_ms = response_data
        cmd_code, status_msg = decode_response(response)
        
        success = len(response) >= 2 and response[1] == 0x00
        print_test("Unregister master", success, status_msg, latency_ms)
        
        self.is_master = False
        return success
    
    async def test_denied_without_master(self):
        """Test 9: Verify commands denied without master"""
        print_section("TEST 9: Command Denial Without Master")
        
        if self.is_master:
            print_info("Re-registering as master to test denial...")
            if not await self.test_master_registration():
                print_error("Could not re-register as master for test")
                return False
        
        # Try heartbeat as non-master
        packet = bytes([UDP_COMMANDS['HEARTBEAT']])
        response_data = await self.send_command("HEARTBEAT (non-master)", packet)
        
        if not response_data:
            print_test("Denial verification", False, "No response")
            return False
        
        response, latency_ms = response_data
        
        # Should get IGNORED or DENIED status (0x01 or 0x02)
        success = len(response) >= 2 and response[1] in [0x01, 0x02]
        status_msg = RESPONSE_STATUS.get(response[1], f"0x{response[1]:02X}")
        print_test("Non-master command denied", success, f"Got {status_msg}", latency_ms)
        
        return success
    
    async def run_full_test_suite(self):
        """Run all tests"""
        print(f"\n{Color.BOLD}{Color.HEADER}")
        print("╔════════════════════════════════════════════════════════════════════╗")
        print("║         WebSocket Bridge Test Suite for K10 Bot                    ║")
        print("║                                                                    ║")
        print(f"║  Target: {self.robot_ip:<54} ║")
        print(f"║  Master Token: {self.master_token:<48} ║")
        print(f"║  Started: {datetime.now().strftime('%Y-%m-%d %H:%M:%S'):<52} ║")
        print("╚════════════════════════════════════════════════════════════════════╝")
        print(f"{Color.ENDC}\n")
        
        results = {}
        
        # Test 1: Connection
        results['connection'] = await self.test_connection()
        if not results['connection']:
            print_error("Connection failed, cannot continue")
            return results
        
        # Test 2: Master registration
        results['master_registration'] = await self.test_master_registration()
        
        if results['master_registration']:
            # Test 3: Heartbeat
            results['heartbeat'] = await self.test_heartbeat()
            
            # Test 4: Ping/Latency
            results['ping'] = await self.test_ping(count=3)
            
            # Test 5: Set servo angles
            results['set_servo_angles'] = await self.test_set_servo_angles()
            
            # Test 6: Set servo speeds
            results['set_servo_speeds'] = await self.test_set_servo_speeds()
            
            # Test 7: Stop servos
            results['stop_servos'] = await self.test_stop_servos()
            
            # Test 8: Unregister
            results['master_unregistration'] = await self.test_master_unregistration()
            
            # Test 9: Denial check
            results['denial_check'] = await self.test_denied_without_master()
        
        # Print summary
        print_section("TEST SUMMARY")
        passed = sum(1 for v in results.values() if v)
        total = len(results)
        
        print(f"  {Color.BOLD}Results: {Color.GREEN}{passed}/{total} passed{Color.ENDC}\n")
        
        for test_name, result in results.items():
            status_icon = f"{Color.GREEN}✓{Color.ENDC}" if result else f"{Color.RED}✗{Color.ENDC}"
            print(f"    {status_icon}  {test_name.replace('_', ' ').title()}")
        
        if self.latencies:
            print(f"\n  {Color.BOLD}Performance:{Color.ENDC}")
            avg_latency = sum(self.latencies) / len(self.latencies)
            min_latency = min(self.latencies)
            max_latency = max(self.latencies)
            print(f"    • Average latency: {avg_latency:.1f}ms")
            print(f"    • Min latency: {min_latency:.1f}ms")
            print(f"    • Max latency: {max_latency:.1f}ms")
            print(f"    • Measurements: {len(self.latencies)}")
        
        print(f"\n{Color.BOLD}{Color.GREEN if passed == total else Color.YELLOW}")
        if passed == total:
            print("  All tests PASSED! ✓")
        else:
            print(f"  {total - passed} test(s) FAILED")
        print(f"{Color.ENDC}\n")
        
        return results


# ─── Main ─────────────────────────────────────────────────────────────────────

async def main():
    parser = argparse.ArgumentParser(
        description='WebSocket Bridge Test Suite for K10 Bot',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  python3 test_websocket_bridge.py 192.168.1.100
  python3 test_websocket_bridge.py 192.168.1.100 "12345" --verbose
  python3 test_websocket_bridge.py localhost --verbose
        '''
    )
    
    parser.add_argument('robot_ip', help='Robot IP address or hostname')
    parser.add_argument('master_token', nargs='?', default='0',
                       help='Master token for authorization (default: 0)')
    parser.add_argument('--verbose', '-v', action='store_true',
                       help='Enable verbose logging')
    
    args = parser.parse_args()
    
    # Create tester and run tests
    tester = WebSocketBridgeTester(args.robot_ip, args.master_token, args.verbose)
    
    try:
        # Connect
        if not await tester.connect():
            sys.exit(1)
        
        # Run test suite
        await tester.run_full_test_suite()
        
    except KeyboardInterrupt:
        print(f"\n{Color.YELLOW}Test interrupted by user{Color.ENDC}")
    finally:
        await tester.disconnect()


if __name__ == '__main__':
    asyncio.run(main())
