"""Bot config inspector panel — pretty-prints the parsed customize.py config."""

from textual.widgets import Static


class BotConfigInspectorPanel(Static):
    """Pretty-prints the parsed customize.py config at runtime — zero network cost."""

    def render(self) -> str:  # noqa: C901
        ctrl = getattr(self.app, "controller", None)
        if ctrl is None or not ctrl.ready:
            return "[dim]🔧 Config not loaded yet — check customize.py.[/dim]"

        cfg = ctrl.bot_config
        lines: list[str] = []

        # ── HARDWARE ────────────────────────────────────────────────
        lines.append("[bold]HARDWARE[/bold]")
        motor_names = cfg.motor_names
        servo_names = cfg.servo_names
        if motor_names:
            for name in motor_names:
                info = cfg.get_motor_info(name)
                lines.append(f"  [cyan]{info['name']:<12}[/cyan] motor  → port {info['motor_id']}")
        if servo_names:
            from udp_client.bot_config import ServoType

            _type_label = {
                ServoType.SERVO_180: "180°",
                ServoType.SERVO_270: "270°",
                ServoType.CONTINUOUS: "CONTINUOUS",
            }
            for name in servo_names:
                info = cfg.get_servo_info(name)
                lines.append(
                    f"  [cyan]{info['name']:<12}[/cyan] servo  → ch {info['channel_id']}"
                    f"  {_type_label.get(info['servo_type'], str(info['servo_type']))}"
                )
        if not motor_names and not servo_names:
            lines.append("  (none)")

        # ── ACTIONS ───────────────────────────────────────────────
        lines.append("")
        lines.append("[bold]ACTIONS[/bold]")
        action_names = cfg.action_names
        if action_names:
            from udp_client.bot_config import MotorCmd, ServoAngleCmd, ServoSpeedCmd

            for action_name in action_names:
                cmds = cfg.get_action_commands(action_name)
                parts: list[str] = []
                for cmd in cmds:
                    if isinstance(cmd, MotorCmd):
                        sign = "+" if cmd.speed >= 0 else ""
                        parts.append(f"{cmd.motor_name} {sign}{cmd.speed}")
                    elif isinstance(cmd, ServoSpeedCmd):
                        sign = "+" if cmd.speed >= 0 else ""
                        parts.append(f"{cmd.servo_name} spd {sign}{cmd.speed}")
                    elif isinstance(cmd, ServoAngleCmd):
                        parts.append(f"{cmd.servo_name} ∠{cmd.angle}°")
                detail = ",  ".join(parts) if parts else "—"
                lines.append(f"  [green]{action_name:<16}[/green] {detail}")
        else:
            lines.append("  (none)")

        # ── BINDINGS ──────────────────────────────────────────────
        lines.append("")
        lines.append("[bold]BINDINGS[/bold]")
        _key_icon: dict[str, str] = {
            "up": "↑",
            "down": "↓",
            "left": "←",
            "right": "→",
            "space": "SPC",
            "shift": "SHF",
            "enter": "ENT",
        }
        kb = ctrl.keyboard_actions
        if kb:
            # Two bindings per row
            def _binding_label(binding: dict[str, str | None] | str) -> str:
                if isinstance(binding, dict):
                    down = binding.get("on_keydown") or ""
                    up = binding.get("on_keyup")
                    return f"{down} (↑{up})" if up else down
                return str(binding)

            items = [(f"[{_key_icon.get(k, k.upper())}]", _binding_label(v)) for k, v in kb.items()]
            for i in range(0, len(items), 2):
                left = f"  {items[i][0]:<5}  → {items[i][1]:<16}"
                right = f"  {items[i + 1][0]:<5}  → {items[i + 1][1]}" if i + 1 < len(items) else ""
                lines.append(left + right)
            if ctrl.keyboard_idle:
                lines.append(f"  (no key pressed)   → {ctrl.keyboard_idle}")
        else:
            lines.append("  (none)")

        # ── JOYSTICK ─────────────────────────────────────────────
        if ctrl.js_axis_bindings or ctrl.js_stick_handlers or ctrl.js_button_actions:
            lines.append("")
            lines.append("[bold]JOYSTICK[/bold]")
            for axis, target in ctrl.js_axis_bindings.items():
                lines.append(f"  {axis:<10}  → [{target}]   scale ×{ctrl.js_speed_scale}")
            for stick, handler in ctrl.js_stick_handlers.items():
                handler_name = getattr(handler, "__name__", repr(handler))
                lines.append(
                    f"  {stick:<10}  → handler: {handler_name}   scale ×{ctrl.js_speed_scale}"
                )
            for btn_id, action in ctrl.js_button_actions.items():
                lines.append(f"  button {btn_id:<4}  → {action}")

        return "\n".join(lines)
