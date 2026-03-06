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

#include "services/DFR1216Service.h"
#include "FlashStringHelper.h"
#include "ResponseHelper.h"
#include <ESPAsyncWebServer.h>
#include <pgmspace.h>
#include <ArduinoJson.h>
#include "IsOpenAPIInterface.h"
#include "services/UDPService.h"
#include "services/AmakerBotService.h"

extern UDPService udp_service;
extern AmakerBotService amakerbot_service;

// DFR1216-specific OpenAPI constants namespace (stored in PROGMEM to save RAM)
namespace DFR1216Consts
{
    // Route paths and actions
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

    // UDP binary protocol constants
    constexpr uint8_t udp_service_id = 0x03;  ///< Unique ID for DFR1216 Service (high nibble of action byte)
    constexpr uint8_t udp_action_set_led_color = (udp_service_id << 4) | 0x01;  ///< [led:1B][r:1B][g:1B][b:1B][brightness:1B]
    constexpr uint8_t udp_action_turn_off_led = (udp_service_id << 4) | 0x02;  ///< [led:1B]
    constexpr uint8_t udp_action_turn_off_all_leds = (udp_service_id << 4) | 0x03;  ///< (no params)
    constexpr uint8_t udp_action_get_led_status = (udp_service_id << 4) | 0x04;  ///< (no params) → [action][ok][JSON]
    constexpr uint8_t udp_action_max = (udp_service_id << 4) | 0x04;  ///< highest valid action code
}


bool DFR1216Service::initializeService()
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

bool DFR1216Service::startService()
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

bool DFR1216Service::stopService()
{
    setServiceStatus(STOPPED);
    logger->info(getServiceName() + " " + getStatusString());
    return true;
}

bool DFR1216Service::setServoAngle(uint8_t channel, uint16_t angle)
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

bool DFR1216Service::setMotorSpeed(uint8_t motor, int8_t speed)
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
    servo_responses.push_back(createServiceNotStartedResponse());
    servo_responses.push_back(createForbiddenResponse());

    OpenAPIRoute servo_route(getPath("setServoAngle").c_str(),
                             RoutesConsts::method_post,
                             progmem_to_string(DFR1216Consts::desc_servo_control).c_str(),
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
    motor_responses.push_back(createServiceNotStartedResponse());
    motor_responses.push_back(createForbiddenResponse());

    OpenAPIRoute motor_route(getPath("setMotorSpeed").c_str(),
                             RoutesConsts::method_post,
                             progmem_to_string(DFR1216Consts::desc_motor_control).c_str(),
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
    status_responses.push_back(createServiceNotStartedResponse());

    registerOpenAPIRoute(
        OpenAPIRoute(getPath("getStatus").c_str(),
                     RoutesConsts::method_get,
                     progmem_to_string(DFR1216Consts::desc_get_status).c_str(),
                     FPSTR(DFR1216Consts::tag_dfr1216), false, {}, status_responses));

    // Register actual HTTP handlers
    webserver.on(getPath("setServoAngle").c_str(), HTTP_POST,
                 [this](AsyncWebServerRequest *request)
                 { this->handle_set_servo_angle(request); });

    webserver.on(getPath("setMotorSpeed").c_str(), HTTP_POST,
                 [this](AsyncWebServerRequest *request)
                 { this->handle_set_motor_speed(request); });

    webserver.on(getPath("getStatus").c_str(), HTTP_GET,
                 [this](AsyncWebServerRequest *request)
                 { this->handle_get_status(request); });

    // LED control routes
    std::vector<OpenAPIParameter> led_color_params;
    led_color_params.push_back(OpenAPIParameter("led", RoutesConsts::type_integer, RoutesConsts::in_query, "LED index (0-2)", true));
    led_color_params.push_back(OpenAPIParameter("red", RoutesConsts::type_integer, RoutesConsts::in_query, "Red value (0-255)", true));
    led_color_params.push_back(OpenAPIParameter("green", RoutesConsts::type_integer, RoutesConsts::in_query, "Green value (0-255)", true));
    led_color_params.push_back(OpenAPIParameter("blue", RoutesConsts::type_integer, RoutesConsts::in_query, "Blue value (0-255)", true));

    std::vector<OpenAPIResponse> led_set_responses;
    led_set_responses.push_back(OpenAPIResponse(200, "LED color set successfully"));
    led_set_responses.push_back(createServiceNotStartedResponse());
    led_set_responses.push_back(createForbiddenResponse());

    registerOpenAPIRoute(
        OpenAPIRoute(getPath("setLEDColor").c_str(),
                     RoutesConsts::method_post,
                     "Set LED color via WS2812 LED",
                     FPSTR(DFR1216Consts::tag_dfr1216), false, led_color_params, led_set_responses));

    webserver.on(getPath("setLEDColor").c_str(), HTTP_POST,
                 [this](AsyncWebServerRequest *request)
                 { this->handle_set_led_color(request); });

    // Turn off LED route
    std::vector<OpenAPIParameter> led_off_params;
    led_off_params.push_back(OpenAPIParameter("led", RoutesConsts::type_integer, RoutesConsts::in_query, "LED index (0-2)", true));

    std::vector<OpenAPIResponse> led_off_responses;
    led_off_responses.push_back(OpenAPIResponse(200, "LED turned off successfully"));
    led_off_responses.push_back(createServiceNotStartedResponse());
    led_off_responses.push_back(createForbiddenResponse());

    registerOpenAPIRoute(
        OpenAPIRoute(getPath("turnOffLED").c_str(),
                     RoutesConsts::method_post,
                     "Turn off a specific LED",
                     FPSTR(DFR1216Consts::tag_dfr1216), false, led_off_params, led_off_responses));

    webserver.on(getPath("turnOffLED").c_str(), HTTP_POST,
                 [this](AsyncWebServerRequest *request)
                 { this->handle_turn_off_led(request); });

    // Get LED status route
    std::vector<OpenAPIResponse> led_status_responses;
    led_status_responses.push_back(OpenAPIResponse(200, "LED status retrieved successfully"));
    led_status_responses.push_back(createServiceNotStartedResponse());

    registerOpenAPIRoute(
        OpenAPIRoute(getPath("getLEDStatus").c_str(),
                     RoutesConsts::method_get,
                     "Get status of all LEDs",
                     FPSTR(DFR1216Consts::tag_dfr1216), false, {}, led_status_responses));

    webserver.on(getPath("getLEDStatus").c_str(), HTTP_GET,
                 [this](AsyncWebServerRequest *request)
                 { this->handle_get_led_status(request); });

registerServiceStatusRoute( this);
  registerSettingsRoutes( this);

    logger->info(getServiceName() + " routes registered");
    return true;
}

void DFR1216Service::handle_set_servo_angle(AsyncWebServerRequest *request)
{
    if (!checkServiceStarted(request))
        return;

    // Check if request is from the registered master
            if (!checkServiceStarted(request) ||  (!checkIsRequestFromMaster(request, &amakerbot_service))) return;


    // Validate parameters
    if (!request->hasArg("channel") || !request->hasArg("angle")) {
        ResponseHelper::sendError(request, ResponseHelper::INVALID_PARAMS, 
                                 FPSTR(DFR1216Consts::msg_missing_servo_params));
        return;
    }

    uint8_t channel = std::atoi(request->arg("channel").c_str());
    uint16_t angle = std::atoi(request->arg("angle").c_str());

    if (setServoAngle(channel, angle))
    {
        JsonDocument doc;
        doc[FPSTR(RoutesConsts::result)] = FPSTR(RoutesConsts::result_ok);
        doc[FPSTR(DFR1216Consts::json_channel)] = channel;
        doc[FPSTR(DFR1216Consts::json_angle)] = angle;
        ResponseHelper::sendJsonResponse(request, 200, doc);
    }
    else
    {
        ResponseHelper::sendError(request, ResponseHelper::OPERATION_FAILED, 
                                 FPSTR(RoutesConsts::resp_operation_failed));
    }
}

void DFR1216Service::handle_set_motor_speed(AsyncWebServerRequest *request)
{
            if (!checkServiceStarted(request) ||  (!checkIsRequestFromMaster(request, &amakerbot_service))) return;

        return;

    // Validate parameters
    if (!request->hasArg("motor") || !request->hasArg("speed")) {
        ResponseHelper::sendError(request, ResponseHelper::INVALID_PARAMS, 
                                 FPSTR(DFR1216Consts::msg_missing_motor_params));
        return;
    }

    uint8_t motor = std::atoi(request->arg("motor").c_str());
    int8_t speed = std::atoi(request->arg("speed").c_str());

    if (setMotorSpeed(motor, speed))
    {
        JsonDocument doc;
        doc[FPSTR(RoutesConsts::result)] = FPSTR(RoutesConsts::result_ok);
        doc[FPSTR(DFR1216Consts::json_motor)] = motor;
        doc[FPSTR(DFR1216Consts::json_speed)] = speed;
        ResponseHelper::sendJsonResponse(request, 200, doc);
    }
    else
    {
        ResponseHelper::sendError(request, ResponseHelper::OPERATION_FAILED, 
                                 FPSTR(RoutesConsts::resp_operation_failed));
    }
}

void DFR1216Service::handle_get_status(AsyncWebServerRequest *request)
{
    if (!checkServiceStarted(request))
        return;
    JsonDocument doc;
    doc[FPSTR(RoutesConsts::message)] = getServiceName();

    const char *status_str = "unknown";
    switch (service_status_)
    {
    case INITIALIZED_FAILED:
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
    request->send(200, RoutesConsts::mime_json, response.c_str());
}

bool DFR1216Service::setLEDColor(uint8_t led_index, uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness)
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

bool DFR1216Service::turnOffLED(uint8_t led_index)
{
    return setLEDColor(led_index, 0, 0, 0, 0);
}

bool DFR1216Service::turnOffAllLEDs()
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

void DFR1216Service::handle_set_led_color(AsyncWebServerRequest *request)
{
    if (!checkServiceStarted(request) || !checkIsRequestFromMaster(request, &amakerbot_service)) return;
    
    if (!request->hasParam("led") || !request->hasParam("red") || 
        !request->hasParam("green") || !request->hasParam("blue"))
    {
        request->send(400, RoutesConsts::mime_json, R"({"error":"Missing required parameters: led, red, green, blue"})");
        return;
    }

    uint8_t led_id = request->getParam("led")->value().toInt();
    uint8_t red = request->getParam("red")->value().toInt();
    uint8_t green = request->getParam("green")->value().toInt();
    uint8_t blue = request->getParam("blue")->value().toInt();

    if (setLEDColor(led_id, red, green, blue))
    {
        JsonDocument doc;
        doc["status"] = "success";
        doc["led"] = led_id;
        doc["red"] = red;
        doc["green"] = green;
        doc["blue"] = blue;
        
        String response;
        serializeJson(doc, response);
        request->send(200, RoutesConsts::mime_json, response.c_str());
    }
    else
    {
        request->send(500, RoutesConsts::mime_json, R"({"error":"Failed to set LED color"})");
    }
}

void DFR1216Service::handle_turn_off_led(AsyncWebServerRequest *request)
{
    if (!checkServiceStarted(request) || !checkIsRequestFromMaster(request, &amakerbot_service)) return;
    
    if (!request->hasParam("led"))
    {
        request->send(400, RoutesConsts::mime_json, R"({"error":"Missing required parameter: led"})");
        return;
    }

    uint8_t led_id = request->getParam("led")->value().toInt();

    if (turnOffLED(led_id))
    {
        JsonDocument doc;
        doc["status"] = "success";
        doc["message"] = "LED turned off";
        doc["led"] = led_id;
        
        String response;
        serializeJson(doc, response);
        request->send(200, RoutesConsts::mime_json, response.c_str());
    }
    else
    {
        request->send(500, RoutesConsts::mime_json, R"({"error":"Failed to turn off LED"})");
    }
}

void DFR1216Service::handle_get_led_status(AsyncWebServerRequest *request)
{
    if (!checkServiceStarted(request)) return;
    
    JsonDocument doc;
    JsonArray leds = doc["leds"].to<JsonArray>();
    
    for (uint8_t i = 0; i < 3; i++)
    {
        JsonObject led = leds.add<JsonObject>();
        led["id"] = i;
        led["red"] = led_states_[i].red;
        led["green"] = led_states_[i].green;
        led["blue"] = led_states_[i].blue;
    }
    
    String response;
    serializeJson(doc, response);
    request->send(200, RoutesConsts::mime_json, response.c_str());
}

// UDP helper function to build binary responses
static inline void udp_build_response(uint8_t action, uint8_t resp, const char *payload, std::string &out)
{
    out.clear();
    out += static_cast<char>(action);
    out += static_cast<char>(resp);
    if (payload && *payload) out.append(payload);
}

bool DFR1216Service::messageHandler(const std::string &message,
                                     const IPAddress &remoteIP,
                                     uint16_t remotePort)
{
    const size_t len = message.size();
    if (len < 1) return false;

    const uint8_t *d      = reinterpret_cast<const uint8_t *>(message.data());
    const uint8_t  action = d[0];

    if (action < 0x31 || action > DFR1216Consts::udp_action_max) return false;

    static std::string resp;
    resp.clear();

    if (!IsServiceInterface::isServiceStarted())
    {
        udp_build_response(action, UDPProto::udp_resp_not_started, nullptr, resp);
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
    // 0x31 SET_LED_COLOR [led:1B][r:1B][g:1B][b:1B][brightness:1B]
    case DFR1216Consts::udp_action_set_led_color:
    {
        if (len < 6)
        {
            udp_build_response(action, UDPProto::udp_resp_invalid_params, nullptr, resp);
            break;
        }
        const uint8_t led_index  = d[1];
        const uint8_t red        = d[2];
        const uint8_t green      = d[3];
        const uint8_t blue       = d[4];
        const uint8_t brightness = d[5];

        if (led_index > 2)
        {
            udp_build_response(action, UDPProto::udp_resp_invalid_values, nullptr, resp);
            break;
        }

        if (setLEDColor(led_index, red, green, blue, brightness))
        {
            udp_build_response(action, UDPProto::udp_resp_ok, nullptr, resp);
        }
        else
        {
            udp_build_response(action, UDPProto::udp_resp_operation_failed, nullptr, resp);
        }
        break;
    }

    // 0x32 TURN_OFF_LED [led:1B]
    case DFR1216Consts::udp_action_turn_off_led:
    {
        if (len < 2)
        {
            udp_build_response(action, UDPProto::udp_resp_invalid_params, nullptr, resp);
            break;
        }
        const uint8_t led_index = d[1];

        if (led_index > 2)
        {
            udp_build_response(action, UDPProto::udp_resp_invalid_values, nullptr, resp);
            break;
        }

        if (turnOffLED(led_index))
        {
            udp_build_response(action, UDPProto::udp_resp_ok, nullptr, resp);
        }
        else
        {
            udp_build_response(action, UDPProto::udp_resp_operation_failed, nullptr, resp);
        }
        break;
    }

    // 0x33 TURN_OFF_ALL_LEDS (no params)
    case DFR1216Consts::udp_action_turn_off_all_leds:
    {
        if (turnOffAllLEDs())
        {
            udp_build_response(action, UDPProto::udp_resp_ok, nullptr, resp);
        }
        else
        {
            udp_build_response(action, UDPProto::udp_resp_operation_failed, nullptr, resp);
        }
        break;
    }

    // 0x34 GET_LED_STATUS (no params) → [action][ok][JSON]
    case DFR1216Consts::udp_action_get_led_status:
    {
        JsonDocument doc;
        JsonArray leds = doc["leds"].to<JsonArray>();
        
        for (uint8_t i = 0; i < 3; i++)
        {
            JsonObject led = leds.add<JsonObject>();
            led["id"] = i;
            led["red"] = led_states_[i].red;
            led["green"] = led_states_[i].green;
            led["blue"] = led_states_[i].blue;
        }
        
        std::string json_str;
        serializeJson(doc, json_str);
        
        resp.clear();
        resp += static_cast<char>(action);
        resp += static_cast<char>(UDPProto::udp_resp_ok);
        resp += json_str;
        
        udp_service.sendReply(resp, remoteIP, remotePort);
        return true;
    }

    default:
    {
        udp_build_response(action, UDPProto::udp_resp_unknown_cmd, nullptr, resp);
        break;
    }
    }

    udp_service.sendReply(resp, remoteIP, remotePort);
    return true;
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
