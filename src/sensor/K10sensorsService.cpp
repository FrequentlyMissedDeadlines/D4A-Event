#include "K10sensorsService.h"
#include <WebServer.h>
#include <inttypes.h>
#include <cstdio>
#include <cstdint>

DFRobot_AHT20 aht20_sensor;
extern UNIHIKER_K10 unihiker;

bool K10SensorsService::initializeService()
{
  return true;
}
bool K10SensorsService::startService()
{
  return true;
}
bool K10SensorsService::stopService()
{
  return true;
}
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
  doc["light"] = unihiker.readALS();
  doc["hum_rel"] = aht20_sensor.getHumidity_RH();
  doc["celcius"] = aht20_sensor.getTemperature_C();
  doc["mic_data"] = unihiker.readMICData();
  JsonArray accel = doc["accelerometer"].to<JsonArray>();
  accel.add(unihiker.getAccelerometerX());
  accel.add(unihiker.getAccelerometerY());
  accel.add(unihiker.getAccelerometerZ());
  String output;
  serializeJson(doc, output);
  return std::string(output.c_str());
}

bool K10SensorsService::registerRoutes()
{
  static constexpr char kSchemaJson[] PROGMEM = R"({"type":"object","properties":{"light":{"type":"number","description":"Ambient light sensor reading"},"hum_rel":{"type":"number","description":"Relative humidity percentage"},"celcius":{"type":"number","description":"Temperature in Celsius"},"mic_data":{"type":"number","description":"Microphone data reading"},"accelerometer":{"type":"array","description":"3-axis accelerometer data [x,y,z]","items":{"type":"number"}}}})";
  static constexpr char kExampleJson[] PROGMEM = R"({"light":125.5,"hum_rel":45.2,"celcius":23.8,"mic_data":512,"accelerometer":[0.12,-0.05,9.81]})";
  static constexpr char kRouteDesc[] PROGMEM = "Retrieves all K10 sensor readings including light, temperature, humidity, microphone, and accelerometer data";
  static constexpr char kResponseOk[] PROGMEM = "Sensor data retrieved successfully";
  static constexpr char kResponseErr[] PROGMEM = "Sensor initialization or reading failed";

  std::string path = std::string(RoutesConsts::kPathAPI) + getName();
  
#ifdef DEBUG
  if (logger)
    logger->debug("Registering " + path);
#endif

  std::vector<OpenAPIResponse> responses;
  OpenAPIResponse successResponse(200, kResponseOk);
  successResponse.schema = kSchemaJson;
  successResponse.example = kExampleJson;
  responses.push_back(successResponse);

  OpenAPIResponse errorResponse(503, kResponseErr);
  errorResponse.schema = R"({"type":"object","properties":{"result":{"type":"string"},"message":{"type":"string"}}})";
  responses.push_back(errorResponse);

  OpenAPIRoute route(path.c_str(), "GET", kRouteDesc, "Sensors", false, {}, responses);
  registerOpenAPIRoute(route);

  // Try to initialize AHT20 sensor
  int initResult = aht20_sensor.begin();
  if (initResult != 0)
  {
    if (logger)
      logger->error("AHT20 sensor init failed: " + std::to_string(initResult));
    
    webserver.on(path.c_str(), HTTP_GET, [this]()
    {
      webserver.send(503, RoutesConsts::kMimeJSON, this->getResultJsonString(RoutesConsts::kResultErr, "Failed to initialize AHT20 sensor").c_str());
    });
    return true; // Return true to not block other services
  }

  if (!sensorReady())
  {
    if (logger)
      logger->warning("AHT20 sensor not ready yet");
      
    webserver.on(path.c_str(), HTTP_GET, [this]()
    {
      webserver.send(503, RoutesConsts::kMimeJSON, this->getResultJsonString(RoutesConsts::kResultErr, "AHT20 sensor measurement not ready during initialization").c_str());
    });
    return true; // Return true to not block other services
  }

  if (logger)
    logger->info("AHT20 sensor initialized successfully");
    
  webserver.on(path.c_str(), HTTP_GET, [this]()
  {
    try{
    std::string json = this->getSensorJson();
    webserver.send(200, RoutesConsts::kMimeJSON, json.c_str());
    } catch (...) {
      webserver.send(500, RoutesConsts::kMimeJSON, this->getResultJsonString(RoutesConsts::kResultErr, "getSensorJson() failed").c_str());
    }
  });

  return true;
}

std::string K10SensorsService::getName()
{
  return "sensors/v1";
}

std::string K10SensorsService::getPath(const std::string& finalpathstring)
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
