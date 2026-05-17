"""Joystick visualizer panel — ASCII rendering of gamepad state."""

from textual.widgets import Static


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
        up = "▲" if dy > 0 else "△"
        down = "▼" if dy < 0 else "▽"
        left = "◄" if dx < 0 else "◁"
        right = "►" if dx > 0 else "▷"
        mid = "●" if (dx != 0 or dy != 0) else "·"
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
            output += "🎮 (no controller)\n\n"
            output += f"  LT [{'░' * 10}] 0.00   RT [{'░' * 10}] 0.00\n\n"
            ls = self._stick_grid(lx, ly)
            rs = self._stick_grid(rx, ry)
            output += f"  L ({lx:+.2f},{ly:+.2f})     R ({rx:+.2f},{ry:+.2f})\n"
            output += "  ┌─────────┐     ┌─────────┐\n"
            for l_row, r_row in zip(ls, rs):
                output += f"  │{l_row}│     │{r_row}│\n"
            output += "  └─────────┘     └─────────┘\n\n"
            dpad = self._dpad_cross(dx, dy)
            output += "  D-Pad:           Buttons:\n"
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
            output += "  LStick          RStick\n"
            output += f"  {lx:+.2f},{ly:+.2f}     {rx:+.2f},{ry:+.2f}\n"
            output += "  ┌─────────┐     ┌─────────┐\n"
            for l_row, r_row in zip(ls, rs):
                output += f"  │{l_row}│     │{r_row}│\n"
            output += "  └─────────┘     └─────────┘\n\n"

            dpad = self._dpad_cross(*state.dpad)
            active = handler.get_active_buttons_display(state.id) or "(none)"
            output += "  D-Pad:           Buttons:\n"
            output += f"    {dpad[0]}          {active}\n"
            output += f"    {dpad[1]}\n"
            output += f"    {dpad[2]}\n"

        return output
