"""Device list panel — shows connected input devices."""

from textual.app import ComposeResult
from textual.containers import Vertical
from textual.widget import Widget
from textual.widgets import Button, Static


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
