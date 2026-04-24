"""
Configuration constants for UDP client.
"""

from pathlib import Path

# Server defaults
DEFAULT_SERVER_IP = "192.168.4.1"
DEFAULT_SERVER_PORT = 24642

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
NETWORK_TIMEOUT = 5.0       # seconds
RECONNECT_DELAY = 2.0       # seconds
MAX_RECONNECT_ATTEMPTS = 0  # 0 = infinite
HEARTBEAT_INTERVAL = 0.040  # seconds (40 ms)
PING_INTERVAL = 0.250       # seconds between keep-alive pings
PING_TIMEOUT = 0.200        # seconds to wait for ping echo
PING_HISTORY_SIZE = 4       # RTT samples averaged for display

# Message protocol
MESSAGE_SEPARATOR = b"\n"
MAX_MESSAGE_SIZE = 512
