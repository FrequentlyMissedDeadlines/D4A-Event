"""Keyboard display panel — shows pressed keys."""

from textual.widgets import Static


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
            output += (
                " ".join(
                    f"[bold reverse]{name}[/]" if pressed else f"[dim]{name}[/]"
                    for name, pressed in key_labels
                )
                + "\n"
            )

            active = app.get_active_keys_display()
            output += f"Pressed: {active}\n"
        else:
            output += "[UP] [DOWN] [LEFT] [RIGHT] [SPACE]\n"
            output += "No keys pressed\n"

        return output
