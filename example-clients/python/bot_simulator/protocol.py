"""
Bot Simulator — UDP Protocol Handler
=====================================

Simulates the K10 Bot binary protocol over UDP.

Supported service commands
--------------------------
AmakerBotService (0x4N):
  0x41  MASTER_REGISTER     — validates token, sets master IP
  0x42  MASTER_UNREGISTER   — releases master
  0x43  HEARTBEAT           — keepalive (no reply when authorised)
  0x44  PING                — 4-byte echo
  0x45  GET_NAME            — returns bot name
  0x46  SET_NAME            — updates bot name

MotorServoService (0x2N):
  0x21  SET_MOTORS_SPEED    — [mask:u8][speed:i8]
  0x22  SET_SERVO_TYPE      — [mask:u8][type:u8] activates channels
  0x23  SET_SERVOS_SPEED    — [mask:u8][speed:i8] (continuous only)
  0x24  SET_SERVOS_ANGLE    — [mask:u8][angle_hi:u8][angle_lo:u8] big-endian i16
  0x28  STOP_ALL_MOTORS     — zeros all motor speeds

Threading model
---------------
``BotSimulatorProtocol.start()`` spawns a daemon thread for the UDP socket.
All state changes are safe to read from the main thread between frames because
individual attribute writes are GIL-atomic on CPython; no extra locking needed
for simple int/float reads.

Events are delivered via the ``on_event(event_type, data)`` callable which is
called from the background thread — callers must post to a queue if the
callback accesses a GUI toolkit.
"""

from __future__ import annotations

import math
import socket
import struct
import threading
import time
from dataclasses import dataclass, field
from enum import IntEnum
from typing import Callable, Dict, Optional, Tuple


# ---------------------------------------------------------------------------
# Response status codes (AmakerBotService)
# ---------------------------------------------------------------------------

STATUS_SUCCESS: int = 0x00
STATUS_IGNORED: int = 0x01
STATUS_DENIED:  int = 0x02
STATUS_ERROR:   int = 0x03

_STATUS_NAMES: Dict[int, str] = {
    STATUS_SUCCESS: "SUCCESS",
    STATUS_IGNORED: "IGNORED",
    STATUS_DENIED:  "DENIED",
    STATUS_ERROR:   "ERROR",
}


# ---------------------------------------------------------------------------
# Domain types
# ---------------------------------------------------------------------------

class ServoType(IntEnum):
    """Operating mode of a servo channel — mirrors the firmware enum."""
    SERVO_180  = 0   # Standard 180° positional servo
    SERVO_270  = 1   # Wide-angle 270° positional servo
    CONTINUOUS = 2   # Continuous-rotation servo (speed-controlled)


@dataclass
class ServoState:
    """Live state for one servo channel (0–5)."""
    channel_id: int
    servo_type: ServoType = ServoType.SERVO_180
    angle: float = 90.0   # degrees  [0, max_angle]   — positional servos
    speed: int   = 0       # −100…+100              — continuous servos
    active: bool = False   # True when the channel is "connected" in the UI


@dataclass
class MotorState:
    """Live state for one DC motor channel (1–4)."""
    motor_id: int   # 1–4
    speed: int = 0  # −100…+100


# ---------------------------------------------------------------------------
# Protocol simulator
# ---------------------------------------------------------------------------

class BotSimulatorProtocol:
    """
    UDP server that fully implements the K10 Bot binary protocol.

    Usage::

        proto = BotSimulatorProtocol(port=24642, on_event=my_callback)
        proto.start()
        # … run GUI …
        proto.stop()

    The ``on_event`` callback receives ``(event_type: str, data)`` calls:

    * ``("log",           dict)``  — a message was received or sent
    * ``("state_changed", None)``  — a servo/motor state was updated
    * ``("master_changed", ip_or_None)`` — master registration changed
    * ``("error",         str)``   — unhandled exception in the recv loop
    """

    TOKEN:    str = "sim01"
    BOT_NAME: str = "Bot-Simulator"

    # Commands whose receipt should NOT be logged (prevents log flooding).
    # 0x43 HEARTBEAT fires every 40 ms when a master is connected.
    _SILENT_CMDS: frozenset = frozenset({0x43})

    def __init__(
        self,
        port: int = 24642,
        on_event: Optional[Callable] = None,
    ) -> None:
        self.port     = port
        self.on_event = on_event or (lambda *a: None)

        # Session state
        self.master_ip:      Optional[str] = None
        self.last_heartbeat: float         = 0.0
        self.bot_name:       str           = self.BOT_NAME

        # Hardware state
        self.servos: Dict[int, ServoState] = {
            i: ServoState(channel_id=i) for i in range(6)
        }
        self.motors: Dict[int, MotorState] = {
            i + 1: MotorState(motor_id=i + 1) for i in range(4)
        }

        self._sock:    Optional[socket.socket]    = None
        self._thread:  Optional[threading.Thread] = None
        self._running: bool                        = False

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    def start(self) -> None:
        """Bind the UDP socket and start the receive loop thread."""
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._sock.bind(("0.0.0.0", self.port))
        self._sock.settimeout(0.5)
        self._running = True
        self._thread = threading.Thread(
            target=self._run,
            daemon=True,
            name="bot-sim-udp",
        )
        self._thread.start()

    def stop(self) -> None:
        """Signal the receive loop to exit and close the socket."""
        self._running = False
        if self._sock:
            try:
                self._sock.close()
            except OSError:
                pass

    def simulate_receive(self, data: bytes, addr: Tuple[str, int]) -> None:
        """Inject a packet as if it were received from *addr* (feature #10 — packet inject)."""
        if data:
            self._dispatch(data, addr)

    # ------------------------------------------------------------------
    # Receive loop
    # ------------------------------------------------------------------

    def _run(self) -> None:
        while self._running:
            try:
                data, addr = self._sock.recvfrom(256)
                if data:
                    self._dispatch(data, addr)
            except socket.timeout:
                continue
            except OSError:
                break
            except Exception as exc:  # noqa: BLE001
                if self._running:
                    self.on_event("error", str(exc))

    # ------------------------------------------------------------------
    # Dispatch
    # ------------------------------------------------------------------

    def _dispatch(self, data: bytes, addr: Tuple[str, int]) -> None:
        cmd = data[0]

        if cmd not in self._SILENT_CMDS:
            self._log_in(data, addr, self._describe(data))

        handlers = {
            0x41: self._cmd_master_register,
            0x42: self._cmd_master_unregister,
            0x43: self._cmd_heartbeat,
            0x44: self._cmd_ping,
            0x45: self._cmd_get_name,
            0x46: self._cmd_set_name,
            0x21: self._cmd_set_motors_speed,
            0x22: self._cmd_set_servo_type,
            0x23: self._cmd_set_servos_speed,
            0x24: self._cmd_set_servos_angle,
            0x28: self._cmd_stop_all_motors,
        }
        handler = handlers.get(cmd)
        if handler:
            handler(data, addr)
        else:
            self._log_warn(f"Unknown command 0x{cmd:02X}  [{data.hex(' ')}]")

    # ------------------------------------------------------------------
    # AmakerBotService handlers
    # ------------------------------------------------------------------

    def _cmd_master_register(self, data: bytes, addr: Tuple[str, int]) -> None:
        if len(data) < 2:
            self._udp_reply(data, addr, STATUS_IGNORED)
            return
        token = data[1:].decode("ascii", errors="replace")
        if token != self.TOKEN:
            self._udp_reply(data, addr, STATUS_DENIED)
            self._log_warn(f"Registration DENIED — bad token '{token}' (expected '{self.TOKEN}')")
            return
        self.master_ip = addr[0]
        self._udp_reply(data, addr, STATUS_SUCCESS)
        self.on_event("master_changed", addr[0])

    def _cmd_master_unregister(self, data: bytes, addr: Tuple[str, int]) -> None:
        if not self._is_master(addr):
            self._udp_reply(data, addr, STATUS_DENIED)
            return
        self.master_ip = None
        self._udp_reply(data, addr, STATUS_SUCCESS)
        self.on_event("master_changed", None)

    def _cmd_heartbeat(self, data: bytes, addr: Tuple[str, int]) -> None:
        if not self._is_master(addr):
            self._udp_reply(data, addr, STATUS_DENIED)
            return
        self.last_heartbeat = time.monotonic()
        # No reply when authorised — fire-and-forget

    def _cmd_ping(self, data: bytes, addr: Tuple[str, int]) -> None:
        if not self._is_master(addr):
            return  # not claimed — no reply
        if len(data) >= 5:
            reply = data[:5]
            self._send(reply, addr, "PING echo")

    def _cmd_get_name(self, data: bytes, addr: Tuple[str, int]) -> None:
        reply = bytes([0x45]) + self.bot_name.encode("ascii")
        self._send(reply, addr, f"GET_NAME → '{self.bot_name}'")

    def _cmd_set_name(self, data: bytes, addr: Tuple[str, int]) -> None:
        if not self._is_master(addr):
            self._udp_reply(data, addr, STATUS_DENIED)
            return
        if len(data) < 2:
            self._udp_reply(data, addr, STATUS_ERROR)
            return
        name = data[1:].decode("ascii", errors="replace").strip()
        if not name or len(name) > 32:
            self._udp_reply(data, addr, STATUS_ERROR)
            return
        self.bot_name = name
        self._udp_reply(data, addr, STATUS_SUCCESS)

    # ------------------------------------------------------------------
    # MotorServoService handlers
    # ------------------------------------------------------------------

    def _cmd_set_motors_speed(self, data: bytes, addr: Tuple[str, int]) -> None:
        if len(data) < 3:
            return
        mask  = data[1]
        speed = struct.unpack("b", bytes([data[2]]))[0]  # signed i8
        for i in range(4):
            if mask & (1 << i):
                self.motors[i + 1].speed = speed
        self.on_event("state_changed", None)

    def _cmd_set_servo_type(self, data: bytes, addr: Tuple[str, int]) -> None:
        if len(data) < 3:
            return
        mask       = data[1]
        try:
            stype  = ServoType(data[2])
        except ValueError:
            self._log_warn(f"SET_SERVO_TYPE: unknown type 0x{data[2]:02X}")
            return
        for i in range(6):
            if mask & (1 << i):
                self.servos[i].servo_type = stype
                self.servos[i].active     = True
        self.on_event("state_changed", None)

    def _cmd_set_servos_speed(self, data: bytes, addr: Tuple[str, int]) -> None:
        if len(data) < 3:
            return
        mask  = data[1]
        speed = struct.unpack("b", bytes([data[2]]))[0]
        for i in range(6):
            if mask & (1 << i):
                self.servos[i].speed = speed
        self.on_event("state_changed", None)

    def _cmd_set_servos_angle(self, data: bytes, addr: Tuple[str, int]) -> None:
        if len(data) < 4:
            return
        mask  = data[1]
        angle = struct.unpack(">h", bytes([data[2], data[3]]))[0]  # big-endian i16
        for i in range(6):
            if mask & (1 << i):
                self.servos[i].angle = float(angle)
        self.on_event("state_changed", None)

    def _cmd_stop_all_motors(self, data: bytes, addr: Tuple[str, int]) -> None:
        for motor in self.motors.values():
            motor.speed = 0
        self.on_event("state_changed", None)

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    def _is_master(self, addr: Tuple[str, int]) -> bool:
        return self.master_ip is not None and self.master_ip == addr[0]

    def _udp_reply(self, data: bytes, addr: Tuple[str, int], status: int) -> None:
        """Echo the full incoming message followed by the status byte."""
        reply = data + bytes([status])
        label = _STATUS_NAMES.get(status, f"0x{status:02X}")
        self._send(reply, addr, f"→ {label}")

    def _send(self, data: bytes, addr: Tuple[str, int], desc: str = "") -> None:
        try:
            self._sock.sendto(data, addr)
            self._log_out(data, addr, desc)
        except OSError:
            pass

    def _log_in(self, data: bytes, addr: Tuple[str, int], desc: str) -> None:
        self.on_event("log", {
            "direction": "in",
            "addr":      addr,
            "data":      data,
            "desc":      desc,
        })

    def _log_out(self, data: bytes, addr: Tuple[str, int], desc: str) -> None:
        self.on_event("log", {
            "direction": "out",
            "addr":      addr,
            "data":      data,
            "desc":      desc,
        })

    def _log_warn(self, msg: str) -> None:
        self.on_event("log", {
            "direction": "warn",
            "addr":      None,
            "data":      b"",
            "desc":      msg,
        })

    # ------------------------------------------------------------------
    # Human-readable command description
    # ------------------------------------------------------------------

    _CMD_NAMES: Dict[int, str] = {
        0x41: "MASTER_REGISTER",   0x42: "MASTER_UNREGISTER",
        0x43: "HEARTBEAT",         0x44: "PING",
        0x45: "GET_NAME",          0x46: "SET_NAME",
        0x21: "SET_MOTORS_SPEED",  0x22: "SET_SERVO_TYPE",
        0x23: "SET_SERVOS_SPEED",  0x24: "SET_SERVOS_ANGLE",
        0x28: "STOP_ALL_MOTORS",
    }

    def _describe(self, data: bytes) -> str:
        if not data:
            return ""
        cmd      = data[0]
        name     = self._CMD_NAMES.get(cmd, f"?0x{cmd:02X}?")
        hex_part = " ".join(f"{b:02X}" for b in data)
        detail   = self._decode_payload(data)
        if detail:
            return f"{name}  [{hex_part}]  {detail}"
        return f"{name}  [{hex_part}]"

    def _decode_payload(self, data: bytes) -> str:
        """Return a short human-readable payload summary."""
        if not data:
            return ""
        cmd = data[0]
        try:
            if cmd == 0x41 and len(data) >= 2:
                return f"token='{data[1:].decode('ascii', errors='replace')}'"
            if cmd == 0x21 and len(data) >= 3:
                mask  = data[1]
                speed = struct.unpack("b", bytes([data[2]]))[0]
                motors = [str(i + 1) for i in range(4) if mask & (1 << i)]
                return f"motors={motors}  speed={speed:+d}"
            if cmd == 0x22 and len(data) >= 3:
                mask  = data[1]
                stype = ServoType(data[2]).name if data[2] in ServoType._value2member_map_ else f"0x{data[2]:02X}"
                chs   = [str(i) for i in range(6) if mask & (1 << i)]
                return f"channels={chs}  type={stype}"
            if cmd == 0x23 and len(data) >= 3:
                mask  = data[1]
                speed = struct.unpack("b", bytes([data[2]]))[0]
                chs   = [str(i) for i in range(6) if mask & (1 << i)]
                return f"channels={chs}  speed={speed:+d}"
            if cmd == 0x24 and len(data) >= 4:
                mask  = data[1]
                angle = struct.unpack(">h", bytes([data[2], data[3]]))[0]
                chs   = [str(i) for i in range(6) if mask & (1 << i)]
                return f"channels={chs}  angle={angle}°"
        except Exception:  # noqa: BLE001
            pass
        return ""
