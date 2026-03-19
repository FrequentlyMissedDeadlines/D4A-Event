// DFR1216 (Unihiker Expansion) Service Implementation
/**
 * @file DFR1216Board.cpp
 * @brief Implementation for DFR1216 expansion board integration with the main application
 * @details Exposed routes:
 *          - POST /api/dfr1216/setServoAngle - Set the angle of a servo motor on the expansion board
 *          - POST /api/dfr1216/setMotorSpeed - Set the speed and direction of a DC motor
 *          - GET /api/dfr1216/getStatus - Get initialization status and operational state of the board
 *
 */

#include "services/DFR1216.h"
#include "FlashStringHelper.h"
#include <ESPAsyncWebServer.h>
#include <pgmspace.h>
#include <ArduinoJson.h>


// DFR1216-specific OpenAPI constants namespace (stored in PROGMEM to save RAM)
namespace DFR1216Consts
{
    // Service name
    constexpr const char str_service_name[] PROGMEM = "DFR1216 Service";
    constexpr const char action_get_status[] PROGMEM = "getStatus";
    constexpr const char action_set_motor_speed[] PROGMEM = "setMotorSpeed";
    constexpr const char action_set_servo_angle[] PROGMEM = "setServoAngle";
    constexpr const char desc_angle_degrees[] PROGMEM = "Angle in degrees (0-180)";
    constexpr const char desc_get_status[] PROGMEM = "Get initialization status and operational state of the DFR1216 expansion board";
    constexpr const char desc_motor_control[] PROGMEM = "Set the speed and direction of a DC motor on the DFR1216 expansion board";
    constexpr const char desc_motor_number[] PROGMEM = "Motor number (1-4)";
    constexpr const char desc_motor_params[] PROGMEM = "Motor control parameters";
    constexpr const char desc_servo_channel[] PROGMEM = "Servo channel (0-5)";
    constexpr const char desc_servo_control[] PROGMEM = "Set the angle of a servo motor on the DFR1216 expansion board";
    constexpr const char desc_servo_params[] PROGMEM = "Servo control parameters";
    constexpr const char desc_speed_percent[] PROGMEM = "Speed percentage (-100 to +100)";
    constexpr const char json_angle[] PROGMEM = "angle";
    constexpr const char json_channel[] PROGMEM = "channel";
    constexpr const char json_error[] PROGMEM = "{\"error\":\"";
    constexpr const char json_motor[] PROGMEM = "motor";
    constexpr const char json_speed[] PROGMEM = "speed";
    constexpr const char msg_angle_out_of_range[] PROGMEM = "Angle out of range (0-180)";
    constexpr const char msg_missing_motor_params[] PROGMEM = "Missing required parameters: motor and speed";
    constexpr const char msg_missing_servo_params[] PROGMEM = "Missing required parameters: channel and angle";
    constexpr const char msg_motor_out_of_range[] PROGMEM = "Motor number out of range (1-4)";
    constexpr const char msg_servo_channel_out_of_range[] PROGMEM = "Servo channel out of range (0-5)";
    constexpr const char msg_speed_out_of_range[] PROGMEM = "Speed out of range (-100 to +100)";
    constexpr const char msg_battery_read_error[] PROGMEM = "Battery read error";
    constexpr const char param_angle[] PROGMEM = "angle";
    constexpr const char param_channel[] PROGMEM = "channel";
    constexpr const char param_motor[] PROGMEM = "motor";
    constexpr const char param_speed[] PROGMEM = "speed";
    constexpr const char path_dfr1216[] PROGMEM = "dfr1216/";
    constexpr const char tag_dfr1216[] PROGMEM = "DFR1216";

    // Response descriptions
    constexpr const char resp_servo_angle_set[] PROGMEM = "Servo angle set successfully";
    constexpr const char resp_motor_speed_set[] PROGMEM = "Motor speed set successfully";
    constexpr const char resp_status_retrieved[] PROGMEM = "Status retrieved successfully";

    // JSON Schema definitions
    constexpr const char schema_channel_angle[] PROGMEM = "{\"type\":\"object\",\"required\":[\"channel\",\"angle\"],\"properties\":{\"channel\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":5},\"angle\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":180}}}";
    constexpr const char schema_DFR1216_I2Cmotor_speed[] PROGMEM = "{\"type\":\"object\",\"required\":[\"motor\",\"speed\"],\"properties\":{\"motor\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4},\"speed\":{\"type\":\"integer\",\"minimum\":-100,\"maximum\":100}}}";
    constexpr const char schema_status[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"service\":{\"type\":\"string\"},\"initialized\":{\"type\":\"boolean\"}}}";
    constexpr const char req_channel_angle[] PROGMEM = "{\"type\":\"object\",\"required\":[\"channel\",\"angle\"],\"properties\":{\"channel\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":5},\"angle\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":180}}}";
    constexpr const char req_motor_speed[] PROGMEM = "{\"type\":\"object\",\"required\":[\"motor\",\"speed\"],\"properties\":{\"motor\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4},\"speed\":{\"type\":\"integer\",\"minimum\":-100,\"maximum\":100}}}";

    // Example values
    constexpr const char ex_channel_angle[] PROGMEM = "{\"channel\":0,\"angle\":90}";
    constexpr const char ex_motor_speed[] PROGMEM = "{\"motor\":1,\"speed\":50}";

    // Response examples
    constexpr const char resp_servo_angle_example[] PROGMEM = "{\"result\":\"ok\",\"channel\":0,\"angle\":90}";
    constexpr const char resp_motor_speed_example[] PROGMEM = "{\"result\":\"ok\",\"motor\":1,\"speed\":75}";
    constexpr const char resp_status_schema[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"message\":{\"type\":\"string\"},\"status\":{\"type\":\"string\"}}}";
    constexpr const char resp_status_example[] PROGMEM = "{\"message\":\"DFR1216Board\",\"status\":\"started\"}";

    // UDP binary protocol constants
    constexpr uint8_t udp_service_id = 0x03;  ///< Unique ID for DFR1216 Service (high nibble of action byte)
    constexpr uint8_t udp_action_set_led_color = (udp_service_id << 4) | 0x01;  ///< [led:1B][r:1B][g:1B][b:1B][brightness:1B]
    constexpr uint8_t udp_action_turn_off_led = (udp_service_id << 4) | 0x02;  ///< [led:1B]
    constexpr uint8_t udp_action_turn_off_all_leds = (udp_service_id << 4) | 0x03;  ///< (no params)
    constexpr uint8_t udp_action_get_led_status = (udp_service_id << 4) | 0x04;  ///< (no params) → [action][ok][JSON]
    constexpr uint8_t udp_action_max = (udp_service_id << 4) | 0x04;  ///< highest valid action code
}

DFR1216_I2C controller = DFR1216_I2C();

std::string DFR1216Board::getServiceName()
{
    return progmem_to_string(DFR1216Consts::str_service_name);
}

bool DFR1216Board::initializeService()
{
    if (!controller.begin())
    {
        setServiceStatus(INITIALIZED_FAILED);
        logger->error(getServiceName() + " " + getStatusString());
        return false;
    }
    setServiceStatus(INITIALIZED);
    logger->info(getServiceName() + " " + getStatusString());   
    return true;
}

bool DFR1216Board::startService()
{
    if (IsServiceInterface::getStatus() != INITIALIZED)
    {
        setServiceStatus(START_FAILED);
        logger->error(getServiceName() + " " + getStatusString());   
        return false;
    }

    setServiceStatus(STARTED);

    logger->info(getServiceName() + " " + getStatusString());
    return true;
}

bool DFR1216Board::stopService()
{
    setServiceStatus(STOPPED);
    logger->info(getServiceName() + " " + getStatusString());
    return true;
}

bool DFR1216Board::saveSettings() { return true; }
bool DFR1216Board::loadSettings() { return true; }

bool DFR1216Board::setServoAngle(uint8_t channel, uint16_t angle)
{
    if (!isServiceStarted())
        return false;

    if (channel > 5)
    {
        logger->error(progmem_to_string(DFR1216Consts::msg_servo_channel_out_of_range));
        return false;
    }

    if (angle > 180)
    {
        logger->error(progmem_to_string(DFR1216Consts::msg_angle_out_of_range));
        return false;
    }

    controller.setServoAngle(static_cast<eServoNumber_t>(channel), angle);

    char log_buf[64];
    snprintf(log_buf, sizeof(log_buf), "Set servo %u to angle %u", channel, angle);
    logger->info(log_buf);
    return true;
}

bool DFR1216Board::setMotorSpeed(uint8_t motor, int8_t speed)
{
    if (!isServiceStarted())
        return false;

    if (motor < 1 || motor > 4)
    {
        logger->error(progmem_to_string(DFR1216Consts::msg_motor_out_of_range));
        return false;
    }

    if (speed < -100 || speed > 100)
    {
        logger->error(progmem_to_string(DFR1216Consts::msg_speed_out_of_range));
        return false;
    }

    // Convert speed percentage to duty cycle (0-65535)
    // Negative speed = reverse, positive = forward
    eMotorNumber_t motor_enum;

    // Map motor number to enum
    switch (motor)
    {
    case 1:
        motor_enum = speed >= 0 ? eMotor1_A : eMotor1_B;
        break;
    case 2:
        motor_enum = speed >= 0 ? eMotor2_A : eMotor2_B;
        break;
    case 3:
        motor_enum = speed >= 0 ? eMotor3_A : eMotor3_B;
        break;
    case 4:
        motor_enum = speed >= 0 ? eMotor4_A : eMotor4_B;
        break;
    default:
        return false;
    }

    // Convert speed percentage to duty cycle (0-65535)
    uint16_t duty = static_cast<uint16_t>((abs(speed) * 65535) / 100);
    controller.setMotorDuty(motor_enum, duty);

    char log_buf[64];
    snprintf(log_buf, sizeof(log_buf), "Set motor %u to speed %d", motor, speed);
    logger->info(log_buf);
    return true;
}



bool DFR1216Board::setLEDColor(uint8_t led_index, uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness)
{
    if (!isServiceStarted())
    {
        IsServiceInterface::logger->error("Service not started");
        return false;
    }

    if (led_index > 2)
    {
        IsServiceInterface::logger->error("Invalid LED index: " + std::to_string(led_index));
        return false;
    }

    try
    {
        // WS2812 LED array - set RGB values for the specified LED
        uint32_t colors[3] = {0, 0, 0};
        colors[led_index] = (static_cast<uint32_t>(red) << 16) | (static_cast<uint32_t>(green) << 8) | blue;
        
        controller.setWS2812(colors, brightness);
        
        // Store in cache
        led_states_[led_index].red = red;
        led_states_[led_index].green = green;
        led_states_[led_index].blue = blue;
        
        char log_buf[64];
        snprintf(log_buf, sizeof(log_buf), "Set LED %u to RGB(%u,%u,%u)", led_index, red, green, blue);
        IsServiceInterface::logger->info(log_buf);
        return true;
    }
    catch (const std::exception& e)
    {
        IsServiceInterface::logger->error("Error setting LED color: " + std::string(e.what()));
        return false;
    }
}

bool DFR1216Board::turnOffLED(uint8_t led_index)
{
    return setLEDColor(led_index, 0, 0, 0, 0);
}

bool DFR1216Board::turnOffAllLEDs()
{
    bool success = true;
    for (uint8_t i = 0; i < 3; i++)
    {
        if (!turnOffLED(i))
        {
            success = false;
        }
    }
    return success;
}

// UDP helper function to build binary responses
static inline void udp_build_response(uint8_t action, uint8_t resp, const char *payload, std::string &out)
{
    out.clear();
    out += static_cast<char>(action);
    out += static_cast<char>(resp);
    if (payload && *payload) out.append(payload);
}

// bool DFR1216Board::messageHandler(const std::string &message,

bool DFR1216Board::messageHandler(const std::string & /*message*/,
                                  const IPAddress   & /*remoteIP*/,
                                  uint16_t            /*remotePort*/)
{
    // TODO: implement LED UDP command handling
    return false;
}


//                                      const IPAddress &remoteIP,
//                                      uint16_t remotePort)
// {
//     const size_t len = message.size();
//     if (len < 1) return false;

//     const uint8_t *d      = reinterpret_cast<const uint8_t *>(message.data());
//     const uint8_t  action = d[0];

//     if (action < 0x31 || action > DFR1216Consts::udp_action_max) return false;

//     static std::string resp;
//     resp.clear();

//     if (!IsServiceInterface::isServiceStarted())
//     {
//         udp_build_response(action, BotMessageProto::udp_resp_not_started, nullptr, resp);
//         udp_service.sendReply(resp, remoteIP, remotePort);
//         return true;
//     }

//     if (!checkUDPIsMaster(action, remoteIP, &amakerbot_service, resp))
//     {
//         udp_service.sendReply(resp, remoteIP, remotePort);
//         return true;
//     }

//     switch (action)
//     {
//     // 0x31 SET_LED_COLOR [led:1B][r:1B][g:1B][b:1B][brightness:1B]
//     case DFR1216Consts::udp_action_set_led_color:
//     {
//         if (len < 6)
//         {
//             udp_build_response(action, BotMessageProto::udp_resp_invalid_params, nullptr, resp);
//             break;
//         }
//         const uint8_t led_index  = d[1];
//         const uint8_t red        = d[2];
//         const uint8_t green      = d[3];
//         const uint8_t blue       = d[4];
//         const uint8_t brightness = d[5];

//         if (led_index > 2)
//         {
//             udp_build_response(action, BotMessageProto::udp_resp_invalid_values, nullptr, resp);
//             break;
//         }

//         if (setLEDColor(led_index, red, green, blue, brightness))
//         {
//             udp_build_response(action, BotMessageProto::udp_resp_ok, nullptr, resp);
//         }
//         else
//         {
//             udp_build_response(action, BotMessageProto::udp_resp_operation_failed, nullptr, resp);
//         }
//         break;
//     }

//     // 0x32 TURN_OFF_LED [led:1B]
//     case DFR1216Consts::udp_action_turn_off_led:
//     {
//         if (len < 2)
//         {
//             udp_build_response(action, BotMessageProto::udp_resp_invalid_params, nullptr, resp);
//             break;
//         }
//         const uint8_t led_index = d[1];

//         if (led_index > 2)
//         {
//             udp_build_response(action, BotMessageProto::udp_resp_invalid_values, nullptr, resp);
//             break;
//         }

//         if (turnOffLED(led_index))
//         {
//             udp_build_response(action, BotMessageProto::udp_resp_ok, nullptr, resp);
//         }
//         else
//         {
//             udp_build_response(action, BotMessageProto::udp_resp_operation_failed, nullptr, resp);
//         }
//         break;
//     }

//     // 0x33 TURN_OFF_ALL_LEDS (no params)
//     case DFR1216Consts::udp_action_turn_off_all_leds:
//     {
//         if (turnOffAllLEDs())
//         {
//             udp_build_response(action, BotMessageProto::udp_resp_ok, nullptr, resp);
//         }
//         else
//         {
//             udp_build_response(action, BotMessageProto::udp_resp_operation_failed, nullptr, resp);
//         }
//         break;
//     }

//     // 0x34 GET_LED_STATUS (no params) → [action][ok][JSON]
//     case DFR1216Consts::udp_action_get_led_status:
//     {
//         JsonDocument doc;
//         JsonArray leds = doc["leds"].to<JsonArray>();
        
//         for (uint8_t i = 0; i < 3; i++)
//         {
//             JsonObject led = leds.add<JsonObject>();
//             led["id"] = i;
//             led["red"] = led_states_[i].red;
//             led["green"] = led_states_[i].green;
//             led["blue"] = led_states_[i].blue;
//         }
        
//         std::string json_str;
//         serializeJson(doc, json_str);
        
//         resp.clear();
//         resp += static_cast<char>(action);
//         resp += static_cast<char>(BotMessageProto::udp_resp_ok);
//         resp += json_str;
        
//         udp_service.sendReply(resp, remoteIP, remotePort);
//         return true;
//     }

//     default:
//     {
//         udp_build_response(action, BotMessageProto::udp_resp_unknown_cmd, nullptr, resp);
//         break;
//     }
//     }

//     udp_service.sendReply(resp, remoteIP, remotePort);
//     return true;
// }

// ---------------------------------------------------------------------------
// DFR1216Board low-level hardware methods (I2C register writes/reads)
// ---------------------------------------------------------------------------

void DFR1216Board::setMotorPeriod(ePeriod_t number, uint16_t motorPeriod)
{
    uint8_t reg = 0;
    uint8_t result = 0;
    uint8_t _tempData[TEMP_LEN] = {0};
    if      (number == eMotor1_2) reg = I2C_MOTOR12_PERIOD_H;
    else if (number == eMotor3_4) reg = I2C_MOTOR34_PERIOD_H;
    else if (number == eServo0_1) reg = I2C_SERVO01_PERIOD_H;
    else if (number == eServo2_5) reg = I2C_SERVO25_PERIOD_H;
    _tempData[0] = (motorPeriod >> 8) & 0xFF;
    _tempData[1] = (motorPeriod >> 0) & 0xFF;
    for (uint8_t i = 0; i < RETRY_COUNT; i++) {
        result = writeReg(reg, _tempData, 2);
        if (result == 0) return;
        delay(I2C_RETRY_DELAY_MS);
    }
}

void DFR1216Board::setMotorDuty(eMotorNumber_t number, uint16_t duty)
{
    uint8_t reg = I2C_MOTOR1_Z_DUTY_H + number * 2;
    uint8_t result = 0;
    uint8_t _tempData[TEMP_LEN] = {0};
    _tempData[0] = (duty >> 8) & 0xFF;
    _tempData[1] = (duty >> 0) & 0xFF;
    for (uint8_t i = 0; i < RETRY_COUNT; i++) {
        result = writeReg(reg, _tempData, 2);
        if (result == 0) return;
        delay(I2C_RETRY_DELAY_MS);
    }
}

void DFR1216Board::setServo360(eServoNumber_t number, eServo360Direction_t direction, uint8_t speed)
{
    if (speed > 100) speed = 100;
    uint16_t period = 0;
    if      (direction == eBackward) period = SERVO360_STOP_US + (speed * (SERVO360_BACKWARD_MAX_US - SERVO360_STOP_US) / 100);
    else if (direction == eForward)  period = SERVO360_STOP_US - (speed * (SERVO360_STOP_US - SERVO360_FORWARD_MIN_US) / 100);
    else if (direction == eStop)     period = SERVO360_STOP_US;
    else return;

    uint8_t reg = number * 2 + I2C_SERVO0_DUTY_H;
    uint8_t result = 0;
    uint8_t _tempData[TEMP_LEN] = {0};
    _tempData[0] = (period >> 8) & 0xFF;
    _tempData[1] = (period >> 0) & 0xFF;
    for (uint8_t i = 0; i < RETRY_COUNT; i++) {
        result = writeReg(reg, _tempData, 2);
        if (result == 0) return;
        delay(I2C_RETRY_DELAY_MS);
    }
}

void DFR1216Board::setServoAngle(eServoNumber_t number, uint16_t angle)
{
    setServoAngle(number, angle, 270);
}

void DFR1216Board::setServoAngle(eServoNumber_t number, uint16_t angle, uint16_t maxAngle)
{
    uint16_t period = 0;
    if (maxAngle == 270) {
        if (angle > 270) angle = 270;
        period = SERVO270_MIN_US + ((uint32_t)angle * (SERVO270_MAX_US - SERVO270_MIN_US) / 270);
    } else {
        if (angle > 180) angle = 180;
        period = SERVO180_MIN_US + ((uint32_t)angle * (SERVO180_MAX_US - SERVO180_MIN_US) / 180);
    }
    uint8_t reg = number * 2 + I2C_SERVO0_DUTY_H;
    uint8_t result = 0;
    uint8_t _tempData[TEMP_LEN] = {0};
    _tempData[0] = (period >> 8) & 0xFF;
    _tempData[1] = (period >> 0) & 0xFF;
    for (uint8_t i = 0; i < RETRY_COUNT; i++) {
        result = writeReg(reg, _tempData, 2);
        if (result == 0) return;
        delay(I2C_RETRY_DELAY_MS);
    }
}

uint8_t DFR1216Board::getBattery(void)
{
    uint8_t result = 0;
    uint8_t _tempData[TEMP_LEN] = {0};
    for (uint8_t i = 0; i < RETRY_COUNT; i++) {
        result = readReg(I2C_BATTERY, _tempData, 1);
        if (result == 0) return _tempData[0];
        delay(I2C_RETRY_DELAY_MS);
    }
    return 0xFF;
}

uint32_t DFR1216Board::getIRData(void)
{
    uint8_t result = 0;
    uint8_t _tempData[TEMP_LEN] = {0};
    for (uint8_t i = 0; i < RETRY_COUNT; i++) {
        result = readReg(I2C_IR_R_STATE, _tempData, 5);
        if (result == 0) {
            if (_tempData[0] == DATA_DISABLE) return 0x00000000;
            return ((uint32_t)_tempData[1] << 24) | ((uint32_t)_tempData[2] << 16) |
                   ((uint32_t)_tempData[3] << 8)  |  _tempData[4];
        }
        delay(I2C_RETRY_DELAY_MS);
    }
    return 0xFFFFFFFF;
}

uint8_t DFR1216Board::sendIR(uint32_t data)
{
    uint8_t result = 0;
    uint8_t _tempData[TEMP_LEN] = {0};
    _tempData[0] = DATA_ENABLE;
    _tempData[1] = (data >> 24) & 0xFF;
    _tempData[2] = (data >> 16) & 0xFF;
    _tempData[3] = (data >> 8)  & 0xFF;
    _tempData[4] =  data        & 0xFF;
    for (uint8_t i = 0; i < RETRY_COUNT; i++) {
        result = writeReg(I2C_IR_S_STATE, _tempData, 5);
        if (result == 0) return 0;
        delay(I2C_RETRY_DELAY_MS);
    }
    return 0xFF;
}

uint8_t DFR1216Board::setWS2812(uint32_t *data, uint8_t bright)
{
    uint8_t result = 0;
    uint8_t _tempData[TEMP_LEN] = {0};
    _tempData[0] = DATA_ENABLE;
    _tempData[1] = bright;
    _tempData[2] = (data[0] >> 16) & 0xFF;
    _tempData[3] = (data[0] >> 8)  & 0xFF;
    _tempData[4] = (data[0] >> 0)  & 0xFF;
    _tempData[5] = (data[1] >> 16) & 0xFF;
    _tempData[6] = (data[1] >> 8)  & 0xFF;
    _tempData[7] = (data[1] >> 0)  & 0xFF;
    for (uint8_t i = 0; i < RETRY_COUNT; i++) {
        result = writeReg(I2C_WS2812_STATE, _tempData, 8);
        if (result == 0) return 0;
        delay(I2C_RETRY_DELAY_MS);
    }
    return 0xFF;
}

uint8_t DFR1216Board::setMode(eIONumber_t number, eIOType_t mode)
{
    uint8_t result = 0;
    uint8_t reg = I2C_IO_MODE_C0 + number;
    uint8_t _tempData[TEMP_LEN] = {0};
    _tempData[0] = mode;
    for (uint8_t i = 0; i < RETRY_COUNT; i++) {
        result = writeReg(reg, _tempData, 1);
        if (result == 0) { delay(10); return 0; }
        delay(I2C_RETRY_DELAY_MS);
    }
    return 0xFF;
}

uint8_t DFR1216Board::setGpioState(eIONumber_t number, eGpioState_t state)
{
    uint8_t result = 0;
    uint8_t reg = I2C_W_C0 + number;
    uint8_t _tempData[TEMP_LEN] = {0};
    _tempData[0] = state;
    for (uint8_t i = 0; i < RETRY_COUNT; i++) {
        result = writeReg(reg, _tempData, 1);
        if (result == 0) { delay(10); return 0; }
        delay(I2C_RETRY_DELAY_MS);
    }
    return 0xFF;
}

uint8_t DFR1216Board::getGpioState(eIONumber_t number)
{
    uint8_t result = 0;
    uint8_t reg = I2C_R_C0 + number;
    uint8_t _tempData[TEMP_LEN] = {0};
    for (uint8_t i = 0; i < RETRY_COUNT; i++) {
        result = readReg(reg, _tempData, 1);
        if (result == 0) return _tempData[0];
        delay(I2C_RETRY_DELAY_MS);
    }
    return 0xFF;
}

uint16_t DFR1216Board::getADCValue(eIONumber_t number)
{
    uint8_t  result = 0;
    uint8_t  reg    = I2C_ADC_C0_S + number * 3;
    uint8_t  _tempData[TEMP_LEN] = {0};
    for (uint8_t i = 0; i < RETRY_COUNT; i++) {
        result = readReg(reg, _tempData, 3);
        if (result == 0) {
            if (_tempData[0] == DATA_ENABLE) {
                uint16_t v = ((uint16_t)_tempData[1] << 8) | _tempData[2];
                if (v > 3900) v = 4095;
                else if (v < 40) v = 0;
                return v;
            } else if (_tempData[0] == MODE_ERROR) {
                return 0xFFFF;
            }
        }
        delay(I2C_RETRY_DELAY_MS);
    }
    return 0xFFFF;
}

sDhtData_t DFR1216Board::getDHTValue(eIONumber_t number)
{
    sDhtData_t dhtData = {0.0f, 0.0f, 0};
    uint8_t reg = I2C_DHT_C0_S + number * 5;
    uint8_t _tempData[TEMP_LEN] = {0};
    _tempData[0] = DATA_ENABLE;
    for (uint8_t i = 0; i < RETRY_COUNT; i++) {
        if (writeReg(reg, _tempData, 1) == 0) break;
        delay(I2C_RETRY_DELAY_MS);
    }
    delay(30);
    for (uint8_t i = 0; i < RETRY_COUNT; i++) {
        if (readReg(reg, _tempData, 5) == 0) {
            if (_tempData[0] == DATA_ENABLE) {
                dhtData.temperature = (_tempData[1] & 0x80)
                    ? -((float)(_tempData[1] & 0x7F) + (float)_tempData[2] * 0.01f)
                    :   (float)_tempData[1] + (float)_tempData[2] * 0.01f;
                dhtData.humidity = (float)_tempData[3] + (float)_tempData[4] * 0.01f;
                dhtData.state    = _tempData[0];
                return dhtData;
            } else if (_tempData[0] == MODE_ERROR) {
                dhtData.state = MODE_ERROR;
                return dhtData;
            }
        }
        delay(30);
    }
    return dhtData;
}

float DFR1216Board::get18b20Value(eIONumber_t number)
{
    uint8_t reg = I2C_18B20_C0_S + number * 3;
    uint8_t _tempData[TEMP_LEN] = {0};
    _tempData[0] = DATA_ENABLE;
    for (uint8_t i = 0; i < RETRY_COUNT; i++) {
        if (writeReg(reg, _tempData, 1) == 0) break;
        delay(I2C_RETRY_DELAY_MS);
    }
    delay(50);
    for (uint8_t i = 0; i < RETRY_COUNT; i++) {
        if (readReg(reg, _tempData, 3) == 0) {
            if (_tempData[0] == DATA_ENABLE) {
                if (_tempData[1] == 0xFF && _tempData[2] == 0xFF) return 0.0f;
                float sign = (_tempData[1] & 0x80) ? -1.0f : 1.0f;
                _tempData[1] &= 0x7F;
                return sign * ((_tempData[1] * 256 + _tempData[2]) / 16.0f);
            } else if (_tempData[0] == MODE_ERROR) {
                return 0.0f;
            }
        }
        delay(30);
    }
    return 0.0f;
}

int16_t DFR1216Board::getSr04Distance(void)
{
    uint8_t reg = I2C_SR04_STATE;
    uint8_t _tempData[TEMP_LEN] = {0};
    _tempData[0] = SR04_COLLECT;
    for (uint8_t i = 0; i < RETRY_COUNT; i++) {
        if (writeReg(reg, _tempData, 1) == 0) break;
        delay(I2C_RETRY_DELAY_MS);
    }
    delay(30);
    for (uint8_t i = 0; i < RETRY_COUNT; i++) {
        if (readReg(reg, _tempData, 3) == 0) {
            if (_tempData[0] == SR04_COMPLETE)
                return (int16_t)(((uint16_t)_tempData[1] << 8) | _tempData[2]);
        }
        delay(30);
    }
    return -1;
}

// ---------------------------------------------------------------------------
// DFR1216Board constructor / destructor
// ---------------------------------------------------------------------------

DFR1216Board::DFR1216Board() {}
DFR1216Board::~DFR1216Board() {}

// ---------------------------------------------------------------------------
// DFR1216_I2C — constructor, begin(), writeReg(), readReg()
// ---------------------------------------------------------------------------

SemaphoreHandle_t DFR1216_I2C::__i2c_mutex = nullptr;

DFR1216_I2C::DFR1216_I2C(TwoWire *pWire, uint8_t addr)
{
    __pWire      = pWire;
    __I2C_addr   = addr;
}

bool DFR1216_I2C::begin()
{
    if (!__i2c_mutex)
        __i2c_mutex = xSemaphoreCreateRecursiveMutex();

    uint8_t retry    = 0;
    uint8_t tempData = DATA_ENABLE;

    __pWire->begin();
    __pWire->setClock(400000);
    __pWire->beginTransmission(__I2C_addr);
    if (__pWire->endTransmission() != 0)
        return false;

    // Reset all sensors on the board
    writeReg(I2C_RESET_SENSOR, &tempData, 1);
    delay(20);

    while (true)
    {
        __pWire->begin();
        __pWire->beginTransmission(__I2C_addr);
        if (__pWire->endTransmission() == 0)
            return true;
        if (++retry > 100)
            return false;
        delay(10);
    }
}

uint8_t DFR1216_I2C::writeReg(uint8_t reg, uint8_t *data, uint8_t len)
{
    if (__i2c_mutex) xSemaphoreTakeRecursive(__i2c_mutex, portMAX_DELAY);
    __pWire->beginTransmission(__I2C_addr);
    __pWire->write(reg);
    for (uint8_t i = 0; i < len; ++i)
        __pWire->write(data[i]);
    uint8_t result = __pWire->endTransmission();
    if (__i2c_mutex) xSemaphoreGiveRecursive(__i2c_mutex);
    return result;
}

int16_t DFR1216_I2C::readReg(uint8_t reg, uint8_t *data, uint8_t len)
{
    if (__i2c_mutex) xSemaphoreTakeRecursive(__i2c_mutex, portMAX_DELAY);
    if (writeReg(reg, nullptr, 0) != 0)
    {
        if (__i2c_mutex) xSemaphoreGiveRecursive(__i2c_mutex);
        return -1;
    }
    __pWire->requestFrom(static_cast<uint8_t>(__I2C_addr), static_cast<uint8_t>(len));
    uint8_t i = 0;
    while (__pWire->available())
        data[i++] = __pWire->read();
    if (__i2c_mutex) xSemaphoreGiveRecursive(__i2c_mutex);
    return (i == len) ? 0 : -1;
}
