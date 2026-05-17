"""Unit tests for BotConfig — hardware registration, packet building, and public API."""

import pytest

from udp_client.bot_config import (
    BotConfig,
    MotorCmd,
    ServoAngleCmd,
    ServoSpeedCmd,
    ServoType,
)

# ── Fixtures ──────────────────────────────────────────────────────────


@pytest.fixture
def cfg() -> BotConfig:
    """A typical two-motor, one-servo config with a few actions."""
    c = BotConfig()
    c.add_motor("left", motor_id=1)
    c.add_motor("right", motor_id=2)
    c.add_servo("arm", channel_id=0, servo_type=ServoType.CONTINUOUS)
    c.add_servo("pan", channel_id=3, servo_type=ServoType.SERVO_180)

    c.add_action("forward", [MotorCmd("left", 80), MotorCmd("right", 80)])
    c.add_action("backward", [MotorCmd("left", -80), MotorCmd("right", -80)])
    c.add_action("turn_left", [MotorCmd("left", -60), MotorCmd("right", 60)])
    c.add_action("stop", [MotorCmd("left", 0), MotorCmd("right", 0)])
    c.add_action("arm_extend", [ServoSpeedCmd("arm", 70)])
    c.add_action("arm_stop", [ServoSpeedCmd("arm", 0)])
    c.add_action("look_center", [ServoAngleCmd("pan", 90)])
    return c


# ── Hardware registration ─────────────────────────────────────────────


class TestAddMotor:
    def test_valid_ids(self):
        c = BotConfig()
        for i in range(1, 5):
            c.add_motor(f"m{i}", motor_id=i)
        assert c.motor_names == ["m1", "m2", "m3", "m4"]

    def test_invalid_id_zero(self):
        with pytest.raises(ValueError, match="1–4"):
            BotConfig().add_motor("x", motor_id=0)

    def test_invalid_id_five(self):
        with pytest.raises(ValueError, match="1–4"):
            BotConfig().add_motor("x", motor_id=5)

    def test_fluent_chaining(self):
        c = BotConfig().add_motor("a", 1).add_motor("b", 2)
        assert c.motor_names == ["a", "b"]


class TestAddServo:
    def test_valid_channels(self):
        c = BotConfig()
        for i in range(6):
            c.add_servo(f"s{i}", channel_id=i)
        assert c.servo_names == [f"s{i}" for i in range(6)]

    def test_invalid_channel(self):
        with pytest.raises(ValueError, match="0–5"):
            BotConfig().add_servo("x", channel_id=6)

    def test_default_type_is_180(self):
        c = BotConfig()
        c.add_servo("s", channel_id=0)
        assert c.get_servo_type("s") == ServoType.SERVO_180

    def test_explicit_type(self):
        c = BotConfig()
        c.add_servo("s", channel_id=0, servo_type=ServoType.CONTINUOUS)
        assert c.get_servo_type("s") == ServoType.CONTINUOUS


# ── Public introspection API ──────────────────────────────────────────


class TestPublicAPI:
    def test_motor_names(self, cfg: BotConfig):
        assert cfg.motor_names == ["left", "right"]

    def test_servo_names(self, cfg: BotConfig):
        assert cfg.servo_names == ["arm", "pan"]

    def test_action_names(self, cfg: BotConfig):
        assert "forward" in cfg.action_names
        assert "stop" in cfg.action_names

    def test_has_motor(self, cfg: BotConfig):
        assert cfg.has_motor("left")
        assert not cfg.has_motor("nonexistent")

    def test_has_servo(self, cfg: BotConfig):
        assert cfg.has_servo("arm")
        assert not cfg.has_servo("nonexistent")

    def test_get_motor_info(self, cfg: BotConfig):
        info = cfg.get_motor_info("left")
        assert info == {"name": "left", "motor_id": 1}

    def test_get_motor_info_missing(self, cfg: BotConfig):
        with pytest.raises(KeyError):
            cfg.get_motor_info("ghost")

    def test_get_servo_info(self, cfg: BotConfig):
        info = cfg.get_servo_info("arm")
        assert info["channel_id"] == 0
        assert info["servo_type"] == ServoType.CONTINUOUS

    def test_get_action_commands_returns_copy(self, cfg: BotConfig):
        cmds = cfg.get_action_commands("forward")
        assert len(cmds) == 2
        # Mutating the copy must not affect the config
        cmds.clear()
        assert len(cfg.get_action_commands("forward")) == 2

    def test_get_action_commands_missing(self, cfg: BotConfig):
        with pytest.raises(KeyError):
            cfg.get_action_commands("nonexistent")

    def test_motors_property(self, cfg: BotConfig):
        assert cfg.motors == {"left": 1, "right": 2}

    def test_servos_property(self, cfg: BotConfig):
        s = cfg.servos
        assert s["arm"]["channel_id"] == 0
        assert s["pan"]["servo_type"] == "SERVO_180"


# ── Packet building ──────────────────────────────────────────────────


class TestBuildPackets:
    """Test binary packet encoding against the MotorServoService protocol."""

    def test_single_motor(self):
        c = BotConfig()
        c.add_motor("m", motor_id=1)
        c.add_action("go", [MotorCmd("m", 100)])
        pkts = c.build_packets("go")
        assert len(pkts) == 1
        # 0x21, mask=0x01 (motor 1), speed=100 (0x64)
        assert pkts[0] == bytes([0x21, 0x01, 0x64])

    def test_two_motors_same_speed_combined(self, cfg: BotConfig):
        """Motors with the same speed should share a single packet."""
        pkts = cfg.build_packets("forward")  # left=80, right=80
        motor_pkts = [p for p in pkts if p[0] == 0x21]
        assert len(motor_pkts) == 1
        # mask should be motor 1 | motor 2 = 0x03
        assert motor_pkts[0][1] == 0x03
        # speed 80 = 0x50
        assert motor_pkts[0][2] == 80

    def test_two_motors_different_speed(self, cfg: BotConfig):
        """Motors with different speeds need separate packets."""
        pkts = cfg.build_packets("turn_left")  # left=-60, right=60
        motor_pkts = [p for p in pkts if p[0] == 0x21]
        assert len(motor_pkts) == 2
        # Each packet targets exactly one motor
        masks = {p[1] for p in motor_pkts}
        assert masks == {0x01, 0x02}

    def test_negative_speed_twos_complement(self):
        """Negative speeds should be encoded as unsigned bytes (two's complement)."""
        c = BotConfig()
        c.add_motor("m", motor_id=1)
        c.add_action("rev", [MotorCmd("m", -100)])
        pkts = c.build_packets("rev")
        # -100 & 0xFF = 156 = 0x9C
        assert pkts[0][2] == (-100 & 0xFF)

    def test_servo_speed_packet(self, cfg: BotConfig):
        pkts = cfg.build_packets("arm_extend")
        servo_pkts = [p for p in pkts if p[0] == 0x23]
        assert len(servo_pkts) == 1
        # channel 0 → mask 0x01, speed 70 = 0x46
        assert servo_pkts[0] == bytes([0x23, 0x01, 70])

    def test_servo_angle_packet(self, cfg: BotConfig):
        pkts = cfg.build_packets("look_center")
        angle_pkts = [p for p in pkts if p[0] == 0x24]
        assert len(angle_pkts) == 1
        # channel 3 → mask 0x08, angle 90 → big-endian 0x00 0x5A
        assert angle_pkts[0] == bytes([0x24, 0x08, 0x00, 0x5A])

    def test_servo_angle_large(self):
        """Angle > 255 exercises the big-endian u16 encoding."""
        c = BotConfig()
        c.add_servo("s", channel_id=1, servo_type=ServoType.SERVO_270)
        c.add_action("wide", [ServoAngleCmd("s", 270)])
        pkts = c.build_packets("wide")
        angle_pkts = [p for p in pkts if p[0] == 0x24]
        # 270 = 0x010E → hi=0x01, lo=0x0E
        assert angle_pkts[0] == bytes([0x24, 0x02, 0x01, 0x0E])

    def test_stop_all_packet(self, cfg: BotConfig):
        pkt = cfg.build_stop_all_packet()
        assert pkt == bytes([0x28])

    def test_unknown_action_raises(self, cfg: BotConfig):
        with pytest.raises(KeyError, match="Unknown action"):
            cfg.build_packets("nonexistent")

    def test_unknown_motor_in_action_skipped(self):
        """A command referencing a missing motor is silently skipped."""
        c = BotConfig()
        c.add_action("bad", [MotorCmd("ghost", 50)])
        pkts = c.build_packets("bad")
        assert pkts == []

    def test_mixed_motor_and_servo(self):
        """An action with both motor and servo commands produces separate packets."""
        c = BotConfig()
        c.add_motor("m", motor_id=1)
        c.add_servo("s", channel_id=0, servo_type=ServoType.CONTINUOUS)
        c.add_action("combo", [MotorCmd("m", 50), ServoSpeedCmd("s", 30)])
        pkts = c.build_packets("combo")
        assert len(pkts) == 2
        opcodes = {p[0] for p in pkts}
        assert opcodes == {0x21, 0x23}

    def test_servo_type_packet(self, cfg: BotConfig):
        pkt = cfg.build_servo_type_packet("arm")
        # 0x22, mask=0x01, type=2 (CONTINUOUS)
        assert pkt == bytes([0x22, 0x01, 0x02])

    def test_build_packets_from_cmds_direct(self, cfg: BotConfig):
        """build_packets_from_cmds works without a named action."""
        cmds = [MotorCmd("left", 50), MotorCmd("right", 50)]
        pkts = cfg.build_packets_from_cmds(cmds)
        assert len(pkts) == 1
        assert pkts[0][0] == 0x21

    def test_zero_speed_motor(self, cfg: BotConfig):
        pkts = cfg.build_packets("stop")
        motor_pkts = [p for p in pkts if p[0] == 0x21]
        assert len(motor_pkts) == 1
        # Both motors at speed 0 → combined mask 0x03, speed 0x00
        assert motor_pkts[0] == bytes([0x21, 0x03, 0x00])
