"""Server connection panel widget."""

from textual.app import ComposeResult
from textual.containers import Horizontal
from textual.widget import Widget
from textual.widgets import Button, Input, Label


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
