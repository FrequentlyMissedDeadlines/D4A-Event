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
 * 
 */

#include "ServoService.h"
#include "../ResponseHelper.h"
#include <WebServer.h>
#include <pgmspace.h>
#include <ArduinoJson.h>
#include "../../devices/DFR1216/DFRobot_UnihikerExpansion.h"
#include "../settings/SettingsService.h"

constexpr uint8_t MAX_SERVO_CHANNELS = 8;

DFRobot_UnihikerExpansion_I2C servoController = DFRobot_UnihikerExpansion_I2C();

extern SettingsService settings_service;

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
    constexpr const char req_channel_speed[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"channel\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":7},\"speed\":{\"type\":\"integer\",\"minimum\":-100,\"maximum\":100}},\"required\":[\"channel\",\"speed\"]}";
    constexpr const char req_angle[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"angle\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":360}},\"required\":[\"angle\"]}";
    constexpr const char req_speed[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"speed\":{\"type\":\"integer\",\"minimum\":-100,\"maximum\":100}},\"required\":[\"speed\"]}";
    constexpr const char req_channel_connection[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"channel\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":7},\"connection\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":3,\"description\":\"0=None, 1=continuous, 2=angular 180 degree, 3=angular 270 degrees\"}},\"required\":[\"channel\",\"connection\"]}";
    constexpr const char req_servos_speed_multiple[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"servos\":{\"type\":\"array\",\"items\":{\"type\":\"object\",\"properties\":{\"channel\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":7},\"speed\":{\"type\":\"integer\",\"minimum\":-100,\"maximum\":100}},\"required\":[\"channel\",\"speed\"]}}},\"required\":[\"servos\"]}";
    constexpr const char req_servos_angle_multiple[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"servos\":{\"type\":\"array\",\"items\":{\"type\":\"object\",\"properties\":{\"channel\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":7},\"angle\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":360}},\"required\":[\"channel\",\"angle\"]}}},\"required\":[\"servos\"]}";

    // Example values
    constexpr const char ex_channel_angle[] PROGMEM = "{\"channel\":0,\"angle\":90}";
    constexpr const char ex_channel_speed[] PROGMEM = "{\"channel\":0,\"speed\":50}";
    constexpr const char ex_channel_status[] PROGMEM = "{\"channel\":0,\"status\":\"ANGULAR_180\"}";
    constexpr const char ex_all_servos[] PROGMEM = "{\"servos\":[{\"channel\":0,\"status\":\"ANGULAR_180\"},{\"channel\":1,\"status\":\"NOT_CONNECTED\"}]}";
    constexpr const char ex_angle[] PROGMEM = "{\"angle\":90}";
    constexpr const char ex_speed[] PROGMEM = "{\"speed\":50}";
    constexpr const char ex_channel_connection[] PROGMEM = "{\"channel\":0,\"connection\":0}";
    constexpr const char ex_servos_speed_multiple[] PROGMEM = "{\"servos\":[{\"channel\":0,\"speed\":50},{\"channel\":1,\"speed\":-30}]}";
    constexpr const char ex_servos_angle_multiple[] PROGMEM = "{\"servos\":[{\"channel\":0,\"angle\":90},{\"channel\":1,\"angle\":180}]}";
    constexpr const char ex_result_ok[] PROGMEM = "{\"result\":\"ok\",\"message\":\"setServoAngle\"}";
}

std::array<ServoConnection, MAX_SERVO_CHANNELS> attached_servos = {NOT_CONNECTED, NOT_CONNECTED, NOT_CONNECTED, NOT_CONNECTED,
                                                                   NOT_CONNECTED, NOT_CONNECTED, NOT_CONNECTED, NOT_CONNECTED};

bool ServoService::initializeService()
{
    logger->info(fpstr_to_string(FPSTR(ServoConsts::msg_initializing)));
    if (servoController.begin())
    {
        logger->info(fpstr_to_string(FPSTR(ServoConsts::msg_initialized_success)));
        setServiceStatus(INITIALIZED);
    }
    else
    {
        logger->warning(fpstr_to_string(FPSTR(ServoConsts::msg_issue_detected)));
        setServiceStatus(INITIALIZED_FAILED);
    }

    // Return true to allow other services to continue even if servo fails
    return true;
}

bool ServoService::startService()
{

    if (isServiceInitialized())
    {
                
#ifdef VERBOSE_DEBUG
        logger->debug(getServiceName() + fpstr_to_string(FPSTR(RoutesConsts::str_space)) + getStatusString());   
#endif
    }
    else
    {
        setServiceStatus(START_FAILED);
        logger->error(getServiceName() + fpstr_to_string(FPSTR(ServoConsts::msg_start_failed)));
        return false;
    }
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
            throw std::out_of_range(fpstr_to_string(FPSTR(ServoConsts::err_channel_range)));
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
bool ServoService::setServoSpeed(uint8_t channel, int8_t speed)
{
    if (!isServiceStarted())
    {
        return false;
    }

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
        return true;
    }
    catch (const std::exception &e)
    {
        logger->error(e.what());
        return false;
    }
}

bool ServoService::setAllServoSpeed(int8_t speed)
{
    bool allSuccess = true;
    for (uint8_t channel = 0; channel < MAX_SERVO_CHANNELS; channel++)
    {
        if (attached_servos[channel] == ServoConnection::ROTATIONAL)
        {
            allSuccess = allSuccess && this->setServoSpeed(channel, speed);
        }
    }
    return allSuccess;
}

bool ServoService::setAllServoAngle(u_int16_t angle)
{
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
bool ServoService::setServosSpeedMultiple(const std::vector<ServoSpeedOp>& ops)
{
    if (!isServiceStarted() || ops.empty())
        return false;
    bool all_success = true;
    for (const auto& op : ops) {
        if (!setServoSpeed(op.channel, op.speed))
            all_success = false;
    }
    return all_success;
}

/**
 * @brief Set angle for multiple servos at once
 * @param ops Vector of ServoAngleOp operations
 * @return true if all operations successful, false otherwise
 */
bool ServoService::setServosAngleMultiple(const std::vector<ServoAngleOp>& ops)
{
    if (!isServiceStarted() || ops.empty())
        return false;
    bool all_success = true;
    for (const auto& op : ops) {
        if (!setServoAngle(op.channel, op.angle))
            all_success = false;
    }
    return all_success;
}

bool ServoService::stopService()
{
    // Stop all servos when service stops
setServiceStatus(STOPPED);
    return false;
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
bool ServoService::addRouteSetServoAngle(const std::vector<OpenAPIResponse>& standard_responses)
{
    std::string path = std::string(RoutesConsts::path_api) + getServiceSubPath() + fpstr_to_string(FPSTR(RoutesConsts::str_slash)) + ServoConsts::action_set_angle;
    logRouteRegistration(path);

    OpenAPIRoute angle_route(path.c_str(), RoutesConsts::method_post, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_set_angle)), reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)), false, {}, standard_responses);
    angle_route.requestBody = OpenAPIRequestBody(reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_servo_angle_control)),
                                                ServoConsts::req_channel_angle_07, true);
    angle_route.requestBody.example = ServoConsts::ex_channel_angle;
    registerOpenAPIRoute(angle_route);

    webserver.on(path.c_str(), HTTP_POST, [this]()
    {
        if (!checkServiceStarted()) return;
        
        String body = webserver.arg(FPSTR(ServoConsts::str_plain));
        
        if (body.isEmpty())
        {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_params);
            return;
        }
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, body.c_str());
        
        if (error || !doc[ServoConsts::servo_channel].is<uint8_t>() || !doc[ServoConsts::servo_angle].is<uint16_t>())
        {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_params);
            return;
        }

        uint8_t ch = doc[ServoConsts::servo_channel].as<uint8_t>();
        uint16_t angle = doc[ServoConsts::servo_angle].as<uint16_t>();

        if (angle > 360 || ch > 7)
        {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_values);
            return;
        }

        if (setServoAngle(ch, angle))
        {
            ResponseHelper::sendSuccess(FPSTR(ServoConsts::action_set_angle));
        }
        else
        {
            ResponseHelper::sendError(ResponseHelper::OPERATION_FAILED, FPSTR(ServoConsts::action_set_angle));
        }
    });

    return true;
}

/**
 * @brief Add route for setting servo speed
 */
bool ServoService::addRouteSetServoSpeed(const std::vector<OpenAPIResponse>& standard_responses)
{
    std::string path = std::string(RoutesConsts::path_api) + getServiceSubPath() + fpstr_to_string(FPSTR(RoutesConsts::str_slash)) + ServoConsts::action_set_speed;
    logRouteRegistration(path);

    OpenAPIRoute speed_route(path.c_str(), RoutesConsts::method_post, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_set_speed)), reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)), false, {}, standard_responses);
    speed_route.requestBody = OpenAPIRequestBody(reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_servo_speed_control)),
                                                ServoConsts::req_channel_speed, true);
    speed_route.requestBody.example = ServoConsts::ex_channel_speed;
    registerOpenAPIRoute(speed_route);

    webserver.on(path.c_str(), HTTP_POST, [this]()
    {
        if (!checkServiceStarted()) return;
        
        String body = webserver.arg(FPSTR(ServoConsts::str_plain));
        
        if (body.isEmpty())
        {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_params);
            return;
        }
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, body.c_str());
        
        if (error || !doc[ServoConsts::servo_channel].is<int>() || !doc[ServoConsts::servo_speed].is<int>())
        {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_params);
            return;
        }
        
        uint8_t channel = doc[ServoConsts::servo_channel].as<uint8_t>();
        int8_t speed = doc[ServoConsts::servo_speed].as<int>();
        
        if (channel > 7 || speed < -100 || speed > 100)
        {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_values);
            return;
        }
        
        if (this->setServoSpeed(channel, speed))
        {
            ResponseHelper::sendSuccess(FPSTR(ServoConsts::action_set_speed));
        }
        else
        {
            ResponseHelper::sendError(ResponseHelper::OPERATION_FAILED, FPSTR(ServoConsts::action_set_speed));
        }
    });

    return true;
}

/**
 * @brief Add route for stopping all servos
 */
bool ServoService::addRouteStopAll(const std::vector<OpenAPIResponse>& standard_responses)
{
    std::string path = std::string(RoutesConsts::path_api) + getServiceSubPath() + fpstr_to_string(FPSTR(RoutesConsts::str_slash)) + ServoConsts::action_stop_all;
    logRouteRegistration(path);

    OpenAPIRoute stop_all_route(path.c_str(), RoutesConsts::method_post, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_stop_all)), reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)), false, {}, standard_responses);
    registerOpenAPIRoute(stop_all_route);

    webserver.on(path.c_str(), HTTP_POST, [this]()
    {
        if (!checkServiceStarted()) return;
        
        if (this->setAllServoSpeed(0))
        {
            ResponseHelper::sendSuccess(ServoConsts::action_stop_all);
        }
        else
        {
            ResponseHelper::sendError(ResponseHelper::OPERATION_FAILED, ServoConsts::action_stop_all);
        }
    });

    return true;
}

/**
 * @brief Add route for getting servo status
 */
bool ServoService::addRouteGetStatus(const std::vector<OpenAPIResponse>& standard_responses)
{
    std::string path = std::string(RoutesConsts::path_api) + getServiceSubPath() + fpstr_to_string(FPSTR(RoutesConsts::str_slash)) + ServoConsts::action_get_status;
    logRouteRegistration(path);

    std::vector<OpenAPIParameter> status_params;
    status_params.push_back(OpenAPIParameter(ServoConsts::param_channel, RoutesConsts::type_integer, RoutesConsts::in_query, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_servo_channel)), true));

    std::vector<OpenAPIResponse> status_responses;
    OpenAPIResponse status_ok(200, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_status_retrieved)));
    status_ok.schema = ServoConsts::schema_channel_status;
    status_ok.example = ServoConsts::ex_channel_status;
    status_responses.push_back(status_ok);
    status_responses.push_back(createMissingParamsResponse());

    OpenAPIRoute status_route(path.c_str(), RoutesConsts::method_get, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_get_status)), reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)), true, status_params, status_responses);
    registerOpenAPIRoute(status_route);

    webserver.on(path.c_str(), HTTP_GET, [this]()
    {
        if (!checkServiceStarted()) return;
        
        if (!webserver.hasArg(ServoConsts::servo_channel))
        {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_params);
            return;
        }

        uint8_t channel = (uint8_t)webserver.arg(ServoConsts::servo_channel).toInt();
        
        if (channel > 7)
        {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_values);
            return;
        }
        
        std::string status = getAttachedServo(channel);
        webserver.send(200, RoutesConsts::mime_json, status.c_str());
    });

    return true;
}

/**
 * @brief Add route for getting all servos status
 */
bool ServoService::addRouteGetAllStatus()
{
    std::string path = std::string(RoutesConsts::path_api) + getServiceSubPath() + fpstr_to_string(FPSTR(RoutesConsts::str_slash)) + ServoConsts::action_get_all_status;
    logRouteRegistration(path);

    std::vector<OpenAPIResponse> all_status_responses;
    OpenAPIResponse all_status_ok(200, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_all_status_retrieved)));
    all_status_ok.schema = ServoConsts::schema_all_servos;
    all_status_ok.example = ServoConsts::ex_all_servos;
    all_status_responses.push_back(all_status_ok);

    OpenAPIRoute all_status_route(path.c_str(), RoutesConsts::method_get, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_get_all_status)), reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)), false, {}, all_status_responses);
    registerOpenAPIRoute(all_status_route);

    webserver.on(path.c_str(), HTTP_GET, [this]()
    {
        if (!checkServiceStarted()) return;
        
        std::string status = getAllAttachedServos();
        webserver.send(200, RoutesConsts::mime_json, status.c_str());
    });

    return true;
}

/**
 * @brief Add route for setting all servos to same angle
 */
bool ServoService::addRouteSetAllAngle(const std::vector<OpenAPIResponse>& standard_responses)
{
    std::string path = std::string(RoutesConsts::path_api) + getServiceSubPath() + fpstr_to_string(FPSTR(RoutesConsts::str_slash)) + ServoConsts::action_set_all_angle;
    logRouteRegistration(path);

    OpenAPIRoute all_angle_route(path.c_str(), RoutesConsts::method_post, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_set_all_angle)), reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)), false, {}, standard_responses);
    all_angle_route.requestBody = OpenAPIRequestBody(reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_angle_for_all)),
                                                   ServoConsts::req_angle, true);
    all_angle_route.requestBody.example = ServoConsts::ex_angle;
    registerOpenAPIRoute(all_angle_route);

    webserver.on(path.c_str(), HTTP_POST, [this]()
    {
        if (!checkServiceStarted()) return;
        
        String body = webserver.arg(FPSTR(ServoConsts::str_plain));
        
        if (body.isEmpty())
        {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_params);
            return;
        }
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, body.c_str());
        
        if (error || !doc[ServoConsts::servo_angle].is<uint16_t>())
        {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_params);
            return;
        }

        uint16_t angle = doc[ServoConsts::servo_angle].as<uint16_t>();
        
        if (angle > 360)
        {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_values);
            return;
        }
        
        if (setAllServoAngle(angle))
        {
            ResponseHelper::sendSuccess(FPSTR(ServoConsts::action_set_all_angle));
        }
        else
        {
            ResponseHelper::sendError(ResponseHelper::OPERATION_FAILED, FPSTR(ServoConsts::action_set_all_angle));
        }
    });

    return true;
}

/**
 * @brief Add route for setting all servos to same speed
 */
bool ServoService::addRouteSetAllSpeed(const std::vector<OpenAPIResponse>& standard_responses)
{
    std::string path = std::string(RoutesConsts::path_api) + getServiceSubPath() + fpstr_to_string(FPSTR(RoutesConsts::str_slash)) + ServoConsts::action_set_all_speed;
    logRouteRegistration(path);

    OpenAPIRoute all_speed_route(path.c_str(), RoutesConsts::method_post, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_set_all_speed)), reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)), false, {}, standard_responses);
    all_speed_route.requestBody = OpenAPIRequestBody(reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_speed_for_all)),
                                                   ServoConsts::req_speed, true);
    all_speed_route.requestBody.example = ServoConsts::ex_speed;
    registerOpenAPIRoute(all_speed_route);

    webserver.on(path.c_str(), HTTP_POST, [this]()
    {
        if (!checkServiceStarted()) return;
        
        String body = webserver.arg(FPSTR(ServoConsts::str_plain));
        
        if (body.isEmpty())
        {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_params);
            return;
        }
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, body.c_str());
        
        if (error || !doc[ServoConsts::servo_speed].is<int8_t>())
        {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_params);
            return;
        }

        int8_t speed = doc[ServoConsts::servo_speed].as<int8_t>();
        
        if (speed < -100 || speed > 100)
        {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_values);
            return;
        }
        
        if (setAllServoSpeed(speed))
        {
            ResponseHelper::sendSuccess(FPSTR(ServoConsts::action_set_all_speed));
        }
        else
        {
            ResponseHelper::sendError(ResponseHelper::OPERATION_FAILED, FPSTR(ServoConsts::action_set_all_speed));
        }
    });

    return true;
}

/**
 * @brief Add route for setting multiple servos speed at once
 */
bool ServoService::addRouteSetServosSpeedMultiple(const std::vector<OpenAPIResponse>& standard_responses)
{
    std::string path = std::string(RoutesConsts::path_api) + getServiceSubPath() + fpstr_to_string(FPSTR(RoutesConsts::str_slash)) + ServoConsts::action_set_servos_speed_multiple;
#ifdef VERBOSE_DEBUG
    logger->debug(fpstr_to_string(FPSTR(RoutesConsts::str_plus)) + path);
#endif    
    logRouteRegistration(path);
    OpenAPIRoute multi_speed_route(path.c_str(), RoutesConsts::method_post, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_set_servos_speed_multiple)), reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)), false, {}, standard_responses);
    multi_speed_route.requestBody = OpenAPIRequestBody(reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_set_servos_speed_multiple)),
                                                     ServoConsts::req_servos_speed_multiple, true);
    multi_speed_route.requestBody.example = ServoConsts::ex_servos_speed_multiple;
    registerOpenAPIRoute(multi_speed_route);

    webserver.on(path.c_str(), HTTP_POST, [this]()
    {
        if (!checkServiceStarted()) return;
        
        String body = webserver.arg(FPSTR(ServoConsts::str_plain));
        
        if (body.isEmpty())
        {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_params);
            return;
        }
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, body.c_str());
        if (error || !doc[ServoConsts::servos].is<JsonArray>())
        {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_params);
            return;
        }

        JsonArray arr = doc[ServoConsts::servos].as<JsonArray>();
        std::vector<ServoSpeedOp> ops;
        for (JsonObject servo_obj : arr)
        {
            if (!servo_obj[ServoConsts::servo_channel].is<uint8_t>() || !servo_obj[ServoConsts::servo_speed].is<int8_t>())
                continue;
            ServoSpeedOp op;
            op.channel = servo_obj[ServoConsts::servo_channel].as<uint8_t>();
            op.speed = servo_obj[ServoConsts::servo_speed].as<int8_t>();
            ops.push_back(op);
        }

        if (ops.empty())
        {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_params);
            return;
        }

        if (setServosSpeedMultiple(ops))
        {
            ResponseHelper::sendSuccess(FPSTR(ServoConsts::action_set_servos_speed_multiple));
        }
        else
        {
            ResponseHelper::sendError(ResponseHelper::OPERATION_FAILED, FPSTR(ServoConsts::action_set_servos_speed_multiple));
        }
    });

    return true;
}

/**
 * @brief Add route for setting multiple servos angle at once
 */
bool ServoService::addRouteSetServosAngleMultiple(const std::vector<OpenAPIResponse>& standard_responses)
{
    std::string path = std::string(RoutesConsts::path_api) + getServiceSubPath() + fpstr_to_string(FPSTR(RoutesConsts::str_slash)) + ServoConsts::action_set_servos_angle_multiple;
#ifdef VERBOSE_DEBUG
    logger->debug(fpstr_to_string(FPSTR(RoutesConsts::str_plus)) + path);
    #endif
    logRouteRegistration(path);
    OpenAPIRoute multi_angle_route(path.c_str(), RoutesConsts::method_post, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_set_servos_angle_multiple)), reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)), false, {}, standard_responses);
    multi_angle_route.requestBody = OpenAPIRequestBody(reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_set_servos_angle_multiple)),
                                                     ServoConsts::req_servos_angle_multiple, true);
    multi_angle_route.requestBody.example = ServoConsts::ex_servos_angle_multiple;
    registerOpenAPIRoute(multi_angle_route);

    webserver.on(path.c_str(), HTTP_POST, [this]()
    {
        if (!checkServiceStarted()) return;
        
        String body = webserver.arg(FPSTR(ServoConsts::str_plain));
        
        if (body.isEmpty())
        {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_params);
            return;
        }
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, body.c_str());
        if (error || !doc[ServoConsts::servos].is<JsonArray>())
        {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_params);
            return;
        }

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
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_params);
            return;
        }

        if (setServosAngleMultiple(ops))
        {
            ResponseHelper::sendSuccess(FPSTR(ServoConsts::action_set_servos_angle_multiple));
        }
        else
        {
            ResponseHelper::sendError(ResponseHelper::OPERATION_FAILED, FPSTR(ServoConsts::action_set_servos_angle_multiple));
        }
    });

    return true;
}

/**
 * @brief Add route for attaching servo to a channel
 */
bool ServoService::addRouteAttachServo(const std::vector<OpenAPIResponse>& standard_responses)
{
    std::string path = getPath(ServoConsts::action_attach_servo);
#ifdef VERBOSE_DEBUG
    logger->debug(fpstr_to_string(FPSTR(RoutesConsts::str_plus)) + path);
#endif
    logRouteRegistration(path);
    OpenAPIRoute attach_route(path.c_str(), RoutesConsts::method_post, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_attach_servo)), reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)), false, {}, standard_responses);
    attach_route.requestBody = OpenAPIRequestBody(reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_attachment_config)),
                                                 ServoConsts::req_channel_connection, true);
    attach_route.requestBody.example = ServoConsts::ex_channel_connection;
    registerOpenAPIRoute(attach_route);

    webserver.on(path.c_str(), HTTP_POST, [this]()
    {
        if (!checkServiceStarted()) return;
        
        String body = webserver.arg(FPSTR(ServoConsts::str_plain));
        
        if (body.isEmpty())
        {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_params);
            return;
        }
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, body.c_str());
        
        if (error || !doc[ServoConsts::servo_channel].is<uint8_t>() || !doc[ServoConsts::connection].is<uint8_t>())
        {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_params);
            return;
        }
        
        uint8_t channel = doc[ServoConsts::servo_channel].as<uint8_t>();
        uint8_t connection = doc[ServoConsts::connection].as<uint8_t>();
        
        if (channel > 7 || connection > 3)
        {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_values);
            return;
        }
        
        ServoConnection servo_connection = static_cast<ServoConnection>(connection);
        if (attachServo(channel, servo_connection))
        {
            ResponseHelper::sendSuccess(FPSTR(ServoConsts::action_attach_servo));
        }
        else
        {
            ResponseHelper::sendError(ResponseHelper::OPERATION_FAILED, FPSTR(ServoConsts::action_attach_servo));
        }
    });

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

    registerSettingsRoutes(reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servo)), this);

    return true;
}

std::string ServoService::getServiceName()
{
    return fpstr_to_string(FPSTR(ServoConsts::str_service_name));
}

std::string ServoService::getServiceSubPath()
{
    return fpstr_to_string(FPSTR(ServoConsts::path_service));
}

bool ServoService::saveSettings()
{
    if (!settings_service_) {
        if (logger) {
            logger->error("Servo Service: Settings service not available");
        }
        return false;
    }
    
    std::string comma = fpstr_to_string(FPSTR(ServoConsts::str_comma));
    return settings_service_->setSetting(getServiceName(), reinterpret_cast<const char *>(FPSTR(ServoConsts::settings_key_servos)), std::to_string(static_cast<int>(attached_servos[0])) + comma + std::to_string(static_cast<int>(attached_servos[1])) + comma + std::to_string(static_cast<int>(attached_servos[2])) + comma + std::to_string(static_cast<int>(attached_servos[3])) + comma + std::to_string(static_cast<int>(attached_servos[4])) + comma + std::to_string(static_cast<int>(attached_servos[5])) + comma + std::to_string(static_cast<int>(attached_servos[6])) + comma + std::to_string(static_cast<int>(attached_servos[7])));
}

bool ServoService::loadSettings()
{
    if (!settings_service_) {
        if (logger) {
            logger->error("Servo Service: Settings service not available");
        }
        return false;
    }

    std::string attached_servos_settings = settings_service_->getSetting(getServiceName(), reinterpret_cast<const char *>(FPSTR(ServoConsts::settings_key_servos)));
    if (attached_servos_settings.empty())
    {
        logger->info(fpstr_to_string(FPSTR(ServoConsts::msg_no_saved_settings)));
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

    logger->info(fpstr_to_string(FPSTR(ServoConsts::msg_loaded_settings)));
    return true;
}
