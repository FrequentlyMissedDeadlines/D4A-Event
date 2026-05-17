"""
udp_client/control/stick_helpers.py
────────────────────────────────────
Built-in stick handler factories for common drive modes.

A *stick handler* is any callable with the signature::

    handler(x: float, y: float, scale: int, bot_config: BotConfig) -> list[BotCmd]

where *x* and *y* are the raw stick axis values (``-1.0 … +1.0``,
deadzone already applied, **y NOT inverted**), *scale* is
``JOYSTICK_SPEED_SCALE`` from ``customize.py``, and *bot_config* is
the :py:class:`~udp_client.bot_config.BotConfig` instance so the handler
can emit the correct command type (motor vs. servo).

The factories below return such callables, pre-configured with the
motor/servo names declared in ``customize.py § 1``.
"""

from __future__ import annotations

from collections.abc import Callable
from typing import TYPE_CHECKING

from udp_client.bot_config import BotCmd, MotorCmd, ServoSpeedCmd

if TYPE_CHECKING:
    from udp_client.bot_config import BotConfig

#: Type alias for a stick handler function.
StickHandler = Callable[[float, float, int, "BotConfig"], list[BotCmd]]


def _speed_cmd(name: str, speed: int, bot_config: BotConfig) -> BotCmd:
    """Return a :py:class:`MotorCmd` or :py:class:`ServoSpeedCmd` depending
    on how *name* was registered in *bot_config*.

    Falls back to :py:class:`ServoSpeedCmd` if the name is not found in
    either registry (the error will surface later in ``build_packets_from_cmds``).
    """
    if name in bot_config._motors:
        return MotorCmd(name, speed)
    return ServoSpeedCmd(name, speed)


def arcade_drive(left_motor: str, right_motor: str) -> StickHandler:
    """Factory: arcade-style single-stick drive.

    - **Y axis** (up/down) controls forward / backward speed for both motors.
    - **X axis** (left/right) controls turning by mixing into the differential.

    ``left_speed = fwd + turn``, ``right_speed = fwd - turn``, where
    ``fwd = -y`` (stick up = forward) and ``turn = x``.

    Args:
        left_motor:  Name of the left motor/servo (from ``BotConfig``).
        right_motor: Name of the right motor/servo (from ``BotConfig``).

    Returns:
        A stick handler callable.

    Example::

        JOYSTICK_STICKS = {
            "left": arcade_drive("left_servo", "right_servo"),
        }
    """

    def _handler(x: float, y: float, scale: int, bot_config: BotConfig) -> list[BotCmd]:
        fwd = -y  # stick up = positive
        turn = x
        left_speed = max(-100, min(100, int((fwd + turn) * scale)))
        right_speed = max(-100, min(100, int((fwd - turn) * scale)))
        return [
            _speed_cmd(left_motor, left_speed, bot_config),
            _speed_cmd(right_motor, right_speed, bot_config),
        ]

    return _handler


def tank_drive(left_motor: str, right_motor: str) -> StickHandler:
    """Factory: two-axis tank drive using a single stick (alias for arcade).

    Functionally identical to :py:func:`arcade_drive`.  For true
    independent-axis tank drive, use per-axis bindings instead::

        JOYSTICK_STICKS = {
            "left_y":  "left_servo",
            "right_y": "right_servo",
        }

    Args:
        left_motor:  Name of the left motor/servo.
        right_motor: Name of the right motor/servo.

    Returns:
        A stick handler callable.
    """
    return arcade_drive(left_motor, right_motor)
