"""
Application state management.
"""

import time
from dataclasses import dataclass, field
from typing import Dict, Optional

from udp_client.network.udp_client import UDPClient


@dataclass
class AppState:
    """Central application state."""

    # Network state
    server_ip: str = "192.168.1.100"
    server_port: int = 24642
    connected: bool = False
    connection_error: Optional[str] = None
    reconnect_in_progress: bool = False

    # Statistics
    packets_sent: int = 0
    commands_per_sec: float = 0.0
    latency_ms: Optional[float] = None
    ping_avg_ms: Optional[float] = None
    last_update_time: float = field(default_factory=time.time)
    update_count_this_second: int = 0

    # Device states
    keyboard_connected: bool = True
    joystick_count: int = 0
    joystick_names: Dict[int, str] = field(default_factory=dict)

    # UI state
    show_help: bool = False
    device_manager_open: bool = False

    def update_stats(self, udp_client: UDPClient) -> None:
        """Update statistics from UDP client."""
        self.connected = udp_client.connected
        self.packets_sent = udp_client.packets_sent
        self.ping_avg_ms = udp_client.get_avg_ping_ms()
        self.latency_ms = self.ping_avg_ms  # backward compat

        # Calculate commands per second
        current_time = time.time()
        time_delta = current_time - self.last_update_time

        if time_delta >= 1.0:
            self.commands_per_sec = self.update_count_this_second / time_delta
            self.update_count_this_second = 0
            self.last_update_time = current_time
        else:
            self.update_count_this_second += 1

    def set_connection_error(self, error: Optional[str]) -> None:
        """Set connection error message."""
        self.connection_error = error

    def get_status_color(self) -> str:
        """Get color code for connection status."""
        if self.connected:
            return "green"
        elif self.reconnect_in_progress:
            return "yellow"
        else:
            return "red"

    def get_status_icon(self) -> str:
        """Get icon for connection status."""
        if self.connected:
            return "🟢"
        elif self.reconnect_in_progress:
            return "🟡"
        else:
            return "🔴"
