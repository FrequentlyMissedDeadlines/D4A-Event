"""
Unified input manager combining keyboard and joystick inputs.
"""

from dataclasses import dataclass
from typing import Callable, Dict, List, Optional

from udp_client.input.keyboard_handler import KeyboardHandler
from udp_client.input.joystick_handler import JoystickHandler, JoystickState


@dataclass
class InputState:
    """Combined state from all input devices."""

    keyboard_keys: Dict[str, bool]
    joystick_states: List[JoystickState]
    last_update_ms: float = 0.0


class InputManager:
    """Unified input manager for keyboard and joysticks."""

    def __init__(self, keyboard_enabled: bool = True, joystick_enabled: bool = True):
        self.keyboard_enabled = keyboard_enabled
        self.joystick_enabled = joystick_enabled

        self.keyboard_handler: Optional[KeyboardHandler] = None
        self.joystick_handler: Optional[JoystickHandler] = None

        self.current_state: InputState = InputState(keyboard_keys={}, joystick_states=[])
        self.on_input_change: Optional[Callable[[InputState], None]] = None

        self._initialize()

    def _initialize(self) -> None:
        """Initialize enabled input handlers."""
        if self.keyboard_enabled:
            self.keyboard_handler = KeyboardHandler()
            self.keyboard_handler.on_key_change = self._on_keyboard_change

        if self.joystick_enabled:
            self.joystick_handler = JoystickHandler()
            self.joystick_handler.on_joystick_change = self._on_joystick_change

    def start(self) -> None:
        """Start listening to all input devices."""
        if self.keyboard_handler:
            self.keyboard_handler.start()

        if self.joystick_handler:
            self.joystick_handler.start()

    def stop(self) -> None:
        """Stop listening to all input devices."""
        if self.keyboard_handler:
            self.keyboard_handler.stop()

        if self.joystick_handler:
            self.joystick_handler.stop()

    def update(self) -> None:
        """Update all input states. Call this regularly for joystick updates."""
        if self.joystick_handler:
            self.joystick_handler.update()

    def _on_keyboard_change(self, state: Dict[str, bool]) -> None:
        """Handle keyboard state change."""
        self.current_state.keyboard_keys = state
        self._notify_change()

    def _on_joystick_change(self, states: List[JoystickState]) -> None:
        """Handle joystick state change."""
        self.current_state.joystick_states = states
        self._notify_change()

    def _notify_change(self) -> None:
        """Notify listeners of input state change."""
        if self.on_input_change:
            self.on_input_change(self.current_state)

    def get_state(self) -> InputState:
        """Get current combined input state."""
        return self.current_state

    def get_active_devices(self) -> List[str]:
        """Get list of active input devices."""
        devices = []

        if self.keyboard_handler:
            keys = self.keyboard_handler.get_key_state()
            if any(keys.values()):
                devices.append("Keyboard")

        if self.joystick_handler:
            for state in self.joystick_handler.get_states():
                if state.connected and (any(state.buttons.values()) or state.left_stick_x != 0 or state.left_stick_y != 0):
                    devices.append(f"Joystick: {state.name}")

        return devices

    def get_input_summary(self) -> str:
        """Get human-readable summary of current input."""
        summary_parts = []

        if self.keyboard_handler:
            active_keys = self.keyboard_handler.get_active_keys_display()
            if active_keys != "(no keys)":
                summary_parts.append(f"⌨️  {active_keys}")

        if self.joystick_handler:
            for state in self.joystick_handler.get_states():
                if state.connected:
                    active_buttons = self.joystick_handler.get_active_buttons_display(state.id)
                    if active_buttons != "-none-":
                        summary_parts.append(f"🎮 {state.name}: {active_buttons}")

        return " | ".join(summary_parts) if summary_parts else "No input"
