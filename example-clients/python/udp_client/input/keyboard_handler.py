"""
Keyboard input handler using pynput.
"""

import asyncio
from typing import Callable, Dict, Optional, Set

from pynput import keyboard


class KeyboardHandler:
    """Handle keyboard input without requiring window focus."""

    def __init__(self):
        self.listener: Optional[keyboard.Listener] = None
        self.pressed_keys: Set[keyboard.Key] = set()
        self.on_key_change: Optional[Callable[[Dict[str, bool]], None]] = None

        # Key name mapping for display
        self.key_names = {
            keyboard.Key.up: "UP",
            keyboard.Key.down: "DOWN",
            keyboard.Key.left: "LEFT",
            keyboard.Key.right: "RIGHT",
            keyboard.Key.space: "SPACE",
            keyboard.Key.shift: "SHIFT",
            keyboard.Key.ctrl: "CTRL",
            keyboard.Key.alt: "ALT",
            keyboard.Key.enter: "ENTER",
            keyboard.Key.esc: "ESC",
        }

    def start(self) -> None:
        """Start listening to keyboard input."""
        self.listener = keyboard.Listener(
            on_press=self._on_press,
            on_release=self._on_release,
        )
        self.listener.start()

    def stop(self) -> None:
        """Stop listening to keyboard input."""
        if self.listener:
            self.listener.stop()

    def _on_press(self, key: keyboard.Key) -> None:
        """Handle key press."""
        self.pressed_keys.add(key)
        self._notify_change()

    def _on_release(self, key: keyboard.Key) -> None:
        """Handle key release."""
        self.pressed_keys.discard(key)
        self._notify_change()

    def _notify_change(self) -> None:
        """Notify listeners of key state change."""
        if self.on_key_change:
            state = self.get_key_state()
            self.on_key_change(state)

    def get_key_state(self) -> Dict[str, bool]:
        """Get current state of all relevant keys."""
        state = {
            "up": keyboard.Key.up in self.pressed_keys,
            "down": keyboard.Key.down in self.pressed_keys,
            "left": keyboard.Key.left in self.pressed_keys,
            "right": keyboard.Key.right in self.pressed_keys,
            "space": keyboard.Key.space in self.pressed_keys,
            "shift": keyboard.Key.shift in self.pressed_keys or keyboard.Key.shift_r in self.pressed_keys,
            "enter": keyboard.Key.enter in self.pressed_keys,
        }
        return state

    def get_active_keys_display(self) -> str:
        """Get a display string of currently pressed keys."""
        active = []
        for key in self.pressed_keys:
            if key in self.key_names:
                active.append(self.key_names[key])
            elif hasattr(key, "char") and key.char and key.char.isalnum():
                active.append(key.char.upper())
        return " ".join(active) if active else "(no keys)"

    def is_key_pressed(self, key_name: str) -> bool:
        """Check if a specific key is currently pressed."""
        state = self.get_key_state()
        return state.get(key_name, False)
