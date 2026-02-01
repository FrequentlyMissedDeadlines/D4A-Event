/**
 * @file main.cpp
 * @brief Main application entry point for K10-Bot
 * @details Initializes hardware, manages services, and handles FreeRTOS tasks
 * for WiFi, UDP server, HTTP server, display, and sensor management.
 */

#include <stdio.h>
#include <Arduino.h>
#include <WiFi.h>
#include <freertos/task.h>
#include <TFT_eSPI.h>
#include "services/WiFiService.h"
#include <WebServer.h>
#include <AsyncUDP.h>
#include <LittleFS.h>
#include <SPIFFS.h>
#include <FFat.h>
#include <unihiker_k10.h>
#include "services/BoardInfoService.h"
#include "services/WebcamService.h"
#include "services/ServoService.h"
#include "services/UDPService.h"
#include "services/HTTPService.h"
#include "services/SettingsService.h"
#include "sensor/K10sensorsService.h"
#include "services/RollingLogger.h"
#include "ui/utb2026.h"


#define LOAD_FONT8N // TFT font - special case for library config
#define VERBOSE_DEBUG // Enable verbose debug logging
namespace
{
  constexpr uint16_t kWebPort = 80;
  constexpr TickType_t kUdpTaskDelayTicks = pdMS_TO_TICKS(1000);
  constexpr TickType_t kDisplayTaskDelayTicks = pdMS_TO_TICKS(250);
  constexpr TickType_t kDisplayUpdateIntervalTicks = pdMS_TO_TICKS(25000);
  constexpr TickType_t kWebServerTaskDelayTicks = pdMS_TO_TICKS(10);
  constexpr uint8_t kWifiMaxAttempts = 20;
  constexpr uint16_t kWifiAttemptDelayMs = 500;
  constexpr uint32_t kSerialBaud = 115200;
  constexpr uint32_t kColorOk = 0x00D000;
  constexpr uint32_t kColorError = 0xD00000;
  constexpr uint32_t kColorInfo = 0xD0D0D0;
  constexpr uint32_t kColorSubtle = 0xE0E0E0;

  std::string ssid = "";
  std::string password = "";

  // Separate buffer for display (non-blocking)
  constexpr int MAX_MESSAGE_LEN = 256;
  std::set<std::string> allRoutes = {};
  // Global variables for master IP and token (required by web_server.cpp)
  std::string master_IP = "";
  std::string master_TOKEN = "";
}

TFT_eSPI tft;

UNIHIKER_K10 unihiker;
Music music;
WifiService wifiService = WifiService();
WebServer webserver(kWebPort);
UDPService udpService = UDPService();
HTTPService httpService = HTTPService();
SettingsService settingsService = SettingsService();
K10SensorsService k10sensorsService = K10SensorsService();
BoardInfoService boardInfo = BoardInfoService();
ServoService servoService = ServoService();
WebcamService webcamService = WebcamService();
RollingLogger debugLogger = RollingLogger();
RollingLogger appInfoLogger = RollingLogger();
UTB2026 utb2026UI;
#include "services/MusicService.h"
MusicService musicService = MusicService();

// Camera frame queue
// Packet handling is implemented in udp_handler module

// FreeRTOS Task: Handle UDP messages on Core 0
void udpSvrTask(void *pvParameters)
{
  for (;;)
  {
    vTaskDelay(kUdpTaskDelayTicks);
  }
}
// FreeRTOS Task: Update display on Core 1 (non-blocking) - SIMPLIFIED to avoid blocking
void displayTask(void *pvParameters)
{
  char localLastMessage[MAX_MESSAGE_LEN] = {0};
  int localTotalMessages = 0;
  int lastDisplayedTotalMessages = 0;
  TickType_t lastUpdateTick = xTaskGetTickCount();
  TickType_t lastWakeTick = xTaskGetTickCount();

  for (;;)
  {
    // Check if there's a master conflict to handle
    // This will display the dialog and wait for button input

    TickType_t now = xTaskGetTickCount();
    if ((now - lastUpdateTick) >= kDisplayUpdateIntervalTicks)
    {
      lastUpdateTick = now;
    }

    vTaskDelayUntil(&lastWakeTick, kDisplayTaskDelayTicks);
  }
}

// FreeRTOS Task: Handle web server on Core 1
void httpSvrTask(void *pvParameters)
{
  debugLogger.info("HTTP server task started");
  int loopCount = 0;
  for (;;)
  {
    httpService.handleClient(&webserver);
    loopCount++;
    if (loopCount % 1000 == 0)
    {
      debugLogger.trace("WebServer task running...");
    }
    // Very short delay to allow other tasks to run
    vTaskDelay(kWebServerTaskDelayTicks);
  }
}

namespace
{

  /**
   * @fn start_service
   * @brief Initialize and start a service, attaching the debug logger. Uses LED 0 to express state.
   * @param service Reference to the ServiceInterface to start.
   * @return true if the service started successfully, false otherwise.
   */
  bool start_service(IsServiceInterface &service)
  {
    // If the service supports logging, attach the global debug logger.
    unihiker.rgb->write(0, 32, 0, 0); // pixel0 = red
    service.setLogger(&debugLogger);
#ifdef VERBOSE_DEBUG
    debugLogger.debug("Service " + service.getServiceName());
#endif
    if (service.initializeService())
    {
      unihiker.rgb->write(0, 32, 32, 0); // pixel0 = red
      if (!service.startService())
      {
        appInfoLogger.error(service.getServiceName() + " start failed.");
      }
      else
      {
        unihiker.rgb->write(0, 0, 32, 0); // pixel0 = red
#ifdef VERBOSE_DEBUG
        appInfoLogger.debug(service.getServiceName() + " started.");
#endif
      }
    }
    else
    {
      appInfoLogger.error(service.getServiceName() + " initialize failed.");
    }

    // // Register OpenAPI routes if the service implements IsOpenAPIInterface
    IsOpenAPIInterface *openapi = service.asOpenAPIInterface();
    if (openapi)
    {
      openapi->registerRoutes();
      try
      {
#ifdef VERBOSE_DEBUG
        debugLogger.info("OpenAPI registered " + service.getServiceName());
#endif
        httpService.registerOpenAPIService(openapi);
      }
      catch (const std::exception &e)
      {
        debugLogger.error("registerOpenAPIService failed for " + service.getServiceName());
      }
    }
    else
    {
#ifdef VERBOSE_DEBUG
      debugLogger.debug("No OpenAPI for " + service.getServiceName());
#endif
    }
    unihiker.rgb->write(0, 0, 0, 0); // pixel0 = red

    return true;
  }

} // namespace

// void listFilesInFS(fs::FS &fs, const char* fsName)
// {
//   debugLogger.info(std::string("=== ") + fsName + " File List ===");
//   File root = fs.open("/");
//   if (!root)
//   {
//     debugLogger.error("Failed to open root directory");
//     return;
//   }
//   if (!root.isDirectory())
//   {
//     debugLogger.error("Root is not a directory");
//     return;
//   }
//   int fileCount = 0;
//   File file = root.openNextFile();
//   while (file)
//   {
//     fileCount++;
//     String filename = file.name();
//     size_t filesize = file.size();
//     debugLogger.info(std::string(filename.c_str()) + " (" + std::to_string(filesize) + " bytes)");
//     file = root.openNextFile();
//   }
//   debugLogger.info("Total: " + std::to_string(fileCount) + " files");
// }
// void testAllPartFitions()
// {
//   const char* partitions[] = { "voice_data"};
//   constexpr size_t num_partitions = 1;
//   for (size_t i = 0; i < num_partitions; i++)
//   {
//     const char* partitionLabel = partitions[i];
//     debugLogger.info(std::string("Checking FS '") + partitionLabel + "'");
//     // Try LittleFS on partition
//     if (LittleFS.begin(false, "/littlefs", 10, "voice_data")) 
//     {
//       appInfoLogger.info("SUCCESS: LittleFS " + std::string(partitionLabel) + " mounted");
//       listFilesInFS(LittleFS, (std::string("LittleFS_") + partitionLabel).c_str());
//       LittleFS.end();    
//     }
//     else
//     {
//       debugLogger.error(std::string("FAILED: LittleFS '") + partitionLabel + "'");
//     }
//     delay(500);
//   }
// }

void setup()
{
  // Small delay to ensure system stabilizes
  delay(500);
#ifdef VERBOSE_DEBUG
  // Initialize Serial for debugging
  Serial.begin(kSerialBaud);
  delay(1000); // Wait for Serial to initialize
  Serial.println("\n\n===========================================");
  Serial.println("[BOOT] K10-Bot Starting...");
  Serial.println("===========================================");
  Serial.flush();

  Serial.println("[INIT] Initializing UNIHIKER...");
#endif
  unihiker.begin();
  unihiker.initScreen(2, 30);
  unihiker.creatCanvas();
  unihiker.setScreenBackground(TFT_DARKGREY);
  unihiker.canvas->canvasClear();
#ifdef VERBOSE_DEBUG
  Serial.println("[INIT] Display initialized");
#endif

  unihiker.rgb->write(0, 0, 0, 0);
  unihiker.rgb->write(1, 0, 0, 0);
  unihiker.rgb->write(2, 0, 0, 0);
  // Initialize the display once via the helper

  appInfoLogger.set_logger_viewport(0, 120, 240, 320);
  appInfoLogger.set_logger_text_color(TFT_GREEN, TFT_DARKCYAN);
  appInfoLogger.set_max_rows(8);
  appInfoLogger.set_log_level(RollingLogger::INFO);

  debugLogger.set_logger_text_color(TFT_BLACK, TFT_BLACK);
  debugLogger.set_logger_viewport(0, 40, 240, 200);
  debugLogger.set_max_rows(16);
  debugLogger.set_log_level(RollingLogger::DEBUG);

// testAllPartFitions();
// return;
  



  debugLogger.info("Starting services...");
  if (!start_service(wifiService))
  {
    appInfoLogger.error("WiFi failed to start.");
    appInfoLogger.error("Fix code.");
    return;
  }
  start_service(settingsService);
  start_service(k10sensorsService);

  start_service(boardInfo);
  start_service(servoService);
  start_service(webcamService);
  start_service(musicService);

  if (start_service(udpService))
  {
    xTaskCreatePinnedToCore(udpSvrTask, "UDPServer_Task", 2048, nullptr, 3, nullptr, 0);
  }
  else
  {
    appInfoLogger.error("Failed to start UDP service");
  }

  if (start_service(httpService))
  {
    xTaskCreatePinnedToCore(httpSvrTask, "WebServer_Task", 8192, nullptr, 2, nullptr, 1);
  }
  else
  {
    appInfoLogger.error("Failed to start webserver");
  }

  utb2026UI.init();

  utb2026UI.set_info(utb2026UI.KEY_WIFI_NAME, wifiService.getSSID().c_str());
  utb2026UI.set_info(utb2026UI.KEY_IP_ADDRESS, wifiService.getIP().c_str());
  utb2026UI.draw_all();
  unihiker.rgb->write(0, 0, 0, 0);
  unihiker.rgb->write(1, 0, 0, 0);
  unihiker.rgb->write(2, 0, 0, 0);

  appInfoLogger.info("Bot "+wifiService.getHostname()+" started.");
  appInfoLogger.info(wifiService.getIP() + " " + wifiService.getSSID());
  appInfoLogger.info("UDP port:" + std::to_string(udpService.getPort()));
  
  xTaskCreatePinnedToCore(displayTask, "Display_Task", 4096, nullptr, 1, nullptr, 1);
}

void loop()
{
  // debugLogger.debug("debug msg");
  // debugLogger.info("info msg");
  // debugLogger.warning("warning msg");
  // debugLogger.error("error msg");

  // // All application logic runs inside FreeRTOS tasks
  // delay(60000);
}
