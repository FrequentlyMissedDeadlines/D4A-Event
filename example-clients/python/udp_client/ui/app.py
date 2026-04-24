"""
Textual TUI application for K10 Bot UDP client.
"""

import asyncio
from typing import Optional

from textual.app import ComposeResult
from textual.containers import Container, Horizontal, Vertical, VerticalScroll
from textual.reactive import reactive
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

from udp_client.input.input_manager import InputManager
from udp_client.input.joystick_handler import JoystickHandler
from udp_client.network.udp_client import UDPClient
from udp_client.state.app_state import AppState


class StatusIndicator(Static):
    """Real-time connection status indicator."""

    app_state: reactive[AppState] = reactive(AppState())

    def render(self) -> str:
        icon = self.app_state.get_status_icon()
        status = "Connected" if self.app_state.connected else "Disconnected"
        color = "green" if self.app_state.connected else "red"
        return f"[bold {color}]{icon} {status}[/bold {color}]"


class ServerPanel(Static):
    """Server connection settings and status."""

    def compose(self) -> ComposeResult:
        with Horizontal():
            yield Label("📡 Server:")
            yield Input(value="192.168.1.100", id="server_ip", classes="input-small")
            yield Label(":")
            yield Input(value="24642", id="server_port", classes="input-small")
            yield Button("Connect", id="btn_connect", variant="primary")
            yield Button("Disconnect", id="btn_disconnect")

    def on_mount(self) -> None:
        self.query_one("#server_ip", Input).value = "192.168.1.100"
        self.query_one("#server_port", Input).value = "24642"


class JoystickDisplay(Static):
    """Display for a single joystick state."""

    def __init__(self, joystick_id: int, name: str, **kwargs):
        super().__init__(**kwargs)
        self.joystick_id = joystick_id
        self.name = name

    def render(self) -> str:
        return f"🎮 {self.name} (ID: {self.joystick_id})"


class InputDevicesPanel(Static):
    """Display connected input devices."""

    def render(self) -> str:
        output = "🎮 INPUT DEVICES \n"
        output += "─" * 40 + "\n"
        output += "✓ Keyboard (Always available)\n"
        output += "(Joysticks will appear when connected)\n"
        return output


class JoystickStatePanel(Static):
    """Display current joystick states with visualization."""

    joystick_handler: Optional[JoystickHandler] = None

    @staticmethod
    def _stick_grid(x: float, y: float, width: int = 9, height: int = 5) -> list:
        """Render a small 2D grid showing analog stick position."""
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
        """Render a fill bar for a trigger (0.0–1.0)."""
        filled = round(value * width)
        return "█" * filled + "░" * (width - filled)

    @staticmethod
    def _dpad_cross(dx: int, dy: int) -> list:
        """Render a 3-line directional cross for the D-Pad."""
        up    = "▲" if dy > 0 else "△"
        down  = "▼" if dy < 0 else "▽"
        left  = "◄" if dx < 0 else "◁"
        right = "►" if dx > 0 else "▷"
        mid   = "●" if (dx != 0 or dy != 0) else "·"
        return [
            f"  {up}  ",
            f"{left} {mid} {right}",
            f"  {down}  ",
        ]

    def render(self) -> str:
        if not self.joystick_handler:
            return "🕹️  JOYSTICK STATE \n" + "─" * 40 + "\n(No joystick handler)"

        output = "🕹️  JOYSTICK STATE \n" + "─" * 44 + "\n"

        states = self.joystick_handler.get_states()
        if not states:
            return output + "No joystick connected\n"

        for state in states:
            output += f"🎮 {state.name} (ID:{state.id})\n\n"

            # ── Triggers ────────────────────────────────────────
            lt = self._trigger_bar(state.left_trigger)
            rt = self._trigger_bar(state.right_trigger)
            output += f"  LT [{lt}] {state.left_trigger:.2f}"
            output += f"   RT [{rt}] {state.right_trigger:.2f}\n\n"

            # ── Analog sticks side by side ───────────────────────
            ls = self._stick_grid(state.left_stick_x, state.left_stick_y)
            rs = self._stick_grid(state.right_stick_x, state.right_stick_y)
            lx, ly = state.left_stick_x, state.left_stick_y
            rx, ry = state.right_stick_x, state.right_stick_y
            output += f"  L     ({lx:+.2f},{ly:+.2f})     R ({rx:+.2f},{ry:+.2f})\n"
            output += "  ┌─────────┐     ┌─────────┐\n"
            for l_row, r_row in zip(ls, rs):
                output += f"  │{l_row}│     │{r_row}│\n"
            output += "  └─────────┘     └─────────┘\n\n"

            # ── D-Pad + Buttons ──────────────────────────────────
            dpad = self._dpad_cross(*state.dpad)
            active = self.joystick_handler.get_active_buttons_display(state.id) or "(none)"
            output += f"  D-Pad:           Buttons:\n"
            output += f"  {dpad[0]}              {active}\n"
            output += f"  {dpad[1]}\n"
            output += f"  {dpad[2]}\n"

        return output


class KeyboardStatePanel(Static):
    """Display current keyboard state."""

    keyboard_keys: dict = {}

    def render(self) -> str:
        output = "⌨️  KEYBOARD STATE \n" + "─" * 40 + "\n"

        if not self.keyboard_keys:
            output += "No keys pressed\n"
            return output

        pressed = [key.upper() for key, pressed in self.keyboard_keys.items() if pressed]

        if pressed:
            output += f"Pressed: {' '.join(pressed)}\n"
        else:
            output += "No keys pressed\n"

        return output


class StatisticsPanel(Static):
    """Display connection and performance statistics."""

    packets_sent: reactive[int] = reactive(0)
    latency_ms: reactive[Optional[float]] = reactive(None)
    commands_per_sec: reactive[float] = reactive(0.0)

    def render(self) -> str:
        output = "📊 STATISTICS\n" + "─" * 40 + "\n"
        output += f"Packets sent:  {self.packets_sent}\n"
        output += f"Commands/sec:  {self.commands_per_sec:.1f}\n"

        if self.latency_ms is not None:
            output += f"Latency:       {self.latency_ms:.1f} ms\n"
        else:
            output += "Latency:       -- ms\n"

        return output


class HelpPanel(Static):
    """Help and keyboard shortcuts."""

    def render(self) -> str:
        return """KEYBOARD SHORTCUTS
─────────────────────────────────
[q]       Quit application
[c]       Connect to server
[d]       Disconnect from server
[?]       Show/hide this help
[arrows]  Navigate if using keyboard input

KEYBOARD CONTROL
─────────────────────────────────
Arrow Keys  Movement (Up/Down/Left/Right)
Space       Action button
Shift       Modifier key
Enter       Confirm/Send

JOYSTICK MAPPING
─────────────────────────────────
Left Stick  Primary control
Right Stick Secondary control / Aiming
LT / RT     Analog triggers
A / B / X / Y  Action buttons
LB / RB     Shoulder buttons

NOTE: Ensure the K10 Bot is running
and accessible at the configured address.
"""


class K10BotClientApp(Container):
    """Main application container."""

    CSS = """
    Screen {
        layout: vertical;
    }

    #status-bar {
        height: 1;
        dock: top;
        background: $surface;
        border: solid $accent;
    }

    #main-panel {
        width: 1fr;
        height: 1fr;
    }

    .input-small {
        width: 15;
    }

    ServerPanel {
        height: 3;
        border: solid $accent;
        padding: 1;
    }

    StatisticsPanel {
        height: auto;
        border: solid $accent;
        padding: 1;
    }

    JoystickStatePanel {
        height: auto;
        border: solid $accent;
        padding: 1;
    }

    KeyboardStatePanel {
        height: auto;
        border: solid $accent;
        padding: 1;
    }

    HelpPanel {
        height: auto;
        border: solid $accent;
        padding: 1;
    }

    InputDevicesPanel {
        height: auto;
        border: solid $accent;
        padding: 1;
    }
    """

    def compose(self) -> ComposeResult:
        yield Header(show_clock=True)

        with Vertical(id="main-panel"):
            yield ServerPanel()

            with TabbedContent():
                with TabPane("🎮 Input", id="tab-input"):
                    with VerticalScroll():
                        yield InputDevicesPanel()
                        yield KeyboardStatePanel(id="keyboard-panel")
                        yield JoystickStatePanel(id="joystick-panel")

                with TabPane("📊 Stats", id="tab-stats"):
                    with VerticalScroll():
                        yield StatisticsPanel(id="stats-panel")

                with TabPane("❓ Help", id="tab-help"):
                    with VerticalScroll():
                        yield HelpPanel()

        yield Footer()

    def on_mount(self) -> None:
        """Initialize the app on mount."""
        self.title = "K10 Bot UDP Client"
        self.sub_title = "A cross-platform Textual TUI controller"
