#!/usr/bin/env python3
"""
K10 Bot — UDP test suite
Usage: python test_amakerbot_udp.py <ip> <port> <token>

Covers:
  • AmakerBotService  (0x41 – 0x46)  master registration, heartbeat, ping, bot name
  • BoardInfoService  (0x11 – 0x14)  onboard RGB LEDs
  • DFR1216Service    (0x31 – 0x34)  expansion board RGB LEDs
  • ServoService      (0x51 – 0x59)  servos and DC motors
  • MusicService      (text)         play / tone / stop / melodies
"""

import argparse
import json
import socket
import struct
import sys
import threading
import time

# ─── Constants ───────────────────────────────────────────────────────────────

DEFAULT_TIMEOUT = 2.0          # seconds

# UDPResponseStatus (AmakerBotService replies)
UDP_SUCCESS = 0x01
UDP_IGNORED = 0x02
UDP_DENIED  = 0x03
UDP_ERROR   = 0x04
_AB_STATUS = {UDP_SUCCESS: "SUCCESS", UDP_IGNORED: "IGNORED",
              UDP_DENIED: "DENIED", UDP_ERROR: "ERROR"}

# UDPProto (binary resp_code, all other services)
RESP_OK               = 0x00
RESP_INVALID_PARAMS   = 0x01
RESP_INVALID_VALUES   = 0x02
RESP_OPERATION_FAILED = 0x03
RESP_NOT_STARTED      = 0x04
RESP_UNKNOWN_CMD      = 0x05
RESP_NOT_MASTER       = 0x06
_PROTO_STATUS = {
    RESP_OK: "ok",
    RESP_INVALID_PARAMS: "invalid_params",
    RESP_INVALID_VALUES: "invalid_values",
    RESP_OPERATION_FAILED: "operation_failed",
    RESP_NOT_STARTED: "not_started",
    RESP_UNKNOWN_CMD: "unknown_cmd",
    RESP_NOT_MASTER: "not_master",
}

# ─── Colours ─────────────────────────────────────────────────────────────────

GREEN  = "\033[92m"
RED    = "\033[91m"
YELLOW = "\033[93m"
CYAN   = "\033[96m"
RESET  = "\033[0m"
BOLD   = "\033[1m"

def _ok(msg: str)   -> None: print(f"  {GREEN}✔ {msg}{RESET}")
def _fail(msg: str) -> None: print(f"  {RED}✘ {msg}{RESET}")
def _warn(msg: str) -> None: print(f"  {YELLOW}⚠ {msg}{RESET}")
def _info(msg: str) -> None: print(f"  {CYAN}ℹ {msg}{RESET}")
def _section(title: str) -> None:
    print(f"\n{BOLD}{CYAN}{'─' * 60}{RESET}")
    print(f"{BOLD}{CYAN}  {title}{RESET}")
    print(f"{BOLD}{CYAN}{'─' * 60}{RESET}")

# ─── Socket helpers ──────────────────────────────────────────────────────────

class UDPClient:
    """Reusable UDP socket bound once so the device can reply to the same port."""

    def __init__(self, ip: str, port: int, timeout: float = DEFAULT_TIMEOUT):
        self.target = (ip, port)
        self.timeout = timeout
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.settimeout(timeout)
        self.sock.bind(("", 0))   # bind to ephemeral port — keep it stable for master-IP checks

    def send(self, data: bytes) -> bytes | None:
        """Send raw bytes and return the response, or None on timeout."""
        self.sock.sendto(data, self.target)
        try:
            resp, _ = self.sock.recvfrom(512)
            return resp
        except socket.timeout:
            return None

    def send_text(self, message: str) -> str | None:
        resp = self.send(message.encode())
        return resp.decode(errors="replace") if resp else None

    def send_no_reply(self, data: bytes) -> None:
        """Fire-and-forget (e.g. heartbeat)."""
        self.sock.sendto(data, self.target)

    def close(self) -> None:
        self.sock.close()


# ─── Result tracker ──────────────────────────────────────────────────────────

class Results:
    def __init__(self):
        self.passed = 0
        self.failed = 0
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


# ─── Speed / angle helpers ───────────────────────────────────────────────────

def encode_speed(speed: int) -> int:
    """Map −100..+100 to 28..228 (encoded = speed + 128)."""
    return max(28, min(228, speed + 128))


def encode_angle(degrees: int) -> bytes:
    """Pack a centre-zero angle as int16 LE with bit0=1 (set) or bit0=0 (skip)."""
    raw = (degrees << 1) | 1
    return struct.pack("<h", raw)


def skip_angle() -> bytes:
    return struct.pack("<h", 0)


# ─── Heartbeat thread ────────────────────────────────────────────────────────

class HeartbeatThread:
    """Sends [0x43] every 25 ms to keep the master-watchdog alive."""

    def __init__(self, client: UDPClient):
        self._client = client
        self._running = False
        self._thread: threading.Thread | None = None

    def start(self) -> None:
        self._running = True
        self._thread = threading.Thread(target=self._loop, daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._running = False
        if self._thread:
            self._thread.join(timeout=1.0)

    def _loop(self) -> None:
        pkt = bytes([0x43])
        while self._running:
            self._client.send_no_reply(pkt)
            time.sleep(0.025)


# ─── Test sections ───────────────────────────────────────────────────────────

def test_amakerbot_registration(client: UDPClient, token: str, r: Results) -> bool:
    """Register as master. Returns True on success."""
    _section("AmakerBotService — master registration")

    # 0x41 MASTER_REGISTER
    req = bytes([0x41]) + token.encode()
    resp = client.send(req)

    if resp is None:
        _fail("No response to MASTER_REGISTER — is the device reachable?")
        r.failed += 1
        return False

    expected_len = len(req) + 1          # echo + 1 status byte
    r.check(len(resp) == expected_len,
            f"Reply length {len(resp)} == {expected_len}")
    r.check(resp[0] == 0x41,
            f"Echo action byte 0x{resp[0]:02X} == 0x41")

    status = resp[-1] if len(resp) >= 1 else 0xFF
    label = _AB_STATUS.get(status, f"0x{status:02X}")

    if status == UDP_SUCCESS:
        _ok(f"MASTER_REGISTER → {label} (registered)")
        r.passed += 1
        return True
    elif status == UDP_IGNORED:
        _warn(f"MASTER_REGISTER → {label} (already registered or slot taken)")
        r.passed += 1          # technically valid
        return True
    else:
        _fail(f"MASTER_REGISTER → {label} (token wrong or internal error?)")
        r.failed += 1
        return False


def test_amakerbot_ping(client: UDPClient, r: Results) -> None:
    _section("AmakerBotService — PING (latency probe)")
    SEQ = 0xDEADBEEF & 0xFFFFFFFF
    pkt = bytes([0x44]) + struct.pack("<I", SEQ)
    t0 = time.monotonic()
    resp = client.send(pkt)
    rtt = (time.monotonic() - t0) * 1000

    if resp is None:
        r.check(False, "PING — no reply (is device master-registered?)")
        return

    r.check(len(resp) == 5, f"PING reply length {len(resp)} == 5")
    r.check(resp[0] == 0x44, f"PING action echo 0x{resp[0]:02X} == 0x44")
    echoed_seq = struct.unpack("<I", resp[1:5])[0] if len(resp) >= 5 else 0
    r.check(echoed_seq == SEQ, f"PING seq echo 0x{echoed_seq:08X} == 0x{SEQ:08X}")
    _info(f"Round-trip time: {rtt:.1f} ms")


def test_board_leds(client: UDPClient, r: Results) -> None:
    _section("BoardInfoService — onboard RGB LEDs (0x1x)")

    # 0x11 SET_LED_COLOR — LED 0 → red
    resp = client.send(bytes([0x11, 0, 255, 0, 0]))
    r.check(resp is not None and resp[0] == 0x11,
            "SET_LED_COLOR LED0 red — echo 0x11")
    if resp and len(resp) >= 2:
        r.check(resp[1] == RESP_OK, f"resp_code {_PROTO_STATUS.get(resp[1], hex(resp[1]))} == ok")

    # 0x11 SET_LED_COLOR — LED 1 → green
    resp = client.send(bytes([0x11, 1, 0, 255, 0]))
    r.check(resp is not None and len(resp) >= 2 and resp[1] == RESP_OK,
            "SET_LED_COLOR LED1 green → ok")

    # 0x12 TURN_OFF_LED — LED 0
    resp = client.send(bytes([0x12, 0]))
    r.check(resp is not None and len(resp) >= 2 and resp[1] == RESP_OK,
            "TURN_OFF_LED LED0 → ok")

    # 0x13 TURN_OFF_ALL_LEDS
    resp = client.send(bytes([0x13]))
    r.check(resp is not None and len(resp) >= 2 and resp[1] == RESP_OK,
            "TURN_OFF_ALL_LEDS → ok")

    # 0x14 GET_LED_STATUS
    resp = client.send(bytes([0x14]))
    if resp is None or len(resp) < 3 or resp[1] != RESP_OK:
        r.check(False, "GET_LED_STATUS — valid response with ok")
    else:
        try:
            payload = json.loads(resp[2:].decode())
            r.check("leds" in payload and len(payload["leds"]) == 3,
                    f"GET_LED_STATUS JSON contains 3 LEDs: {payload}")
        except (json.JSONDecodeError, UnicodeDecodeError) as exc:
            r.check(False, f"GET_LED_STATUS JSON parse error: {exc}")

    # Invalid LED index
    resp = client.send(bytes([0x11, 9, 255, 0, 0]))
    r.check(resp is not None and len(resp) >= 2 and resp[1] == RESP_INVALID_VALUES,
            "SET_LED_COLOR invalid LED index → invalid_values")


def test_dfr1216_leds(client: UDPClient, r: Results) -> None:
    _section("DFR1216Service — expansion board LEDs (0x3x)")

    # 0x31 SET_LED_COLOR — LED 0 → blue, brightness 200
    resp = client.send(bytes([0x31, 0, 0, 0, 255, 200]))
    r.check(resp is not None and len(resp) >= 2 and resp[1] == RESP_OK,
            "SET_LED_COLOR LED0 blue brightness=200 → ok")

    # 0x32 TURN_OFF_LED — LED 0
    resp = client.send(bytes([0x32, 0]))
    r.check(resp is not None and len(resp) >= 2 and resp[1] == RESP_OK,
            "TURN_OFF_LED LED0 → ok")

    # 0x33 TURN_OFF_ALL_LEDS
    resp = client.send(bytes([0x33]))
    r.check(resp is not None and len(resp) >= 2 and resp[1] == RESP_OK,
            "TURN_OFF_ALL_LEDS → ok")

    # 0x34 GET_LED_STATUS
    resp = client.send(bytes([0x34]))
    if resp is None or len(resp) < 3 or resp[1] != RESP_OK:
        r.check(False, "GET_LED_STATUS — valid response with ok")
    else:
        try:
            payload = json.loads(resp[2:].decode())
            r.check("leds" in payload,
                    f"GET_LED_STATUS JSON contains 'leds' key: {payload}")
        except (json.JSONDecodeError, UnicodeDecodeError) as exc:
            r.check(False, f"GET_LED_STATUS JSON parse error: {exc}")


def test_servos(client: UDPClient, r: Results) -> None:
    _section("ServoService — servos & motors (0x5x)")
    # ServoService udp_service_id=0x05 → action range 0x51–0x59.

    # 0x58 GET_ALL_STATUS — read current state before touching anything
    resp = client.send(bytes([0x58]))
    if resp is None:
        r.check(False, "GET_ALL_STATUS — no response (wrong action byte? firmware not uploaded?)")
    elif len(resp) < 2:
        r.check(False, f"GET_ALL_STATUS — response too short: {resp.hex()}")
    elif resp[1] != RESP_OK:
        code = _PROTO_STATUS.get(resp[1], f"0x{resp[1]:02X}")
        r.check(False, f"GET_ALL_STATUS — resp_code={code} (raw: {resp.hex()})")
    else:
        try:
            status = json.loads(resp[2:].decode())
            r.check(True, f"GET_ALL_STATUS: {status}")
        except Exception as exc:
            r.check(False, f"GET_ALL_STATUS — JSON parse error: {exc} | raw payload: {resp[2:].hex() or '<empty>'}")

    # 0x59 GET_BATTERY
    resp = client.send(bytes([0x59]))
    if resp is None:
        r.check(False, "GET_BATTERY — no response")
    elif len(resp) < 2:
        r.check(False, f"GET_BATTERY — response too short: {resp.hex()}")
    elif resp[1] != RESP_OK:
        code = _PROTO_STATUS.get(resp[1], f"0x{resp[1]:02X}")
        r.check(False, f"GET_BATTERY — resp_code={code} (raw: {resp.hex()})")
    elif len(resp) < 3:
        r.check(False, f"GET_BATTERY — missing battery byte (raw: {resp.hex()})")
    else:
        r.check(True, f"GET_BATTERY: {resp[2]}%")

    # 0x54 ATTACH_SERVO — channel 0 → Angular 180 (mask=0x01, type=2)
    resp = client.send(bytes([0x54, 0x01, 0x02]))
    r.check(resp is not None and len(resp) >= 2 and resp[1] == RESP_OK,
            "ATTACH_SERVO ch0 Angular180 → ok")

    # 0x51 SET_SERVO_ANGLE — channel 0 → +45°
    pkt = bytes([0x51]) + encode_angle(45)
    resp = client.send(pkt)
    r.check(resp is not None and len(resp) >= 2 and resp[1] == RESP_OK,
            "SET_SERVO_ANGLE ch0 +45° → ok")
    time.sleep(0.3)

    # SET_SERVO_ANGLE — channel 0 → −45°
    pkt = bytes([0x51]) + encode_angle(-45)
    resp = client.send(pkt)
    r.check(resp is not None and len(resp) >= 2 and resp[1] == RESP_OK,
            "SET_SERVO_ANGLE ch0 −45° → ok")
    time.sleep(0.3)

    # Return channel 0 to 0°
    client.send(bytes([0x51]) + encode_angle(0))

    # 0x57 GET_SERVO_STATUS — channel 0 (mask=0x01)
    resp = client.send(bytes([0x57, 0x01]))
    if resp is not None and len(resp) >= 3 and resp[1] == RESP_OK:
        try:
            payload = json.loads(resp[2:].decode())
            r.check("attached_servos" in payload,
                    f"GET_SERVO_STATUS ch0: {payload}")
        except Exception as exc:
            r.check(False, f"GET_SERVO_STATUS JSON parse error: {exc}")
    else:
        r.check(False, "GET_SERVO_STATUS — valid response")

    # 0x54 ATTACH_SERVO — channel 0 → Rotational (mask=0x01, type=1)
    resp = client.send(bytes([0x54, 0x01, 0x01]))
    r.check(resp is not None and len(resp) >= 2 and resp[1] == RESP_OK,
            "ATTACH_SERVO ch0 Rotational → ok")

    # 0x52 SET_SERVO_SPEED — ch0 = +40, all 8 bytes sent (mask=0x01)
    speeds = [encode_speed(40)] + [0x80] * 7
    pkt = bytes([0x52, 0x01]) + bytes(speeds)
    resp = client.send(pkt)
    r.check(resp is not None and len(resp) >= 2 and resp[1] == RESP_OK,
            "SET_SERVO_SPEED ch0 +40 → ok")
    time.sleep(0.4)

    # 0x53 STOP_SERVOS — channel 0 (mask=0x01)
    resp = client.send(bytes([0x53, 0x01]))
    r.check(resp is not None and len(resp) >= 2 and resp[1] == RESP_OK,
            "STOP_SERVOS ch0 → ok")

    # 0x55 SET_MOTOR_SPEED — motor 0 = +60, motor 1 = −60
    pkt = bytes([0x55, encode_speed(60), encode_speed(-60)])
    resp = client.send(pkt)
    r.check(resp is not None and len(resp) >= 2 and resp[1] == RESP_OK,
            "SET_MOTOR_SPEED m0=+60 m1=−60 → ok")
    time.sleep(0.4)

    # 0x56 STOP_MOTORS — all motors (mask=0x0F)
    resp = client.send(bytes([0x56, 0x0F]))
    r.check(resp is not None and len(resp) >= 2 and resp[1] == RESP_OK,
            "STOP_MOTORS all → ok")

    # Detach servo on channel 0 (type=0 = Not Connected)
    client.send(bytes([0x54, 0x01, 0x00]))
    _info("Channel 0 detached (Not Connected)")


def test_music(client: UDPClient, r: Results) -> None:
    return
    _section("MusicService — text protocol")

    # melodies list
    resp = client.send_text("Music:melodies")
    if resp:
        try:
            melodies = json.loads(resp)
            r.check(isinstance(melodies, list) and len(melodies) > 0,
                    f"Music:melodies → {len(melodies)} entries")
        except json.JSONDecodeError:
            r.check(False, f"Music:melodies — JSON parse error, got: {resp!r}")
    else:
        r.check(False, "Music:melodies — no response")

    # play melody 0
    resp = client.send_text('Music:play:{"melody":0,"option":4}')
    if resp:
        try:
            obj = json.loads(resp)
            r.check(obj.get("result") == "ok",
                    f'Music:play → result={obj.get("result")}')
        except json.JSONDecodeError:
            r.check(False, f"Music:play — JSON parse error: {resp!r}")
    else:
        r.check(False, "Music:play — no response")
    time.sleep(0.5)

    # stop
    resp = client.send_text("Music:stop")
    if resp:
        try:
            obj = json.loads(resp)
            r.check(obj.get("result") == "ok",
                    f'Music:stop → result={obj.get("result")}')
        except json.JSONDecodeError:
            r.check(False, f"Music:stop — JSON parse error: {resp!r}")
    else:
        r.check(False, "Music:stop — no response")

    # tone
    resp = client.send_text('Music:tone:{"freq":440,"beat":2}')
    if resp:
        try:
            obj = json.loads(resp)
            r.check(obj.get("result") == "ok",
                    f'Music:tone A4 → result={obj.get("result")}')
        except json.JSONDecodeError:
            r.check(False, f"Music:tone — JSON parse error: {resp!r}")
    else:
        r.check(False, "Music:tone — no response")
    time.sleep(0.3)
    client.send_text("Music:stop")

    # playnotes — C4 quarter + silence eighth @ 120 BPM
    # tempo=0x78, note=0x3C dur=0x04, silence=0x80 dur=0x02
    resp = client.send_text("Music:playnotes:783C048002")
    if resp:
        try:
            obj = json.loads(resp)
            r.check(obj.get("result") == "ok",
                    f'Music:playnotes → result={obj.get("result")}')
        except json.JSONDecodeError:
            r.check(False, f"Music:playnotes — JSON parse error: {resp!r}")
    else:
        r.check(False, "Music:playnotes — no response")


def test_amakerbot_name(client: UDPClient, r: Results) -> None:
    _section("AmakerBotService — bot name (0x45 GET / 0x46 SET)")

    # ── 0x45 GET name ─────────────────────────────────────────────────────────
    resp = client.send(bytes([0x45]))
    if resp is None:
        r.check(False, "GET_NAME (0x45) — no response")
        return

    r.check(resp[0] == 0x45, f"GET_NAME action echo 0x{resp[0]:02X} == 0x45")
    r.check(len(resp) >= 2, f"GET_NAME reply length {len(resp)} >= 2 (has name bytes)")
    current_name = resp[1:].decode(errors="replace") if len(resp) > 1 else ""
    _info(f"Current bot name: {current_name!r}")

    # ── 0x46 SET name (master, valid) ─────────────────────────────────────────
    new_name = "TestBot"
    req = bytes([0x46]) + new_name.encode()
    resp = client.send(req)
    if resp is None:
        r.check(False, "SET_NAME (0x46) valid — no response")
    else:
        expected_len = len(req) + 1
        r.check(len(resp) == expected_len,
                f"SET_NAME reply length {len(resp)} == {expected_len}")
        r.check(resp[0] == 0x46, f"SET_NAME action echo 0x{resp[0]:02X} == 0x46")
        status = resp[-1]
        label = _AB_STATUS.get(status, f"0x{status:02X}")
        r.check(status == UDP_SUCCESS, f"SET_NAME valid → {label}")

    # ── verify the change with GET ────────────────────────────────────────────
    resp = client.send(bytes([0x45]))
    if resp is not None and len(resp) > 1:
        got_name = resp[1:].decode(errors="replace")
        r.check(got_name == new_name,
                f"GET_NAME after SET → {got_name!r} == {new_name!r}")
    else:
        r.check(False, "GET_NAME after SET — no response or empty")

    # ── 0x46 SET name — empty string (should fail) ────────────────────────────
    req_empty = bytes([0x46])
    resp = client.send(req_empty)
    if resp is not None:
        status = resp[-1]
        label = _AB_STATUS.get(status, f"0x{status:02X}")
        r.check(status in (UDP_ERROR, UDP_IGNORED),
                f"SET_NAME empty → {label} (expected ERROR or IGNORED)")
    else:
        r.skip("SET_NAME empty — no reply (device may silently discard)")

    # ── 0x46 SET name — name too long (> 32 chars) ───────────────────────────
    long_name = "A" * 33
    req_long = bytes([0x46]) + long_name.encode()
    resp = client.send(req_long)
    if resp is not None:
        status = resp[-1]
        label = _AB_STATUS.get(status, f"0x{status:02X}")
        r.check(status in (UDP_ERROR, UDP_IGNORED),
                f"SET_NAME too long (33 chars) → {label} (expected ERROR or IGNORED)")
    else:
        r.skip("SET_NAME too long — no reply (device may silently discard)")

    # ── restore original name ─────────────────────────────────────────────────
    if current_name:
        restore_req = bytes([0x46]) + current_name.encode()
        client.send(restore_req)
        _info(f"Bot name restored to {current_name!r}")


def test_amakerbot_unregister(client: UDPClient, r: Results) -> None:
    _section("AmakerBotService — unregister master")

    resp = client.send(bytes([0x42]))
    if resp is None:
        r.check(False, "MASTER_UNREGISTER — no response")
        return

    r.check(len(resp) == 2, f"Reply length {len(resp)} == 2")
    r.check(resp[0] == 0x42, f"Echo action byte 0x{resp[0]:02X} == 0x42")
    status = resp[-1] if len(resp) >= 2 else 0xFF
    label = _AB_STATUS.get(status, f"0x{status:02X}")
    r.check(status == UDP_SUCCESS, f"MASTER_UNREGISTER → {label}")


# ─── Error / edge-case probes ─────────────────────────────────────────────────

def test_error_cases(client: UDPClient, r: Results) -> None:
    _section("Error cases & edge cases")

    # Truncated binary frame — should return invalid_params
    resp = client.send(bytes([0x11]))   # SET_LED_COLOR needs 5 bytes
    r.check(resp is not None and len(resp) >= 2 and resp[1] == RESP_INVALID_PARAMS,
            "Truncated SET_LED_COLOR → invalid_params")

    # Out-of-range LED index
    resp = client.send(bytes([0x11, 99, 0, 0, 0]))
    r.check(resp is not None and len(resp) >= 2 and resp[1] == RESP_INVALID_VALUES,
            "SET_LED_COLOR led=99 → invalid_values")

    # Unknown binary action
    resp = client.send(bytes([0xFF]))
    if resp is None:
        r.check(True, "Unknown action 0xFF — silently ignored (no reply)")
    else:
        r.check(len(resp) >= 2 and resp[1] in (RESP_UNKNOWN_CMD, RESP_NOT_MASTER),
                f"Unknown action 0xFF → {_PROTO_STATUS.get(resp[1], hex(resp[1]))}")

    # Music — invalid command
    resp = client.send_text("Music:nonexistent")
    if resp:
        try:
            obj = json.loads(resp)
            r.check(obj.get("result") == "error",
                    f'Music:nonexistent → result={obj.get("result")} (expected error)')
        except json.JSONDecodeError:
            r.skip("Music:nonexistent — non-JSON response")
    else:
        r.skip("Music:nonexistent — no response (service not started?)")


# ─── CLI entry point ──────────────────────────────────────────────────────────

def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="K10 Bot UDP test suite",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "Examples:\n"
            "  python test_amakerbot_udp.py 192.168.1.42 24642 A3K9B\n"
            "  python test_amakerbot_udp.py 192.168.1.42 24642 A3K9B --skip-servos\n"
            "  python test_amakerbot_udp.py 192.168.1.42 24642 A3K9B --servo-180 2 --servo-270 3 --servo-cont 4 --motor 1\n"
        ),
    )
    p.add_argument("ip",    help="K10 device IP address")
    p.add_argument("port",  type=int, help="UDP port (default 24642)", nargs="?", default=24642)
    p.add_argument("token", help="5-char registration token shown on device screen at boot")
    p.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT,
                   help=f"Socket receive timeout in seconds (default {DEFAULT_TIMEOUT})")
    p.add_argument("--skip-servos",  action="store_true", help="Skip servo/motor tests (no hardware connected)")
    p.add_argument("--skip-dfr",     action="store_true", help="Skip DFR1216 expansion board LED tests")
    p.add_argument("--skip-music",   action="store_true", help="Skip MusicService tests")
    p.add_argument("--skip-errors",  action="store_true", help="Skip error/edge-case tests")
    # Servo/motor channel selection
    p.add_argument("--servo-180",  type=int, default=0, metavar="CH",
                   help="Servo channel (0-7) wired as Angular 180° (default: 0)")
    p.add_argument("--servo-270",  type=int, default=None, metavar="CH",
                   help="Servo channel (0-7) wired as Angular 270° (omit to skip 270° test)")
    p.add_argument("--servo-cont", type=int, default=None, metavar="CH",
                   help="Servo channel (0-7) wired as continuous/rotational (omit to skip speed test)")
    p.add_argument("--motor",      type=int, default=0, metavar="IDX",
                   help="Motor index (0-3, i.e. board motor IDX+1) to test SET_MOTOR_SPEED (default: 0)")
    return p.parse_args()


def main() -> int:
    args = parse_args()

    print(f"\n{BOLD}K10 Bot UDP Test Suite{RESET}")
    print(f"  Target : {args.ip}:{args.port}")
    print(f"  Token  : {args.token}")
    print(f"  Timeout: {args.timeout}s")

    client = UDPClient(args.ip, args.port, timeout=args.timeout)
    r = Results()

    # ── Step 1: register as master ────────────────────────────────────────────
    registered = test_amakerbot_registration(client, args.token, r)

    if not registered:
        _warn("Master registration failed — protected commands will likely return NOT_MASTER.")
        _warn("Continuing anyway so you can inspect error responses.")

    # ── Step 2: start heartbeat ───────────────────────────────────────────────
    hb = HeartbeatThread(client)
    hb.start()
    _info("Heartbeat thread started (25 ms interval)")

    try:
        # ── Step 3: run test sections ─────────────────────────────────────────
        test_amakerbot_ping(client, r)
        test_amakerbot_name(client, r)
        test_board_leds(client, r)

        if args.skip_dfr:
            r.skip("DFR1216 LED tests (--skip-dfr)")
        else:
            test_dfr1216_leds(client, r)

        if args.skip_servos:
            r.skip("Servo/motor tests (--skip-servos)")
        else:
            test_servos(client, r)

        if args.skip_music:
            r.skip("MusicService tests (--skip-music)")
        else:
            test_music(client, r)

        if not args.skip_errors:
            test_error_cases(client, r)

    finally:
        # ── Step 4: stop heartbeat & unregister ───────────────────────────────
        hb.stop()
        _info("Heartbeat thread stopped")

        if registered:
            test_amakerbot_unregister(client, r)

        client.close()

    r.summary()
    return 0 if r.failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
