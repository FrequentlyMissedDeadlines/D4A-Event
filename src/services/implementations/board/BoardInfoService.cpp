/**
 * @file BoardInfoService.cpp
 * @brief Implementation for board information service
 * @details Exposed routes:
 *          - GET /api/board/v1/ - Retrieve board information including system metrics, memory usage, chip details, and firmware version
 *          - GET /api/board/v1/leds - Get RGB LED status
 *          - POST /api/board/v1/leds/set - Set RGB LED color
 *          - POST /api/board/v1/leds/off - Turn off RGB LED
 * 
 */

#include "services/BoardInfoService.h"
#include <ESPAsyncWebServer.h>
#include <unihiker_k10.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ArduinoJson.h>
#include "services/UDPService.h"

// Forward declarations
extern UNIHIKER_K10 unihiker;
extern UDPService udp_service;

// BoardInfoService constants namespace
namespace BoardInfoConsts
{
    constexpr const char path_service[] PROGMEM = "board/v1";
    constexpr const char str_board_name[] PROGMEM = "UNIHIKER_K10";
    constexpr const char str_firmware_version[] PROGMEM = "1.0.0";
    constexpr const char str_service_name[] PROGMEM = "Board info";
    constexpr const char str_led[] PROGMEM = "led";
    constexpr const char str_red[] PROGMEM = "red";
    constexpr const char str_green[] PROGMEM = "green";
    constexpr const char str_blue[] PROGMEM = "blue";
    constexpr const char str_leds[] PROGMEM = "leds";

    // UDP binary protocol constants
    constexpr uint8_t udp_service_id = 0x01;  ///< Unique ID for BoardInfo Service (high nibble of action byte)
    constexpr uint8_t udp_action_set_led_color = (udp_service_id << 4) | 0x01;  ///< [led:1B][r:1B][g:1B][b:1B]
    constexpr uint8_t udp_action_turn_off_led = (udp_service_id << 4) | 0x02;  ///< [led:1B]
    constexpr uint8_t udp_action_turn_off_all_leds = (udp_service_id << 4) | 0x03;  ///< (no params)
    constexpr uint8_t udp_action_get_led_status = (udp_service_id << 4) | 0x04;  ///< (no params) → [action][ok][JSON]
    constexpr uint8_t udp_action_max = (udp_service_id << 4) | 0x04;  ///< highest valid action code
}



bool BoardInfoService::registerRoutes()
{

  static constexpr char schema_json[] PROGMEM = R"({"type":"object","properties":{"uptimeMs":{"type":"integer","description":"System uptime in milliseconds"},"board":{"type":"string","description":"Board model name"},"version":{"type":"string","description":"Firmware version"},"heapTotal":{"type":"integer","description":"Total heap size in bytes"},"heapFree":{"type":"integer","description":"Free heap size in bytes"},"freeStackBytes":{"type":"integer","description":"Free stack space for current task"},"chipCores":{"type":"integer","description":"Number of CPU cores"},"chipModel":{"type":"string","description":"Chip model name"},"chipRevision":{"type":"integer","description":"Chip revision number"},"cpuFreqMHz":{"type":"integer","description":"CPU frequency in MHz"},"freeSketchSpace":{"type":"integer","description":"Available flash space for sketch"},"sdkVersion":{"type":"string","description":"ESP-IDF SDK version"}}})";
  static constexpr char example_json[] PROGMEM = R"({"uptimeMs":123456,"board":"UNIHIKER_K10","version":"1.0.0","heapTotal":327680,"heapFree":280000,"freeStackBytes":2048,"chipCores":2,"chipModel":"ESP32-S3","chipRevision":1,"cpuFreqMHz":240,"freeSketchSpace":1310720,"sdkVersion":"v4.4.2"})";
  static constexpr char route_desc[] PROGMEM = "Retrieves comprehensive board information including system metrics, memory usage, chip details, and firmware version";
  static constexpr char response_desc[] PROGMEM = "Board information retrieved successfully";

  std::string path = getPath("");
#ifdef VERBOSE_DEBUG
  logger->debug("Registering " + path);
#endif

  std::vector<OpenAPIResponse> responses;
  OpenAPIResponse successResponse(200, response_desc);
  successResponse.schema = schema_json;
  successResponse.example = example_json;
  responses.push_back(successResponse);
  responses.push_back(createServiceNotStartedResponse());

  OpenAPIRoute route(path.c_str(), RoutesConsts::method_get, route_desc, "Board Info", false, {}, responses);
  registerOpenAPIRoute(route);
  
  webserver.on(path.c_str(), HTTP_GET, [this](AsyncWebServerRequest *request)
               {
               if (!checkServiceStarted(request)) return;
               
               // Gather ESP32 metrics
               JsonDocument doc;
               doc["uptimeMs"] = millis ();
               doc["board"] = FPSTR(BoardInfoConsts::str_board_name);
               doc["version"] = FPSTR(BoardInfoConsts::str_firmware_version);
               doc["heapTotal"] = ESP.getHeapSize ();
               doc["heapFree"] = ESP.getFreeHeap ();
               doc["uptimeMs"] = millis ();
               doc["freeStackBytes"] = uxTaskGetStackHighWaterMark (NULL);
               doc["chipCores"] = ESP.getChipCores ();
               doc["chipModel"] = ESP.getChipModel ();
               doc["chipRevision"] = ESP.getChipRevision ();
               doc["cpuFreqMHz"] = ESP.getCpuFreqMHz ();
               doc["freeSketchSpace"] = ESP.getFreeSketchSpace ();
               doc["sdkVersion"] = ESP.getSdkVersion ();

               String output;
               serializeJson (doc, output);
               request->send (200, RoutesConsts::mime_json, output.c_str ()); });

  // RGB LED control routes
  std::string path_leds = getPath("leds");
  std::string path_leds_set = getPath("leds/set");
  std::string path_leds_off = getPath("leds/off");

  // GET /api/board/v1/leds - Get RGB LED status
  std::vector<OpenAPIResponse> leds_get_responses;
  OpenAPIResponse leds_get_response(200, "RGB LED status retrieved successfully");
  leds_get_response.schema = R"({"type":"object","properties":{"leds":{"type":"array","items":{"type":"object","properties":{"led":{"type":"integer"},"red":{"type":"integer"},"green":{"type":"integer"},"blue":{"type":"integer"}}}}}})";
  leds_get_responses.push_back(leds_get_response);
  leds_get_responses.push_back(createServiceNotStartedResponse());

  registerOpenAPIRoute(
      OpenAPIRoute(path_leds.c_str(), RoutesConsts::method_get, 
                   "Get status of all RGB LEDs", "Board LEDs", false, {}, leds_get_responses));

  webserver.on(path_leds.c_str(), HTTP_GET,
               [this](AsyncWebServerRequest *request) { this->handle_get_rgb_leds(request); });

  // POST /api/board/v1/leds/set - Set RGB LED color
  std::vector<OpenAPIParameter> led_set_params;
  led_set_params.push_back(OpenAPIParameter("led", RoutesConsts::type_integer, RoutesConsts::in_query, "LED index (0-2)", true));
  led_set_params.push_back(OpenAPIParameter("red", RoutesConsts::type_integer, RoutesConsts::in_query, "Red value (0-255)", true));
  led_set_params.push_back(OpenAPIParameter("green", RoutesConsts::type_integer, RoutesConsts::in_query, "Green value (0-255)", true));
  led_set_params.push_back(OpenAPIParameter("blue", RoutesConsts::type_integer, RoutesConsts::in_query, "Blue value (0-255)", true));

  std::vector<OpenAPIResponse> led_set_responses;
  led_set_responses.push_back(OpenAPIResponse(200, "RGB LED color set successfully"));
  led_set_responses.push_back(createServiceNotStartedResponse());

  registerOpenAPIRoute(
      OpenAPIRoute(path_leds_set.c_str(), RoutesConsts::method_post,
                   "Set color of a specific RGB LED", "Board LEDs", false, led_set_params, led_set_responses));

  webserver.on(path_leds_set.c_str(), HTTP_POST,
               [this](AsyncWebServerRequest *request) { this->handle_set_rgb_led(request); });

  // POST /api/board/v1/leds/off - Turn off RGB LED
  std::vector<OpenAPIParameter> led_off_params;
  led_off_params.push_back(OpenAPIParameter("led", RoutesConsts::type_integer, RoutesConsts::in_query, "LED index (0-2)", true));

  std::vector<OpenAPIResponse> led_off_responses;
  led_off_responses.push_back(OpenAPIResponse(200, "RGB LED turned off successfully"));
  led_off_responses.push_back(createServiceNotStartedResponse());

  registerOpenAPIRoute(
      OpenAPIRoute(path_leds_off.c_str(), RoutesConsts::method_post,
                   "Turn off a specific RGB LED", "Board LEDs", false, led_off_params, led_off_responses));

  webserver.on(path_leds_off.c_str(), HTTP_POST,
               [this](AsyncWebServerRequest *request) { this->handle_turn_off_rgb_led(request); });

registerServiceStatusRoute( this);
  registerSettingsRoutes( this);

  return true;
  // Add board-related routes here
}

std::string BoardInfoService::getServiceName()
{
  return progmem_to_string(BoardInfoConsts::str_service_name);
}
std::string BoardInfoService::getServiceSubPath()
{
    return progmem_to_string(BoardInfoConsts::path_service);
}

bool BoardInfoService::setRGBLED(uint8_t led_index, uint8_t red, uint8_t green, uint8_t blue)
{
    if (led_index > 2)
    {
        if (logger) logger->error("Invalid LED index: " + std::to_string(led_index));
        return false;
    }

    try
    {
        unihiker.rgb->write(led_index, red, green, blue);
        rgb_led_states_[led_index] = {red, green, blue};
        
        char log_buf[64];
        snprintf(log_buf, sizeof(log_buf), "Set RGB LED %u to (%u,%u,%u)", led_index, red, green, blue);
        if (logger) logger->info(log_buf);
        return true;
    }
    catch (const std::exception& e)
    {
        if (logger) logger->error("Error setting RGB LED: " + std::string(e.what()));
        return false;
    }
}

bool BoardInfoService::turnOffRGBLED(uint8_t led_index)
{
    return setRGBLED(led_index, 0, 0, 0);
}

bool BoardInfoService::turnOffAllRGBLEDs()
{
    bool success = true;
    for (uint8_t i = 0; i < 3; i++)
    {
        if (!turnOffRGBLED(i))
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

bool BoardInfoService::messageHandler(const std::string &message,
                                       const IPAddress &remoteIP,
                                       uint16_t remotePort)
{
    const size_t len = message.size();
    if (len < 1) return false;

    const uint8_t *d      = reinterpret_cast<const uint8_t *>(message.data());
    const uint8_t  action = d[0];

    if (action < 0x11 || action > BoardInfoConsts::udp_action_max) return false;

    static std::string resp;
    resp.clear();

    if (!IsServiceInterface::isServiceStarted())
    {
        udp_build_response(action, UDPProto::udp_resp_not_started, nullptr, resp);
        udp_service.sendReply(resp, remoteIP, remotePort);
        return true;
    }

    switch (action)
    {
    // 0x11 SET_LED_COLOR [led:1B][r:1B][g:1B][b:1B]
    case BoardInfoConsts::udp_action_set_led_color:
    {
        if (len < 5)
        {
            udp_build_response(action, UDPProto::udp_resp_invalid_params, nullptr, resp);
            break;
        }
        const uint8_t led_index = d[1];
        const uint8_t red       = d[2];
        const uint8_t green     = d[3];
        const uint8_t blue      = d[4];

        if (led_index > 2)
        {
            udp_build_response(action, UDPProto::udp_resp_invalid_values, nullptr, resp);
            break;
        }

        if (setRGBLED(led_index, red, green, blue))
        {
            udp_build_response(action, UDPProto::udp_resp_ok, nullptr, resp);
        }
        else
        {
            udp_build_response(action, UDPProto::udp_resp_operation_failed, nullptr, resp);
        }
        break;
    }

    // 0x12 TURN_OFF_LED [led:1B]
    case BoardInfoConsts::udp_action_turn_off_led:
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

        if (turnOffRGBLED(led_index))
        {
            udp_build_response(action, UDPProto::udp_resp_ok, nullptr, resp);
        }
        else
        {
            udp_build_response(action, UDPProto::udp_resp_operation_failed, nullptr, resp);
        }
        break;
    }

    // 0x13 TURN_OFF_ALL_LEDS (no params)
    case BoardInfoConsts::udp_action_turn_off_all_leds:
    {
        if (turnOffAllRGBLEDs())
        {
            udp_build_response(action, UDPProto::udp_resp_ok, nullptr, resp);
        }
        else
        {
            udp_build_response(action, UDPProto::udp_resp_operation_failed, nullptr, resp);
        }
        break;
    }

    // 0x14 GET_LED_STATUS (no params) → [action][ok][JSON]
    case BoardInfoConsts::udp_action_get_led_status:
    {
        JsonDocument doc;
        JsonArray leds = doc[FPSTR(BoardInfoConsts::str_leds)].to<JsonArray>();
        
        for (uint8_t i = 0; i < 3; i++)
        {
            JsonObject led = leds.add<JsonObject>();
            led[FPSTR(BoardInfoConsts::str_led)] = i;
            led[FPSTR(BoardInfoConsts::str_red)] = rgb_led_states_[i].red;
            led[FPSTR(BoardInfoConsts::str_green)] = rgb_led_states_[i].green;
            led[FPSTR(BoardInfoConsts::str_blue)] = rgb_led_states_[i].blue;
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

void BoardInfoService::handle_set_rgb_led(AsyncWebServerRequest *request)
{
    if (!checkServiceStarted(request)) return;
    
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

    if (setRGBLED(led_id, red, green, blue))
    {
        JsonDocument doc;
        doc["status"] = "success";
        doc["led"] = led_id;
        doc[FPSTR(BoardInfoConsts::str_red)] = red;
        doc[FPSTR(BoardInfoConsts::str_green)] = green;
        doc[FPSTR(BoardInfoConsts::str_blue)] = blue;
        
        String response;
        serializeJson(doc, response);
        request->send(200, RoutesConsts::mime_json, response.c_str());
    }
    else
    {
        request->send(500, RoutesConsts::mime_json, R"({"error":"Failed to set RGB LED"})");
    }
}

void BoardInfoService::handle_get_rgb_leds(AsyncWebServerRequest *request)
{
    if (!checkServiceStarted(request)) return;
    
    JsonDocument doc;
    JsonArray leds = doc[FPSTR(BoardInfoConsts::str_leds)].to<JsonArray>();
    
    for (uint8_t i = 0; i < 3; i++)
    {
        JsonObject led = leds.add<JsonObject>();
        led[FPSTR(BoardInfoConsts::str_led)] = i;
        led[FPSTR(BoardInfoConsts::str_red)] = rgb_led_states_[i].red;
        led[FPSTR(BoardInfoConsts::str_green)] = rgb_led_states_[i].green;
        led[FPSTR(BoardInfoConsts::str_blue)] = rgb_led_states_[i].blue;
    }
    
    String response;
    serializeJson(doc, response);
    request->send(200, RoutesConsts::mime_json, response.c_str());
}

void BoardInfoService::handle_turn_off_rgb_led(AsyncWebServerRequest *request)
{
    if (!checkServiceStarted(request)) return;
    
    if (!request->hasParam("led"))
    {
        request->send(400, RoutesConsts::mime_json, R"({"error":"Missing required parameter: led"})");
        return;
    }

    uint8_t led_id = request->getParam("led")->value().toInt();

    if (turnOffRGBLED(led_id))
    {
        JsonDocument doc;
        doc["status"] = "success";
        doc["message"] = "RGB LED turned off";
        doc[FPSTR(BoardInfoConsts::str_led)] = led_id;
        
        String response;
        serializeJson(doc, response);
        request->send(200, RoutesConsts::mime_json, response.c_str());
    }
    else
    {
        request->send(500, RoutesConsts::mime_json, R"({"error":"Failed to turn off RGB LED"})");
    }
}