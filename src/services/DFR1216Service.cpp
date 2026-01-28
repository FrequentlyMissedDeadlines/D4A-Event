// DFR1216 (Unihiker Expansion) Service Implementation
/**
 * @file DFR1216Service.cpp
 * @brief Implementation for DFR1216 expansion board integration with the main application
 */

#include "DFR1216Service.h"
#include <WebServer.h>
#include <pgmspace.h>
#include <ArduinoJson.h>

constexpr char kPathDFR1216[] = "dfr1216/";
constexpr char kActionSetServoAngle[] = "setServoAngle";
constexpr char kActionSetMotorSpeed[] = "setMotorSpeed";
constexpr char kActionGetStatus[] = "getStatus";

constexpr char kParamChannel[] = "channel";
constexpr char kParamMotor[] = "motor";
constexpr char kParamAngle[] = "angle";
constexpr char kParamSpeed[] = "speed";

// Error messages stored in PROGMEM to save RAM
static const char kMsgNotInitialized[] PROGMEM = "DFR1216 controller not initialized.";
static const char kMsgSuccess[] PROGMEM = "Command executed successfully.";
static const char kMsgFailed[] PROGMEM = "Command failed.";
static const char kMsgMissingParamsServo[] PROGMEM = "Missing required parameters: channel and angle";
static const char kMsgMissingParamsMotor[] PROGMEM = "Missing required parameters: motor and speed";

// JSON key constants in PROGMEM
static const char JSON_SERVICE[] PROGMEM = "service";
static const char JSON_INITIALIZED[] PROGMEM = "initialized";
static const char JSON_SUCCESS[] PROGMEM = "success";
static const char JSON_CHANNEL[] PROGMEM = "channel";
static const char JSON_ANGLE[] PROGMEM = "angle";
static const char JSON_MOTOR[] PROGMEM = "motor";
static const char JSON_SPEED[] PROGMEM = "speed";

// Common HTTP constants in PROGMEM
static const char kJsonErrorPrefix[] PROGMEM = "{\"error\":\"";
static const char kJsonErrorSuffix[] PROGMEM = "\"}";

// API route paths
static const char kRouteSetServoAngle[] = "/api/dfr1216/setServoAngle";
static const char kRouteSetMotorSpeed[] = "/api/dfr1216/setMotorSpeed";
static const char kRouteGetStatus[] = "/api/dfr1216/getStatus";

// DFR1216-specific OpenAPI constants namespace
namespace DFR1216Consts {
    // Parameter names
    static const char ParamChannel[] PROGMEM = "channel";
    static const char ParamMotor[] PROGMEM = "motor";
    static const char ParamAngle[] PROGMEM = "angle";
    static const char ParamSpeed[] PROGMEM = "speed";
    
    // Schema definitions
    static const char SchemaChannelAngle[] PROGMEM = "{\"type\":\"object\",\"required\":[\"channel\",\"angle\"],\"properties\":{\"channel\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":5},\"angle\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":180}}}";
    static const char SchemaMotorSpeed[] PROGMEM = "{\"type\":\"object\",\"required\":[\"motor\",\"speed\"],\"properties\":{\"motor\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4},\"speed\":{\"type\":\"integer\",\"minimum\":-100,\"maximum\":100}}}";
    static const char SchemaServiceStatus[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"service\":{\"type\":\"string\"},\"initialized\":{\"type\":\"boolean\"}}}";
    
    // Request body schemas
    static const char ReqChannelAngle05[] PROGMEM = "{\"type\":\"object\",\"required\":[\"channel\",\"angle\"],\"properties\":{\"channel\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":5},\"angle\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":180}}}";
    static const char ReqMotorSpeed[] PROGMEM = "{\"type\":\"object\",\"required\":[\"motor\",\"speed\"],\"properties\":{\"motor\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4},\"speed\":{\"type\":\"integer\",\"minimum\":-100,\"maximum\":100}}}";
    
    // Example values
    static const char ExChannelAngle[] PROGMEM = "{\"channel\":0,\"angle\":90}";
    static const char ExMotorSpeed[] PROGMEM = "{\"motor\":1,\"speed\":50}";
}

// Helper function to create JSON error response
static inline String createJsonError(const __FlashStringHelper* msg)
{
    String result;
    result.reserve(64);
    result += FPSTR(kJsonErrorPrefix);
    result += msg;
    result += FPSTR(kJsonErrorSuffix);
    return result;
}

bool DFR1216Service::initializeService()
{
    initialized = controller.begin();
    if (initialized)
    {
        logger->info("DFR1216Service initialized successfully");
    }
    else
    {
        logger->error("Failed to initialize DFR1216Service");
    }
    return initialized;
}

bool DFR1216Service::startService()
{
    if (!initialized)
    {
        logger->error("Cannot start DFR1216Service - not initialized");
        return false;
    }
    logger->info("DFR1216Service started");
    return true;
}

bool DFR1216Service::stopService()
{
    if (initialized)
    {
        logger->info("DFR1216Service stopped");
    }
    return true;
}

bool DFR1216Service::setServoAngle(uint8_t channel, uint16_t angle)
{
    if (!initialized)
    {
        logger->error("DFR1216Service not initialized");
        return false;
    }

    if (channel > 5)
    {
        logger->error("Servo channel out of range (0-5)");
        return false;
    }
    if (angle > 180)
    {
        logger->error("Angle out of range (0-180)");
        return false;
    }

    controller.setServoAngle(static_cast<eServoNumber_t>(channel), angle);
    
    // Avoid string concatenation - log separately
    char logBuf[64];
    snprintf(logBuf, sizeof(logBuf), "Set servo %u to angle %u", channel, angle);
    logger->info(logBuf);
    return true;
}

bool DFR1216Service::setMotorSpeed(uint8_t motor, int8_t speed)
{
    if (!initialized)
    {
        logger->error("DFR1216Service not initialized");
        return false;
    }

    if (motor < 1 || motor > 4)
    {
        logger->error("Motor number out of range (1-4)");
        return false;
    }
    if (speed < -100 || speed > 100)
    {
        logger->error("Speed out of range (-100 to +100)");
        return false;
    }

    // Convert speed percentage to duty cycle (0-65535)
    // Negative speed = reverse, positive = forward
    uint16_t duty = 0;
    eMotorNumber_t motorEnum;
    
    // Map motor number to enum
    switch (motor)
    {
        case 1: motorEnum = speed >= 0 ? eMotor1_A : eMotor1_B; break;
        case 2: motorEnum = speed >= 0 ? eMotor2_A : eMotor2_B; break;
        case 3: motorEnum = speed >= 0 ? eMotor3_A : eMotor3_B; break;
        case 4: motorEnum = speed >= 0 ? eMotor4_A : eMotor4_B; break;
        default: return false;
    }
    
    // Convert speed percentage to duty cycle (0-65535)
    duty = static_cast<uint16_t>((abs(speed) * 65535) / 100);
    
    controller.setMotorDuty(motorEnum, duty);

    // Avoid string concatenation - log separately
    char logBuf[64];
    snprintf(logBuf, sizeof(logBuf), "Set motor %u to speed %d", motor, speed);
    logger->info(logBuf);
    return true;
}

std::string DFR1216Service::getStatus()
{
    // Specify capacity to avoid heap fragmentation
    JsonDocument doc;
    doc[FPSTR(JSON_SERVICE)] = "DFR1216Service";
    doc[FPSTR(JSON_INITIALIZED)] = initialized;
    doc[RoutesConsts::FieldStatus] = initialized ? "running" : "not initialized";

    std::string output;
    serializeJson(doc, output);
    return output;
}

bool DFR1216Service::registerRoutes()
{
    // Define parameters for servo angle endpoint
    std::vector<OpenAPIParameter> servoParams;
    servoParams.push_back(OpenAPIParameter(DFR1216Consts::ParamChannel, RoutesConsts::TypeInteger, RoutesConsts::InQuery, "Servo channel (0-5)", true));
    servoParams.push_back(OpenAPIParameter(DFR1216Consts::ParamAngle, RoutesConsts::TypeInteger, RoutesConsts::InQuery, "Angle in degrees (0-180)", true));
    
    std::vector<OpenAPIResponse> servoResponses;
    OpenAPIResponse servoOk(200, "Servo angle set successfully");
    servoOk.schema = DFR1216Consts::SchemaChannelAngle;
    servoOk.example = "{\"status\":\"success\",\"channel\":0,\"angle\":90}";
    servoResponses.push_back(servoOk);
    servoResponses.push_back(createMissingParamsResponse());
    servoResponses.push_back(createNotInitializedResponse());

    OpenAPIRoute servoRoute(getPath(kRouteSetServoAngle).c_str(), RoutesConsts::MethodPOST, 
                            "Set the angle of a servo motor on the DFR1216 expansion board", 
                            "DFR1216", false, servoParams, servoResponses);
    servoRoute.requestBody = OpenAPIRequestBody("Servo control parameters", 
        DFR1216Consts::ReqChannelAngle05, true);
    servoRoute.requestBody.example = DFR1216Consts::ExChannelAngle;
    openAPIRoutes.push_back(servoRoute);

    // Define parameters for motor speed endpoint
    std::vector<OpenAPIParameter> motorParams;
    motorParams.push_back(OpenAPIParameter(DFR1216Consts::ParamMotor, RoutesConsts::TypeInteger, RoutesConsts::InQuery, "Motor number (1-4)", true));
    motorParams.push_back(OpenAPIParameter(DFR1216Consts::ParamSpeed, RoutesConsts::TypeInteger, RoutesConsts::InQuery, "Speed percentage (-100 to +100)", true));
    
    std::vector<OpenAPIResponse> motorResponses;
    OpenAPIResponse motorOk(200, "Motor speed set successfully");
    motorOk.schema = DFR1216Consts::SchemaMotorSpeed;
    motorOk.example = "{\"status\":\"success\",\"motor\":1,\"speed\":75}";
    motorResponses.push_back(motorOk);
    motorResponses.push_back(createMissingParamsResponse());
    motorResponses.push_back(createNotInitializedResponse());

    OpenAPIRoute motorRoute(getPath(kRouteSetMotorSpeed).c_str(), RoutesConsts::MethodPOST,
                            "Set the speed and direction of a DC motor on the DFR1216 expansion board",
                            "DFR1216", false, motorParams, motorResponses);
    motorRoute.requestBody = OpenAPIRequestBody("Motor control parameters",
        DFR1216Consts::ReqMotorSpeed, true);
    motorRoute.requestBody.example = DFR1216Consts::ExMotorSpeed;
    openAPIRoutes.push_back(motorRoute);

    // Define status endpoint
    std::vector<OpenAPIResponse> statusResponses;
    OpenAPIResponse statusOk(200, "Status retrieved successfully");
    statusOk.schema = DFR1216Consts::SchemaServiceStatus;
    statusOk.example = "{\"service\":\"DFR1216Service\",\"initialized\":true,\"status\":\"running\"}";
    statusResponses.push_back(statusOk);

    openAPIRoutes.push_back(
        OpenAPIRoute(getPath(kRouteGetStatus).c_str(), RoutesConsts::MethodGET,
                     "Get initialization status and operational state of the DFR1216 expansion board",
                     "DFR1216", false, {}, statusResponses));

    // Register actual HTTP handlers
    webserver.on(getPath(kRouteSetServoAngle).c_str(), HTTP_POST,
                 [this]()
                 { this->handleSetServoAngle(); });

    webserver.on(getPath(kRouteSetMotorSpeed).c_str(), HTTP_POST,
                 [this]()
                 { this->handleSetMotorSpeed(); });

    webserver.on(getPath(kRouteGetStatus).c_str(), HTTP_GET,
                 [this]()
                 { this->handleGetStatus(); });

    registerSettingsRoutes("DFR1216", this);

    logger->info("DFR1216Service routes registered");
    return true;
}

void DFR1216Service::handleSetServoAngle()
{
    if (!initialized)
    {
        webserver.send(503, RoutesConsts::MimeJSON, createJsonError(FPSTR(kMsgNotInitialized)));
        return;
    }

    if (!webserver.hasArg(kParamChannel) || !webserver.hasArg(kParamAngle))
    {
        webserver.send(422, RoutesConsts::MimeJSON, createJsonError(FPSTR(kMsgMissingParamsServo)));
        return;
    }

    uint8_t channel = webserver.arg(kParamChannel).toInt();
    uint16_t angle = webserver.arg(kParamAngle).toInt();

    if (setServoAngle(channel, angle))
    {
        JsonDocument doc;
        doc[RoutesConsts::FieldStatus] = FPSTR(JSON_SUCCESS);
        doc[FPSTR(JSON_CHANNEL)] = channel;
        doc[FPSTR(JSON_ANGLE)] = angle;
        
        std::string response;
        serializeJson(doc, response);
        webserver.send(200, RoutesConsts::MimeJSON, response.c_str());
    }
    else
    {
        webserver.send(456, RoutesConsts::MimeJSON, createJsonError(FPSTR(kMsgFailed)));
    }
}

void DFR1216Service::handleSetMotorSpeed()
{
    if (!initialized)
    {
        webserver.send(503, RoutesConsts::MimeJSON, createJsonError(FPSTR(kMsgNotInitialized)));
        return;
    }

    if (!webserver.hasArg(kParamMotor) || !webserver.hasArg(kParamSpeed))
    {
        webserver.send(422, RoutesConsts::MimeJSON, createJsonError(FPSTR(kMsgMissingParamsMotor)));
        return;
    }

    uint8_t motor = webserver.arg(kParamMotor).toInt();
    int8_t speed = webserver.arg(kParamSpeed).toInt();

    if (setMotorSpeed(motor, speed))
    {
        JsonDocument doc;
        doc[RoutesConsts::FieldStatus] = FPSTR(JSON_SUCCESS);
        doc[FPSTR(JSON_MOTOR)] = motor;
        doc[FPSTR(JSON_SPEED)] = speed;
        
        std::string response;
        serializeJson(doc, response);
        webserver.send(200, RoutesConsts::MimeJSON, response.c_str());
    }
    else
    {
        webserver.send(456, RoutesConsts::MimeJSON, createJsonError(FPSTR(kMsgFailed)));
    }
}

void DFR1216Service::handleGetStatus()
{
    std::string status = getStatus();
    webserver.send(200, RoutesConsts::MimeJSON, status.c_str());
}

bool DFR1216Service::saveSettings()
{
    // To be implemented if needed
    return true;
}

bool DFR1216Service::loadSettings()
{
    // To be implemented if needed
    return true;
}
