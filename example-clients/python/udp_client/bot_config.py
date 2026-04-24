"""
Bot configuration and action mapping.

Defines named motors/servos and maps action names to binary UDP packets
following the MotorServoService protocol (service_id = 0x02).

Binary protocol summary (action byte = (0x02 << 4) | cmd):
  0x21  SET_MOTORS_SPEED  [motor_mask:u8][speed:i8  -100..+100]
  0x22  SET_SERVO_TYPE    [servo_mask:u8][type:u8  0=180°, 1=270°, 2=continuous]
  0x23  SET_SERVOS_SPEED  [servo_mask:u8][speed:i8  -100..+100]  (continuous only)
  0x24  SET_SERVOS_ANGLE  [servo_mask:u8][angle_hi:u8][angle_lo:u8]  (big-endian i16)
  0x28  STOP_ALL_MOTORS   (no payload)

Motor bitmask: bit 0 = motor 1, bit 1 = motor 2, bit 2 = motor 3, bit 3 = motor 4
Servo bitmask: bit 0 = channel 0, …, bit 5 = channel 5

Example
-------
    config = BotConfig()

    # Attach hardware
    config.add_motor("left",  motor_id=1)
    config.add_motor("right", motor_id=2)
    config.add_servo("arm",   channel_id=0, servo_type=ServoType.CONTINUOUS)

    # Define actions
    config.add_action("forward",    [MotorCmd("left", 100), MotorCmd("right", 100)])
    config.add_action("backward",   [MotorCmd("left", -100), MotorCmd("right", -100)])
    config.add_action("turn_left",  [MotorCmd("left", -50), MotorCmd("right", 50)])
    config.add_action("turn_right", [MotorCmd("left", 50), MotorCmd("right", -50)])
    config.add_action("stop",       [MotorCmd("left", 0), MotorCmd("right", 0)])
    config.add_action("arm_extend", [ServoSpeedCmd("arm", 80)])
    config.add_action("arm_retract",[ServoSpeedCmd("arm", -80)])

    # Build raw bytes (no network needed)
    packets = config.build_packets("forward")

    # Or execute directly via UDPClient
    await config.execute("forward", udp_client)
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import IntEnum
from typing import Dict, List, Union


# ---------------------------------------------------------------------------
# Servo type
# ---------------------------------------------------------------------------

class ServoType(IntEnum):
    """Operating mode of a servo channel, matches the device firmware values."""
    SERVO_180  = 0  # Standard 180° positional servo
    SERVO_270  = 1  # Wide-angle 270° positional servo
    CONTINUOUS = 2  # Continuous-rotation servo — controlled via speed, not angle


# ---------------------------------------------------------------------------
# Command value-objects (what you put inside an action)
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class MotorCmd:
    """Apply a speed to a named motor channel.

    Args:
        motor_name: The name used in :py:meth:`BotConfig.add_motor`.
        speed: Target speed in the range ``-100`` (full reverse) to
               ``+100`` (full forward).  ``0`` stops the motor.
    """
    motor_name: str
    speed: int  # -100..+100


@dataclass(frozen=True)
class ServoAngleCmd:
    """Set the absolute angle of a named positional servo.

    Args:
        servo_name: The name used in :py:meth:`BotConfig.add_servo`.
        angle: Target angle in degrees.  Valid range depends on servo type:
               ``0–180`` for SERVO_180, ``0–270`` for SERVO_270.
    """
    servo_name: str
    angle: int  # degrees


@dataclass(frozen=True)
class ServoSpeedCmd:
    """Set the speed of a named continuous-rotation servo.

    Args:
        servo_name: The name used in :py:meth:`BotConfig.add_servo`.
        speed: Speed in the range ``-100`` to ``+100``.  ``0`` stops the servo.
    """
    servo_name: str
    speed: int  # -100..+100


#: Union type accepted by :py:meth:`BotConfig.add_action`.
BotCmd = Union[MotorCmd, ServoAngleCmd, ServoSpeedCmd]


# ---------------------------------------------------------------------------
# Internal channel descriptors
# ---------------------------------------------------------------------------

@dataclass
class _MotorChannel:
    name: str
    motor_id: int  # 1-4

    @property
    def mask(self) -> int:
        """Bitmask for this motor (bit 0 = motor 1, …, bit 3 = motor 4)."""
        return 1 << (self.motor_id - 1)


@dataclass
class _ServoChannel:
    name: str
    channel_id: int  # 0-5
    servo_type: ServoType = ServoType.SERVO_180

    @property
    def mask(self) -> int:
        """Bitmask for this servo channel (bit 0 = channel 0, …)."""
        return 1 << self.channel_id


# ---------------------------------------------------------------------------
# BotConfig
# ---------------------------------------------------------------------------

class BotConfig:
    """Bot hardware configuration and named-action registry.

    Use :py:meth:`add_motor` / :py:meth:`add_servo` to attach hardware
    channels to friendly names, then :py:meth:`add_action` to define what
    each action does.  Actions are translated to binary UDP packets by
    :py:meth:`build_packets` and can be sent with :py:meth:`execute`.

    All mutating methods return ``self`` so calls can be chained fluently.
    """

    # MotorServoService protocol constants
    _CMD_SET_MOTORS_SPEED: int = 0x21
    _CMD_SET_SERVO_TYPE:   int = 0x22
    _CMD_SET_SERVOS_SPEED: int = 0x23
    _CMD_SET_SERVOS_ANGLE: int = 0x24
    _CMD_STOP_ALL_MOTORS:  int = 0x28

    def __init__(self) -> None:
        self._motors:  Dict[str, _MotorChannel] = {}
        self._servos:  Dict[str, _ServoChannel] = {}
        self._actions: Dict[str, List[BotCmd]]  = {}

    # ------------------------------------------------------------------
    # Hardware attachment
    # ------------------------------------------------------------------

    def add_motor(self, name: str, motor_id: int) -> "BotConfig":
        """Register a DC motor channel under a friendly name.

        Args:
            name: Identifier used later in :py:class:`MotorCmd`.
            motor_id: Physical motor number on the DFR1216 board (``1``–``4``).

        Returns:
            ``self`` for fluent chaining.
        """
        if not 1 <= motor_id <= 4:
            raise ValueError(f"motor_id must be 1–4, got {motor_id}")
        self._motors[name] = _MotorChannel(name, motor_id)
        return self

    def add_servo(
        self,
        name: str,
        channel_id: int,
        servo_type: ServoType = ServoType.SERVO_180,
    ) -> "BotConfig":
        """Register a servo channel under a friendly name.

        Args:
            name: Identifier used later in :py:class:`ServoAngleCmd` /
                  :py:class:`ServoSpeedCmd`.
            channel_id: Physical servo channel on the DFR1216 board (``0``–``5``).
            servo_type: :py:class:`ServoType` that controls which protocol
                        command is used (angle vs. speed).

        Returns:
            ``self`` for fluent chaining.
        """
        if not 0 <= channel_id <= 5:
            raise ValueError(f"channel_id must be 0–5, got {channel_id}")
        self._servos[name] = _ServoChannel(name, channel_id, servo_type)
        return self

    # ------------------------------------------------------------------
    # Action definition
    # ------------------------------------------------------------------

    def add_action(self, name: str, commands: List[BotCmd]) -> "BotConfig":
        """Define a named action as a list of motor/servo commands.

        The order of commands within the list does not matter; they are
        grouped and packed into the minimal number of binary packets.

        Args:
            name: Action name (e.g. ``"forward"``, ``"stop"``).
            commands: List of :py:class:`MotorCmd`, :py:class:`ServoAngleCmd`,
                      or :py:class:`ServoSpeedCmd` instances.

        Returns:
            ``self`` for fluent chaining.
        """
        self._actions[name] = list(commands)
        return self

    # ------------------------------------------------------------------
    # Packet building
    # ------------------------------------------------------------------

    def build_packets(self, action_name: str) -> List[bytes]:
        """Translate a named action into binary UDP packet(s).

        Motors/servos that share the **same speed or angle value** are
        combined into a single packet using the bitmask protocol.
        Different values require separate packets.

        Args:
            action_name: Name previously registered with :py:meth:`add_action`.

        Returns:
            Ordered list of ``bytes`` objects ready to send via UDP.

        Raises:
            KeyError: If *action_name* is not registered, or a command
                      references an unregistered motor/servo name.
        """
        if action_name not in self._actions:
            raise KeyError(f"Unknown action '{action_name}'. "
                           f"Registered: {list(self._actions)}")
        commands = self._actions[action_name]
        packets: List[bytes] = []

        # --- Motor speed commands ---
        # Group by speed value → accumulate bitmask per speed
        motor_by_speed: Dict[int, int] = {}  # speed -> mask
        for cmd in commands:
            if isinstance(cmd, MotorCmd):
                motor = self._motors[cmd.motor_name]
                motor_by_speed.setdefault(cmd.speed, 0)
                motor_by_speed[cmd.speed] |= motor.mask

        for speed, mask in motor_by_speed.items():
            # Frame: [0x21][motor_mask:u8][speed:i8]
            packets.append(bytes([
                self._CMD_SET_MOTORS_SPEED,
                mask & 0xFF,
                speed & 0xFF,  # Python int → unsigned byte (two's complement)
            ]))

        # --- Continuous servo speed commands ---
        servo_speed_by_speed: Dict[int, int] = {}  # speed -> mask
        for cmd in commands:
            if isinstance(cmd, ServoSpeedCmd):
                servo = self._servos[cmd.servo_name]
                servo_speed_by_speed.setdefault(cmd.speed, 0)
                servo_speed_by_speed[cmd.speed] |= servo.mask

        for speed, mask in servo_speed_by_speed.items():
            # Frame: [0x23][servo_mask:u8][speed:i8]
            packets.append(bytes([
                self._CMD_SET_SERVOS_SPEED,
                mask & 0xFF,
                speed & 0xFF,
            ]))

        # --- Positional servo angle commands ---
        servo_angle_by_angle: Dict[int, int] = {}  # angle -> mask
        for cmd in commands:
            if isinstance(cmd, ServoAngleCmd):
                servo = self._servos[cmd.servo_name]
                servo_angle_by_angle.setdefault(cmd.angle, 0)
                servo_angle_by_angle[cmd.angle] |= servo.mask

        for angle, mask in servo_angle_by_angle.items():
            # Frame: [0x24][servo_mask:u8][angle_hi:u8][angle_lo:u8]  (big-endian i16)
            angle_u16 = angle & 0xFFFF
            packets.append(bytes([
                self._CMD_SET_SERVOS_ANGLE,
                mask & 0xFF,
                (angle_u16 >> 8) & 0xFF,
                angle_u16 & 0xFF,
            ]))

        return packets

    def build_stop_all_packet(self) -> bytes:
        """Build the STOP_ALL_MOTORS emergency-stop packet (``0x28``).

        Returns:
            Single-byte ``bytes`` object.
        """
        return bytes([self._CMD_STOP_ALL_MOTORS])

    def build_servo_type_packet(self, servo_name: str) -> bytes:
        """Build a SET_SERVO_TYPE packet for the registered servo type.

        Useful to send once after connecting so the bot knows the servo mode.

        Args:
            servo_name: Name previously registered with :py:meth:`add_servo`.

        Returns:
            3-byte ``bytes`` object: ``[0x22][mask][type]``.
        """
        servo = self._servos[servo_name]
        return bytes([
            self._CMD_SET_SERVO_TYPE,
            servo.mask & 0xFF,
            int(servo.servo_type),
        ])

    # ------------------------------------------------------------------
    # Execution helpers
    # ------------------------------------------------------------------

    async def execute(self, action_name: str, udp_client) -> bool:
        """Build and send all packets for a named action.

        Args:
            action_name: Name previously registered with :py:meth:`add_action`.
            udp_client: A :py:class:`~udp_client.network.udp_client.UDPClient`
                        instance (must be connected).

        Returns:
            ``True`` if every packet was sent successfully.
        """
        packets = self.build_packets(action_name)
        results = [await udp_client.send_raw(p) for p in packets]
        return all(results)

    async def stop_all(self, udp_client) -> bool:
        """Send the STOP_ALL_MOTORS emergency command.

        Args:
            udp_client: A connected :py:class:`~udp_client.network.udp_client.UDPClient`.

        Returns:
            ``True`` if the packet was sent successfully.
        """
        return await udp_client.send_raw(self.build_stop_all_packet())

    # ------------------------------------------------------------------
    # Introspection
    # ------------------------------------------------------------------

    @property
    def motors(self) -> Dict[str, int]:
        """Mapping of motor name → motor_id for inspection."""
        return {name: ch.motor_id for name, ch in self._motors.items()}

    @property
    def servos(self) -> Dict[str, dict]:
        """Mapping of servo name → {channel_id, servo_type} for inspection."""
        return {
            name: {"channel_id": ch.channel_id, "servo_type": ch.servo_type.name}
            for name, ch in self._servos.items()
        }

    @property
    def actions(self) -> List[str]:
        """Names of all registered actions."""
        return list(self._actions.keys())

    def __repr__(self) -> str:
        return (
            f"BotConfig("
            f"motors={list(self._motors)}, "
            f"servos={list(self._servos)}, "
            f"actions={list(self._actions)})"
        )
