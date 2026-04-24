"""
Async UDP client for communicating with K10 Bot.
"""

import asyncio
import json
import os
import socket
import time
from collections import deque
from datetime import datetime
from typing import Callable, Optional

from config import (
    DEFAULT_SERVER_IP,
    DEFAULT_SERVER_PORT,
    NETWORK_TIMEOUT,
    PING_HISTORY_SIZE,
    PING_TIMEOUT,
    RECONNECT_DELAY,
    MAX_RECONNECT_ATTEMPTS,
)


class UDPClient:
    """Async UDP client with auto-reconnect and statistics tracking."""

    def __init__(
        self,
        server_ip: str = DEFAULT_SERVER_IP,
        server_port: int = DEFAULT_SERVER_PORT,
    ):
        self.server_ip = server_ip
        self.server_port = server_port
        self.socket: Optional[socket.socket] = None
        self.connected = False
        self.packets_sent = 0
        self.last_send_time: Optional[datetime] = None
        self.reconnect_attempts = 0

        # Ping RTT rolling window (last PING_HISTORY_SIZE samples)
        self._ping_history: deque = deque(maxlen=PING_HISTORY_SIZE)

        # Callbacks
        self.on_connect: Optional[Callable] = None
        self.on_disconnect: Optional[Callable] = None
        self.on_error: Optional[Callable[[str], None]] = None
        self.on_ping: Optional[Callable[[float], None]] = None

    async def connect(self) -> bool:
        """Connect and verify with a 0x44 ping round-trip.

        Creates a UDP socket then sends a PING (0x44 + 4-byte random nonce)
        and waits for the device echo before setting connected=True.
        Returns False immediately if the device does not respond.
        """
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.socket.settimeout(NETWORK_TIMEOUT)

            # Verify reachability: send PING (0x44 + 4-byte nonce), wait for echo
            ping_id = os.urandom(4)
            t_start = time.monotonic()
            loop = asyncio.get_running_loop()
            self.socket.sendto(b"\x44" + ping_id, (self.server_ip, self.server_port))
            data = await asyncio.wait_for(
                loop.run_in_executor(None, lambda: self.socket.recv(16)),
                timeout=NETWORK_TIMEOUT,
            )
            if not (len(data) >= 5 and data[0] == 0x44 and data[1:5] == ping_id):
                raise ConnectionError("Unexpected reply during connect ping")

            rtt_ms = (time.monotonic() - t_start) * 1000.0
            self._ping_history.append(rtt_ms)

            self.connected = True
            self.reconnect_attempts = 0
            if self.on_connect:
                self.on_connect()
            return True

        except (asyncio.TimeoutError, socket.timeout):
            self.connected = False
            if self.socket:
                self.socket.close()
                self.socket = None
            if self.on_error:
                self.on_error(
                    f"Connection failed: device not responding at "
                    f"{self.server_ip}:{self.server_port}"
                )
            return False
        except Exception as e:
            self.connected = False
            if self.socket:
                self.socket.close()
                self.socket = None
            if self.on_error:
                self.on_error(f"Connection failed: {e}")
            return False

    async def disconnect(self) -> None:
        """Disconnect from the UDP server."""
        if self.socket:
            try:
                self.socket.close()
            except Exception:
                pass
        self.connected = False
        if self.on_disconnect:
            self.on_disconnect()

    async def send_heartbeat(self) -> bool:
        """
        Send a heartbeat packet (0x43) to the K10 Bot.

        Fire-and-forget — no reply is expected.
        Should be called every 40 ms while connected to prevent
        the bot's watchdog from stopping all motors.

        Returns:
            True if sent successfully, False otherwise
        """
        if not self.connected or not self.socket:
            return False
        try:
            self.socket.sendto(b"\x43", (self.server_ip, self.server_port))
            self.packets_sent += 1
            self.last_send_time = datetime.now()
            return True
        except Exception as e:
            await self.on_network_error(f"Heartbeat failed: {str(e)}")
            return False

    async def ping(self) -> Optional[float]:
        """
        Send a 0x44 ping and return the round-trip time in ms.

        Stores the RTT in a rolling window of PING_HISTORY_SIZE samples and
        calls on_ping(avg_ms) after each successful echo.  Sets connected=False
        and calls on_error on timeout so the UI reacts immediately.

        Returns:
            RTT in milliseconds, or None on failure.
        """
        if not self.connected or not self.socket:
            return None
        try:
            ping_id = os.urandom(4)
            t_start = time.monotonic()
            loop = asyncio.get_running_loop()
            # Short socket timeout so the recv thread exits quickly on failure
            self.socket.settimeout(PING_TIMEOUT)
            self.socket.sendto(b"\x44" + ping_id, (self.server_ip, self.server_port))
            data = await loop.run_in_executor(None, lambda: self.socket.recv(16))
            self.socket.settimeout(NETWORK_TIMEOUT)

            if len(data) >= 5 and data[0] == 0x44 and data[1:5] == ping_id:
                rtt_ms = (time.monotonic() - t_start) * 1000.0
                self._ping_history.append(rtt_ms)
                if self.on_ping:
                    self.on_ping(self.get_avg_ping_ms())
                return rtt_ms
            return None

        except socket.timeout:
            if self.socket:
                self.socket.settimeout(NETWORK_TIMEOUT)
            self.connected = False
            await self.on_network_error("Ping timeout — link lost")
            return None
        except Exception as e:
            if self.socket:
                try:
                    self.socket.settimeout(NETWORK_TIMEOUT)
                except Exception:
                    pass
            self.connected = False
            await self.on_network_error(f"Ping failed: {e}")
            return None

    async def send_raw(self, data: bytes) -> bool:
        """
        Send a raw binary packet to the K10 Bot.

        This is the low-level method used by BotConfig.execute() to send
        MotorServoService binary frames (e.g. 0x21 motor speed packets).

        Args:
            data: Raw bytes to transmit (no encoding applied).

        Returns:
            True if sent successfully, False otherwise
        """
        if not self.connected or not self.socket:
            await self.reconnect()
            if not self.connected:
                return False
        try:
            self.socket.sendto(data, (self.server_ip, self.server_port))
            self.packets_sent += 1
            self.last_send_time = datetime.now()
            return True
        except socket.timeout:
            await self.on_network_error("Send timeout")
            return False
        except Exception as e:
            await self.on_network_error(f"Send failed: {str(e)}")
            return False

    async def send_command(self, command: dict) -> bool:
        """
        Send a command to the K10 Bot.

        Args:
            command: Dictionary containing command data

        Returns:
            True if sent successfully, False otherwise
        """
        if not self.connected or not self.socket:
            await self.reconnect()
            if not self.connected:
                return False

        try:
            message = json.dumps(command).encode("utf-8")
            self.socket.sendto(message, (self.server_ip, self.server_port))
            self.packets_sent += 1
            self.last_send_time = datetime.now()
            return True
        except socket.timeout:
            await self.on_network_error("Send timeout")
            return False
        except Exception as e:
            await self.on_network_error(f"Send failed: {str(e)}")
            return False

    async def reconnect(self) -> bool:
        """
        Attempt to reconnect to the server.

        Returns:
            True if reconnection successful, False otherwise
        """
        if MAX_RECONNECT_ATTEMPTS > 0 and self.reconnect_attempts >= MAX_RECONNECT_ATTEMPTS:
            if self.on_error:
                self.on_error("Max reconnection attempts reached")
            return False

        self.reconnect_attempts += 1
        await asyncio.sleep(RECONNECT_DELAY)
        return await self.connect()

    async def on_network_error(self, error_msg: str) -> None:
        """Handle network errors and attempt reconnection."""
        self.connected = False
        if self.on_error:
            self.on_error(error_msg)

    def get_avg_ping_ms(self) -> Optional[float]:
        """Return the average RTT over the last PING_HISTORY_SIZE pings in ms."""
        if not self._ping_history:
            return None
        return sum(self._ping_history) / len(self._ping_history)

    def get_latency_ms(self) -> Optional[float]:
        """Return average ping RTT in ms (kept for backward compatibility)."""
        return self.get_avg_ping_ms()

    def get_stats(self) -> dict:
        """Get client statistics."""
        return {
            "connected": self.connected,
            "packets_sent": self.packets_sent,
            "latency_ms": self.get_latency_ms(),
            "last_send_time": self.last_send_time,
            "server": f"{self.server_ip}:{self.server_port}",
        }
