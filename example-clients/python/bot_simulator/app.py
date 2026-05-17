"""
Bot Simulator — Main Application Window (v2)
=============================================

Dark-themed Tkinter application — all 11 UI improvements implemented.

Improvements included
---------------------
#1  Editable servo labels          — double-click any servo widget (servo_widget.py)
#2  Pulse animation on change      — flash() on servo_widget (servo_widget.py)
#3  Motor speed panel              — VU-meter bars for DC motors 1-4
#4  Hex byte inspector             — click a log line to inspect its raw bytes
#5  Resizable pane divider         — ttk.PanedWindow between servo/motor and log
#6  Log filter toggles             — IN / OUT / SYS toggle buttons hide categories
#7  Auto-scroll pause on hover     — scroll pauses while mouse is inside the log
#8  Connection indicator LED       — colour-coded oval in the toolbar
#9  Heartbeat watchdog bar         — draining bar shows time since last heartbeat
#10 Manual packet injection        — enter hex bytes and send as a simulated packet
#11 Session log to file            — checkbox tees all messages to a .log file
"""

from __future__ import annotations

import os
import queue
import time
import tkinter as tk
from datetime import datetime
from tkinter import messagebox, ttk
from typing import Optional

from .protocol import BotSimulatorProtocol, ServoState, ServoType
from .servo_widget import ServoWidget


# ---------------------------------------------------------------------------
# Colour palette
# ---------------------------------------------------------------------------

_BG        = "#1e1e1e"
_BG_PANEL  = "#161616"
_BG_TB     = "#252525"
_BG_ENTRY  = "#2c2c2c"
_FG        = "#e0e0e0"
_FG_DIM    = "#888888"
_FG_IN     = "#5bc0eb"
_FG_OUT    = "#90be6d"
_FG_WARN   = "#f4a261"
_FG_TS     = "#4a4a4a"
_FG_ADDR   = "#777777"
_FG_MASTER = "#90be6d"
_FG_TOKEN  = "#e9c46a"
_BTN_START = "#2d6a4f"
_BTN_STOP  = "#6b2d2d"


# ---------------------------------------------------------------------------
# #3 — Motor speed bar widget
# ---------------------------------------------------------------------------

class _MotorBar(tk.Canvas):
    """Horizontal VU-meter bar for a single DC motor channel."""

    W = 242   # width matches 2×servo widgets + padding
    H = 14

    def __init__(self, parent: tk.Widget, **kwargs) -> None:
        super().__init__(
            parent,
            width=self.W, height=self.H,
            bg="#111111",
            highlightthickness=1,
            highlightbackground="#2a2a2a",
            **kwargs,
        )
        cx = self.W // 2
        self.create_line(cx, 0, cx, self.H, fill="#2e2e2e", width=1)
        self._bar   = self.create_rectangle(cx, 2, cx, self.H - 2,
                                            fill="#2d6a4f", outline="")
        self._label = self.create_text(cx, self.H // 2, text="0",
                                       fill="#555555", font=("monospace", 7))

    def set_speed(self, speed: int) -> None:
        cx   = self.W // 2
        half = cx - 4
        off  = int((speed / 100.0) * half)
        x1, x2 = (cx + off, cx) if off < 0 else (cx, cx + off)
        col  = "#4ade80" if speed > 0 else ("#f87171" if speed < 0 else "#2d6a4f")
        self.coords(self._bar, x1, 2, x2, self.H - 2)
        self.itemconfig(self._bar, fill=col)
        text      = f"{speed:+d}%" if speed != 0 else "STOP"
        text_col  = col if speed != 0 else "#444444"
        self.itemconfig(self._label, text=text, fill=text_col)


# ---------------------------------------------------------------------------
# Main application
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Inject hex — example packets shown in the combobox dropdown
# Format: ("Display label", "hex bytes")
# ---------------------------------------------------------------------------

_INJECT_EXAMPLES: list[tuple[str, str]] = [
    # ── Session management ────────────────────────────────────────────────
    ("── Session ──────────────────────",          ""),
    ("Register master (token: sim01)",             "41 73 69 6D 30 31"),
    ("Unregister master",                          "42"),
    ("Ping",                                       "44 DE AD BE EF"),
    ("Get bot name",                               "45"),
    # ── Servo type setup ─────────────────────────────────────────────────
    ("── Servo type ───────────────────",          ""),
    ("CH0 → 180° rotational",                      "22 01 00"),
    ("CH1 → 180° rotational",                      "22 02 00"),
    ("CH0+CH1 → 180° rotational",                  "22 03 00"),
    ("CH0 → 270° rotational",                      "22 01 01"),
    ("CH1 → 270° rotational",                      "22 02 01"),
    ("CH0 → continuous",                           "22 01 02"),
    ("CH1 → continuous",                           "22 02 02"),
    ("CH0+CH1 → continuous",                       "22 03 02"),
    ("All 6 channels → 180° rotational",           "22 3F 00"),
    ("All 6 channels → continuous",               "22 3F 02"),
    # ── Servo angle (rotational) ──────────────────────────────────────────
    ("── Servo angle ──────────────────",          ""),
    ("CH0 → 0°",                                   "24 01 00 00"),
    ("CH0 → 45°",                                  "24 01 00 2D"),
    ("CH0 → 90°",                                  "24 01 00 5A"),
    ("CH0 → 135°",                                 "24 01 00 87"),
    ("CH0 → 180°",                                 "24 01 00 B4"),
    ("CH1 → 90°",                                  "24 02 00 5A"),
    ("CH0+CH1 → 90°",                              "24 03 00 5A"),
    ("All 6 ch → 90°",                             "24 3F 00 5A"),
    # ── Servo speed (continuous) ─────────────────────────────────────────
    ("── Servo speed (continuous) ─────",          ""),
    ("CH0 → +100 % (full forward)",                "23 01 64"),
    ("CH0 → +50 %",                                "23 01 32"),
    ("CH0 → 0 % (stop)",                           "23 01 00"),
    ("CH0 → -50 %",                                "23 01 CE"),
    ("CH0 → -100 % (full reverse)",                "23 01 9C"),
    ("CH1 → +75 %",                                "23 02 4B"),
    ("All 6 ch → stop",                            "23 3F 00"),
    # ── DC motors ────────────────────────────────────────────────────────
    ("── DC motors ────────────────────",          ""),
    ("M1 → +100 % (full forward)",                 "21 01 64"),
    ("M1 → +50 %",                                 "21 01 32"),
    ("M1 → 0 % (stop)",                            "21 01 00"),
    ("M1 → -50 %",                                 "21 01 CE"),
    ("M1+M2 → +80 %",                              "21 03 50"),
    ("M1+M2 forward, M3+M4 reverse (tank turn)",   "21 03 50  21 0C B0"),
    ("Stop all motors",                            "28"),
]


class BotSimulatorApp(tk.Tk):
    """Top-level Tk window — K10 Bot Simulator with all UI improvements."""

    MAX_LOG_LINES = 400
    HB_TIMEOUT_MS = 200   # #9 display-only watchdog window (ms)

    def __init__(self) -> None:
        super().__init__()

        self.title("K10 Bot Simulator")
        self.configure(bg=_BG)
        self.minsize(820, 520)
        self.resizable(True, True)

        # -- Core state -------------------------------------------------------
        self._connected_servos = tk.IntVar(value=2)
        self._port_var         = tk.StringVar(value="24642")
        self._master_var       = tk.StringVar(value="\u2014")
        self._status_var       = tk.StringVar(value="Stopped")

        # -- Backend ----------------------------------------------------------
        self._protocol: Optional[BotSimulatorProtocol] = None
        self._event_q:  queue.Queue = queue.Queue()
        self._is_running = False

        # -- #2 flash: snapshot for change detection --------------------------
        self._prev_servo_snapshot: dict[int, tuple] = {}

        # -- #4 hex inspector -------------------------------------------------
        self._hex_counter = 0
        self._hex_data: dict[str, bytes] = {}
        self._hex_inspect_var = tk.StringVar(value="  Click a message to inspect bytes")

        # -- #6 log filters ---------------------------------------------------
        self._log_filter_in  = tk.BooleanVar(value=True)
        self._log_filter_out = tk.BooleanVar(value=True)
        self._log_filter_sys = tk.BooleanVar(value=True)

        # -- #7 auto-scroll ---------------------------------------------------
        self._log_autoscroll = True

        # -- #10 inject -------------------------------------------------------
        self._inject_var = tk.StringVar()

        # -- #11 log to file --------------------------------------------------
        self._log_to_file = tk.BooleanVar(value=False)
        self._log_file    = None

        # -- Build UI ---------------------------------------------------------
        self._build_toolbar()
        self._build_main_area()
        self._build_status_bar()
        self._refresh_servos()
        self.after(50,  self._poll_events)
        self.after(30,  self._update_watchdog)
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    # =========================================================================
    # UI construction
    # =========================================================================

    def _build_toolbar(self) -> None:
        tb = tk.Frame(self, bg=_BG_TB, pady=6)
        tb.pack(fill="x", side="top")

        # -- #8 LED indicator -------------------------------------------------
        self._led_canvas = tk.Canvas(
            tb, width=14, height=14,
            bg=_BG_TB, highlightthickness=0,
        )
        self._led_canvas.pack(side="left", padx=(8, 4))
        self._led_oval = self._led_canvas.create_oval(2, 2, 12, 12,
                                                       fill="#444444", outline="")

        tk.Label(
            tb, text="\U0001f916  Bot Simulator",
            bg=_BG_TB, fg=_FG, font=("sans-serif", 13, "bold"),
        ).pack(side="left", padx=4)

        # -- Right-hand controls ----------------------------------------------
        right = tk.Frame(tb, bg=_BG_TB)
        right.pack(side="right", padx=10)

        # -- #9 Heartbeat watchdog bar ----------------------------------------
        tk.Label(right, text="HB:", bg=_BG_TB, fg=_FG_DIM,
                 font=("monospace", 8)).pack(side="left")
        self._hb_canvas = tk.Canvas(
            right, width=60, height=8, bg="#111111",
            highlightthickness=1, highlightbackground="#333333",
        )
        self._hb_canvas.pack(side="left", padx=(2, 14))
        self._hb_bar = self._hb_canvas.create_rectangle(0, 0, 0, 8,
                                                          fill="#2d6a4f", outline="")

        # Servo count
        tk.Label(right, text="Servos:", bg=_BG_TB, fg=_FG_DIM,
                 font=("monospace", 10)).pack(side="left")
        tk.Spinbox(
            right, from_=0, to=6,
            textvariable=self._connected_servos,
            width=2,
            bg=_BG_ENTRY, fg=_FG, buttonbackground="#3a3a3a",
            relief="flat", font=("monospace", 10),
            command=self._on_servo_count_changed,
        ).pack(side="left", padx=(4, 12))

        # Port
        tk.Label(right, text="Port:", bg=_BG_TB, fg=_FG_DIM,
                 font=("monospace", 10)).pack(side="left")
        tk.Entry(
            right, textvariable=self._port_var, width=7,
            bg=_BG_ENTRY, fg=_FG, insertbackground=_FG,
            relief="flat", font=("monospace", 10),
        ).pack(side="left", padx=(4, 12))

        # Start / Stop
        self._start_btn = tk.Button(
            right, text="\u25b6  Start",
            bg=_BTN_START, fg="#ffffff", activebackground="#3a8a65",
            relief="flat", font=("monospace", 10, "bold"),
            padx=10, pady=2, cursor="hand2",
            command=self._toggle_server,
        )
        self._start_btn.pack(side="left")

    # -- #5 PanedWindow main area ----------------------------------------

    def _build_main_area(self) -> None:
        self._paned = ttk.PanedWindow(self, orient="horizontal")
        self._paned.pack(fill="both", expand=True, padx=8, pady=(4, 4))

        left  = tk.Frame(self._paned, bg=_BG)
        right = tk.Frame(self._paned, bg=_BG)
        self._paned.add(left,  weight=0)
        self._paned.add(right, weight=1)

        self._build_servo_panel(left)
        self._build_motor_panel(left)   # #3
        self._build_log_panel(right)

    def _build_servo_panel(self, parent: tk.Widget) -> None:
        outer = tk.Frame(parent, bg=_BG)
        outer.pack(fill="x", pady=(0, 0))

        tk.Label(outer, text="Servo Channels",
                 bg=_BG, fg=_FG_DIM, font=("monospace", 9), anchor="w",
                 ).pack(fill="x", pady=(0, 4))

        grid = tk.Frame(outer, bg=_BG)
        grid.pack()

        self._servo_widgets: list[ServoWidget] = []
        for i in range(6):
            row, col = divmod(i, 2)
            w = ServoWidget(grid, channel_id=i,
                            on_type_change=self._on_servo_type_changed)
            w.grid(row=row, column=col, padx=3, pady=3)
            self._servo_widgets.append(w)

    # -- #3 Motor panel --------------------------------------------------

    def _build_motor_panel(self, parent: tk.Widget) -> None:
        outer = tk.Frame(parent, bg=_BG)
        outer.pack(fill="x", pady=(8, 0))

        tk.Label(outer, text="DC Motors",
                 bg=_BG, fg=_FG_DIM, font=("monospace", 9), anchor="w",
                 ).pack(fill="x", pady=(0, 4))

        self._motor_bars: list[_MotorBar] = []
        for i in range(4):
            row_frame = tk.Frame(outer, bg=_BG)
            row_frame.pack(fill="x", pady=1)
            tk.Label(row_frame, text=f"M{i + 1}", bg=_BG, fg=_FG_DIM,
                     font=("monospace", 8), width=3).pack(side="left")
            bar = _MotorBar(row_frame)
            bar.pack(side="left")
            self._motor_bars.append(bar)

    # -- Log panel with #4 #6 #7 #10 #11 ---------------------------------

    def _build_log_panel(self, parent: tk.Widget) -> None:
        outer = tk.Frame(parent, bg=_BG)
        outer.pack(fill="both", expand=True)
        outer.columnconfigure(0, weight=1)
        outer.rowconfigure(2, weight=1)   # log text expands

        # Row 0 — header
        hdr = tk.Frame(outer, bg=_BG)
        hdr.grid(row=0, column=0, columnspan=2, sticky="ew", pady=(0, 2))

        tk.Label(hdr, text="Message Log",
                 bg=_BG, fg=_FG_DIM, font=("monospace", 9), anchor="w",
                 ).pack(side="left")

        # #11 log to file checkbox
        tk.Checkbutton(
            hdr, text="\U0001f4c4", variable=self._log_to_file,
            command=self._toggle_log_file,
            bg=_BG, fg=_FG_DIM, selectcolor="#1a2a1a",
            activebackground=_BG, activeforeground=_FG,
            relief="flat", font=("monospace", 10), cursor="hand2",
        ).pack(side="right", padx=(4, 0))

        tk.Button(hdr, text="Clear",
                  bg="#2c2c2c", fg=_FG_DIM, activebackground="#3a3a3a",
                  relief="flat", font=("monospace", 9), padx=6, pady=0,
                  cursor="hand2", command=self._clear_log,
                  ).pack(side="right")

        # Row 1 — #6 filter toggles
        fbar = tk.Frame(outer, bg=_BG)
        fbar.grid(row=1, column=0, columnspan=2, sticky="w", pady=(0, 2))

        for label, var, tag, fg_on in [
            ("\u2190 IN",    self._log_filter_in,  "line_in",  _FG_IN),
            ("\u2192 OUT",   self._log_filter_out, "line_out", _FG_OUT),
            ("\u00b7\u00b7\u00b7 SYS", self._log_filter_sys, "line_sys", _FG_WARN),
        ]:
            tk.Checkbutton(
                fbar, text=label, variable=var,
                indicatoron=0,
                command=lambda t=tag, v=var: self._on_filter_toggle(t, v),
                bg="#252525", fg=fg_on,
                selectcolor="#1a2a1a",
                activebackground="#2a2a2a", activeforeground=fg_on,
                relief="flat", padx=6, pady=2,
                font=("monospace", 8), cursor="hand2",
            ).pack(side="left", padx=2)

        # Row 2 — log text + scrollbar_y
        self._log_text = tk.Text(
            outer,
            bg=_BG_PANEL, fg=_FG,
            font=("monospace", 9),
            state="disabled",
            wrap="none",
            relief="flat",
            cursor="arrow",
            selectbackground="#2d4a6d",
        )
        self._log_text.grid(row=2, column=0, sticky="nsew")

        scroll_y = ttk.Scrollbar(outer, orient="vertical",   command=self._log_text.yview)
        scroll_x = ttk.Scrollbar(outer, orient="horizontal", command=self._log_text.xview)
        scroll_y.grid(row=2, column=1, sticky="ns")
        scroll_x.grid(row=3, column=0, sticky="ew")
        self._log_text.configure(
            yscrollcommand=scroll_y.set,
            xscrollcommand=scroll_x.set,
        )

        # Text tags — colours
        self._log_text.tag_configure("ts",      foreground=_FG_TS)
        self._log_text.tag_configure("in",      foreground=_FG_IN)
        self._log_text.tag_configure("out",     foreground=_FG_OUT)
        self._log_text.tag_configure("warn",    foreground=_FG_WARN)
        self._log_text.tag_configure("addr",    foreground=_FG_ADDR)
        self._log_text.tag_configure("desc",    foreground=_FG)
        # Line-level filter tags (#6) — elide=False initially
        self._log_text.tag_configure("line_in",  elide=False)
        self._log_text.tag_configure("line_out", elide=False)
        self._log_text.tag_configure("line_sys", elide=False)

        # #7 — hover to pause auto-scroll
        self._log_text.bind("<Enter>", lambda _e: setattr(self, "_log_autoscroll", False))
        self._log_text.bind("<Leave>", lambda _e: setattr(self, "_log_autoscroll", True))

        # #4 — click to inspect bytes
        self._log_text.bind("<Button-1>", self._on_log_click)

        # Row 4 — #4 hex inspector label
        tk.Label(
            outer, textvariable=self._hex_inspect_var,
            bg="#0e1a0e", fg="#4a8a4a",
            font=("monospace", 8), anchor="w", padx=6, pady=2,
        ).grid(row=4, column=0, columnspan=2, sticky="ew")

        # Row 5 — #10 inject bar with example dropdown
        inject_frame = tk.Frame(outer, bg=_BG_TB)
        inject_frame.grid(row=5, column=0, columnspan=2, sticky="ew")

        tk.Label(inject_frame, text=" Inject hex:", bg=_BG_TB, fg=_FG_DIM,
                 font=("monospace", 9)).pack(side="left")

        tk.Button(
            inject_frame, text="Send",
            bg="#2d4a6d", fg=_FG, activebackground="#3a5a8a",
            relief="flat", font=("monospace", 9), padx=8, pady=2,
            cursor="hand2", command=self._do_inject,
        ).pack(side="right", padx=(0, 4), pady=2)

        # Build display-label → hex mapping (skip separator entries)
        self._inject_examples: dict[str, str] = {
            label: hex_val
            for label, hex_val in _INJECT_EXAMPLES
            if hex_val  # exclude section headers
        }
        display_values = [label for label, _ in _INJECT_EXAMPLES]

        style = ttk.Style()
        style.theme_use("default")
        style.configure(
            "Inject.TCombobox",
            fieldbackground=_BG_ENTRY,
            background=_BG_TB,
            foreground=_FG,
            selectbackground=_BG_ENTRY,
            selectforeground=_FG,
            arrowcolor=_FG_DIM,
        )

        self._inject_combo = ttk.Combobox(
            inject_frame,
            textvariable=self._inject_var,
            values=display_values,
            style="Inject.TCombobox",
            font=("monospace", 9),
        )
        self._inject_combo.pack(side="left", fill="x", expand=True, padx=4, pady=2)
        self._inject_combo.bind("<<ComboboxSelected>>", self._on_inject_example_selected)
        # Allow Enter key to send
        self._inject_combo.bind("<Return>", lambda _e: self._do_inject())

    def _build_status_bar(self) -> None:
        bar = tk.Frame(self, bg=_BG_TB, pady=3)
        bar.pack(fill="x", side="bottom")

        def _lbl(t: str) -> tk.Label:
            return tk.Label(bar, text=t, bg=_BG_TB, fg=_FG_DIM, font=("monospace", 9))

        def _val(var: tk.Variable, fg: str) -> tk.Label:
            return tk.Label(bar, textvariable=var, bg=_BG_TB, fg=fg,
                            font=("monospace", 9, "bold"))

        _lbl("  Status:").pack(side="left")
        _val(self._status_var, _FG).pack(side="left", padx=(2, 10))
        _lbl("Master:").pack(side="left")
        _val(self._master_var, _FG_MASTER).pack(side="left", padx=(2, 10))
        _lbl("Token:").pack(side="left")
        tk.Label(bar, text=BotSimulatorProtocol.TOKEN,
                 bg=_BG_TB, fg=_FG_TOKEN, font=("monospace", 9, "bold"),
                 ).pack(side="left", padx=(2, 10))

        right = tk.Frame(bar, bg=_BG_TB)
        right.pack(side="right", padx=8)
        for sq, col, label in [
            ("\u25a0", "#909090", " rotational  "),
            ("\u25a0", "#2d8a4e", " continuous"),
        ]:
            tk.Label(right, text=sq,    bg=_BG_TB, fg=col,     font=("monospace", 10)).pack(side="left")
            tk.Label(right, text=label, bg=_BG_TB, fg=_FG_DIM, font=("monospace", 8)).pack(side="left")

    # =========================================================================
    # Server lifecycle
    # =========================================================================

    def _toggle_server(self) -> None:
        if self._is_running:
            self._stop_server()
        else:
            self._start_server()

    def _start_server(self) -> None:
        try:
            port = int(self._port_var.get())
            if not 1 <= port <= 65535:
                raise ValueError
        except ValueError:
            messagebox.showerror("Invalid Port", "Port must be 1–65535.")
            return

        self._protocol = BotSimulatorProtocol(port=port, on_event=self._on_backend_event)
        try:
            self._protocol.start()
        except OSError as exc:
            messagebox.showerror("Bind Error", f"Cannot listen on UDP :{port}\n{exc}")
            self._protocol = None
            return

        self._is_running = True
        self._start_btn.configure(text="\u25a0  Stop", bg=_BTN_STOP, activebackground="#8a3d3d")
        self._status_var.set(f"Listening on :{port}")
        self._update_led("listening")   # #8
        self._system_log(
            f"Server started \u2014 UDP :{port}  (token: {BotSimulatorProtocol.TOKEN})"
        )

    def _stop_server(self) -> None:
        if self._protocol:
            self._protocol.stop()
            self._protocol = None
        self._is_running = False
        self._prev_servo_snapshot.clear()
        self._start_btn.configure(text="\u25b6  Start", bg=_BTN_START, activebackground="#3a8a65")
        self._status_var.set("Stopped")
        self._master_var.set("\u2014")
        self._update_led("stopped")   # #8
        self._refresh_servos()
        self._refresh_motors()
        self._system_log("Server stopped")

    # =========================================================================
    # #8 LED indicator
    # =========================================================================

    def _update_led(self, state: str) -> None:
        colours = {
            "stopped":   "#444444",
            "listening": "#e9c46a",
            "connected": "#4ade80",
            "timeout":   "#f4a261",
        }
        self._led_canvas.itemconfig(self._led_oval, fill=colours.get(state, "#444444"))

    # =========================================================================
    # #9 Heartbeat watchdog bar
    # =========================================================================

    def _update_watchdog(self) -> None:
        if self._protocol and self._is_running:
            if self._protocol.master_ip:
                elapsed_ms  = (time.monotonic() - self._protocol.last_heartbeat) * 1000
                fill_ratio  = max(0.0, 1.0 - elapsed_ms / self.HB_TIMEOUT_MS)
                fill_w      = int(60 * fill_ratio)
                timed_out   = elapsed_ms >= self.HB_TIMEOUT_MS
                bar_col     = "#f87171" if timed_out else "#4ade80"
                led_state   = "timeout" if timed_out else "connected"
                self._hb_canvas.coords(self._hb_bar, 0, 0, fill_w, 8)
                self._hb_canvas.itemconfig(self._hb_bar, fill=bar_col)
                self._update_led(led_state)
            else:
                self._hb_canvas.coords(self._hb_bar, 0, 0, 0, 8)
                self._update_led("listening")
        else:
            self._hb_canvas.coords(self._hb_bar, 0, 0, 0, 8)
            if not self._is_running:
                self._update_led("stopped")
        self.after(30, self._update_watchdog)

    # =========================================================================
    # Event queue — background thread → Tk main thread
    # =========================================================================

    def _on_backend_event(self, event_type: str, data) -> None:
        self._event_q.put((event_type, data))

    def _poll_events(self) -> None:
        try:
            while True:
                event_type, data = self._event_q.get_nowait()
                if event_type == "log":
                    self._append_log(data)
                elif event_type == "state_changed":
                    self._refresh_servos()
                    self._refresh_motors()   # #3
                elif event_type == "master_changed":
                    self._master_var.set(data if data else "\u2014")
                    if data:
                        self._system_log(f"Master registered: {data}")
                    else:
                        self._system_log("Master unregistered")
                elif event_type == "error":
                    self._system_log(f"\u26a0  {data}")
        except queue.Empty:
            pass
        finally:
            self.after(50, self._poll_events)

    # =========================================================================
    # #6 Log filter toggles
    # =========================================================================

    def _on_filter_toggle(self, tag: str, var: tk.BooleanVar) -> None:
        self._log_text.tag_configure(tag, elide=not var.get())

    # =========================================================================
    # #4 Hex byte inspector
    # =========================================================================

    def _on_log_click(self, event: "tk.Event[tk.Text]") -> None:
        try:
            idx  = self._log_text.index(f"@{event.x},{event.y}")
            tags = self._log_text.tag_names(idx)
        except tk.TclError:
            return
        for tag in tags:
            if tag.startswith("msg_"):
                raw = self._hex_data.get(tag, b"")
                if raw:
                    parts  = [f"{b:02X}" for b in raw]
                    groups = ["  ".join(parts[i:i + 8]) for i in range(0, len(parts), 8)]
                    self._hex_inspect_var.set("  " + "  \u00b7  ".join(groups))
                else:
                    self._hex_inspect_var.set("  (no payload bytes)")
                return
        self._hex_inspect_var.set("  Click a message to inspect bytes")

    # =========================================================================
    # #10 Packet injection
    # =========================================================================

    def _on_inject_example_selected(self, _event: "tk.Event") -> None:  # type: ignore[type-arg]
        """When a dropdown item is selected, replace the field with its hex bytes."""
        label = self._inject_var.get()
        hex_val = self._inject_examples.get(label, "")
        if hex_val:
            self._inject_var.set(hex_val)
        self._inject_combo.icursor("end")
        self._inject_combo.selection_clear()

    def _do_inject(self) -> None:
        raw = self._inject_var.get().strip()
        if not raw:
            return
        # Support multiple space-separated packets on one line (e.g. tank turn example)
        # Split on double-space to separate packets, single space separates bytes
        packet_strs = [p.strip() for p in raw.split("  ") if p.strip()]
        packets: list[bytes] = []
        for ps in packet_strs:
            try:
                packets.append(bytes.fromhex(ps.replace(" ", "")))
            except ValueError:
                messagebox.showerror("Invalid hex", f"Cannot parse: '{ps}'")
                return
        if not self._protocol:
            messagebox.showwarning("Not running", "Start the server first.")
            return
        addr = (self._protocol.master_ip or "127.0.0.1", 0)
        for data in packets:
            self._protocol.simulate_receive(data, addr)

    # =========================================================================
    # #11 Log to file
    # =========================================================================

    def _toggle_log_file(self) -> None:
        if self._log_to_file.get():
            filename = f"bot_sim_{datetime.now().strftime('%Y%m%d_%H%M%S')}.log"
            try:
                self._log_file = open(filename, "w", encoding="utf-8")
                self._system_log(f"\U0001f4c4 Logging to '{os.path.abspath(filename)}'")
            except OSError as exc:
                messagebox.showerror("File Error", str(exc))
                self._log_to_file.set(False)
        else:
            if self._log_file:
                self._log_file.close()
                self._log_file = None
            self._system_log("\U0001f4c4 File logging stopped")

    # =========================================================================
    # Log helpers
    # =========================================================================

    def _append_log(self, entry: dict) -> None:
        direction = entry.get("direction", "")
        addr      = entry.get("addr")
        desc      = entry.get("desc", "")
        data      = entry.get("data", b"")
        ts        = datetime.now().strftime("%H:%M:%S.%f")[:-3]

        # Determine line filter tag (#6)
        line_tag = {"in": "line_in", "out": "line_out"}.get(direction, "line_sys")

        # Assign hex data tag (#4)
        msg_tag: Optional[str] = None
        if data:
            msg_tag = f"msg_{self._hex_counter}"
            self._hex_data[msg_tag] = data
            self._hex_counter += 1

        self._log_text.configure(state="normal")

        # Record insert position for line-level tagging
        ins_line = self._log_text.index("end")  # "N.0" before inserting

        self._log_text.insert("end", f"[{ts}] ", "ts")
        if direction == "in":
            self._log_text.insert("end", "\u2190 IN   ", "in")
        elif direction == "out":
            self._log_text.insert("end", "\u2192 OUT  ", "out")
        else:
            self._log_text.insert("end", "  \u00b7\u00b7\u00b7  ", "warn")
        if addr:
            self._log_text.insert("end", f"{addr[0]}:{addr[1]}  ", "addr")
        self._log_text.insert("end", desc, "desc")
        self._log_text.insert("end", "\n")

        # Apply line-level tags for filter (#6) and hex (#4)
        end_pos = self._log_text.index("end")
        self._log_text.tag_add(line_tag, ins_line, end_pos)
        if msg_tag:
            self._log_text.tag_add(msg_tag, ins_line, end_pos)

        # Prune (#7 doesn't interfere with this)
        n_lines = int(self._log_text.index("end - 1c").split(".")[0])
        if n_lines > self.MAX_LOG_LINES:
            self._log_text.delete("1.0", f"{n_lines - self.MAX_LOG_LINES}.0")

        # #7 auto-scroll pause
        if self._log_autoscroll:
            self._log_text.see("end")

        self._log_text.configure(state="disabled")

        # #11 file logging
        if self._log_file and self._log_to_file.get():
            dir_str  = {"in": "<- IN", "out": "-> OUT"}.get(direction, "   ...")
            addr_str = f"{addr[0]}:{addr[1]}  " if addr else ""
            self._log_file.write(f"[{ts}] {dir_str}  {addr_str}{desc}\n")
            self._log_file.flush()

    def _system_log(self, msg: str) -> None:
        self._append_log({"direction": "sys", "addr": None, "data": b"", "desc": msg})

    def _clear_log(self) -> None:
        self._log_text.configure(state="normal")
        self._log_text.delete("1.0", "end")
        self._log_text.configure(state="disabled")
        self._hex_data.clear()
        self._hex_counter = 0
        self._hex_inspect_var.set("  Click a message to inspect bytes")

    # =========================================================================
    # Servo / motor refresh
    # =========================================================================

    def _on_servo_count_changed(self) -> None:
        self._refresh_servos()

    def _on_servo_type_changed(self, channel_id: int, stype: ServoType) -> None:
        """Called when the user picks a servo type via right-click menu."""
        if self._protocol:
            self._protocol.servos[channel_id].servo_type = stype
            self._protocol.servos[channel_id].active     = True
        n = self._connected_servos.get()
        if channel_id >= n:
            self._connected_servos.set(channel_id + 1)
        type_names = {
            ServoType.SERVO_180:  "180° rotational",
            ServoType.SERVO_270:  "270° rotational",
            ServoType.CONTINUOUS: "continuous",
        }
        label = self._servo_widgets[channel_id].get_label()
        self._system_log(
            f"CH{channel_id} ({label}) → type set to {type_names[stype]} (via UI)"
        )
        self._refresh_servos()

    def _refresh_servos(self) -> None:
        n = self._connected_servos.get()
        for i, widget in enumerate(self._servo_widgets):
            if self._protocol:
                state        = self._protocol.servos[i]
                state.active = i < n
                new_snap = (state.angle, state.speed, state.servo_type, state.active)
                old_snap = self._prev_servo_snapshot.get(i)
                widget.update_state(state)
                # #2 — flash only on real change, only for active channels
                if state.active and old_snap is not None and old_snap != new_snap:
                    widget.flash()
                self._prev_servo_snapshot[i] = new_snap
            else:
                dummy = ServoState(channel_id=i, active=(i < n))
                widget.update_state(dummy)

    def _refresh_motors(self) -> None:  # #3
        if self._protocol:
            for i, bar in enumerate(self._motor_bars):
                bar.set_speed(self._protocol.motors[i + 1].speed)
        else:
            for bar in self._motor_bars:
                bar.set_speed(0)

    # =========================================================================
    # Cleanup
    # =========================================================================

    def _on_close(self) -> None:
        self._stop_server()
        if self._log_file:
            self._log_file.close()
        self.destroy()
