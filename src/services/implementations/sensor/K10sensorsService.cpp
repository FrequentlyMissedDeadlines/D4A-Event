#include "services/K10sensorsService.h"
#include "ResponseHelper.h"
#include <ESPAsyncWebServer.h>
#include <inttypes.h>
#include <cstdio>
#include <cstdint>
#include <ArduinoJson.h>
#include "services/UDPService.h"

// K10SensorsService constants namespace
namespace K10SensorsConsts
{
    constexpr const char json_accelerometer[] PROGMEM = "accelerometer";
    constexpr const char json_celcius[] PROGMEM = "celcius";
    constexpr const char json_hum_rel[] PROGMEM = "hum_rel";
    constexpr const char json_light[] PROGMEM = "light";
    constexpr const char json_mic_data[] PROGMEM = "mic_data";
    constexpr const char msg_aht20_init_failed[] PROGMEM = "AHT20 sensor init failed: ";
    constexpr const char msg_aht20_init_success[] PROGMEM = "AHT20 sensor initialized successfully";
    constexpr const char msg_aht20_not_ready[] PROGMEM = "AHT20 sensor not ready yet";
    constexpr const char msg_aht20_not_ready_init[] PROGMEM = "AHT20 sensor measurement not ready during initialization";
    constexpr const char msg_failed_init_aht20[] PROGMEM = "Failed to initialize AHT20 sensor";
    constexpr const char msg_get_sensor_failed[] PROGMEM = "getSensorJson() failed";
    constexpr const char path_service[] PROGMEM = "sensors/v1";
    constexpr const char settings_domain_sensors[] PROGMEM = "Sensors";
    constexpr const char str_service_name[] PROGMEM = "K10 Sensors Service";
    constexpr const char tag_sensors[] PROGMEM = "Sensors";

    // UDP binary protocol (request: [action:1B], response: [action:1B][resp:1B][payload])
    // Action byte layout: [service_id:4bits][base_action:4bits]
    // Response codes are shared — see UDPProto namespace in isUDPMessageHandlerInterface.h
    constexpr uint8_t udp_service_id         = 0x02; ///< Unique ID for this service (high nibble of action byte)
    constexpr uint8_t udp_action_get_sensors = (udp_service_id << 4) | 0x01; ///< → [action][ok][JSON sensors]
    constexpr uint8_t udp_action_min         = (udp_service_id << 4) | 0x01;
    constexpr uint8_t udp_action_max         = (udp_service_id << 4) | 0x01;
}

DFRobot_AHT20 aht20_sensor;
extern UNIHIKER_K10 unihiker;
extern UDPService   udp_service;


bool K10SensorsService::sensorReady()
{
  if (aht20_sensor.startMeasurementReady())
  {
    return true;
  }

  return false;
}

std::string K10SensorsService::getSensorJson()
{
  JsonDocument doc = JsonDocument();
  doc[FPSTR(K10SensorsConsts::json_light)] = unihiker.readALS();
  doc[FPSTR(K10SensorsConsts::json_hum_rel)] = aht20_sensor.getHumidity_RH();
  doc[FPSTR(K10SensorsConsts::json_celcius)] = aht20_sensor.getTemperature_C();
  doc[FPSTR(K10SensorsConsts::json_mic_data)] = unihiker.readMICData();
  JsonArray accel = doc[FPSTR(K10SensorsConsts::json_accelerometer)].to<JsonArray>();
  accel.add(unihiker.getAccelerometerX());
  accel.add(unihiker.getAccelerometerY());
  accel.add(unihiker.getAccelerometerZ());
  String output;
  serializeJson(doc, output);
  return std::string(output.c_str());
}

bool K10SensorsService::registerRoutes()
{
  static constexpr char schema_json[] PROGMEM = R"({"type":"object","properties":{"light":{"type":"number","description":"Ambient light sensor reading"},"hum_rel":{"type":"number","description":"Relative humidity percentage"},"celcius":{"type":"number","description":"Temperature in Celsius"},"mic_data":{"type":"number","description":"Microphone data reading"},"accelerometer":{"type":"array","description":"3-axis accelerometer data [x,y,z]","items":{"type":"number"}}}})";
  static constexpr char example_json[] PROGMEM = R"({"light":125.5,"hum_rel":45.2,"celcius":23.8,"mic_data":512,"accelerometer":[0.12,-0.05,9.81]})";
  static constexpr char route_desc[] PROGMEM = "Retrieves all K10 sensor readings including light, temperature, humidity, microphone, and accelerometer data";
  static constexpr char response_ok[] PROGMEM = "Sensor data retrieved successfully";
  static constexpr char response_err[] PROGMEM = "Sensor initialization or reading failed";

  std::string path = getPath("");
  
  logRouteRegistration(path);

  std::vector<OpenAPIResponse> responses;
  OpenAPIResponse successResponse = createSuccessResponse(response_ok);
  successResponse.schema = schema_json;
  successResponse.example = example_json;
  responses.push_back(successResponse);

  OpenAPIResponse errorResponse(503, response_err);
  errorResponse.schema = R"({"type":"object","properties":{"result":{"type":"string"},"message":{"type":"string"}}})";
  responses.push_back(errorResponse);
  responses.push_back(createServiceNotStartedResponse());

  OpenAPIRoute route(path.c_str(), RoutesConsts::method_get, route_desc, "Sensors", false, {}, responses);
  registerOpenAPIRoute(route);

  // Try to initialize AHT20 sensor
  int initResult = aht20_sensor.begin();
  if (initResult != 0)
  {
    if (logger)
      logger->error(progmem_to_string(K10SensorsConsts::msg_aht20_init_failed) + std::to_string(initResult));
  }
  else if (!sensorReady())
  {
    if (logger)
      logger->warning(progmem_to_string(K10SensorsConsts::msg_aht20_not_ready));
  }
  else
  {
    if (logger)
      logger->info(progmem_to_string(K10SensorsConsts::msg_aht20_init_success));
  }

  // Single route handler with all logic inside
  webserver.on(path.c_str(), HTTP_GET, [this, initResult](AsyncWebServerRequest *request)
  {
    if (!checkServiceStarted(request)) return;
    
    // Check if AHT20 initialization failed
    if (initResult != 0)
    {
      ResponseHelper::sendError(request, ResponseHelper::SERVICE_UNAVAILABLE,
        progmem_to_string(K10SensorsConsts::msg_failed_init_aht20).c_str());
      return;
    }
    
    // Check if sensor is ready
    if (!sensorReady())
    {
      ResponseHelper::sendError(request, ResponseHelper::SERVICE_UNAVAILABLE,
        progmem_to_string(K10SensorsConsts::msg_aht20_not_ready_init).c_str());
      return;
    }
    
    // Sensor is ready, get and return data
    try
    {
      std::string json = this->getSensorJson();
      request->send(200, RoutesConsts::mime_json, json.c_str());
    }
    catch (...)
    {
      ResponseHelper::sendError(request, ResponseHelper::SERVICE_UNAVAILABLE,
        progmem_to_string(K10SensorsConsts::msg_get_sensor_failed).c_str());
    }
  });
registerServiceStatusRoute( this);
  registerSettingsRoutes( this);
  return true;
}

std::string K10SensorsService::getServiceName()
{
  return progmem_to_string(K10SensorsConsts::str_service_name);
}
std::string K10SensorsService::getServiceSubPath()
{
    return progmem_to_string(K10SensorsConsts::path_service);
}
std::string K10SensorsService::getPath(const std::string& finalpathstring)
{
  if (baseServicePath.empty()) {
    baseServicePath = std::string(RoutesConsts::path_api) + getServiceSubPath() + "/";
  }
  return baseServicePath + finalpathstring;
}

// ─── UDP binary protocol ─────────────────────────────────────────────────────

static inline void udp_build(uint8_t action, uint8_t resp, const char *msg, std::string &out)
{
    out.clear();
    out += static_cast<char>(action);
    out += static_cast<char>(resp);
    if (msg && *msg) out.append(msg);
}
static inline void udp_build(uint8_t action, uint8_t resp, const std::string &msg, std::string &out)
{
    out.clear();
    out += static_cast<char>(action);
    out += static_cast<char>(resp);
    out += msg;
}

/**
 * @brief Handle an incoming binary UDP message for K10SensorsService.
 *
 * REQUEST  : [action:1B]
 *   0x01 GET_SENSORS  → [action][ok][JSON sensor payload]
 *
 * RESPONSE : [action:1B][resp_code:1B][optional_payload]
 *   0x00=ok  0x01=sensor_not_ready  0x04=not_started  0x05=unknown_cmd
 *
 * @return true if first byte is a recognised action code (message claimed)
 */
bool K10SensorsService::messageHandler(const std::string &message,
                                       const IPAddress &remoteIP,
                                       uint16_t remotePort)
{
    if (message.size() < 1) return false;

    const uint8_t action = static_cast<uint8_t>(message[0]);
    if (action < K10SensorsConsts::udp_action_min || action > K10SensorsConsts::udp_action_max) return false;

    static std::string resp;
    resp.clear();

    if (!isServiceStarted())
    {
        udp_build(action, UDPProto::udp_resp_not_started, nullptr, resp);
        udp_service.sendReply(resp, remoteIP, remotePort);
        return true;
    }

    switch (action)
    {
    // 0x01 GET_SENSORS → JSON payload
    case K10SensorsConsts::udp_action_get_sensors:
        if (!sensorReady())
            udp_build(action, UDPProto::udp_resp_operation_failed, nullptr, resp);
        else
            udp_build(action, UDPProto::udp_resp_ok, getSensorJson(), resp);
        break;
    default:
        udp_build(action, UDPProto::udp_resp_unknown_cmd, nullptr, resp);
        break;
    }

#ifdef VERBOSE_DEBUG
    if (resp.size() >= 2)
        logger->debug("UDP bin a=0x" + std::string(String(resp[0], HEX).c_str()) +
                      " r=0x"        + std::string(String(resp[1], HEX).c_str()) +
                      (resp.size() > 2 ? " +" + std::to_string(resp.size() - 2) + "B" : ""));
#endif

    if (!resp.empty())
        udp_service.sendReply(resp, remoteIP, remotePort);
    return true;
}
