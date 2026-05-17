"""
Bot Simulator — Servo Channel Widget
======================================

A ``tkinter.Canvas``-based widget that renders the live state of a single
servo channel.

UI improvements
---------------
* **#1** Double-click to set a custom channel label (e.g. "arm", "camera").
* **#2** ``flash()`` briefly highlights the border white whenever the state
         changes, providing a visual cue for which channel was commanded.
"""

from __future__ import annotations

import math
from tkinter import simpledialog
from typing import Callable, Optional

import tkinter as tk

from .protocol import ServoState, ServoType


# ---------------------------------------------------------------------------
# Colour palette  (dark theme)
# ---------------------------------------------------------------------------

_BG             = "#1e1e1e"
_COL_ROTATIONAL = "#909090"
_COL_CONTINUOUS = "#2d8a4e"
_COL_CONT_FWD   = "#4ade80"
_COL_CONT_REV   = "#f87171"
_COL_INACTIVE   = "#2a2a2a"
_COL_ARC_TRACK  = "#383838"
_COL_HUB_BODY   = "#303030"
_COL_NEEDLE     = "#e0e0e0"
_COL_LABEL      = "#888888"
_COL_VALUE      = "#ffffff"
_COL_BORDER_ON  = "#555555"
_COL_BORDER_OFF = "#2e2e2e"
_COL_FLASH      = "#ffffff"


class ServoWidget(tk.Canvas):
    """Canvas widget displaying the state of one servo channel."""

    SIZE      = 120
    _CX       = SIZE / 2
    _CY       = SIZE / 2 + 6
    _R        = 44
    _HUB      = 14
    _NEEDLE_R = 30
    _FLASH_DUR_MS = 160

    def __init__(
        self,
        parent: tk.Widget,
        channel_id: int,
        on_type_change: Optional["Callable[[int, ServoType], None]"] = None,
        **kwargs,
    ) -> None:
        super().__init__(
            parent,
            width=self.SIZE, height=self.SIZE,
            bg=_BG,
            highlightthickness=1,
            highlightbackground=_COL_BORDER_OFF,
            **kwargs,
        )
        self.channel_id      = channel_id
        self._custom_label   = ""
        self._on_type_change = on_type_change  # callback(channel_id, ServoType)
        self._state: Optional[ServoState] = None
        self._draw_inactive()
        self.bind("<Double-Button-1>", self._on_double_click)
        # Right-click → type selector
        self.bind("<Button-3>",        self._on_right_click)
        self.bind("<Button-2>",        self._on_right_click)  # macOS middle-click

    # ------------------------------------------------------------------
    # #1 — label API
    # ------------------------------------------------------------------

    def set_label(self, name: str) -> None:
        self._custom_label = name.strip()
        self.update_state(self._state)

    def get_label(self) -> str:
        return self._custom_label or f"CH{self.channel_id}"

    def _on_double_click(self, _event: "tk.Event[tk.Canvas]") -> None:
        new_name = simpledialog.askstring(
            "Rename Channel",
            f"Label for channel {self.channel_id}:",
            initialvalue=self._custom_label or f"CH{self.channel_id}",
            parent=self,
        )
        if new_name is not None:
            self.set_label(new_name)

    # ------------------------------------------------------------------
    # Type selection via right-click context menu
    # ------------------------------------------------------------------

    def _on_right_click(self, event: "tk.Event[tk.Canvas]") -> None:
        menu = tk.Menu(self, tearoff=0,
                       bg="#252525", fg="#e0e0e0",
                       activebackground="#3a3a3a", activeforeground="#ffffff",
                       relief="flat", bd=1)

        current = self._state.servo_type if self._state else None

        entries = [
            ("⬤  Continuous rotation (green)", ServoType.CONTINUOUS),
            ("◐  Angular 180°  (gray)",         ServoType.SERVO_180),
            ("◕  Angular 270°  (gray)",         ServoType.SERVO_270),
        ]
        for label, stype in entries:
            is_current = current == stype
            menu.add_command(
                label=("✓ " if is_current else "   ") + label,
                command=lambda t=stype: self._set_type(t),
                state="disabled" if is_current else "normal",
            )

        menu.tk_popup(event.x_root, event.y_root)

    def _set_type(self, stype: ServoType) -> None:
        """Apply *stype* locally and notify the app via the callback."""
        if self._state is None:
            return
        self._state.servo_type = stype
        self._state.active     = True
        # Reset values to sensible defaults for the new mode
        if stype == ServoType.CONTINUOUS:
            self._state.speed = 0
        else:
            self._state.angle = 90.0 if stype == ServoType.SERVO_180 else 135.0
        self.update_state(self._state)
        if self._on_type_change:
            self._on_type_change(self.channel_id, stype)

    # ------------------------------------------------------------------
    # #2 — flash animation
    # ------------------------------------------------------------------

    def flash(self) -> None:
        """Briefly flash the border white to signal a state change."""
        self.configure(highlightbackground=_COL_FLASH)
        self.after(self._FLASH_DUR_MS, self._restore_border)

    def _restore_border(self) -> None:
        active = self._state is not None and self._state.active
        self.configure(
            highlightbackground=_COL_BORDER_ON if active else _COL_BORDER_OFF,
        )

    # ------------------------------------------------------------------
    # Public state API
    # ------------------------------------------------------------------

    def update_state(self, state: Optional[ServoState]) -> None:
        self._state = state
        self.delete("all")
        active = state is not None and state.active
        self.configure(
            highlightbackground=_COL_BORDER_ON if active else _COL_BORDER_OFF,
        )
        if not active or state is None:
            self._draw_inactive()
        elif state.servo_type == ServoType.CONTINUOUS:
            self._draw_continuous(state)
        else:
            self._draw_rotational(state)

    # ------------------------------------------------------------------
    # Drawing — inactive
    # ------------------------------------------------------------------

    def _draw_inactive(self) -> None:
        cx, cy, r = self._CX, self._CY, self._R
        self.create_oval(
            cx - r, cy - r, cx + r, cy + r,
            fill=_COL_INACTIVE, outline=_COL_ARC_TRACK, width=1,
        )
        self.create_text(cx, cy, text=self.get_label(), fill="#444444",
                         font=("monospace", 11, "bold"))
        self.create_text(cx, self.SIZE - 7, text="disconnected",
                         fill="#3a3a3a", font=("monospace", 7))

    # ------------------------------------------------------------------
    # Drawing — rotational servo  (SERVO_180 / SERVO_270)
    # ------------------------------------------------------------------

    def _draw_rotational(self, s: ServoState) -> None:
        cx, cy, r = self._CX, self._CY, self._R
        col = _COL_ROTATIONAL
        max_angle: float = 180.0 if s.servo_type == ServoType.SERVO_180 else 270.0
        angle = max(0.0, min(float(s.angle), max_angle))
        arc_start  = 270.0 - max_angle / 2.0
        arc_extent = max_angle

        # --- Background track arc ----------------------------------------
        self.create_arc(cx - r, cy - r, cx + r, cy + r,
                        start=arc_start, extent=arc_extent,
                        style="arc", outline=_COL_ARC_TRACK, width=6)

        # --- Midpoint tick (halfway along the arc) -----------------------
        mid_rad = math.radians(arc_start + arc_extent / 2.0)
        for frac in (0.33, 0.5, 0.66) if s.servo_type == ServoType.SERVO_270 else (0.5,):
            t_rad = math.radians(arc_start + arc_extent * frac)
            t_r0  = r - 2
            t_r1  = r + 3
            self.create_line(
                cx + t_r0 * math.cos(t_rad), cy - t_r0 * math.sin(t_rad),
                cx + t_r1 * math.cos(t_rad), cy - t_r1 * math.sin(t_rad),
                fill="#505050", width=1,
            )

        # --- Endpoint tick dots at 0° and max° ---------------------------
        for end_angle in (arc_start, arc_start + arc_extent):
            e_rad = math.radians(end_angle)
            ex = cx + r * math.cos(e_rad)
            ey = cy - r * math.sin(e_rad)
            self.create_oval(ex - 2, ey - 2, ex + 2, ey + 2,
                             fill="#555555", outline="")

        # --- Endpoint labels: 0° at start, max° at end ------------------
        # Offset labels outward by a small margin from the arc
        label_r = r + 10
        for end_angle, end_text in ((arc_start, "0°"), (arc_start + arc_extent, f"{int(max_angle)}°")):
            e_rad = math.radians(end_angle)
            lx = cx + label_r * math.cos(e_rad)
            ly = cy - label_r * math.sin(e_rad)
            self.create_text(lx, ly, text=end_text,
                             fill="#555555", font=("monospace", 6), anchor="center")

        # --- Progress arc (zero → current angle) --------------------------
        progress_extent = (angle / max_angle) * arc_extent
        if progress_extent > 0.5:
            self.create_arc(cx - r, cy - r, cx + r, cy + r,
                            start=arc_start, extent=progress_extent,
                            style="arc", outline=col, width=4)

        # --- Hub circle ---------------------------------------------------
        h = self._HUB
        self.create_oval(cx - h, cy - h, cx + h, cy + h,
                         fill=_COL_HUB_BODY, outline=col, width=2)

        # --- Needle -------------------------------------------------------
        tk_angle  = arc_start + progress_extent
        angle_rad = math.radians(tk_angle)
        nx = cx + self._NEEDLE_R * math.cos(angle_rad)
        ny = cy - self._NEEDLE_R * math.sin(angle_rad)
        self.create_line(cx, cy, nx, ny, fill=_COL_NEEDLE, width=3, capstyle="round")
        self.create_oval(cx - 3, cy - 3, cx + 3, cy + 3, fill=col, outline="")

        # --- Labels -------------------------------------------------------
        type_label = "180°" if s.servo_type == ServoType.SERVO_180 else "270°"
        self.create_text(cx, 9,              text=self.get_label(),  fill=_COL_LABEL, font=("monospace", 8))
        self.create_text(cx, self.SIZE - 19, text=type_label,        fill=col,        font=("monospace", 8))
        self.create_text(cx, self.SIZE -  7, text=f"{int(angle)}°",  fill=_COL_VALUE, font=("monospace", 9, "bold"))

    # ------------------------------------------------------------------
    # Drawing — continuous servo
    # ------------------------------------------------------------------

    def _draw_continuous(self, s: ServoState) -> None:
        cx, cy, r = self._CX, self._CY, self._R
        col = _COL_CONTINUOUS
        speed_norm = max(-1.0, min(1.0, s.speed / 100.0))

        # --- Outer ring background ----------------------------------------
        self.create_oval(cx - r, cy - r, cx + r, cy + r,
                         fill="#0e2018", outline=col, width=2)

        # --- Speed arc (proportional to |speed|, clockwise from 12) ------
        if abs(speed_norm) > 0.01:
            arc_extent = speed_norm * 300.0
            arc_col    = _COL_CONT_FWD if speed_norm > 0 else _COL_CONT_REV
            ir = r - 9
            self.create_arc(cx - ir, cy - ir, cx + ir, cy + ir,
                            start=90, extent=-arc_extent,
                            style="arc", outline=arc_col, width=6)

        # --- Scale tick marks at 12, 3, 6, 9 o'clock (0 / ±50% / ±100%) -
        # 12 o'clock = 90° tkinter = 0 speed
        # 3 o'clock  = 0°  tkinter = +50% (clockwise)
        # 6 o'clock  = 270° tkinter = +100%
        # 9 o'clock  = 180° tkinter = -50% (counter-clockwise)
        for tick_tk, tick_label in (
            (90,  "0"),
            (0,   "+50"),
            (270, "+100"),
            (180, "-50"),
        ):
            t_rad = math.radians(tick_tk)
            t_r0  = r - 1
            t_r1  = r + 4
            self.create_line(
                cx + t_r0 * math.cos(t_rad), cy - t_r0 * math.sin(t_rad),
                cx + t_r1 * math.cos(t_rad), cy - t_r1 * math.sin(t_rad),
                fill="#405040", width=1,
            )
            # tiny label just outside the tick
            lx = cx + (r + 11) * math.cos(t_rad)
            ly = cy - (r + 11) * math.sin(t_rad)
            self.create_text(lx, ly, text=tick_label,
                             fill="#3a5a3a", font=("monospace", 6), anchor="center")

        # --- Hub circle ---------------------------------------------------
        h = self._HUB
        self.create_oval(cx - h, cy - h, cx + h, cy + h,
                         fill=_COL_HUB_BODY, outline=col, width=2)

        # --- Direction arrow on ring edge ---------------------------------
        if abs(speed_norm) > 0.01:
            arrow_pos_tk = 45.0
            a_rad = math.radians(arrow_pos_tk)
            ax = cx + r * math.cos(a_rad)
            ay = cy - r * math.sin(a_rad)
            tang_offset = 90.0 if speed_norm > 0 else -90.0
            t_rad = math.radians(arrow_pos_tk + tang_offset)
            tx = ax + 10 * math.cos(t_rad)
            ty = ay - 10 * math.sin(t_rad)
            arrow_col = _COL_CONT_FWD if speed_norm > 0 else _COL_CONT_REV
            self.create_line(ax, ay, tx, ty, fill=arrow_col, width=2,
                             arrow="last", arrowshape=(7, 9, 4))

        # --- Labels -------------------------------------------------------
        if s.speed == 0:
            speed_str, val_col = "STOP", "#666666"
        else:
            speed_str = f"{s.speed:+d}%"
            val_col   = _COL_CONT_FWD if s.speed > 0 else _COL_CONT_REV

        self.create_text(cx, 9,              text=self.get_label(), fill=_COL_LABEL, font=("monospace", 8))
        self.create_text(cx, self.SIZE - 19, text="CONT",           fill=col,        font=("monospace", 8))
        self.create_text(cx, self.SIZE -  7, text=speed_str,        fill=val_col,    font=("monospace", 9, "bold"))
