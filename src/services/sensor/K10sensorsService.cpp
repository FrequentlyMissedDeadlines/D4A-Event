#include "../sensor/K10sensorsService.h"
#include "../ResponseHelper.h"
#include <WebServer.h>
#include <inttypes.h>
#include <cstdio>
#include <cstdint>

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
}

DFRobot_AHT20 aht20_sensor;
extern UNIHIKER_K10 unihiker;


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
      logger->error(fpstr_to_string(FPSTR(K10SensorsConsts::msg_aht20_init_failed)) + std::to_string(initResult));
  }
  else if (!sensorReady())
  {
    if (logger)
      logger->warning(fpstr_to_string(FPSTR(K10SensorsConsts::msg_aht20_not_ready)));
  }
  else
  {
    if (logger)
      logger->info(fpstr_to_string(FPSTR(K10SensorsConsts::msg_aht20_init_success)));
  }

  // Single route handler with all logic inside
  webserver.on(path.c_str(), HTTP_GET, [this, initResult]()
  {
    if (!checkServiceStarted()) return;
    
    // Check if AHT20 initialization failed
    if (initResult != 0)
    {
      ResponseHelper::sendError(ResponseHelper::SERVICE_UNAVAILABLE,
        fpstr_to_string(FPSTR(K10SensorsConsts::msg_failed_init_aht20)).c_str());
      return;
    }
    
    // Check if sensor is ready
    if (!sensorReady())
    {
      ResponseHelper::sendError(ResponseHelper::SERVICE_UNAVAILABLE,
        fpstr_to_string(FPSTR(K10SensorsConsts::msg_aht20_not_ready_init)).c_str());
      return;
    }
    
    // Sensor is ready, get and return data
    try
    {
      std::string json = this->getSensorJson();
      webserver.send(200, RoutesConsts::mime_json, json.c_str());
    }
    catch (...)
    {
      ResponseHelper::sendError(ResponseHelper::SERVICE_UNAVAILABLE,
        fpstr_to_string(FPSTR(K10SensorsConsts::msg_get_sensor_failed)).c_str());
    }
  });

  registerSettingsRoutes(fpstr_to_string(FPSTR(K10SensorsConsts::settings_domain_sensors)).c_str(), this);
  return true;
}

std::string K10SensorsService::getServiceName()
{
  return fpstr_to_string(FPSTR(K10SensorsConsts::str_service_name));
}
std::string K10SensorsService::getServiceSubPath()
{
    return fpstr_to_string(FPSTR(K10SensorsConsts::path_service));
}
std::string K10SensorsService::getPath(const std::string& finalpathstring)
{
  if (baseServicePath.empty()) {
    baseServicePath = std::string(RoutesConsts::path_api) + getServiceSubPath() + "/";
  }
  return baseServicePath + finalpathstring;
}
