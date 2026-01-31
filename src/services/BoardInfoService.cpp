/**
 * @file BoardInfoService.cpp
 * @brief Implementation for board information service
 * @details Exposed routes:
 *          - GET /api/board/v1/ - Retrieve board information including system metrics, memory usage, chip details, and firmware version
 * 
 */

#include "BoardInfoService.h"
#include <WebServer.h>
#include <unihiker_k10.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ArduinoJson.h>

// BoardInfoService constants namespace
namespace BoardInfoConsts
{
    constexpr const char str_board_name[] PROGMEM = "UNIHIKER_K10";
    constexpr const char str_service_name[] PROGMEM = "Board info Service X";
    constexpr const char path_service[] PROGMEM = "board/v1";
    constexpr const char str_firmware_version[] PROGMEM = "1.0.0";
}

bool BoardInfoService::initializeService() { 
    service_status_ = STARTED;
    status_timestamp_ = millis();
    #ifdef DEBUG
    logger->debug(getServiceName() + " initialize done");
    #endif
    return true; 
}
bool BoardInfoService::startService() { 
    service_status_ = STARTED;
    status_timestamp_ = millis();
    #ifdef DEBUG
    logger->debug(getServiceName() + " start done");
    #endif
    return true; 
}
bool BoardInfoService::stopService() { 
    service_status_ = STOPPED;
    status_timestamp_ = millis();
    #ifdef DEBUG
    logger->debug(getServiceName() + " stop done");
    #endif  
    return true; 
}

bool BoardInfoService::registerRoutes()
{


  static constexpr char kSchemaJson[] PROGMEM = R"({"type":"object","properties":{"uptimeMs":{"type":"integer","description":"System uptime in milliseconds"},"board":{"type":"string","description":"Board model name"},"version":{"type":"string","description":"Firmware version"},"heapTotal":{"type":"integer","description":"Total heap size in bytes"},"heapFree":{"type":"integer","description":"Free heap size in bytes"},"freeStackBytes":{"type":"integer","description":"Free stack space for current task"},"chipCores":{"type":"integer","description":"Number of CPU cores"},"chipModel":{"type":"string","description":"Chip model name"},"chipRevision":{"type":"integer","description":"Chip revision number"},"cpuFreqMHz":{"type":"integer","description":"CPU frequency in MHz"},"freeSketchSpace":{"type":"integer","description":"Available flash space for sketch"},"sdkVersion":{"type":"string","description":"ESP-IDF SDK version"}}})";
  static constexpr char kExampleJson[] PROGMEM = R"({"uptimeMs":123456,"board":"UNIHIKER_K10","version":"1.0.0","heapTotal":327680,"heapFree":280000,"freeStackBytes":2048,"chipCores":2,"chipModel":"ESP32-S3","chipRevision":1,"cpuFreqMHz":240,"freeSketchSpace":1310720,"sdkVersion":"v4.4.2"})";
  static constexpr char kRouteDesc[] PROGMEM = "Retrieves comprehensive board information including system metrics, memory usage, chip details, and firmware version";
  static constexpr char kResponseDesc[] PROGMEM = "Board information retrieved successfully";

  std::string path = getPath("");
#ifdef DEBUG
  logger->debug("Registering " + path);
#endif

  std::vector<OpenAPIResponse> responses;
  OpenAPIResponse successResponse(200, kResponseDesc);
  successResponse.schema = kSchemaJson;
  successResponse.example = kExampleJson;
  responses.push_back(successResponse);

  OpenAPIRoute route(path.c_str(), RoutesConsts::method_get, kRouteDesc, "Board Info", false, {}, responses);
  registerOpenAPIRoute(route);
  
  webserver.on(path.c_str(), HTTP_GET, []()
               {
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
               webserver.send (200, RoutesConsts::mime_json, output.c_str ()); });

  registerSettingsRoutes("Board Info", this);

  return true;
  // Add board-related routes here
}

std::string BoardInfoService::getServiceName()
{
  return fpstr_to_string(FPSTR(BoardInfoConsts::str_service_name));
}
std::string BoardInfoService::getServiceSubPath()
{
    return fpstr_to_string(FPSTR(BoardInfoConsts::path_service));
}
std::string BoardInfoService::getPath(const std::string& finalpathstring)
{
  if (baseServicePath.empty()) {
    baseServicePath = std::string(RoutesConsts::path_api) + getServiceSubPath() + "/";
  }
  return baseServicePath + finalpathstring;
}

bool BoardInfoService::saveSettings()
{
    // To be implemented if needed
    return true;
}

bool BoardInfoService::loadSettings()
{
    // To be implemented if needed
    return true;
}