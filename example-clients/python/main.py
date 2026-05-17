#!/usr/bin/env python3
"""
K10 Bot UDP Client — Main Entry Point
======================================

This file is the application entry point and TUI shell.  You should not need
to edit it.  To customise your bot's hardware mapping and control bindings,
edit ``customize.py`` instead — that is the only file intended for users.

Architecture overview::

    customize.py   ← ★ EDIT THIS FILE to define your bot
    config.py      ← network / UI settings (server IP, heartbeat rate, …)
    main.py        ← Textual TUI app  (this file)
    udp_client/
      control/     ← Controller: input state → UDP packets
      network/     ← UDPClient: async socket
      input/       ← Keyboard + joystick handlers
      state/       ← Application state model
      ui/
        constants.py  ← Shared display constants & messages
        widgets/      ← Reusable TUI widget panels
      bot_config.py← BotConfig: hardware declaration + packet builder
"""

import sys
import time
from datetime import datetime
from pathlib import Path

# Ensure project root is on sys.path so config.py / customize.py resolve
# when running directly (``python main.py``).  Redundant if the package was
# installed with ``pip install -e .``.
_PROJECT_ROOT = Path(__file__).parent
if str(_PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(_PROJECT_ROOT))

from textual.app import App, ComposeResult
from textual.containers import Vertical, VerticalScroll
from textual.widgets import (
    Button,
    Footer,
    Header,
    Input,
    TabbedContent,
    TabPane,
)

from config import (
    DEFAULT_SERVER_IP,
    DEFAULT_SERVER_PORT,
    HEARTBEAT_INTERVAL,
    JOYSTICK_ENABLED,
    PING_INTERVAL,
)
from udp_client.control.controller import Controller
from udp_client.input.calibration_wizard import run_wizard
from udp_client.input.input_manager import InputManager
from udp_client.network.udp_client import UDPClient
from udp_client.ui.constants import ActionLogged
from udp_client.ui.widgets import (
    ActionHistoryPanel,
    BotConfigInspectorPanel,
    BotStatusPanel,
    ConnectionWizardScreen,
    CurrentActionIndicator,
    DeviceListPanel,
    HelpDisplayPanel,
    JoystickVisualizerPanel,
    KeyboardDisplayPanel,
    ServerConnectionPanel,
    StatisticsDisplayPanel,
    VirtualDpadPanel,
)

# ---------------------------------------------------------------------------
# Persistence helpers — remember last used server address
# ---------------------------------------------------------------------------

_LAST_SERVER_FILE = _PROJECT_ROOT / ".last_server"


def load_last_server() -> str | None:
    """Return the last-used server string (``host:port``) or ``None``."""
    try:
        return _LAST_SERVER_FILE.read_text().strip() or None
    except Exception:
        return None


def save_last_server(address: str) -> None:
    """Persist *address* so it is pre-filled on next launch."""
    try:
        _LAST_SERVER_FILE.write_text(address)
    except Exception:
        pass


# ---------------------------------------------------------------------------
# Main application
# ---------------------------------------------------------------------------


class K10BotClient(App):
    """Main Textual application for K10 Bot UDP client."""

    BINDINGS = [
        ("q", "quit", "Quit"),
        ("c", "connect", "Connect"),
        ("d", "disconnect", "Disconnect"),
        ("j", "calibrate_joystick", "Calibrate"),
        ("r", "reload_config", "Reload config"),
        ("t", "toggle_theme", "Theme"),
        ("w", "connection_wizard", "Wizard"),
    ]

    _THEMES = ["textual-dark", "textual-light"]

    KEY_EXPIRY_SECONDS = 0.2
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

    BotStatusPanel {
        border: solid $success;
        padding: 1;
        height: auto;
    }

    ActionHistoryPanel {
        border: solid $warning;
        padding: 1;
        height: auto;
    }

    BotConfigInspectorPanel {
        border: solid $primary;
        padding: 1;
        height: auto;
    }

    VirtualDpadPanel {
        border: solid $accent;
        padding: 1;
        height: auto;
    }

    JoystickVisualizerPanel.compact {
        display: none;
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

    CurrentActionIndicator {
        height: 3;
        border: heavy $success;
        content-align: center middle;
        text-align: center;
        text-style: bold;
        padding: 0 2;
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
            yield CurrentActionIndicator()

            with TabbedContent():
                with TabPane("🎮 Input", id="tab-input"):
                    with VerticalScroll():
                        yield DeviceListPanel()
                        yield VirtualDpadPanel()
                        yield KeyboardDisplayPanel()
                        yield JoystickVisualizerPanel()

                with TabPane("📊 Stats", id="tab-stats"):
                    with VerticalScroll():
                        yield BotStatusPanel()
                        yield ActionHistoryPanel()
                        yield StatisticsDisplayPanel()

                with TabPane("❓ Help", id="tab-help"):
                    with VerticalScroll():
                        yield HelpDisplayPanel()

                with TabPane("🔧 Config", id="tab-config"):
                    with VerticalScroll():
                        yield BotConfigInspectorPanel()

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

        # Load customize.py mappings once and keep a controller for the tick loop.
        self.controller = Controller(self.udp_client)

        # Wire up connection callbacks so the UI reacts to async connect/error
        self.udp_client.on_connect = lambda: (
            save_last_server(f"{self.udp_client.server_ip}:{self.udp_client.server_port}"),
            self.notify(
                f"✅ Connected to {self.udp_client.server_ip}:{self.udp_client.server_port}",
                timeout=3,
            ),
            # Send servo type definitions so the bot knows each channel's mode
            self.run_worker(self._send_servo_definitions(), exclusive=False),
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

        # Notify user if joystick was requested but pygame is missing
        if JOYSTICK_ENABLED and not self.input_manager.joystick_enabled:
            self.notify(
                "⚠️ Joystick disabled: pygame not installed. Run: pip install pygame",
                severity="warning",
                timeout=8,
            )

        # Set default value — prefer last-used server, fall back to config default
        saved = load_last_server()
        initial_address = saved if saved else f"{DEFAULT_SERVER_IP}:{DEFAULT_SERVER_PORT}"
        self.query_one("#server_address", Input).value = initial_address

        # Periodic UI refresh for live input state
        self._refresh_timer = self.set_interval(1.0 / 10, self._refresh_panels)

        # Heartbeat loop: fires every 40 ms, sends 0x43 only while connected
        self._heartbeat_timer = self.set_interval(HEARTBEAT_INTERVAL, self._send_heartbeat)

        # Ping loop: fires every 250 ms, verifies link and measures RTT
        self._ping_timer = self.set_interval(PING_INTERVAL, self._send_ping)

        # Show connection wizard on first run (no saved server)
        if not saved:
            self.set_timer(0.3, self._show_wizard)

    def on_button_pressed(self, event: Button.Pressed) -> None:
        """Handle button presses."""
        if event.button.id == "btn_connect":
            self.action_connect()
        elif event.button.id == "btn_disconnect":
            self.action_disconnect()
        elif event.button.id == "btn_calibrate":
            self.action_calibrate_joystick()

    async def _send_servo_definitions(self) -> None:
        """Send SET_SERVO_TYPE packets for every registered servo.

        Called automatically after a successful connection so the bot knows
        each servo channel's mode (180°, 270°, or continuous).
        """
        cfg = self.controller.bot_config
        if cfg is None:
            return
        for name in cfg.servo_names:
            pkt = cfg.build_servo_type_packet(name)
            await self.udp_client.send_raw(pkt)
        if cfg.servo_names:
            self.notify(
                f"📡 Sent servo definitions ({len(cfg.servo_names)} channels)",
                timeout=2,
            )

    def action_connect(self) -> None:
        """Connect to server."""
        try:
            address = self.query_one("#server_address", Input).value.strip()

            if ":" not in address:
                self.notify(
                    "Use host:port format (e.g. 192.168.1.178:24642)",
                    timeout=3,
                    severity="error",
                )
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

    def action_connection_wizard(self) -> None:
        """Open the connection wizard modal."""
        self.push_screen(ConnectionWizardScreen(), self._on_wizard_result)

    def _show_wizard(self) -> None:
        """Called from set_timer after mount to show the wizard on first run."""
        self.push_screen(ConnectionWizardScreen(), self._on_wizard_result)

    def _on_wizard_result(self, address: str | None) -> None:
        """Callback when the wizard is dismissed; pre-fills and connects if an IP was chosen."""
        if address:
            self.query_one("#server_address", Input).value = address
            self.action_connect()

    async def _send_heartbeat(self) -> None:
        """Send a heartbeat packet and dispatch control commands every 40 ms."""
        if not (hasattr(self, "udp_client") and self.udp_client.connected):
            return
        await self.udp_client.send_heartbeat()

        # Dispatch keyboard + joystick input → bot commands (customize.py)
        if hasattr(self, "controller"):
            joystick_states = (
                self.input_manager.joystick_handler.get_states()
                if self.input_manager and self.input_manager.joystick_handler
                else []
            )
            prev_time = self.controller.last_action_time
            await self.controller.tick(self.get_key_state(), joystick_states)
            # Post a message if the action changed (new dispatch)
            if (
                self.controller.last_action is not None
                and self.controller.last_action_time != prev_time
            ):
                self.post_message(ActionLogged(self.controller.last_action, datetime.now()))

    async def _send_ping(self) -> None:
        """Send a 0x44 ping every 250 ms to verify the link and measure RTT."""
        if hasattr(self, "udp_client") and self.udp_client.connected:
            await self.udp_client.ping()

    def _refresh_panels(self) -> None:
        """Pump input events and refresh display panels with live input state."""
        try:
            if hasattr(self, "input_manager") and self.input_manager:
                self.input_manager.update()

            # Compact mode: hide joystick panel when no gamepad is connected
            handler = (
                self.input_manager.joystick_handler
                if self.input_manager and self.input_manager.joystick_handler
                else None
            )
            has_joystick = bool(handler and any(s.connected for s in handler.get_states()))
            joy_panel = self.query_one(JoystickVisualizerPanel)
            joy_panel.set_class(not has_joystick, "compact")

            self.query_one(CurrentActionIndicator).refresh()
            self.query_one(VirtualDpadPanel).refresh()
            self.query_one(KeyboardDisplayPanel).refresh()
            self.query_one(JoystickVisualizerPanel).refresh()
            self.query_one(StatisticsDisplayPanel).refresh()
            self.query_one(BotStatusPanel).refresh()
            self.query_one(BotConfigInspectorPanel).refresh()
        except Exception:
            pass

    def action_calibrate_joystick(self) -> None:
        """Suspend the TUI, run the interactive calibration wizard, then resume."""
        if not self.input_manager.joystick_enabled:
            self.notify(
                "⚠️ Joystick not available (pygame not installed).",
                severity="warning",
                timeout=4,
            )
            return

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
            import pygame as _pg

            _pg.init()
            _pg.joystick.init()
            if handler is not None:
                handler.reload_calibration()
            self.notify("✅ Joystick calibration saved and applied.", timeout=4)
        else:
            import pygame as _pg

            _pg.init()
            _pg.joystick.init()
            self.notify("Calibration cancelled.", timeout=3, severity="warning")

    def action_reload_config(self) -> None:
        """Live-reload customize.py and show a diff notification."""
        ctrl = getattr(self, "controller", None)
        if ctrl is None:
            self.notify("⚠️ No controller running.", severity="warning", timeout=3)
            return

        # Snapshot current state for diff
        old_act_keys = set(ctrl.bot_config.action_names) if ctrl.bot_config else set()

        ctrl._load_customize()

        # Build diff summary
        new_act_keys = set(ctrl.bot_config.action_names) if ctrl.bot_config else set()
        added = new_act_keys - old_act_keys
        removed = old_act_keys - new_act_keys

        parts: list[str] = []
        if added:
            parts.append(", ".join(f"+{a}" for a in sorted(added)))
        if removed:
            parts.append(", ".join(f"-{a}" for a in sorted(removed)))
        diff_str = f"  ({', '.join(parts)})" if parts else "  (no action changes)"

        self.notify(f"✅ customize.py reloaded{diff_str}", timeout=5)
        try:
            self.query_one(BotConfigInspectorPanel).refresh()
            self.query_one(VirtualDpadPanel).refresh()
        except Exception:
            pass

    def action_toggle_theme(self) -> None:
        """Cycle through dark / light themes."""
        current = self.theme or self._THEMES[0]
        idx = self._THEMES.index(current) if current in self._THEMES else 0
        self.theme = self._THEMES[(idx + 1) % len(self._THEMES)]
        self.notify(f"🎨 Theme: {self.theme}", timeout=2)

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
