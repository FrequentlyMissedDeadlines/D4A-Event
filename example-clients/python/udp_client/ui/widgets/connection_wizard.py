"""Connection wizard modal screen — scans the LAN for K10 Bots."""

import asyncio
import os
import socket

from textual.app import ComposeResult
from textual.containers import Horizontal, Vertical
from textual.screen import ModalScreen
from textual.widgets import Button, Label, ListItem, ListView

from config import DEFAULT_SERVER_PORT


class ConnectionWizardScreen(ModalScreen):
    """First-run modal: scans the LAN for K10 Bots and pre-fills the server IP."""

    _PORT = DEFAULT_SERVER_PORT
    _TIMEOUT = 0.30  # seconds per probe
    _BATCH = 40  # concurrent UDP probes

    DEFAULT_CSS = """
    ConnectionWizardScreen {
        align: center middle;
    }
    #wiz-outer {
        width: 66;
        height: auto;
        max-height: 30;
        border: thick $accent;
        background: $surface;
        padding: 1 2;
    }
    #wiz-title {
        text-align: center;
        text-style: bold;
        margin-bottom: 1;
    }
    #wiz-status { margin-bottom: 1; }
    #wiz-list {
        height: auto;
        max-height: 10;
        border: solid $accent;
        margin-bottom: 1;
    }
    #wiz-buttons { height: 3; }
    """

    def compose(self) -> ComposeResult:
        with Vertical(id="wiz-outer"):
            yield Label("🔍  K10 Bot — Connection Wizard", id="wiz-title")
            yield Label("Detecting local network…", id="wiz-status")
            yield ListView(id="wiz-list")
            with Horizontal(id="wiz-buttons"):
                yield Button("Connect", id="btn-wiz-connect", variant="primary", disabled=True)
                yield Button("Skip", id="btn-wiz-skip", variant="default")

    def on_mount(self) -> None:
        self._found_ips: list[str] = []
        self._selected_addr: str | None = None
        self.run_worker(self._scan_network(), exclusive=True)

    # ------------------------------------------------------------------
    # Network scan
    # ------------------------------------------------------------------

    @staticmethod
    def _local_prefix() -> str | None:
        """Derive the /24 prefix of the default route interface (e.g. '192.168.1')."""
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect(("8.8.8.8", 80))
            local_ip = s.getsockname()[0]
            s.close()
            return ".".join(local_ip.split(".")[:3])
        except Exception:
            return None

    async def _probe(self, ip: str) -> str | None:
        """Return *ip* if it answers a 0x44 PING, else ``None``.

        Uses a proper ``asyncio.DatagramProtocol`` so no threads are blocked.
        """
        loop = asyncio.get_running_loop()
        nonce = os.urandom(4)
        fut: asyncio.Future[bool] = loop.create_future()

        class _PingProto(asyncio.DatagramProtocol):
            def datagram_received(self, data: bytes, addr: tuple[str, int]) -> None:
                if len(data) >= 5 and data[0] == 0x44 and data[1:5] == nonce:
                    if not fut.done():
                        fut.set_result(True)

            def error_received(self, exc: Exception) -> None:
                if not fut.done():
                    fut.set_result(False)

        try:
            transport, _ = await loop.create_datagram_endpoint(
                _PingProto, remote_addr=(ip, self._PORT)
            )
            transport.sendto(b"\x44" + nonce)
            try:
                ok = await asyncio.wait_for(fut, timeout=self._TIMEOUT)
            except TimeoutError:
                ok = False
            transport.close()
            return ip if ok else None
        except Exception:
            return None

    async def _scan_network(self) -> None:
        status_lbl = self.query_one("#wiz-status", Label)
        prefix = self._local_prefix()
        if not prefix:
            status_lbl.update(
                "⚠️  Could not detect local network — enter IP manually after closing."
            )
            return

        status_lbl.update(f"Scanning {prefix}.0/24 …")
        hosts = [f"{prefix}.{i}" for i in range(1, 255)]

        found: list[str] = []
        for i in range(0, len(hosts), self._BATCH):
            batch = hosts[i : i + self._BATCH]
            results = await asyncio.gather(*[self._probe(h) for h in batch], return_exceptions=True)
            found.extend(r for r in results if isinstance(r, str))
            # Update progress
            status_lbl.update(f"Scanning {prefix}.0/24 … ({i + self._BATCH}/{len(hosts)})")

        self._found_ips = found
        lv = self.query_one("#wiz-list", ListView)
        if found:
            await lv.mount(*[ListItem(Label(f"📡  {ip}:{self._PORT}")) for ip in found])
            status_lbl.update(f"Found {len(found)} device(s) — select one and click Connect.")
            self.query_one("#btn-wiz-connect", Button).disabled = False
        else:
            status_lbl.update(
                "No devices found on this subnet. Enter the IP manually after closing."
            )

    # ------------------------------------------------------------------
    # UI events
    # ------------------------------------------------------------------

    def on_list_view_selected(self, event: ListView.Selected) -> None:
        idx = self.query_one("#wiz-list", ListView).index
        if idx is not None and 0 <= idx < len(self._found_ips):
            self._selected_addr = f"{self._found_ips[idx]}:{self._PORT}"
            self.query_one("#btn-wiz-connect", Button).disabled = False

    def on_button_pressed(self, event: Button.Pressed) -> None:
        if event.button.id == "btn-wiz-connect":
            self.dismiss(self._selected_addr)
        elif event.button.id == "btn-wiz-skip":
            self.dismiss(None)
