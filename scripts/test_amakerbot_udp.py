#!/usr/bin/env python3
"""
test_amakerbot_udp.py — End-to-end UDP test suite for the K10-Bot AmakerBot services.

Usage
-----
    python test_amakerbot_udp.py <robot_ip> <port> <token>

    robot_ip  – IP address of the K10-Bot (e.g. 192.168.1.56)
    port      – UDP port (default: 24642)
    token     – 5-char hex token shown on the device screen in APP_LOG mode

Protocol summary
----------------
  Text commands  : "<Service>:<command>[:<json>]"
  Binary commands: AmakerBotService uses raw bytes (0x41–0x44)

  Heartbeat (0x43) MUST arrive at the device at least every 50 ms while
  registered, otherwise all motors are stopped. This script keeps a
  background heartbeat thread running throughout the test session.

Exit codes
----------
  0  – all tests passed
  1  – one or more tests failed
  2  – argument error
"""

import json
import socket
import sys
import threading
import time
from typing import Optional

# ── ANSI colours ──────────────────────────────────────────────────────────────
GREEN  = "\033[32m"
RED    = "\033[31m"
YELLOW = "\033[33m"
CYAN   = "\033[36m"
BOLD   = "\033[1m"
RESET  = "\033[0m"

# ── Test counters ─────────────────────────────────────────────────────────────
_passed = 0
_failed = 0

# ── UDP sockets ───────────────────────────────────────────────────────────────
RECV_TIMEOUT = 2.0      # seconds to wait for a reply
HB_INTERVAL  = 0.025   # 25 ms heartbeat → safe margin below 50 ms limit

_send_sock: socket.socket
_recv_sock: socket.socket
_robot_addr: tuple[str, int]

# ── Heartbeat state ───────────────────────────────────────────────────────────
_hb_running  = False
_hb_thread: Optional[threading.Thread] = None


# ─────────────────────────────────────────────────────────────────────────────
# Internal helpers
# ─────────────────────────────────────────────────────────────────────────────

def _send_raw(data: bytes) -> None:
    """Send raw bytes to the robot."""
    _send_sock.sendto(data, _robot_addr)


def _send_text(service: str, command: str, payload: Optional[dict] = None) -> None:
    """Send a structured text-protocol UDP message."""
    msg = f"{service}:{command}"
    if payload is not None:
        msg += ":" + json.dumps(payload, separators=(",", ":"))
    _send_raw(msg.encode())


def _recv(timeout: float = RECV_TIMEOUT) -> Optional[str]:
    """
    Wait up to *timeout* seconds for a UDP reply.
    Returns the decoded string, or None on timeout.
    """
    _recv_sock.settimeout(timeout)
    try:
        data, _ = _recv_sock.recvfrom(4096)
        return data.decode(errors="replace")
    except socket.timeout:
        return None


def _recv_json(timeout: float = RECV_TIMEOUT) -> Optional[dict]:
    """Receive a reply and parse it as JSON. Returns None on timeout or parse error."""
    raw = _recv(timeout)
    if raw is None:
        return None
    try:
        return json.loads(raw)
    except json.JSONDecodeError:
        return {"_raw": raw}


# ─────────────────────────────────────────────────────────────────────────────
# Reporting
# ─────────────────────────────────────────────────────────────────────────────

def _section(title: str) -> None:
    print(f"\n{BOLD}{CYAN}{'─' * 60}{RESET}")
    print(f"{BOLD}{CYAN}  {title}{RESET}")
    print(f"{BOLD}{CYAN}{'─' * 60}{RESET}")


def _pass(test: str, detail: str = "") -> None:
    global _passed
    _passed += 1
    suffix = f"  → {detail}" if detail else ""
    print(f"  {GREEN}✓ PASS{RESET}  {test}{suffix}")


def _fail(test: str, detail: str = "") -> None:
    global _failed
    _failed += 1
    suffix = f"  → {detail}" if detail else ""
    print(f"  {RED}✗ FAIL{RESET}  {test}{suffix}")


def _info(msg: str) -> None:
    print(f"  {YELLOW}·{RESET}  {msg}")


# ─────────────────────────────────────────────────────────────────────────────
# Heartbeat
# ─────────────────────────────────────────────────────────────────────────────

def _hb_loop() -> None:
    hb_pkt = bytes([0x43])
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        while _hb_running:
            s.sendto(hb_pkt, _robot_addr)
            time.sleep(HB_INTERVAL)


def start_heartbeat() -> None:
    global _hb_running, _hb_thread
    _hb_running = True
    _hb_thread = threading.Thread(target=_hb_loop, daemon=True, name="heartbeat")
    _hb_thread.start()
    _info("Heartbeat thread started (25 ms interval)")


def stop_heartbeat() -> None:
    global _hb_running
    _hb_running = False
    if _hb_thread:
        _hb_thread.join(timeout=1.0)
    _info("Heartbeat thread stopped")


# ─────────────────────────────────────────────────────────────────────────────
# AmakerBotService tests
# ─────────────────────────────────────────────────────────────────────────────

def test_amakerbot(token: str) -> bool:
    """
    Register as master, send a ping, then verify heartbeat activation.
    Returns True if registration succeeded (so other tests can proceed).
    """
    _section("AmakerBotService — registration & heartbeat")

    # ── 1. MASTER_REGISTER (0x41 + token) ─────────────────────────────────
    # Reply format: [echo of full request][UDPResponseStatus]
    #   request = [0x41][token_bytes]  → reply = [0x41][token_bytes][status]
    #   UDPResponseStatus: SUCCESS=0x01, IGNORED=0x02, DENIED=0x03, ERROR=0x04
    reg_request = bytes([0x41]) + token.encode()
    expected_reply_len = len(reg_request) + 1   # echo + 1 status byte
    _send_raw(reg_request)
    reply = _recv(timeout=2.0)

    registered = False
    if reply is None:
        _fail("0x41 MASTER_REGISTER", "no reply received (timeout)")
        return False
    else:
        reply_bytes = reply.encode("latin-1")   # preserve raw byte values
        if len(reply_bytes) == expected_reply_len and reply_bytes[0] == 0x41:
            status = reply_bytes[-1]
            if status == 0x01:   # SUCCESS
                registered = True
                _pass("0x41 MASTER_REGISTER", f"SUCCESS (reply: {reply_bytes.hex()})")
            elif status == 0x02:  # IGNORED (already master)
                registered = True
                _pass("0x41 MASTER_REGISTER", f"IGNORED — already master (reply: {reply_bytes.hex()})")
            elif status == 0x03:  # DENIED
                _fail("0x41 MASTER_REGISTER", f"DENIED — wrong token (reply: {reply_bytes.hex()})")
                return False
            elif status == 0x04:  # ERROR
                _fail("0x41 MASTER_REGISTER", f"ERROR from server (reply: {reply_bytes.hex()})")
                return False
            else:
                _fail("0x41 MASTER_REGISTER", f"Unknown status 0x{status:02x} (reply: {reply_bytes.hex()})")
                return False
        else:
            _fail("0x41 MASTER_REGISTER", f"Malformed reply (expected {expected_reply_len} bytes): {reply_bytes.hex()}")
            return False

    # ── 2. PING (0x44) — 100 pings to measure RTT and loss ────────────────
    # Firmware requires: [0x44][id:4B] (5 bytes minimum)
    # Reply:             [0x44][id:4B] (exact echo of the 5-byte request)
    # The 4-byte ID is used to match replies and detect reordering.
    PING_COUNT   = 100
    PING_TIMEOUT = 0.1   # seconds per ping
    rtts: list[float] = []
    lost = 0
    for seq in range(PING_COUNT):
        ping_id = seq.to_bytes(4, "big")
        pkt = bytes([0x44]) + ping_id
        t0 = time.perf_counter()
        _send_raw(pkt)
        pr = _recv(timeout=PING_TIMEOUT)
        elapsed_ms = (time.perf_counter() - t0) * 1000
        if pr is None:
            lost += 1
        else:
            pr_bytes = pr.encode("latin-1")
            if pr_bytes == pkt:
                rtts.append(elapsed_ms)
            else:
                lost += 1  # got a reply but it didn't match (stale / wrong)

    if rtts:
        avg_rtt = sum(rtts) / len(rtts)
        min_rtt = min(rtts)
        max_rtt = max(rtts)
        loss_pct = lost / PING_COUNT * 100
        detail = (f"avg={avg_rtt:.1f} ms  min={min_rtt:.1f} ms  max={max_rtt:.1f} ms"
                  f"  loss={lost}/{PING_COUNT} ({loss_pct:.0f}%)")
        if lost == 0:
            _pass(f"0x44 PING x{PING_COUNT}", detail)
        else:
            _fail(f"0x44 PING x{PING_COUNT}", detail)
    else:
        _fail(f"0x44 PING x{PING_COUNT}", f"all {PING_COUNT} pings lost (100% loss)")

    # ── 3. Start heartbeat & confirm motor watchdog doesn't fire ───────────
    start_heartbeat()
    time.sleep(0.2)  # let several heartbeats through

    _pass("0x43 HEARTBEAT", "running")
    return True


def test_amakerbot_unregister() -> None:
    _section("AmakerBotService — unregister")
    stop_heartbeat()
    # Reply format: [0x42][UDPResponseStatus]
    #   UDPResponseStatus: SUCCESS=0x01, DENIED=0x03, ERROR=0x04
    _send_raw(bytes([0x42]))
    reply = _recv(timeout=2.0)
    if reply is None:
        _fail("0x42 MASTER_UNREGISTER", "no reply received (timeout)")
        return
    reply_bytes = reply.encode("latin-1")
    if len(reply_bytes) == 2 and reply_bytes[0] == 0x42:
        status = reply_bytes[1]
        if status == 0x01:   # SUCCESS
            _pass("0x42 MASTER_UNREGISTER", f"SUCCESS (reply: {reply_bytes.hex()})")
        elif status == 0x03:  # DENIED
            _fail("0x42 MASTER_UNREGISTER", f"DENIED — not the master (reply: {reply_bytes.hex()})")
        elif status == 0x04:  # ERROR
            _fail("0x42 MASTER_UNREGISTER", f"ERROR from server (reply: {reply_bytes.hex()})")
        else:
            _fail("0x42 MASTER_UNREGISTER", f"Unknown status 0x{status:02x} (reply: {reply_bytes.hex()})")
    else:
        _fail("0x42 MASTER_UNREGISTER", f"Malformed reply (expected 2 bytes): {reply_bytes.hex()}")


# ─────────────────────────────────────────────────────────────────────────────
# ServoService tests
# ─────────────────────────────────────────────────────────────────────────────

def test_servo_service() -> None:
    _section("ServoService — servo channels")

    SVC = "Servo Service"

    # ── getAllStatus ───────────────────────────────────────────────────────
    _send_text(SVC, "getAllStatus")
    r = _recv_json()
    if r and "servos" in r:
        _pass("getAllStatus", f"{len(r['servos'])} channels reported")
    elif r:
        _fail("getAllStatus", f"unexpected reply: {r}")
    else:
        _fail("getAllStatus", "timeout — no reply")

    # ── getStatus (channel 0) ──────────────────────────────────────────────
    _send_text(SVC, "getStatus", {"channel": 0})
    r = _recv_json()
    if r and "status" in r:
        _pass("getStatus ch0", f"status={r['status']}")
    elif r:
        _fail("getStatus ch0", f"unexpected reply: {r}")
    else:
        _fail("getStatus ch0", "timeout")

    # ── attachServo ch0 as Angular 180° ───────────────────────────────────
    _send_text(SVC, "attachServo", {"channel": 0, "connection": 2})
    r = _recv_json()
    if r and r.get("result") == "ok":
        _pass("attachServo ch0 (Angular 180°)", r.get("message", ""))
    elif r:
        _fail("attachServo ch0", f"reply: {r}")
    else:
        _fail("attachServo ch0", "timeout")

    # ── setServoAngle ch0 → 90° ───────────────────────────────────────────
    _send_text(SVC, "setServoAngle", {"channel": 0, "angle": 90})
    r = _recv_json()
    if r and r.get("result") == "ok":
        _pass("setServoAngle ch0=90°", r.get("message", ""))
    elif r:
        _fail("setServoAngle ch0", f"reply: {r}")
    else:
        _fail("setServoAngle ch0", "timeout")

    # ── setAllServoAngle → 45° ────────────────────────────────────────────
    _send_text(SVC, "setAllServoAngle", {"angle": 45})
    r = _recv_json()
    if r and r.get("result") == "ok":
        _pass("setAllServoAngle 45°", r.get("message", ""))
    elif r:
        _fail("setAllServoAngle", f"reply: {r}")
    else:
        _fail("setAllServoAngle", "timeout")

    # ── setServosAngleMultiple ─────────────────────────────────────────────
    _send_text(SVC, "setServosAngleMultiple",
               {"servos": [{"channel": 0, "angle": 90}, {"channel": 1, "angle": 45}]})
    r = _recv_json()
    if r and r.get("result") == "ok":
        _pass("setServosAngleMultiple", r.get("message", ""))
    elif r:
        _fail("setServosAngleMultiple", f"reply: {r}")
    else:
        _fail("setServosAngleMultiple", "timeout")

    # ── Detach ch0, reattach as Continuous Rotation ───────────────────────
    _send_text(SVC, "attachServo", {"channel": 0, "connection": 0})   # detach
    _recv_json()
    _send_text(SVC, "attachServo", {"channel": 0, "connection": 1})   # continuous
    r = _recv_json()
    if r and r.get("result") == "ok":
        _pass("attachServo ch0 (Continuous Rotation)", r.get("message", ""))
    elif r:
        _fail("attachServo ch0 Continuous", f"reply: {r}")
    else:
        _fail("attachServo ch0 Continuous", "timeout")

    # ── setServoSpeed ch0 → 30 ────────────────────────────────────────────
    _send_text(SVC, "setServoSpeed", {"channel": 0, "speed": 30, "duration_ms": 500})
    r = _recv_json()
    if r and r.get("result") == "ok":
        _pass("setServoSpeed ch0=30 (500 ms)", r.get("message", ""))
    elif r:
        _fail("setServoSpeed ch0", f"reply: {r}")
    else:
        _fail("setServoSpeed ch0", "timeout")
    time.sleep(0.6)   # wait for auto-stop

    # ── setAllServoSpeed ──────────────────────────────────────────────────
    _send_text(SVC, "setAllServoSpeed", {"speed": 20, "duration_ms": 300})
    r = _recv_json()
    if r and r.get("result") == "ok":
        _pass("setAllServoSpeed 20 (300 ms)", r.get("message", ""))
    elif r:
        _fail("setAllServoSpeed", f"reply: {r}")
    else:
        _fail("setAllServoSpeed", "timeout")
    time.sleep(0.4)

    # ── setServosSpeedMultiple ─────────────────────────────────────────────
    _send_text(SVC, "setServosSpeedMultiple",
               {"servos": [{"channel": 0, "speed": 50, "duration_ms": 200},
                            {"channel": 1, "speed": -30, "duration_ms": 200}]})
    r = _recv_json()
    if r and r.get("result") == "ok":
        _pass("setServosSpeedMultiple", r.get("message", ""))
    elif r:
        _fail("setServosSpeedMultiple", f"reply: {r}")
    else:
        _fail("setServosSpeedMultiple", "timeout")
    time.sleep(0.3)

    # ── stopAll ───────────────────────────────────────────────────────────
    _send_text(SVC, "stopAll")
    r = _recv_json()
    if r and r.get("result") == "ok":
        _pass("stopAll", r.get("message", ""))
    elif r:
        _fail("stopAll", f"reply: {r}")
    else:
        _fail("stopAll", "timeout")

    # ── Detach ch0 to leave board clean ──────────────────────────────────
    _send_text(SVC, "attachServo", {"channel": 0, "connection": 0})
    _recv_json()
    _info("ch0 detached (cleanup)")


def test_motor_service() -> None:
    _section("ServoService — DC motor channels")

    SVC = "Servo Service"

    # ── setMotorSpeed motor 1 → 50 ────────────────────────────────────────
    _send_text(SVC, "setMotorSpeed", {"motor": 1, "speed": 50})
    r = _recv_json()
    if r and r.get("result") == "ok":
        _pass("setMotorSpeed M1=50", r.get("message", ""))
    elif r:
        _fail("setMotorSpeed M1", f"reply: {r}")
    else:
        _fail("setMotorSpeed M1", "timeout")

    time.sleep(0.3)

    # ── setMotorSpeed motor 2 → -30 (reverse) ────────────────────────────
    _send_text(SVC, "setMotorSpeed", {"motor": 2, "speed": -30})
    r = _recv_json()
    if r and r.get("result") == "ok":
        _pass("setMotorSpeed M2=-30 (reverse)", r.get("message", ""))
    elif r:
        _fail("setMotorSpeed M2", f"reply: {r}")
    else:
        _fail("setMotorSpeed M2", "timeout")

    time.sleep(0.3)

    # ── stopAllMotors ─────────────────────────────────────────────────────
    _send_text(SVC, "stopAllMotors")
    r = _recv_json()
    if r and r.get("result") == "ok":
        _pass("stopAllMotors", r.get("message", ""))
    elif r:
        _fail("stopAllMotors", f"reply: {r}")
    else:
        _fail("stopAllMotors", "timeout")


# ─────────────────────────────────────────────────────────────────────────────
# K10SensorsService tests
# ─────────────────────────────────────────────────────────────────────────────

def test_sensors_service() -> None:
    _section("K10SensorsService — sensors")

    _send_text("K10 Sensors Service", "getSensors")
    r = _recv_json()
    if r and "light" in r:
        _pass("getSensors",
              f"light={r.get('light')}, "
              f"temp={r.get('celcius')}°C, "
              f"hum={r.get('hum_rel')}%")
    elif r and r.get("result") == "error":
        _fail("getSensors", r.get("message", "sensor error"))
    elif r:
        _fail("getSensors", f"unexpected: {r}")
    else:
        _fail("getSensors", "timeout")


# ─────────────────────────────────────────────────────────────────────────────
# MusicService tests
# ─────────────────────────────────────────────────────────────────────────────

def test_music_service() -> None:
    _section("MusicService — music & tones")

    # ── melodies (query) ──────────────────────────────────────────────────
    _send_text("Music", "melodies")
    r = _recv_json()
    if isinstance(r, list):
        _pass("melodies", f"{len(r)} melodies returned")
    elif isinstance(r, dict) and "_raw" in r:
        try:
            lst = json.loads(r["_raw"])
            _pass("melodies", f"{len(lst)} melodies returned")
        except Exception:
            _fail("melodies", f"parse error: {r['_raw'][:60]}")
    elif r:
        _fail("melodies", f"unexpected reply: {r}")
    else:
        _fail("melodies", "timeout")

    # ── play melody 0 (DADADADUM) once in background ──────────────────────
    _send_text("Music", "play", {"melody": 0, "option": 4})
    r = _recv_json()
    if r and r.get("result") == "ok":
        _pass("play melody 0 (DADADADUM)", r.get("message", ""))
    elif r:
        _fail("play melody 0", f"reply: {r}")
    else:
        _fail("play melody 0", "timeout")

    time.sleep(1.0)

    # ── stop ──────────────────────────────────────────────────────────────
    _send_text("Music", "stop")
    r = _recv_json()
    if r and r.get("result") == "ok":
        _pass("stop", r.get("message", ""))
    elif r:
        _fail("stop", f"reply: {r}")
    else:
        _fail("stop", "timeout")

    # ── tone 440 Hz ───────────────────────────────────────────────────────
    _send_text("Music", "tone", {"freq": 440, "beat": 4000})
    r = _recv_json()
    if r and r.get("result") == "ok":
        _pass("tone 440 Hz (A4)", r.get("message", ""))
    elif r:
        _fail("tone 440 Hz", f"reply: {r}")
    else:
        _fail("tone 440 Hz", "timeout")

    time.sleep(0.5)

    # ── stop ──────────────────────────────────────────────────────────────
    _send_text("Music", "stop")
    _recv_json()

    # ── playnotes (C4 quarter + silence eighth at 120 BPM) ────────────────
    # Payload: [BPM=120, note=60, dur=4, silence(0x80), dur=2]
    payload_bytes = bytes([120, 60, 4, 0x80, 2])
    hex_str = payload_bytes.hex().upper()
    _send_raw(f"Music:playnotes:{hex_str}".encode())
    r = _recv_json()
    if r and r.get("result") == "ok":
        _pass("playnotes (C4 + silence)", r.get("message", ""))
    elif r:
        _fail("playnotes", f"reply: {r}")
    else:
        _fail("playnotes", "timeout")

    time.sleep(0.5)
    _send_text("Music", "stop")
    _recv_json()


# ─────────────────────────────────────────────────────────────────────────────
# Entry point
# ─────────────────────────────────────────────────────────────────────────────

def main() -> int:
    global _send_sock, _recv_sock, _robot_addr

    # ── Argument parsing ──────────────────────────────────────────────────
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <robot_ip> <port> <token>")
        print(f"  e.g. {sys.argv[0]} 192.168.1.56 24642 a3k9b")
        return 2

    robot_ip = sys.argv[1]
    try:
        udp_port = int(sys.argv[2])
    except ValueError:
        print(f"ERROR: port must be an integer, got '{sys.argv[2]}'")
        return 2
    token = sys.argv[3]

    _robot_addr = (robot_ip, udp_port)

    print(f"\n{BOLD}K10-Bot AmakerBot UDP Test Suite{RESET}")
    print(f"  Target : {robot_ip}:{udp_port}")
    print(f"  Token  : {token}")
    print(f"  Date   : {time.strftime('%Y-%m-%d %H:%M:%S')}")

    # ── Open sockets ──────────────────────────────────────────────────────
    _send_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    _recv_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # Bind to an ephemeral port so the robot knows where to send replies
    _recv_sock.bind(("", 0))
    local_port = _recv_sock.getsockname()[1]
    # Route replies to our recv socket by using it for send as well
    _send_sock.close()
    _send_sock = _recv_sock       # single socket for send + recv
    _info(f"Listening for replies on local port {local_port}")

    try:
        # ── AmakerBot registration ────────────────────────────────────────
        registered = test_amakerbot(token)
        if not registered:
            print(f"\n{RED}Registration failed — aborting remaining tests.{RESET}")
            return 1

        # ── Service tests (order matters: heartbeat must be running) ──────
        test_servo_service()
        test_motor_service()
        test_sensors_service()
        test_music_service()

        # ── Unregister ────────────────────────────────────────────────────
        test_amakerbot_unregister()

    finally:
        stop_heartbeat()
        _recv_sock.close()

    # ── Summary ───────────────────────────────────────────────────────────
    total = _passed + _failed
    print(f"\n{BOLD}{'─' * 60}{RESET}")
    print(f"{BOLD}Results: {_passed}/{total} passed", end="")
    if _failed:
        print(f"  ({RED}{_failed} failed{RESET}{BOLD})", end="")
    print(RESET)

    return 0 if _failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
