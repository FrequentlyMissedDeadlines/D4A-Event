// DFR1216 (Unihiker Expansion) Service Implementation
/**
 * @file DFR1216Service.cpp
 * @brief Implementation for DFR1216 expansion board integration with the main application
 * @details Exposed routes:
 *          - POST /api/dfr1216/setServoAngle - Set the angle of a servo motor on the expansion board
 *          - POST /api/dfr1216/setMotorSpeed - Set the speed and direction of a DC motor
 *          - GET /api/dfr1216/getStatus - Get initialization status and operational state of the board
 * 
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

// Additional DFR1216Service constants (merge with DFR1216Consts below)
namespace DFR1216ExtraConsts
{
    constexpr const char msg_init_success[] PROGMEM = "DFR1216Service initialized successfully";
    constexpr const char msg_init_failed[] PROGMEM = "Failed to initialize DFR1216Service";
    constexpr const char msg_cannot_start[] PROGMEM = "Cannot start DFR1216Service - not initialized";
    constexpr const char msg_started[] PROGMEM = "DFR1216Service started";
    constexpr const char msg_stopped[] PROGMEM = "DFR1216Service stopped";
    constexpr const char msg_not_initialized[] PROGMEM = "DFR1216Service not initialized";
    constexpr const char msg_servo_out_of_range[] PROGMEM = "Servo channel out of range (0-5)";
    constexpr const char msg_angle_out_of_range[] PROGMEM = "Angle out of range (0-180)";
    constexpr const char msg_motor_out_of_range[] PROGMEM = "Motor number out of range (1-4)";
    constexpr const char msg_speed_out_of_range[] PROGMEM = "Speed out of range (-100 to +100)";
    constexpr const char msg_routes_registered[] PROGMEM = "DFR1216Service routes registered";
    constexpr const char desc_servo_channel[] PROGMEM = "Servo channel (0-5)";
    constexpr const char desc_angle_degrees[] PROGMEM = "Angle in degrees (0-180)";
    constexpr const char desc_motor_number[] PROGMEM = "Motor number (1-4)";
    constexpr const char desc_speed_percent[] PROGMEM = "Speed percentage (-100 to +100)";
    constexpr const char resp_servo_angle_set[] PROGMEM = "Servo angle set successfully";
    constexpr const char resp_motor_speed_set[] PROGMEM = "Motor speed set successfully";
    constexpr const char resp_status_retrieved[] PROGMEM = "Status retrieved successfully";
    constexpr const char desc_servo[] PROGMEM = "Set the angle of a servo motor on the DFR1216 expansion board";
    constexpr const char desc_motor[] PROGMEM = "Set the speed and direction of a DC motor on the DFR1216 expansion board";
    constexpr const char desc_status[] PROGMEM = "Get initialization status and operational state of the DFR1216 expansion board";
    constexpr const char desc_servo_params[] PROGMEM = "Servo control parameters";
    constexpr const char desc_motor_params[] PROGMEM = "Motor control parameters";
    constexpr const char tag_dfr1216[] PROGMEM = "DFR1216";
}

// DFR1216-specific OpenAPI constants namespace
namespace DFR1216Consts
{
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
static inline String createJsonError(const __FlashStringHelper *msg)
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
        service_status_ = STARTED;
        logger->info(std::string(DFR1216ExtraConsts::msg_init_success));
    }
    else
    {
        service_status_ = INIT_FAILED;
        logger->error(std::string(DFR1216ExtraConsts::msg_init_failed));
    }
    status_timestamp_ = millis();
    return initialized;
}

bool DFR1216Service::startService()
{
    if (!initialized)
    {
        service_status_ = START_FAILED;
        status_timestamp_ = millis();
        logger->error(std::string(DFR1216ExtraConsts::msg_cannot_start));
        return false;
    }
    service_status_ = STARTED;
    status_timestamp_ = millis();
    logger->info(std::string(DFR1216ExtraConsts::msg_started));
    return true;
}

bool DFR1216Service::stopService()
{
    if (initialized)
    {
        service_status_ = STOPPED;
        status_timestamp_ = millis();
        logger->info(std::string(DFR1216ExtraConsts::msg_stopped));
    }
    return true;
}

bool DFR1216Service::setServoAngle(uint8_t channel, uint16_t angle)
{
    if (!initialized)
    {
        logger->error(std::string(DFR1216ExtraConsts::msg_not_initialized));
        return false;
    }

    if (channel > 5)
    {
        logger->error(std::string(DFR1216ExtraConsts::msg_servo_out_of_range));
        return false;
    }
    if (angle > 180)
    {
        logger->error(std::string(DFR1216ExtraConsts::msg_angle_out_of_range));
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
        logger->error(std::string(DFR1216ExtraConsts::msg_not_initialized));
        return false;
    }

    if (motor < 1 || motor > 4)
    {
        logger->error(std::string(DFR1216ExtraConsts::msg_motor_out_of_range));
        return false;
    }
    if (speed < -100 || speed > 100)
    {
        logger->error(std::string(DFR1216ExtraConsts::msg_speed_out_of_range));
        return false;
    }

    // Convert speed percentage to duty cycle (0-65535)
    // Negative speed = reverse, positive = forward
    uint16_t duty = 0;
    eMotorNumber_t motorEnum;

    // Map motor number to enum
    switch (motor)
    {
    case 1:
        motorEnum = speed >= 0 ? eMotor1_A : eMotor1_B;
        break;
    case 2:
        motorEnum = speed >= 0 ? eMotor2_A : eMotor2_B;
        break;
    case 3:
        motorEnum = speed >= 0 ? eMotor3_A : eMotor3_B;
        break;
    case 4:
        motorEnum = speed >= 0 ? eMotor4_A : eMotor4_B;
        break;
    default:
        return false;
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
    doc[RoutesConsts::field_status] = initialized ? "running" : "not initialized";

    std::string output;
    serializeJson(doc, output);
    return output;
}

bool DFR1216Service::registerRoutes()
{
    // Define parameters for servo angle endpoint
    std::vector<OpenAPIParameter> servoParams;
    servoParams.push_back(OpenAPIParameter(DFR1216Consts::ParamChannel, RoutesConsts::type_integer, RoutesConsts::in_query, DFR1216ExtraConsts::desc_servo_channel, true));
    servoParams.push_back(OpenAPIParameter(DFR1216Consts::ParamAngle, RoutesConsts::type_integer, RoutesConsts::in_query, DFR1216ExtraConsts::desc_angle_degrees, true));

    std::vector<OpenAPIResponse> servoResponses;
    OpenAPIResponse servoOk(200, DFR1216ExtraConsts::resp_servo_angle_set);
    servoOk.schema = DFR1216Consts::SchemaChannelAngle;
    servoOk.example = "{\"status\":\"success\",\"channel\":0,\"angle\":90}";
    servoResponses.push_back(servoOk);
    servoResponses.push_back(createMissingParamsResponse());
    servoResponses.push_back(createNotInitializedResponse());

    OpenAPIRoute servoRoute(getPath(kRouteSetServoAngle).c_str(), RoutesConsts::method_post,
                            DFR1216ExtraConsts::desc_servo,
                            DFR1216ExtraConsts::tag_dfr1216, false, servoParams, servoResponses);
    servoRoute.requestBody = OpenAPIRequestBody(DFR1216ExtraConsts::desc_servo_params,
                                                DFR1216Consts::ReqChannelAngle05, true);
    servoRoute.requestBody.example = DFR1216Consts::ExChannelAngle;
    registerOpenAPIRoute(servoRoute);

    // Define parameters for motor speed endpoint
    std::vector<OpenAPIParameter> motorParams;
    motorParams.push_back(OpenAPIParameter(DFR1216Consts::ParamMotor, RoutesConsts::type_integer, RoutesConsts::in_query, DFR1216ExtraConsts::desc_motor_number, true));
    motorParams.push_back(OpenAPIParameter(DFR1216Consts::ParamSpeed, RoutesConsts::type_integer, RoutesConsts::in_query, DFR1216ExtraConsts::desc_speed_percent, true));

    std::vector<OpenAPIResponse> motorResponses;
    OpenAPIResponse motorOk(200, DFR1216ExtraConsts::resp_motor_speed_set);
    motorOk.schema = DFR1216Consts::SchemaMotorSpeed;
    motorOk.example = "{\"status\":\"success\",\"motor\":1,\"speed\":75}";
    motorResponses.push_back(motorOk);
    motorResponses.push_back(createMissingParamsResponse());
    motorResponses.push_back(createNotInitializedResponse());

    OpenAPIRoute motorRoute(getPath(kRouteSetMotorSpeed).c_str(), RoutesConsts::method_post,
                            DFR1216ExtraConsts::desc_motor,
                            DFR1216ExtraConsts::tag_dfr1216, false, motorParams, motorResponses);
    motorRoute.requestBody = OpenAPIRequestBody(DFR1216ExtraConsts::desc_motor_params,
                                                DFR1216Consts::ReqMotorSpeed, true);
    motorRoute.requestBody.example = DFR1216Consts::ExMotorSpeed;
    registerOpenAPIRoute(motorRoute);

    // Define status endpoint with standardized service status format
    static constexpr char kStatusSchema[] PROGMEM = R"({"type":"object","properties":{"servicename":{"type":"string","description":"Service name"},"status":{"type":"string","enum":["init failed","start failed","started","stopped","stop failed"],"description":"Current service status"},"ts":{"type":"integer","description":"Unix timestamp of status change"}}})";
    static constexpr char kStatusExample[] PROGMEM = R"({"servicename":"DFR1216Service","status":"started","ts":123456789})";

    std::vector<OpenAPIResponse> statusResponses;
    OpenAPIResponse statusOk(200, FPSTR(DFR1216ExtraConsts::resp_status_retrieved));
    statusOk.schema = kStatusSchema;
    statusOk.example = kStatusExample;
    statusResponses.push_back(statusOk);

    registerOpenAPIRoute(
        OpenAPIRoute(getPath(kRouteGetStatus).c_str(), RoutesConsts::method_get,
                     DFR1216ExtraConsts::desc_status,
                     DFR1216ExtraConsts::tag_dfr1216, false, {}, statusResponses));

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

    logger->info(FPSTR(DFR1216ExtraConsts::msg_routes_registered));
    return true;
}

void DFR1216Service::handleSetServoAngle()
{
    if (!initialized)
    {
        webserver.send(503, RoutesConsts::mime_json, createJsonError(FPSTR(kMsgNotInitialized)));
        return;
    }

    if (!webserver.hasArg(kParamChannel) || !webserver.hasArg(kParamAngle))
    {
        webserver.send(422, RoutesConsts::mime_json, createJsonError(FPSTR(kMsgMissingParamsServo)));
        return;
    }

    uint8_t channel = webserver.arg(kParamChannel).toInt();
    uint16_t angle = webserver.arg(kParamAngle).toInt();

    if (setServoAngle(channel, angle))
    {
        JsonDocument doc;
        doc[RoutesConsts::field_status] = FPSTR(JSON_SUCCESS);
        doc[FPSTR(JSON_CHANNEL)] = channel;
        doc[FPSTR(JSON_ANGLE)] = angle;

        std::string response;
        serializeJson(doc, response);
        webserver.send(200, RoutesConsts::mime_json, response.c_str());
    }
    else
    {
        webserver.send(456, RoutesConsts::mime_json, createJsonError(FPSTR(kMsgFailed)));
    }
}

void DFR1216Service::handleSetMotorSpeed()
{
    if (!initialized)
    {
        webserver.send(503, RoutesConsts::mime_json, createJsonError(FPSTR(kMsgNotInitialized)));
        return;
    }

    if (!webserver.hasArg(kParamMotor) || !webserver.hasArg(kParamSpeed))
    {
        webserver.send(422, RoutesConsts::mime_json, createJsonError(FPSTR(kMsgMissingParamsMotor)));
        return;
    }

    uint8_t motor = webserver.arg(kParamMotor).toInt();
    int8_t speed = webserver.arg(kParamSpeed).toInt();

    if (setMotorSpeed(motor, speed))
    {
        JsonDocument doc;
        doc[RoutesConsts::field_status] = FPSTR(JSON_SUCCESS);
        doc[FPSTR(JSON_MOTOR)] = motor;
        doc[FPSTR(JSON_SPEED)] = speed;

        std::string response;
        serializeJson(doc, response);
        webserver.send(200, RoutesConsts::mime_json, response.c_str());
    }
    else
    {
        webserver.send(456, RoutesConsts::mime_json, createJsonError(FPSTR(kMsgFailed)));
    }
}

void DFR1216Service::handleGetStatus()
{
    JsonDocument doc;
    doc[PSTR("servicename")] = "DFR1216Service";

    const char *status_str = "unknown";
    switch (service_status_)
    {
    case INIT_FAILED:
        status_str = "init failed";
        break;
    case START_FAILED:
        status_str = "start failed";
        break;
    case STARTED:
        status_str = "started";
        break;
    case STOPPED:
        status_str = "stopped";
        break;
    case STOP_FAILED:
        status_str = "stop failed";
        break;
    }
    doc[PSTR("status")] = status_str;
    doc[PSTR("ts")] = (unsigned long)status_timestamp_;

    std::string response;
    serializeJson(doc, response);
    webserver.send(200, RoutesConsts::mime_json, response.c_str());
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
