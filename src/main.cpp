// VERSION: UDP message receiver with web display (FreeRTOS dual-core)
// Description: Core 0 handles UDP, Core 1 handles web server for parallel processing

#include <Arduino.h>
#include <WiFi.h>
#include <stdio.h>

#ifdef DEBUG
#define DEBUG_TO_SERIAL(x) Serial.println(x)
#define DEBUGF_TO_SERIAL(fmt, ...) Serial.printf(fmt, __VA_ARGS__)
#else
#define DEBUG_TO_SERIAL(x)
#define DEBUGF_TO_SERIAL(fmt, ...)
#endif
#include <WebServer.h>
#include <AsyncUDP.h>
#include <unihiker_k10.h>
#include "network/udp_handler.h"
#include "network/web_server.h"
#include "servo/servo_handler.h"
#include "camera/unihiker_k10_webcam.h"
#include "sensor/sensor_handler.h"
#include <freertos/task.h>

const char* ssid = "Freebox-A35871";
const char* password = "azertQSDF1234";

namespace {
constexpr uint16_t kWebPort = 81;
constexpr uint16_t kUdpPort = 12345;
constexpr TickType_t kUdpTaskDelayTicks = pdMS_TO_TICKS(1000);
constexpr TickType_t kDisplayTaskDelayTicks = pdMS_TO_TICKS(250);
constexpr TickType_t kDisplayUpdateIntervalTicks = pdMS_TO_TICKS(2500);
constexpr TickType_t kWebServerTaskDelayTicks = pdMS_TO_TICKS(10);
constexpr uint8_t kWifiMaxAttempts = 20;
constexpr uint16_t kWifiAttemptDelayMs = 500;
constexpr uint32_t kColorOk = 0x00D000;
constexpr uint32_t kColorError = 0xD00000;
constexpr uint32_t kColorInfo = 0xD0D0D0;
constexpr uint32_t kColorSubtle = 0xE0E0E0;

void initializeDisplayHardware();
bool connectToWiFi();
void drawRuntimeStatus(uint8_t startLine);
void startCommunicationModules(uint8_t &line);
void startServoAndRoutes(uint8_t &line);
void createApplicationTasks();
void drawCanvasLine(uint8_t line, const char* text, uint32_t color);
void refreshCanvas();
}

UNIHIKER_K10 unihiker;
WebServer server(kWebPort);
AsyncUDP udp;

volatile bool gUdpRunning = false;
volatile bool gHttpRunning = false;

// Separate buffer for display (non-blocking)
constexpr int MAX_MESSAGE_LEN = 256;

// Global variables for master IP and token (required by web_server.cpp)
String master_IP = "";
String master_TOKEN = "";

// Camera frame queue
// Packet handling is implemented in udp_handler module

// FreeRTOS Task: Handle UDP messages on Core 0
void udpTask(void *pvParameters) {
  for (;;) {
    vTaskDelay(kUdpTaskDelayTicks);
  }
}
// FreeRTOS Task: Update display on Core 1 (non-blocking) - SIMPLIFIED to avoid blocking
void displayTask(void *pvParameters) {
  char localLastMessage[MAX_MESSAGE_LEN] = {0};
  int localTotalMessages = 0;
  int lastDisplayedTotalMessages = 0;
  TickType_t lastUpdateTick = xTaskGetTickCount();
  TickType_t lastWakeTick = xTaskGetTickCount();

  for (;;) {
    // Check if there's a master conflict to handle
    // This will display the dialog and wait for button input
    WebServerModule_handleMasterConflict();
    
    TickType_t now = xTaskGetTickCount();
    if ((now - lastUpdateTick) >= kDisplayUpdateIntervalTicks) {
      lastUpdateTick = now;

      if (UDPHandler_tryCopyDisplay(localLastMessage, sizeof(localLastMessage), localTotalMessages)) {
        if (localLastMessage[0] != '\0' && localTotalMessages > lastDisplayedTotalMessages) {
          unihiker.canvas->canvasClear();
          unihiker.canvas->canvasText("UDP Receiver", 1, kColorInfo);

          char totalBuffer[32];
          snprintf(totalBuffer, sizeof(totalBuffer), "Total: %d", localTotalMessages);
          unihiker.canvas->canvasText(totalBuffer, 3, kColorInfo);

          char lastBuffer[80];
          snprintf(lastBuffer, sizeof(lastBuffer), "Last: %s", localLastMessage);
          unihiker.canvas->canvasText(lastBuffer, 2, kColorSubtle);

          drawRuntimeStatus(4);

          unihiker.canvas->updateCanvas();
          lastDisplayedTotalMessages = localTotalMessages;
        }
      }
    }

    vTaskDelayUntil(&lastWakeTick, kDisplayTaskDelayTicks);
  }
}

// FreeRTOS Task: Handle web server on Core 1
void webServerTask(void *pvParameters) {
  DEBUG_TO_SERIAL("WebServer task started on Core 1");
  int loopCount = 0;
  for (;;) {
    WebServerModule_handleClient(&server);
    loopCount++;
    if (loopCount % 1000 == 0) {
      DEBUGF_TO_SERIAL("WebServer loop count: %d\n", loopCount);
    }
    // Very short delay to allow other tasks to run
    vTaskDelay(kWebServerTaskDelayTicks); 
  }
}

namespace {

void initializeDisplayHardware() {
  unihiker.begin();
  unihiker.initScreen(1, 15);
  unihiker.creatCanvas();
  unihiker.setScreenBackground(0);
  unihiker.canvas->canvasClear();
}

void drawCanvasLine(uint8_t line, const char* text, uint32_t color) {
  unihiker.canvas->canvasText(text, line, color);
}

void refreshCanvas() {
  unihiker.canvas->updateCanvas();
}

bool connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unihiker.canvas->canvasClear();
  drawCanvasLine(1, "Connecting WiFi...", kColorInfo);
  refreshCanvas();

  for (uint8_t attempt = 1; attempt <= kWifiMaxAttempts; ++attempt) {
    if (WiFi.status() == WL_CONNECTED) {
      drawCanvasLine(1, "WiFi Ok.", kColorOk);
      refreshCanvas();
      return true;
    }

    char attemptBuffer[48];
    snprintf(attemptBuffer, sizeof(attemptBuffer), "#%u %s", attempt, ssid);
    drawCanvasLine(2, attemptBuffer, kColorInfo);
    refreshCanvas();
    delay(kWifiAttemptDelayMs);
  }

  drawCanvasLine(1, "WiFi Failed.", kColorError);
  refreshCanvas();
  return false;
}

void drawRuntimeStatus(uint8_t startLine) {
  IPAddress ip = WiFi.localIP();
  char ipBuffer[24];
  snprintf(ipBuffer, sizeof(ipBuffer), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  drawCanvasLine(startLine++, ipBuffer, kColorInfo);

  char udpBuffer[40];
  snprintf(udpBuffer, sizeof(udpBuffer), "%s UDP (%u)", gUdpRunning ? "UDP on" : "NO UDP", static_cast<unsigned>(kUdpPort));
  drawCanvasLine(startLine++, udpBuffer, gUdpRunning ? kColorOk : kColorError);

  char httpBuffer[40];
  snprintf(httpBuffer, sizeof(httpBuffer), "%s HTTP (%u)", gHttpRunning ? "HTTP on" : "NO HTTP", static_cast<unsigned>(kWebPort));
  drawCanvasLine(startLine, httpBuffer, gHttpRunning ? kColorOk : kColorError);
}

void startCommunicationModules(uint8_t &line) {
  if (UDPHandler_begin(&udp, kUdpPort)) {
    gUdpRunning = true;
    drawCanvasLine(line++, "UDP: Started", kColorOk);
  } else {
    drawCanvasLine(line++, "UDP: Failed", kColorError);
  }

  WebServerModule_begin(&server);
  gHttpRunning = true;
  drawCanvasLine(line++, "HTTP: Started", kColorOk);
  refreshCanvas();
}

void startServoAndRoutes(uint8_t &line) {
  if (ServoHandler_init()) {
    drawCanvasLine(line++, "Servo: Started", kColorOk);
    DEBUG_TO_SERIAL("Servo controller initialized successfully");
    SensorModule_registerRoutes(&server, unihiker);
  } else {
    drawCanvasLine(line++, "Servo: Init Failed", kColorError);
    DEBUG_TO_SERIAL("ERROR: Failed to initialize servo controller");
  }
  ServoModule_registerRoutes(&server);
  refreshCanvas();
}

void createApplicationTasks() {
  xTaskCreatePinnedToCore(udpTask, "UDP_Task", 2048, nullptr, 3, nullptr, 0);
  xTaskCreatePinnedToCore(displayTask, "Display_Task", 4096, nullptr, 1, nullptr, 1);
  xTaskCreatePinnedToCore(webServerTask, "WebServer_Task", 4096, nullptr, 2, nullptr, 1);
}

}  // namespace





void setup() {
  // Small delay to ensure system stabilizes
  delay(1000);

  // Initialize Serial for debugging
  Serial.begin(115200);
  delay(500);
  DEBUG_TO_SERIAL("\n\n=== K10 UDP Receiver Starting ===");

  // Initialize the display once via the helper
  initializeDisplayHardware();
  drawCanvasLine(1, "Starting...", kColorInfo);
  refreshCanvas();
  delay(500);

  // Establish WiFi before starting any modules
  if (!connectToWiFi()) {
    drawCanvasLine(2, "WiFi failed. Reboot to retry.", kColorError);
    refreshCanvas();
    return;
  }

  unihiker.canvas->canvasClear();
  uint8_t statusLine = 1;
  drawCanvasLine(statusLine++, "WiFi Ok.", kColorOk);

  startCommunicationModules(statusLine);
  startServoAndRoutes(statusLine);
  createApplicationTasks();

  drawCanvasLine(statusLine++, "All tasks started.", kColorOk);
  drawCanvasLine(statusLine, "Ready!", kColorOk);
  refreshCanvas();
}

void loop() {
  // All application logic runs inside FreeRTOS tasks
  delay(1);
}