#include "BoardInfoService.h"
#include <WebServer.h>
#include <unihiker_k10.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ArduinoJson.h>

bool BoardInfoService::initializeService() { return true; }
bool BoardInfoService::startService() { return true; }
bool BoardInfoService::stopService() { return true; }

bool BoardInfoService::registerRoutes()
{


  static constexpr char kSchemaJson[] PROGMEM = R"({"type":"object","properties":{"uptimeMs":{"type":"integer","description":"System uptime in milliseconds"},"board":{"type":"string","description":"Board model name"},"version":{"type":"string","description":"Firmware version"},"heapTotal":{"type":"integer","description":"Total heap size in bytes"},"heapFree":{"type":"integer","description":"Free heap size in bytes"},"freeStackBytes":{"type":"integer","description":"Free stack space for current task"},"chipCores":{"type":"integer","description":"Number of CPU cores"},"chipModel":{"type":"string","description":"Chip model name"},"chipRevision":{"type":"integer","description":"Chip revision number"},"cpuFreqMHz":{"type":"integer","description":"CPU frequency in MHz"},"freeSketchSpace":{"type":"integer","description":"Available flash space for sketch"},"sdkVersion":{"type":"string","description":"ESP-IDF SDK version"}}})";
  static constexpr char kExampleJson[] PROGMEM = R"({"uptimeMs":123456,"board":"UNIHIKER_K10","version":"1.0.0","heapTotal":327680,"heapFree":280000,"freeStackBytes":2048,"chipCores":2,"chipModel":"ESP32-S3","chipRevision":1,"cpuFreqMHz":240,"freeSketchSpace":1310720,"sdkVersion":"v4.4.2"})";
  static constexpr char kRouteDesc[] PROGMEM = "Retrieves comprehensive board information including system metrics, memory usage, chip details, and firmware version";
  static constexpr char kResponseDesc[] PROGMEM = "Board information retrieved successfully";

  std::string path = std::string(RoutesConsts::kPathAPI) + getName();
#ifdef DEBUG
  logger->debug("Registering " + path);
#endif

  std::vector<OpenAPIResponse> responses;
  OpenAPIResponse successResponse(200, kResponseDesc);
  successResponse.schema = kSchemaJson;
  successResponse.example = kExampleJson;
  responses.push_back(successResponse);

  OpenAPIRoute route(path.c_str(), "GET", kRouteDesc, "Board Info", false, {}, responses);
  registerOpenAPIRoute(route);
  
  webserver.on(path.c_str(), HTTP_GET, []()
               {
               // Gather ESP32 metrics
               JsonDocument doc;
               doc["uptimeMs"] = millis ();
               doc["board"] = "UNIHIKER_K10";
               doc["version"] = "1.0.0";
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
               webserver.send (200, RoutesConsts::kMimeJSON, output.c_str ()); });
  return true;
  // Add board-related routes here
}

std::string BoardInfoService::getName()
{
  return "board/v1";
}

std::string BoardInfoService::getPath(const std::string& finalpathstring)
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