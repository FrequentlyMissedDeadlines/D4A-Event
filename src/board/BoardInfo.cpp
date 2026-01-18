#include "BoardInfo.h"
#include <WebServer.h>
#include <unihiker_k10.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>


std::set<std::string> BoardInfo::getRoutes()
{
  return routes;
}

bool BoardInfo::registerRoutes(WebServer *server, std::string basePath)
{
  if (!server)
  {
    return false;
  }
std::string path = "/api/" + basePath;
  server->on(path.c_str(), HTTP_GET, [server]()
             {
               // Gather ESP32 metrics

               uint32_t freeStack = uxTaskGetStackHighWaterMark(NULL);
               std::string json = "{";
               json += "\"board\":\"UNIHIKER_K10\",";
               json += "\"version\":\"1.0.0\",";
               json += "\"heapTotal\":" + std::to_string(ESP.getHeapSize()) + ",";
               json += "\"heapFree\":" + std::to_string(ESP.getFreeHeap()) + ",";
               json += "\"uptimeMs\":" + std::to_string(millis()) + ",";
               json += "\"freeStackBytes\":" + std::to_string(freeStack) + ",";
               json += "\"chipCores\":" + std::to_string(ESP.getChipCores()) + ",";
               json += "\"chipModel\":\"" + std::string(ESP.getChipModel()) + "\",";
               json += "\"chipRevision\":" + std::to_string(ESP.getChipRevision()) + ",";
               json += "\"cpuFreqMHz\":" + std::to_string(ESP.getCpuFreqMHz()) + ",";
               json += "\"freeSketchSpace\":" + std::to_string(ESP.getFreeSketchSpace()) + ",";
               json += "\"sdkVersion\":\"" + std::string(ESP.getSdkVersion()) + "\"";

               json += "}";
               server->send(200, "application/json", json.c_str());
             });
  routes.insert(path);
  return true;
  // Add board-related routes here
}