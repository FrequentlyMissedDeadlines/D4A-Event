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

constexpr uint8_t MAX_SERVO_CHANNELS = 8;

DFRobot_UnihikerExpansion_I2C servoController = DFRobot_UnihikerExpansion_I2C();
bool initialized = false;

constexpr char kPathServo[] = "servo/";

constexpr char kActionSetAngle[] = "setServoAngle";
constexpr char kActionSetSpeed[] = "setServoSpeed";
constexpr char kActionStopAll[] = "stopAll";
constexpr char kActionGetStatus[] = "getStatus";
constexpr char kActionGetAllStatus[] = "getAllStatus";
constexpr char kActionAttachServo[] = "attachServo";
constexpr char kActionSetAllAngle[] = "setAllServoAngle";
constexpr char kActionSetAllSpeed[] = "setAllServoSpeed";

constexpr char kServoChannel[] = "channel";
constexpr char kServoAngle[] = "angle";
constexpr char kServoSpeed[] = "speed";
constexpr char kServoModel[] = "model";

constexpr char kMsgNotInitialized[] = "Servo controller not initialized.";
constexpr char kMsgFailedAction[] = "Servo controller action failed.";

std::array<ConnectionStatus, MAX_SERVO_CHANNELS> attached_servos = {NOT_CONNECTED, NOT_CONNECTED, NOT_CONNECTED, NOT_CONNECTED,
                                                                    NOT_CONNECTED, NOT_CONNECTED, NOT_CONNECTED, NOT_CONNECTED};

bool ServoService::initializeService()
{
    logger->info("Initializing Servo Service...");
    initialized = servoController.begin();
    if (initialized) {
        logger->info("Servo controller initialized successfully.");
    } else {
        logger->error("Servo controller initialization failed");
    }
    // Return true to allow other services to continue even if servo fails
    return true;
}

bool ServoService::startService()
{
    // nothing to start with DFRobot_UnihikerExpansion
    initialized = true;
    return initialized;
}

bool ServoService::attachServo(uint8_t channel, ServoModel model)
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
        switch (model)
        {
        case ANGULAR_180:
            attached_servos[channel] = ConnectionStatus::ANGULAR180;
            break;
        case ANGULAR_270:
            attached_servos[channel] = ConnectionStatus::ANGULAR270;
            break;
        case CONTINUOUS:
            attached_servos[channel] = ConnectionStatus::ROTATIONAL;
            break;
        default:
            attached_servos[channel] = ConnectionStatus::NOT_CONNECTED;
        }
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
            throw std::out_of_range("Channel out of range");
        }
        if (attached_servos[channel] == ConnectionStatus::ANGULAR180)
        {
            if (angle > 180)
            {
                throw std::out_of_range("Angle out of range for 180° servo");
            }
            servoController.setServoAngle(eServoNumber_t(channel), angle);
            return true;
        }
        else if (attached_servos[channel] == ConnectionStatus::ANGULAR270)
        {
            if (angle > 270)
            {
                throw std::out_of_range("Angle out of range for 270° servo");
            }
            servoController.setServoAngle(eServoNumber_t(channel), angle);
            return true;
        }
        else
        {
            throw std::runtime_error("Servo not attached on channel");
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
            throw std::out_of_range("Channel out of range");
        }
        if (attached_servos[channel] != ConnectionStatus::ROTATIONAL)
        {
            throw std::runtime_error("Servo not continuous on channel");
        }
        if (speed < -100 || speed > 100)
        {
            throw std::out_of_range("Speed out of range (-100 to 100)");
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
        if (attached_servos[channel] == ConnectionStatus::ROTATIONAL)
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
        if (attached_servos[channel] == ConnectionStatus::ANGULAR180 || attached_servos[channel] == ConnectionStatus::ANGULAR270)
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
    JsonDocument doc = JsonDocument();
    doc["attached_servos"] = JsonArray();
    for (uint8_t channel = 0; channel < MAX_SERVO_CHANNELS; channel++)
    {
        const char* status = attached_servos.at(channel) == NOT_CONNECTED ? "Not Connected"
                             : attached_servos.at(channel) == ROTATIONAL  ? "Rotational"
                             : attached_servos.at(channel) == ANGULAR180  ? "Angular 180"
                             : attached_servos.at(channel) == ANGULAR270  ? "Angular 270"
                                                                          : "Unknown";
        JsonObject servoObj = JsonObject();
        // Use channel number directly as char key to avoid std::to_string
        char channelKey[2];
        channelKey[0] = '0' + channel;
        channelKey[1] = '\0';
        servoObj[channelKey] = status;
        doc["attached_servos"].add(servoObj);
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
    std::string status = attached_servos.at(channel) == NOT_CONNECTED ? "Not Connected"
                         : attached_servos.at(channel) == ROTATIONAL  ? "Rotational"
                         : attached_servos.at(channel) == ANGULAR180  ? "Angular 180"
                         : attached_servos.at(channel) == ANGULAR270  ? "Angular 270"
                                                                      : "Unknown";
    JsonDocument doc = JsonDocument();
    doc["channel"] = channel;
    doc["status"] = status;
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
    // Define common response schemas
    std::vector<OpenAPIResponse> standardResponses;
    OpenAPIResponse okResp(200, "Operation successful");
    okResp.schema = "{\"type\":\"object\",\"properties\":{\"result\":{\"type\":\"string\"},\"message\":{\"type\":\"string\"}}}";
    okResp.example = "{\"result\":\"ok\",\"message\":\"setServoAngle\"}";
    standardResponses.push_back(okResp);
    standardResponses.push_back(OpenAPIResponse(422, "Missing or invalid parameters"));
    standardResponses.push_back(OpenAPIResponse(456, "Operation failed"));
    standardResponses.push_back(OpenAPIResponse(503, "Servo controller not initialized"));

    // Set servo angle endpoint
    std::string path = std::string(RoutesConsts::kPathAPI) + getName() + "/"+kActionSetAngle;
#ifdef DEBUG
    logger->debug("+" + path);
#endif
    
    std::vector<OpenAPIParameter> angleParams;
    angleParams.push_back(OpenAPIParameter("channel", "integer", "body", "Servo channel (0-7)", true));
    angleParams.push_back(OpenAPIParameter("angle", "integer", "body", "Angle in degrees (0-180 for 180° servos, 0-270 for 270° servos)", true));
    
    OpenAPIRoute angleRoute(path.c_str(), "POST", "Set servo angle for angular servos (180° or 270°)", "Servos", true, angleParams, standardResponses);
    angleRoute.requestBody = OpenAPIRequestBody("Servo angle control", 
        "{\"type\":\"object\",\"properties\":{\"channel\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":7},\"angle\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":360}},\"required\":[\"channel\",\"angle\"]}", true);
    angleRoute.requestBody.example = "{\"channel\":0,\"angle\":90}";
    registerOpenAPIRoute(angleRoute);
    
    webserver.on(path.c_str(), HTTP_PUT, [this]()
                 {    
                   if (!webserver.hasArg(kServoChannel) || !webserver.hasArg(kServoAngle))
                   {
                       webserver.send(422, RoutesConsts::kMimeJSON, getResultJsonString(RoutesConsts::kResultErr,  RoutesConsts::kMsgInvalidParams).c_str());
                          return;   
                   }

                   uint8_t ch = (uint8_t)webserver.arg(kServoChannel).toInt();
                   uint16_t angle = (uint16_t)webserver.arg(kServoAngle).toInt();

                   if ( angle > 360 || ch > 7)
                    { 
                    webserver.send(422, RoutesConsts::kMimeJSON, getResultJsonString(RoutesConsts::kResultErr, RoutesConsts::kMsgInvalidValues ).c_str());
                       return;
                   }

                   if (setServoAngle(ch, angle))
                   {

                       webserver.send (200, RoutesConsts::kMimeJSON, getResultJsonString(RoutesConsts::kResultOk, kActionSetAngle).c_str());
                       return;
                   }
                   else
                   {
                    webserver.send(456, RoutesConsts::kMimeJSON, getResultJsonString(RoutesConsts::kResultErr, kActionSetAngle).c_str());
                       return;
                   } });

    // Set servo speed endpoint
    path = std::string(RoutesConsts::kPathAPI) + getName() + "/"+ kActionSetSpeed;
#ifdef DEBUG
    logger->debug("+" + path);
#endif
    
    std::vector<OpenAPIParameter> speedParams;
    speedParams.push_back(OpenAPIParameter("channel", "integer", "body", "Servo channel (0-7)", true));
    speedParams.push_back(OpenAPIParameter("speed", "integer", "body", "Speed percentage (-100 to +100, negative is reverse)", true));
    
    OpenAPIRoute speedRoute(path.c_str(), "POST", "Set continuous servo speed for rotational servos", "Servos", true, speedParams, standardResponses);
    speedRoute.requestBody = OpenAPIRequestBody("Servo speed control",
        "{\"type\":\"object\",\"properties\":{\"channel\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":7},\"speed\":{\"type\":\"integer\",\"minimum\":-100,\"maximum\":100}},\"required\":[\"channel\",\"speed\"]}", true);
    speedRoute.requestBody.example = "{\"channel\":0,\"speed\":50}";
    registerOpenAPIRoute(speedRoute);
    webserver.on(path.c_str(), HTTP_PUT, [this]()
                 {
        if (!webserver.hasArg(kServoChannel) || !webserver.hasArg(kServoSpeed)) {

            webserver.send(422, RoutesConsts::kMimeJSON, getResultJsonString(RoutesConsts::kResultErr, RoutesConsts::kMsgInvalidParams).c_str());
            return;
        }
        
        uint8_t channel = (uint8_t)webserver.arg(kServoChannel).toInt();
        int8_t speed = (int8_t)webserver.arg(kServoSpeed).toInt();
        
        
        if (channel > 7 || speed < -100 || speed > 100) {
            webserver.send(422, RoutesConsts::kMimeJSON, getResultJsonString(RoutesConsts::kResultErr, RoutesConsts::kMsgInvalidValues).c_str());
            return;
        }
        
        if (this->setServoSpeed(channel, speed)) {
            
            webserver.send (200, RoutesConsts::kMimeJSON, getResultJsonString(RoutesConsts::kResultOk, kActionSetSpeed).c_str ());
        } else {
            webserver.send(456, RoutesConsts::kMimeJSON, getResultJsonString(RoutesConsts::kResultErr, kActionSetSpeed).c_str());
        } });

    // API: Stop all servos at /api/servo/stop_all
    path = std::string(RoutesConsts::kPathAPI) + getName() + "/"+ kActionStopAll;
#ifdef DEBUG
    logger->debug("+" + path);
#endif
    
    OpenAPIRoute stopAllRoute(path.c_str(), "POST", "Stop all servos by setting speed to 0", "Servos", false, {}, standardResponses);
    registerOpenAPIRoute(stopAllRoute);
    webserver.on(path.c_str(), HTTP_PUT, [this]()
                 {
        if (this->setAllServoSpeed(0)) {
            webserver.send(200, RoutesConsts::kMimeJSON, getResultJsonString(RoutesConsts::kResultOk, kActionStopAll).c_str());
        } else {
            webserver.send(456, RoutesConsts::kMimeJSON, getResultJsonString(RoutesConsts::kResultErr, kActionStopAll).c_str());
        } });

    // API: Get attached servo status for a specific channel
    path = std::string(RoutesConsts::kPathAPI) + getName() + "/" + kActionGetStatus;
#ifdef DEBUG
    logger->debug("+" + path);
#endif
    
    std::vector<OpenAPIParameter> statusParams;
    statusParams.push_back(OpenAPIParameter("channel", "integer", "query", "Servo channel (0-7)", true));
    
    std::vector<OpenAPIResponse> statusResponses;
    OpenAPIResponse statusOk(200, "Servo status retrieved");
    statusOk.schema = "{\"type\":\"object\",\"properties\":{\"channel\":{\"type\":\"integer\"},\"status\":{\"type\":\"string\"}}}";
    statusOk.example = "{\"channel\":0,\"status\":\"ANGULAR180\"}";
    statusResponses.push_back(statusOk);
    statusResponses.push_back(OpenAPIResponse(422, "Invalid channel"));
    
    OpenAPIRoute statusRoute(path.c_str(), "GET", "Get servo type and connection status for a specific channel", "Servos", true, statusParams, statusResponses);
    registerOpenAPIRoute(statusRoute);
    webserver.on(path.c_str(), HTTP_GET, [this]()
                 {
        if (!webserver.hasArg(kServoChannel)) {
            webserver.send(422, RoutesConsts::kMimeJSON, getResultJsonString(RoutesConsts::kResultErr, RoutesConsts::kMsgInvalidParams).c_str());
            return;
        }
        
        uint8_t channel = (uint8_t)webserver.arg(kServoChannel).toInt();
        
        if (channel > 7) {
            webserver.send(422, RoutesConsts::kMimeJSON, getResultJsonString(RoutesConsts::kResultErr, RoutesConsts::kMsgInvalidValues).c_str());
            return;
        }
        
        std::string status = getAttachedServo(channel);
        webserver.send(200, RoutesConsts::kMimeJSON, status.c_str());
    });

    // API: Get all attached servos status
    path = std::string(RoutesConsts::kPathAPI) + getName() + "/" + kActionGetAllStatus;
#ifdef DEBUG
    logger->debug("+" + path);
#endif
    
    std::vector<OpenAPIResponse> allStatusResponses;
    OpenAPIResponse allStatusOk(200, "All servos status retrieved");
    allStatusOk.schema = "{\"type\":\"object\",\"properties\":{\"servos\":{\"type\":\"array\",\"items\":{\"type\":\"object\"}}}}";
    allStatusOk.example = "{\"servos\":[{\"channel\":0,\"status\":\"ANGULAR180\"},{\"channel\":1,\"status\":\"NOT_CONNECTED\"}]}";
    allStatusResponses.push_back(allStatusOk);
    
    OpenAPIRoute allStatusRoute(path.c_str(), "GET", "Get connection status and type for all 8 servo channels", "Servos", false, {}, allStatusResponses);
    registerOpenAPIRoute(allStatusRoute);
    webserver.on(path.c_str(), HTTP_GET, [this]()
                 {
        std::string status = getAllAttachedServos();
        webserver.send(200, RoutesConsts::kMimeJSON, status.c_str());
    });

    // API: Set all angular servos to the same angle
    path = std::string(RoutesConsts::kPathAPI) + getName() + "/" + kActionSetAllAngle;
#ifdef DEBUG
    logger->debug("+" + path);
#endif
    
    std::vector<OpenAPIParameter> allAngleParams;
    allAngleParams.push_back(OpenAPIParameter("angle", "integer", "body", "Angle in degrees (0-360)", true));
    
    OpenAPIRoute allAngleRoute(path.c_str(), "POST", "Set all attached angular servos to the same angle simultaneously", "Servos", true, allAngleParams, standardResponses);
    allAngleRoute.requestBody = OpenAPIRequestBody("Angle for all servos",
        "{\"type\":\"object\",\"properties\":{\"angle\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":360}},\"required\":[\"angle\"]}", true);
    allAngleRoute.requestBody.example = "{\"angle\":90}";
    registerOpenAPIRoute(allAngleRoute);
    webserver.on(path.c_str(), HTTP_PUT, [this]()
                 {
        if (!webserver.hasArg(kServoAngle)) {
            webserver.send(422, RoutesConsts::kMimeJSON, getResultJsonString(RoutesConsts::kResultErr, RoutesConsts::kMsgInvalidParams).c_str());
            return;
        }
        
        uint16_t angle = (uint16_t)webserver.arg(kServoAngle).toInt();
        
        if (angle > 360) {
            webserver.send(422, RoutesConsts::kMimeJSON, getResultJsonString(RoutesConsts::kResultErr, RoutesConsts::kMsgInvalidValues).c_str());
            return;
        }
        
        if (setAllServoAngle(angle)) {
            webserver.send(200, RoutesConsts::kMimeJSON, getResultJsonString(RoutesConsts::kResultOk, kActionSetAllAngle).c_str());
        } else {
            webserver.send(400, RoutesConsts::kMimeJSON, getResultJsonString(RoutesConsts::kResultErr, kActionSetAllAngle).c_str());
        }
    });

    // API: Set all continuous servos to the same speed
    path = std::string(RoutesConsts::kPathAPI) + getName() + "/" + kActionSetAllSpeed;
#ifdef DEBUG
    logger->debug("+" + path);
#endif
    
    std::vector<OpenAPIParameter> allSpeedParams;
    allSpeedParams.push_back(OpenAPIParameter("speed", "integer", "body", "Speed percentage (-100 to +100)", true));
    
    OpenAPIRoute allSpeedRoute(path.c_str(), "POST", "Set all attached continuous rotation servos to the same speed simultaneously", "Servos", true, allSpeedParams, standardResponses);
    allSpeedRoute.requestBody = OpenAPIRequestBody("Speed for all servos",
        "{\"type\":\"object\",\"properties\":{\"speed\":{\"type\":\"integer\",\"minimum\":-100,\"maximum\":100}},\"required\":[\"speed\"]}", true);
    allSpeedRoute.requestBody.example = "{\"speed\":50}";
    registerOpenAPIRoute(allSpeedRoute);
    webserver.on(path.c_str(), HTTP_PUT, [this]()
                 {
        if (!webserver.hasArg(kServoSpeed)) {
            webserver.send(422, RoutesConsts::kMimeJSON, getResultJsonString(RoutesConsts::kResultErr, RoutesConsts::kMsgInvalidParams).c_str());
            return;
        }
        
        int8_t speed = (int8_t)webserver.arg(kServoSpeed).toInt();
        
        if (speed < -100 || speed > 100) {
            webserver.send(422, RoutesConsts::kMimeJSON, getResultJsonString(RoutesConsts::kResultErr, RoutesConsts::kMsgInvalidValues).c_str());
            return;
        }
        
        if (setAllServoSpeed(speed)) {
            webserver.send(200, RoutesConsts::kMimeJSON, getResultJsonString(RoutesConsts::kResultOk, kActionSetAllSpeed).c_str());
        } else {
            webserver.send(456, RoutesConsts::kMimeJSON, getResultJsonString(RoutesConsts::kResultErr, kActionSetAllSpeed).c_str());
        }
    });

    // API: Attach servo to a channel
    path = std::string(RoutesConsts::kPathAPI) + getName() + "/" + kActionAttachServo;
#ifdef DEBUG
    logger->debug("+" + path);
#endif
    
    std::vector<OpenAPIParameter> attachParams;
    attachParams.push_back(OpenAPIParameter("channel", "integer", "body", "Servo channel (0-7)", true));
    attachParams.push_back(OpenAPIParameter("model", "integer", "body", "Servo model type (0=180°, 1=270°, 2=continuous)", true));
    
    OpenAPIRoute attachRoute(path.c_str(), "POST", "Register a servo type to a channel before use", "Servos", true, attachParams, standardResponses);
    attachRoute.requestBody = OpenAPIRequestBody("Servo attachment configuration",
        "{\"type\":\"object\",\"properties\":{\"channel\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":7},\"model\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":2,\"description\":\"0=180deg angular, 1=270deg angular, 2=continuous rotation\"}},\"required\":[\"channel\",\"model\"]}", true);
    attachRoute.requestBody.example = "{\"channel\":0,\"model\":0}";
    registerOpenAPIRoute(attachRoute);
    webserver.on(path.c_str(), HTTP_PUT, [this]()
                 {
        if (!webserver.hasArg(kServoChannel) || !webserver.hasArg(kServoModel)) {
            webserver.send(422, RoutesConsts::kMimeJSON, getResultJsonString(RoutesConsts::kResultErr, RoutesConsts::kMsgInvalidParams).c_str());
            return;
        }
        
        uint8_t channel = (uint8_t)webserver.arg(kServoChannel).toInt();
        uint8_t model = (uint8_t)webserver.arg(kServoModel).toInt();
        
        if (channel > 7 || model > 2) {
            webserver.send(422, RoutesConsts::kMimeJSON, getResultJsonString(RoutesConsts::kResultErr, RoutesConsts::kMsgInvalidValues).c_str());
            return;
        }
        
        ServoModel servoModel = static_cast<ServoModel>(model);
        if (attachServo(channel, servoModel)) {
            webserver.send(200, RoutesConsts::kMimeJSON, getResultJsonString(RoutesConsts::kResultOk, kActionAttachServo).c_str());
        } else {
            webserver.send(456, RoutesConsts::kMimeJSON, getResultJsonString(RoutesConsts::kResultErr, kActionAttachServo).c_str());
        }
    });

    return true;
}

std::string ServoService::getName()
{
    return "servos/v1";
}

std::string ServoService::getPath(const std::string& finalpathstring)
{
    if (baseServicePath.empty()) {
        // Cache base path on first call
        std::string serviceName = getName();
        size_t slashPos = serviceName.find('/');
        if (slashPos != std::string::npos) {
            serviceName = serviceName.substr(0, slashPos);
        }
        baseServicePath = std::string(RoutesConsts::kPathAPI) + serviceName + "/";
    }
    return baseServicePath + finalpathstring;
}
