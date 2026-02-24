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

#include "services/ServoService.h"
#include "ResponseHelper.h"
#include <ESPAsyncWebServer.h>
#include <pgmspace.h>
#include <ArduinoJson.h>
#include "DFR1216/DFRobot_UnihikerExpansion.h"
#include "services/SettingsService.h"
#include "services/UDPService.h"

constexpr uint8_t MAX_SERVO_CHANNELS = 8;

DFRobot_UnihikerExpansion_I2C servoController = DFRobot_UnihikerExpansion_I2C();

extern SettingsService settings_service;
extern UDPService      udp_service;

// Module-level static JSON documents — zero heap allocation on hot UDP path
static JsonDocument s_udp_req;   ///< Reused for incoming UDP JSON payload
static JsonDocument s_udp_resp;  ///< Reused for outgoing UDP JSON response

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

    // UDP message handling
    constexpr const char msg_udp_bad_format[]    PROGMEM = "UDP bad message format";
    constexpr const char msg_udp_unknown_cmd[]   PROGMEM = "UDP unknown command";
    constexpr const char msg_udp_json_error[]    PROGMEM = "UDP JSON parse error";
    constexpr const char udp_field_result[]      PROGMEM = "result";
    constexpr const char udp_field_message[]     PROGMEM = "message";
    constexpr const char udp_val_ok[]            PROGMEM = "ok";
    constexpr const char udp_val_error[]         PROGMEM = "error";
}

std::array<ServoConnection, MAX_SERVO_CHANNELS> attached_servos = {NOT_CONNECTED, NOT_CONNECTED, NOT_CONNECTED, NOT_CONNECTED,
                                                                   NOT_CONNECTED, NOT_CONNECTED, NOT_CONNECTED, NOT_CONNECTED};

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

    webserver.on(path.c_str(), HTTP_POST, 
        [this](AsyncWebServerRequest *request)
        {
            if (!checkServiceStarted(request)) return;
            
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
            }
        },
        nullptr,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonBodyParser::storeBody(request, data, len, index, total);
        }
    );

    return true;
}

/**
 * @brief Add route for setting servo speed
 */
bool ServoService::addRouteSetServoSpeed(const std::vector<OpenAPIResponse>& standard_responses)
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

    webserver.on(path.c_str(), HTTP_POST, 
        [this](AsyncWebServerRequest *request)
        {
            if (!checkServiceStarted(request)) return;
            
            // Parse and validate JSON body
            JsonDocument doc;
            if (!JsonBodyParser::parseBody(request, doc, [](const JsonDocument& d) {
                return d[ServoConsts::servo_channel].is<int>() && 
                       d[ServoConsts::servo_speed].is<int>();
            })) return;
            
            uint8_t channel = doc[ServoConsts::servo_channel].as<uint8_t>();
            int8_t speed = doc[ServoConsts::servo_speed].as<int>();
            
            if (channel > 7 || speed < -100 || speed > 100)
            {
                ResponseHelper::sendError(request, ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_values);
                return;
            }
            
            if (this->setServoSpeed(channel, speed))
            {
                ResponseHelper::sendSuccess(request, FPSTR(ServoConsts::action_set_speed));
            }
            else
            {
                ResponseHelper::sendError(request, ResponseHelper::OPERATION_FAILED, FPSTR(ServoConsts::action_set_speed));
            }
        },
        nullptr,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonBodyParser::storeBody(request, data, len, index, total);
        }
    );

    return true;
}

/**
 * @brief Add route for stopping all servos
 */
bool ServoService::addRouteStopAll(const std::vector<OpenAPIResponse>& standard_responses)
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
        if (!checkServiceStarted(request)) return;
        
        if (this->setAllServoSpeed(0))
        {
            ResponseHelper::sendSuccess(request, ServoConsts::action_stop_all);
        }
        else
        {
            ResponseHelper::sendError(request, ResponseHelper::OPERATION_FAILED, ServoConsts::action_stop_all);
        }
    });

    return true;
}

/**
 * @brief Add route for getting servo status
 */
bool ServoService::addRouteGetStatus(const std::vector<OpenAPIResponse>& standard_responses)
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
        if (!checkServiceStarted(request)) return;
        
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
        request->send(200, RoutesConsts::mime_json, status.c_str());
    });

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
        if (!checkServiceStarted(request)) return;
        
        std::string status = getAllAttachedServos();
        request->send(200, RoutesConsts::mime_json, status.c_str());
    });

    return true;
}

/**
 * @brief Add route for setting all servos to same angle
 */
bool ServoService::addRouteSetAllAngle(const std::vector<OpenAPIResponse>& standard_responses)
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

    webserver.on(path.c_str(), HTTP_POST, 
        [this](AsyncWebServerRequest *request)
        {
            if (!checkServiceStarted(request)) return;
            
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
            }
        },
        nullptr,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonBodyParser::storeBody(request, data, len, index, total);
        }
    );

    return true;
}

/**
 * @brief Add route for setting all servos to same speed
 */
bool ServoService::addRouteSetAllSpeed(const std::vector<OpenAPIResponse>& standard_responses)
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

    webserver.on(path.c_str(), HTTP_POST, 
        [this](AsyncWebServerRequest *request)
        {
            if (!checkServiceStarted(request)) return;
            
            // Parse and validate JSON body
            JsonDocument doc;
            if (!JsonBodyParser::parseBody(request, doc, [](const JsonDocument& d) {
                return d[FPSTR(ServoConsts::servo_speed)].is<int8_t>();
            })) return;

            int8_t speed = doc[FPSTR(ServoConsts::servo_speed)].as<int8_t>();
            
            if (speed < -100 || speed > 100)
            {
                ResponseHelper::sendError(request, ResponseHelper::INVALID_PARAMS, RoutesConsts::msg_invalid_values);
                return;
            }
            
            if (setAllServoSpeed(speed))
            {
                ResponseHelper::sendSuccess(request, FPSTR(ServoConsts::action_set_all_speed));
            }
            else
            {
                ResponseHelper::sendError(request, ResponseHelper::OPERATION_FAILED, FPSTR(ServoConsts::action_set_all_speed));
            }
        },
        nullptr,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonBodyParser::storeBody(request, data, len, index, total);
        }
    );

    return true;
}

/**
 * @brief Add route for setting multiple servos speed at once
 */
bool ServoService::addRouteSetServosSpeedMultiple(const std::vector<OpenAPIResponse>& standard_responses)
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

    webserver.on(path.c_str(), HTTP_POST, 
        [this](AsyncWebServerRequest *request)
        {
            if (!checkServiceStarted(request)) return;
            
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
            }
        },
        nullptr,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonBodyParser::storeBody(request, data, len, index, total);
        }
    );

    return true;
}

/**
 * @brief Add route for setting multiple servos angle at once
 */
bool ServoService::addRouteSetServosAngleMultiple(const std::vector<OpenAPIResponse>& standard_responses)
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

    webserver.on(path.c_str(), HTTP_POST, 
        [this](AsyncWebServerRequest *request)
        {
            if (!checkServiceStarted(request)) return;
            
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
            }
        },
        nullptr,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonBodyParser::storeBody(request, data, len, index, total);
        }
    );

    return true;
}

/**
 * @brief Add route for attaching servo to a channel
 */
bool ServoService::addRouteAttachServo(const std::vector<OpenAPIResponse>& standard_responses)
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

    webserver.on(path.c_str(), HTTP_POST, 
        [this](AsyncWebServerRequest *request)
        {
            if (!checkServiceStarted(request)) return;
            
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
            }
        },
        nullptr,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonBodyParser::storeBody(request, data, len, index, total);
        }
    );

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
registerServiceStatusRoute( this);
  registerSettingsRoutes( this);

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
    if (!settings_service_) {
        if (logger) {
            logger->error("Servo Service: Settings service not available");
        }
        return false;
    }
    
    std::string comma = progmem_to_string(ServoConsts::str_comma);
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

// ─── UDP helpers ──────────────────────────────────────────────────────────────

/**
 * @brief Serialize a success response into @p out.
 * @param action  Command name echoed back
 * @param out     Output string populated in-place
 */
static void udp_buildSuccess(const char *action, std::string &out)
{
    s_udp_resp.clear();
    s_udp_resp[ServoConsts::udp_field_result]  = ServoConsts::udp_val_ok;
    s_udp_resp[ServoConsts::udp_field_message] = action;
    String buf;
    serializeJson(s_udp_resp, buf);
    out = buf.c_str();
}

/**
 * @brief Serialize an error response into @p out.
 * @param reason  Human-readable error string
 * @param out     Output string populated in-place
 */
static void udp_buildError(const char *reason, std::string &out)
{
    s_udp_resp.clear();
    s_udp_resp[ServoConsts::udp_field_result]  = ServoConsts::udp_val_error;
    s_udp_resp[ServoConsts::udp_field_message] = reason;
    String buf;
    serializeJson(s_udp_resp, buf);
    out = buf.c_str();
}

// ─── Per-command UDP handlers ─────────────────────────────────────────────────

bool ServoService::handleUDP_setServoAngle(const JsonDocument &doc, std::string &response)
{
    if (!doc[ServoConsts::servo_channel].is<uint8_t>() ||
        !doc[ServoConsts::servo_angle].is<uint16_t>())
    {
        udp_buildError(reinterpret_cast<const char *>(FPSTR(RoutesConsts::msg_invalid_params)), response);
        return false;
    }
    uint8_t  ch    = doc[ServoConsts::servo_channel].as<uint8_t>();
    uint16_t angle = doc[ServoConsts::servo_angle].as<uint16_t>();
    if (ch > 7 || angle > 360)
    {
        udp_buildError(reinterpret_cast<const char *>(FPSTR(RoutesConsts::msg_invalid_values)), response);
        return false;
    }
    if (setServoAngle(ch, angle))
    {
        udp_buildSuccess(ServoConsts::action_set_angle, response);
        return true;
    }
    udp_buildError(ServoConsts::msg_failed_action, response);
    return false;
}

bool ServoService::handleUDP_setServoSpeed(const JsonDocument &doc, std::string &response)
{
    if (!doc[ServoConsts::servo_channel].is<int>() ||
        !doc[ServoConsts::servo_speed].is<int>())
    {
        udp_buildError(reinterpret_cast<const char *>(FPSTR(RoutesConsts::msg_invalid_params)), response);
        return false;
    }
    uint8_t ch    = doc[ServoConsts::servo_channel].as<uint8_t>();
    int8_t  speed = doc[ServoConsts::servo_speed].as<int8_t>();
    if (ch > 7 || speed < -100 || speed > 100)
    {
        udp_buildError(reinterpret_cast<const char *>(FPSTR(RoutesConsts::msg_invalid_values)), response);
        return false;
    }
    if (setServoSpeed(ch, speed))
    {
        udp_buildSuccess(ServoConsts::action_set_speed, response);
        return true;
    }
    udp_buildError(ServoConsts::msg_failed_action, response);
    return false;
}

bool ServoService::handleUDP_stopAll(const JsonDocument &, std::string &response)
{
    if (setAllServoSpeed(0))
    {
        udp_buildSuccess(ServoConsts::action_stop_all, response);
        return true;
    }
    udp_buildError(ServoConsts::msg_failed_action, response);
    return false;
}

bool ServoService::handleUDP_setAllAngle(const JsonDocument &doc, std::string &response)
{
    if (!doc[ServoConsts::servo_angle].is<uint16_t>())
    {
        udp_buildError(reinterpret_cast<const char *>(FPSTR(RoutesConsts::msg_invalid_params)), response);
        return false;
    }
    uint16_t angle = doc[ServoConsts::servo_angle].as<uint16_t>();
    if (angle > 360)
    {
        udp_buildError(reinterpret_cast<const char *>(FPSTR(RoutesConsts::msg_invalid_values)), response);
        return false;
    }
    if (setAllServoAngle(angle))
    {
        udp_buildSuccess(ServoConsts::action_set_all_angle, response);
        return true;
    }
    udp_buildError(ServoConsts::msg_failed_action, response);
    return false;
}

bool ServoService::handleUDP_setAllSpeed(const JsonDocument &doc, std::string &response)
{
    if (!doc[ServoConsts::servo_speed].is<int>())
    {
        udp_buildError(reinterpret_cast<const char *>(FPSTR(RoutesConsts::msg_invalid_params)), response);
        return false;
    }
    int8_t speed = doc[ServoConsts::servo_speed].as<int8_t>();
    if (speed < -100 || speed > 100)
    {
        udp_buildError(reinterpret_cast<const char *>(FPSTR(RoutesConsts::msg_invalid_values)), response);
        return false;
    }
    if (setAllServoSpeed(speed))
    {
        udp_buildSuccess(ServoConsts::action_set_all_speed, response);
        return true;
    }
    udp_buildError(ServoConsts::msg_failed_action, response);
    return false;
}

bool ServoService::handleUDP_setServosAngleMultiple(const JsonDocument &doc, std::string &response)
{
    if (!doc[ServoConsts::servos].is<JsonArrayConst>())
    {
        udp_buildError(reinterpret_cast<const char *>(FPSTR(RoutesConsts::msg_invalid_params)), response);
        return false;
    }
    std::vector<ServoAngleOp> ops;
    for (JsonObjectConst servo_obj : doc[ServoConsts::servos].as<JsonArrayConst>())
    {
        if (!servo_obj[ServoConsts::servo_channel].is<uint8_t>() ||
            !servo_obj[ServoConsts::servo_angle].is<uint16_t>())
            continue;
        ServoAngleOp op;
        op.channel = servo_obj[ServoConsts::servo_channel].as<uint8_t>();
        op.angle   = servo_obj[ServoConsts::servo_angle].as<uint16_t>();
        ops.push_back(op);
    }
    if (ops.empty())
    {
        udp_buildError(reinterpret_cast<const char *>(FPSTR(RoutesConsts::msg_invalid_params)), response);
        return false;
    }
    if (setServosAngleMultiple(ops))
    {
        udp_buildSuccess(ServoConsts::action_set_servos_angle_multiple, response);
        return true;
    }
    udp_buildError(ServoConsts::msg_failed_action, response);
    return false;
}

bool ServoService::handleUDP_setServosSpeedMultiple(const JsonDocument &doc, std::string &response)
{
    if (!doc[ServoConsts::servos].is<JsonArrayConst>())
    {
        udp_buildError(reinterpret_cast<const char *>(FPSTR(RoutesConsts::msg_invalid_params)), response);
        return false;
    }
    std::vector<ServoSpeedOp> ops;
    for (JsonObjectConst servo_obj : doc[ServoConsts::servos].as<JsonArrayConst>())
    {
        if (!servo_obj[ServoConsts::servo_channel].is<uint8_t>() ||
            !servo_obj[ServoConsts::servo_speed].is<int>())
            continue;
        ServoSpeedOp op;
        op.channel = servo_obj[ServoConsts::servo_channel].as<uint8_t>();
        op.speed   = servo_obj[ServoConsts::servo_speed].as<int8_t>();
        ops.push_back(op);
    }
    if (ops.empty())
    {
        udp_buildError(reinterpret_cast<const char *>(FPSTR(RoutesConsts::msg_invalid_params)), response);
        return false;
    }
    if (setServosSpeedMultiple(ops))
    {
        udp_buildSuccess(ServoConsts::action_set_servos_speed_multiple, response);
        return true;
    }
    udp_buildError(ServoConsts::msg_failed_action, response);
    return false;
}

bool ServoService::handleUDP_attachServo(const JsonDocument &doc, std::string &response)
{
    if (!doc[ServoConsts::servo_channel].is<uint8_t>() ||
        !doc[ServoConsts::connection].is<uint8_t>())
    {
        udp_buildError(reinterpret_cast<const char *>(FPSTR(RoutesConsts::msg_invalid_params)), response);
        return false;
    }
    uint8_t channel    = doc[ServoConsts::servo_channel].as<uint8_t>();
    uint8_t connection = doc[ServoConsts::connection].as<uint8_t>();
    if (channel > 7 || connection > 3)
    {
        udp_buildError(reinterpret_cast<const char *>(FPSTR(RoutesConsts::msg_invalid_values)), response);
        return false;
    }
    if (attachServo(channel, static_cast<ServoConnection>(connection)))
    {
        udp_buildSuccess(ServoConsts::action_attach_servo, response);
        return true;
    }
    udp_buildError(ServoConsts::msg_failed_action, response);
    return false;
}

bool ServoService::handleUDP_getStatus(const JsonDocument &doc, std::string &response)
{
    if (!doc[ServoConsts::servo_channel].is<uint8_t>())
    {
        udp_buildError(reinterpret_cast<const char *>(FPSTR(RoutesConsts::msg_invalid_params)), response);
        return false;
    }
    uint8_t channel = doc[ServoConsts::servo_channel].as<uint8_t>();
    if (channel > 7)
    {
        udp_buildError(reinterpret_cast<const char *>(FPSTR(RoutesConsts::msg_invalid_values)), response);
        return false;
    }
    response = getAttachedServo(channel);
    return true;
}

bool ServoService::handleUDP_getAllStatus(const JsonDocument &, std::string &response)
{
    response = getAllAttachedServos();
    return true;
}

// ─── Main UDP dispatcher ──────────────────────────────────────────────────────

/**
 * @brief Handle an incoming UDP message for ServoService.
 *        Format: "Servo Service:<command>[:<JSON>]"
 *        Example: "Servo Service:setServoAngle:{\"channel\":0,\"angle\":90}"
 * @param message    Raw UDP payload
 * @param remoteIP   Sender IP (used to send reply)
 * @param remotePort Sender port (used to send reply)
 * @return true if the message was claimed by this service
 */
bool ServoService::messageHandler(const std::string &message,
                                  const IPAddress &remoteIP,
                                  uint16_t remotePort)
{
    // ── 1. Ownership check — O(prefix) early-exit ──────────────────────────
    const std::string service_name = getServiceName();     // "Servo Service"
    const size_t      name_len     = service_name.size();

    if (message.size() <= name_len + 1 ||
        message.compare(0, name_len, service_name) != 0 ||
        message[name_len] != ':')
        return false;

    // ── 2. Extract command token ───────────────────────────────────────────
    const size_t cmd_start = name_len + 1;
    const size_t sep_pos   = message.find(':', cmd_start);
    const std::string cmd  = (sep_pos == std::string::npos)
                                 ? message.substr(cmd_start)
                                 : message.substr(cmd_start, sep_pos - cmd_start);

    if (cmd.empty())
    {
        logger->warning(progmem_to_string(ServoConsts::msg_udp_bad_format));
        return true;   // claimed but malformed
    }

    // ── 3. Parse optional JSON payload (zero heap: reuse static doc) ───────
    s_udp_req.clear();
    if (sep_pos != std::string::npos && sep_pos + 1 < message.size())
    {
        DeserializationError err = deserializeJson(s_udp_req, message.c_str() + sep_pos + 1);
        if (err)
        {
            logger->warning(progmem_to_string(ServoConsts::msg_udp_json_error));
            return true;   // claimed but JSON invalid
        }
    }

    if (!isServiceStarted())
        return true;   // claimed, silently drop

    // ── 4. Dispatch table ─────────────────────────────────────────────────
    static std::string response;   // static buffer — no realloc after first call
    response.clear();

    if      (cmd == ServoConsts::action_set_angle)
        handleUDP_setServoAngle(s_udp_req, response);
    else if (cmd == ServoConsts::action_set_speed)
        handleUDP_setServoSpeed(s_udp_req, response);
    else if (cmd == ServoConsts::action_stop_all)
        handleUDP_stopAll(s_udp_req, response);
    else if (cmd == ServoConsts::action_set_all_angle)
        handleUDP_setAllAngle(s_udp_req, response);
    else if (cmd == ServoConsts::action_set_all_speed)
        handleUDP_setAllSpeed(s_udp_req, response);
    else if (cmd == ServoConsts::action_set_servos_angle_multiple)
        handleUDP_setServosAngleMultiple(s_udp_req, response);
    else if (cmd == ServoConsts::action_set_servos_speed_multiple)
        handleUDP_setServosSpeedMultiple(s_udp_req, response);
    else if (cmd == ServoConsts::action_attach_servo)
        handleUDP_attachServo(s_udp_req, response);
    else if (cmd == ServoConsts::action_get_status)
        handleUDP_getStatus(s_udp_req, response);
    else if (cmd == ServoConsts::action_get_all_status)
        handleUDP_getAllStatus(s_udp_req, response);
    else
    {
        logger->warning(progmem_to_string(ServoConsts::msg_udp_unknown_cmd) + ": " + cmd);
        udp_buildError(ServoConsts::msg_udp_unknown_cmd, response);
    }

#ifdef VERBOSE_DEBUG
    logger->debug("UDP " + cmd + " -> " + response);
#endif

    // ── 5. Send reply back to sender ──────────────────────────────────────
    if (!response.empty())
        udp_service.sendReply(response, remoteIP, remotePort);

    return true;
}