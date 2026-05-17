"""
udp_client/control/controller.py
─────────────────────────────────
Control loop: translates live input state into bot commands.

This module reads your mappings from ``customize.py`` and dispatches binary
UDP packets every tick (called every 40 ms by the heartbeat timer in ``main.py``).

You do NOT need to edit this file.  All customization lives in ``customize.py``.

Architecture
────────────
::

    K10BotApp  ──► Controller.tick(key_state, joystick_states)
                         │
                    ┌────┴────────────────────────────────┐
                    │  Phase 1: COLLECT                   │
                    │  keyboard  → [BotCmd, …]            │
                    │  sticks    → [BotCmd, …]            │
                    │  buttons   → [BotCmd, …]            │
                    │  dpad      → [BotCmd, …]            │
                    ├─────────────────────────────────────┤
                    │  Phase 2: MERGE  (per channel)      │
                    │  motor/servo speed → sum, clamp     │
                    │  servo angle       → average, clamp │
                    ├─────────────────────────────────────┤
                    │  Phase 3: SEND                      │
                    │  merged BotCmds → UDP packets       │
                    └─────────────────────────────────────┘
"""

from __future__ import annotations

import importlib
import logging
import time
from typing import TYPE_CHECKING

logger = logging.getLogger(__name__)

from udp_client.bot_config import (
    BotCmd,
    BotConfig,
    MotorCmd,
    ServoAngleCmd,
    ServoSpeedCmd,
    ServoType,
)

if TYPE_CHECKING:
    from udp_client.control.stick_helpers import StickHandler
    from udp_client.input.joystick_handler import JoystickState
    from udp_client.network.udp_client import UDPClient


class Controller:
    """Bridges live input state → bot commands using the mappings in ``customize.py``.

    The :py:class:`Controller` is instantiated once in ``main.py`` and its
    :py:meth:`tick` method is called every 40 ms by the TUI heartbeat timer.

    Example::

        controller = Controller(udp_client)

        # Called every 40 ms by the TUI:
        await controller.tick(app.get_key_state(), joystick_states)
    """

    # MotorServoService protocol constant (service 0x02, cmd 0x01)
    _CMD_SET_MOTORS_SPEED: int = 0x21

    # -----------------------------------------------------------------------
    # Construction
    # -----------------------------------------------------------------------

    def __init__(self, udp_client: UDPClient) -> None:
        """
        Args:
            udp_client: The shared :py:class:`~udp_client.network.udp_client.UDPClient`
                        instance managed by the TUI app.
        """
        self._client = udp_client
        self._ready = False
        self._bot_config: BotConfig | None = None
        self._keyboard_actions: dict[str, dict[str, str | None]] = {}
        self._keyboard_idle: str | None = None
        self._prev_key_state: dict[str, bool] = {}
        self._js_speed_scale: int = 80
        self._js_axis_bindings: dict[str, str] = {}  # e.g. {"left_y": "left"}
        self._js_stick_handlers: dict[str, StickHandler] = {}  # e.g. {"left": callable}
        self._js_button_actions: dict[int, str] = {}
        self._js_dpad_actions: dict[str, dict[str, str | None]] = {}
        self._prev_dpad: dict[str, bool] = {}

        self._load_customize()

        # Live state — read by the TUI BotStatusPanel
        self.last_action: str | None = None
        self.last_action_time: float | None = None
        self.motor_speeds: dict[str, int] = {}  # motor_name → speed
        self.servo_speeds: dict[str, int] = {}  # servo_name → speed

    # -----------------------------------------------------------------------
    # Public read-only accessors (for TUI widgets)
    # -----------------------------------------------------------------------

    @property
    def ready(self) -> bool:
        """Whether customize.py was loaded successfully."""
        return self._ready

    @property
    def bot_config(self) -> BotConfig | None:
        """The parsed :py:class:`BotConfig`, or ``None`` if not loaded."""
        return self._bot_config

    @property
    def keyboard_actions(self) -> dict[str, dict[str, str | None]]:
        """Parsed keyboard bindings from customize.py."""
        return self._keyboard_actions

    @property
    def keyboard_idle(self) -> str | None:
        """Action sent when no key is pressed."""
        return self._keyboard_idle

    @property
    def js_speed_scale(self) -> int:
        """Joystick speed scale factor."""
        return self._js_speed_scale

    @property
    def js_axis_bindings(self) -> dict[str, str]:
        """Per-axis joystick bindings."""
        return self._js_axis_bindings

    @property
    def js_stick_handlers(self) -> dict:
        """Whole-stick handler bindings."""
        return self._js_stick_handlers

    @property
    def js_button_actions(self) -> dict[int, str]:
        """Button → action bindings."""
        return self._js_button_actions

    # -----------------------------------------------------------------------
    # Configuration loading
    # -----------------------------------------------------------------------

    def _load_customize(self) -> None:
        """Import and cache all settings from ``customize.py``.

        Called once at construction.  If ``customize.py`` is missing or raises
        an exception, the controller falls back to no-op mode — no packets are
        ever sent, and a warning is printed to stdout.
        """
        try:
            cx = importlib.import_module("customize")

            self._bot_config = cx.BOT_CONFIG

            # § 3 — Keyboard
            raw_kb = dict(getattr(cx, "KEYBOARD_ACTIONS", {}))
            self._keyboard_actions = {}
            for key, binding in raw_kb.items():
                parsed = self._parse_key_binding(binding, f"KEYBOARD_ACTIONS['{key}']")
                if parsed:
                    self._keyboard_actions[key] = parsed
            self._keyboard_idle = getattr(cx, "KEYBOARD_IDLE_ACTION", None)

            # § 4 — Joystick
            self._js_speed_scale = int(getattr(cx, "JOYSTICK_SPEED_SCALE", 80))

            raw_sticks = dict(getattr(cx, "JOYSTICK_STICKS", {}))
            self._js_axis_bindings = {}
            self._js_stick_handlers = {}
            for key, value in raw_sticks.items():
                if key in ("left", "right"):  # whole-stick handler
                    self._js_stick_handlers[key] = value
                elif key.endswith(("_x", "_y")):  # per-axis → motor/servo name
                    self._js_axis_bindings[key] = value

            self._js_button_actions = dict(getattr(cx, "JOYSTICK_BUTTON_ACTIONS", {}))

            # § 4 — Joystick D-Pad
            raw_dpad = dict(getattr(cx, "JOYSTICK_DPAD_ACTIONS", {}))
            self._js_dpad_actions = {}
            for key, binding in raw_dpad.items():
                parsed = self._parse_key_binding(binding, f"JOYSTICK_DPAD_ACTIONS['{key}']")
                if parsed:
                    self._js_dpad_actions[key] = parsed

            # Validate all referenced action names exist
            self._validate_action_refs()

            self._ready = True

        except ModuleNotFoundError:
            logger.warning(
                "customize.py not found. "
                "Copy the template and edit it to define your bot: "
                "cp customize.py.example customize.py  — "
                "No control commands will be sent until customize.py exists."
            )
        except Exception as exc:
            logger.error(
                "Error loading customize.py: %s — Fix the error, then restart the application.", exc
            )

    # -----------------------------------------------------------------------
    # Main tick — called every 40 ms
    # -----------------------------------------------------------------------

    async def tick(
        self,
        key_state: dict[str, bool],
        joystick_states: list[JoystickState],
    ) -> None:
        """Dispatch input events as bot commands.

        Called every 40 ms (same cadence as the heartbeat) by the TUI app
        while the UDP connection is active.

        All input sources (keyboard, joystick sticks, buttons, D-Pad) are
        evaluated independently.  Their resulting commands are **merged**
        before a single set of UDP packets is sent:

        - Motor / servo **speeds** are summed per channel, then clamped
          to ``-100 … +100``.
        - Servo **angles** are averaged per channel, then clamped to the
          valid range for the servo type.

        Args:
            key_state:       Dict mapping key-name → ``True`` if currently pressed.
            joystick_states: List of :py:class:`~udp_client.input.joystick_handler.JoystickState`
                             objects, one per connected gamepad.
        """
        if not self._ready or not self._client.connected or self._bot_config is None:
            return

        # --- Collect commands from every input source ---
        all_cmds: list[BotCmd] = []
        js_cmds = self._collect_joystick(joystick_states)
        # Joystick is "actively driving" only if it produces non-zero speeds
        js_active = any(
            (isinstance(c, (MotorCmd, ServoSpeedCmd)) and c.speed != 0)
            or isinstance(c, ServoAngleCmd)
            for c in js_cmds
        )
        kb_cmds = self._collect_keyboard(key_state, has_joystick_input=js_active)
        all_cmds.extend(kb_cmds)
        all_cmds.extend(js_cmds)

        # --- Merge & send ---
        merged = self._merge_commands(all_cmds)
        if not merged:
            # No input source produced any command → emergency stop
            await self._client.send_raw(self._bot_config.build_stop_all_packet())
            self.last_action = "stop_all"
            self.last_action_time = time.monotonic()
            return

        action_label_parts: list[str] = []
        packets = self._bot_config.build_packets_from_cmds(merged)
        for pkt in packets:
            await self._client.send_raw(pkt)

        # Update live-state dicts for TUI panels
        for cmd in merged:
            if isinstance(cmd, MotorCmd):
                self.motor_speeds[cmd.motor_name] = cmd.speed
                action_label_parts.append(f"{cmd.motor_name}={cmd.speed}")
            elif isinstance(cmd, ServoSpeedCmd):
                self.servo_speeds[cmd.servo_name] = cmd.speed
                action_label_parts.append(f"{cmd.servo_name}={cmd.speed}")
            elif isinstance(cmd, ServoAngleCmd):
                self.servo_speeds[cmd.servo_name] = cmd.angle
                action_label_parts.append(f"{cmd.servo_name}@{cmd.angle}°")

        self.last_action = ", ".join(action_label_parts) if action_label_parts else None
        self.last_action_time = time.monotonic()

    # -----------------------------------------------------------------------
    # Keyboard — collect commands
    # -----------------------------------------------------------------------

    def _collect_keyboard(self, key_state: dict[str, bool], *, has_joystick_input: bool = False) -> list[BotCmd]:
        """Return the list of :py:class:`BotCmd` contributed by keyboard input.

        Detects **edges** — keys that just went down or just went up — by
        comparing *key_state* with the previous tick's snapshot.  Held keys
        re-fire their ``on_keydown`` action every tick (continuous drive).

        Args:
            key_state: Dict mapping key-name → ``True`` if currently pressed.
            has_joystick_input: When ``True``, the keyboard idle action is
                suppressed so that it does not zero-out channels the joystick
                is actively driving.
        """
        if self._bot_config is None:
            return []

        cmds: list[BotCmd] = []
        fired_any = False

        for key, binding in self._keyboard_actions.items():
            now_pressed = key_state.get(key, False)
            was_pressed = self._prev_key_state.get(key, False)

            if now_pressed:
                cmds.extend(self._resolve_action(binding["on_keydown"]))  # type: ignore[arg-type]
                fired_any = True
            elif was_pressed and not now_pressed and binding["on_keyup"]:
                cmds.extend(self._resolve_action(binding["on_keyup"]))
                fired_any = True

        if not fired_any and self._keyboard_idle and not has_joystick_input:
            cmds.extend(self._resolve_action(self._keyboard_idle))

        self._prev_key_state = dict(key_state)
        return cmds

    # -----------------------------------------------------------------------
    # Joystick — collect commands
    # -----------------------------------------------------------------------

    def _collect_joystick(self, joystick_states: list[JoystickState]) -> list[BotCmd]:
        """Return the list of :py:class:`BotCmd` contributed by joystick input.

        Handles analog sticks, buttons, and D-Pad.  Only the first
        connected joystick is used.
        """
        if not joystick_states or self._bot_config is None:
            return []

        state = next((s for s in joystick_states if s.connected), None)
        if state is None:
            return []

        cmds: list[BotCmd] = []

        # --- Analog sticks ---
        # Axis raw values (deadzone already applied by JoystickHandler)
        axis_values = {
            "left_x": state.left_stick_x,
            "left_y": state.left_stick_y,
            "right_x": state.right_stick_x,
            "right_y": state.right_stick_y,
        }

        # Whole-stick handlers take priority over per-axis bindings
        handled_prefixes: set[str] = set()
        for stick, handler in self._js_stick_handlers.items():
            x = axis_values[f"{stick}_x"]
            y = axis_values[f"{stick}_y"]
            try:
                result = handler(x, y, self._js_speed_scale, self._bot_config)
                if result:
                    cmds.extend(result)
            except Exception as exc:
                logger.warning("Stick handler '%s' raised %s: %s", stick, type(exc).__name__, exc)
            handled_prefixes.add(stick)

        # Per-axis bindings (skipped for sticks that have a handler)
        for axis_key, motor_name in self._js_axis_bindings.items():
            prefix = axis_key.rsplit("_", 1)[0]  # "left_y" → "left"
            if prefix in handled_prefixes:
                continue
            raw = axis_values.get(axis_key, 0.0)
            # Invert Y so "stick up" = positive speed
            if axis_key.endswith("_y"):
                raw = -raw
            speed = int(raw * self._js_speed_scale)
            if self._bot_config.has_motor(motor_name):
                cmds.append(MotorCmd(motor_name, speed))
            elif self._bot_config.has_servo(motor_name):
                cmds.append(ServoSpeedCmd(motor_name, speed))

        # --- Buttons → named actions ---
        for btn_id, action in self._js_button_actions.items():
            if state.buttons.get(btn_id):
                cmds.extend(self._resolve_action(action))

        # --- D-Pad → on_keydown / on_keyup ---
        dx, dy = state.dpad
        dpad_state: dict[str, bool] = {
            "up": dy > 0,
            "down": dy < 0,
            "left": dx < 0,
            "right": dx > 0,
        }
        for direction, binding in self._js_dpad_actions.items():
            now = dpad_state.get(direction, False)
            was = self._prev_dpad.get(direction, False)
            if now:
                cmds.extend(self._resolve_action(binding["on_keydown"]))  # type: ignore[arg-type]
            elif was and not now and binding["on_keyup"]:
                cmds.extend(self._resolve_action(binding["on_keyup"]))
        self._prev_dpad = dpad_state

        return cmds

    # -----------------------------------------------------------------------
    # Merging
    # -----------------------------------------------------------------------

    def _merge_commands(self, cmds: list[BotCmd]) -> list[BotCmd]:
        """Merge commands from all input sources into one command per channel.

        - :py:class:`MotorCmd` / :py:class:`ServoSpeedCmd`: **sum** speeds
          per channel, clamp to ``-100 … +100``.  **However**, if any
          input source contributes a speed of ``0`` for a channel, that
          channel is forced to ``0`` regardless of other contributions
          (stop has priority).
        - :py:class:`ServoAngleCmd`: **average** angles per channel, clamp
          to the valid range for the servo type (``0–180`` for SERVO_180,
          ``0–270`` for SERVO_270).

        Returns:
            A deduplicated list of merged commands (one per channel).
        """
        motor_speeds: dict[str, int] = {}
        motor_zeroed: set[str] = set()
        servo_speeds: dict[str, int] = {}
        servo_zeroed: set[str] = set()
        servo_angles: dict[str, list[int]] = {}

        for cmd in cmds:
            if isinstance(cmd, MotorCmd):
                if cmd.speed == 0:
                    motor_zeroed.add(cmd.motor_name)
                motor_speeds[cmd.motor_name] = motor_speeds.get(cmd.motor_name, 0) + cmd.speed
            elif isinstance(cmd, ServoSpeedCmd):
                if cmd.speed == 0:
                    servo_zeroed.add(cmd.servo_name)
                servo_speeds[cmd.servo_name] = servo_speeds.get(cmd.servo_name, 0) + cmd.speed
            elif isinstance(cmd, ServoAngleCmd):
                servo_angles.setdefault(cmd.servo_name, []).append(cmd.angle)

        merged: list[BotCmd] = []
        for name, total in motor_speeds.items():
            speed = 0 if name in motor_zeroed else max(-100, min(100, total))
            merged.append(MotorCmd(name, speed))
        for name, total in servo_speeds.items():
            speed = 0 if name in servo_zeroed else max(-100, min(100, total))
            merged.append(ServoSpeedCmd(name, speed))
        for name, angles in servo_angles.items():
            avg = round(sum(angles) / len(angles))
            # Clamp to servo's actual range
            max_angle = 270  # default fallback
            if self._bot_config and self._bot_config.has_servo(name):
                servo_type = self._bot_config.get_servo_type(name)
                max_angle = 180 if servo_type == ServoType.SERVO_180 else 270
            merged.append(ServoAngleCmd(name, max(0, min(max_angle, avg))))
        return merged

    # -----------------------------------------------------------------------
    # Helpers
    # -----------------------------------------------------------------------

    @staticmethod
    def _parse_key_binding(
        binding: str | dict[str, str],
        context: str,
    ) -> dict[str, str | None] | None:
        """Normalize a keyboard / D-Pad binding into ``{on_keydown, on_keyup}``.

        Args:
            binding:  Raw value from ``customize.py`` — either a plain string
                      or a dict with ``on_keydown`` (mandatory) and optional
                      ``on_keyup``.
            context:  Human-readable location string for error messages
                      (e.g. ``"KEYBOARD_ACTIONS['up']"``.

        Returns:
            Normalized dict, or ``None`` if the binding is malformed (an
            error message is printed to stdout).
        """
        if isinstance(binding, str):
            return {"on_keydown": binding, "on_keyup": None}
        if not isinstance(binding, dict):
            logger.warning(
                "%s — expected str or dict, got %s. Skipping.", context, type(binding).__name__
            )
            return None
        if "on_keydown" not in binding:
            logger.warning("%s — missing required key 'on_keydown'. Skipping.", context)
            return None
        return {
            "on_keydown": binding["on_keydown"],
            "on_keyup": binding.get("on_keyup"),
        }

    def _validate_action_refs(self) -> None:
        """Warn about action names referenced in bindings that don't exist in BotConfig."""
        if self._bot_config is None:
            return
        known = set(self._bot_config.action_names)

        def _check(name: str | None, context: str) -> None:
            if name and name not in known:
                logger.warning(
                    "%s references unknown action '%s'. Known: %s", context, name, sorted(known)
                )

        # Keyboard
        for key, binding in self._keyboard_actions.items():
            _check(binding["on_keydown"], f"KEYBOARD_ACTIONS['{key}'].on_keydown")
            _check(binding["on_keyup"], f"KEYBOARD_ACTIONS['{key}'].on_keyup")
        _check(self._keyboard_idle, "KEYBOARD_IDLE_ACTION")

        # Buttons
        for btn_id, action in self._js_button_actions.items():
            _check(action, f"JOYSTICK_BUTTON_ACTIONS[{btn_id}]")

        # D-Pad
        for key, binding in self._js_dpad_actions.items():
            _check(binding["on_keydown"], f"JOYSTICK_DPAD_ACTIONS['{key}'].on_keydown")
            _check(binding["on_keyup"], f"JOYSTICK_DPAD_ACTIONS['{key}'].on_keyup")

    def _resolve_action(self, action_name: str) -> list[BotCmd]:
        """Look up a named action and return its list of :py:class:`BotCmd`.

        Returns an empty list for unknown action names.
        """
        if self._bot_config is None:
            return []
        try:
            return self._bot_config.get_action_commands(action_name)
        except KeyError:
            return []
