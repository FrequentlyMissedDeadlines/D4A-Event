"""
Interactive joystick calibration wizard.

Run as a standalone script:
    python -m udp_client.input.calibration_wizard

Or import and call programmatically:
    from udp_client.input.calibration_wizard import run_wizard
    run_wizard()

Calibration steps
-----------------
  1. Center  — rest all sticks/triggers → records neutral drift per axis.
  2. Range   — move everything to its limits → records min/max per axis.
  3. Deadzone — confirm or override the deadzone value.
  4. Inversion — choose which axes to invert (Y-axes are pre-suggested).

The result is saved to ``joystick_calibration.json`` (next to config.py) and
is automatically loaded by :class:`~udp_client.input.joystick_handler.JoystickHandler`
on next start.
"""

from __future__ import annotations

import os
import sys
import time
from pathlib import Path

os.environ["PYGAME_HIDE_SUPPORT_PROMPT"] = "hide"
import pygame

# Allow running as `python calibration_wizard.py` directly from this directory
_HERE = Path(__file__).resolve().parent
_PYTHON_ROOT = _HERE.parent.parent
if str(_PYTHON_ROOT) not in sys.path:
    sys.path.insert(0, str(_PYTHON_ROOT))

from udp_client.input.joystick_calibration import (
    DEFAULT_CALIBRATION_FILE,
    AxisCalibration,
    CalibrationStore,
    JoystickCalibration,
)

# ---------------------------------------------------------------------------
# Axis metadata
# ---------------------------------------------------------------------------

AXIS_NAMES: dict[int, str] = {
    0: "Left Stick X",
    1: "Left Stick Y",
    2: "Left Trigger",
    3: "Right Stick X",
    4: "Right Stick Y",
    5: "Right Trigger",
}

# Axes where Y=+1 means "down" on most gamepads — suggest inversion
_SUGGEST_INVERT = {1, 4}

# Trigger axes: rest position is -1.0 on most controllers
_TRIGGER_AXES = {2, 5}

# Sampling rate for the range phase
_SAMPLE_HZ = 50
_SAMPLE_SLEEP = 1.0 / _SAMPLE_HZ

# ---------------------------------------------------------------------------
# Formatting helpers
# ---------------------------------------------------------------------------

_SEP = "─" * 54


def _hr(title: str = "") -> None:
    if title:
        pad = max(0, 54 - len(title) - 2)
        print(f"\n── {title} {'─' * pad}")
    else:
        print(_SEP)


def _axis_label(axis_id: int) -> str:
    return AXIS_NAMES.get(axis_id, f"Axis {axis_id}")


# ---------------------------------------------------------------------------
# ANSI / live-display helpers
# ---------------------------------------------------------------------------

_ANSI: bool = sys.stdout.isatty()


def _esc(code: str) -> str:
    return f"\033[{code}" if _ANSI else ""


_R = _esc("0m")  # reset
_B = _esc("1m")  # bold
_DIM = _esc("2m")  # dim
_Y = _esc("33m")  # yellow  — active axis value
_G = _esc("32m")  # green   — peak / confirmed
_C = _esc("36m")  # cyan    — target marker in grid
_M = _esc("35m")  # magenta — direction instruction
_EL = _esc("K")  # erase to end of line

# Number of lines printed by _render_joy_display (must stay in sync with that function)
_DISPLAY_H = 17


def _trig_bar(raw: float, active: bool, width: int = 12) -> str:
    """Horizontal bar for a trigger axis (raw −1…+1 → 0…100%)."""
    norm = max(0.0, min(1.0, (raw + 1.0) / 2.0))
    filled = round(norm * width)
    bar = "█" * filled + "░" * (width - filled)
    return f"{_Y}{bar}{_R}" if active else bar


def _stick_grid(
    x: float,
    y: float,
    active: bool,
    target_col: int | None,
    target_row: int | None,
    w: int = 9,
    h: int = 5,
) -> list[str]:
    """Render a w×h ASCII grid with a live dot and an optional target marker (◎)."""
    col = round((x + 1) / 2 * (w - 1))
    row = round((1 - (y + 1) / 2) * (h - 1))
    col = max(0, min(w - 1, col))
    row = max(0, min(h - 1, row))
    cx, cy = w // 2, h // 2
    lines: list[str] = []
    for r in range(h):
        line = ""
        for c in range(w):
            is_dot = r == row and c == col
            is_tgt = (target_col is not None and r == cy and c == target_col) or (
                target_row is not None and c == cx and r == target_row
            )
            if is_dot:
                line += f"{_Y}{_B}●{_R}" if active else "●"
            elif is_tgt:
                line += f"{_C}◎{_R}"
            elif r == cy and c == cx:
                line += "+"
            elif r == cy or c == cx:
                line += "·"
            else:
                line += " "
        lines.append(line)
    return lines


_ARROW_LABELS: dict[str, str] = {
    "fully LEFT": "◄◄  push LEFT",
    "fully RIGHT": "push RIGHT  ►►",
    "fully UP": "▲▲  push UP",
    "fully DOWN": "push DOWN  ▼▼",
    "fully pressed": "▼▼  press fully",
    "fully released": "▲▲  release fully",
}


def _render_joy_display(
    js: pygame.joystick.Joystick,
    active_axis: int,
    target_sign: int,
    direction_hint: str,
    peak: float,
) -> None:
    """Print (or reprint) the fixed _DISPLAY_H-line joystick visualisation block.

    Lines are each terminated with _EL (erase-to-EOL) so redraws are clean.
    """
    pygame.event.pump()
    na = js.get_numaxes()
    vals = [js.get_axis(i) if i < na else 0.0 for i in range(6)]
    lx, ly, lt, rx, ry, rt = vals  # 0=LSX 1=LSY 2=LT 3=RSX 4=RSY 5=RT

    # Target marker position for each stick
    def _targets(base: int) -> tuple[int | None, int | None]:
        if active_axis == base:  # X axis → target column
            return (0 if target_sign < 0 else 8), None
        if active_axis == base + 1:  # Y axis → target row
            # raw min (sign=-1) is physically UP → row 0 (top of grid)
            return None, (0 if target_sign < 0 else 4)
        return None, None

    l_tc, l_tr = _targets(0)  # left stick:  axes 0,1
    r_tc, r_tr = _targets(3)  # right stick: axes 3,4
    lg = _stick_grid(lx, ly, active_axis in (0, 1), l_tc, l_tr)
    rg = _stick_grid(rx, ry, active_axis in (3, 4), r_tc, r_tr)

    def _hi(i: int, s: str) -> str:
        """Highlight string if axis i is the active one."""
        return f"{_Y}{_B}{s}{_R}" if i == active_axis else s

    lx_s = _hi(0, f"{lx:+.3f}")
    ly_s = _hi(1, f"{ly:+.3f}")
    rx_s = _hi(3, f"{rx:+.3f}")
    ry_s = _hi(4, f"{ry:+.3f}")
    lt_s = _hi(2, f"{(lt + 1) / 2:.3f}")
    rt_s = _hi(5, f"{(rt + 1) / 2:.3f}")

    arrow = _ARROW_LABELS.get(direction_hint, direction_hint)
    label = _axis_label(active_axis)

    # Extent bar: fraction of travel toward the target reached so far.
    # target_sign=-1 means we want peak as negative as possible (min toward -1).
    # target_sign=+1 means we want peak as positive as possible (max toward +1).
    pb_w = 18
    pb_n = round(min(1.0, max(0.0, peak * target_sign)) * pb_w)
    pb = "█" * pb_n + "░" * (pb_w - pb_n)

    # Exactly _DISPLAY_H lines:
    out = [
        f"  {_B}Axis {active_axis}: {label}{_R}  →  {_M}{_B}{arrow}{_R}",  # 1
        "",  # 2
        f"  {'Left Stick':<20}      Right Stick",  # 3
        f"  ┌{'─' * 9}┐           ┌{'─' * 9}┐",  # 4
        *[f"  │{lg[r]}│           │{rg[r]}│" for r in range(5)],  # 5–9
        f"  └{'─' * 9}┘           └{'─' * 9}┘",  # 10
        f"  X:{lx_s} Y:{ly_s}      X:{rx_s} Y:{ry_s}",  # 11
        "",  # 12
        f"  LT [{_trig_bar(lt, active_axis == 2)}] {lt_s}   "
        f"RT [{_trig_bar(rt, active_axis == 5)}] {rt_s}",  # 13
        "",  # 14
        f"  Peak: {_G}{_B}{peak:+.4f}{_R}   Extent [{pb}]",  # 15
        "",  # 16
        f"  {_DIM}Hold position — press ENTER to confirm  ·  Ctrl+C to abort{_R}",  # 17
    ]
    sys.stdout.write("".join(ln + _EL + "\n" for ln in out))
    sys.stdout.flush()


def _live_sample_axis(
    js: pygame.joystick.Joystick,
    axis: int,
    target_sign: int,
    direction_hint: str,
) -> float:
    """Show a live joystick visualisation and return the extreme axis value
    captured when the user presses ENTER.

    The display updates in real-time: the active axis is highlighted in yellow,
    the target position is shown as ◎ in the stick grid, and a peak extent bar
    grows as the user pushes toward the expected limit.

    Peak is tracked in the direction of target_sign (not by abs), so triggers
    starting at -1.0 are handled correctly for both min and max passes.
    """
    peak = js.get_axis(axis)
    _render_joy_display(js, axis, target_sign, direction_hint, peak)

    def _is_better(v: float) -> bool:
        """Return True if v is more extreme in the target direction than peak."""
        return v * target_sign > peak * target_sign

    if not _ANSI:
        # Fallback when stdout is not a tty (piped / redirected)
        input()
    elif os.name == "nt":
        import msvcrt

        while True:
            pygame.event.pump()
            v = js.get_axis(axis)
            if _is_better(v):
                peak = v
            sys.stdout.write(f"\033[{_DISPLAY_H}A\r")
            _render_joy_display(js, axis, target_sign, direction_hint, peak)
            if msvcrt.kbhit():
                ch = msvcrt.getch()
                if ch in (b"\r", b"\n"):
                    break
            time.sleep(_SAMPLE_SLEEP)
    else:
        import select as _sel
        import termios
        import tty

        fd = sys.stdin.fileno()
        old = termios.tcgetattr(fd)
        try:
            # setcbreak: keystrokes are available immediately (no line buffering),
            # but output processing stays ON so \n still does CR+LF correctly.
            # This avoids the diagonal-scroll bug caused by tty.setraw().
            tty.setcbreak(fd)
            while True:
                pygame.event.pump()
                v = js.get_axis(axis)
                if _is_better(v):
                    peak = v
                sys.stdout.write(f"\033[{_DISPLAY_H}A\r")
                _render_joy_display(js, axis, target_sign, direction_hint, peak)
                ready, _, _ = _sel.select([sys.stdin], [], [], _SAMPLE_SLEEP)
                if ready:
                    ch = sys.stdin.read(1)
                    if ch in ("\r", "\n", " "):
                        break
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, old)

    print(f"\n  ✓  Captured: {_G}{_B}{peak:+.4f}{_R}")
    return peak


# ---------------------------------------------------------------------------
# Wizard phases
# ---------------------------------------------------------------------------


def _select_joystick(joysticks: list[pygame.joystick.Joystick]) -> pygame.joystick.Joystick | None:
    """Prompt the user to select a joystick from the detected list."""
    print("\nDetected joysticks:")
    for i, js in enumerate(joysticks):
        print(f"  [{i}] {js.get_name()}  ({js.get_numaxes()} axes, {js.get_numbuttons()} buttons)")

    default = 0
    raw = input(f"\nSelect joystick [{default}]: ").strip()
    try:
        idx = int(raw) if raw else default
        if 0 <= idx < len(joysticks):
            return joysticks[idx]
    except ValueError:
        pass
    print("Invalid selection.")
    return None


def _phase_center(js: pygame.joystick.Joystick) -> dict[int, float]:
    """Phase 1 — record the raw neutral position of each axis."""
    _hr("Step 1 — Center calibration")
    print("  Release all sticks and triggers to their natural rest position.")
    input("  Press ENTER when ready...")

    pygame.event.pump()
    centers: dict[int, float] = {}
    num_axes = js.get_numaxes()
    for i in range(num_axes):
        centers[i] = js.get_axis(i)

    print("\n  Recorded centers:")
    for i, v in centers.items():
        print(f"    {_axis_label(i):<22}  {v:+.4f}")

    return centers


# Per-axis movement instructions: (min-direction label, max-direction label)
_AXIS_DIRECTIONS: dict[int, tuple[str, str]] = {
    0: ("fully LEFT", "fully RIGHT"),  # Left Stick X
    1: ("fully UP", "fully DOWN"),  # Left Stick Y
    2: ("fully released", "fully pressed"),  # Left Trigger
    3: ("fully LEFT", "fully RIGHT"),  # Right Stick X
    4: ("fully UP", "fully DOWN"),  # Right Stick Y
    5: ("fully released", "fully pressed"),  # Right Trigger
}


def _phase_range(
    js: pygame.joystick.Joystick,
    centers: dict[int, float],
) -> tuple[dict[int, float], dict[int, float]]:
    """Phase 2 — guided per-axis range calibration with live joystick visualisation.

    For every axis the display highlights the active axis and shows a ◎ target
    marker where the user should push.  The user presses ENTER to confirm each
    extreme; no time limit.
    """
    _hr("Step 2 — Range calibration  (axis by axis)")
    print("  Each axis is calibrated one at a time.")
    print("  The live display shows all stick/trigger values.")
    print("  The highlighted axis (◎ marker) shows where to push.")
    print("  Move to the indicated position, then press ENTER.\n")

    num_axes = js.get_numaxes()
    mins: dict[int, float] = {i: centers.get(i, 0.0) for i in range(num_axes)}
    maxs: dict[int, float] = {i: centers.get(i, 0.0) for i in range(num_axes)}

    for i in range(num_axes):
        label = _axis_label(i)
        dir_min, dir_max = _AXIS_DIRECTIONS.get(i, ("to minimum", "to maximum"))

        print(f"\n  ── Axis {i}: {label}")

        # --- Minimum extreme ---
        mins[i] = _live_sample_axis(js, i, -1, dir_min)

        # --- Return to center ---
        if i not in _TRIGGER_AXES:
            input(f"  Release {label} back to centre, then press ENTER… ")

        # --- Maximum extreme ---
        maxs[i] = _live_sample_axis(js, i, +1, dir_max)

        print(f"  ✓  range  [{mins[i]:+.4f} … {maxs[i]:+.4f}]")

    print("\n  Release all controls back to neutral.")
    input("  Press ENTER to continue… ")

    print("\n  Recorded range:")
    for i in range(num_axes):
        print(
            f"    {_axis_label(i):<22}  "
            f"min={mins[i]:+.4f}  center={centers.get(i, 0.0):+.4f}  max={maxs[i]:+.4f}"
        )

    return mins, maxs


def _phase_deadzone(default: float = 0.15) -> float:
    """Phase 3 — confirm or override the deadzone."""
    _hr("Step 3 — Deadzone")
    print("  The deadzone is the fraction of stick movement ignored near centre.")
    print("  Typical range: 0.05 (precise) – 0.25 (forgiving).")
    raw = input(f"  Enter deadzone or ENTER to keep [{default:.2f}]: ").strip()
    if not raw:
        return default
    try:
        value = float(raw)
        if 0.0 <= value <= 0.50:
            return value
        print("  Out of range, keeping default.")
    except ValueError:
        print("  Not a number, keeping default.")
    return default


def _phase_inversion(num_axes: int) -> dict[int, bool]:
    """Phase 4 — per-axis inversion flags."""
    _hr("Step 4 — Axis inversion")
    print("  On most gamepads pushing a stick UP gives a NEGATIVE Y value.")
    print("  Invert the axis so +1 = up / forward.\n")

    inverted: dict[int, bool] = {}
    for i in range(num_axes):
        label = _axis_label(i)
        suggested = i in _SUGGEST_INVERT
        hint = "Y/n" if suggested else "y/N"
        raw = input(f"  Invert {label:<22} [{hint}]: ").strip().lower()
        if raw in ("y", "yes"):
            inverted[i] = True
        elif raw in ("n", "no"):
            inverted[i] = False
        else:
            inverted[i] = suggested  # accept suggestion on empty input

    return inverted


# ---------------------------------------------------------------------------
# Main wizard entry-point
# ---------------------------------------------------------------------------


def run_wizard(
    calibration_file: Path = DEFAULT_CALIBRATION_FILE,
) -> JoystickCalibration | None:
    """Run the interactive calibration wizard.

    Args:
        calibration_file: Where to save (and merge with existing) calibration
                          data.  Defaults to ``joystick_calibration.json``
                          next to ``config.py``.

    Returns:
        The :class:`~udp_client.input.joystick_calibration.JoystickCalibration`
        that was saved, or ``None`` if the wizard was aborted.
    """
    print()
    print("╔══════════════════════════════════════════════════════╗")
    print("║        K10 Bot — Joystick Calibration Wizard         ║")
    print("╚══════════════════════════════════════════════════════╝")

    # Init pygame
    if not pygame.get_init():
        pygame.init()
    if not pygame.joystick.get_init():
        pygame.joystick.init()

    count = pygame.joystick.get_count()
    if count == 0:
        print("\n  No joystick detected.  Plug one in and try again.")
        return None

    joysticks: list[pygame.joystick.Joystick] = []
    for i in range(count):
        js = pygame.joystick.Joystick(i)
        js.init()
        joysticks.append(js)

    js = _select_joystick(joysticks)
    if js is None:
        return None

    js_name = js.get_name()
    num_axes = js.get_numaxes()
    print(f"\n  Calibrating: {js_name}  ({num_axes} axes)")

    # --- Run phases ---
    centers = _phase_center(js)
    mins, maxs = _phase_range(js, centers)
    deadzone = _phase_deadzone()
    inverted_map = _phase_inversion(num_axes)

    # --- Build calibration object ---
    axes: dict[int, AxisCalibration] = {}
    for i in range(num_axes):
        axes[i] = AxisCalibration(
            center=round(centers.get(i, 0.0), 5),
            min=round(mins.get(i, -1.0), 5),
            max=round(maxs.get(i, 1.0), 5),
            inverted=inverted_map.get(i, False),
        )

    calibration = JoystickCalibration(
        joystick_name=js_name,
        deadzone=deadzone,
        axes=axes,
    )

    # --- Merge with existing file ---
    store = CalibrationStore()
    if calibration_file.exists():
        try:
            store.load(calibration_file)
        except Exception:
            pass  # corrupt file — start fresh

    store.set(calibration)
    store.save(calibration_file)

    # --- Summary ---
    _hr("Calibration saved")
    print(f"  File:     {calibration_file}")
    print(f"  Joystick: {js_name}")
    print(f"  Deadzone: {deadzone:.2f}")
    print()
    for i, ac in sorted(axes.items()):
        inv = " (inverted)" if ac.inverted else ""
        print(
            f"  {_axis_label(i):<22}  center={ac.center:+.4f}  [{ac.min:+.4f} … {ac.max:+.4f}]{inv}"
        )
    print()
    print("  Calibration will be applied automatically on next launch.")
    print()

    return calibration


# ---------------------------------------------------------------------------
# Script entry-point
# ---------------------------------------------------------------------------


def main() -> None:
    """Entry-point for ``python -m udp_client.input.calibration_wizard``."""
    try:
        result = run_wizard()
        if result is None:
            sys.exit(1)
    except KeyboardInterrupt:
        print("\n\n  Calibration cancelled.")
        sys.exit(0)
    finally:
        if pygame.get_init():
            pygame.quit()


if __name__ == "__main__":
    main()
