"""
config.py  —  Infrastructure settings (network, UI, timing).
=============================================================

This file controls **how** the client connects and behaves.
It is part of the framework: you rarely need to touch it.

To define **what** the bot does (hardware layout, actions, control bindings),
edit ``customize.py`` instead — that is the file intended for users.

Settings overview
─────────────────
  DEFAULT_SERVER_IP / PORT  – IP address and UDP port of the K10 Bot
  HEARTBEAT_INTERVAL        – How often the 0x43 keep-alive is sent (seconds)
  KEYBOARD_ENABLED          – Enable pynput global keyboard listener
  JOYSTICK_ENABLED          – Enable pygame joystick support
  JOYSTICK_DEADZONE         – Fallback deadzone when no calibration file exists
  CALIBRATION_FILE          – Path to the joystick calibration JSON (auto-loaded)
  REFRESH_RATE              – TUI panel refresh rate in Hz
  PING_INTERVAL             – How often RTT pings are sent (seconds)
"""

from pathlib import Path

# Server defaults
DEFAULT_SERVER_IP = "192.168.1.179"
DEFAULT_SERVER_PORT = 24642
BOT_TOKEN = "6DEE8"  # 5-char ASCII token for 0x41 MASTER_REGISTER

# Input configuration
KEYBOARD_ENABLED = True
JOYSTICK_ENABLED = True

# Joystick configuration
JOYSTICK_DEADZONE = 0.15  # 15% deadzone for analog sticks (used when no calibration file)
MAX_JOYSTICKS = 4

# Joystick calibration file (auto-loaded by JoystickHandler on startup)
# Run `python -m udp_client.input.calibration_wizard` to generate it.
CALIBRATION_FILE = Path(__file__).parent / "joystick_calibration.json"

# UI configuration
REFRESH_RATE = 60  # Hz
UPDATE_INTERVAL = 1 / REFRESH_RATE

# Network configuration
NETWORK_TIMEOUT = 5.0  # seconds
RECONNECT_DELAY = 2.0  # seconds
MAX_RECONNECT_ATTEMPTS = 0  # 0 = infinite
HEARTBEAT_INTERVAL = 0.040  # seconds (40 ms)
PING_INTERVAL = 0.250  # seconds between keep-alive pings
PING_TIMEOUT = 0.200  # seconds to wait for ping echo
PING_HISTORY_SIZE = 20  # RTT samples kept for sparkline display (20 points)

# Message protocol
MESSAGE_SEPARATOR = b"\n"
MAX_MESSAGE_SIZE = 512
