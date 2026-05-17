"""Current action indicator — large banner showing the active bot action."""

from textual.widgets import Static

from udp_client.ui.constants import ACTION_COLOUR, ACTION_ICONS


class CurrentActionIndicator(Static):
    """Large full-width banner showing the current bot action — readable from a distance."""

    def render(self) -> str:
        ctrl = getattr(self.app, "controller", None)
        action = (ctrl.last_action or "idle") if ctrl else "idle"
        icon = ACTION_ICONS.get(action, "●")
        colour = ACTION_COLOUR.get(action, "white")
        return f"[{colour}]  {icon}  {action.upper()}[/{colour}]"
