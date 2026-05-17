"""
Joystick calibration data model and JSON persistence.

Calibration corrects for:
  - Axis drift (non-zero neutral center)
  - Asymmetric physical range (axis doesn't reach ±1.0 evenly)
  - Per-joystick deadzone override
  - Inverted axes

The calibration file is keyed by joystick name so profiles survive
USB reconnects and persist across sessions.

JSON file format
----------------
{
  "Xbox 360 Controller": {
    "deadzone": 0.12,
    "axes": {
      "0": {"center": 0.01, "min": -1.0, "max": 0.99, "inverted": false},
      "1": {"center": -0.02, "min": -0.98, "max": 1.0, "inverted": true}
    }
  }
}

Quickstart
----------
  # Load once at startup
  store = CalibrationStore()
  store.load()                            # reads joystick_calibration.json

  # Use inside an axis handler
  corrected = store.apply_axis(joystick_name, axis_id, raw_value)
  if corrected is None:
      corrected = raw_value               # no calibration for this joystick
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path

# Default path: example-clients/python/joystick_calibration.json
DEFAULT_CALIBRATION_FILE: Path = (
    Path(__file__).resolve().parent.parent.parent / "joystick_calibration.json"
)


# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------


@dataclass
class AxisCalibration:
    """Per-axis calibration parameters.

    Attributes:
        center:   Raw axis value at the physical neutral/rest position.
        min:      Minimum raw value observed during range sampling.
        max:      Maximum raw value observed during range sampling.
        inverted: When ``True`` the normalised output is negated.
    """

    center: float = 0.0
    min: float = -1.0
    max: float = 1.0
    inverted: bool = False


@dataclass
class JoystickCalibration:
    """Full calibration profile for one joystick (identified by name).

    Attributes:
        joystick_name: Human-readable name returned by pygame (e.g.
                       ``"Xbox 360 Controller"``).
        deadzone:      Fraction of the range around zero to treat as idle.
        axes:          Mapping of axis index → :class:`AxisCalibration`.
    """

    joystick_name: str
    deadzone: float = 0.15
    axes: dict[int, AxisCalibration] = field(default_factory=dict)

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def apply_axis(self, axis_id: int, raw: float) -> float:
        """Return a calibrated, deadzone-applied value in ``[-1.0, 1.0]``.

        If no :class:`AxisCalibration` is stored for *axis_id* the value
        is passed through the deadzone filter only.

        Args:
            axis_id: pygame axis index.
            raw:     Raw axis value from pygame (nominally in ``[-1, 1]``).

        Returns:
            Corrected value in ``[-1.0, 1.0]``.
        """
        cal = self.axes.get(axis_id)
        if cal is None:
            return self._apply_deadzone(raw)

        # 1. Remove neutral drift
        centered = raw - cal.center

        # 2. Asymmetric range normalisation → [-1, 1]
        if centered >= 0:
            span = cal.max - cal.center
            normalized = (centered / span) if span > 1e-6 else 0.0
        else:
            span = cal.center - cal.min
            normalized = (centered / span) if span > 1e-6 else 0.0

        # 3. Safety clamp
        normalized = max(-1.0, min(1.0, normalized))

        # 4. Deadzone
        normalized = self._apply_deadzone(normalized)

        # 5. Optional inversion
        if cal.inverted:
            normalized = -normalized

        return normalized

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    def _apply_deadzone(self, value: float) -> float:
        """Re-scale value so the deadzone region collapses to zero."""
        if abs(value) < self.deadzone:
            return 0.0
        sign = 1.0 if value > 0 else -1.0
        return sign * (abs(value) - self.deadzone) / (1.0 - self.deadzone)


# ---------------------------------------------------------------------------
# Persistence
# ---------------------------------------------------------------------------


class CalibrationStore:
    """Registry of :class:`JoystickCalibration` objects, backed by JSON.

    Example::

        store = CalibrationStore()
        store.load()                              # load from default file
        value = store.apply_axis(name, axis, raw) # returns None if unknown
    """

    def __init__(self) -> None:
        self._calibrations: dict[str, JoystickCalibration] = {}

    # ------------------------------------------------------------------
    # CRUD
    # ------------------------------------------------------------------

    def get(self, joystick_name: str) -> JoystickCalibration | None:
        """Return the calibration for a joystick, or ``None`` if absent."""
        return self._calibrations.get(joystick_name)

    def set(self, calibration: JoystickCalibration) -> None:
        """Store or replace the calibration profile for a joystick."""
        self._calibrations[calibration.joystick_name] = calibration

    def remove(self, joystick_name: str) -> bool:
        """Delete the calibration profile for a joystick.

        Returns:
            ``True`` if the profile existed and was removed.
        """
        if joystick_name in self._calibrations:
            del self._calibrations[joystick_name]
            return True
        return False

    @property
    def known_joysticks(self) -> list[str]:
        """Names of all joysticks with a stored profile."""
        return list(self._calibrations.keys())

    # ------------------------------------------------------------------
    # Convenience
    # ------------------------------------------------------------------

    def apply_axis(self, joystick_name: str, axis_id: int, raw: float) -> float | None:
        """Apply calibration to a raw axis value.

        Args:
            joystick_name: Name of the joystick as reported by pygame.
            axis_id:       Axis index.
            raw:           Raw value from pygame (``[-1, 1]``).

        Returns:
            Calibrated value in ``[-1.0, 1.0]``, or ``None`` if no
            calibration profile exists for *joystick_name*.
        """
        cal = self._calibrations.get(joystick_name)
        if cal is None:
            return None
        return cal.apply_axis(axis_id, raw)

    # ------------------------------------------------------------------
    # Serialisation
    # ------------------------------------------------------------------

    def load(self, path: Path = DEFAULT_CALIBRATION_FILE) -> None:
        """Load calibration profiles from a JSON file.

        Silently replaces any previously loaded profiles.

        Args:
            path: Path to the JSON file (default: ``joystick_calibration.json``
                  next to ``config.py``).

        Raises:
            FileNotFoundError: If *path* does not exist.
            json.JSONDecodeError: If the file is malformed.
        """
        with open(path, encoding="utf-8") as fh:
            data: dict = json.load(fh)

        self._calibrations.clear()
        for js_name, js_data in data.items():
            axes: dict[int, AxisCalibration] = {}
            for str_id, axis_data in js_data.get("axes", {}).items():
                axes[int(str_id)] = AxisCalibration(
                    center=float(axis_data.get("center", 0.0)),
                    min=float(axis_data.get("min", -1.0)),
                    max=float(axis_data.get("max", 1.0)),
                    inverted=bool(axis_data.get("inverted", False)),
                )
            self._calibrations[js_name] = JoystickCalibration(
                joystick_name=js_name,
                deadzone=float(js_data.get("deadzone", 0.15)),
                axes=axes,
            )

    def save(self, path: Path = DEFAULT_CALIBRATION_FILE) -> None:
        """Persist all calibration profiles to a JSON file.

        Creates parent directories if they do not exist.

        Args:
            path: Destination JSON file path.
        """
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)

        data: dict = {}
        for js_name, cal in self._calibrations.items():
            data[js_name] = {
                "deadzone": cal.deadzone,
                "axes": {
                    str(axis_id): {
                        "center": ac.center,
                        "min": ac.min,
                        "max": ac.max,
                        "inverted": ac.inverted,
                    }
                    for axis_id, ac in sorted(cal.axes.items())
                },
            }

        with open(path, "w", encoding="utf-8") as fh:
            json.dump(data, fh, indent=2)

    # ------------------------------------------------------------------
    # Dunder
    # ------------------------------------------------------------------

    def __repr__(self) -> str:
        return f"CalibrationStore({list(self._calibrations.keys())})"
