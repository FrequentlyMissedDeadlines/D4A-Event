"""
Async UDP client for communicating with K10 Bot.
"""

import asyncio
import json
import os
import socket
import time
from collections import deque
from collections.abc import Callable
from datetime import datetime

from config import (
    BOT_TOKEN,
    DEFAULT_SERVER_IP,
    DEFAULT_SERVER_PORT,
    MAX_RECONNECT_ATTEMPTS,
    NETWORK_TIMEOUT,
    PING_HISTORY_SIZE,
    PING_TIMEOUT,
    RECONNECT_DELAY,
)


class UDPClient:
    """Async UDP client with auto-reconnect and statistics tracking."""

    def __init__(
        self,
        server_ip: str = DEFAULT_SERVER_IP,
        server_port: int = DEFAULT_SERVER_PORT,
        token: str = BOT_TOKEN,
    ):
        self.server_ip = server_ip
        self.server_port = server_port
        self.token = token
        self.socket: socket.socket | None = None
        self.connected = False
        self.packets_sent = 0
        self.last_send_time: datetime | None = None
        self.reconnect_attempts = 0

        # Ping RTT rolling window (last PING_HISTORY_SIZE samples)
        self._ping_history: deque = deque(maxlen=PING_HISTORY_SIZE)

        # Send-timestamp ring for packet-rate calculation (last 50 sends)
        self._send_timestamps: deque = deque(maxlen=50)

        # Callbacks
        self.on_connect: Callable | None = None
        self.on_disconnect: Callable | None = None
        self.on_error: Callable[[str], None] | None = None
        self.on_ping: Callable[[float], None] | None = None

    async def connect(self) -> bool:
        """Register as master then verify with a 0x44 ping round-trip.

        Protocol handshake:
          1. Send 0x41 + token  (MASTER_REGISTER) — bot replies echo + 0x00 on success.
          2. Send 0x44 + 4-byte nonce (PING)      — bot echoes the 5 bytes back.

        Returns False immediately if either step fails or times out.
        """
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.socket.settimeout(NETWORK_TIMEOUT)
            loop = asyncio.get_running_loop()
            addr = (self.server_ip, self.server_port)

            # Step 1 — MASTER_REGISTER (0x41 + token)
            reg_pkt = b"\x41" + self.token.encode("ascii")
            self.socket.sendto(reg_pkt, addr)
            reg_reply = await asyncio.wait_for(
                loop.run_in_executor(None, lambda: self.socket.recv(32)),
                timeout=NETWORK_TIMEOUT,
            )
            # Reply format: echo of sent bytes + status byte (0x00 = SUCCESS)
            if not (reg_reply and reg_reply[0] == 0x41 and reg_reply[-1] == 0x00):
                status = reg_reply[-1] if reg_reply else 0xFF
                status_map = {0x01: "IGNORED", 0x02: "DENIED", 0x03: "ERROR"}
                status_name = status_map.get(status, f"0x{status:02X}")
                raise ConnectionError(f"MASTER_REGISTER failed: {status_name}")

            # Step 2 — PING (0x44 + 4-byte nonce)
            ping_id = os.urandom(4)
            t_start = time.monotonic()
            self.socket.sendto(b"\x44" + ping_id, addr)
            ping_reply = await asyncio.wait_for(
                loop.run_in_executor(None, lambda: self.socket.recv(16)),
                timeout=NETWORK_TIMEOUT,
            )
            if not (len(ping_reply) >= 5 and ping_reply[0] == 0x44 and ping_reply[1:5] == ping_id):
                raise ConnectionError("Unexpected reply during connect ping")

            rtt_ms = (time.monotonic() - t_start) * 1000.0
            self._ping_history.append(rtt_ms)

            self.connected = True
            self.reconnect_attempts = 0
            if self.on_connect:
                self.on_connect()
            return True

        except TimeoutError:
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
        """Send 0x42 MASTER_UNREGISTER then close the socket."""
        if self.socket and self.connected:
            try:
                await self._async_sendto(b"\x42")
            except Exception:
                pass
        if self.socket:
            try:
                self.socket.close()
            except Exception:
                pass
        self.connected = False
        if self.on_disconnect:
            self.on_disconnect()

    async def _async_sendto(self, data: bytes) -> None:
        """Send *data* via the UDP socket without blocking the event loop."""
        loop = asyncio.get_running_loop()
        addr = (self.server_ip, self.server_port)
        await loop.run_in_executor(None, self.socket.sendto, data, addr)

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
            await self._async_sendto(b"\x43")
            self.packets_sent += 1
            self.last_send_time = datetime.now()
            self._send_timestamps.append(time.monotonic())
            return True
        except Exception:
            return False

    async def ping(self) -> float | None:
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

        except TimeoutError:
            if self.socket:
                self.socket.settimeout(NETWORK_TIMEOUT)
            self.connected = False
            await self.on_network_error("Ping timeout — attempting reconnect…")
            await self.reconnect()
            return None
        except Exception as e:
            if self.socket:
                try:
                    self.socket.settimeout(NETWORK_TIMEOUT)
                except Exception:
                    pass
            self.connected = False
            await self.on_network_error(f"Ping failed: {e} — attempting reconnect…")
            await self.reconnect()
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
            await self._async_sendto(data)
            self.packets_sent += 1
            self.last_send_time = datetime.now()
            self._send_timestamps.append(time.monotonic())
            return True
        except TimeoutError:
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
            await self._async_sendto(message)
            self.packets_sent += 1
            self.last_send_time = datetime.now()
            self._send_timestamps.append(time.monotonic())
            return True
        except TimeoutError:
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

    def get_avg_ping_ms(self) -> float | None:
        """Return the average RTT over the last PING_HISTORY_SIZE pings in ms."""
        if not self._ping_history:
            return None
        return sum(self._ping_history) / len(self._ping_history)

    def get_ping_history(self) -> list[float]:
        """Return a copy of the RTT sample list (oldest first)."""
        return list(self._ping_history)

    def get_packet_rate(self) -> float:
        """Return packets/second sent in the last 1 second rolling window."""
        now = time.monotonic()
        cutoff = now - 1.0
        return sum(1 for t in self._send_timestamps if t >= cutoff)

    def get_latency_ms(self) -> float | None:
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
