"""Unit tests for Controller._merge_commands — pure logic, no I/O."""

import pytest

from udp_client.bot_config import (
    BotConfig,
    MotorCmd,
    ServoAngleCmd,
    ServoSpeedCmd,
    ServoType,
)
from udp_client.control.controller import Controller

# ── Helpers ───────────────────────────────────────────────────────────


class _FakeUDP:
    """Minimal stand-in for UDPClient so Controller can be instantiated."""

    connected = False


def _make_controller() -> Controller:
    """Build a Controller with a dummy UDP client and a small BotConfig."""
    ctrl = Controller(_FakeUDP())  # type: ignore[arg-type]
    # Manually inject a BotConfig (bypass customize.py loading)
    cfg = BotConfig()
    cfg.add_motor("left", motor_id=1)
    cfg.add_motor("right", motor_id=2)
    cfg.add_servo("arm", channel_id=0, servo_type=ServoType.CONTINUOUS)
    cfg.add_servo("pan", channel_id=3, servo_type=ServoType.SERVO_180)
    cfg.add_servo("tilt", channel_id=4, servo_type=ServoType.SERVO_270)
    ctrl._bot_config = cfg
    ctrl._ready = True
    return ctrl


@pytest.fixture
def ctrl() -> Controller:
    return _make_controller()


# ── Motor merging ─────────────────────────────────────────────────────


class TestMergeMotors:
    def test_single_motor_passthrough(self, ctrl: Controller):
        merged = ctrl._merge_commands([MotorCmd("left", 60)])
        assert len(merged) == 1
        assert merged[0] == MotorCmd("left", 60)

    def test_same_motor_speeds_summed(self, ctrl: Controller):
        """Two sources contributing to the same motor are summed."""
        cmds = [MotorCmd("left", 40), MotorCmd("left", 30)]
        merged = ctrl._merge_commands(cmds)
        motor = next(c for c in merged if isinstance(c, MotorCmd) and c.motor_name == "left")
        assert motor.speed == 70

    def test_sum_clamped_to_100(self, ctrl: Controller):
        cmds = [MotorCmd("left", 80), MotorCmd("left", 80)]
        merged = ctrl._merge_commands(cmds)
        motor = next(c for c in merged if isinstance(c, MotorCmd) and c.motor_name == "left")
        assert motor.speed == 100

    def test_sum_clamped_to_minus_100(self, ctrl: Controller):
        cmds = [MotorCmd("left", -80), MotorCmd("left", -80)]
        merged = ctrl._merge_commands(cmds)
        motor = next(c for c in merged if isinstance(c, MotorCmd) and c.motor_name == "left")
        assert motor.speed == -100

    def test_zero_forces_stop(self, ctrl: Controller):
        """If any source contributes speed=0, the channel is forced to 0."""
        cmds = [MotorCmd("left", 80), MotorCmd("left", 0)]
        merged = ctrl._merge_commands(cmds)
        motor = next(c for c in merged if isinstance(c, MotorCmd) and c.motor_name == "left")
        assert motor.speed == 0

    def test_different_motors_independent(self, ctrl: Controller):
        cmds = [MotorCmd("left", 50), MotorCmd("right", -30)]
        merged = ctrl._merge_commands(cmds)
        speeds = {c.motor_name: c.speed for c in merged if isinstance(c, MotorCmd)}
        assert speeds == {"left": 50, "right": -30}


# ── Servo speed merging ──────────────────────────────────────────────


class TestMergeServoSpeed:
    def test_single_servo_speed(self, ctrl: Controller):
        merged = ctrl._merge_commands([ServoSpeedCmd("arm", 50)])
        assert len(merged) == 1
        assert merged[0] == ServoSpeedCmd("arm", 50)

    def test_servo_speeds_summed(self, ctrl: Controller):
        cmds = [ServoSpeedCmd("arm", 30), ServoSpeedCmd("arm", 40)]
        merged = ctrl._merge_commands(cmds)
        servo = next(c for c in merged if isinstance(c, ServoSpeedCmd))
        assert servo.speed == 70

    def test_servo_zero_forces_stop(self, ctrl: Controller):
        cmds = [ServoSpeedCmd("arm", 80), ServoSpeedCmd("arm", 0)]
        merged = ctrl._merge_commands(cmds)
        servo = next(c for c in merged if isinstance(c, ServoSpeedCmd))
        assert servo.speed == 0


# ── Servo angle merging ──────────────────────────────────────────────


class TestMergeServoAngle:
    def test_single_angle(self, ctrl: Controller):
        merged = ctrl._merge_commands([ServoAngleCmd("pan", 90)])
        assert len(merged) == 1
        assert merged[0] == ServoAngleCmd("pan", 90)

    def test_angles_averaged(self, ctrl: Controller):
        cmds = [ServoAngleCmd("pan", 60), ServoAngleCmd("pan", 120)]
        merged = ctrl._merge_commands(cmds)
        angle = next(c for c in merged if isinstance(c, ServoAngleCmd))
        assert angle.angle == 90

    def test_angle_clamped_180(self, ctrl: Controller):
        """SERVO_180 angles are clamped to 0–180."""
        cmds = [ServoAngleCmd("pan", 200)]
        merged = ctrl._merge_commands(cmds)
        angle = next(c for c in merged if isinstance(c, ServoAngleCmd))
        assert angle.angle == 180

    def test_angle_clamped_270(self, ctrl: Controller):
        """SERVO_270 angles are clamped to 0–270."""
        cmds = [ServoAngleCmd("tilt", 300)]
        merged = ctrl._merge_commands(cmds)
        angle = next(c for c in merged if isinstance(c, ServoAngleCmd))
        assert angle.angle == 270

    def test_angle_not_negative(self, ctrl: Controller):
        cmds = [ServoAngleCmd("pan", -10)]
        merged = ctrl._merge_commands(cmds)
        angle = next(c for c in merged if isinstance(c, ServoAngleCmd))
        assert angle.angle == 0


# ── Mixed merging ─────────────────────────────────────────────────────


class TestMergeMixed:
    def test_empty_input(self, ctrl: Controller):
        assert ctrl._merge_commands([]) == []

    def test_motor_and_servo_independent(self, ctrl: Controller):
        cmds = [MotorCmd("left", 50), ServoSpeedCmd("arm", 30)]
        merged = ctrl._merge_commands(cmds)
        assert len(merged) == 2
        types = {type(c) for c in merged}
        assert types == {MotorCmd, ServoSpeedCmd}

    def test_all_three_types(self, ctrl: Controller):
        cmds = [
            MotorCmd("left", 50),
            ServoSpeedCmd("arm", 30),
            ServoAngleCmd("pan", 90),
        ]
        merged = ctrl._merge_commands(cmds)
        assert len(merged) == 3
        types = {type(c) for c in merged}
        assert types == {MotorCmd, ServoSpeedCmd, ServoAngleCmd}
