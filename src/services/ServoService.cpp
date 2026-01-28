// Servo controller handler for DFR0548 board
/**
 * @file servo_handler.cpp
 * @brief Implementation for servo controller integration with the main application
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
    constexpr const char PathServo[] PROGMEM = "servo/";

    constexpr const char ActionSetAngle[] PROGMEM = "setServoAngle";
    constexpr const char ActionSetSpeed[] PROGMEM = "setServoSpeed";
    constexpr const char ActionStopAll[] PROGMEM = "stopAll";
    constexpr const char ActionGetStatus[] PROGMEM = "getStatus";
    constexpr const char ActionGetAllStatus[] PROGMEM = "getAllStatus";
    constexpr const char ActionAttachServo[] PROGMEM = "attachServo";
    constexpr const char ActionSetAllAngle[] PROGMEM = "setAllServoAngle";
    constexpr const char ActionSetAllSpeed[] PROGMEM = "setAllServoSpeed";

    constexpr const char ServoChannel[] PROGMEM = "channel";
    constexpr const char ServoAngle[] PROGMEM = "angle";
    constexpr const char ServoSpeed[] PROGMEM = "speed";
    constexpr const char Connection[] PROGMEM = "connection";

    constexpr const char MsgNotInitialized[] PROGMEM = "Servo controller not initialized.";
    constexpr const char MsgFailedAction[] PROGMEM = "Servo controller action failed.";

    // Error messages for exceptions
    constexpr const char ErrChannelRange[] PROGMEM = "Channel out of range";
    constexpr const char ErrAngleRange180[] PROGMEM = "Angle out of range for 180° servo";
    constexpr const char ErrAngleRange270[] PROGMEM = "Angle out of range for 270° servo";
    constexpr const char ErrServoNotAttached[] PROGMEM = "Servo not attached on channel";
    constexpr const char ErrServoNotContinuous[] PROGMEM = "Servo not continuous on channel";
    constexpr const char ErrSpeedRange[] PROGMEM = "Speed out of range (-100 to 100)";

    // JSON keys and status strings
    constexpr const char JsonAttachedServos[] PROGMEM = "attached_servos";
    constexpr const char StatusNotConnected[] PROGMEM = "Not Connected";
    constexpr const char StatusRotational[] PROGMEM = "Rotational";
    constexpr const char StatusAngular180[] PROGMEM = "Angular 180";
    constexpr const char StatusAngular270[] PROGMEM = "Angular 270";
    constexpr const char StatusUnknown[] PROGMEM = "Unknown";
    constexpr const char SettingsKeyServos[] PROGMEM = "attached_servos";
    constexpr const char TagServo[] PROGMEM = "Servo";

    // OpenAPI tags and descriptions
    constexpr const char TagServos[] PROGMEM = "Servos";
    constexpr const char DescServoChannel[] PROGMEM = "Servo channel (0-7)";
    constexpr const char DescAngleDegrees[] PROGMEM = "Angle in degrees (0-180 for 180° servos, 0-270 for 270° servos)";
    constexpr const char DescAngleDegrees360[] PROGMEM = "Angle in degrees (0-360)";
    constexpr const char DescSpeedPercent[] PROGMEM = "Speed percentage (-100 to +100, negative is reverse)";
    constexpr const char DescSpeedPercent100[] PROGMEM = "Speed percentage (-100 to +100)";
    constexpr const char DescConnectionType[] PROGMEM = "Servo connection type (0=None, 1=continuous, 2=angular 180 degree, 3=angular 270 degrees)";
    constexpr const char DescServoAngleControl[] PROGMEM = "Servo angle control";
    constexpr const char DescServoSpeedControl[] PROGMEM = "Servo speed control";
    constexpr const char DescAngleForAll[] PROGMEM = "Angle for all servos";
    constexpr const char DescSpeedForAll[] PROGMEM = "Speed for all servos";
    constexpr const char DescAttachmentConfig[] PROGMEM = "Servo attachment configuration";
    constexpr const char DescStatusRetrieved[] PROGMEM = "Servo status retrieved";
    constexpr const char DescAllStatusRetrieved[] PROGMEM = "All servos status retrieved";
    constexpr const char DescSetAngle[] PROGMEM = "Set servo angle for angular servos (180° or 270°)";
    constexpr const char DescSetSpeed[] PROGMEM = "Set continuous servo speed for rotational servos";
    constexpr const char DescStopAll[] PROGMEM = "Stop all servos by setting speed to 0";
    constexpr const char DescGetStatus[] PROGMEM = "Get servo type and connection status for a specific channel";
    constexpr const char DescGetAllStatus[] PROGMEM = "Get connection status and type for all 8 servo channels";
    constexpr const char DescSetAllAngle[] PROGMEM = "Set all attached angular servos to the same angle simultaneously";
    constexpr const char DescSetAllSpeed[] PROGMEM = "Set all attached continuous rotation servos to the same speed simultaneously";
    constexpr const char DescAttachServo[] PROGMEM = "Register a servo type to a channel before use";

    // OpenAPI parameter names
    constexpr const char ParamChannel[] PROGMEM = "channel";
    constexpr const char ParamAngle[] PROGMEM = "angle";
    constexpr const char ParamSpeed[] PROGMEM = "speed";

    // OpenAPI schemas
    constexpr const char SchemaChannelStatus[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"channel\":{\"type\":\"integer\"},\"status\":{\"type\":\"string\"}}}";
    constexpr const char SchemaAllServos[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"servos\":{\"type\":\"array\",\"items\":{\"type\":\"object\"}}}}";
    constexpr const char ReqChannelAngle07[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"channel\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":7},\"angle\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":360}},\"required\":[\"channel\",\"angle\"]}";
    constexpr const char ReqChannelSpeed[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"channel\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":7},\"speed\":{\"type\":\"integer\",\"minimum\":-100,\"maximum\":100}},\"required\":[\"channel\",\"speed\"]}";
    constexpr const char ReqAngle[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"angle\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":360}},\"required\":[\"angle\"]}";
    constexpr const char ReqSpeed[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"speed\":{\"type\":\"integer\",\"minimum\":-100,\"maximum\":100}},\"required\":[\"speed\"]}";
    constexpr const char ReqChannelConnection[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"channel\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":7},\"connection\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":3,\"description\":\"0=None, 1=continuous, 2=angular 180 degree, 3=angular 270 degrees\"}},\"required\":[\"channel\",\"connection\"]}";

    // Example values
    constexpr const char ExChannelAngle[] PROGMEM = "{\"channel\":0,\"angle\":90}";
    constexpr const char ExChannelSpeed[] PROGMEM = "{\"channel\":0,\"speed\":50}";
    constexpr const char ExChannelStatus[] PROGMEM = "{\"channel\":0,\"status\":\"ANGULAR_180\"}";
    constexpr const char ExAllServos[] PROGMEM = "{\"servos\":[{\"channel\":0,\"status\":\"ANGULAR_180\"},{\"channel\":1,\"status\":\"NOT_CONNECTED\"}]}";
    constexpr const char ExAngle[] PROGMEM = "{\"angle\":90}";
    constexpr const char ExSpeed[] PROGMEM = "{\"speed\":50}";
    constexpr const char ExChannelConnection[] PROGMEM = "{\"channel\":0,\"connection\":0}";
    constexpr const char ExResultOk[] PROGMEM = "{\"result\":\"ok\",\"message\":\"setServoAngle\"}";
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
    }
    else
    {
        logger->warning("Servo controller issue detected.");
    }
    // Return true to allow other services to continue even if servo fails
    return true;
}

bool ServoService::startService()
{
    // nothing to start with DFRobot_UnihikerExpansion
    initialized = true;
    if (initialized)
    {
#ifdef DEBUG
        logger->debug(getName() + " start done");
#endif
    }
    else
    {
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
            throw std::out_of_range(reinterpret_cast<const char *>(FPSTR(ServoConsts::ErrChannelRange)));
        }
        if (attached_servos[channel] == ServoConnection::ANGULAR_180)
        {
            if (angle > 180)
            {
                throw std::out_of_range(reinterpret_cast<const char *>(FPSTR(ServoConsts::ErrAngleRange180)));
            }
            servoController.setServoAngle(eServoNumber_t(channel), angle);
            return true;
        }
        else if (attached_servos[channel] == ServoConnection::ANGULAR_270)
        {
            if (angle > 270)
            {
                throw std::out_of_range(reinterpret_cast<const char *>(FPSTR(ServoConsts::ErrAngleRange270)));
            }
            servoController.setServoAngle(eServoNumber_t(channel), angle);
            return true;
        }
        else
        {
            throw std::runtime_error(reinterpret_cast<const char *>(FPSTR(ServoConsts::ErrServoNotAttached)));
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
            throw std::out_of_range(reinterpret_cast<const char *>(FPSTR(ServoConsts::ErrChannelRange)));
        }
        if (attached_servos[channel] != ServoConnection::ROTATIONAL)
        {
            throw std::runtime_error(reinterpret_cast<const char *>(FPSTR(ServoConsts::ErrServoNotContinuous)));
        }
        if (speed < -100 || speed > 100)
        {
            throw std::out_of_range(reinterpret_cast<const char *>(FPSTR(ServoConsts::ErrSpeedRange)));
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
    return false;
}
std::string ServoService::getAllAttachedServos()
{
    if (!initialized)
    {
        return {};
    }
    JsonDocument doc;
    JsonArray servosArray = doc[FPSTR(ServoConsts::JsonAttachedServos)].to<JsonArray>();

    for (uint8_t channel = 0; channel < MAX_SERVO_CHANNELS; channel++)
    {
        const char *status = attached_servos.at(channel) == NOT_CONNECTED ? reinterpret_cast<const char *>(FPSTR(ServoConsts::StatusNotConnected))
                             : attached_servos.at(channel) == ROTATIONAL  ? reinterpret_cast<const char *>(FPSTR(ServoConsts::StatusRotational))
                             : attached_servos.at(channel) == ANGULAR_180 ? reinterpret_cast<const char *>(FPSTR(ServoConsts::StatusAngular180))
                             : attached_servos.at(channel) == ANGULAR_270 ? reinterpret_cast<const char *>(FPSTR(ServoConsts::StatusAngular270))
                                                                          : reinterpret_cast<const char *>(FPSTR(ServoConsts::StatusUnknown));
        JsonObject servoObj = servosArray.add<JsonObject>();
        servoObj[ServoConsts::ServoChannel] = channel;
        servoObj[ServoConsts::Connection] = status;
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
    std::string status = attached_servos.at(channel) == NOT_CONNECTED ? reinterpret_cast<const char *>(FPSTR(ServoConsts::StatusNotConnected))
                         : attached_servos.at(channel) == ROTATIONAL  ? reinterpret_cast<const char *>(FPSTR(ServoConsts::StatusRotational))
                         : attached_servos.at(channel) == ANGULAR_180 ? reinterpret_cast<const char *>(FPSTR(ServoConsts::StatusAngular180))
                         : attached_servos.at(channel) == ANGULAR_270 ? reinterpret_cast<const char *>(FPSTR(ServoConsts::StatusAngular270))
                                                                      : reinterpret_cast<const char *>(FPSTR(ServoConsts::StatusUnknown));
    JsonDocument doc = JsonDocument();
    doc[ServoConsts::ServoChannel] = channel;
    doc[ServoConsts::Connection] = status;
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
    OpenAPIResponse okResp = createSuccessResponse(RoutesConsts::RespOperationSuccess);
    okResp.example = ServoConsts::ExResultOk;
    standardResponses.push_back(okResp);
    standardResponses.push_back(createMissingParamsResponse());
    standardResponses.push_back(createOperationFailedResponse());
    standardResponses.push_back(createNotInitializedResponse());

    // Set servo angle endpoint
    std::string path = std::string(RoutesConsts::PathAPI) + getServiceSubPath() + "/" + ServoConsts::ActionSetAngle;
#ifdef DEBUG
    logger->debug("+" + path);
#endif

    std::vector<OpenAPIParameter> angleParams;
    angleParams.push_back(OpenAPIParameter(ServoConsts::ParamChannel, RoutesConsts::TypeInteger, RoutesConsts::InQuery, reinterpret_cast<const char *>(FPSTR(ServoConsts::DescServoChannel)), true));
    angleParams.push_back(OpenAPIParameter(ServoConsts::ParamAngle, RoutesConsts::TypeInteger, RoutesConsts::InQuery, reinterpret_cast<const char *>(FPSTR(ServoConsts::DescAngleDegrees)), true));

    OpenAPIRoute angleRoute(path.c_str(), RoutesConsts::MethodPOST, reinterpret_cast<const char *>(FPSTR(ServoConsts::DescSetAngle)), reinterpret_cast<const char *>(FPSTR(ServoConsts::TagServos)), true, angleParams, standardResponses);
    angleRoute.requestBody = OpenAPIRequestBody(reinterpret_cast<const char *>(FPSTR(ServoConsts::DescServoAngleControl)),
                                                ServoConsts::ReqChannelAngle07, true);
    angleRoute.requestBody.example = ServoConsts::ExChannelAngle;
    registerOpenAPIRoute(angleRoute);

    webserver.on(path.c_str(), HTTP_POST, [this]()
                 {    
                   if (!webserver.hasArg(ServoConsts::ServoChannel) || !webserver.hasArg(ServoConsts::ServoAngle))
                   {
                       webserver.send(422, RoutesConsts::MimeJSON, getResultJsonString(RoutesConsts::ResultErr,  RoutesConsts::MsgInvalidParams).c_str());
                          return;   
                   }

                   uint8_t ch = (uint8_t)webserver.arg(ServoConsts::ServoChannel).toInt();
                   uint16_t angle = (uint16_t)webserver.arg(ServoConsts::ServoAngle).toInt();

                   if ( angle > 360 || ch > 7)
                    { 
                    webserver.send(422, RoutesConsts::MimeJSON, getResultJsonString(RoutesConsts::ResultErr, RoutesConsts::MsgInvalidValues ).c_str());
                       return;
                   }

                   if (setServoAngle(ch, angle))
                   {

                       webserver.send (200, RoutesConsts::MimeJSON, getResultJsonString(RoutesConsts::ResultOk, ServoConsts::ActionSetAngle).c_str());
                       return;
                   }
                   else
                   {
                    webserver.send(456, RoutesConsts::MimeJSON, getResultJsonString(RoutesConsts::ResultErr, ServoConsts::ActionSetAngle).c_str());
                       return;
                   } });

    // Set servo speed endpoint
    path = std::string(RoutesConsts::PathAPI) + getServiceSubPath() + "/" + ServoConsts::ActionSetSpeed;
#ifdef DEBUG
    logger->debug("+" + path);
#endif

    std::vector<OpenAPIParameter> speedParams;
    speedParams.push_back(OpenAPIParameter(ServoConsts::ParamChannel, RoutesConsts::TypeInteger, RoutesConsts::InQuery, reinterpret_cast<const char *>(FPSTR(ServoConsts::DescServoChannel)), true));
    speedParams.push_back(OpenAPIParameter(ServoConsts::ParamSpeed, RoutesConsts::TypeInteger, RoutesConsts::InQuery, reinterpret_cast<const char *>(FPSTR(ServoConsts::DescSpeedPercent)), true));

    OpenAPIRoute speedRoute(path.c_str(), RoutesConsts::MethodPOST, reinterpret_cast<const char *>(FPSTR(ServoConsts::DescSetSpeed)), reinterpret_cast<const char *>(FPSTR(ServoConsts::TagServos)), true, speedParams, standardResponses);
    speedRoute.requestBody = OpenAPIRequestBody(reinterpret_cast<const char *>(FPSTR(ServoConsts::DescServoSpeedControl)),
                                                ServoConsts::ReqChannelSpeed, true);
    speedRoute.requestBody.example = ServoConsts::ExChannelSpeed;
    registerOpenAPIRoute(speedRoute);
    webserver.on(path.c_str(), HTTP_POST, [this]()
                 {
        if (!webserver.hasArg(ServoConsts::ServoChannel) || !webserver.hasArg(ServoConsts::ServoSpeed)) {

            webserver.send(422, RoutesConsts::MimeJSON, getResultJsonString(RoutesConsts::ResultErr, RoutesConsts::MsgInvalidParams).c_str());
            return;
        }
        
        uint8_t channel = (uint8_t)webserver.arg(ServoConsts::ServoChannel).toInt();
        int8_t speed = (int8_t)webserver.arg(ServoConsts::ServoSpeed).toInt();
        
        
        if (channel > 7 || speed < -100 || speed > 100) {
            webserver.send(422, RoutesConsts::MimeJSON, getResultJsonString(RoutesConsts::ResultErr, RoutesConsts::MsgInvalidValues).c_str());
            return;
        }
        
        if (this->setServoSpeed(channel, speed)) {
            
            webserver.send (200, RoutesConsts::MimeJSON, getResultJsonString(RoutesConsts::ResultOk, ServoConsts::ActionSetSpeed).c_str ());
        } else {
            webserver.send(456, RoutesConsts::MimeJSON, getResultJsonString(RoutesConsts::ResultErr, ServoConsts::ActionSetSpeed).c_str());
        } });

    // API: Stop all servos at /api/servo/stop_all
    path = std::string(RoutesConsts::PathAPI) + getServiceSubPath() + "/" + ServoConsts::ActionStopAll;
#ifdef DEBUG
    logger->debug("+" + path);
#endif

    OpenAPIRoute stopAllRoute(path.c_str(), RoutesConsts::MethodPOST, reinterpret_cast<const char *>(FPSTR(ServoConsts::DescStopAll)), reinterpret_cast<const char *>(FPSTR(ServoConsts::TagServos)), false, {}, standardResponses);
    registerOpenAPIRoute(stopAllRoute);
    webserver.on(path.c_str(), HTTP_POST, [this]()
                 {
        if (this->setAllServoSpeed(0)) {
            webserver.send(200, RoutesConsts::MimeJSON, getResultJsonString(RoutesConsts::ResultOk, ServoConsts::ActionStopAll).c_str());
        } else {
            webserver.send(456, RoutesConsts::MimeJSON, getResultJsonString(RoutesConsts::ResultErr, ServoConsts::ActionStopAll).c_str());
        } });

    // API: Get attached servo status for a specific channel
    path = std::string(RoutesConsts::PathAPI) + getServiceSubPath() + "/" + ServoConsts::ActionGetStatus;
#ifdef DEBUG
    logger->debug("+" + path);
#endif

    std::vector<OpenAPIParameter> statusParams;
    statusParams.push_back(OpenAPIParameter(ServoConsts::ParamChannel, RoutesConsts::TypeInteger, RoutesConsts::InQuery, reinterpret_cast<const char *>(FPSTR(ServoConsts::DescServoChannel)), true));

    std::vector<OpenAPIResponse> statusResponses;
    OpenAPIResponse statusOk(200, reinterpret_cast<const char *>(FPSTR(ServoConsts::DescStatusRetrieved)));
    statusOk.schema = ServoConsts::SchemaChannelStatus;
    statusOk.example = ServoConsts::ExChannelStatus;
    statusResponses.push_back(statusOk);
    statusResponses.push_back(createMissingParamsResponse());

    OpenAPIRoute statusRoute(path.c_str(), RoutesConsts::MethodGET, reinterpret_cast<const char *>(FPSTR(ServoConsts::DescGetStatus)), reinterpret_cast<const char *>(FPSTR(ServoConsts::TagServos)), true, statusParams, statusResponses);
    registerOpenAPIRoute(statusRoute);
    webserver.on(path.c_str(), HTTP_GET, [this]()
                 {
if (!webserver.hasArg(ServoConsts::ServoChannel)) {
            webserver.send(422, RoutesConsts::MimeJSON, getResultJsonString(RoutesConsts::ResultErr, RoutesConsts::MsgInvalidParams).c_str());
            return;
        }

        uint8_t channel = (uint8_t)webserver.arg(ServoConsts::ServoChannel).toInt();
        
        if (channel > 7) {
            webserver.send(422, RoutesConsts::MimeJSON, getResultJsonString(RoutesConsts::ResultErr, RoutesConsts::MsgInvalidValues).c_str());
            return;
        }
        
        std::string status = getAttachedServo(channel);
        webserver.send(200, RoutesConsts::MimeJSON, status.c_str()); });

    // API: Get all attached servos status
    path = std::string(RoutesConsts::PathAPI) + getServiceSubPath() + "/" + ServoConsts::ActionGetAllStatus;
#ifdef DEBUG
    logger->debug("+" + path);
#endif

    std::vector<OpenAPIResponse> allStatusResponses;
    OpenAPIResponse allStatusOk(200, reinterpret_cast<const char *>(FPSTR(ServoConsts::DescAllStatusRetrieved)));
    allStatusOk.schema = ServoConsts::SchemaAllServos;
    allStatusOk.example = ServoConsts::ExAllServos;
    allStatusResponses.push_back(allStatusOk);

    OpenAPIRoute allStatusRoute(path.c_str(), RoutesConsts::MethodGET, reinterpret_cast<const char *>(FPSTR(ServoConsts::DescGetAllStatus)), reinterpret_cast<const char *>(FPSTR(ServoConsts::TagServos)), false, {}, allStatusResponses);
    registerOpenAPIRoute(allStatusRoute);
    webserver.on(path.c_str(), HTTP_GET, [this]()
                 {
        std::string status = getAllAttachedServos();
        webserver.send(200, RoutesConsts::MimeJSON, status.c_str()); });

    // API: Set all angular servos to the same angle
    path = std::string(RoutesConsts::PathAPI) + getServiceSubPath() + "/" + ServoConsts::ActionSetAllAngle;
#ifdef DEBUG
    logger->debug("+" + path);
#endif

    std::vector<OpenAPIParameter> allAngleParams;
    allAngleParams.push_back(OpenAPIParameter(ServoConsts::ParamAngle, RoutesConsts::TypeInteger, RoutesConsts::InQuery, reinterpret_cast<const char *>(FPSTR(ServoConsts::DescAngleDegrees360)), true));

    OpenAPIRoute allAngleRoute(path.c_str(), RoutesConsts::MethodPOST, reinterpret_cast<const char *>(FPSTR(ServoConsts::DescSetAllAngle)), reinterpret_cast<const char *>(FPSTR(ServoConsts::TagServos)), true, allAngleParams, standardResponses);
    allAngleRoute.requestBody = OpenAPIRequestBody(reinterpret_cast<const char *>(FPSTR(ServoConsts::DescAngleForAll)),
                                                   ServoConsts::ReqAngle, true);
    allAngleRoute.requestBody.example = ServoConsts::ExAngle;
    registerOpenAPIRoute(allAngleRoute);
    webserver.on(path.c_str(), HTTP_POST, [this]()
                 {
if (!webserver.hasArg(ServoConsts::ServoAngle)) {
            webserver.send(422, RoutesConsts::MimeJSON, getResultJsonString(RoutesConsts::ResultErr, RoutesConsts::MsgInvalidParams).c_str());
            return;
        }

        uint16_t angle = (uint16_t)webserver.arg(ServoConsts::ServoAngle).toInt();
        
        if (angle > 360) {
            webserver.send(422, RoutesConsts::MimeJSON, getResultJsonString(RoutesConsts::ResultErr, RoutesConsts::MsgInvalidValues).c_str());
            return;
        }
        
        if (setAllServoAngle(angle)) {
            webserver.send(200, RoutesConsts::MimeJSON, getResultJsonString(RoutesConsts::ResultOk, ServoConsts::ActionSetAllAngle).c_str());
        } else {
            webserver.send(422, RoutesConsts::MimeJSON, getResultJsonString(RoutesConsts::ResultErr, ServoConsts::ActionSetAllAngle).c_str());
        } });

    // API: Set all continuous servos to the same speed
    path = std::string(RoutesConsts::PathAPI) + getServiceSubPath() + "/" + ServoConsts::ActionSetAllSpeed;
#ifdef DEBUG
    logger->debug("+" + path);
#endif

    std::vector<OpenAPIParameter> allSpeedParams;
    allSpeedParams.push_back(OpenAPIParameter(ServoConsts::ParamSpeed, RoutesConsts::TypeInteger, RoutesConsts::InQuery, reinterpret_cast<const char *>(FPSTR(ServoConsts::DescSpeedPercent100)), true));

    OpenAPIRoute allSpeedRoute(path.c_str(), RoutesConsts::MethodPOST, reinterpret_cast<const char *>(FPSTR(ServoConsts::DescSetAllSpeed)), reinterpret_cast<const char *>(FPSTR(ServoConsts::TagServos)), true, allSpeedParams, standardResponses);
    allSpeedRoute.requestBody = OpenAPIRequestBody(reinterpret_cast<const char *>(FPSTR(ServoConsts::DescSpeedForAll)),
                                                   ServoConsts::ReqSpeed, true);
    allSpeedRoute.requestBody.example = ServoConsts::ExSpeed;
    registerOpenAPIRoute(allSpeedRoute);
    webserver.on(path.c_str(), HTTP_POST, [this]()
                 {
if (!webserver.hasArg(ServoConsts::ServoSpeed)) {
            webserver.send(422, RoutesConsts::MimeJSON, getResultJsonString(RoutesConsts::ResultErr, RoutesConsts::MsgInvalidParams).c_str());
            return;
        }

        int8_t speed = (int8_t)webserver.arg(ServoConsts::ServoSpeed).toInt();
        
        if (speed < -100 || speed > 100) {
            webserver.send(422, RoutesConsts::MimeJSON, getResultJsonString(RoutesConsts::ResultErr, RoutesConsts::MsgInvalidValues).c_str());
            return;
        }
        
        if (setAllServoSpeed(speed)) {
            webserver.send(200, RoutesConsts::MimeJSON, getResultJsonString(RoutesConsts::ResultOk, ServoConsts::ActionSetAllSpeed).c_str());
        } else {
            webserver.send(456, RoutesConsts::MimeJSON, getResultJsonString(RoutesConsts::ResultErr, ServoConsts::ActionSetAllSpeed).c_str());
        } });

    // API: Attach servo to a channel
    path = getPath(ServoConsts::ActionAttachServo);
#ifdef DEBUG
    logger->debug("+" + path);
#endif

    std::vector<OpenAPIParameter> attachParams;
    attachParams.push_back(OpenAPIParameter(ServoConsts::ParamChannel, RoutesConsts::TypeInteger, RoutesConsts::InQuery, reinterpret_cast<const char *>(FPSTR(ServoConsts::DescServoChannel)), true));
    attachParams.push_back(OpenAPIParameter(ServoConsts::ParamSpeed, RoutesConsts::TypeInteger, RoutesConsts::InQuery, reinterpret_cast<const char *>(FPSTR(ServoConsts::DescConnectionType)), true));

    OpenAPIRoute attachRoute(path.c_str(), RoutesConsts::MethodPOST, reinterpret_cast<const char *>(FPSTR(ServoConsts::DescAttachServo)), reinterpret_cast<const char *>(FPSTR(ServoConsts::TagServos)), true, attachParams, standardResponses);
    attachRoute.requestBody = OpenAPIRequestBody(reinterpret_cast<const char *>(FPSTR(ServoConsts::DescAttachmentConfig)),
                                                 ServoConsts::ReqChannelConnection, true);
    attachRoute.requestBody.example = ServoConsts::ExChannelConnection;
    registerOpenAPIRoute(attachRoute);
    webserver.on(path.c_str(), HTTP_POST, [this]()
                 {
        if (!webserver.hasArg(ServoConsts::ServoChannel) || !webserver.hasArg(ServoConsts::Connection)) {
            webserver.send(422, RoutesConsts::MimeJSON, getResultJsonString(RoutesConsts::ResultErr, RoutesConsts::MsgInvalidParams).c_str());
            return;
        }
        
        uint8_t channel = (uint8_t)webserver.arg(ServoConsts::ServoChannel).toInt();
        uint8_t connection = (uint8_t)webserver.arg(ServoConsts::Connection).toInt();
        
        if (channel > 7 || connection > 3) {
            webserver.send(422, RoutesConsts::MimeJSON, getResultJsonString(RoutesConsts::ResultErr, RoutesConsts::MsgInvalidValues).c_str());
            return;
        }
        
        ServoConnection servoConnection = static_cast<ServoConnection>(connection);
        if (attachServo(channel, servoConnection)) {
            webserver.send(200, RoutesConsts::MimeJSON, getResultJsonString(RoutesConsts::ResultOk, ServoConsts::ActionAttachServo).c_str());
        } else {
            webserver.send(456, RoutesConsts::MimeJSON, getResultJsonString(RoutesConsts::ResultErr, ServoConsts::ActionAttachServo).c_str());
        } });

    registerSettingsRoutes(reinterpret_cast<const char *>(FPSTR(ServoConsts::TagServo)), this);

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
        baseServicePath = std::string(RoutesConsts::PathAPI) + getServiceName() + "/";
    }
    return baseServicePath + finalpathstring;
}

bool ServoService::saveSettings()
{
    return settingsService.setSetting(getServiceName(), reinterpret_cast<const char *>(FPSTR(ServoConsts::SettingsKeyServos)), std::to_string(static_cast<int>(attached_servos[0])) + "," + std::to_string(static_cast<int>(attached_servos[1])) + "," + std::to_string(static_cast<int>(attached_servos[2])) + "," + std::to_string(static_cast<int>(attached_servos[3])) + "," + std::to_string(static_cast<int>(attached_servos[4])) + "," + std::to_string(static_cast<int>(attached_servos[5])) + "," + std::to_string(static_cast<int>(attached_servos[6])) + "," + std::to_string(static_cast<int>(attached_servos[7])));
    return true;
}

bool ServoService::loadSettings()
{

    std::string attached_servos_settings = settingsService.getSetting(getServiceName(), reinterpret_cast<const char *>(FPSTR(ServoConsts::SettingsKeyServos)));
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
