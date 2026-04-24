#!/usr/bin/env python3
"""
K10 Bot UDP Client - Main Entry Point

A cross-platform Textual TUI application for controlling K10 Bot via UDP.
Supports keyboard and multiple joystick inputs.
"""

import asyncio
import sys
import time
from pathlib import Path

# Add project root to path
PROJECT_ROOT = Path(__file__).parent
sys.path.insert(0, str(PROJECT_ROOT))

from textual.app import ComposeResult, App
from textual.containers import Container, Horizontal, Vertical, VerticalScroll
from textual.widgets import (
    Button,
    Footer,
    Header,
    Input,
    Label,
    Static,
    TabbedContent,
    TabPane,
)
from textual.widget import Widget

from config import (
    DEFAULT_SERVER_IP,
    DEFAULT_SERVER_PORT,
    HEARTBEAT_INTERVAL,
    KEYBOARD_ENABLED,
    JOYSTICK_ENABLED,
    PING_INTERVAL,
    UPDATE_INTERVAL,
)
from udp_client.input.calibration_wizard import run_wizard
from udp_client.input.joystick_calibration import CalibrationStore, DEFAULT_CALIBRATION_FILE
from udp_client.input.input_manager import InputManager
from udp_client.network.udp_client import UDPClient
from udp_client.state.app_state import AppState


class StatusBar(Static):
    """Top status bar showing connection status."""

    def render(self) -> str:
        return "K10 Bot UDP Client [Press ? for help]"


class ServerConnectionPanel(Widget):
    """Server connection settings."""

    def compose(self) -> ComposeResult:
        with Horizontal():
            yield Label("📡 Server:")
            yield Input(
                id="server_address",
                placeholder="host:port",
                classes="input-field",
            )
            yield Button("Connect", id="btn_connect", variant="primary")
            yield Button("Disconnect", id="btn_disconnect")

    DEFAULT_CSS = """
    ServerConnectionPanel {
        height: 3;
        padding: 0 1;
        border-bottom: solid $accent;
    }

    ServerConnectionPanel Horizontal {
        height: 3;
        align: left middle;
    }

    #server_address {
        width: 30;
    }
    """


class DeviceListPanel(Widget):
    """Display connected devices."""

    def compose(self) -> ComposeResult:
        with Vertical():
            yield Static(self._device_text(), id="device_list_text")
            yield Button("🎮 Calibrate Joystick  [j]", id="btn_calibrate", variant="default")

    @staticmethod
    def _device_text() -> str:
        output = "🎮 INPUT DEVICES\n"
        output += "─" * 50 + "\n"
        output += "✓ Keyboard (Active)\n"
        output += "(Joysticks will appear here when connected)\n"
        return output

    DEFAULT_CSS = """
    DeviceListPanel {
        height: auto;
        padding: 0;
    }
    DeviceListPanel Vertical {
        height: auto;
    }
    #btn_calibrate {
        margin-top: 1;
        width: auto;
    }
    """


class JoystickVisualizerPanel(Static):
    """Visualize joystick states."""

    @staticmethod
    def _stick_grid(x: float, y: float, width: int = 9, height: int = 5) -> list:
        col = round((x + 1) / 2 * (width - 1))
        row = round((1 - (y + 1) / 2) * (height - 1))
        col = max(0, min(width - 1, col))
        row = max(0, min(height - 1, row))
        cx, cy = width // 2, height // 2
        lines = []
        for r in range(height):
            line = ""
            for c in range(width):
                if r == row and c == col:
                    line += "●"
                elif r == cy and c == cx:
                    line += "+"
                elif r == cy or c == cx:
                    line += "·"
                else:
                    line += " "
            lines.append(line)
        return lines

    @staticmethod
    def _trigger_bar(value: float, width: int = 10) -> str:
        filled = round(value * width)
        return "█" * filled + "░" * (width - filled)

    @staticmethod
    def _dpad_cross(dx: int, dy: int) -> list:
        up    = "▲" if dy > 0 else "△"
        down  = "▼" if dy < 0 else "▽"
        left  = "◄" if dx < 0 else "◁"
        right = "►" if dx > 0 else "▷"
        mid   = "●" if (dx != 0 or dy != 0) else "·"
        return [f"  {up}  ", f"{left} {mid} {right}", f"  {down}  "]

    def render(self) -> str:
        output = "🕹️  JOYSTICK STATE\n" + "─" * 44 + "\n"

        app = self.app
        handler = (
            app.input_manager.joystick_handler
            if hasattr(app, "input_manager") and app.input_manager
            else None
        )

        if not handler:
            lx = ly = rx = ry = lt = rt = 0.0
            dx = dy = 0
            output += f"🎮 (no controller)\n\n"
            output += f"  LT [{'░' * 10}] 0.00   RT [{'░' * 10}] 0.00\n\n"
            ls = self._stick_grid(lx, ly)
            rs = self._stick_grid(rx, ry)
            output += f"  L ({lx:+.2f},{ly:+.2f})     R ({rx:+.2f},{ry:+.2f})\n"
            output += "  ┌─────────┐     ┌─────────┐\n"
            for l_row, r_row in zip(ls, rs):
                output += f"  │{l_row}│     │{r_row}│\n"
            output += "  └─────────┘     └─────────┘\n\n"
            dpad = self._dpad_cross(dx, dy)
            output += f"  D-Pad:           Buttons:\n"
            output += f"  {dpad[0]}              (none)\n"
            output += f"  {dpad[1]}\n"
            output += f"  {dpad[2]}\n"
            return output

        states = handler.get_states()
        if not states:
            output += "No joystick connected\n"
            return output

        for state in states:
            if not state.connected:
                continue
            output += f"🎮 {state.name} (ID:{state.id})\n\n"

            lt = self._trigger_bar(state.left_trigger)
            rt = self._trigger_bar(state.right_trigger)
            output += f"  LTrig  {state.left_trigger:+.2f}    RTrig  {state.right_trigger:+.2f}\n"
            output += "  ┌──────────┐    ┌──────────┐\n"
            output += f"  │{lt}│    │{rt}│\n"
            output += "  └──────────┘    └──────────┘\n\n"
            

            ls = self._stick_grid(state.left_stick_x, state.left_stick_y)
            rs = self._stick_grid(state.right_stick_x, state.right_stick_y)
            lx, ly = state.left_stick_x, state.left_stick_y
            rx, ry = state.right_stick_x, state.right_stick_y
            output += f"  LStick          RStick\n"
            output += f"  {lx:+.2f},{ly:+.2f}     {rx:+.2f},{ry:+.2f}\n"
            output += "  ┌─────────┐     ┌─────────┐\n"
            for l_row, r_row in zip(ls, rs):
                output += f"  │{l_row}│     │{r_row}│\n"
            output += "  └─────────┘     └─────────┘\n\n"

            dpad = self._dpad_cross(*state.dpad)
            active = handler.get_active_buttons_display(state.id) or "(none)"
            output += f"  D-Pad:           Buttons:\n"
            output += f"    {dpad[0]}          {active}\n"
            output += f"    {dpad[1]}\n"
            output += f"    {dpad[2]}\n"

        return output


class KeyboardDisplayPanel(Static):
    """Display keyboard input state."""

    def render(self) -> str:
        output = "⌨️  KEYBOARD STATE\n"
        output += "─" * 50 + "\n"

        app = self.app
        if hasattr(app, "get_key_state"):
            state = app.get_key_state()

            key_labels = [
                ("UP", state.get("up", False)),
                ("DOWN", state.get("down", False)),
                ("LEFT", state.get("left", False)),
                ("RIGHT", state.get("right", False)),
                ("SPACE", state.get("space", False)),
                ("SHIFT", state.get("shift", False)),
                ("ENTER", state.get("enter", False)),
            ]
            output += " ".join(
                f"[bold reverse]{name}[/]" if pressed else f"[dim]{name}[/]"
                for name, pressed in key_labels
            ) + "\n"

            active = app.get_active_keys_display()
            output += f"Pressed: {active}\n"
        else:
            output += "[UP] [DOWN] [LEFT] [RIGHT] [SPACE]\n"
            output += "No keys pressed\n"

        return output


class StatisticsDisplayPanel(Static):
    """Display connection statistics."""

    def render(self) -> str:
        output = "📊 STATISTICS\n"
        output += "─" * 50 + "\n"

        app = self.app
        if hasattr(app, "udp_client"):
            client = app.udp_client
            status = "Connected" if client.connected else "Disconnected"
            icon = "🟢" if client.connected else "🔴"
            avg_ping = client.get_avg_ping_ms()
            ping_str = f"{avg_ping:6.1f} ms" if avg_ping is not None else "  ---.- ms"
            output += f"Status:        {icon} {status}\n"
            output += f"Packets Sent:  {client.packets_sent}\n"
            output += f"Avg Ping (×4): {ping_str}\n"
        else:
            output += "Status:        🔴 Disconnected\n"
            output += "Packets Sent:  0\n"
            output += "Avg Ping (×4):   ---.- ms\n"

        return output


class HelpDisplayPanel(Static):
    """Display help information."""

    def render(self) -> str:
        return """K10 BOT UDP CLIENT - HELP
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
  Default Server:   192.168.1.100:24642
  Edit the server IP and port in the UI
  Click "Connect" to establish connection

TROUBLESHOOTING:
  • Ensure K10 Bot is powered on and connected to network
  • Check firewall settings if connection fails
  • Verify IP address matches your bot's network IP
  • Check that UDP port 24642 is not blocked

SUPPORTED PLATFORMS:
  ✓ Linux
  ✓ macOS
  ✓ Windows (Windows Terminal)

For more info, see README.md in the project root.
════════════════════════════════════════════════════"""


class K10BotClient(App):
    """Main Textual application for K10 Bot UDP client."""

    BINDINGS = [
        ("q", "quit", "Quit"),
        ("c", "connect", "Connect"),
        ("d", "disconnect", "Disconnect"),
        ("j", "calibrate_joystick", "Calibrate Joystick"),
    ]

    KEY_EXPIRY_SECONDS = 0.4
    """How long a key is considered 'pressed' after the last event (terminals have no key-up)."""

    # Mapping from Textual key names to our logical key state names
    _KEY_MAP: dict[str, str] = {
        "up": "up",
        "down": "down",
        "left": "left",
        "right": "right",
        "space": "space",
        "enter": "enter",
    }

    CSS = """
    Screen {
        layout: vertical;
        background: $surface;
    }

    Header {
        dock: top;
    }

    Footer {
        dock: bottom;
    }

    #main-container {
        width: 1fr;
        height: 1fr;
    }

    Input {
        margin: 0 1;
    }

    StatisticsDisplayPanel {
        border: solid $accent;
        padding: 1;
    }

    JoystickVisualizerPanel {
        border: solid $accent;
        padding: 1;
    }

    KeyboardDisplayPanel {
        border: solid $accent;
        padding: 1;
    }

    DeviceListPanel {
        border: solid $accent;
        padding: 1;
    }

    HelpDisplayPanel {
        border: solid $accent;
        padding: 1;
    }

    TabbedContent {
        height: 1fr;
    }
    """

    def compose(self) -> ComposeResult:
        """Compose the UI."""
        yield Header(show_clock=True)

        with Vertical(id="main-container"):
            yield ServerConnectionPanel()

            with TabbedContent():
                with TabPane("🎮 Input", id="tab-input"):
                    with VerticalScroll():
                        yield DeviceListPanel()
                        yield KeyboardDisplayPanel()
                        yield JoystickVisualizerPanel()

                with TabPane("📊 Stats", id="tab-stats"):
                    with VerticalScroll():
                        yield StatisticsDisplayPanel()

                with TabPane("❓ Help", id="tab-help"):
                    with VerticalScroll():
                        yield HelpDisplayPanel()

        yield Footer()

    def on_key(self, event) -> None:
        """Track key presses via Textual events (terminals have no key-up)."""
        now = time.monotonic()
        key: str = event.key

        # Don't track keys when the server address input is focused
        focused = self.focused
        if isinstance(focused, Input):
            return

        # Map direct key name
        mapped = self._KEY_MAP.get(key)
        if mapped:
            self._pressed_keys[mapped] = now

        # Handle modifier+key combos (e.g. "shift+up")
        if "+" in key:
            parts = key.split("+")
            base = parts[-1]
            mapped = self._KEY_MAP.get(base)
            if mapped:
                self._pressed_keys[mapped] = now
            # Track shift modifier
            if "shift" in parts:
                self._pressed_keys["shift"] = now

        # Track single alphanumeric keys (w/a/s/d etc.)
        if len(key) == 1 and key.isalnum():
            self._pressed_keys[key.lower()] = now

    def get_key_state(self) -> dict[str, bool]:
        """Get current key state with auto-expiry."""
        now = time.monotonic()
        expiry = self.KEY_EXPIRY_SECONDS
        keys = ["up", "down", "left", "right", "space", "shift", "enter"]
        return {k: (now - self._pressed_keys.get(k, 0)) < expiry for k in keys}

    def get_active_keys_display(self) -> str:
        """Get display string of currently active keys."""
        now = time.monotonic()
        expiry = self.KEY_EXPIRY_SECONDS
        active: list[str] = []
        for key, ts in self._pressed_keys.items():
            if (now - ts) < expiry:
                active.append(key.upper())
        return " ".join(sorted(active)) if active else "(no keys)"

    def on_mount(self) -> None:
        """Initialize on mount."""
        self.title = "K10 Bot UDP Client"
        self.sub_title = "Cross-Platform Textual UDP Controller"

        # Textual-driven key tracking (timestamp per key, auto-expires)
        self._pressed_keys: dict[str, float] = {}

        # Initialize network and input
        self.udp_client = UDPClient(DEFAULT_SERVER_IP, DEFAULT_SERVER_PORT)

        # Wire up connection callbacks so the UI reacts to async connect/error
        self.udp_client.on_connect = lambda: self.notify(
            f"✅ Connected to {self.udp_client.server_ip}:{self.udp_client.server_port}",
            timeout=3,
        )
        self.udp_client.on_disconnect = lambda: self.notify("Disconnected", timeout=2)
        self.udp_client.on_error = lambda msg: self.notify(
            f"⚠️ {msg}", timeout=4, severity="warning"
        )

        self.input_manager = InputManager(
            keyboard_enabled=False,  # Use Textual key events instead of pynput
            joystick_enabled=JOYSTICK_ENABLED,
        )
        self.input_manager.start()

        # Set default value
        self.query_one("#server_address", Input).value = f"{DEFAULT_SERVER_IP}:{DEFAULT_SERVER_PORT}"

        # Periodic UI refresh for live input state
        self._refresh_timer = self.set_interval(1.0 / 10, self._refresh_panels)

        # Heartbeat loop: fires every 40 ms, sends 0x43 only while connected
        self._heartbeat_timer = self.set_interval(HEARTBEAT_INTERVAL, self._send_heartbeat)

        # Ping loop: fires every 250 ms, verifies link and measures RTT
        self._ping_timer = self.set_interval(PING_INTERVAL, self._send_ping)

    def on_button_pressed(self, event: Button.Pressed) -> None:
        """Handle button presses."""
        if event.button.id == "btn_connect":
            self.action_connect()
        elif event.button.id == "btn_disconnect":
            self.action_disconnect()
        elif event.button.id == "btn_calibrate":
            self.action_calibrate_joystick()

    def action_connect(self) -> None:
        """Connect to server."""
        try:
            address = self.query_one("#server_address", Input).value.strip()

            if ":" not in address:
                self.notify("Use host:port format (e.g. 192.168.1.178:24642)", timeout=3, severity="error")
                return

            server_ip, port_str = address.rsplit(":", 1)

            if not port_str.isdigit():
                self.notify("Invalid port number", timeout=3, severity="error")
                return

            server_port = int(port_str)
            server_ip = server_ip.strip()

            self.udp_client.server_ip = server_ip
            self.udp_client.server_port = server_port

            # Start connection in background
            self.run_worker(self.udp_client.connect(), exclusive=False)
            self.notify(f"Connecting to {server_ip}:{server_port}…", timeout=2)

        except Exception as e:
            self.notify(f"Connection error: {str(e)}", timeout=3, severity="error")

    def action_disconnect(self) -> None:
        """Disconnect from server."""
        self.run_worker(self.udp_client.disconnect(), exclusive=False)
        self.notify("Disconnected", timeout=2)

    async def _send_heartbeat(self) -> None:
        """Send a heartbeat packet every 40 ms while connected."""
        if hasattr(self, "udp_client") and self.udp_client.connected:
            await self.udp_client.send_heartbeat()

    async def _send_ping(self) -> None:
        """Send a 0x44 ping every 250 ms to verify the link and measure RTT."""
        if hasattr(self, "udp_client") and self.udp_client.connected:
            await self.udp_client.ping()

    def _refresh_panels(self) -> None:
        """Pump input events and refresh display panels with live input state."""
        try:
            if hasattr(self, "input_manager") and self.input_manager:
                self.input_manager.update()
            self.query_one(KeyboardDisplayPanel).refresh()
            self.query_one(JoystickVisualizerPanel).refresh()
            self.query_one(StatisticsDisplayPanel).refresh()
        except Exception:
            pass

    def action_calibrate_joystick(self) -> None:
        """Suspend the TUI, run the interactive calibration wizard, then resume."""
        handler = (
            self.input_manager.joystick_handler
            if hasattr(self, "input_manager") and self.input_manager
            else None
        )
        with self.suspend():
            try:
                result = run_wizard()
            except KeyboardInterrupt:
                result = None
            finally:
                import pygame as _pg
                if _pg.get_init():
                    _pg.quit()

        if result is not None:
            # Re-init pygame and reload calibration into the live joystick handler
            import pygame as _pg
            _pg.init()
            _pg.joystick.init()
            if handler is not None:
                handler.reload_calibration()
            self.notify("✅ Joystick calibration saved and applied.", timeout=4)
        else:
            # Still re-init pygame so normal joystick input keeps working
            import pygame as _pg
            _pg.init()
            _pg.joystick.init()
            self.notify("Calibration cancelled.", timeout=3, severity="warning")

    def action_quit(self) -> None:
        """Quit the application."""
        if hasattr(self, "input_manager") and self.input_manager:
            self.input_manager.stop()
        self.exit()


def main() -> None:
    """Main entry point."""
    app = K10BotClient()
    app.run()


if __name__ == "__main__":
    main()
