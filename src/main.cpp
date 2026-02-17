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
#include <esp_log.h>

#include <WebServer.h>
#include <AsyncUDP.h>
#include <LittleFS.h>
#include <SPIFFS.h>
#include <FFat.h>
#include <unihiker_k10.h>
#include <esp_camera.h>
#include <img_converters.h>
#include "services/RollingLogger.h"
#include "services/esp_to_rolling.h"
#include "services/board/BoardInfoService.h"
 #include "services/camera/WebcamService.h"
#include "services/servo/ServoService.h"
#include "services/udp/UDPService.h"
#include "services/http/HTTPService.h"
#include "services/settings/SettingsService.h"
#include "services/sensor/K10sensorsService.h"
#include "services/log/RollingLoggerService.h"
#include "services/music/MusicService.h"
#include "services/wifi/WiFiService.h"
#include "ui/utb2026.h"

#define LOAD_FONT8N   // TFT font - special case for library config
#define VERBOSE_DEBUG // Enable verbose debug logging

// Main application constants
namespace MainConsts
{
  constexpr const char msg_http_task_started[] PROGMEM = "HTTP server task started";
  constexpr const char msg_webserver_running[] PROGMEM = "WebServer task running...";
  constexpr const char msg_service[] PROGMEM = "Service ";
  constexpr const char msg_start_failed[] PROGMEM = " start failed.";
  constexpr const char msg_started[] PROGMEM = " started.";
  constexpr const char msg_initialize_failed[] PROGMEM = " initialize failed.";
  constexpr const char msg_openapi_registered[] PROGMEM = "OpenAPI registered ";
  constexpr const char msg_register_failed[] PROGMEM = "registerOpenAPIService failed for ";
  constexpr const char msg_no_openapi[] PROGMEM = "No OpenAPI for ";
  constexpr const char msg_starting_services[] PROGMEM = "Starting services...";
  constexpr const char msg_fatal_wifi_failed[] PROGMEM = "FATAL : WiFi failed to start.";
  constexpr const char msg_failed_udp[] PROGMEM = "Failed to start UDP service";
  constexpr const char msg_failed_webserver[] PROGMEM = "Failed to start webserver";
  constexpr const char msg_bot_started[] PROGMEM = "Bot ";
  constexpr const char msg_udp_port[] PROGMEM = "UDP port:";
  constexpr const char str_serial_init_display[] PROGMEM = "[INIT] Display initialized";
}

namespace
{
  constexpr uint16_t web_port = 80;
  constexpr TickType_t udp_task_delay_ticks = pdMS_TO_TICKS(1000);
  constexpr TickType_t display_task_delay_ticks = pdMS_TO_TICKS(250);
  constexpr TickType_t display_update_interval_ticks = pdMS_TO_TICKS(250);
  constexpr TickType_t web_server_task_delay_ticks = pdMS_TO_TICKS(10);
  constexpr uint8_t wifi_max_attempts = 20;
  constexpr uint16_t wifi_attempt_delay_ms = 500;
  constexpr uint32_t serial_baud = 115200;
  constexpr uint32_t color_ok = 0x00D000;
  constexpr uint32_t color_error = 0xD00000;
  constexpr uint32_t color_info = 0xD0D0D0;
  constexpr uint32_t color_subtle = 0xE0E0E0;

  std::string ssid = "";
  std::string password = "";

  // Separate buffer for display (non-blocking)
  constexpr int max_message_len = 256;
  std::set<std::string> all_routes = {};
  // Global variables for master IP and token (required by web_server.cpp)
  std::string master_ip = "";
  std::string master_token = "";
}

TFT_eSPI tft;

UNIHIKER_K10 unihiker;
Music music;
WifiService wifi_service = WifiService();
WebServer webserver(web_port);
UDPService udp_service = UDPService();
HTTPService http_service = HTTPService();
SettingsService settings_service = SettingsService();
K10SensorsService k10sensors_service = K10SensorsService();
BoardInfoService board_info = BoardInfoService();
ServoService servo_service = ServoService();
WebcamService webcam_service = WebcamService();
RollingLogger debug_logger = RollingLogger();
RollingLogger app_info_logger = RollingLogger();
RollingLogger esp_logger = RollingLogger();
RollingLoggerService rolling_logger_service = RollingLoggerService();
UTB2026 ui = UTB2026();

MusicService music_service = MusicService();

// define CONFIG_GC2145_SUPPORT 1

// Camera frame queue
// Packet handling is implemented in udp_handler module

/**
 * @brief FreeRTOS Task: Handle UDP messages on Core 0
 * @param pvParameters Task parameters (unused)
 */
void xtask_UDP_SVR(void *pvParameters)
{
  for (;;)
  {
    vTaskDelay(udp_task_delay_ticks);
  }
}

/**
 * @brief FreeRTOS Task: Update display on Core 1 (non-blocking)
 * @param pvParameters Task parameters (unused)
 * @details Simplified implementation to avoid blocking operations
 */
void task_DISPLAY(void *pvParameters)
{
  char local_last_message[max_message_len] = {0};
  int local_total_messages = 0;
  int last_displayed_total_messages = 0;
  TickType_t last_update_tick = xTaskGetTickCount();
  TickType_t last_wake_tick = xTaskGetTickCount();

  for (;;)
  {
    TickType_t now = xTaskGetTickCount();
    if ((now - last_update_tick) >= display_update_interval_ticks)
    {
      last_update_tick = now;
    }

    vTaskDelayUntil(&last_wake_tick, display_task_delay_ticks);
  }
}

/**
 * @brief FreeRTOS Task: Handle web server on Core 1
 * @param pvParameters Task parameters (unused)
 */
void task_HTTP_SVR(void *pvParameters)
{
  debug_logger.info(progmem_to_string(MainConsts::msg_http_task_started));
  int loop_count = 0;
  for (;;)
  {
    http_service.handleClient(&webserver);
    loop_count++;
    if (loop_count % 1000 == 0)
    {
      debug_logger.trace(progmem_to_string(MainConsts::msg_webserver_running));
    }
    // Very short delay to allow other tasks to run
    vTaskDelay(web_server_task_delay_ticks);
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

#ifdef VERBOSE_DEBUG
    debug_logger.debug(progmem_to_string(MainConsts::msg_service) + service.getServiceName());
#endif
    service.setLogger(&debug_logger);

    // Attach settings service if this is not the settings service itself
    if (&service != static_cast<IsServiceInterface *>(&settings_service))
    {
      service.setSettingsService(&settings_service);
    }

    if (service.initializeService())
    {
      unihiker.rgb->write(0, 32, 32, 0); // pixel0 = red
      if (!service.startService())
      {
        app_info_logger.error(service.getServiceName() + progmem_to_string(MainConsts::msg_start_failed));
      }
      else
      {
        unihiker.rgb->write(0, 0, 32, 0); // pixel0 = red
#ifdef VERBOSE_DEBUG
        app_info_logger.debug(service.getServiceName() + progmem_to_string(MainConsts::msg_started));
#endif
      }
    }
    else
    {
      app_info_logger.error(service.getServiceName() + progmem_to_string(MainConsts::msg_initialize_failed));
    }

    // Register OpenAPI routes if the service implements IsOpenAPIInterface
    IsOpenAPIInterface *openapi = service.asOpenAPIInterface();
    if (openapi)
    {
      openapi->registerRoutes();
      try
      {
#ifdef VERBOSE_DEBUG
        debug_logger.info(progmem_to_string(MainConsts::msg_openapi_registered) + service.getServiceName());
#endif
        http_service.registerOpenAPIService(openapi);
      }
      catch (const std::exception &e)
      {
        debug_logger.error(progmem_to_string(MainConsts::msg_register_failed) + service.getServiceName());
      }
    }
    else
    {
#ifdef VERBOSE_DEBUG
      debug_logger.debug(progmem_to_string(MainConsts::msg_no_openapi) + service.getServiceName());
#endif
    }
    unihiker.rgb->write(0, 0, 0, 0); // pixel0 = red

    return true;
  }

}

/**
 * @brief Arduino setup function - initializes hardware and services
 * @details Initializes display, loggers, services, and FreeRTOS tasks
 */
void setup()
{
  // Small delay to ensure system stabilizes
  delay(500);
  
  // Configure ESP-IDF logging AS FIRST THING before any hardware init
  esp_log_level_set("*", ESP_LOG_DEBUG);
  
  // Initialize loggers BEFORE hardware to capture early logs
  app_info_logger.set_max_rows(32);
  app_info_logger.set_log_level(RollingLogger::INFO);
  debug_logger.set_max_rows(32);
  debug_logger.set_log_level(RollingLogger::DEBUG);
  esp_logger.set_max_rows(32);
  esp_logger.set_log_level(RollingLogger::DEBUG);

  // Redirect ESP-IDF logs BEFORE any other initialization
  esp_log_to_rolling_init(&esp_logger);
  
  // Test ESP logging immediately after redirect
  ESP_LOGI("Main", "TEST 1: ESP-IDF logging initialized");
  ESP_LOGD("Main", "TEST 2: Debug level message");
  ESP_LOGW("Main", "TEST 3: Warning level message");
  ESP_LOGE("Main", "TEST 4: Error level message");
  
  // Now initialize hardware
  unihiker.begin();
  unihiker.initScreen(2, 30);
  unihiker.creatCanvas();
  unihiker.setScreenBackground(TFT_BLACK);

  unihiker.rgb->write(0, 0, 0, 0);
  unihiker.rgb->write(1, 0, 0, 0);
  unihiker.rgb->write(2, 0, 0, 0);
  


  ui.init();
  // ui.add_logger_view(&app_info_logger, 0, 120, 240, 240, TFT_BLACK, TFT_BLACK);
  ui.add_logger_view(&debug_logger, 0, 40, 240, 120, TFT_DARKGREY,TFT_DARKGREY);
  xTaskCreatePinnedToCore(task_DISPLAY, "Display_Task", 4096, nullptr, 1, nullptr, 1);

  debug_logger.info(progmem_to_string(MainConsts::msg_starting_services));
  if (!start_service(wifi_service))
  {
    app_info_logger.error(progmem_to_string(MainConsts::msg_fatal_wifi_failed));

    return;
  }
  start_service(settings_service);
  start_service(k10sensors_service);
  start_service(board_info);
  start_service(servo_service);
  start_service(webcam_service);
  start_service(music_service);

  // Set up rolling logger service with logger instances (including esp_logger)
  rolling_logger_service.set_logger_instances(&debug_logger, &app_info_logger, &esp_logger);
  start_service(rolling_logger_service);

  if (start_service(udp_service))
  {
    xTaskCreatePinnedToCore(xtask_UDP_SVR, "UDPServer_Task", 2048, nullptr, 3, nullptr, 0);
  }
  else
  {
    app_info_logger.error(progmem_to_string(MainConsts::msg_failed_udp));
  }

  // Initialize fast camera and register route BEFORE starting HTTP service

  if (start_service(http_service))
  {
    xTaskCreatePinnedToCore(task_HTTP_SVR, "WebServer_Task", 8192, nullptr, 2, nullptr, 1);
  }
  else
  {
    app_info_logger.error(progmem_to_string(MainConsts::msg_failed_webserver));
  }

  ui.set_info(ui.KEY_WIFI_NAME, wifi_service.getSSID().c_str());
  ui.set_info(ui.KEY_IP_ADDRESS, wifi_service.getIP().c_str());
  ui.draw_all();
  unihiker.rgb->write(0, 0, 0, 0);
  unihiker.rgb->write(1, 0, 0, 0);
  unihiker.rgb->write(2, 0, 0, 0);

  app_info_logger.info(progmem_to_string(MainConsts::msg_bot_started) + wifi_service.getHostname() + progmem_to_string(MainConsts::msg_started));
  app_info_logger.info(wifi_service.getIP() + progmem_to_string(RoutesConsts::str_space) + wifi_service.getSSID());
  app_info_logger.info(progmem_to_string(MainConsts::msg_udp_port) + std::to_string(udp_service.getPort()));
}

/**
 * @brief Arduino main loop function
 * @details All application logic runs inside FreeRTOS tasks, this loop is empty
 */
void loop()
{
  // All application logic runs inside FreeRTOS tasks
  delay(1000);
}





// //debugging function to list files in a filesystem (e.g., LittleFS, SPIFFS, FFat)
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
// // debug function to check all partitions for LittleFS and list files (used for debugging storage issues)
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