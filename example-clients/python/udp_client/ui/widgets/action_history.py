"""Action history panel — scrolling list of recent bot actions."""

from collections import deque
from datetime import datetime

from textual.widgets import Static

from udp_client.ui.constants import ACTION_ICONS, ActionLogged


class ActionHistoryPanel(Static):
    """Scrolling list of the last 10 dispatched bot actions."""

    MAX_ENTRIES = 10

    def __init__(self) -> None:
        super().__init__()
        self._history: deque[tuple[datetime, str]] = deque(maxlen=self.MAX_ENTRIES)

    def on_action_logged(self, event: ActionLogged) -> None:
        """Receive a new action entry and re-render."""
        self._history.append((event.ts, event.action))
        self.refresh()

    def render(self) -> str:
        output = "📋 ACTION HISTORY\n"
        output += "─" * 50 + "\n"
        if not self._history:
            output += "  (no actions yet)\n"
            return output
        for ts, action in reversed(self._history):
            icon = ACTION_ICONS.get(action, "←")
            output += f"  {ts.strftime('%H:%M:%S.%f')[:-3]}  {action:<20} {icon}\n"
        return output
