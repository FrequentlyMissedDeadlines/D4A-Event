#include <stdio.h>
#include <Arduino.h>
#include <WiFi.h>
#include <freertos/task.h>
#include <TFT_eSPI.h>
#include "services/WiFiService.h"
#include <WebServer.h>
#include <AsyncUDP.h>
#include <unihiker_k10.h>

#include "board/BoardInfo.h"
#include "camera/K10Webcam.h"
#include "services/ServoService.h"
#include "services/UDPService.h"
#include "services/HTTPService.h"
#include "sensor/K10sensorsService.h"
#include "ui/utb2026.h"
#include "services/LoggerService.h"

#define LOAD_FONT8N // TFT font
#define DEFAULT_HTTP_PORT 80
#define DEFAULT_UDP_PORT 22026
#define DEFAULT_UDP_DELAY_TICKS 1000
#define DEFAULT_DISPLAY_DELAY_TICKS 250
#define DEFAULT_UI_DELAY_TICKS 25000
#define DEFAULT_HTTP_DELAY_TICKS 10
#define DEFAULT_WIFI_MAX_ATTEMPTS 20
#define DEFAULT_WIFI_DELAY_MS 500
#define DEFAULT_SERIAL_BAUD 115200

namespace
{
  constexpr uint16_t kWebPort = DEFAULT_HTTP_PORT;
  constexpr uint16_t kUdpPort = DEFAULT_UDP_PORT;
  constexpr TickType_t kUdpTaskDelayTicks = pdMS_TO_TICKS(DEFAULT_UDP_DELAY_TICKS);
  constexpr TickType_t kDisplayTaskDelayTicks = pdMS_TO_TICKS(DEFAULT_DISPLAY_DELAY_TICKS);
  constexpr TickType_t kDisplayUpdateIntervalTicks = pdMS_TO_TICKS (DEFAULT_UI_DELAY_TICKS);
  constexpr TickType_t kWebServerTaskDelayTicks = pdMS_TO_TICKS(DEFAULT_HTTP_DELAY_TICKS);
  constexpr uint8_t kWifiMaxAttempts = DEFAULT_WIFI_MAX_ATTEMPTS;
  constexpr uint16_t kWifiAttemptDelayMs = DEFAULT_WIFI_DELAY_MS;
  constexpr uint32_t kColorOk = 0x00D000;
  constexpr uint32_t kColorError = 0xD00000;
  constexpr uint32_t kColorInfo = 0xD0D0D0;
  constexpr uint32_t kColorSubtle = 0xE0E0E0;

  std::string ssid ="" ; 
  std::string password ="" ; 

  WifiService wifiService;

  void initialize_display_hardware();
  void draw_runtime_status(uint8_t start_line);
  void start_communication_modules();
  void start_servo_and_routes();
  void create_application_tasks();
  void draw_canvas_line(uint8_t line, const char *text, uint32_t color);
  void refresh_canvas();
  // Separate buffer for display (non-blocking)
  constexpr int MAX_MESSAGE_LEN = 256;
  std::set<std::string> allRoutes = {};
  // Global variables for master IP and token (required by web_server.cpp)
  std::string master_IP = "";
  std::string master_TOKEN = "";
}
TFT_eSPI tft;


UNIHIKER_K10 unihiker;

WifiService wifi = WifiService();
HTTPService httpService = HTTPService();
UDPService udpService = UDPService();
K10SensorsService k10sensorsService = K10SensorsService();
BoardInfo boardInfo = BoardInfo();
LoggerService debugLogger =LoggerService();
LoggerService appInfoLog=LoggerService();
WebServer server(kWebPort);

UTB2026 utb2026UI;
volatile bool gUdpRunning = false;
volatile bool gHttpRunning = false;

// Camera frame queue
// Packet handling is implemented in udp_handler module

// FreeRTOS Task: Handle UDP messages on Core 0
void udpTask(void *pvParameters)
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
    httpService.handleMasterConflict();

    TickType_t now = xTaskGetTickCount();
    if ((now - lastUpdateTick) >= kDisplayUpdateIntervalTicks)
    {
      lastUpdateTick = now;


    }

    vTaskDelayUntil(&lastWakeTick, kDisplayTaskDelayTicks);
  }
}

// FreeRTOS Task: Handle web server on Core 1
void webServerTask(void *pvParameters)
{

  int loopCount = 0;
  for (;;)
  {
    httpService.handleClient(&server);
    loopCount++;
    if (loopCount % 1000 == 0)
    {
      debugLogger.debug("WebServer task running...");
    }
    // Very short delay to allow other tasks to run
    vTaskDelay(kWebServerTaskDelayTicks);
  }
}

namespace
{

  void initialize_display_hardware()
  {
    unihiker.begin();
    unihiker.initScreen(2, 30);
    unihiker.creatCanvas();
    unihiker.setScreenBackground(0);
    unihiker.canvas->canvasClear();
  }

  void draw_canvas_line(uint8_t line, const char *text, uint32_t color)
  {
    unihiker.canvas->canvasText(text, line, color);
  }

  void refresh_canvas()
  {
    unihiker.canvas->updateCanvas();
  }

  // bool connectToWiFi()
  // {
  //   WiFi.mode(WIFI_STA);
  //   WiFi.begin(ssid.c_str(), password.c_str());

  //   unihiker.canvas->canvasClear();
  //   applogger.log("Connecting WiFi...", Logger::INFO);
  //   refreshCanvas();

  //   for (uint8_t attempt = 1; attempt <= kWifiMaxAttempts; ++attempt)
  //   {
  //     if (WiFi.status() == WL_CONNECTED)
  //     {
  //       applogger.log(WiFi.localIP().toString().c_str(), Logger::INFO);

  //       refreshCanvas();
  //       return true;
  //     }

  //     char attemptBuffer[48];
  //     snprintf(attemptBuffer, sizeof(attemptBuffer), "#%u %s", attempt, ssid);
  //     applogger.log(attemptBuffer, Logger::INFO);
  //     refreshCanvas();
  //     delay(kWifiAttemptDelayMs);
  //   }

  //   applogger.log("WiFi FWifiServiceailed.", Logger::ERROR);
  //   refreshCanvas();
  //   return false;
  // }

  void start_communication_modules()
  {
    // Initialize and start HTTP service
    debugLogger.info("WebServer: Initializing");
    if (!httpService.init())
    {
      debugLogger.error("HTTP init failed.");
    }
    
    debugLogger.info("WebServer: Starting on port " + std::to_string(kWebPort));
    httpService.begin(&server);
    if (!httpService.start())
    {
      debugLogger.error("HTTP start failed.");
    }
    
    // Initialize and start UDP service
    appInfoLog.info("UDP: Starting on port " + std::to_string(kUdpPort));
    if (udpService.init() && udpService.start())
    {
      gUdpRunning = true;
      appInfoLog.info("UDP: Started");
    }
    else
    {
      appInfoLog.error("UDP: Failed");
    }
    
    // Register UDP routes
    if (udpService.registerRoutes(&server, "udp_in"))
    { 
      debugLogger.info("WebServer: Started");
      debugLogger.info("Register UDP routes");
      allRoutes.insert(udpService.getRoutes().begin(), udpService.getRoutes().end());
    }
    else
    {
      debugLogger.error("WebServer: Failed.");
    }
    
    // Register board info routes
    debugLogger.info("Register board routes");
    if (boardInfo.registerRoutes(&server, "board"))
    {
      debugLogger.info("Board routes registered.");
      allRoutes.insert(boardInfo.getRoutes().begin(), boardInfo.getRoutes().end());
    }
    else
    {
      debugLogger.error("Board routes failed.");
    }

    gHttpRunning = true;
    appInfoLog.info("HTTP: Started");

  }

  void start_servo_and_routes()
  {
    // Initialize and start K10 sensors service
    debugLogger.info("Sensors: Initializing");
    if (!k10sensorsService.init())
    {
      debugLogger.error("Sensors init failed.");
    }
    
    debugLogger.info("Sensors: Starting");
    if (!k10sensorsService.start())
    {
      debugLogger.error("Sensors start failed.");
    }
    
    // Register sensor routes
    if (k10sensorsService.registerRoutes(&server, "sensors"))
    {
      debugLogger.info("Sensor routes registered.");
      allRoutes.insert(k10sensorsService.getRoutes().begin(), k10sensorsService.getRoutes().end());
    }
    else
    {
      debugLogger.error("Sensor routes failed.");
    }
  }

  void create_application_tasks()
  {
    xTaskCreatePinnedToCore(udpTask, "UDP_Task", 2048, nullptr, 3, nullptr, 0);
    xTaskCreatePinnedToCore(displayTask, "Display_Task", 4096, nullptr, 1, nullptr, 1);
    xTaskCreatePinnedToCore(webServerTask, "WebServer_Task", 4096, nullptr, 2, nullptr, 1);
  }

} // namespace

void setup()
{
  // Small delay to ensure system stabilizes
  delay(500);

  // Initialize Serial for debugging
  Serial.begin(DEFAULT_SERIAL_BAUD);
  delay(500);

  // Initialize the display once via the helper
  initialize_display_hardware();

  utb2026UI.init();

  appInfoLog.set_logger_viewport(0, 120, 240, 320);
  appInfoLog.set_logger_text_color(TFT_GREEN, TFT_DARKGREY);
  appInfoLog.set_max_rows(4);
  appInfoLog.set_log_level(LoggerService::DEBUG);
  appInfoLog.info("AppLogger initialized.");

  debugLogger.set_logger_text_color(TFT_BLUE, TFT_BLUE);
  debugLogger.set_logger_viewport(0, 40, 240, 160);
  debugLogger.set_max_rows(10);
  debugLogger.set_log_level(LoggerService::DEBUG);
  debugLogger.info("Logger initialized.");

  debugLogger.info("Wifi_activation");
  // Establish WiFi before starting any modules
  if (!wifiService.wifi_activation())
  {
    debugLogger.error("WiFi failed. Reboot to retry.");
    return;
  }

  utb2026UI.draw_all();
  debugLogger.info("WiFi Ok.");
  appInfoLog.info("SSID:"+wifiService.getSSID());
  utb2026UI.set_info(utb2026UI.KEY_WIFI_NAME, wifiService.getSSID().c_str());
  
  appInfoLog.info("IP:"+  wifiService.getIP());
  utb2026UI.set_info(utb2026UI.KEY_IP_ADDRESS, wifiService.getIP() .c_str());
  utb2026UI.draw_all();  
  
  debugLogger.info("startCommunicationModules.");
  start_communication_modules();
  utb2026UI.draw_all();
  
  debugLogger.info("startServoAndRoutes.");
  start_servo_and_routes();
  utb2026UI.draw_all();
  
  debugLogger.info("createApplicationTasks.");
  create_application_tasks();
  debugLogger.log("Ready!", LoggerService::INFO);

  utb2026UI.draw_all();
}

void loop()
{
  debugLogger.debug("debug msg");
  debugLogger.info("info msg");
  debugLogger.warning("warning msg");
  debugLogger.error("error msg");

  // All application logic runs inside FreeRTOS tasks
  delay(1500);
}