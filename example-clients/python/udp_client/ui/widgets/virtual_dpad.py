"""Virtual D-pad panel — shows active keys with bound action names."""

from textual.widgets import Static


class VirtualDpadPanel(Static):
    """D-pad overlay showing active keys with their bound action names from customize.py."""

    def render(self) -> str:  # noqa: C901
        app = self.app
        ctrl = getattr(app, "controller", None)
        kb = (ctrl.keyboard_actions if ctrl else {}) or {}

        # Reverse-map: logical-key -> action label
        up_action = kb.get("up", kb.get("w", "up"))
        down_action = kb.get("down", kb.get("s", "down"))
        left_action = kb.get("left", kb.get("a", "left"))
        right_action = kb.get("right", kb.get("d", "right"))

        # Collect extra bindings (not the four cardinal directions)
        _cardinal = {"up", "down", "left", "right", "w", "a", "s", "d"}
        extras = [(k, v) for k, v in kb.items() if k not in _cardinal]

        # Which keys are currently active?
        state = app.get_key_state() if hasattr(app, "get_key_state") else {}

        def _fmt(key: str, label: str) -> str:
            active = state.get(key, False)
            text = (
                f"↑ {label}"
                if key == "up"
                else (
                    f"↓ {label}"
                    if key == "down"
                    else (
                        f"← {label}"
                        if key == "left"
                        else (f"→ {label}" if key == "right" else label)
                    )
                )
            )
            return f"[bold reverse]{text}[/]" if active else f"[ {text} ]"

        up_str = _fmt("up", up_action)
        down_str = _fmt("down", down_action)
        left_str = _fmt("left", left_action)
        right_str = _fmt("right", right_action)

        col_w = 18
        lines = [
            "⌨️  VIRTUAL D-PAD",
            "─" * 50,
            f"{'':>{col_w}}{up_str}",
            f"{left_str:<{col_w}}      {right_str}",
            f"{'':>{col_w}}{down_str}",
        ]

        if extras:
            lines.append("")
            _key_icon = {"space": "SPACE", "shift": "SHIFT", "enter": "ENTER"}
            for k, v in extras:
                label = _key_icon.get(k, k.upper())
                active = state.get(k, False)
                cell = f"[bold reverse][{label} → {v}][/]" if active else f"[{label} → {v}]"
                lines.append(f"  {cell}")

        return "\n".join(lines)
