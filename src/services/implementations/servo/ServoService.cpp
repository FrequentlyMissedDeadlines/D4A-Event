// Servo controller handler for DFR0548 board
/**
 * @file ServoService.cpp
 * @brief Implementation for servo controller integration with the main application
 * @details Exposed routes:
 *          - POST /api/servos/v1/setServoAngle - Set servo angle for angular servos (180° or 270°)
 *          - POST /api/servos/v1/setServoSpeed - Set continuous servo speed for rotational servos
 *          - POST /api/servos/v1/stopAll - Stop all servos by setting speed to 0
 *          - GET /api/servos/v1/getStatus - Get servo type and connection status for a specific channel
 *          - GET /api/servos/v1/getAllStatus - Get connection status and type for all 8 servo channels
 *          - POST /api/servos/v1/attachServo - Register a servo type to a channel before use
 *          - POST /api/servos/v1/setAllServoAngle - Set all attached angular servos to the same angle
 *          - POST /api/servos/v1/setAllServoSpeed - Set all attached continuous rotation servos to the same speed
 *          - POST /api/servos/v1/setServosSpeedMultiple - Set speed for multiple servos at once
 *          - POST /api/servos/v1/setServosAngleMultiple - Set angle for multiple servos at once
 *          - POST /api/servos/v1/setMotorSpeed - Set DC motor speed (motors 1-4)
 *          - POST /api/servos/v1/stopAllMotors - Stop all DC motors
 *          - POST /api/servos/v1/setAllMotorsSpeed - Set same speed on all DC motors
 *          - GET /api/servos/v1/getBattery - Get K10 board battery level (0-100%)
 *
 */

#include "services/ServoService.h"
#include "ResponseHelper.h"
#include <ESPAsyncWebServer.h>
#include <pgmspace.h>
#include <ArduinoJson.h>
#include <freertos/timers.h>
#include "DFR1216/DFR1216.h"
#include "services/SettingsService.h"
#include "services/UDPService.h"
#include "services/AmakerBotService.h"

constexpr uint8_t MAX_SERVO_CHANNELS = 8;
constexpr uint8_t MAX_MOTOR_CHANNELS = 4;
constexpr uint16_t MOTOR_PWM_PERIOD = 1000; ///< PWM period (µs) for DC motors — 1 kHz

DFR1216_I2C servoController = DFR1216_I2C();

extern SettingsService settings_service;
extern UDPService udp_service;
extern AmakerBotService amakerbot_service;

// No module-level UDP buffers needed — binary protocol uses raw message bytes directly.

// Servo Service constants (stored in PROGMEM to save RAM)
namespace ServoConsts
{
    constexpr const char action_attach_servo[] PROGMEM = "attachServo";
    constexpr const char action_get_all_status[] PROGMEM = "getAllStatus";
    constexpr const char action_get_status[] PROGMEM = "getStatus";
    constexpr const char action_set_all_angle[] PROGMEM = "setAllServoAngle";
    constexpr const char action_set_all_speed[] PROGMEM = "setAllServoSpeed";
    constexpr const char action_set_angle[] PROGMEM = "setServoAngle";
    constexpr const char action_set_servos_angle_multiple[] PROGMEM = "setServosAngleMultiple";
    constexpr const char action_set_servos_speed_multiple[] PROGMEM = "setServosSpeedMultiple";
    constexpr const char action_set_speed[] PROGMEM = "setServoSpeed";
    constexpr const char action_stop_all[] PROGMEM = "stopAll";
    constexpr const char connection[] PROGMEM = "connection";
    constexpr const char msg_failed_action[] PROGMEM = "Servo controller action failed.";
    constexpr const char msg_initializing[] PROGMEM = "Initializing Servo Service...";
    constexpr const char msg_initialized_success[] PROGMEM = "Servo controller initialized successfully.";
    constexpr const char msg_issue_detected[] PROGMEM = "Servo controller issue detected.";
    constexpr const char msg_not_initialized[] PROGMEM = "Servo controller not initialized.";
    constexpr const char msg_start_failed[] PROGMEM = " start failed";
    constexpr const char path_servo[] PROGMEM = "servo/";
    constexpr const char servo_angle[] PROGMEM = "angle";
    constexpr const char servo_channel[] PROGMEM = "channel";
    constexpr const char servo_duration_ms[] PROGMEM = "duration_ms";  ///< Optional auto-stop delay field
    constexpr const char servo_speed[] PROGMEM = "speed";
    constexpr const char servos[] PROGMEM = "servos";
    constexpr const char msg_no_saved_settings[] PROGMEM = "No saved servo settings found.";
    constexpr const char msg_loaded_settings[] PROGMEM = "Loaded servo settings successfully.";
    constexpr const char str_service_name[] PROGMEM = "Servo Service";
    constexpr const char path_service[] PROGMEM = "servos/v1";
    constexpr const char str_plain[] PROGMEM = "plain";
    constexpr const char str_comma[] PROGMEM = ",";

    // Error messages for exceptions
    constexpr const char err_channel_range[] PROGMEM = "Channel out of range";
    constexpr const char err_angle_range_180[] PROGMEM = "Angle out of range for 180° servo";
    constexpr const char err_angle_range_270[] PROGMEM = "Angle out of range for 270° servo";
    constexpr const char err_servo_not_attached[] PROGMEM = "Servo not attached on channel";
    constexpr const char err_servo_not_continuous[] PROGMEM = "Servo not continuous on channel";
    constexpr const char err_speed_range[] PROGMEM = "Speed out of range";

    // JSON keys and status strings
    constexpr const char json_attached_servos[] PROGMEM = "attached_servos";
    constexpr const char status_not_connected[] PROGMEM = "Not Connected";
    constexpr const char status_rotational[] PROGMEM = "Rotational";
    constexpr const char status_angular_180[] PROGMEM = "Angular 180";
    constexpr const char status_angular_270[] PROGMEM = "Angular 270";
    constexpr const char status_unknown[] PROGMEM = "Unknown";
    constexpr const char settings_key_servos[] PROGMEM = "attached_servos";
    constexpr const char tag_servo[] PROGMEM = "Servo";
    constexpr const char tag[] PROGMEM = "servo";
    // OpenAPI tags and descriptions
    constexpr const char tag_servos[] PROGMEM = "Servos";
    constexpr const char desc_servo_channel[] PROGMEM = "Servo channel (0-7)";
    constexpr const char desc_angle_degrees[] PROGMEM = "Angle in degrees (0-180 for 180° servos, 0-270 for 270° servos)";
    constexpr const char desc_angle_degrees_360[] PROGMEM = "Angle in degrees (0-360)";
    constexpr const char desc_speed_percent[] PROGMEM = "Speed percentage (-100 to +100, negative is reverse)";

    constexpr const char desc_connection_type[] PROGMEM = "Servo connection type (0=None, 1=continuous, 2=angular 180 degree, 3=angular 270 degrees)";
    constexpr const char desc_servo_angle_control[] PROGMEM = "Servo angle control";
    constexpr const char desc_servo_speed_control[] PROGMEM = "Servo speed control";
    constexpr const char desc_angle_for_all[] PROGMEM = "Angle for all servos";
    constexpr const char desc_speed_for_all[] PROGMEM = "Speed for all servos";
    constexpr const char desc_attachment_config[] PROGMEM = "Servo attachment configuration";
    constexpr const char desc_status_retrieved[] PROGMEM = "Servo status retrieved";
    constexpr const char desc_all_status_retrieved[] PROGMEM = "All servos status retrieved";
    constexpr const char desc_set_angle[] PROGMEM = "Set servo angle for angular servos (180° or 270°)";
    constexpr const char desc_set_speed[] PROGMEM = "Set continuous servo speed for rotational servos";
    constexpr const char desc_stop_all[] PROGMEM = "Stop all servos by setting speed to 0";
    constexpr const char desc_get_status[] PROGMEM = "Get servo type and connection status for a specific channel";
    constexpr const char desc_get_all_status[] PROGMEM = "Get connection status and type for all 8 servo channels";
    constexpr const char desc_set_all_angle[] PROGMEM = "Set all attached angular servos to the same angle simultaneously";
    constexpr const char desc_set_all_speed[] PROGMEM = "Set all attached continuous rotation servos to the same speed simultaneously";
    constexpr const char desc_set_servos_speed_multiple[] PROGMEM = "Set speed for multiple servos at once";
    constexpr const char desc_set_servos_angle_multiple[] PROGMEM = "Set angle for multiple servos at once";
    constexpr const char desc_attach_servo[] PROGMEM = "Register a servo type to a channel before use";

    // OpenAPI parameter names
    constexpr const char param_channel[] PROGMEM = "channel";
    constexpr const char param_angle[] PROGMEM = "angle";
    constexpr const char param_speed[] PROGMEM = "speed";

    // OpenAPI schemas
    constexpr const char schema_channel_status[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"channel\":{\"type\":\"integer\"},\"status\":{\"type\":\"string\"}}}";
    constexpr const char schema_all_servos[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"servos\":{\"type\":\"array\",\"items\":{\"type\":\"object\"}}}}";
    constexpr const char req_channel_angle_07[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"channel\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":7},\"angle\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":360}},\"required\":[\"channel\",\"angle\"]}";
    constexpr const char req_channel_speed[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"channel\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":7},\"speed\":{\"type\":\"integer\",\"minimum\":-100,\"maximum\":100},\"duration_ms\":{\"type\":\"integer\",\"minimum\":1,\"description\":\"Auto-stop delay in ms (optional)\"}},\"required\":[\"channel\",\"speed\"]}";
    constexpr const char req_angle[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"angle\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":360}},\"required\":[\"angle\"]}";
    constexpr const char req_speed[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"speed\":{\"type\":\"integer\",\"minimum\":-100,\"maximum\":100},\"duration_ms\":{\"type\":\"integer\",\"minimum\":1,\"description\":\"Auto-stop delay in ms (optional)\"}},\"required\":[\"speed\"]}";
    constexpr const char req_channel_connection[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"channel\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":7},\"connection\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":3,\"description\":\"0=None, 1=continuous, 2=angular 180 degree, 3=angular 270 degrees\"}},\"required\":[\"channel\",\"connection\"]}";
    constexpr const char req_servos_speed_multiple[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"servos\":{\"type\":\"array\",\"items\":{\"type\":\"object\",\"properties\":{\"channel\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":7},\"speed\":{\"type\":\"integer\",\"minimum\":-100,\"maximum\":100},\"duration_ms\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"channel\",\"speed\"]}}},\"required\":[\"servos\"]}";
    constexpr const char req_servos_angle_multiple[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"servos\":{\"type\":\"array\",\"items\":{\"type\":\"object\",\"properties\":{\"channel\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":7},\"angle\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":360}},\"required\":[\"channel\",\"angle\"]}}},\"required\":[\"servos\"]}";

    // Example values
    constexpr const char ex_channel_angle[] PROGMEM = "{\"channel\":0,\"angle\":90}";
    constexpr const char ex_channel_speed[] PROGMEM = "{\"channel\":0,\"speed\":50,\"duration_ms\":500}";
    constexpr const char ex_channel_status[] PROGMEM = "{\"channel\":0,\"status\":\"ANGULAR_180\"}";
    constexpr const char ex_all_servos[] PROGMEM = "{\"servos\":[{\"channel\":0,\"status\":\"ANGULAR_180\"},{\"channel\":1,\"status\":\"NOT_CONNECTED\"}]}";
    constexpr const char ex_angle[] PROGMEM = "{\"angle\":90}";
    constexpr const char ex_speed[] PROGMEM = "{\"speed\":50}";
    constexpr const char ex_channel_connection[] PROGMEM = "{\"channel\":0,\"connection\":0}";
    constexpr const char ex_servos_speed_multiple[] PROGMEM = "{\"servos\":[{\"channel\":0,\"speed\":50,\"duration_ms\":500},{\"channel\":1,\"speed\":-30}]}";
    constexpr const char ex_servos_angle_multiple[] PROGMEM = "{\"servos\":[{\"channel\":0,\"angle\":90},{\"channel\":1,\"angle\":180}]}";
    constexpr const char ex_result_ok[] PROGMEM = "{\"result\":\"ok\",\"message\":\"setServoAngle\"}";

    // UDP message handling
    constexpr const char msg_udp_bad_format[] PROGMEM = "UDP bad message format";
    constexpr const char msg_udp_unknown_cmd[] PROGMEM = "UDP unknown command";
    constexpr const char msg_udp_json_error[] PROGMEM = "UDP JSON parse error";

    // ── Binary UDP protocol ──────────────────────────────────────────────────
    // Request  shape : [action:1B][dest_mask:1B][value...]
    //   Servo mask   : bit N = servo channel N  (N = 0-7)
    //   Motor mask   : bit N = motor N+1        (N = 0-3, bits 4-7 ignored)
    // Response shape : [action:1B][resp_code:1B][optional_payload]
    // ────────────────────────────────────────────────────────────────────────
    constexpr uint8_t udp_service_id = 0x02; ///<` Unique ID for this service (high nibble of action byte)

    // Action codes (byte 0 of both request and response)
    constexpr uint8_t udp_action_set_servo_angle = (udp_service_id << 4) | 0x01;  ///< [ch0:int16_LE]...[chN:int16_LE]  bit0=0→skip, bit0=1→angle=raw>>1 (centre-zero °)
    constexpr uint8_t udp_action_set_servo_speed = (udp_service_id << 4) | 0x02;  ///< [mask][speed]               int8  -100..+100
    constexpr uint8_t udp_action_stop_servos = (udp_service_id << 4) | 0x03;      ///< [mask]
    constexpr uint8_t udp_action_attach_servo = (udp_service_id << 4) | 0x04;     ///< [mask][type]                uint8 0-3
    constexpr uint8_t udp_action_set_motor_speed = (udp_service_id << 4) | 0x05;  ///< [m0:int16_LE]...[mN:int16_LE]  bit0=0→skip, bit0=1→speed=raw>>1 (int8 −100..+100)
    constexpr uint8_t udp_action_stop_motors = (udp_service_id << 4) | 0x06;      ///< [mask]
    constexpr uint8_t udp_action_get_servo_status = (udp_service_id << 4) | 0x07; ///< [mask]  → [action][ok][JSON]
    constexpr uint8_t udp_action_get_all_status = (udp_service_id << 4) | 0x08;   ///<          → [action][ok][JSON]
    constexpr uint8_t udp_action_get_battery = (udp_service_id << 4) | 0x09;      ///<          → [action][ok][batt_byte]
    constexpr uint8_t udp_action_min = (udp_service_id << 4) | 0x01;              ///< lowest valid action code
    constexpr uint8_t udp_action_max = (udp_service_id << 4) | 0x09;              ///< highest valid action code

    // Motor control
    constexpr const char action_set_motor_speed[] PROGMEM = "setMotorSpeed";
    constexpr const char action_stop_all_motors[] PROGMEM = "stopAllMotors";
    constexpr const char action_set_all_motors_speed[] PROGMEM = "setAllMotorsSpeed";
    constexpr const char motor_channel[] PROGMEM = "motor";
    constexpr const char err_motor_range[] PROGMEM = "Motor out of range (1-4)";
    constexpr const char desc_set_motor_speed[] PROGMEM = "Set DC motor speed (motor 1-4, speed -100 to +100)";
    constexpr const char desc_stop_all_motors[] PROGMEM = "Stop all DC motors";
    constexpr const char desc_set_all_motors_speed[] PROGMEM = "Set the same speed on all DC motors (-100 to +100)";
    constexpr const char req_motor_speed[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"motor\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4},\"speed\":{\"type\":\"integer\",\"minimum\":-100,\"maximum\":100}},\"required\":[\"motor\",\"speed\"]}";
    constexpr const char ex_motor_speed[] PROGMEM = "{\"motor\":1,\"speed\":75}";
    constexpr const char req_all_motors_speed[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"speed\":{\"type\":\"integer\",\"minimum\":-100,\"maximum\":100}},\"required\":[\"speed\"]}";
    constexpr const char ex_all_motors_speed[] PROGMEM = "{\"speed\":50}";

    // Battery route
    constexpr const char action_get_battery[] PROGMEM = "getBattery";
    constexpr const char desc_get_battery[] PROGMEM = "Get K10 board battery level (0-100%)";
    constexpr const char json_battery[] PROGMEM = "battery";
    constexpr const char schema_battery[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"battery\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":100}}}";
    constexpr const char ex_battery[] PROGMEM = "{\"battery\":85}";
}

std::array<ServoConnection, MAX_SERVO_CHANNELS> attached_servos = {NOT_CONNECTED, NOT_CONNECTED, NOT_CONNECTED, NOT_CONNECTED,
                                                                   NOT_CONNECTED, NOT_CONNECTED, NOT_CONNECTED, NOT_CONNECTED};

// Track current servo speeds to avoid redundant updates (initialized to invalid value)
std::array<int8_t, MAX_SERVO_CHANNELS> servo_speeds = {-128, -128, -128, -128, -128, -128, -128, -128};

// Track current motor speeds to avoid redundant updates (initialized to invalid value)
std::array<int8_t, MAX_MOTOR_CHANNELS> motor_speeds = {-128, -128, -128, -128};

// Per-channel one-shot FreeRTOS timers for the optional auto-stop (duration_ms) feature
static TimerHandle_t channel_stop_timers[MAX_SERVO_CHANNELS] = {};
extern ServoService servo_service; // defined in main.cpp

/**
 * @brief FreeRTOS timer callback: stop the servo channel encoded in the timer ID.
 * @note  Called from the FreeRTOS timer service task. The one-shot timer is already
 *        dormant when the callback runs, so calling xTimerStop inside is safe.
 */
static void servo_stop_timer_cb(TimerHandle_t xTimer)
{
    const uint8_t ch = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(pvTimerGetTimerID(xTimer)));
    servo_service.setServoSpeed(ch, 0); // duration_ms=0 → no recursive scheduling
}

bool ServoService::initializeService()
{
    logger->info(progmem_to_string(ServoConsts::msg_initializing));
    if (servoController.begin())
    {
        logger->info(progmem_to_string(ServoConsts::msg_initialized_success));
        setServiceStatus(INITIALIZED);
    }
    else
    {
        logger->warning(progmem_to_string(ServoConsts::msg_issue_detected));
        setServiceStatus(INITIALIZED_FAILED);
    }

    // Create per-channel one-shot timers for the optional timed-stop feature
    for (uint8_t ch = 0; ch < MAX_SERVO_CHANNELS; ++ch)
    {
        channel_stop_timers[ch] = xTimerCreate(
            "svStop",
            pdMS_TO_TICKS(1000),  // initial period is irrelevant; reset before each start
            pdFALSE,              // one-shot (no auto-reload)
            reinterpret_cast<void *>(static_cast<uintptr_t>(ch)),
            servo_stop_timer_cb);
    }

    // Return true to allow other services to continue even if servo fails
    return true;
}

bool ServoService::startService()
{

    if (isServiceInitialized())
    {

#ifdef VERBOSE_DEBUG
        logger->debug(getServiceName() + progmem_to_string(RoutesConsts::str_space) + getStatusString());
#endif
    }
    else
    {
        setServiceStatus(START_FAILED);
        logger->error(getServiceName() + progmem_to_string(ServoConsts::msg_start_failed));
        return false;
    }
    servoController.setMotorPeriod(eMotor1_2, MOTOR_PWM_PERIOD);
    servoController.setMotorPeriod(eMotor3_4, MOTOR_PWM_PERIOD);
    setServiceStatus(STARTED);
    return true;
}

bool ServoService::attachServo(uint8_t channel, ServoConnection connection)
{
    if (!isServiceStarted())
    {
        return false;
    }
    try
    {
        if (channel >= MAX_SERVO_CHANNELS)
        {
            throw std::out_of_range(progmem_to_string(ServoConsts::err_channel_range));
        }
        attached_servos[channel] = connection;

        return true;
    }
    catch (const std::exception &e)
    {
        logger->error(std::string(e.what()));
        return false;
    }
}
// Set servo angle (wrapper for easier integration)
bool ServoService::setServoAngle(uint8_t channel, uint16_t angle)
{
    if (!isServiceStarted())
    {
        return false;
    }
#ifdef SERVO_VERBOSE_DEBUG
    logger->debug("setServoAngle #" + std::to_string(channel) + ": " + std::to_string(angle));
#endif
    try
    {
        if (channel >= MAX_SERVO_CHANNELS)
        {
            throw std::out_of_range(reinterpret_cast<const char *>(FPSTR(ServoConsts::err_channel_range)));
        }
        if (attached_servos[channel] == ServoConnection::ANGULAR_180)
        {
            if (angle > 180)
            {
                throw std::out_of_range(reinterpret_cast<const char *>(FPSTR(ServoConsts::err_angle_range_180)));
            }
            servoController.setServoAngle(eServoNumber_t(channel), angle);
            return true;
        }
        else if (attached_servos[channel] == ServoConnection::ANGULAR_270)
        {
            if (angle > 270)
            {
                throw std::out_of_range(reinterpret_cast<const char *>(FPSTR(ServoConsts::err_angle_range_270)));
            }
            servoController.setServoAngle(eServoNumber_t(channel), angle);
            return true;
        }
        else
        {
            throw std::runtime_error(reinterpret_cast<const char *>(FPSTR(ServoConsts::err_servo_not_attached)));
        }
    }
    catch (const std::exception &e)
    {
        logger->error(e.what());
        return false;
    }
}

// Set servo speed for continuous rotation
bool ServoService::setServoSpeed(uint8_t channel, int8_t speed, uint32_t duration_ms)
{
    if (!isServiceStarted())
    {
        return false;
    }
#ifdef SERVO_VERBOSE_DEBUG
    logger->debug("setServoSpeed #" + std::to_string(channel) + ": " + std::to_string(speed));
#endif

    try
    {
        if (channel >= MAX_SERVO_CHANNELS)
        {
            throw std::out_of_range(reinterpret_cast<const char *>(FPSTR(ServoConsts::err_channel_range)));
        }
        if (attached_servos[channel] != ServoConnection::ROTATIONAL)
        {
            throw std::runtime_error(reinterpret_cast<const char *>(FPSTR(ServoConsts::err_servo_not_continuous)));
        }
        if (speed < -100 || speed > 100)
        {
            throw std::out_of_range(reinterpret_cast<const char *>(FPSTR(ServoConsts::err_speed_range)));
        }
        servoController.setServo360(eServoNumber_t(channel),
                                    speed > 0 ? eServo360Direction_t::eForward
                                              : (speed < 0 ? eServo360Direction_t::eBackward
                                                           : eServo360Direction_t::eStop),
                                    static_cast<uint8_t>(std::abs(speed)));
        // Schedule an auto-stop if requested; cancel any pending stop when speed == 0
        scheduleChannelStop(channel, (speed != 0) ? duration_ms : 0);
        return true;
    }
    catch (const std::exception &e)
    {
        logger->error(e.what());
        return false;
    }
}

bool ServoService::setAllServoSpeed(int8_t speed, uint32_t duration_ms)
{
#ifdef SERVO_VERBOSE_DEBUG
    logger->debug("setAllServoSpeed " + std::to_string(speed));
#endif

    bool allSuccess = true;
    for (uint8_t channel = 0; channel < MAX_SERVO_CHANNELS; channel++)
    {
#ifdef SERVO_VERBOSE_DEBUG
        logger->debug("  channel " + std::to_string(channel) + ": " + std::string(attached_servos[channel] == ServoConnection::ROTATIONAL ? "ROTATIONAL" : "NOT ROTATIONAL"));
#endif
        if (attached_servos[channel] == ServoConnection::ROTATIONAL)
        {
            allSuccess = allSuccess && this->setServoSpeed(channel, speed, duration_ms);
        }
    }
    return allSuccess;
}

bool ServoService::setAllServoAngle(u_int16_t angle)
{
#ifdef SERVO_VERBOSE_DEBUG
    logger->debug("setAllServoAngle " + std::to_string(angle));
#endif
    bool allSuccess = true;
    for (uint8_t channel = 0; channel < MAX_SERVO_CHANNELS; channel++)
    {
        if (attached_servos[channel] == ServoConnection::ANGULAR_180 || attached_servos[channel] == ServoConnection::ANGULAR_270)
        {
            allSuccess = allSuccess && this->setServoAngle(channel, angle);
        }
    }
    return allSuccess;
}

/**
 * @brief Set speed for multiple servos at once
 * @param ops Vector of ServoSpeedOp operations
 * @return true if all operations successful, false otherwise
 */
bool ServoService::setServosSpeedMultiple(const std::vector<ServoSpeedOp> &ops)
{
    if (!isServiceStarted() || ops.empty())
        return false;
#ifdef SERVO_VERBOSE_DEBUG
    logger->debug("setServosSpeedMultiple " + std::to_string(ops.size()) + " ops");
#endif
    bool all_success = true;
    for (const auto &op : ops)
    {
#ifdef SERVO_VERBOSE_DEBUG
        logger->debug("  op: channel=" + std::to_string(op.channel) + ", speed=" + std::to_string(op.speed));
#endif
        if (!setServoSpeed(op.channel, op.speed, op.duration_ms))
            all_success = false;
    }
    return all_success;
}

/**
 * @brief Start or cancel the per-channel auto-stop FreeRTOS timer.
 * @param channel     Servo channel (0-7).
 * @param duration_ms Auto-stop delay in ms; 0 cancels any pending stop.
 * @note  xTimerChangePeriod both changes the period AND starts the timer if it is dormant.
 *        Calling it with an already-running timer resets the countdown to the new period.
 */
void ServoService::scheduleChannelStop(uint8_t channel, uint32_t duration_ms)
{
    if (channel >= MAX_SERVO_CHANNELS || !channel_stop_timers[channel])
        return;
    if (duration_ms > 0)
        xTimerChangePeriod(channel_stop_timers[channel], pdMS_TO_TICKS(duration_ms), 0);
    else
        xTimerStop(channel_stop_timers[channel], 0);
}

/**
 * @brief Set angle for multiple servos at once
 * @param ops Vector of ServoAngleOp operations
 * @return true if all operations successful, false otherwise
 */
bool ServoService::setServosAngleMultiple(const std::vector<ServoAngleOp> &ops)
{
#ifdef SERVO_VERBOSE_DEBUG
    logger->debug("setServosAngleMultiple " + std::to_string(ops.size()) + " ops");
#endif
    if (!isServiceStarted() || ops.empty())
        return false;
    bool all_success = true;
    for (const auto &op : ops)
    {
#ifdef SERVO_VERBOSE_DEBUG
        logger->debug("  op: channel=" + std::to_string(op.channel) + ", angle=" + std::to_string(op.angle));
#endif
        if (!setServoAngle(op.channel, op.angle))
            all_success = false;
    }
    return all_success;
}

/**
 * @brief Set DC motor speed via DFR1216 expansion board
 * @param motor Motor number (1-4)
 * @param speed Speed percentage (-100 to +100, negative is reverse)
 * @return true if successful, false otherwise
 */
bool ServoService::setMotorSpeed(uint8_t motor, int8_t speed)
{
    if (!isServiceStarted())
        return false;
#ifdef SERVO_VERBOSE_DEBUG
    logger->debug("setMotorSpeed #" + std::to_string(motor) + ": " + std::to_string(speed));
#endif
    try
    {
        if (motor < 1 || motor > MAX_MOTOR_CHANNELS)
            throw std::out_of_range(progmem_to_string(ServoConsts::err_motor_range));
        if (speed < -100 || speed > 100)
            throw std::out_of_range(reinterpret_cast<const char *>(FPSTR(ServoConsts::err_speed_range)));

        eMotorNumber_t motor_a = static_cast<eMotorNumber_t>((motor - 1) * 2);
        eMotorNumber_t motor_b = static_cast<eMotorNumber_t>((motor - 1) * 2 + 1);
        uint16_t duty = static_cast<uint16_t>((std::abs(speed) * 65535) / 100);

        if (speed > 0)
        {
            servoController.setMotorDuty(motor_a, duty);
            servoController.setMotorDuty(motor_b, 0);
        }
        else if (speed < 0)
        {
            servoController.setMotorDuty(motor_a, 0);
            servoController.setMotorDuty(motor_b, duty);
        }
        else
        {
            servoController.setMotorDuty(motor_a, 0);
            servoController.setMotorDuty(motor_b, 0);
        }
        return true;
    }
    catch (const std::exception &e)
    {
        logger->error(e.what());
        return false;
    }
}

bool ServoService::stopService()
{
    // Stop and delete per-channel auto-stop timers
    for (uint8_t ch = 0; ch < MAX_SERVO_CHANNELS; ++ch)
    {
        if (channel_stop_timers[ch])
        {
            xTimerStop(channel_stop_timers[ch], 0);
            xTimerDelete(channel_stop_timers[ch], 0);
            channel_stop_timers[ch] = nullptr;
        }
    }
    setServiceStatus(STOPPED);
    return false;
}

/**
 * @brief Set the same speed on all DC motors.
 * @param speed Speed value (-100 to +100)
 * @return true if all motors were set successfully
 */
bool ServoService::setAllMotorsSpeed(int8_t speed)
{
    if (!isServiceStarted())
        return false;

#ifdef SERVO_VERBOSE_DEBUG
    logger->debug("setAllMotorsSpeed " + std::to_string(speed));
#endif
    if (speed < -100 || speed > 100)
    {
        logger->error(progmem_to_string(ServoConsts::err_speed_range));
        return false;
    }
    bool ok = true;
    for (uint8_t m = 1; m <= MAX_MOTOR_CHANNELS; m++)
        ok = ok && setMotorSpeed(m, speed);
    return ok;
}
std::string ServoService::getAllAttachedServos()
{
    if (!isServiceStarted())
    {
        return {};
    }
    JsonDocument doc;
    JsonArray servosArray = doc[FPSTR(ServoConsts::json_attached_servos)].to<JsonArray>();

    for (uint8_t channel = 0; channel < MAX_SERVO_CHANNELS; channel++)
    {
        const char *status = attached_servos.at(channel) == NOT_CONNECTED ? reinterpret_cast<const char *>(FPSTR(ServoConsts::status_not_connected))
                             : attached_servos.at(channel) == ROTATIONAL  ? reinterpret_cast<const char *>(FPSTR(ServoConsts::status_rotational))
                             : attached_servos.at(channel) == ANGULAR_180 ? reinterpret_cast<const char *>(FPSTR(ServoConsts::status_angular_180))
                             : attached_servos.at(channel) == ANGULAR_270 ? reinterpret_cast<const char *>(FPSTR(ServoConsts::status_angular_270))
                                                                          : reinterpret_cast<const char *>(FPSTR(ServoConsts::status_unknown));
        JsonObject servoObj = servosArray.add<JsonObject>();
        servoObj[ServoConsts::servo_channel] = channel;
        servoObj[ServoConsts::connection] = status;
    }

    String output;
    serializeJson(doc, output);
    return std::string(output.c_str());
}

std::string ServoService::getAttachedServo(uint8_t channel)
{
    if (!isServiceStarted())
    {
        return {};
    }
    std::string status = attached_servos.at(channel) == NOT_CONNECTED ? reinterpret_cast<const char *>(FPSTR(ServoConsts::status_not_connected))
                         : attached_servos.at(channel) == ROTATIONAL  ? reinterpret_cast<const char *>(FPSTR(ServoConsts::status_rotational))
                         : attached_servos.at(channel) == ANGULAR_180 ? reinterpret_cast<const char *>(FPSTR(ServoConsts::status_angular_180))
                         : attached_servos.at(channel) == ANGULAR_270 ? reinterpret_cast<const char *>(FPSTR(ServoConsts::status_angular_270))
                                                                      : reinterpret_cast<const char *>(FPSTR(ServoConsts::status_unknown));
    JsonDocument doc = JsonDocument();
    doc[ServoConsts::servo_channel] = channel;
    doc[ServoConsts::connection] = status;
    String output;
    serializeJson(doc, output);
    return std::string(output.c_str());
}

/**
 * @brief Add route for setting servo angle
 */
bool ServoService::addRouteSetServoAngle(const std::vector<OpenAPIResponse> &standard_responses)
{
    std::string path = getPath(ServoConsts::action_set_angle);
    logRouteRegistration(path);

    OpenAPIRoute angle_route(path.c_str(), RoutesConsts::method_post,
                             reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_set_angle)),
                             reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)),
                             false, {}, standard_responses);
    angle_route.requestBody = OpenAPIRequestBody(reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_servo_angle_control)),
                                                 ServoConsts::req_channel_angle_07, true);
    angle_route.requestBody.example = ServoConsts::ex_channel_angle;
    registerOpenAPIRoute(angle_route);

    webserver.on(path.c_str(), HTTP_POST, [this](AsyncWebServerRequest *request)
                 {
                       if (!checkServiceStarted(request) ||  (!checkIsRequestFromMaster(request, &amakerbot_service))) return;

            // Parse and validate JSON body
            JsonDocument doc;
            if (!JsonBodyParser::parseBody(request, doc, [](const JsonDocument& d) {
                return d[ServoConsts::servo_channel].is<uint8_t>() && 
                       d[ServoConsts::servo_angle].is<uint16_t>();
            })) return;

            uint8_t ch = doc[ServoConsts::servo_channel].as<uint8_t>();
            uint16_t angle = doc[ServoConsts::servo_angle].as<uint16_t>();

            if (angle > 360 || ch > 7)
            {
                ResponseHelper::sendError(request, ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_values);
                return;
            }

            if (setServoAngle(ch, angle))
            {
                ResponseHelper::sendSuccess(request, FPSTR(ServoConsts::action_set_angle));
            }
            else
            {
                ResponseHelper::sendError(request, ResponseHelper::OPERATION_FAILED, FPSTR(ServoConsts::action_set_angle));
            } }, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                 { JsonBodyParser::storeBody(request, data, len, index, total); });

    return true;
}

/**
 * @brief Add route for setting servo speed
 */
bool ServoService::addRouteSetServoSpeed(const std::vector<OpenAPIResponse> &standard_responses)
{
    std::string path = getPath(ServoConsts::action_set_speed);
    logRouteRegistration(path);

    OpenAPIRoute speed_route(path.c_str(), RoutesConsts::method_post,
                             reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_set_speed)),
                             reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)),
                             false, {}, standard_responses);
    speed_route.requestBody = OpenAPIRequestBody(reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_servo_speed_control)),
                                                 ServoConsts::req_channel_speed, true);
    speed_route.requestBody.example = ServoConsts::ex_channel_speed;
    registerOpenAPIRoute(speed_route);

    webserver.on(path.c_str(), HTTP_POST, [this](AsyncWebServerRequest *request)
                 {
                      if (!checkServiceStarted(request) ||  (!checkIsRequestFromMaster(request, &amakerbot_service))) return;

            // Parse and validate JSON body
            JsonDocument doc;
            if (!JsonBodyParser::parseBody(request, doc, [](const JsonDocument& d) {
                return d[ServoConsts::servo_channel].is<int>() && 
                       d[ServoConsts::servo_speed].is<int>();
            })) return;
            
            uint8_t channel = doc[ServoConsts::servo_channel].as<uint8_t>();
            int8_t speed = doc[ServoConsts::servo_speed].as<int>();
            uint32_t duration_ms = doc[ServoConsts::servo_duration_ms] | 0u;
            
            if (channel > 7 || speed < -100 || speed > 100)
            {
                ResponseHelper::sendError(request, ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_values);
                return;
            }
            
            if (this->setServoSpeed(channel, speed, duration_ms))
            {
                ResponseHelper::sendSuccess(request, FPSTR(ServoConsts::action_set_speed));
            }
            else
            {
                ResponseHelper::sendError(request, ResponseHelper::OPERATION_FAILED, FPSTR(ServoConsts::action_set_speed));
            } }, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                 { JsonBodyParser::storeBody(request, data, len, index, total); });

    return true;
}

/**
 * @brief Add route for stopping all servos
 */
bool ServoService::addRouteStopAll(const std::vector<OpenAPIResponse> &standard_responses)
{
    std::string path = getPath(ServoConsts::action_stop_all);
    logRouteRegistration(path);

    // Note: No request body needed - parameterless POST operation
    OpenAPIRoute stop_all_route(path.c_str(), RoutesConsts::method_post,
                                reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_stop_all)),
                                reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)),
                                false, {}, standard_responses);
    registerOpenAPIRoute(stop_all_route);

    webserver.on(path.c_str(), HTTP_POST, [this](AsyncWebServerRequest *request)
                 {
            if (!checkServiceStarted(request) ||  (!checkIsRequestFromMaster(request, &amakerbot_service))) return;

        if (this->setAllServoSpeed(0))
        {
            ResponseHelper::sendSuccess(request, ServoConsts::action_stop_all);
        }
        else
        {
            ResponseHelper::sendError(request, ResponseHelper::OPERATION_FAILED, ServoConsts::action_stop_all);
        } });

    return true;
}

/**
 * @brief Add route for getting servo status
 */
bool ServoService::addRouteGetStatus(const std::vector<OpenAPIResponse> &standard_responses)
{
    std::string path = getPath(ServoConsts::action_get_status);
    logRouteRegistration(path);

    std::vector<OpenAPIParameter> status_params;
    status_params.push_back(OpenAPIParameter(ServoConsts::param_channel, RoutesConsts::type_integer, RoutesConsts::in_query, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_servo_channel)), true));

    std::vector<OpenAPIResponse> status_responses;
    OpenAPIResponse status_ok(200, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_status_retrieved)));
    status_ok.schema = ServoConsts::schema_channel_status;
    status_ok.example = ServoConsts::ex_channel_status;
    status_responses.push_back(status_ok);
    status_responses.push_back(createMissingParamsResponse());

    OpenAPIRoute status_route(path.c_str(), RoutesConsts::method_get,
                              reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_get_status)),
                              reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)),
                              true, status_params, status_responses);
    registerOpenAPIRoute(status_route);

    webserver.on(path.c_str(), HTTP_GET, [this](AsyncWebServerRequest *request)
                 {
            if (!checkServiceStarted(request) ||  (!checkIsRequestFromMaster(request, &amakerbot_service))) return;

        // Validate channel parameter with range check
        std::string channel_str = ParamValidator::getValidatedParam(
            request,
            ServoConsts::servo_channel,
            RoutesConsts::msg_invalid_params,
            [](const std::string& val) {
                int ch = std::atoi(val.c_str());
                return ch >= 0 && ch <= 7;
            }
        );
        if (channel_str.empty()) return;

        uint8_t channel = (uint8_t)std::atoi(channel_str.c_str());
        
        std::string status = getAttachedServo(channel);
        request->send(200, RoutesConsts::mime_json, status.c_str()); });

    return true;
}

/**
 * @brief Add route for getting all servos status
 */
bool ServoService::addRouteGetAllStatus()
{
    std::string path = getPath(ServoConsts::action_get_all_status);
    logRouteRegistration(path);

    std::vector<OpenAPIResponse> all_status_responses;
    OpenAPIResponse all_status_ok(200, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_all_status_retrieved)));
    all_status_ok.schema = ServoConsts::schema_all_servos;
    all_status_ok.example = ServoConsts::ex_all_servos;
    all_status_responses.push_back(all_status_ok);

    OpenAPIRoute all_status_route(path.c_str(), RoutesConsts::method_get,
                                  reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_get_all_status)),
                                  reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)),
                                  false, {}, all_status_responses);
    registerOpenAPIRoute(all_status_route);

    webserver.on(path.c_str(), HTTP_GET, [this](AsyncWebServerRequest *request)
                 {
            if (!checkServiceStarted(request) ||  (!checkIsRequestFromMaster(request, &amakerbot_service))) return;


        std::string status = getAllAttachedServos();
        request->send(200, RoutesConsts::mime_json, status.c_str()); });

    return true;
}

/**
 * @brief Add route for getting battery level
 */
bool ServoService::addRouteGetBattery()
{
    std::string path = getPath(ServoConsts::action_get_battery);
    logRouteRegistration(path);

    std::vector<OpenAPIResponse> battery_responses;
    OpenAPIResponse battery_ok(200, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_get_battery)));
    battery_ok.schema = ServoConsts::schema_battery;
    battery_ok.example = ServoConsts::ex_battery;
    battery_responses.push_back(battery_ok);

    OpenAPIRoute battery_route(path.c_str(), RoutesConsts::method_get,
                               reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_get_battery)),
                               reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)),
                               false, {}, battery_responses);
    registerOpenAPIRoute(battery_route);

    webserver.on(path.c_str(), HTTP_GET, [this](AsyncWebServerRequest *request)
                 {
            if (!checkServiceStarted(request) ||  (!checkIsRequestFromMaster(request, &amakerbot_service))) return;
        JsonDocument doc;
        doc[ServoConsts::json_battery] = servoController.getBattery();
        String output;
        serializeJson(doc, output);
        request->send(200, RoutesConsts::mime_json, output.c_str()); });

    return true;
}

/**
 * @brief Add route for setting all servos to same angle
 */
bool ServoService::addRouteSetAllAngle(const std::vector<OpenAPIResponse> &standard_responses)
{
    std::string path = getPath(ServoConsts::action_set_all_angle);
    logRouteRegistration(path);

    OpenAPIRoute all_angle_route(path.c_str(), RoutesConsts::method_post,
                                 reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_set_all_angle)),
                                 reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)),
                                 false, {}, standard_responses);
    all_angle_route.requestBody = OpenAPIRequestBody(reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_angle_for_all)),
                                                     ServoConsts::req_angle, true);
    all_angle_route.requestBody.example = ServoConsts::ex_angle;
    registerOpenAPIRoute(all_angle_route);

    webserver.on(path.c_str(), HTTP_POST, [this](AsyncWebServerRequest *request)
                 {
            if (!checkServiceStarted(request) ||  (!checkIsRequestFromMaster(request, &amakerbot_service))) return;

            // Parse and validate JSON body
            JsonDocument doc;
            if (!JsonBodyParser::parseBody(request, doc, [](const JsonDocument& d) {
                return d[ServoConsts::servo_angle].is<uint16_t>();
            })) return;

            uint16_t angle = doc[ServoConsts::servo_angle].as<uint16_t>();
            
            if (angle > 360)
            {
                ResponseHelper::sendError(request, ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_values);
                return;
            }
            
            if (setAllServoAngle(angle))
            {
                ResponseHelper::sendSuccess(request, FPSTR(ServoConsts::action_set_all_angle));
            }
            else
            {
                ResponseHelper::sendError(request, ResponseHelper::OPERATION_FAILED, FPSTR(ServoConsts::action_set_all_angle));
            } }, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                 { JsonBodyParser::storeBody(request, data, len, index, total); });

    return true;
}

/**
 * @brief Add route for setting all servos to same speed
 */
bool ServoService::addRouteSetAllSpeed(const std::vector<OpenAPIResponse> &standard_responses)
{
    std::string path = getPath(ServoConsts::action_set_all_speed);
    logRouteRegistration(path);

    OpenAPIRoute all_speed_route(path.c_str(), RoutesConsts::method_post,
                                 reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_set_all_speed)),
                                 reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)),
                                 false, {}, standard_responses);
    all_speed_route.requestBody = OpenAPIRequestBody(reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_speed_for_all)),
                                                     ServoConsts::req_speed, true);
    all_speed_route.requestBody.example = ServoConsts::ex_speed;
    registerOpenAPIRoute(all_speed_route);

    webserver.on(path.c_str(), HTTP_POST, [this](AsyncWebServerRequest *request)
                 {
            if (!checkServiceStarted(request) ||  (!checkIsRequestFromMaster(request, &amakerbot_service))) return;

            // Parse and validate JSON body
            JsonDocument doc;
            if (!JsonBodyParser::parseBody(request, doc, [](const JsonDocument& d) {
                return d[FPSTR(ServoConsts::servo_speed)].is<int8_t>();
            })) return;

            int8_t speed = doc[FPSTR(ServoConsts::servo_speed)].as<int8_t>();
            uint32_t duration_ms = doc[FPSTR(ServoConsts::servo_duration_ms)] | 0u;
            
            if (speed < -100 || speed > 100)
            {
                ResponseHelper::sendError(request, ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_values);
                return;
            }
            
            if (setAllServoSpeed(speed, duration_ms))
            {
                ResponseHelper::sendSuccess(request, FPSTR(ServoConsts::action_set_all_speed));
            }
            else
            {
                ResponseHelper::sendError(request, ResponseHelper::OPERATION_FAILED, FPSTR(ServoConsts::action_set_all_speed));
            } }, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                 { JsonBodyParser::storeBody(request, data, len, index, total); });

    return true;
}

/**
 * @brief Add route for setting multiple servos speed at once
 */
bool ServoService::addRouteSetServosSpeedMultiple(const std::vector<OpenAPIResponse> &standard_responses)
{
    std::string path = getPath(ServoConsts::action_set_servos_speed_multiple);
    logRouteRegistration(path);

    OpenAPIRoute multi_speed_route(path.c_str(), RoutesConsts::method_post,
                                   reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_set_servos_speed_multiple)),
                                   reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)),
                                   false, {}, standard_responses);
    multi_speed_route.requestBody = OpenAPIRequestBody(reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_set_servos_speed_multiple)),
                                                       ServoConsts::req_servos_speed_multiple, true);
    multi_speed_route.requestBody.example = ServoConsts::ex_servos_speed_multiple;
    registerOpenAPIRoute(multi_speed_route);

    webserver.on(path.c_str(), HTTP_POST, [this](AsyncWebServerRequest *request)
                 {
            if (!checkServiceStarted(request) ||  (!checkIsRequestFromMaster(request, &amakerbot_service))) return;

            // Parse and validate JSON body
            JsonDocument doc;
            if (!JsonBodyParser::parseBody(request, doc, [](const JsonDocument& d) {
                return d[ServoConsts::servos].is<JsonArray>();
            })) return;

            JsonArray arr = doc[ServoConsts::servos].as<JsonArray>();
            std::vector<ServoSpeedOp> ops;
            for (JsonObject servo_obj : arr)
            {
                if (!servo_obj[ServoConsts::servo_channel].is<uint8_t>() || !servo_obj[ServoConsts::servo_speed].is<int8_t>())
                    continue;
                ServoSpeedOp op;
                op.channel = servo_obj[ServoConsts::servo_channel].as<uint8_t>();
                op.speed = servo_obj[ServoConsts::servo_speed].as<int8_t>();
                op.duration_ms = servo_obj[ServoConsts::servo_duration_ms] | 0u;
                ops.push_back(op);
            }

            if (ops.empty())
            {
                ResponseHelper::sendError(request, ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_params);
                return;
            }

            if (setServosSpeedMultiple(ops))
            {
                ResponseHelper::sendSuccess(request, FPSTR(ServoConsts::action_set_servos_speed_multiple));
            }
            else
            {
                ResponseHelper::sendError(request, ResponseHelper::OPERATION_FAILED, FPSTR(ServoConsts::action_set_servos_speed_multiple));
            } }, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                 { JsonBodyParser::storeBody(request, data, len, index, total); });

    return true;
}

/**
 * @brief Add route for setting multiple servos angle at once
 */
bool ServoService::addRouteSetServosAngleMultiple(const std::vector<OpenAPIResponse> &standard_responses)
{
    std::string path = getPath(ServoConsts::action_set_servos_angle_multiple);
    logRouteRegistration(path);

    OpenAPIRoute multi_angle_route(path.c_str(), RoutesConsts::method_post,
                                   reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_set_servos_angle_multiple)),
                                   reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)),
                                   false, {}, standard_responses);
    multi_angle_route.requestBody = OpenAPIRequestBody(reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_set_servos_angle_multiple)),
                                                       ServoConsts::req_servos_angle_multiple, true);
    multi_angle_route.requestBody.example = ServoConsts::ex_servos_angle_multiple;
    registerOpenAPIRoute(multi_angle_route);

    webserver.on(path.c_str(), HTTP_POST, [this](AsyncWebServerRequest *request)
                 {
            if (!checkServiceStarted(request) ||  (!checkIsRequestFromMaster(request, &amakerbot_service))) return;

            // Parse and validate JSON body
            JsonDocument doc;
            if (!JsonBodyParser::parseBody(request, doc, [](const JsonDocument& d) {
                return d[ServoConsts::servos].is<JsonArray>();
            })) return;

            JsonArray arr = doc[ServoConsts::servos].as<JsonArray>();
            std::vector<ServoAngleOp> ops;
            for (JsonObject servo_obj : arr)
            {
                if (!servo_obj[ServoConsts::servo_channel].is<uint8_t>() || !servo_obj[ServoConsts::servo_angle].is<uint16_t>())
                    continue;
                ServoAngleOp op;
                op.channel = servo_obj[ServoConsts::servo_channel].as<uint8_t>();
                op.angle = servo_obj[ServoConsts::servo_angle].as<uint16_t>();
                ops.push_back(op);
            }

            if (ops.empty())
            {
                ResponseHelper::sendError(request, ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_params);
                return;
            }

            if (setServosAngleMultiple(ops))
            {
                ResponseHelper::sendSuccess(request, FPSTR(ServoConsts::action_set_servos_angle_multiple));
            }
            else
            {
                ResponseHelper::sendError(request, ResponseHelper::OPERATION_FAILED, FPSTR(ServoConsts::action_set_servos_angle_multiple));
            } }, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                 { JsonBodyParser::storeBody(request, data, len, index, total); });

    return true;
}

/**
 * @brief Add route for attaching servo to a channel
 */
bool ServoService::addRouteAttachServo(const std::vector<OpenAPIResponse> &standard_responses)
{
    std::string path = getPath(ServoConsts::action_attach_servo);
    logRouteRegistration(path);

    OpenAPIRoute attach_route(path.c_str(), RoutesConsts::method_post,
                              reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_attach_servo)),
                              reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)),
                              false, {}, standard_responses);
    attach_route.requestBody = OpenAPIRequestBody(reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_attachment_config)),
                                                  ServoConsts::req_channel_connection, true);
    attach_route.requestBody.example = ServoConsts::ex_channel_connection;
    registerOpenAPIRoute(attach_route);

    webserver.on(path.c_str(), HTTP_POST, [this](AsyncWebServerRequest *request)
                 {
            if (!checkServiceStarted(request) ||  (!checkIsRequestFromMaster(request, &amakerbot_service))) return;

            // Parse and validate JSON body
            JsonDocument doc;
            if (!JsonBodyParser::parseBody(request, doc, [](const JsonDocument& d) {
                return d[FPSTR(ServoConsts::servo_channel)].is<uint8_t>() && 
                       d[FPSTR(ServoConsts::connection)].is<uint8_t>();
            })) return;
            
            uint8_t channel = doc[FPSTR(ServoConsts::servo_channel)].as<uint8_t>();
            uint8_t connection = doc[FPSTR(ServoConsts::connection)].as<uint8_t>();
            
            if (channel > 7 || connection > 3)
            {
                ResponseHelper::sendError(request, ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_values);
                return;
            }
            
            ServoConnection servo_connection = static_cast<ServoConnection>(connection);
            if (attachServo(channel, servo_connection))
            {
                ResponseHelper::sendSuccess(request, FPSTR(ServoConsts::action_attach_servo));
            }
            else
            {
                ResponseHelper::sendError(request, ResponseHelper::OPERATION_FAILED, FPSTR(ServoConsts::action_attach_servo));
            } }, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                 { JsonBodyParser::storeBody(request, data, len, index, total); });

    return true;
}

/**
 * @brief Add route for setting DC motor speed
 */
bool ServoService::addRouteSetMotorSpeed(const std::vector<OpenAPIResponse> &standard_responses)
{
    std::string path = getPath(ServoConsts::action_set_motor_speed);
    logRouteRegistration(path);

    OpenAPIRoute motor_route(path.c_str(), RoutesConsts::method_post,
                             reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_set_motor_speed)),
                             reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)),
                             false, {}, standard_responses);
    motor_route.requestBody = OpenAPIRequestBody(reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_set_motor_speed)),
                                                 ServoConsts::req_motor_speed, true);
    motor_route.requestBody.example = ServoConsts::ex_motor_speed;
    registerOpenAPIRoute(motor_route);

    webserver.on(path.c_str(), HTTP_POST, [this](AsyncWebServerRequest *request)
                 {
            if (!checkServiceStarted(request) ||  (!checkIsRequestFromMaster(request, &amakerbot_service))) return;


            JsonDocument doc;
            if (!JsonBodyParser::parseBody(request, doc, [](const JsonDocument &d) {
                return d[ServoConsts::motor_channel].is<uint8_t>() &&
                       d[ServoConsts::servo_speed].is<int>();
            })) return;

            uint8_t motor = doc[ServoConsts::motor_channel].as<uint8_t>();
            int8_t  speed = doc[ServoConsts::servo_speed].as<int8_t>();

            if (motor < 1 || motor > 4 || speed < -100 || speed > 100)
            {
                ResponseHelper::sendError(request, ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_values);
                return;
            }

            if (setMotorSpeed(motor, speed))
                ResponseHelper::sendSuccess(request, FPSTR(ServoConsts::action_set_motor_speed));
            else
                ResponseHelper::sendError(request, ResponseHelper::OPERATION_FAILED, FPSTR(ServoConsts::action_set_motor_speed)); }, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                 { JsonBodyParser::storeBody(request, data, len, index, total); });

    return true;
}

/**
 * @brief Add route for stopping all DC motors
 */
bool ServoService::addRouteStopAllMotors(const std::vector<OpenAPIResponse> &standard_responses)
{
    std::string path = getPath(ServoConsts::action_stop_all_motors);
    logRouteRegistration(path);

    OpenAPIRoute stop_route(path.c_str(), RoutesConsts::method_post,
                            reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_stop_all_motors)),
                            reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)),
                            false, {}, standard_responses);
    registerOpenAPIRoute(stop_route);

    webserver.on(path.c_str(), HTTP_POST, [this](AsyncWebServerRequest *request)
                 {
            if (!checkServiceStarted(request) ||  (!checkIsRequestFromMaster(request, &amakerbot_service))) return;

        bool ok = true;
        for (uint8_t m = 1; m <= MAX_MOTOR_CHANNELS; m++)
            ok = ok && setMotorSpeed(m, 0);

        if (ok)
            ResponseHelper::sendSuccess(request, ServoConsts::action_stop_all_motors);
        else
            ResponseHelper::sendError(request, ResponseHelper::OPERATION_FAILED, ServoConsts::action_stop_all_motors); });

    return true;
}

/**
 * @brief Add route for setting all DC motors to the same speed
 */
bool ServoService::addRouteSetAllMotorsSpeed(const std::vector<OpenAPIResponse> &standard_responses)
{
    std::string path = getPath(ServoConsts::action_set_all_motors_speed);
    logRouteRegistration(path);

    OpenAPIRoute route(path.c_str(), RoutesConsts::method_post,
                       reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_set_all_motors_speed)),
                       reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)),
                       false, {}, standard_responses);
    route.requestBody = OpenAPIRequestBody(
        reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_set_all_motors_speed)),
        ServoConsts::req_all_motors_speed, true);
    route.requestBody.example = ServoConsts::ex_all_motors_speed;
    registerOpenAPIRoute(route);

    webserver.on(path.c_str(), HTTP_POST, [this](AsyncWebServerRequest *request)
                 {
            if (!checkServiceStarted(request) ||  (!checkIsRequestFromMaster(request, &amakerbot_service))) return;
            JsonDocument doc;
            if (!JsonBodyParser::parseBody(request, doc, [](const JsonDocument &d) {
                return d[ServoConsts::servo_speed].is<int>();
            })) return;

            int8_t speed = doc[ServoConsts::servo_speed].as<int8_t>();

            if (speed < -100 || speed > 100)
            {
                ResponseHelper::sendError(request, ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_values);
                return;
            }

            if (setAllMotorsSpeed(speed))
                ResponseHelper::sendSuccess(request, FPSTR(ServoConsts::action_set_all_motors_speed));
            else
                ResponseHelper::sendError(request, ResponseHelper::OPERATION_FAILED, FPSTR(ServoConsts::action_set_all_motors_speed)); }, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                 { JsonBodyParser::storeBody(request, data, len, index, total); });

    return true;
}

/**
 * @brief Register HTTP routes for servo control.
 * @return true if registration was successful, false otherwise.
 */
bool ServoService::registerRoutes()
{
    // Define common response schemas using optimized helper functions
    std::vector<OpenAPIResponse> standard_responses;
    OpenAPIResponse ok_resp = createSuccessResponse(RoutesConsts::resp_operation_success);
    ok_resp.example = ServoConsts::ex_result_ok;
    standard_responses.push_back(ok_resp);
    standard_responses.push_back(createMissingParamsResponse());
    standard_responses.push_back(createOperationFailedResponse());
    standard_responses.push_back(createNotInitializedResponse());
    standard_responses.push_back(createServiceNotStartedResponse());
    standard_responses.push_back(createForbiddenResponse());

    // Register all routes using dedicated helper functions
    addRouteSetServoAngle(standard_responses);
    addRouteSetServoSpeed(standard_responses);
    addRouteStopAll(standard_responses);
    addRouteGetStatus(standard_responses);
    addRouteGetAllStatus();
    addRouteSetAllAngle(standard_responses);
    addRouteSetAllSpeed(standard_responses);
    addRouteSetServosSpeedMultiple(standard_responses);
    addRouteSetServosAngleMultiple(standard_responses);
    addRouteAttachServo(standard_responses);
    addRouteSetMotorSpeed(standard_responses);
    addRouteStopAllMotors(standard_responses);
    addRouteSetAllMotorsSpeed(standard_responses);
    addRouteGetBattery();
    registerServiceStatusRoute(this);
    registerSettingsRoutes(this);

    return true;
}

std::string ServoService::getServiceName()
{
    return progmem_to_string(ServoConsts::str_service_name);
}

std::string ServoService::getServiceSubPath()
{
    return progmem_to_string(ServoConsts::path_service);
}

bool ServoService::saveSettings()
{
    if (!settings_service_)
    {
        if (logger)
        {
            logger->error("Servo Service: Settings service not available");
        }
        return false;
    }

    std::string comma = progmem_to_string(ServoConsts::str_comma);
    return settings_service_->setSetting(getServiceName(), reinterpret_cast<const char *>(FPSTR(ServoConsts::settings_key_servos)), std::to_string(static_cast<int>(attached_servos[0])) + comma + std::to_string(static_cast<int>(attached_servos[1])) + comma + std::to_string(static_cast<int>(attached_servos[2])) + comma + std::to_string(static_cast<int>(attached_servos[3])) + comma + std::to_string(static_cast<int>(attached_servos[4])) + comma + std::to_string(static_cast<int>(attached_servos[5])) + comma + std::to_string(static_cast<int>(attached_servos[6])) + comma + std::to_string(static_cast<int>(attached_servos[7])));
}

bool ServoService::loadSettings()
{
    if (!settings_service_)
    {
        if (logger)
        {
            logger->error("Servo Service: Settings service not available");
        }
        return false;
    }

    std::string attached_servos_settings = settings_service_->getSetting(getServiceName(), reinterpret_cast<const char *>(FPSTR(ServoConsts::settings_key_servos)));
    if (attached_servos_settings.empty())
    {
        logger->info(progmem_to_string(ServoConsts::msg_no_saved_settings));
        return true;
    }

    // Parse comma-separated values and attach each servo
    size_t pos = 0;
    uint8_t channel = 0;
    std::string token;
    std::string remaining = attached_servos_settings;

    while ((pos = remaining.find(',')) != std::string::npos && channel < MAX_SERVO_CHANNELS)
    {
        token = remaining.substr(0, pos);
        int connection_value = atoi(token.c_str());
        ServoConnection connection = static_cast<ServoConnection>(connection_value);
        attachServo(channel, connection);
        remaining.erase(0, pos + 1);
        channel++;
    }

    // Handle last value (no comma after it)
    if (channel < MAX_SERVO_CHANNELS && !remaining.empty())
    {
        int connection_value = atoi(remaining.c_str());
        ServoConnection connection = static_cast<ServoConnection>(connection_value);
        attachServo(channel, connection);
    }

    logger->info(progmem_to_string(ServoConsts::msg_loaded_settings));
    return true;
}

// ─── UDP binary protocol ────────────────────────────────────────────────────────────

static inline void udp_build(uint8_t action, uint8_t resp, const char *msg, std::string &out)
{
    out.clear();
    out += static_cast<char>(action);
    out += static_cast<char>(resp);
    if (msg && *msg)
        out.append(msg);
}
static inline void udp_build(uint8_t action, uint8_t resp, const std::string &msg, std::string &out)
{
    out.clear();
    out += static_cast<char>(action);
    out += static_cast<char>(resp);
    out += msg;
}

/**
 * @brief Returns JSON status for servo channels selected by @p mask.
 */
std::string ServoService::getAttachedServosMasked(uint8_t mask)
{
    JsonDocument doc;
    JsonArray arr = doc[FPSTR(ServoConsts::json_attached_servos)].to<JsonArray>();
    for (uint8_t ch = 0; ch < MAX_SERVO_CHANNELS; ++ch)
    {
        if (!(mask & (1u << ch)))
            continue;
        JsonObject obj = arr.add<JsonObject>();
        obj[ServoConsts::servo_channel] = ch;
        const ServoConnection s = attached_servos[ch];
        obj[ServoConsts::connection] =
            s == NOT_CONNECTED ? reinterpret_cast<const char *>(FPSTR(ServoConsts::status_not_connected))
            : s == ROTATIONAL  ? reinterpret_cast<const char *>(FPSTR(ServoConsts::status_rotational))
            : s == ANGULAR_180 ? reinterpret_cast<const char *>(FPSTR(ServoConsts::status_angular_180))
            : s == ANGULAR_270 ? reinterpret_cast<const char *>(FPSTR(ServoConsts::status_angular_270))
                               : reinterpret_cast<const char *>(FPSTR(ServoConsts::status_unknown));
    }
    String buf;
    serializeJson(doc, buf);
    return buf.c_str();
}

// ─── Binary UDP dispatcher ───────────────────────────────────────────────────

/**
 * @brief Handle an incoming binary UDP message for ServoService.
 *
 * REQUEST  : [action:1B][value...]
 *   0x01 SET_SERVO_ANGLE  [ch0:int16_LE][ch1:int16_LE]...  one int16 per servo (up to 8)
 *                          bit0=0 → skip channel;  bit0=1 → angle = raw>>1 (centre-zero °)
 *                          180° servo: ±90  → hw 0..180
 *                          270° servo: ±135 → hw 0..270
 *   0x02 SET_SERVO_SPEED  [mask][speed]               int8  −100..+100
 *   0x03 STOP_SERVOS      [mask]
 *   0x04 ATTACH_SERVO     [mask][type]                uint8 0-3
 *   0x05 SET_MOTOR_SPEED  [m0:int16_LE]...[mN:int16_LE]  one int16 per motor (up to 4)
 *                          bit0=0 → skip;  bit0=1 → speed = raw>>1 (centre-zero, −100..+100)
 *   0x06 STOP_MOTORS      [mask]
 *   0x07 GET_SERVO_STATUS [mask]                      → [action][ok][JSON]
 *   0x08 GET_ALL_STATUS   (no params)                 → [action][ok][JSON]
 *   0x09 GET_BATTERY      (no params)                 → [action][ok][batt%:1B]
 *
 * RESPONSE : [action:1B][resp_code:1B][optional_payload]
 *
 * @return true if first byte is a recognised action code (message claimed)
 */
bool ServoService::messageHandler(const std::string &message,
                                  const IPAddress &remoteIP,
                                  uint16_t remotePort)
{
    const size_t len = message.size();
    if (len < 1)
        return false;

    const uint8_t *d = reinterpret_cast<const uint8_t *>(message.data());
    const uint8_t action = d[0];

    if (action < ServoConsts::udp_action_min || action > ServoConsts::udp_action_max)
        return false;
#ifdef SERVO_VERBOSE_DEBUG
    std::string hex_dump;
    for (size_t i = 0; i < len; ++i)
    {
        char buf[4];
        snprintf(buf, sizeof(buf), "%02X ", d[i]);
        hex_dump += buf;
    }
    logger->debug("UDP rx " + std::to_string(len) + "bytes : " + hex_dump);
#endif
    static std::string resp;
    resp.clear();

    if (!isServiceStarted())
    {
        udp_build(action, UDPProto::udp_resp_not_started, nullptr, resp);
        udp_service.sendReply(resp, remoteIP, remotePort);
        return true;
    }

    if (!checkUDPIsMaster(action, remoteIP, &amakerbot_service, resp))
    {
        udp_service.sendReply(resp, remoteIP, remotePort);
        return true;
    }

    switch (action)
    {
    // 0x01 SET_SERVO_ANGLE  [ch0:int16_LE][ch1:int16_LE]...  one int16 per servo, up to MAX_SERVO_CHANNELS
    //   bit0 == 0 → skip this channel
    //   bit0 == 1 → angle = raw >> 1 (arithmetic shift, centre-zero °)
    //   180° servo: ±90  → hw 0..180 ;  270° servo: ±135 → hw 0..270
    case ServoConsts::udp_action_set_servo_angle:
    {
        const uint8_t n_ch = static_cast<uint8_t>(
            ((len - 1) / 2) < MAX_SERVO_CHANNELS ? (len - 1) / 2 : MAX_SERVO_CHANNELS);
        bool ok = true;
        for (uint8_t ch = 0; ch < n_ch; ++ch)
        {
            const size_t off = 1u + ch * 2u;
            const int16_t raw = static_cast<int16_t>(
                static_cast<uint16_t>(d[off]) | (static_cast<uint16_t>(d[off + 1]) << 8));
            if (!(raw & 1))
                continue;                   // bit0 == 0 → skip
            const int16_t angle = raw >> 1; // bits 15:1 as signed angle (centre-zero °)
            const ServoConnection sc = attached_servos[ch];
            uint16_t hw;
            if (sc == ANGULAR_180 && angle >= -90 && angle <= 90)
                hw = static_cast<uint16_t>(angle + 90);
            else if (sc == ANGULAR_270 && angle >= -135 && angle <= 135)
                hw = static_cast<uint16_t>(angle + 135);
            else
            {
                ok = false;
                continue;
            }
            servoController.setServoAngle(static_cast<eServoNumber_t>(ch), hw);
        }
        udp_build(action, ok ? UDPProto::udp_resp_ok : UDPProto::udp_resp_operation_failed, nullptr, resp);
        break;
    }
    // 0x02 SET_SERVO_SPEED  [mask:1B][encoded_speeds...][1B each for each servo]
    //   Variable payload: each byte corresponds to destination servo in order
    //   each uint8 byte encodes: speed = byte - 128  (shifts 28..228 to -100..+100)
    //   Speed -100 → byte 0x1C (28);  Speed 0 → byte 0x80 (128);  Speed +100 → byte 0xE4 (228)
    //   Only applies speed if different from current tracked speed
    case ServoConsts::udp_action_set_servo_speed:
    {
        if (len < 3)
        {
            udp_build(action, UDPProto::udp_resp_invalid_params, nullptr, resp);
            break;
        }
        const uint8_t mask = d[1];
        bool ok = true;
        uint8_t speed_idx = 2; // Start at byte 2 for speed bytes
        for (uint8_t ch = 0; ch < MAX_SERVO_CHANNELS && speed_idx < len; ++ch, ++speed_idx)
        {
            if (!(mask & (1u << ch)))
                continue;                                                                       // Skip this channel
            const int8_t speed = static_cast<int8_t>(static_cast<int16_t>(d[speed_idx]) - 128); // Decode: speed = byte - 128
            if (speed < -100 || speed > 100)
            {
                udp_build(action, UDPProto::udp_resp_invalid_values, nullptr, resp);
                return true;
            }
            // Only apply if speed differs from current tracked speed
            if (speed == servo_speeds[ch])
            {
#ifdef SERVO_VERBOSE_DEBUG
                logger->debug("Servo " + std::to_string(ch) + " speed unchanged (" + std::to_string(speed) + "), skipping");
#endif
                continue;
            }
            if (attached_servos[ch] != ROTATIONAL)
            {
                ok = false;
                continue;
            }
            servo_speeds[ch] = speed; // Track new speed
            const eServo360Direction_t dir = speed > 0   ? eServo360Direction_t::eForward
                                             : speed < 0 ? eServo360Direction_t::eBackward
                                                         : eServo360Direction_t::eStop;
            const uint8_t duty = static_cast<uint8_t>(speed < 0 ? -speed : speed);
            servoController.setServo360(static_cast<eServoNumber_t>(ch), dir, duty);
        }
        // Optional trailing uint16 LE at bytes 10-11: auto-stop duration_ms
        // The sender must include all 8 speed bytes (total packet len ≥ 10) for this to apply.
        // duration_ms = 0 means no auto-stop.
        {
            const uint32_t duration_ms = (len >= 12)
                ? (static_cast<uint32_t>(d[10]) | (static_cast<uint32_t>(d[11]) << 8))
                : 0;
            for (uint8_t ch = 0; ch < MAX_SERVO_CHANNELS; ++ch)
            {
                if (!(mask & (1u << ch)) || attached_servos[ch] != ROTATIONAL)
                    continue;
                // Schedule stop for channels running (cancel stop for channels that were stopped)
                scheduleChannelStop(ch, (servo_speeds[ch] != 0) ? duration_ms : 0);
            }
        }
        udp_build(action, ok ? UDPProto::udp_resp_ok : UDPProto::udp_resp_operation_failed, nullptr, resp);
        break;
    }
    // 0x03 STOP_SERVOS  [mask:1B]
    case ServoConsts::udp_action_stop_servos:
    {
        if (len < 2)
        {
            udp_build(action, UDPProto::udp_resp_invalid_params, nullptr, resp);
            break;
        }
        const uint8_t mask = d[1];
        for (uint8_t ch = 0; ch < MAX_SERVO_CHANNELS; ++ch)
            if ((mask & (1u << ch)) && attached_servos[ch] == ROTATIONAL)
                servoController.setServo360(static_cast<eServoNumber_t>(ch), eServo360Direction_t::eStop, 0);
        udp_build(action, UDPProto::udp_resp_ok, nullptr, resp);
        break;
    }
    // 0x04 ATTACH_SERVO  [mask:1B][type:1B]
    case ServoConsts::udp_action_attach_servo:
    {
        if (len < 3)
        {
            udp_build(action, UDPProto::udp_resp_invalid_params, nullptr, resp);
            break;
        }
        const uint8_t mask = d[1], type = d[2];
        if (type > 3)
        {
            udp_build(action, UDPProto::udp_resp_invalid_values, nullptr, resp);
            break;
        }
        const ServoConnection conn = static_cast<ServoConnection>(type);
        for (uint8_t ch = 0; ch < MAX_SERVO_CHANNELS; ++ch)
            if (mask & (1u << ch))
                attached_servos[ch] = conn;
        udp_build(action, UDPProto::udp_resp_ok, nullptr, resp);
        break;
    }
    // 0x05 SET_MOTOR_SPEED  [encoded_speeds...][1B each for each motor]
    //   Variable payload: each byte corresponds to destination motor in order
    //   each uint8 byte encodes: speed = byte - 128  (shifts 28..228 to -100..+100)
    //   Speed -100 → byte 0x1C (28);  Speed 0 → byte 0x80 (128);  Speed +100 → byte 0xE4 (228)
    //   Only applies speed if different from current tracked speed
    case ServoConsts::udp_action_set_motor_speed:
    {
        if (len < 2)
        {
            udp_build(action, UDPProto::udp_resp_invalid_params, nullptr, resp);
            break;
        }
        bool ok = true;
        uint8_t speed_idx = 1; // Start at byte 1 for speed bytes
        for (uint8_t m = 0; m < MAX_MOTOR_CHANNELS && speed_idx < len; ++m, ++speed_idx)
        {
            const int8_t speed = static_cast<int8_t>(static_cast<int16_t>(d[speed_idx]) - 128); // Decode: speed = byte - 128
            if (speed < -100 || speed > 100)
            {
                udp_build(action, UDPProto::udp_resp_invalid_values, nullptr, resp);
                return true;
            }
            // Only apply if speed differs from current tracked speed
            if (speed == motor_speeds[m])
            {
#ifdef SERVO_VERBOSE_DEBUG
                logger->debug("Motor " + std::to_string(m) + " speed unchanged (" + std::to_string(speed) + "), skipping");
#endif
                continue;
            }
            motor_speeds[m] = speed; // Track new speed
            const uint16_t duty = static_cast<uint16_t>((speed < 0 ? -speed : speed) * 65535 / 100);
#ifdef SERVO_VERBOSE_DEBUG
            logger->debug("Motor " + std::to_string(m) + " speed=" + std::to_string(speed) + " duty=" + std::to_string(duty));
#endif
            const eMotorNumber_t motor_a = static_cast<eMotorNumber_t>(m * 2);
            const eMotorNumber_t motor_b = static_cast<eMotorNumber_t>(m * 2 + 1);
            if (speed > 0)
            {
                servoController.setMotorDuty(motor_a, duty);
                servoController.setMotorDuty(motor_b, 0);
            }
            else if (speed < 0)
            {
                servoController.setMotorDuty(motor_a, 0);
                servoController.setMotorDuty(motor_b, duty);
            }
            else
            {
                servoController.setMotorDuty(motor_a, 0);
                servoController.setMotorDuty(motor_b, 0);
            }
        }
        udp_build(action, ok ? UDPProto::udp_resp_ok : UDPProto::udp_resp_operation_failed, nullptr, resp);
        break;
    }
    // 0x06 STOP_MOTORS  [mask:1B]
    case ServoConsts::udp_action_stop_motors:
    {
        if (len < 2)
        {
            udp_build(action, UDPProto::udp_resp_invalid_params, nullptr, resp);
            break;
        }
        const uint8_t mask = d[1];
        for (uint8_t m = 0; m < MAX_MOTOR_CHANNELS; ++m)
        {
            if (!(mask & (1u << m)))
                continue;
            servoController.setMotorDuty(static_cast<eMotorNumber_t>(m * 2), 0);
            servoController.setMotorDuty(static_cast<eMotorNumber_t>(m * 2 + 1), 0);
        }
        udp_build(action, UDPProto::udp_resp_ok, nullptr, resp);
        break;
    }
    // 0x07 GET_SERVO_STATUS  [mask:1B]  → JSON of selected channels
    case ServoConsts::udp_action_get_servo_status:
        if (len < 2)
        {
            udp_build(action, UDPProto::udp_resp_invalid_params, nullptr, resp);
            break;
        }
        udp_build(action, UDPProto::udp_resp_ok, getAttachedServosMasked(d[1]), resp);
        break;
    // 0x08 GET_ALL_STATUS  → JSON all channels
    case ServoConsts::udp_action_get_all_status:
        udp_build(action, UDPProto::udp_resp_ok, getAllAttachedServos(), resp);
        break;
    // 0x09 GET_BATTERY  → single byte 0-100
    case ServoConsts::udp_action_get_battery:
    {
        const uint8_t batt = static_cast<uint8_t>(servoController.getBattery());
        resp.clear();
        resp += static_cast<char>(action);
        resp += static_cast<char>(UDPProto::udp_resp_ok);
        resp += static_cast<char>(batt);
        break;
    }
    default:
        udp_build(action, UDPProto::udp_resp_unknown_cmd, nullptr, resp);
        break;
    }

#ifdef VERBOSE_DEBUG
    if (resp.size() >= 2)
        logger->debug("UDP bin a=0x" + std::string(String(resp[0], HEX).c_str()) +
                      " r=0x" + std::string(String(resp[1], HEX).c_str()) +
                      (resp.size() > 2 ? " +" + std::to_string(resp.size() - 2) + "B" : ""));
#endif

    if (!resp.empty())
        udp_service.sendReply(resp, remoteIP, remotePort);
    return true;
}