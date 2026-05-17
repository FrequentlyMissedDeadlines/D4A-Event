"""Bot status panel — live motor bars, connection, last action."""

import time

from textual.widgets import Static


class BotStatusPanel(Static):
    """Live bot activity panel: connection, motor bars, last action."""

    @staticmethod
    def _motor_bar(speed: int, width: int = 10) -> str:
        """Render a signed speed bar.  Positive → right half filled, negative → left."""
        half = width // 2
        filled = round(abs(speed) / 100 * half)
        if speed >= 0:
            left_part = "░" * half
            right_part = "█" * filled + "░" * (half - filled)
        else:
            left_part = "░" * (half - filled) + "█" * filled
            right_part = "░" * half
        return left_part + right_part

    def render(self) -> str:  # noqa: C901
        app = self.app
        client = getattr(app, "udp_client", None)
        ctrl = getattr(app, "controller", None)

        # ─── Header row ─────────────────────────────────────────────────
        if client and client.connected:
            conn_icon, conn_label = "🟢", "Connected  "
        else:
            conn_icon, conn_label = "🔴", "Disconnected"

        avg_ping = client.get_avg_ping_ms() if client else None
        ping_str = f"⚡{avg_ping:5.1f} ms" if avg_ping is not None else "⚡  --- ms"
        pkts = client.packets_sent if client else 0

        width = 56
        border = "─" * width
        output = f"┌─ 🤖 BOT STATUS ─{border[13:]}\u2510\n"
        output += f"│  {conn_icon} {conn_label}  │  {ping_str}  │  📦 {pkts:>6} pkts  │\n"
        output += f"├{border}┤\n"

        # ─── Motor / servo rows ─────────────────────────────────────────
        if ctrl and ctrl.bot_config is not None:
            motor_names = ctrl.bot_config.motor_names
            servo_names = ctrl.bot_config.servo_names
        else:
            motor_names, servo_names = [], []

        channels = [(name, "motor") for name in motor_names] + [
            (name, "servo") for name in servo_names
        ]

        if not channels:
            output += f"│  (no motors/servos configured — edit customize.py){' ' * 7}│\n"
        else:
            # Render two channels per row
            pairs = [channels[i : i + 2] for i in range(0, len(channels), 2)]
            for pair in pairs:
                row = "│  "
                for name, kind in pair:
                    if kind == "motor":
                        speed = ctrl.motor_speeds.get(name, 0) if ctrl else 0
                    else:
                        speed = ctrl.servo_speeds.get(name, 0) if ctrl else 0
                    bar = self._motor_bar(speed)
                    label = name.upper()[:6]
                    row += f"{label:<6}  [{bar}]  {speed:+4d}    "
                # pad to full width
                inner = width - 2
                row = row[: inner + 2]  # trim if too long
                row = row.ljust(inner + 2)
                output += row[: inner + 2] + "│\n"

        output += f"├{border}┤\n"

        # ─── Last action row ─────────────────────────────────────────────
        if ctrl and ctrl.last_action is not None and ctrl.last_action_time is not None:
            elapsed = time.monotonic() - ctrl.last_action_time
            action_name = ctrl.last_action
            # Build a small ASCII time-since bar (10 chars, 0=now, 2s=full)
            age_frac = min(elapsed / 2.0, 1.0)
            bar_len = 8
            filled_b = round((1.0 - age_frac) * bar_len)
            age_bar = "▲" * filled_b + "▽" * (bar_len - filled_b)
            output += f"│  Last action: {action_name:<20} [{age_bar}] {elapsed:4.1f} s ago  │\n"
        else:
            output += f"│  Last action: (none yet){' ' * 31}│\n"

        output += f"└{border}┘\n"
        return output
