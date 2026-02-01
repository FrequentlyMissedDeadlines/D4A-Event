// Servo controller handler for DFR0548 board
/**
 * @file servo_handler.cpp
 * @brief Implementation for servo controller integration with the main application
 * @details Exposed routes:
 *          - POST /api/servo/v1/setServoAngle - Set servo angle for angular servos (180° or 270°)
 *          - POST /api/servo/v1/setServoSpeed - Set continuous servo speed for rotational servos
 *          - POST /api/servo/v1/stopAll - Stop all servos by setting speed to 0
 *          - GET /api/servo/v1/getStatus - Get servo type and connection status for a specific channel
 *          - GET /api/servo/v1/getAllStatus - Get connection status and type for all 8 servo channels
 *          - POST /api/servo/v1/attachServo - Register a servo type to a channel before use
 *          - POST /api/servo/v1/setAllServoAngle - Set all attached angular servos to the same angle
 *          - POST /api/servo/v1/setAllServoSpeed - Set all attached continuous rotation servos to the same speed
 * 
 */

#include "ServoService.h"
#include <WebServer.h>
#include <pgmspace.h>
#include <ArduinoJson.h>
#include "../DFR1216/DFRobot_UnihikerExpansion.h"
#include "SettingsService.h"

constexpr uint8_t MAX_SERVO_CHANNELS = 8;

DFRobot_UnihikerExpansion_I2C servoController = DFRobot_UnihikerExpansion_I2C();
bool initialized = false;

extern SettingsService settingsService;

// Servo Service constants (stored in PROGMEM to save RAM)
namespace ServoConsts
{
    constexpr const char path_servo[] PROGMEM = "servo/";

    constexpr const char action_set_angle[] PROGMEM = "setServoAngle";
    constexpr const char action_set_speed[] PROGMEM = "setServoSpeed";
    constexpr const char action_stop_all[] PROGMEM = "stopAll";
    constexpr const char action_get_status[] PROGMEM = "getStatus";
    constexpr const char action_get_all_status[] PROGMEM = "getAllStatus";
    constexpr const char action_attach_servo[] PROGMEM = "attachServo";
    constexpr const char action_set_all_angle[] PROGMEM = "setAllServoAngle";
    constexpr const char action_set_all_speed[] PROGMEM = "setAllServoSpeed";

    constexpr const char servo_channel[] PROGMEM = "channel";
    constexpr const char servo_angle[] PROGMEM = "angle";
    constexpr const char servo_speed[] PROGMEM = "speed";
    constexpr const char connection[] PROGMEM = "connection";

    constexpr const char msg_not_initialized[] PROGMEM = "Servo controller not initialized.";
    constexpr const char msg_failed_action[] PROGMEM = "Servo controller action failed.";

    // Error messages for exceptions
    constexpr const char err_channel_range[] PROGMEM = "Channel out of range";
    constexpr const char err_angle_range_180[] PROGMEM = "Angle out of range for 180° servo";
    constexpr const char err_angle_range_270[] PROGMEM = "Angle out of range for 270° servo";
    constexpr const char err_servo_not_attached[] PROGMEM = "Servo not attached on channel";
    constexpr const char err_servo_not_continuous[] PROGMEM = "Servo not continuous on channel";
    constexpr const char err_speed_range[] PROGMEM = "Speed out of range (-100 to 100)";

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
    constexpr const char desc_speed_percent_100[] PROGMEM = "Speed percentage (-100 to +100)";
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

    // Example values
    constexpr const char ex_channel_angle[] PROGMEM = "{\"channel\":0,\"angle\":90}";
    constexpr const char ex_channel_speed[] PROGMEM = "{\"channel\":0,\"speed\":50}";
    constexpr const char ex_channel_status[] PROGMEM = "{\"channel\":0,\"status\":\"ANGULAR_180\"}";
    constexpr const char ex_all_servos[] PROGMEM = "{\"servos\":[{\"channel\":0,\"status\":\"ANGULAR_180\"},{\"channel\":1,\"status\":\"NOT_CONNECTED\"}]}";
    constexpr const char ex_angle[] PROGMEM = "{\"angle\":90}";
    constexpr const char ex_speed[] PROGMEM = "{\"speed\":50}";
    constexpr const char ex_channel_connection[] PROGMEM = "{\"channel\":0,\"connection\":0}";
    constexpr const char ex_result_ok[] PROGMEM = "{\"result\":\"ok\",\"message\":\"setServoAngle\"}";
}

std::array<ServoConnection, MAX_SERVO_CHANNELS> attached_servos = {NOT_CONNECTED, NOT_CONNECTED, NOT_CONNECTED, NOT_CONNECTED,
                                                                   NOT_CONNECTED, NOT_CONNECTED, NOT_CONNECTED, NOT_CONNECTED};

bool ServoService::initializeService()
{
    logger->info("Initializing Servo Service...");
    initialized = servoController.begin();
    if (initialized)
    {
        logger->info("Servo controller initialized successfully.");
        service_status_ = STARTED;
    }
    else
    {
        logger->warning("Servo controller issue detected.");
        service_status_ = INIT_FAILED;
    }
    status_timestamp_ = millis();
    // Return true to allow other services to continue even if servo fails
    return true;
}

bool ServoService::startService()
{
    // nothing to start with DFRobot_UnihikerExpansion
    initialized = true;
    if (initialized)
    {
        service_status_ = STARTED;
        status_timestamp_ = millis();
#ifdef VERBOSE_DEBUG
        logger->debug(getName() + ServiceInterfaceConsts::msg_start_done);
#endif
    }
    else
    {
        service_status_ = START_FAILED;
        status_timestamp_ = millis();
        logger->error(getServiceName() + " start failed");
    }
    return initialized;
}

bool ServoService::attachServo(uint8_t channel, ServoConnection connection)
{
    if (!initialized)
    {
        return false;
    }
    try
    {
        if (channel >= MAX_SERVO_CHANNELS)
        {
            throw std::out_of_range("Channel out of range");
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
    if (!initialized)
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
    if (!initialized)
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

bool ServoService::stopService()
{
    // Stop all servos when service stops
    service_status_ = STOPPED;
    status_timestamp_ = millis();
    return false;
}
std::string ServoService::getAllAttachedServos()
{
    if (!initialized)
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
    if (!initialized)
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
 * @brief Register HTTP routes for servo control.
 * @return true if registration was successful, false otherwise.
 */
bool ServoService::registerRoutes()
{
    // Define common response schemas using optimized helper functions
    std::vector<OpenAPIResponse> standardResponses;
    OpenAPIResponse okResp = createSuccessResponse(RoutesConsts::resp_operation_success);
    okResp.example = ServoConsts::ex_result_ok;
    standardResponses.push_back(okResp);
    standardResponses.push_back(createMissingParamsResponse());
    standardResponses.push_back(createOperationFailedResponse());
    standardResponses.push_back(createNotInitializedResponse());

    // Set servo angle endpoint
    std::string path = std::string(RoutesConsts::path_api) + getServiceSubPath() + "/" + ServoConsts::action_set_angle;
#ifdef VERBOSE_DEBUG
    logger->debug("+" + path);
#endif

    std::vector<OpenAPIParameter> angleParams;
    angleParams.push_back(OpenAPIParameter(ServoConsts::param_channel, RoutesConsts::type_integer, RoutesConsts::in_query, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_servo_channel)), true));
    angleParams.push_back(OpenAPIParameter(ServoConsts::param_angle, RoutesConsts::type_integer, RoutesConsts::in_query, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_angle_degrees)), true));

    OpenAPIRoute angleRoute(path.c_str(), RoutesConsts::method_post, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_set_angle)), reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)), true, angleParams, standardResponses);
    angleRoute.requestBody = OpenAPIRequestBody(reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_servo_angle_control)),
                                                ServoConsts::req_channel_angle_07, true);
    angleRoute.requestBody.example = ServoConsts::ex_channel_angle;
    registerOpenAPIRoute(angleRoute);

    webserver.on(path.c_str(), HTTP_POST, [this]()
                 {    
                   if (!webserver.hasArg(ServoConsts::servo_channel) || !webserver.hasArg(ServoConsts::servo_angle))
                   {
                       webserver.send(422, RoutesConsts::mime_json, getResultJsonString(RoutesConsts::result_err,  RoutesConsts::msg_invalid_params).c_str());
                          return;   
                   }

                   uint8_t ch = (uint8_t)webserver.arg(ServoConsts::servo_channel).toInt();
                   uint16_t angle = (uint16_t)webserver.arg(ServoConsts::servo_angle).toInt();

                   if ( angle > 360 || ch > 7)
                    { 
                    webserver.send(422, RoutesConsts::mime_json, getResultJsonString(RoutesConsts::result_err, RoutesConsts::msg_invalid_values ).c_str());
                       return;
                   }

                   if (setServoAngle(ch, angle))
                   {

                       webserver.send (200, RoutesConsts::mime_json, getResultJsonString(RoutesConsts::result_ok, ServoConsts::action_set_angle).c_str());
                       return;
                   }
                   else
                   {
                    webserver.send(456, RoutesConsts::mime_json, getResultJsonString(RoutesConsts::result_err, ServoConsts::action_set_angle).c_str());
                       return;
                   } });

    // Set servo speed endpoint
    path = std::string(RoutesConsts::path_api) + getServiceSubPath() + "/" + ServoConsts::action_set_speed;
#ifdef VERBOSE_DEBUG
    logger->debug("+" + path);
#endif

    std::vector<OpenAPIParameter> speedParams;
    speedParams.push_back(OpenAPIParameter(ServoConsts::param_channel, RoutesConsts::type_integer, RoutesConsts::in_query, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_servo_channel)), true));
    speedParams.push_back(OpenAPIParameter(ServoConsts::param_speed, RoutesConsts::type_integer, RoutesConsts::in_query, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_speed_percent)), true));

    OpenAPIRoute speedRoute(path.c_str(), RoutesConsts::method_post, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_set_speed)), reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)), true, speedParams, standardResponses);
    speedRoute.requestBody = OpenAPIRequestBody(reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_servo_speed_control)),
                                                ServoConsts::req_channel_speed, true);
    speedRoute.requestBody.example = ServoConsts::ex_channel_speed;
    registerOpenAPIRoute(speedRoute);
    webserver.on(path.c_str(), HTTP_POST, [this]()
                 {
        if (!webserver.hasArg(ServoConsts::servo_channel) || !webserver.hasArg(ServoConsts::servo_speed)) {

            webserver.send(422, RoutesConsts::mime_json, getResultJsonString(RoutesConsts::result_err, RoutesConsts::msg_invalid_params).c_str());
            return;
        }
        
        uint8_t channel = (uint8_t)webserver.arg(ServoConsts::servo_channel).toInt();
        int8_t speed = (int8_t)webserver.arg(ServoConsts::servo_speed).toInt();
        
        
        if (channel > 7 || speed < -100 || speed > 100) {
            webserver.send(422, RoutesConsts::mime_json, getResultJsonString(RoutesConsts::result_err, RoutesConsts::msg_invalid_values).c_str());
            return;
        }
        
        if (this->setServoSpeed(channel, speed)) {
            
            webserver.send (200, RoutesConsts::mime_json, getResultJsonString(RoutesConsts::result_ok, ServoConsts::action_set_speed).c_str ());
        } else {
            webserver.send(456, RoutesConsts::mime_json, getResultJsonString(RoutesConsts::result_err, ServoConsts::action_set_speed).c_str());
        } });

    // API: Stop all servos at /api/servo/stop_all
    path = std::string(RoutesConsts::path_api) + getServiceSubPath() + "/" + ServoConsts::action_stop_all;
#ifdef VERBOSE_DEBUG
    logger->debug("+" + path);
#endif

    OpenAPIRoute stopAllRoute(path.c_str(), RoutesConsts::method_post, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_stop_all)), reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)), false, {}, standardResponses);
    registerOpenAPIRoute(stopAllRoute);
    webserver.on(path.c_str(), HTTP_POST, [this]()
                 {
        if (this->setAllServoSpeed(0)) {
            webserver.send(200, RoutesConsts::mime_json, getResultJsonString(RoutesConsts::result_ok, ServoConsts::action_stop_all).c_str());
        } else {
            webserver.send(456, RoutesConsts::mime_json, getResultJsonString(RoutesConsts::result_err, ServoConsts::action_stop_all).c_str());
        } });

    // API: Get attached servo status for a specific channel
    path = std::string(RoutesConsts::path_api) + getServiceSubPath() + "/" + ServoConsts::action_get_status;
#ifdef VERBOSE_DEBUG
    logger->debug("+" + path);
#endif

    std::vector<OpenAPIParameter> statusParams;
    statusParams.push_back(OpenAPIParameter(ServoConsts::param_channel, RoutesConsts::type_integer, RoutesConsts::in_query, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_servo_channel)), true));

    std::vector<OpenAPIResponse> statusResponses;
    OpenAPIResponse statusOk(200, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_status_retrieved)));
    statusOk.schema = ServoConsts::schema_channel_status;
    statusOk.example = ServoConsts::ex_channel_status;
    statusResponses.push_back(statusOk);
    statusResponses.push_back(createMissingParamsResponse());

    OpenAPIRoute statusRoute(path.c_str(), RoutesConsts::method_get, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_get_status)), reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)), true, statusParams, statusResponses);
    registerOpenAPIRoute(statusRoute);
    webserver.on(path.c_str(), HTTP_GET, [this]()
                 {
if (!webserver.hasArg(ServoConsts::servo_channel)) {
            webserver.send(422, RoutesConsts::mime_json, getResultJsonString(RoutesConsts::result_err, RoutesConsts::msg_invalid_params).c_str());
            return;
        }

        uint8_t channel = (uint8_t)webserver.arg(ServoConsts::servo_channel).toInt();
        
        if (channel > 7) {
            webserver.send(422, RoutesConsts::mime_json, getResultJsonString(RoutesConsts::result_err, RoutesConsts::msg_invalid_values).c_str());
            return;
        }
        
        std::string status = getAttachedServo(channel);
        webserver.send(200, RoutesConsts::mime_json, status.c_str()); });

    // API: Get all attached servos status
    path = std::string(RoutesConsts::path_api) + getServiceSubPath() + "/" + ServoConsts::action_get_all_status;
#ifdef VERBOSE_DEBUG
    logger->debug("+" + path);
#endif

    std::vector<OpenAPIResponse> allStatusResponses;
    OpenAPIResponse allStatusOk(200, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_all_status_retrieved)));
    allStatusOk.schema = ServoConsts::schema_all_servos;
    allStatusOk.example = ServoConsts::ex_all_servos;
    allStatusResponses.push_back(allStatusOk);

    OpenAPIRoute allStatusRoute(path.c_str(), RoutesConsts::method_get, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_get_all_status)), reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)), false, {}, allStatusResponses);
    registerOpenAPIRoute(allStatusRoute);
    webserver.on(path.c_str(), HTTP_GET, [this]()
                 {
        std::string status = getAllAttachedServos();
        webserver.send(200, RoutesConsts::mime_json, status.c_str()); });

    // API: Set all angular servos to the same angle
    path = std::string(RoutesConsts::path_api) + getServiceSubPath() + "/" + ServoConsts::action_set_all_angle;
#ifdef VERBOSE_DEBUG
    logger->debug("+" + path);
#endif

    std::vector<OpenAPIParameter> allAngleParams;
    allAngleParams.push_back(OpenAPIParameter(ServoConsts::param_angle, RoutesConsts::type_integer, RoutesConsts::in_query, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_angle_degrees_360)), true));

    OpenAPIRoute allAngleRoute(path.c_str(), RoutesConsts::method_post, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_set_all_angle)), reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)), true, allAngleParams, standardResponses);
    allAngleRoute.requestBody = OpenAPIRequestBody(reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_angle_for_all)),
                                                   ServoConsts::req_angle, true);
    allAngleRoute.requestBody.example = ServoConsts::ex_angle;
    registerOpenAPIRoute(allAngleRoute);
    webserver.on(path.c_str(), HTTP_POST, [this]()
                 {
if (!webserver.hasArg(ServoConsts::servo_angle)) {
            webserver.send(422, RoutesConsts::mime_json, getResultJsonString(RoutesConsts::result_err, RoutesConsts::msg_invalid_params).c_str());
            return;
        }

        uint16_t angle = (uint16_t)webserver.arg(ServoConsts::servo_angle).toInt();
        
        if (angle > 360) {
            webserver.send(422, RoutesConsts::mime_json, getResultJsonString(RoutesConsts::result_err, RoutesConsts::msg_invalid_values).c_str());
            return;
        }
        
        if (setAllServoAngle(angle)) {
            webserver.send(200, RoutesConsts::mime_json, getResultJsonString(RoutesConsts::result_ok, ServoConsts::action_set_all_angle).c_str());
        } else {
            webserver.send(422, RoutesConsts::mime_json, getResultJsonString(RoutesConsts::result_err, ServoConsts::action_set_all_angle).c_str());
        } });

    // API: Set all continuous servos to the same speed
    path = std::string(RoutesConsts::path_api) + getServiceSubPath() + "/" + ServoConsts::action_set_all_speed;
#ifdef VERBOSE_DEBUG
    logger->debug("+" + path);
#endif

    std::vector<OpenAPIParameter> allSpeedParams;
    allSpeedParams.push_back(OpenAPIParameter(ServoConsts::param_speed, RoutesConsts::type_integer, RoutesConsts::in_query, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_speed_percent_100)), true));

    OpenAPIRoute allSpeedRoute(path.c_str(), RoutesConsts::method_post, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_set_all_speed)), reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)), true, allSpeedParams, standardResponses);
    allSpeedRoute.requestBody = OpenAPIRequestBody(reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_speed_for_all)),
                                                   ServoConsts::req_speed, true);
    allSpeedRoute.requestBody.example = ServoConsts::ex_speed;
    registerOpenAPIRoute(allSpeedRoute);
    webserver.on(path.c_str(), HTTP_POST, [this]()
                 {
if (!webserver.hasArg(ServoConsts::servo_speed)) {
            webserver.send(422, RoutesConsts::mime_json, getResultJsonString(RoutesConsts::result_err, RoutesConsts::msg_invalid_params).c_str());
            return;
        }

        int8_t speed = (int8_t)webserver.arg(ServoConsts::servo_speed).toInt();
        
        if (speed < -100 || speed > 100) {
            webserver.send(422, RoutesConsts::mime_json, getResultJsonString(RoutesConsts::result_err, RoutesConsts::msg_invalid_values).c_str());
            return;
        }
        
        if (setAllServoSpeed(speed)) {
            webserver.send(200, RoutesConsts::mime_json, getResultJsonString(RoutesConsts::result_ok, ServoConsts::action_set_all_speed).c_str());
        } else {
            webserver.send(456, RoutesConsts::mime_json, getResultJsonString(RoutesConsts::result_err, ServoConsts::action_set_all_speed).c_str());
        } });

    // API: Attach servo to a channel
    path = getPath(ServoConsts::action_attach_servo);
#ifdef VERBOSE_DEBUG
    logger->debug("+" + path);
#endif

    std::vector<OpenAPIParameter> attachParams;
    attachParams.push_back(OpenAPIParameter(ServoConsts::param_channel, RoutesConsts::type_integer, RoutesConsts::in_query, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_servo_channel)), true));
    attachParams.push_back(OpenAPIParameter(ServoConsts::param_speed, RoutesConsts::type_integer, RoutesConsts::in_query, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_connection_type)), true));

    OpenAPIRoute attachRoute(path.c_str(), RoutesConsts::method_post, reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_attach_servo)), reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servos)), true, attachParams, standardResponses);
    attachRoute.requestBody = OpenAPIRequestBody(reinterpret_cast<const char *>(FPSTR(ServoConsts::desc_attachment_config)),
                                                 ServoConsts::req_channel_connection, true);
    attachRoute.requestBody.example = ServoConsts::ex_channel_connection;
    registerOpenAPIRoute(attachRoute);
    webserver.on(path.c_str(), HTTP_POST, [this]()
                 {
        if (!webserver.hasArg(ServoConsts::servo_channel) || !webserver.hasArg(ServoConsts::connection)) {
            webserver.send(422, RoutesConsts::mime_json, getResultJsonString(RoutesConsts::result_err, RoutesConsts::msg_invalid_params).c_str());
            return;
        }
        
        uint8_t channel = (uint8_t)webserver.arg(ServoConsts::servo_channel).toInt();
        uint8_t connection = (uint8_t)webserver.arg(ServoConsts::connection).toInt();
        
        if (channel > 7 || connection > 3) {
            webserver.send(422, RoutesConsts::mime_json, getResultJsonString(RoutesConsts::result_err, RoutesConsts::msg_invalid_values).c_str());
            return;
        }
        
        ServoConnection servoConnection = static_cast<ServoConnection>(connection);
        if (attachServo(channel, servoConnection)) {
            webserver.send(200, RoutesConsts::mime_json, getResultJsonString(RoutesConsts::result_ok, ServoConsts::action_attach_servo).c_str());
        } else {
            webserver.send(456, RoutesConsts::mime_json, getResultJsonString(RoutesConsts::result_err, ServoConsts::action_attach_servo).c_str());
        } });

    registerSettingsRoutes(reinterpret_cast<const char *>(FPSTR(ServoConsts::tag_servo)), this);

    return true;
}

std::string ServoService::getServiceName()
{
    return "Servo Service";
}

std::string ServoService::getServiceSubPath()
{
    return "servos/v1";
}

std::string ServoService::getPath(const std::string &finalpathstring)
{
    if (baseServicePath.empty())
    {
        baseServicePath = std::string(RoutesConsts::path_api) + getServiceSubPath() + "/";
    }
    return baseServicePath + finalpathstring;
}

bool ServoService::saveSettings()
{
    return settingsService.setSetting(getServiceName(), reinterpret_cast<const char *>(FPSTR(ServoConsts::settings_key_servos)), std::to_string(static_cast<int>(attached_servos[0])) + "," + std::to_string(static_cast<int>(attached_servos[1])) + "," + std::to_string(static_cast<int>(attached_servos[2])) + "," + std::to_string(static_cast<int>(attached_servos[3])) + "," + std::to_string(static_cast<int>(attached_servos[4])) + "," + std::to_string(static_cast<int>(attached_servos[5])) + "," + std::to_string(static_cast<int>(attached_servos[6])) + "," + std::to_string(static_cast<int>(attached_servos[7])));
    return true;
}

bool ServoService::loadSettings()
{

    std::string attached_servos_settings = settingsService.getSetting(getServiceName(), reinterpret_cast<const char *>(FPSTR(ServoConsts::settings_key_servos)));
    if (attached_servos_settings.empty())
    {
        logger->info("No saved servo settings found.");
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

    logger->info("Loaded servo settings successfully.");
    return true;
}
