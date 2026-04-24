#!/usr/bin/env python3
"""
K10 Bot BLE Test Script
=======================
Tests the BLE Nordic UART Service (NUS) transport of the K10-Bot.

Nordic UART Service (NUS) UUIDs:
  Service : 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
  RX char : 6E400002-B5A3-F393-E0A9-E50E24DCCA9E  (write → device)
  TX char : 6E400003-B5A3-F393-E0A9-E50E24DCCA9E  (notify ← device)

Bot protocol (AmakerBotService):
  0x45           GET_NAME   (no auth, always replies)
  0x41 <token>   MASTER_REGISTER
  0x44 <4 bytes> PING       (auth required)
  0x43           HEARTBEAT  (auth required, fire-and-forget)
  0x42           MASTER_UNREGISTER

Usage:
  pip install bleak
  python scripts/test_ble.py                      # auto-discover K10-Bot
  python scripts/test_ble.py --address AA:BB:CC:DD:EE:FF
  python scripts/test_ble.py --token myTok        # 5-char token
  python scripts/test_ble.py --scan-only          # just list nearby BLE devices
"""

import argparse
import asyncio
import struct
import sys
import time
from typing import Optional

try:
    from bleak import BleakClient, BleakScanner
    from bleak.backends.characteristic import BleakGATTCharacteristic
    from bleak.exc import BleakBluetoothNotAvailableError
except ImportError:
    print("ERROR: 'bleak' not installed.")
    print("  Run:  pip install -r scripts/requirements.txt")
    sys.exit(1)

# ── NUS UUIDs ──────────────────────────────────────────────────────────────
NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX_UUID      = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  # write → device
NUS_TX_UUID      = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  # notify ← device

# ── Protocol bytes ─────────────────────────────────────────────────────────
CMD_MASTER_REGISTER   = 0x41
CMD_MASTER_UNREGISTER = 0x42
CMD_HEARTBEAT         = 0x43
CMD_PING              = 0x44
CMD_GET_NAME          = 0x45

STATUS_SUCCESS = 0x00
STATUS_IGNORED = 0x01
STATUS_DENIED  = 0x02
STATUS_ERROR   = 0x03

STATUS_NAMES = {
    STATUS_SUCCESS: "SUCCESS",
    STATUS_IGNORED: "IGNORED",
    STATUS_DENIED:  "DENIED",
    STATUS_ERROR:   "ERROR",
}

DEVICE_NAME   = "K10-Bot"
SCAN_TIMEOUT  = 10.0   # seconds for BLE scan
REPLY_TIMEOUT = 3.0    # seconds to wait for a notification reply
CONNECT_TIMEOUT = 20.0 # seconds for BLE connection (BlueZ can be slow)
CONNECT_RETRIES = 3    # number of connection attempts

# ── Helpers ────────────────────────────────────────────────────────────────

def hex_str(data: bytes) -> str:
    return " ".join(f"{b:02X}" for b in data)


def status_name(byte: int) -> str:
    return STATUS_NAMES.get(byte, f"UNKNOWN(0x{byte:02X})")


def ok(msg: str):   print(f"  ✅  {msg}")
def fail(msg: str): print(f"  ❌  {msg}")
def info(msg: str): print(f"  ℹ️   {msg}")


# ── BLE client wrapper ─────────────────────────────────────────────────────

class NUSClient:
    """Thin wrapper around BleakClient providing send/receive over NUS."""

    def __init__(self, address: str):
        self._address = address
        self._client: Optional[BleakClient] = None
        self._rx_queue: asyncio.Queue[bytes] = asyncio.Queue()

    # -- connection ---------------------------------------------------------

    async def connect(self) -> bool:
        for attempt in range(1, CONNECT_RETRIES + 1):
            self._client = BleakClient(self._address, timeout=CONNECT_TIMEOUT)
            try:
                info(f"Connection attempt {attempt}/{CONNECT_RETRIES} …")
                await self._client.connect()
            except Exception as exc:
                if attempt < CONNECT_RETRIES:
                    info(f"Attempt {attempt} failed ({exc}), retrying in 2s …")
                    await asyncio.sleep(2.0)
                    continue
                fail(f"Connection failed after {CONNECT_RETRIES} attempts: {exc}")
                return False

            if not self._client.is_connected:
                if attempt < CONNECT_RETRIES:
                    info(f"Attempt {attempt}: not connected, retrying …")
                    await asyncio.sleep(2.0)
                    continue
                fail("Client reports not connected after connect().")
                return False

            break  # connected successfully

        # Short pause to let the device finish its post-connect housekeeping
        # (e.g. connection-parameter update) before we touch GATT.
        await asyncio.sleep(0.6)

        # Subscribe to TX notifications (device → host)
        try:
            await self._client.start_notify(NUS_TX_UUID, self._on_notify)
        except Exception as exc:
            fail(f"start_notify failed: {exc}")
            info("Hint: the device may not have enabled the TX characteristic CCCD.")
            info("Check that BotServerBLE::start() was called successfully on the device.")
            return False

        ok(f"Connected to {self._address}")
        return True

    async def disconnect(self):
        if self._client and self._client.is_connected:
            try:
                await self._client.stop_notify(NUS_TX_UUID)
            except Exception:
                pass
            await self._client.disconnect()
        info("Disconnected.")

    # -- notification callback ----------------------------------------------

    def _on_notify(self, _: BleakGATTCharacteristic, data: bytearray):
        self._rx_queue.put_nowait(bytes(data))

    # -- send / receive -----------------------------------------------------

    async def send(self, payload: bytes) -> None:
        """Write payload to the NUS RX characteristic (device receives it)."""
        await self._client.write_gatt_char(NUS_RX_UUID, payload, response=False)

    async def recv(self, timeout: float = REPLY_TIMEOUT) -> Optional[bytes]:
        """Wait for a notification from the device, or return None on timeout."""
        try:
            return await asyncio.wait_for(self._rx_queue.get(), timeout=timeout)
        except asyncio.TimeoutError:
            return None


# ── Individual test cases ──────────────────────────────────────────────────

async def test_get_name(nus: NUSClient) -> bool:
    print("\n── Test: GET_NAME (0x45) ──────────────────────────────────────")
    cmd = bytes([CMD_GET_NAME])
    info(f"TX: {hex_str(cmd)}")
    await nus.send(cmd)
    reply = await nus.recv()
    if reply is None:
        fail("No reply received (timeout).")
        return False
    info(f"RX: {hex_str(reply)}")
    if reply[0] != CMD_GET_NAME:
        fail(f"Unexpected first byte: 0x{reply[0]:02X}")
        return False
    name = reply[1:].decode("ascii", errors="replace")
    ok(f"Bot name: '{name}'")
    return True


async def test_master_register(nus: NUSClient, token: str) -> bool:
    print("\n── Test: MASTER_REGISTER (0x41) ───────────────────────────────")
    if len(token) != 5:
        fail(f"Token must be exactly 5 ASCII chars, got {len(token)}: '{token}'")
        return False
    cmd = bytes([CMD_MASTER_REGISTER]) + token.encode("ascii")
    info(f"TX: {hex_str(cmd)}  (token='{token}')")
    await nus.send(cmd)
    reply = await nus.recv()
    if reply is None:
        fail("No reply received (timeout).")
        return False
    info(f"RX: {hex_str(reply)}")
    if len(reply) < 2:
        fail("Reply too short.")
        return False
    status = reply[-1]
    if status == STATUS_SUCCESS:
        ok(f"Registered as master  [{status_name(status)}]")
        return True
    elif status == STATUS_IGNORED:
        info(f"Already master or token mismatch  [{status_name(status)}]")
        return True  # not a hard failure
    else:
        fail(f"Register failed  [{status_name(status)}]")
        return False


async def test_ping(nus: NUSClient) -> bool:
    print("\n── Test: PING (0x44) ──────────────────────────────────────────")
    ping_id = struct.pack(">I", int(time.time()) & 0xFFFFFFFF)
    cmd = bytes([CMD_PING]) + ping_id
    info(f"TX: {hex_str(cmd)}")
    t0 = time.monotonic()
    await nus.send(cmd)
    reply = await nus.recv()
    rtt = (time.monotonic() - t0) * 1000
    if reply is None:
        fail("No reply received (timeout). Are you registered as master?")
        return False
    info(f"RX: {hex_str(reply)}")
    if reply[0] != CMD_PING or reply[1:5] != ping_id:
        fail("Ping echo mismatch.")
        return False
    ok(f"Ping RTT: {rtt:.1f} ms")
    return True


async def test_heartbeat(nus: NUSClient) -> bool:
    print("\n── Test: HEARTBEAT (0x43) ─────────────────────────────────────")
    cmd = bytes([CMD_HEARTBEAT])
    info(f"TX: {hex_str(cmd)}  (fire-and-forget, no reply expected)")
    await nus.send(cmd)
    # Give the device a moment then check nothing unexpected arrived
    reply = await nus.recv(timeout=1.0)
    if reply is not None:
        info(f"Unexpected reply: {hex_str(reply)}")
    else:
        ok("No reply (correct for heartbeat).")
    return True


async def test_master_unregister(nus: NUSClient) -> bool:
    print("\n── Test: MASTER_UNREGISTER (0x42) ─────────────────────────────")
    cmd = bytes([CMD_MASTER_UNREGISTER])
    info(f"TX: {hex_str(cmd)}")
    await nus.send(cmd)
    reply = await nus.recv()
    if reply is None:
        fail("No reply received (timeout).")
        return False
    info(f"RX: {hex_str(reply)}")
    status = reply[-1]
    if status == STATUS_SUCCESS:
        ok(f"Unregistered  [{status_name(status)}]")
        return True
    else:
        fail(f"Unregister failed  [{status_name(status)}]")
        return False


# ── Scan helper ────────────────────────────────────────────────────────────

async def scan_devices(timeout: float = SCAN_TIMEOUT) -> None:
    print(f"Scanning for BLE devices ({timeout:.0f}s) …")
    try:
        devices = await BleakScanner.discover(timeout=timeout)
    except Exception as exc:
        if "No Bluetooth" in str(exc) or "adapter" in str(exc).lower():
            fail("No Bluetooth adapter found on this machine.")
            info("Make sure a Bluetooth adapter is present and enabled (bluetoothctl power on).")
        else:
            fail(f"Scan error: {exc}")
        return
    if not devices:
        print("No BLE devices found.")
        return
    print(f"\n{'Address':<20}  {'RSSI':>5}  Name")
    print("-" * 55)
    for d in sorted(devices, key=lambda x: x.rssi or -999, reverse=True):
        marker = " ◀ K10-Bot!" if (d.name or "").startswith(DEVICE_NAME) else ""
        print(f"{d.address:<20}  {d.rssi or '?':>5}  {d.name or '(unknown)'}{marker}")


async def find_k10_bot(device_name: str = DEVICE_NAME, timeout: float = SCAN_TIMEOUT) -> Optional[str]:
    print(f"Scanning for '{device_name}' ({timeout:.0f}s) …")
    try:
        device = await BleakScanner.find_device_by_name(device_name, timeout=timeout)
    except Exception as exc:
        if "No Bluetooth" in str(exc) or "adapter" in str(exc).lower():
            fail("No Bluetooth adapter found on this machine.")
            info("Make sure a Bluetooth adapter is present and enabled (bluetoothctl power on).")
        else:
            fail(f"Scan error: {exc}")
        return None
    if device:
        ok(f"Found '{device_name}' at {device.address}")
        return device.address
    fail(f"'{device_name}' not found. Is the device on and advertising?")
    return None


# ── Main ───────────────────────────────────────────────────────────────────

async def run(args: argparse.Namespace) -> int:
    if args.scan_only:
        await scan_devices()
        return 0

    address = args.address
    if address is None:
        address = await find_k10_bot(device_name=args.name)
        if address is None:
            return 1

    nus = NUSClient(address)
    if not await nus.connect():
        return 1

    results: dict[str, bool] = {}

    try:
        # Always test GET_NAME first (no auth required)
        results["get_name"] = await test_get_name(nus)

        # Registration
        results["master_register"] = await test_master_register(nus, args.token)

        if results["master_register"]:
            results["ping"]       = await test_ping(nus)
            results["heartbeat"]  = await test_heartbeat(nus)
            results["unregister"] = await test_master_unregister(nus)
    finally:
        await nus.disconnect()

    # ── Summary ──────────────────────────────────────────────────────────
    print("\n══ Test Summary ════════════════════════════════════════════════")
    passed = sum(1 for v in results.values() if v)
    total  = len(results)
    for name, result in results.items():
        icon = "✅" if result else "❌"
        print(f"  {icon}  {name}")
    print(f"\n  {passed}/{total} tests passed")
    print("════════════════════════════════════════════════════════════════")
    return 0 if passed == total else 1


def main():
    parser = argparse.ArgumentParser(
        description="K10 Bot BLE (NUS) test script",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "--address", "-a",
        help="BLE MAC address of the device (auto-discovered if omitted)",
    )
    parser.add_argument(
        "--name", "-n",
        default=None,
        help=f"BLE device name to scan for (default: '{DEVICE_NAME}', prompted if omitted)",
    )
    parser.add_argument(
        "--token", "-t",
        default=None,
        help="5-char auth token configured on the bot (prompted if omitted)",
    )
    parser.add_argument(
        "--scan-only", "-s",
        action="store_true",
        help="Only scan for nearby BLE devices and exit",
    )
    args = parser.parse_args()

    # Interactive prompts when values not provided via CLI
    if not args.scan_only:
        if args.name is None:
            entered = input(f"BLE device name [{DEVICE_NAME}]: ").strip()
            args.name = entered if entered else DEVICE_NAME
        if args.token is None:
            entered = input("Auth token (5 chars) [abc12]: ").strip()
            args.token = entered if entered else "abc12"

    sys.exit(asyncio.run(run(args)))


if __name__ == "__main__":
    main()
