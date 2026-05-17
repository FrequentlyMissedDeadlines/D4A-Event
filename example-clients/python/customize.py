"""
customize.py  —  Adapt this file to YOUR bot.
==============================================

This is the ONLY file you need to edit to make the controller work with
your specific hardware and preferred control scheme.

Do NOT modify anything inside ``udp_client/`` — that is the framework.

───────────────────────────────────────────────────────────────────────
QUICK OVERVIEW
───────────────────────────────────────────────────────────────────────

  § 1  Hardware    Declare your motors and servos (name ↔ physical port)
  § 2  Actions     Define named movements as lists of motor/servo commands
  § 3  Keyboard    Map keyboard keys → on_keydown / on_keyup actions
  § 4  Joystick    Map analog sticks, buttons, and D-Pad → actions

───────────────────────────────────────────────────────────────────────
HOW THE WIRING WORKS
───────────────────────────────────────────────────────────────────────

  main.py  ──►  K10BotApp (TUI)  ──►  Controller  ──►  customize.py
                                           │
                                       BotConfig
                                           │
                                      UDP packets  ──►  Bot

The Controller (``udp_client/control/controller.py``) runs every 40 ms,
reads the current input state, looks up YOUR mappings below, and sends
the right binary packets to the bot.  You never need to touch that code.

───────────────────────────────────────────────────────────────────────
"""

from udp_client.bot_config import BotConfig, MotorCmd, ServoAngleCmd, ServoSpeedCmd, ServoType
from udp_client.control.stick_helpers import tank_drive, arcade_drive


# ═══════════════════════════════════════════════════════════════════════
# § 1  HARDWARE — declare your motors and servos
# ═══════════════════════════════════════════════════════════════════════
#
# add_motor(name, motor_id)
#   name      – any string you choose; reused in § 2 actions and § 4 joystick
#   motor_id  – physical motor port on the DFR1216 board  (1, 2, 3, or 4)
#
# add_servo(name, channel_id, servo_type)
#   name        – any string you choose
#   channel_id  – physical servo channel on the DFR1216 board  (0 – 5)
#   servo_type  – ServoType.SERVO_180   standard 180° positional servo
#                 ServoType.SERVO_270   wide-angle 270° positional servo
#                 ServoType.CONTINUOUS  continuous-rotation, speed-controlled

BOT_CONFIG = BotConfig()

# ── Motors ────────────────────────────────────────────────────────────
BOT_CONFIG.add_servo("left_servo", channel_id=0, servo_type=ServoType.CONTINUOUS)   # left  drive motor → port 1
BOT_CONFIG.add_servo("right_servo", channel_id=1, servo_type=ServoType.CONTINUOUS)   # right drive motor → port 2
BOT_CONFIG.add_servo("arm_servo", channel_id=2, servo_type=ServoType.SERVO_270)   # left  drive motor → port 1
BOT_CONFIG.add_servo("clam_servo", channel_id=3, servo_type=ServoType.SERVO_270)   # right drive motor → port 2

# ── Servos ────────────────────────────────────────────────────────────
# Uncomment and adapt to add servo channels:
#
# BOT_CONFIG.add_servo("arm_servo",     channel_id=0, servo_type=ServoType.CONTINUOUS)
# BOT_CONFIG.add_servo("gripper", channel_id=1, servo_type=ServoType.SERVO_180)


# ═══════════════════════════════════════════════════════════════════════
# § 2  ACTIONS — define named movements
# ═══════════════════════════════════════════════════════════════════════
#
# add_action(name, [list of commands])
#
# Commands:
#   MotorCmd(motor_name, speed)        speed in  -100 … +100
#   ServoSpeedCmd(servo_name, speed)   speed in  -100 … +100  (CONTINUOUS only)
#   ServoAngleCmd(servo_name, angle)   angle in  0–180 or 0–270 (positional only)
#
# All commands in ONE action are sent together in the same 40 ms tick.
# Every action name referenced in § 3 / § 4 MUST be defined here.

BOT_CONFIG.add_action("forward",    [ServoSpeedCmd("left_servo",  80), ServoSpeedCmd("right_servo",  80)])
BOT_CONFIG.add_action("backward",   [ServoSpeedCmd("left_servo", -80), ServoSpeedCmd("right_servo", -80)])
BOT_CONFIG.add_action("turn_left",  [ServoSpeedCmd("left_servo", -60), ServoSpeedCmd("right_servo",  60)])
BOT_CONFIG.add_action("turn_right", [ServoSpeedCmd("left_servo",  60), ServoSpeedCmd("right_servo", -60)])
BOT_CONFIG.add_action("stop",       [ServoSpeedCmd("left_servo",   0), ServoSpeedCmd("right_servo",   0)])
# Servo actions (uncomment if you added servos in § 1):
#
BOT_CONFIG.add_action("arm_extend",  [ServoSpeedCmd("arm_servo",  70)])
BOT_CONFIG.add_action("arm_retract", [ServoSpeedCmd("arm_servo", -70)])
BOT_CONFIG.add_action("arm_neutral",    [ServoSpeedCmd("arm_servo",   0)])
BOT_CONFIG.add_action("clam_open",   [ServoAngleCmd("clam_servo",  10)])
BOT_CONFIG.add_action("clam_close",  [ServoAngleCmd("clam_servo", 170)])


# ═══════════════════════════════════════════════════════════════════════
# § 3  KEYBOARD BINDINGS
# ═══════════════════════════════════════════════════════════════════════
#
# Map key names → action bindings defined in § 2.
#
# Each key maps to a dict with:
#   "on_keydown"  (mandatory) — action fired once when the key is first pressed
#   "on_keyup"    (optional)  — action fired once when the key is released
#                                defaults to doing nothing
#
# Shorthand: a plain string is equivalent to {"on_keydown": string}
#
# Available key names:
#   Arrow keys : "up"  "down"  "left"  "right"
#   Special    : "space"  "enter"  "shift"
#   Letters    : "w"  "a"  "s"  "d"  (or any other single letter / digit)
#
# When MULTIPLE keys are pressed simultaneously, ALL their on_keydown
# actions execute (e.g. "up" + "right" sends both "forward" and
# "turn_right" — the last packet targeting the same motor channel wins).
#
# Key order in the dict does NOT affect priority.

KEYBOARD_ACTIONS: dict[str, str | dict[str, str]] = {
    "up":    {"on_keydown": "forward",    "on_keyup": "stop"},
    "down":  {"on_keydown": "backward",   "on_keyup": "stop"},
    "left":  {"on_keydown": "turn_left",  "on_keyup": "stop"},
    "right": {"on_keydown": "turn_right", "on_keyup": "stop"},
    # "space": {"on_keydown": "arm_extend", "on_keyup": "arm_neutral"},
    # "shift": "arm_retract",   # shorthand — no on_keyup action
}

# Action sent when NO mapped key is pressed (and no on_keyup was triggered).
#   "stop"  → auto-stop when you release all keys  (recommended)
#   None    → do nothing; the bot keeps its last speed
KEYBOARD_IDLE_ACTION: str | None = "stop"


# ═══════════════════════════════════════════════════════════════════════
# § 4  JOYSTICK BINDINGS
# ═══════════════════════════════════════════════════════════════════════

# Maximum speed sent when a stick axis is at full extent (0–100).
JOYSTICK_SPEED_SCALE: int = 80

# ── Stick bindings ────────────────────────────────────────────────────
#
# Map stick axes or whole sticks to motors/servos/handlers.
#
# KEYS:
#   Per-axis : "left_x", "left_y", "right_x", "right_y"
#              → maps ONE axis to a motor/servo name (string).
#              Speed = axis_value × JOYSTICK_SPEED_SCALE.
#              Y-axis is auto-inverted so that "stick up" = positive speed.
#
#   Whole stick : "left", "right"
#              → maps to a handler function:
#                  handler(x, y, scale, bot_config) -> list[BotCmd]
#              x and y are in -1.0 … +1.0 (deadzone already applied,
#              y NOT inverted — your handler decides the convention).
#
# If both "left" (handler) AND "left_y" (per-axis) are present,
# the handler takes priority and the per-axis entry is ignored.
#
# BUILT-IN HELPERS  (imported from udp_client.control.stick_helpers):
#   tank_drive(left_motor, right_motor)
#     Left stick Y → left motor, right stick Y → right motor.
#   arcade_drive(left_motor, right_motor)
#     Left stick Y → forward/back, left stick X → turning.

JOYSTICK_STICKS: dict = {
    # ── Tank drive (per-axis, simplest) ──
    "left_y":  "left_servo",
    "right_y": "right_servo",

    # ── Tank drive via helper (equivalent to the two lines above) ──
    # "left":  tank_drive("left_servo", "right_servo"),   # returns a handler

    # ── Arcade drive ──
    # "left":  arcade_drive("left_servo", "right_servo"),

    # ── Map an axis to a servo (e.g. turret pan) ──
    # "right_x": "turret",
}

# Button → action name.
# Use the TUI "🎮 Input" tab to identify which button ID corresponds to each
# physical button on your gamepad (0 = A/Cross on most controllers).
JOYSTICK_BUTTON_ACTIONS: dict[int, str] = {
    # 0: "arm_extend",    # A / Cross
    # 1: "arm_retract",   # B / Circle
    # 2: "arm_stop",      # X / Square
    # 3: "stop",          # Y / Triangle
}

# D-Pad → action bindings.
# Map D-Pad directions to actions, using the same on_keydown / on_keyup
# format as keyboard bindings (§ 3).  A plain string is shorthand for
# {"on_keydown": string} with no on_keyup.
#
# Available directions: "up", "down", "left", "right"
#
# Diagonal presses (e.g. up+right) fire BOTH direction actions.
JOYSTICK_DPAD_ACTIONS: dict[str, str | dict[str, str]] = {
    "up":    {"on_keydown": "forward",    "on_keyup": "stop"},
    "down":  {"on_keydown": "backward",   "on_keyup": "stop"},
    "left":  {"on_keydown": "turn_left",  "on_keyup": "stop"},
    "right": {"on_keydown": "turn_right", "on_keyup": "stop"},
}
