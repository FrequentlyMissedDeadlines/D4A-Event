/**
 * @file MotorServoService.h
 * @brief Motor and servo control bridge between BotMessageHandler and DFR1216Board.
 *
 * @details Provides high-level, multi-channel motor and servo control with:
 *   - Per-servo type tracking (180°, 270°, continuous)
 *   - State caching for speeds and angles
 *   - Binary bot-protocol dispatch via IsBotActionHandlerInterface
 *
 * ## Binary protocol — service_id: MotorServoConsts::BOT_SERVICE_ID (0x02)
 *
 * Channel selection uses a **single bitmask byte** instead of a variable-length
 * ID list.  Each bit selects one channel:
 *   - `motor_mask` — bits 0–3 map to motors 1–4  (bit 0 = motor 1, …, bit 3 = motor 4)
 *   - `servo_mask` — bits 0–5 map to servo channels 0–5
 * A single value is applied to every selected channel.
 * In GET responses values are returned in ascending bit order (LSB first).
 *
 * | Action | Cmd  | Payload                                                    | Reply payload                              |
 * |--------|------|------------------------------------------------------------|--------------------------------------------||
 * | 0x21   | 0x01 | [motor_mask:u8][speed:i8 −100..+100]                       | —                                          |
 * | 0x22   | 0x02 | [servo_mask:u8][type:u8 0=180°,1=270°,2=continuous]        | —                                          |
 * | 0x23   | 0x03 | [servo_mask:u8][speed:i8 −100..+100]                       | —                                          |
 * | 0x24   | 0x04 | [servo_mask:u8][angle_hi:u8][angle_lo:u8] (big-endian i16) | —                                          |
 * | 0x25   | 0x05 | [servo_mask:u8][delta_hi:u8][delta_lo:u8] (big-endian i16) | —                                          |
 * | 0x26   | 0x06 | [motor_mask:u8]                                            | [motor_mask:u8][speed₀..speedₙ:i8]        |
 * | 0x27   | 0x07 | [servo_mask:u8]                                            | [servo_mask:u8][ang₀_hi][ang₀_lo]… (i16)  |
 * | 0x28   | 0x08 | (none)                                                     | —                                          |
 * | 0x29   | 0x09 | (none)                                                     | [level:u8  0–100]                          |
 */
#pragma once

#include <pgmspace.h>
#include <cstdint>
#include <string>
#include "IsServiceInterface.h"
#include "BotCommunication/BotMessageHandler.h"
#include "services/DFR1216.h"

// ---------------------------------------------------------------------------
// PROGMEM string constants
// ---------------------------------------------------------------------------
namespace MotorServoConsts
{
    constexpr const char str_service_name[]        PROGMEM = "MotorServo Service";
    constexpr const char msg_board_not_started[]   PROGMEM = "MotorServoService: DFR1216Board not started";
    constexpr const char msg_stopped_all_motors[]  PROGMEM = "MotorServoService: all motors stopped";
    constexpr const char msg_invalid_motor_id[]    PROGMEM = "MotorServoService: invalid motor id (1-4)";
    constexpr const char msg_invalid_servo_id[]    PROGMEM = "MotorServoService: invalid servo id (0-5)";
    constexpr const char msg_invalid_speed[]       PROGMEM = "MotorServoService: speed out of range (-100..+100)";
    constexpr const char msg_invalid_angle[]       PROGMEM = "MotorServoService: angle out of range (-360..+360)";
    constexpr const char msg_invalid_servo_type[]  PROGMEM = "MotorServoService: invalid servo type (0-2)";
    constexpr const char msg_wrong_servo_type[]    PROGMEM = "MotorServoService: wrong servo type for operation";

    constexpr uint8_t  MOTOR_COUNT     = 4;     ///< Motors 1–4 (1-indexed in the API)
    constexpr uint8_t  SERVO_COUNT     = 6;     ///< Servo channels 0–5 (0-indexed in the API)
    constexpr int8_t   SPEED_MIN       = -100;
    constexpr int8_t   SPEED_MAX       =  100;
    constexpr int16_t  ANGLE_MIN       = -360;
    constexpr int16_t  ANGLE_MAX       =  360;

    /// Bitmask covering all 4 motors (bits 0–3).
    constexpr uint8_t  MOTOR_MASK_ALL  = 0x0F;
    /// Bitmask covering all 6 servo channels (bits 0–5).
    constexpr uint8_t  SERVO_MASK_ALL  = 0x3F;

    /// Unique service identifier for the BotMessageHandler registry (high nibble of action byte).
    constexpr uint8_t  BOT_SERVICE_ID  = 0x02;

    // Command identifiers (low nibble of action byte)
    constexpr uint8_t  CMD_SET_MOTORS_SPEED       = 0x01; ///< Set same speed on multiple motors
    constexpr uint8_t  CMD_SET_SERVO_TYPE         = 0x02; ///< Configure servo channel type
    constexpr uint8_t  CMD_SET_SERVOS_SPEED       = 0x03; ///< Set speed on multiple continuous servos
    constexpr uint8_t  CMD_SET_SERVOS_ANGLE       = 0x04; ///< Set absolute angle on positional servos
    constexpr uint8_t  CMD_INCREMENT_SERVOS_ANGLE = 0x05; ///< Increment angle on positional servos
    constexpr uint8_t  CMD_GET_MOTORS_SPEED       = 0x06; ///< Query cached motor speeds
    constexpr uint8_t  CMD_GET_SERVOS_ANGLE       = 0x07; ///< Query cached servo angles
    constexpr uint8_t  CMD_STOP_ALL_MOTORS        = 0x08; ///< Emergency-stop all motors
    constexpr uint8_t  CMD_GET_BATTERY            = 0x09; ///< Read board battery level (0–100 %)
    constexpr uint8_t  CMD_SET_SERVO270_ANGLE     = 0x0A; ///< Set calibrated 270° servo angle (uses SERVO270_CAL_MIN/MAX_US)
} // namespace MotorServoConsts

// ---------------------------------------------------------------------------
// ServoType
// ---------------------------------------------------------------------------
/**
 * @brief Operating mode of a servo channel.
 *
 * @details This value is cached per-channel in MotorServoService and drives
 *          the correct DFR1216Board call (setServoAngle vs. setServo360).
 *          Default after init is SERVO_180.
 */
enum class ServoType : uint8_t
{
    SERVO_180  = 0,  ///< Standard 180° positional servo
    SERVO_270  = 1,  ///< Wide-angle 270° positional servo
    CONTINUOUS = 2,  ///< Continuous-rotation (360°) servo — use setServosSpeed()
};

// ---------------------------------------------------------------------------
// MotorServoService
// ---------------------------------------------------------------------------
/**
 * @class MotorServoService
 * @brief Bridge between BotMessageHandler binary protocol and DFR1216Board.
 *
 * @details Owns all servo-type configuration and speed/angle state caches.
 *          The DFR1216Board reference is used exclusively for hardware writes;
 *          no other service state is modified.
 *
 * Typical usage:
 * @code
 *   DFR1216Board   board;
 *   MotorServoService motor_servo(board);
 *
 *   // Startup (after board is started)
 *   motor_servo.setDebugLogger(&debug_logger);
 *   motor_servo.initializeService();
 *   motor_servo.startService();
 *
 *   // Configure servo 2 as 270° before use (bit 2 = servo 2)
 *   motor_servo.setServoTypes(0x04, ServoType::SERVO_270);
 *
 *   // Drive motors 0 and 2 (bits 0 and 2 of motor_mask) at 75%
 *   motor_servo.setMotorsSpeed(0x05, 75);
 * @endcode
 */
class MotorServoService : public IsServiceInterface,
                          public IsBotActionHandlerInterface
{
public:
    /**
     * @brief Construct with a reference to the DFR1216Board service.
     * @param board Board instance whose hardware methods will be called.
     *              Must be started before MotorServoService::startService().
     */
    explicit MotorServoService();

    // ---- IsServiceInterface ------------------------------------------

    /** @return "MotorServo Service" */
    std::string getServiceName() override;

    /**
     * @brief Reset all state caches (speeds = 0, angles = 0, types = SERVO_180).
     * @return Always true.
     */
    bool initializeService() override;

    /**
     * @brief Verify that the DFR1216Board is running, then mark this service started.
     * @return true on success; false (START_FAILED) if the board is not yet started.
     */
    bool startService() override;

    /**
     * @brief Stop all motors then mark service stopped.
     * @return Always true.
     */
    bool stopService() override;

    // ---- IsBotActionHandlerInterface ---------------------------------

    /** @return MotorServoConsts::BOT_SERVICE_ID (0x02) */
    uint8_t getBotServiceId() const override;

    /**
     * @brief Dispatch a binary bot frame to the appropriate motor/servo command.
     * @param data Raw frame; byte[0] is the action byte (service_id << 4 | cmd)
     * @param len  Frame length in bytes
     * @return Binary response string, or empty string if no reply is needed
     */
    std::string handleBotMessage(const uint8_t *data, size_t len) override;

    // ---- Motor API ---------------------------------------------------

    /**
     * @brief Apply the same speed to one or more motors.
     * @param motor_mask Bitmask selecting motors (bit N → motor N; bits 0–3 = motors 0–3)
     * @param speed      Speed percentage (−100 to +100; 0 = brake/stop)
     * @return BotProto::resp_* status code
     */
    uint8_t setMotorsSpeed(uint8_t motor_mask, int8_t speed);

    /**
     * @brief Stop all four motors immediately (sets speed = 0 on all channels).
     * @return BotProto::resp_ok
     */
    uint8_t stopAllMotors();

    /**
     * @brief Read cached speed values for one or more motors.
     * @param motor_mask Bitmask selecting motors (bit N → motor N; bits 0–3 = motors 0–3)
     * @param out        Caller-provided buffer; must hold ≥ __builtin_popcount(motor_mask) values
     * @return BotProto::resp_* status code; on success, out[] is populated in ascending bit order
     */
    uint8_t getMotorSpeeds(uint8_t motor_mask, int8_t *out) const;

    // ---- Servo API ---------------------------------------------------

    /**
     * @brief Configure the operating type for one or more servo channels.
     *        Changing the type does not move the servo; it only affects subsequent
     *        setServosAngle / setServosSpeed calls.
     * @param servo_mask Bitmask selecting servo channels (bit N → servo N; bits 0–5 = servos 0–5)
     * @param type       ServoType (SERVO_180, SERVO_270, CONTINUOUS)
     * @return BotProto::resp_* status code
     */
    uint8_t setServoTypes(uint8_t servo_mask, ServoType type);

    /**
     * @brief Set speed for one or more continuous-rotation servo channels.
     *        Returns resp_invalid_params if any targeted channel is not CONTINUOUS.
     * @param servo_mask Bitmask selecting servo channels (bit N → servo N; bits 0–5 = servos 0–5)
     * @param speed      Speed percentage (−100 = full reverse, 0 = stop, +100 = full forward)
     * @return BotProto::resp_* status code
     */
    uint8_t setServosSpeed(uint8_t servo_mask, int8_t speed);

    /**
     * @brief Set the same absolute angle for one or more positional servo channels.
     *        SERVO_180 clamps to [0, 180].  SERVO_270 clamps to [0, 270].
     *        Returns resp_invalid_params if any targeted channel is CONTINUOUS.
     * @param servo_mask Bitmask selecting servo channels (bit N → servo N; bits 0–5 = servos 0–5)
     * @param angle      Target angle (−360..+360 accepted; clamped to servo's valid range)
     * @return BotProto::resp_* status code
     */
    uint8_t setServosAngle(uint8_t servo_mask, int16_t angle);

    /**
     * @brief Set the same absolute angle for one or more 270° servo channels using
     *        calibrated PWM endpoints (SERVO270_CAL_MIN_US / SERVO270_CAL_MAX_US).
     *        Provides accurate 1:1 degree mapping. Works on any non-CONTINUOUS channel.
     * @param servo_mask Bitmask selecting servo channels (bit N → servo N; bits 0–5 = servos 0–5)
     * @param angle      Target angle 0–270°
     * @return BotProto::resp_* status code
     */
    uint8_t setServos270Angle(uint8_t servo_mask, uint16_t angle);


    
    /**
     * @brief Increment the current angle of one or more positional servo channels.
     *        The resulting angle is clamped to the servo's valid range.
     *        Returns resp_invalid_params if any targeted channel is CONTINUOUS.
     * @param servo_mask Bitmask selecting servo channels (bit N → servo N; bits 0–5 = servos 0–5)
     * @param delta      Angle increment (−360..+360)
     * @return BotProto::resp_* status code
     */
    uint8_t incrementServosAngle(uint8_t servo_mask, int16_t delta);

    /**
     * @brief Read cached angle values for one or more servo channels.
     *        For CONTINUOUS channels the cached value is the last applied speed (int16).
     * @param servo_mask Bitmask selecting servo channels (bit N → servo N; bits 0–5 = servos 0–5)
     * @param out        Caller-provided buffer; must hold ≥ __builtin_popcount(servo_mask) values
     * @return BotProto::resp_* status code; on success, out[] is populated in ascending bit order
     */
    uint8_t getServosAngle(uint8_t servo_mask, int16_t *out) const;

    /**
     * @brief Return the configured type for one servo channel.
     * @param servo_id 0-based servo channel (0–5)
     * @return ServoType (SERVO_180 if servo_id is out of range)
     */
    ServoType getServoType(uint8_t servo_id) const;

private:

    // ---- State caches ------------------------------------------------
    int8_t    motor_speeds_[MotorServoConsts::MOTOR_COUNT];  ///< Index = motor_id - 1
    int16_t   servo_angles_[MotorServoConsts::SERVO_COUNT];  ///< Index = servo_id
    ServoType servo_types_ [MotorServoConsts::SERVO_COUNT];  ///< Index = servo_id

    // ---- Private helpers ---------------------------------------------

    /**
     * @brief Write speed to one motor via DFR1216Board duty cycle and update cache.
     *        Properly zeros the opposing direction register to avoid H-bridge conflicts.
     * @param motor_id 1-based motor ID (1–4)
     * @param speed    Speed percentage (−100 to +100)
     */
    void applyMotorSpeed(uint8_t motor_id, int8_t speed);

    /**
     * @brief Write an absolute angle to one positional servo and update cache.
     * @param servo_id 0-based servo channel (0–5); must not be CONTINUOUS type
     * @param angle    Already-clamped non-negative angle value
     */
    void applyServoAngle(uint8_t servo_id, int16_t angle);

    /**
     * @brief Write speed to one continuous servo and update cache.
     * @param servo_id 0-based servo channel (0–5); must be CONTINUOUS type
     * @param speed    Speed percentage (−100 to +100)
     */
    void applyServoSpeed(uint8_t servo_id, int8_t speed);

    /**
     * @brief Write a calibrated 270° angle to one servo via board.setServo270() and update cache.
     * @param servo_id 0-based servo channel (0–5)
     * @param angle    Already-validated angle 0–270
     */
    void applyServo270Angle(uint8_t servo_id, uint16_t angle);

    /**
     * @brief Clamp an angle to the physical range of the given servo type.
     * @param type  Servo operating type (SERVO_180 → [0,180]; SERVO_270 → [0,270])
     * @param angle Raw angle value (may be negative or exceed maximum)
     * @return Clamped angle in [0, type_max]
     */
    static int16_t clampToServoRange(ServoType type, int16_t angle);
};
