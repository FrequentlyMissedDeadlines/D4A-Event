#!/usr/bin/env python3
"""
K10 Bot — WebSocket test suite
Usage: python test_websocket.py <ip> <token> [--port PORT] [--timeout SEC]

Connects to ws://<ip>:<port>/ws and exercises the same binary bot protocol
as the UDP suite, but over the WebSocket transport.

Covers:
  • AmakerBotService  (0x4x) — master registration, heartbeat, ping, bot name
  • MotorServoService (0x2x) — motor speed, servo type/speed/angle, stop
  • LEDService        (0x5x) — set colour, turn off, get colour
  • Edge cases        — unknown service, bad values, not-master guard

Requires:
  pip install websockets
"""

import argparse
import asyncio
import struct
import sys
import time

try:
    import websockets
except ImportError:
    print("ERROR: 'websockets' package not installed.  Run:  pip install websockets")
    sys.exit(1)

# ─── Protocol constants ───────────────────────────────────────────────────────

DEFAULT_PORT    = 81
DEFAULT_TIMEOUT = 2.0
WS_PATH         = "/ws"

RESP_OK               = 0x00
RESP_INVALID_PARAMS   = 0x01
RESP_INVALID_VALUES   = 0x02
RESP_OPERATION_FAILED = 0x03
RESP_NOT_STARTED      = 0x04
RESP_UNKNOWN_SERVICE  = 0x05
RESP_UNKNOWN_CMD      = 0x06
RESP_NOT_MASTER       = 0x07

_RESP_NAMES = {
    RESP_OK:               "resp_ok",
    RESP_INVALID_PARAMS:   "resp_invalid_params",
    RESP_INVALID_VALUES:   "resp_invalid_values",
    RESP_OPERATION_FAILED: "resp_operation_failed",
    RESP_NOT_STARTED:      "resp_not_started",
    RESP_UNKNOWN_SERVICE:  "resp_unknown_service",
    RESP_UNKNOWN_CMD:      "resp_unknown_cmd",
    RESP_NOT_MASTER:       "resp_not_master",
}

def resp_name(code: int) -> str:
    return _RESP_NAMES.get(code, f"0x{code:02X}")

# ─── Colour output ────────────────────────────────────────────────────────────

GREEN  = "\033[92m"
RED    = "\033[91m"
YELLOW = "\033[93m"
CYAN   = "\033[96m"
RESET  = "\033[0m"
BOLD   = "\033[1m"

def _ok(msg: str)    -> None: print(f"  {GREEN}✔ {msg}{RESET}")
def _fail(msg: str)  -> None: print(f"  {RED}✘ {msg}{RESET}")
def _warn(msg: str)  -> None: print(f"  {YELLOW}⚠ {msg}{RESET}")
def _info(msg: str)  -> None: print(f"  {CYAN}ℹ {msg}{RESET}")
def _section(t: str) -> None:
    print(f"\n{BOLD}{CYAN}{'─' * 60}{RESET}")
    print(f"{BOLD}{CYAN}  {t}{RESET}")
    print(f"{BOLD}{CYAN}{'─' * 60}{RESET}")

def _hex(data: bytes) -> str:
    return " ".join(f"{b:02X}" for b in data)

# ─── Result tracker ───────────────────────────────────────────────────────────

class Results:
    def __init__(self):
        self.passed  = 0
        self.failed  = 0
        self.skipped = 0

    def check(self, condition: bool, label: str) -> bool:
        if condition:
            _ok(label)
            self.passed += 1
        else:
            _fail(label)
            self.failed += 1
        return condition

    def skip(self, label: str) -> None:
        _warn(f"SKIP — {label}")
        self.skipped += 1

    def summary(self) -> None:
        total = self.passed + self.failed
        print(f"\n{BOLD}{'─' * 60}{RESET}")
        print(f"{BOLD}  Results: {self.passed}/{total} passed", end="")
        if self.skipped:
            print(f"  ({self.skipped} skipped)", end="")
        if self.failed == 0:
            print(f"  {GREEN}ALL PASS{RESET}")
        else:
            print(f"  {RED}{self.failed} FAILED{RESET}")
        print(f"{BOLD}{'─' * 60}{RESET}\n")

# ─── WebSocket client wrapper ─────────────────────────────────────────────────

class WSClient:
    """Thin async wrapper around a websockets connection."""

    def __init__(self, ws, timeout: float):
        self._ws      = ws
        self._timeout = timeout

    async def send(self, data: bytes) -> bytes | None:
        """Send binary frame and await the first reply; returns None on timeout."""
        t0 = time.monotonic()
        await self._ws.send(data)
        try:
            raw = await asyncio.wait_for(self._ws.recv(), timeout=self._timeout)
            latency = (time.monotonic() - t0) * 1000
            _info(f"round-trip {latency:.1f} ms")
            return raw if isinstance(raw, (bytes, bytearray)) else raw.encode()
        except asyncio.TimeoutError:
            return None

    async def fire(self, data: bytes) -> None:
        """Fire-and-forget (no reply expected, e.g. heartbeat)."""
        await self._ws.send(data)

# ─── AmakerBotService (0x4x) ─────────────────────────────────────────────────

async def test_amakerbot(ws: WSClient, token: str, r: Results) -> bool:
    _section("AmakerBotService (0x4x)")

    # --- 0x41 MASTER_REGISTER ---
    _info("0x41 : MASTER_REGISTER")
    req  = bytes([0x41]) + token.encode()
    resp = await ws.send(req)
    if resp is None:
        _fail("0x41 MASTER_REGISTER — no response (device reachable?)")
        r.failed += 1
        return False

    _info(f"0x41 reply: {_hex(resp)}")
    r.check(resp[0] == 0x41, "0x41 echo action byte")
    r.check(len(resp) == 2,  f"0x41 reply length == 2 (got {len(resp)})")
    registered = resp[1] == RESP_OK
    r.check(registered, f"0x41 status == resp_ok (got {resp_name(resp[1])})")
    if not registered:
        _warn("Token rejected — remaining master-only tests may fail")

    # --- 0x43 HEARTBEAT (no reply expected) ---
    _info("0x43 : HEARTBEAT")
    await ws.fire(bytes([0x43]))
    _info("0x43 HEARTBEAT sent (no reply expected)")

    # --- 0x44 PING ---
    _info("0x44 : PING")
    ping_id = struct.pack(">I", 0xCAFEBABE)
    resp = await ws.send(bytes([0x44]) + ping_id)
    if resp is None:
        r.skip("0x44 PING — timeout")
    else:
        _info(f"0x44 reply: {_hex(resp)}")
        r.check(resp[0] == 0x44,     "0x44 echo action byte")
        r.check(len(resp) == 5,      f"0x44 reply length == 5 (got {len(resp)})")
        r.check(resp[1:] == ping_id, f"0x44 echo payload == {_hex(ping_id)}")

    # --- 0x45 GET_BOT_NAME ---
    _info("0x45 : GET_BOT_NAME")
    resp = await ws.send(bytes([0x45]))
    if resp is None:
        r.skip("0x45 GET_BOT_NAME — timeout")
    else:
        _info(f"0x45 reply: {_hex(resp)}")
        r.check(resp[0] == 0x45,          "0x45 echo action byte")
        r.check(resp[1] == RESP_OK,       f"0x45 status == resp_ok (got {resp_name(resp[1])})")
        name = resp[2:].decode(errors="replace")
        r.check(len(name) > 0,            f"0x45 bot name not empty (got '{name}')")
        _info(f"Bot name: '{name}'")

    # --- 0x46 SET_BOT_NAME (master only) ---
    _info("0x46 : SET_BOT_NAME")
    if registered:
        new_name = "K10-WS-Test"
        resp = await ws.send(bytes([0x46]) + new_name.encode())
        if resp is None:
            r.skip("0x46 SET_BOT_NAME — timeout")
        else:
            _info(f"0x46 reply: {_hex(resp)}")
            r.check(resp[0] == 0x46,    "0x46 echo action byte")
            r.check(resp[1] == RESP_OK, f"0x46 status == resp_ok (got {resp_name(resp[1])})")

        # Restore original name — use send() (not fire()) so the device's
        # reply is consumed and doesn't pollute the next recv() call.
        await ws.send(bytes([0x46]) + b"K10-Bot")
    else:
        r.skip("0x46 SET_BOT_NAME (not master)")

    # --- 0x42 MASTER_UNREGISTER ---
    _info("0x42 : MASTER_UNREGISTER")
    resp = await ws.send(bytes([0x42]))
    if resp is None:
        r.skip("0x42 MASTER_UNREGISTER — timeout")
    else:
        _info(f"0x42 reply: {_hex(resp)}")
        r.check(resp[0] == 0x42,    "0x42 echo action byte")
        r.check(resp[1] == RESP_OK, f"0x42 status == resp_ok (got {resp_name(resp[1])})")

    return registered

# ─── MotorServoService (0x2x) ─────────────────────────────────────────────────

async def test_motor_servo(ws: WSClient, r: Results) -> None:
    _section("MotorServoService (0x2x)")

    # --- 0x21 SET_MOTORS_SPEED  motor_mask=0x01, speed=+50 ---
    _info("0x21 : SET_MOTORS_SPEED")
    speed_i8 = struct.pack("b", 50)
    resp = await ws.send(bytes([0x21, 0x01]) + speed_i8)
    if resp is None:
        r.skip("0x21 SET_MOTORS_SPEED — timeout")
    else:
        _info(f"0x21 reply: {_hex(resp)}")
        r.check(resp[0] == 0x21,    "0x21 echo action byte")
        r.check(resp[1] == RESP_OK, f"0x21 status == resp_ok (got {resp_name(resp[1])})")

    # --- 0x22 SET_SERVO_TYPE  servo_mask=0x01, type=0 (180°) ---
    _info("0x22 : SET_SERVO_TYPE")
    resp = await ws.send(bytes([0x22, 0x01, 0x00]))
    if resp is None:
        r.skip("0x22 SET_SERVO_TYPE — timeout")
    else:
        _info(f"0x22 reply: {_hex(resp)}")
        r.check(resp[0] == 0x22,    "0x22 echo action byte")
        r.check(resp[1] == RESP_OK, f"0x22 status == resp_ok (got {resp_name(resp[1])})")

    # --- 0x24 SET_SERVOS_ANGLE  servo_mask=0x01, angle=90° (big-endian i16) ---
    _info("0x24 : SET_SERVOS_ANGLE")
    angle_be = struct.pack(">h", 90)
    resp = await ws.send(bytes([0x24, 0x01]) + angle_be)
    if resp is None:
        r.skip("0x24 SET_SERVOS_ANGLE — timeout")
    else:
        _info(f"0x24 reply: {_hex(resp)}")
        r.check(resp[0] == 0x24,    "0x24 echo action byte")
        r.check(resp[1] == RESP_OK, f"0x24 status == resp_ok (got {resp_name(resp[1])})")

    # --- 0x25 INCREMENT_SERVOS_ANGLE  servo_mask=0x01, delta=+10° ---
    _info("0x25 : INCREMENT_SERVOS_ANGLE")
    delta_be = struct.pack(">h", 10)
    resp = await ws.send(bytes([0x25, 0x01]) + delta_be)
    if resp is None:
        r.skip("0x25 INCREMENT_SERVOS_ANGLE — timeout")
    else:
        _info(f"0x25 reply: {_hex(resp)}")
        r.check(resp[0] == 0x25,    "0x25 echo action byte")
        r.check(resp[1] == RESP_OK, f"0x25 status == resp_ok (got {resp_name(resp[1])})")

    # --- 0x27 GET_SERVOS_ANGLE  servo_mask=0x01 ---
    _info("0x27 : GET_SERVOS_ANGLE")
    resp = await ws.send(bytes([0x27, 0x01]))
    if resp is None:
        r.skip("0x27 GET_SERVOS_ANGLE — timeout")
    else:
        _info(f"0x27 reply: {_hex(resp)}")
        r.check(resp[0] == 0x27,    "0x27 echo action byte")
        r.check(resp[1] == RESP_OK, f"0x27 status == resp_ok (got {resp_name(resp[1])})")
        r.check(len(resp) >= 5,     f"0x27 reply length >= 5 (got {len(resp)})")
        if len(resp) >= 5:
            angle_back = struct.unpack(">h", resp[3:5])[0]
            _info(f"Servo 0 angle readback: {angle_back}°")

    # --- 0x26 GET_MOTORS_SPEED  motor_mask=0x01 ---
    _info("0x26 : GET_MOTORS_SPEED")
    resp = await ws.send(bytes([0x26, 0x01]))
    if resp is None:
        r.skip("0x26 GET_MOTORS_SPEED — timeout")
    else:
        _info(f"0x26 reply: {_hex(resp)}")
        r.check(resp[0] == 0x26,    "0x26 echo action byte")
        r.check(resp[1] == RESP_OK, f"0x26 status == resp_ok (got {resp_name(resp[1])})")
        if len(resp) >= 4:
            spd = struct.unpack("b", bytes([resp[3]]))[0]
            _info(f"Motor 1 speed readback: {spd}")

    # --- 0x28 STOP_ALL_MOTORS ---
    _info("0x28 : STOP_ALL_MOTORS")
    resp = await ws.send(bytes([0x28]))
    if resp is None:
        r.skip("0x28 STOP_ALL_MOTORS — timeout")
    else:
        _info(f"0x28 reply: {_hex(resp)}")
        r.check(resp[0] == 0x28,    "0x28 echo action byte")
        r.check(resp[1] == RESP_OK, f"0x28 status == resp_ok (got {resp_name(resp[1])})")

# ─── LEDService (0x5x) ────────────────────────────────────────────────────────

async def test_led(ws: WSClient, r: Results) -> None:
    _section("LEDService (0x5x)")

    # --- 0x51 SET_COLOR  led_mask=0x07 (all K10), R=255 G=0 B=128 brightness=80 ---
    _info("0x51 : SET_COLOR")
    resp = await ws.send(bytes([0x51, 0x07, 0xFF, 0x00, 0x80, 0x50]))
    if resp is None:
        r.skip("0x51 SET_COLOR — timeout")
    else:
        _info(f"0x51 reply: {_hex(resp)}")
        r.check(resp[0] == 0x51,    "0x51 echo action byte")
        r.check(resp[1] == RESP_OK, f"0x51 status == resp_ok (got {resp_name(resp[1])})")

    # --- 0x54 GET_COLOR  led_mask=0x07 ---
    _info("0x54 : GET_COLOR")
    resp = await ws.send(bytes([0x54, 0x07]))
    if resp is None:
        r.skip("0x54 GET_COLOR — timeout")
    else:
        _info(f"0x54 reply: {_hex(resp)}")
        r.check(resp[0] == 0x54,    "0x54 echo action byte")
        r.check(resp[1] == RESP_OK, f"0x54 status == resp_ok (got {resp_name(resp[1])})")
        r.check(len(resp) >= 15,    f"0x54 reply length >= 15 (got {len(resp)})")
        if len(resp) >= 7:
            _info(f"LED 0 colour: R={resp[3]} G={resp[4]} B={resp[5]} Br={resp[6]}")

    # --- 0x52 TURN_OFF  led_mask=0x07 ---
    _info("0x52 : TURN_OFF")
    resp = await ws.send(bytes([0x52, 0x07]))
    if resp is None:
        r.skip("0x52 TURN_OFF — timeout")
    else:
        _info(f"0x52 reply: {_hex(resp)}")
        r.check(resp[0] == 0x52,    "0x52 echo action byte")
        r.check(resp[1] == RESP_OK, f"0x52 status == resp_ok (got {resp_name(resp[1])})")

    # --- 0x53 TURN_OFF_ALL ---
    _info("0x53 : TURN_OFF_ALL")
    resp = await ws.send(bytes([0x53]))
    if resp is None:
        r.skip("0x53 TURN_OFF_ALL — timeout")
    else:
        _info(f"0x53 reply: {_hex(resp)}")
        r.check(resp[0] == 0x53,    "0x53 echo action byte")
        r.check(resp[1] == RESP_OK, f"0x53 status == resp_ok (got {resp_name(resp[1])})")

# ─── Edge cases ───────────────────────────────────────────────────────────────

async def test_edge_cases(ws: WSClient, r: Results) -> None:
    _section("Edge cases")

    # --- Unknown service (service_id 0x0F) ---
    _info("0xF1 : UNKNOWN_SERVICE")
    resp = await ws.send(bytes([0xF1, 0x00]))
    if resp is None:
        r.skip("Unknown service — timeout")
    else:
        _info(f"Unknown service reply: {_hex(resp)}")
        r.check(resp[1] == RESP_UNKNOWN_SERVICE,
                f"Unknown service → resp_unknown_service (got {resp_name(resp[1])})")

    # --- SET_MOTORS_SPEED with out-of-range speed (127 > 100) ---
    _info("test SET_MOTORS_SPEED (out-of-range): 0x21")
    bad_speed = struct.pack("b", 127)
    resp = await ws.send(bytes([0x21, 0x01]) + bad_speed)
    if resp is None:
        r.skip("0x21 out-of-range speed — timeout")
    else:
        _info(f"0x21 bad speed reply: {_hex(resp)}")
        r.check(resp[1] in (RESP_INVALID_VALUES, RESP_INVALID_PARAMS),
                f"Bad speed → invalid_values/invalid_params (got {resp_name(resp[1])})")

    # --- Not-master guard: 0x46 SET_BOT_NAME without being registered ---
    _info("test SET_BOT_NAME (not master): 0x46")
    resp = await ws.send(bytes([0x46]) + b"Hacked")
    if resp is None:
        r.skip("0x46 not-master guard — timeout")
    else:
        _info(f"0x46 not-master reply: {_hex(resp)}")
        r.check(resp[1] == RESP_NOT_MASTER,
                f"Not-master command → resp_not_master (got {resp_name(resp[1])})")

# ─── Main ─────────────────────────────────────────────────────────────────────

def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="K10 Bot WebSocket test suite")
    p.add_argument("ip",    help="Device IP address")
    p.add_argument("token", help="Server token (shown in device serial log on boot)")
    p.add_argument("--port",    type=int,   default=DEFAULT_PORT,    metavar="PORT",
                   help=f"WebSocket port (default {DEFAULT_PORT})")
    p.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT, metavar="SEC",
                   help=f"Per-request timeout in seconds (default {DEFAULT_TIMEOUT})")
    return p.parse_args()


async def run(args: argparse.Namespace) -> int:
    url = f"ws://{args.ip}:{args.port}{WS_PATH}"
    r   = Results()

    print(f"\n{BOLD}K10 Bot — WebSocket test suite{RESET}")
    _info(f"Target : {url}")
    _info(f"Token  : {args.token}")
    _info(f"Timeout: {args.timeout} s")

    try:
        async with websockets.connect(url, ping_interval=None) as ws:
            _ok(f"Connected to {url}")
            client     = WSClient(ws, args.timeout)
            registered = await test_amakerbot(client, args.token, r)

            # Re-register after the unregister at the end of test_amakerbot
            if registered:
                req  = bytes([0x41]) + args.token.encode()
                resp = await ws.send(req)
                raw  = await asyncio.wait_for(ws.recv(), timeout=args.timeout)
                if isinstance(raw, (bytes, bytearray)) and raw[1] == RESP_OK:
                    _info("Re-registered as master for hardware tests")

            await test_motor_servo(client, r)
            await test_led(client, r)
            await test_edge_cases(client, r)

    except OSError as e:
        _fail(f"Could not connect to {url}: {e}")
        r.failed += 1
    except Exception as e:
        _fail(f"Unexpected error: {e}")
        r.failed += 1

    r.summary()
    return 0 if r.failed == 0 else 1


def main() -> None:
    args = parse_args()
    sys.exit(asyncio.run(run(args)))


if __name__ == "__main__":
    main()
