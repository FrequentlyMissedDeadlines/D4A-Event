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
#include "FlashStringHelper.h"
#include <WebServer.h>
#include <pgmspace.h>
#include <ArduinoJson.h>

// DFR1216-specific OpenAPI constants namespace (stored in PROGMEM to save RAM)
namespace DFR1216Consts
{
    // Route paths and actions
    constexpr const char path_dfr1216[] PROGMEM = "dfr1216/";
    constexpr const char action_set_servo_angle[] PROGMEM = "setServoAngle";
    constexpr const char action_set_motor_speed[] PROGMEM = "setMotorSpeed";
    constexpr const char action_get_status[] PROGMEM = "getStatus";

    // Parameter names
    constexpr const char param_channel[] PROGMEM = "channel";
    constexpr const char param_motor[] PROGMEM = "motor";
    constexpr const char param_angle[] PROGMEM = "angle";
    constexpr const char param_speed[] PROGMEM = "speed";

    // Error messages
    constexpr const char msg_servo_channel_out_of_range[] PROGMEM = "Servo channel out of range (0-5)";
    constexpr const char msg_angle_out_of_range[] PROGMEM = "Angle out of range (0-180)";
    constexpr const char msg_motor_out_of_range[] PROGMEM = "Motor number out of range (1-4)";
    constexpr const char msg_speed_out_of_range[] PROGMEM = "Speed out of range (-100 to +100)";
    constexpr const char msg_missing_servo_params[] PROGMEM = "Missing required parameters: channel and angle";
    constexpr const char msg_missing_motor_params[] PROGMEM = "Missing required parameters: motor and speed";

    // JSON key constants
    constexpr const char json_channel[] PROGMEM = "channel";
    constexpr const char json_angle[] PROGMEM = "angle";
    constexpr const char json_motor[] PROGMEM = "motor";
    constexpr const char json_speed[] PROGMEM = "speed";
    constexpr const char json_error[] PROGMEM = "{\"error\":\"";

    // Description constants for OpenAPI
    constexpr const char desc_servo_channel[] PROGMEM = "Servo channel (0-5)";
    constexpr const char desc_angle_degrees[] PROGMEM = "Angle in degrees (0-180)";
    constexpr const char desc_motor_number[] PROGMEM = "Motor number (1-4)";
    constexpr const char desc_speed_percent[] PROGMEM = "Speed percentage (-100 to +100)";
    constexpr const char desc_servo_control[] PROGMEM = "Set the angle of a servo motor on the DFR1216 expansion board";
    constexpr const char desc_motor_control[] PROGMEM = "Set the speed and direction of a DC motor on the DFR1216 expansion board";
    constexpr const char desc_get_status[] PROGMEM = "Get initialization status and operational state of the DFR1216 expansion board";
    constexpr const char desc_servo_params[] PROGMEM = "Servo control parameters";
    constexpr const char desc_motor_params[] PROGMEM = "Motor control parameters";
    constexpr const char tag_dfr1216[] PROGMEM = "DFR1216";

    // Response descriptions
    constexpr const char resp_servo_angle_set[] PROGMEM = "Servo angle set successfully";
    constexpr const char resp_motor_speed_set[] PROGMEM = "Motor speed set successfully";
    constexpr const char resp_status_retrieved[] PROGMEM = "Status retrieved successfully";

    // JSON Schema definitions
    constexpr const char schema_channel_angle[] PROGMEM = "{\"type\":\"object\",\"required\":[\"channel\",\"angle\"],\"properties\":{\"channel\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":5},\"angle\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":180}}}";
    constexpr const char schema_motor_speed[] PROGMEM = "{\"type\":\"object\",\"required\":[\"motor\",\"speed\"],\"properties\":{\"motor\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4},\"speed\":{\"type\":\"integer\",\"minimum\":-100,\"maximum\":100}}}";
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
    constexpr const char resp_status_example[] PROGMEM = "{\"message\":\"DFR1216Service\",\"status\":\"started\"}";
}

/**
 * @brief Helper function to create JSON error response
 * @param msg Error message as PROGMEM pointer
 * @return Error JSON string
 */
static inline String create_error_json(const __FlashStringHelper *msg)
{
    String result;
    result.reserve(64);
    result += FPSTR(DFR1216Consts::json_error);
    result += msg;
    result += "}";
    return result;
}

bool DFR1216Service::initializeService()
{
    initialized = controller.begin();
    service_status_ = initialized ? STARTED : INIT_FAILED;
    status_timestamp_ = millis();
    
    if (initialized) {
        logger->info(getServiceName() + " " + fpstr_to_string(FPSTR(ServiceInterfaceConsts::msg_initialize_done)));
    } else {
        logger->error(getServiceName() + " " + fpstr_to_string(FPSTR(ServiceInterfaceConsts::msg_initialize_failed)));
    }
    
    return initialized;
}

bool DFR1216Service::startService()
{
    if (!initialized) {
        service_status_ = START_FAILED;
        status_timestamp_ = millis();
        logger->error(getServiceName() + " " + fpstr_to_string(FPSTR(ServiceInterfaceConsts::msg_start_failed)));
        return false;
    }
    
    service_status_ = STARTED;
    status_timestamp_ = millis();
    logger->info(getServiceName() + " " + fpstr_to_string(FPSTR(ServiceInterfaceConsts::msg_start_done)));
    return true;
}

bool DFR1216Service::stopService()
{
    service_status_ = STOPPED;
    status_timestamp_ = millis();
    logger->info(getServiceName() + " " + fpstr_to_string(FPSTR(ServiceInterfaceConsts::msg_stop_done)));
    return true;
}

bool DFR1216Service::setServoAngle(uint8_t channel, uint16_t angle)
{
    if (!initialized) {
        logger->error(fpstr_to_string(FPSTR(RoutesConsts::resp_not_initialized)));
        return false;
    }

    if (channel > 5) {
        logger->error(fpstr_to_string(FPSTR(DFR1216Consts::msg_servo_channel_out_of_range)));
        return false;
    }
    
    if (angle > 180) {
        logger->error(fpstr_to_string(FPSTR(DFR1216Consts::msg_angle_out_of_range)));
        return false;
    }

    controller.setServoAngle(static_cast<eServoNumber_t>(channel), angle);

    char log_buf[64];
    snprintf(log_buf, sizeof(log_buf), "Set servo %u to angle %u", channel, angle);
    logger->info(log_buf);
    return true;
}

bool DFR1216Service::setMotorSpeed(uint8_t motor, int8_t speed)
{
    if (!initialized) {
        logger->error(fpstr_to_string(FPSTR(RoutesConsts::resp_not_initialized)));
        return false;
    }

    if (motor < 1 || motor > 4) {
        logger->error(fpstr_to_string(FPSTR(DFR1216Consts::msg_motor_out_of_range)));
        return false;
    }
    
    if (speed < -100 || speed > 100) {
        logger->error(fpstr_to_string(FPSTR(DFR1216Consts::msg_speed_out_of_range)));
        return false;
    }

    // Convert speed percentage to duty cycle (0-65535)
    // Negative speed = reverse, positive = forward
    eMotorNumber_t motor_enum;

    // Map motor number to enum
    switch (motor) {
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

std::string DFR1216Service::getStatus()
{
    JsonDocument doc;
    doc[FPSTR(RoutesConsts::message)] = getServiceName();
    doc[FPSTR(RoutesConsts::field_status)] = initialized ? "running" : "not initialized";

    std::string output;
    serializeJson(doc, output);
    return output;
}

bool DFR1216Service::registerRoutes()
{
    // Define parameters for servo angle endpoint
    std::vector<OpenAPIParameter> servo_params;
    servo_params.push_back(OpenAPIParameter(
        "channel", RoutesConsts::type_integer, RoutesConsts::in_query,
        FPSTR(DFR1216Consts::desc_servo_channel), true));
    servo_params.push_back(OpenAPIParameter(
        "angle", RoutesConsts::type_integer, RoutesConsts::in_query,
        FPSTR(DFR1216Consts::desc_angle_degrees), true));

    std::vector<OpenAPIResponse> servo_responses;
    OpenAPIResponse servo_ok(200, FPSTR(DFR1216Consts::resp_servo_angle_set));
    servo_ok.schema = DFR1216Consts::schema_channel_angle;
    servo_ok.example = DFR1216Consts::resp_servo_angle_example;
    servo_responses.push_back(servo_ok);
    servo_responses.push_back(createMissingParamsResponse());
    servo_responses.push_back(createNotInitializedResponse());

    OpenAPIRoute servo_route(getPath("setServoAngle").c_str(),
                             RoutesConsts::method_post,
                             fpstr_to_string(FPSTR(DFR1216Consts::desc_servo_control)).c_str(),
                             FPSTR(DFR1216Consts::tag_dfr1216), false, servo_params, servo_responses);
    servo_route.requestBody = OpenAPIRequestBody(FPSTR(DFR1216Consts::desc_servo_params),
                                                 DFR1216Consts::req_channel_angle, true);
    servo_route.requestBody.example = DFR1216Consts::ex_channel_angle;
    registerOpenAPIRoute(servo_route);

    // Define parameters for motor speed endpoint
    std::vector<OpenAPIParameter> motor_params;
    motor_params.push_back(OpenAPIParameter(
        "motor", RoutesConsts::type_integer, RoutesConsts::in_query,
        FPSTR(DFR1216Consts::desc_motor_number), true));
    motor_params.push_back(OpenAPIParameter(
        "speed", RoutesConsts::type_integer, RoutesConsts::in_query,
        FPSTR(DFR1216Consts::desc_speed_percent), true));

    std::vector<OpenAPIResponse> motor_responses;
    OpenAPIResponse motor_ok(200, FPSTR(DFR1216Consts::resp_motor_speed_set));
    motor_ok.schema = DFR1216Consts::schema_motor_speed;
    motor_ok.example = DFR1216Consts::resp_motor_speed_example;
    motor_responses.push_back(motor_ok);
    motor_responses.push_back(createMissingParamsResponse());
    motor_responses.push_back(createNotInitializedResponse());

    OpenAPIRoute motor_route(getPath("setMotorSpeed").c_str(),
                             RoutesConsts::method_post,
                             fpstr_to_string(FPSTR(DFR1216Consts::desc_motor_control)).c_str(),
                             FPSTR(DFR1216Consts::tag_dfr1216), false, motor_params, motor_responses);
    motor_route.requestBody = OpenAPIRequestBody(FPSTR(DFR1216Consts::desc_motor_params),
                                                 DFR1216Consts::req_motor_speed, true);
    motor_route.requestBody.example = DFR1216Consts::ex_motor_speed;
    registerOpenAPIRoute(motor_route);

    // Define status endpoint
    std::vector<OpenAPIResponse> status_responses;
    OpenAPIResponse status_ok(200, FPSTR(DFR1216Consts::resp_status_retrieved));
    status_ok.schema = DFR1216Consts::resp_status_schema;
    status_ok.example = DFR1216Consts::resp_status_example;
    status_responses.push_back(status_ok);

    registerOpenAPIRoute(
        OpenAPIRoute(getPath("getStatus").c_str(),
                     RoutesConsts::method_get,
                     fpstr_to_string(FPSTR(DFR1216Consts::desc_get_status)).c_str(),
                     FPSTR(DFR1216Consts::tag_dfr1216), false, {}, status_responses));

    // Register actual HTTP handlers
    webserver.on(getPath("setServoAngle").c_str(), HTTP_POST,
                 [this]() { this->handle_set_servo_angle(); });

    webserver.on(getPath("setMotorSpeed").c_str(), HTTP_POST,
                 [this]() { this->handle_set_motor_speed(); });

    webserver.on(getPath("getStatus").c_str(), HTTP_GET,
                 [this]() { this->handle_get_status(); });

    registerSettingsRoutes("DFR1216", this);

    logger->info(getServiceName() + " routes registered");
    return true;
}

void DFR1216Service::handle_set_servo_angle()
{
    if (!initialized) {
        webserver.send(503, RoutesConsts::mime_json, 
                      create_error_json(FPSTR(RoutesConsts::resp_not_initialized)));
        return;
    }

    if (!webserver.hasArg("channel") || 
        !webserver.hasArg("angle")) {
        webserver.send(422, RoutesConsts::mime_json, 
                      create_error_json(FPSTR(DFR1216Consts::msg_missing_servo_params)));
        return;
    }

    uint8_t channel = webserver.arg("channel").toInt();
    uint16_t angle = webserver.arg("angle").toInt();

    if (setServoAngle(channel, angle)) {
        JsonDocument doc;
        doc[FPSTR(RoutesConsts::result)] = FPSTR(RoutesConsts::result_ok);
        doc[FPSTR(DFR1216Consts::json_channel)] = channel;
        doc[FPSTR(DFR1216Consts::json_angle)] = angle;

        std::string response;
        serializeJson(doc, response);
        webserver.send(200, RoutesConsts::mime_json, response.c_str());
    } else {
        webserver.send(456, RoutesConsts::mime_json, 
                      create_error_json(FPSTR(RoutesConsts::resp_operation_failed)));
    }
}

void DFR1216Service::handle_set_motor_speed()
{
    if (!initialized) {
        webserver.send(503, RoutesConsts::mime_json, 
                      create_error_json(FPSTR(RoutesConsts::resp_not_initialized)));
        return;
    }

    if (!webserver.hasArg("motor") || 
        !webserver.hasArg("speed")) {
        webserver.send(422, RoutesConsts::mime_json, 
                      create_error_json(FPSTR(DFR1216Consts::msg_missing_motor_params)));
        return;
    }

    uint8_t motor = webserver.arg("motor").toInt();
    int8_t speed = webserver.arg("speed").toInt();

    if (setMotorSpeed(motor, speed)) {
        JsonDocument doc;
        doc[FPSTR(RoutesConsts::result)] = FPSTR(RoutesConsts::result_ok);
        doc[FPSTR(DFR1216Consts::json_motor)] = motor;
        doc[FPSTR(DFR1216Consts::json_speed)] = speed;

        std::string response;
        serializeJson(doc, response);
        webserver.send(200, RoutesConsts::mime_json, response.c_str());
    } else {
        webserver.send(456, RoutesConsts::mime_json, 
                      create_error_json(FPSTR(RoutesConsts::resp_operation_failed)));
    }
}

void DFR1216Service::handle_get_status()
{
    JsonDocument doc;
    doc[FPSTR(RoutesConsts::message)] = getServiceName();

    const char *status_str = "unknown";
    switch (service_status_) {
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
    doc[FPSTR(RoutesConsts::field_status)] = status_str;

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
