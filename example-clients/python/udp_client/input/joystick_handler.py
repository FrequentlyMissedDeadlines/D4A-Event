"""
Joystick input handler using pygame.
"""

import logging
import os
from collections.abc import Callable
from dataclasses import dataclass

# Suppress pygame startup message
os.environ["PYGAME_HIDE_SUPPORT_PROMPT"] = "hide"

try:
    import pygame

    _PYGAME_AVAILABLE = True
except ImportError:
    _PYGAME_AVAILABLE = False
    pygame = None  # type: ignore[assignment]

from udp_client.input.joystick_calibration import DEFAULT_CALIBRATION_FILE, CalibrationStore


@dataclass
class JoystickState:
    """State of a single joystick."""

    id: int
    name: str
    connected: bool
    left_stick_x: float = 0.0
    left_stick_y: float = 0.0
    right_stick_x: float = 0.0
    right_stick_y: float = 0.0
    left_trigger: float = 0.0
    right_trigger: float = 0.0
    buttons: dict[int, bool] = None
    dpad: tuple = (0, 0)  # (x, y): x=-1/0/1 left/center/right, y=-1/0/1 down/center/up

    def __post_init__(self):
        if self.buttons is None:
            self.buttons = {}


class JoystickHandler:
    """Handle multiple joystick inputs using pygame."""

    def __init__(
        self,
        deadzone: float = 0.15,
        max_joysticks: int = 4,
        calibration_store: CalibrationStore | None = None,
        auto_load_calibration: bool = True,
    ):
        self.deadzone = deadzone
        self.max_joysticks = max_joysticks
        self.joysticks: dict[int, pygame.joystick.Joystick] = {}
        self.joystick_states: dict[int, JoystickState] = {}
        self.on_joystick_change: Callable[[list[JoystickState]], None] | None = None

        # Calibration store: use provided, auto-load from file, or none
        if calibration_store is not None:
            self.calibration_store: CalibrationStore | None = calibration_store
        elif auto_load_calibration:
            store = CalibrationStore()
            try:
                store.load(DEFAULT_CALIBRATION_FILE)
                self.calibration_store = store
            except FileNotFoundError:
                self.calibration_store = None  # no calibration file yet — that's fine
        else:
            self.calibration_store = None

        # Initialize pygame if needed
        if not _PYGAME_AVAILABLE:
            raise ImportError(
                "pygame is required for joystick support but is not installed. "
                "Install it with: pip install pygame"
            )
        if not pygame.get_init():
            pygame.init()
            pygame.joystick.init()

    def start(self) -> None:
        """Initialize and detect joysticks."""
        joystick_count = pygame.joystick.get_count()

        for i in range(min(joystick_count, self.max_joysticks)):
            try:
                js = pygame.joystick.Joystick(i)
                js.init()
                self.joysticks[i] = js

                self.joystick_states[i] = JoystickState(
                    id=i,
                    name=js.get_name(),
                    connected=True,
                )
            except Exception as e:
                logging.getLogger(__name__).warning("Error initializing joystick %d: %s", i, e)

    def stop(self) -> None:
        """Clean up joysticks."""
        self.joysticks.clear()
        pygame.quit()

    def reload_calibration(self, calibration_file=DEFAULT_CALIBRATION_FILE) -> None:
        """Reload calibration data from file, replacing the current store."""
        store = CalibrationStore()
        try:
            store.load(calibration_file)
            self.calibration_store = store
        except FileNotFoundError:
            self.calibration_store = None

    def update(self) -> None:
        """
        Update joystick states by processing pygame events.
        Must be called regularly to detect input changes.
        """
        # Process pygame events
        for event in pygame.event.get():
            if event.type == pygame.JOYAXISMOTION:
                self._handle_axis_motion(event)
            elif event.type == pygame.JOYBUTTONDOWN:
                self._handle_button_down(event)
            elif event.type == pygame.JOYBUTTONUP:
                self._handle_button_up(event)
            elif event.type == pygame.JOYHATMOTION:
                self._handle_hat_motion(event)
            elif event.type == pygame.JOYDEVICEADDED:
                self._handle_device_added(event)
            elif event.type == pygame.JOYDEVICEREMOVED:
                self._handle_device_removed(event)

        # Notify change if any state was updated
        if self.on_joystick_change:
            self.on_joystick_change(list(self.joystick_states.values()))

    def _apply_deadzone(self, value: float) -> float:
        """Apply deadzone to analog stick values."""
        if abs(value) < self.deadzone:
            return 0.0
        # Scale the value to account for deadzone
        sign = 1 if value > 0 else -1
        normalized = (abs(value) - self.deadzone) / (1.0 - self.deadzone)
        return sign * normalized

    def _handle_axis_motion(self, event: pygame.event.EventType) -> None:
        """Handle analog stick/trigger motion."""
        js_id = event.joy
        if js_id not in self.joystick_states:
            return

        state = self.joystick_states[js_id]
        raw = event.value

        # --- Apply calibration if a profile exists for this joystick ---
        calibrated: float | None = None
        if self.calibration_store is not None:
            calibrated = self.calibration_store.apply_axis(state.name, event.axis, raw)

        if calibrated is not None:
            # Calibrated value is already in [-1, 1] with deadzone applied.
            # Triggers: calibration maps rest(-1) → 0, full-press → 1 naturally.
            if event.axis == 0:  # Left Stick X
                state.left_stick_x = calibrated
            elif event.axis == 1:  # Left Stick Y
                state.left_stick_y = calibrated
            elif event.axis == 2:  # Left Trigger
                state.left_trigger = max(0.0, calibrated)
            elif event.axis == 3:  # Right Stick X
                state.right_stick_x = calibrated
            elif event.axis == 4:  # Right Stick Y
                state.right_stick_y = calibrated
            elif event.axis == 5:  # Right Trigger
                state.right_trigger = max(0.0, calibrated)
        else:
            # Fallback: original uncalibrated logic
            if event.axis == 0:  # Left Stick X
                state.left_stick_x = self._apply_deadzone(raw)
            elif event.axis == 1:  # Left Stick Y
                state.left_stick_y = self._apply_deadzone(raw)
            elif event.axis == 2:  # Left Trigger — convert from [-1, 1] to [0, 1]
                state.left_trigger = (raw + 1) / 2
            elif event.axis == 3:  # Right Stick X
                state.right_stick_x = self._apply_deadzone(raw)
            elif event.axis == 4:  # Right Stick Y
                state.right_stick_y = self._apply_deadzone(raw)
            elif event.axis == 5:  # Right Trigger — convert from [-1, 1] to [0, 1]
                state.right_trigger = (raw + 1) / 2

    def _handle_button_down(self, event: pygame.event.EventType) -> None:
        """Handle button press."""
        js_id = event.joy
        if js_id not in self.joystick_states:
            return

        state = self.joystick_states[js_id]
        state.buttons[event.button] = True

    def _handle_button_up(self, event: pygame.event.EventType) -> None:
        """Handle button release."""
        js_id = event.joy
        if js_id not in self.joystick_states:
            return

        state = self.joystick_states[js_id]
        state.buttons[event.button] = False

    def _handle_hat_motion(self, event: pygame.event.EventType) -> None:
        """Handle D-pad input."""
        # D-pad is represented as a POV hat in pygame.
        # event.value is (x, y): x=-1/0/1 (left/center/right), y=-1/0/1 (down/center/up)
        js_id = event.joy
        if js_id not in self.joystick_states:
            return
        self.joystick_states[js_id].dpad = event.value

    def _handle_device_added(self, event: pygame.event.EventType) -> None:
        """Handle joystick hot-plug (plugged in after startup)."""
        device_index = event.device_index
        if len(self.joysticks) >= self.max_joysticks:
            return
        try:
            js = pygame.joystick.Joystick(device_index)
            js.init()
            js_id = js.get_instance_id()
            self.joysticks[js_id] = js
            self.joystick_states[js_id] = JoystickState(
                id=js_id,
                name=js.get_name(),
                connected=True,
            )
        except Exception as e:
            logging.getLogger(__name__).warning(
                "Error adding joystick at index %d: %s", device_index, e
            )

    def _handle_device_removed(self, event: pygame.event.EventType) -> None:
        """Handle joystick disconnect."""
        js_id = event.instance_id
        if js_id in self.joystick_states:
            self.joystick_states[js_id].connected = False
            del self.joystick_states[js_id]
        if js_id in self.joysticks:
            try:
                self.joysticks[js_id].quit()
            except Exception:
                pass
            del self.joysticks[js_id]

    def get_states(self) -> list[JoystickState]:
        """Get states of all connected joysticks."""
        return list(self.joystick_states.values())

    def get_button_name(self, button_id: int) -> str:
        """Get human-readable button name."""
        button_names = {
            0: "A",
            1: "B",
            2: "X",
            3: "Y",
            4: "LB",
            5: "RB",
            6: "Back",
            7: "Start",
            8: "LStick Press",
            9: "RStick Press",
        }
        return button_names.get(button_id, f"Button {button_id}")

    def get_active_buttons_display(self, joystick_id: int) -> str:
        """Get display string of pressed buttons on a joystick."""
        if joystick_id not in self.joystick_states:
            return "(no joystick)"

        state = self.joystick_states[joystick_id]
        pressed = [
            self.get_button_name(btn) for btn, is_pressed in state.buttons.items() if is_pressed
        ]
        return " ".join(pressed) if pressed else "-none-"
