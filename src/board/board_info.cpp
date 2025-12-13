#include "board_info.h"
#include <WebServer.h>
#include <unihiker_k10.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#ifdef DEBUG
#define DEBUG_TO_SERIAL(x) Serial.println(x)
#define DEBUGF_TO_SERIAL(fmt, ...) Serial.printf(fmt, __VA_ARGS__)
#else
#define DEBUG_TO_SERIAL(x)
#define DEBUGF_TO_SERIAL(fmt, ...)
#endif

bool BoardModule::registerRoutes(WebServer* server) {
  if (!server) {
    DEBUG_TO_SERIAL("ERROR: Null WebServer pointer in BoardModule::registerRoutes");
    return false; 
  }
  server->on("/api/board", HTTP_GET, [server]() {
    DEBUG_TO_SERIAL("GET /api/board (board info)");
    // Gather ESP32 metrics
    
    
    
    uint32_t freeStack = uxTaskGetStackHighWaterMark(NULL);
    String json = "{";
    json += "\"board\":\"UNIHIKER_K10\",";
    json += "\"version\":\"1.0.0\",";
    json += "\"heapTotal\":" + String(ESP.getHeapSize()) + ",";
    json += "\"heapFree\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"uptimeMs\":" + String(millis()) + ",";
    json += "\"freeStackBytes\":" + String(freeStack) + ",";
    json += "\"chipCores\":\"" + String(ESP.getChipCores()) + "\",";
    json += "\"chipModel\":\"" + String(ESP.getChipModel()) + "\",";
    json += "\"chipRevision\":\"" + String(ESP.getChipRevision()) + "\",";
    json += "\"cpuFreqMHz\":\"" + String(ESP.getCpuFreqMHz()) + "\",";
    json += "\"freeSketchSpace\":\"" + String(ESP.getFreeSketchSpace()) + "\",";
    json += "\"sdkVersion\":\"" + String(ESP.getSdkVersion()) + "\"";

    json += "}";
    server->send(200, "application/json", json);

  });
      return true;
  // Add board-related routes here
}