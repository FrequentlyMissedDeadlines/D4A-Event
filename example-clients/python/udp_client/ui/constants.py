"""
Shared constants and messages used across TUI widgets.
"""

from datetime import datetime

from textual.message import Message

# ---------------------------------------------------------------------------
# Action display maps (shared by CurrentActionIndicator and ActionHistoryPanel)
# ---------------------------------------------------------------------------

ACTION_COLOUR: dict[str, str] = {
    "forward": "bold green",
    "backward": "bold red",
    "turn_left": "bold cyan",
    "turn_right": "bold cyan",
    "stop": "bold red",
}

ACTION_ICONS: dict[str, str] = {
    "forward": "←",
    "backward": "→",
    "turn_left": "↪",
    "turn_right": "↩",
    "stop": "■",
}


class ActionLogged(Message):
    """Posted by the app whenever a new bot action is dispatched."""

    def __init__(self, action: str, ts: datetime) -> None:
        super().__init__()
        self.action = action
        self.ts = ts
