#!/usr/bin/env python3
"""
Heartbeat / Ping Stress Test for K10 Bot

Sends heartbeat (0x43) or ping (0x44) commands at a fixed interval over
WebSocket until the connection fails or the user presses Ctrl+C.

Modes:
  heartbeat  – fire-and-forget (send-side latency only)
  ping       – request/reply   (round-trip latency measured)

Usage:
    python3 test_heartbeat_stress.py <robot_ip> [token] [--mode heartbeat|ping] [--interval 30] [-v]

Examples:
    python3 test_heartbeat_stress.py 192.168.1.100
    python3 test_heartbeat_stress.py 192.168.1.100 "12345" --mode ping --interval 50
    python3 test_heartbeat_stress.py 192.168.1.100 --mode ping --verbose
"""

import asyncio
import struct
import sys
import time
from typing import Optional
import argparse

try:
    import websockets
except ImportError:
    print("ERROR: 'websockets' package required.  pip install websockets")
    sys.exit(1)

# ─── Constants ────────────────────────────────────────────────────────────────

CMD_MASTER_REGISTER = 0x41
CMD_HEARTBEAT = 0x43
CMD_PING = 0x44
STATUS_SUCCESS = 0x00
RECV_TIMEOUT_S = 2.0

# ─── Colours ──────────────────────────────────────────────────────────────────

class C:
    BOLD  = '\033[1m'
    GREEN = '\033[92m'
    YELLOW= '\033[93m'
    RED   = '\033[91m'
    CYAN  = '\033[96m'
    BLUE  = '\033[94m'
    END   = '\033[0m'

# ─── Helpers ──────────────────────────────────────────────────────────────────

def hex_dump(data: bytes) -> str:
    return ' '.join(f'{b:02X}' for b in data)


def percentile(sorted_vals: list[float], pct: float) -> float:
    """Return the p-th percentile from a *sorted* list."""
    if not sorted_vals:
        return 0.0
    k = (len(sorted_vals) - 1) * pct / 100.0
    lo = int(k)
    hi = min(lo + 1, len(sorted_vals) - 1)
    frac = k - lo
    return sorted_vals[lo] + frac * (sorted_vals[hi] - sorted_vals[lo])

# ─── Main Logic ───────────────────────────────────────────────────────────────

async def run(robot_ip: str, token: str, interval_ms: int, verbose: bool, mode: str = "heartbeat"):
    ws_url = f"ws://{robot_ip}:80/ws"
    mode_label = mode.upper()

    print(f"{C.BOLD}{C.CYAN}{'─' * 60}")
    print(f"  {mode_label} Stress Test")
    print(f"{'─' * 60}{C.END}")
    print(f"  Target   : {ws_url}")
    print(f"  Mode     : {mode_label}")
    print(f"  Interval : {interval_ms} ms")
    print(f"  Token    : {token}")
    print(f"  Press {C.BOLD}Ctrl+C{C.END} to stop\n")

    # ── Connect ───────────────────────────────────────────────────────────
    try:
        ws = await websockets.connect(ws_url, ping_interval=None)
    except Exception as exc:
        print(f"{C.RED}  ✗ Connection failed: {exc}{C.END}")
        return

    print(f"{C.GREEN}  ✓ Connected{C.END}")

    # ── Register as master ────────────────────────────────────────────────
    token_val = int(token) if token.isdigit() else 0
    reg_pkt = bytes([CMD_MASTER_REGISTER]) + struct.pack('<I', token_val)
    await ws.send(reg_pkt)
    try:
        resp = await asyncio.wait_for(ws.recv(), timeout=RECV_TIMEOUT_S)
        if len(resp) >= 2 and resp[1] == STATUS_SUCCESS:
            print(f"{C.GREEN}  ✓ Registered as master{C.END}")
        else:
            print(f"{C.RED}  ✗ Registration failed: {hex_dump(resp)}{C.END}")
            await ws.close()
            return
    except asyncio.TimeoutError:
        print(f"{C.RED}  ✗ Registration timeout{C.END}")
        await ws.close()
        return

    # ── Stress loop ─────────────────────────────────────────────────────
    interval_s = interval_ms / 1000.0
    sent = 0
    ok = 0
    fail = 0
    latencies: list[float] = []
    start_time = time.monotonic()
    failure_time: Optional[float] = None
    failure_reason: str = ""
    seq: int = 0  # ping sequence counter

    if mode == "ping":
        print(f"\n{C.BOLD}  Sending pings (round-trip, reply expected) …{C.END}\n")
    else:
        print(f"\n{C.BOLD}  Sending heartbeats (fire-and-forget, no reply expected) …{C.END}\n")

    try:
        while True:
            loop_start = time.monotonic()
            t0 = time.monotonic()

            try:
                if mode == "ping":
                    # ── PING mode: send [0x44][4-byte seq LE], expect echo ─
                    seq += 1
                    ping_pkt = bytes([CMD_PING]) + struct.pack('<I', seq)
                    await ws.send(ping_pkt)

                    resp = await asyncio.wait_for(ws.recv(), timeout=RECV_TIMEOUT_S)
                    latency_ms = (time.monotonic() - t0) * 1000.0
                    sent += 1

                    # Validate: response must be same 5 bytes
                    if isinstance(resp, bytes) and len(resp) >= 5 and resp[0] == CMD_PING:
                        resp_seq = struct.unpack('<I', resp[1:5])[0]
                        if resp_seq == seq:
                            ok += 1
                            latencies.append(latency_ms)
                            if verbose:
                                print(f"    #{sent:>6}  {C.GREEN}OK{C.END}  seq={seq}  {latency_ms:6.2f} ms")
                            elif sent % 100 == 0:
                                avg = sum(latencies[-100:]) / min(100, len(latencies))
                                print(f"    #{sent:>6}  ok={ok}  fail={fail}  avg(last100)={avg:.2f}ms", end='\r')
                        else:
                            fail += 1
                            failure_time = time.monotonic() - start_time
                            failure_reason = f"Seq mismatch: sent={seq} got={resp_seq} ({hex_dump(resp)})"
                            print(f"\n{C.RED}  ✗ Ping #{sent} FAILED: {failure_reason}{C.END}")
                            break
                    else:
                        fail += 1
                        failure_time = time.monotonic() - start_time
                        resp_hex = hex_dump(resp) if isinstance(resp, bytes) else repr(resp)
                        failure_reason = f"Bad response: {resp_hex}"
                        print(f"\n{C.RED}  ✗ Ping #{sent} FAILED: {failure_reason}{C.END}")
                        break

                else:
                    # ── HEARTBEAT mode: fire-and-forget ────────────────────
                    hb_pkt = bytes([CMD_HEARTBEAT])
                    await ws.send(hb_pkt)
                    latency_ms = (time.monotonic() - t0) * 1000.0
                    sent += 1
                    ok += 1
                    latencies.append(latency_ms)

                    if verbose:
                        print(f"    #{sent:>6}  {C.GREEN}OK{C.END}  {latency_ms:6.2f} ms")
                    elif sent % 100 == 0:
                        avg = sum(latencies[-100:]) / min(100, len(latencies))
                        print(f"    #{sent:>6}  ok={ok}  fail={fail}  avg(last100)={avg:.2f}ms", end='\r')

                    # Drain any unsolicited messages without blocking
                    try:
                        while True:
                            await asyncio.wait_for(ws.recv(), timeout=0.001)
                    except (asyncio.TimeoutError, Exception):
                        pass

            except asyncio.TimeoutError:
                sent += 1
                fail += 1
                failure_time = time.monotonic() - start_time
                failure_reason = f"Timeout (no response in {RECV_TIMEOUT_S} s)"
                print(f"\n{C.RED}  ✗ {mode_label} #{sent} FAILED: {failure_reason}{C.END}")
                break

            except websockets.exceptions.ConnectionClosed as exc:
                sent += 1
                fail += 1
                failure_time = time.monotonic() - start_time
                failure_reason = f"Connection closed: {exc}"
                print(f"\n{C.RED}  ✗ {mode_label} #{sent} FAILED: {failure_reason}{C.END}")
                break

            except Exception as exc:
                sent += 1
                fail += 1
                failure_time = time.monotonic() - start_time
                failure_reason = f"Error: {exc}"
                print(f"\n{C.RED}  ✗ {mode_label} #{sent} FAILED: {failure_reason}{C.END}")
                break

            # Sleep for the remaining interval
            elapsed = time.monotonic() - loop_start
            sleep_s = max(0, interval_s - elapsed)
            if sleep_s > 0:
                await asyncio.sleep(sleep_s)

    except KeyboardInterrupt:
        pass

    total_time = time.monotonic() - start_time

    # ── Report ────────────────────────────────────────────────────────────
    print(f"\n\n{C.BOLD}{C.CYAN}{'─' * 60}")
    print(f"  Results")
    print(f"{'─' * 60}{C.END}")

    print(f"  Total time    : {total_time:.1f} s")
    print(f"  {mode_label}s   : {sent} sent, {C.GREEN}{ok} ok{C.END}, {C.RED}{fail} failed{C.END}")
    if sent > 0:
        print(f"  Effective rate: {sent / total_time:.1f} msg/s  (target {1000 / interval_ms:.1f} msg/s)")

    if latencies:
        latencies.sort()
        avg = sum(latencies) / len(latencies)
        lat_kind = "Round-trip latency" if mode == "ping" else "Send latency (no reply)"
        print(f"\n  {C.BOLD}{lat_kind} (ms):{C.END}")
        print(f"    Min  : {latencies[0]:7.2f}")
        print(f"    Avg  : {avg:7.2f}")
        print(f"    p50  : {percentile(latencies, 50):7.2f}")
        print(f"    p95  : {percentile(latencies, 95):7.2f}")
        print(f"    p99  : {percentile(latencies, 99):7.2f}")
        print(f"    Max  : {latencies[-1]:7.2f}")

    if failure_time is not None:
        print(f"\n  {C.RED}{C.BOLD}First failure after {failure_time:.3f} s  ({mode_label} #{sent}){C.END}")
        print(f"  Reason: {failure_reason}")
    else:
        print(f"\n  {C.GREEN}{C.BOLD}No failures — stopped by user after {total_time:.1f} s{C.END}")

    print()

    # ── Cleanup ───────────────────────────────────────────────────────────
    try:
        await ws.close()
    except Exception:
        pass


# ─── Entry point ──────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description='Heartbeat / Ping stress test for K10 Bot (WebSocket)',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  python3 test_heartbeat_stress.py 192.168.1.100
  python3 test_heartbeat_stress.py 192.168.1.100 "12345" --mode ping
  python3 test_heartbeat_stress.py 192.168.1.100 --mode ping --interval 50 -v
  python3 test_heartbeat_stress.py 192.168.1.100 --mode heartbeat --verbose
        '''
    )

    parser.add_argument('robot_ip', help='Robot IP address or hostname')
    parser.add_argument('master_token', nargs='?', default='0',
                        help='Master token (default: 0)')
    parser.add_argument('--mode', '-m', choices=['heartbeat', 'ping'], default='heartbeat',
                        help='Test mode: heartbeat (fire-and-forget) or ping (round-trip) (default: heartbeat)')
    parser.add_argument('--interval', '-i', type=int, default=30,
                        help='Send interval in ms (default: 30)')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Print every message result')

    args = parser.parse_args()

    try:
        asyncio.run(run(args.robot_ip, args.master_token,
                        args.interval, args.verbose, args.mode))
    except KeyboardInterrupt:
        pass


if __name__ == '__main__':
    main()
