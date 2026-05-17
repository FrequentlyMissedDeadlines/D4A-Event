"""Help display panel — keyboard shortcuts and troubleshooting."""

from textual.widgets import Static

from config import DEFAULT_SERVER_IP, DEFAULT_SERVER_PORT


class HelpDisplayPanel(Static):
    """Display help information."""

    def render(self) -> str:
        return f"""K10 BOT UDP CLIENT - HELP
════════════════════════════════════════════════════

KEYBOARD SHORTCUTS:
  [q]       Quit the application
  [c]       Connect to server
  [d]       Disconnect from server
  [?]       Toggle this help panel

KEYBOARD CONTROL:
  Arrow Keys        Movement (↑ ↓ ← →)
  Space             Action/Confirm
  Shift             Modifier key
  Enter             Send command

JOYSTICK/GAMEPAD MAPPING:
  Left Stick        Primary control
  Right Stick       Secondary control
  LT / RT           Analog triggers (0.0 - 1.0)
  A / B / X / Y     Action buttons
  LB / RB           Shoulder buttons
  D-Pad             Alternative movement

JOYSTICK CALIBRATION:
  [j]               Run interactive calibration wizard
                    (suspends TUI, restores on completion)

CONFIGURATION:
  Default Server:   {DEFAULT_SERVER_IP}:{DEFAULT_SERVER_PORT}
  Edit the server IP and port in the UI
  Click "Connect" to establish connection

TROUBLESHOOTING:
  • Ensure K10 Bot is powered on and connected to network
  • Check firewall settings if connection fails
  • Verify IP address matches your bot's network IP
  • Check that UDP port {DEFAULT_SERVER_PORT} is not blocked

SUPPORTED PLATFORMS:
  ✓ Linux
  ✓ macOS
  ✓ Windows (Windows Terminal)

For more info, see README.md in the project root.
════════════════════════════════════════════════════"""
