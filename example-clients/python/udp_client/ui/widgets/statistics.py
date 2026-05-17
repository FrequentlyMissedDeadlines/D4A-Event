"""Statistics display panel — ping sparkline and packet-rate gauge."""

from textual.widgets import Static


class StatisticsDisplayPanel(Static):
    """Display connection statistics with ping sparkline and packet-rate gauge."""

    # Block characters ordered from lowest to highest fill
    _SPARK_CHARS = " ▁▂▃▄▅▆▇█"
    # Expected cadence: 1 heartbeat every 40 ms → 25 pkts/s
    _TARGET_RATE: float = 25.0

    @classmethod
    def _sparkline(cls, samples: list[float], width: int = 20) -> str:
        """Render *samples* as a width-char ASCII sparkline."""
        if not samples:
            return cls._SPARK_CHARS[0] * width
        # Pad or trim to exactly *width* chars (newest at the right)
        padded = ([0.0] * max(0, width - len(samples)) + list(samples))[-width:]
        max_v = max(padded) or 1.0
        levels = len(cls._SPARK_CHARS) - 1
        return "".join(cls._SPARK_CHARS[round(v / max_v * levels)] for v in padded)

    @classmethod
    def _rate_bar(cls, rate: float, width: int = 20) -> str:
        """Render a progress bar for *rate* pkts/s vs _TARGET_RATE."""
        fraction = min(rate / cls._TARGET_RATE, 1.0)
        filled = round(fraction * width)
        colour = "█" if fraction >= 0.9 else ("▓" if fraction >= 0.5 else "░")
        return colour * filled + "░" * (width - filled)

    def render(self) -> str:
        output = "📊 STATISTICS\n"
        output += "─" * 50 + "\n"

        app = self.app
        if hasattr(app, "udp_client"):
            client = app.udp_client
            status = "Connected" if client.connected else "Disconnected"
            icon = "🟢" if client.connected else "🔴"
            output += f"Status:        {icon} {status}\n"
            output += f"Packets Sent:  {client.packets_sent}\n"

            # ── Ping sparkline ────────────────────────────────────────────
            history = client.get_ping_history()
            spark = self._sparkline(history)
            avg_ping = client.get_avg_ping_ms()
            avg_str = f"{avg_ping:.1f} ms avg" if avg_ping is not None else "--- ms avg"
            output += f"Ping  {spark}  {avg_str}\n"

            # ── Packet rate gauge ───────────────────────────────────────
            rate = client.get_packet_rate()
            bar = self._rate_bar(rate)
            output += f"Rate  [{bar}]  {rate:4.1f} / {self._TARGET_RATE:.0f} pkts/s\n"
        else:
            output += "Status:        🔴 Disconnected\n"
            output += "Packets Sent:  0\n"
            spark = self._sparkline([])
            output += f"Ping  {spark}  --- ms avg\n"
            output += f"Rate  [{self._rate_bar(0)}]   0.0 / 25 pkts/s\n"

        return output
