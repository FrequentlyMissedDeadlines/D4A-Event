// VERSION: UDP message receiver with web display (FreeRTOS dual-core)
// Description: Core 0 handles UDP, Core 1 handles web server for parallel processing

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <AsyncUDP.h>
#include <unihiker_k10.h>
#include "udp_handler.h"
#include "web_server.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

const char* ssid = "Freebox-A35871";
const char* password = "azertQSDF1234";

const int WEB_PORT = 80;
UNIHIKER_K10 unihiker;
WebServer server(WEB_PORT);
AsyncUDP udp;
const int UDP_PORT = 12345;

unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_UPDATE_INTERVAL = 2500; // Update display every 2500ms

// Separate buffer for display (non-blocking)
const int MAX_MESSAGE_LEN = 256;
char lastMessage[MAX_MESSAGE_LEN] = "";
int displayTotalMessages = 0;

// Packet handling is implemented in udp_handler module

// FreeRTOS Task: Handle UDP messages on Core 0
void udpTask(void *pvParameters) {
  // Task keeps running and waiting for packets
  // The AsyncUDP callback handles packets automatically
  for (;;) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

// FreeRTOS Task: Update display on Core 1 (non-blocking) - SIMPLIFIED to avoid blocking
void displayTask(void *pvParameters) {
  char localLastMessage[MAX_MESSAGE_LEN];
  int localTotalMessages;
  static int lastDisplayedTotalMessages = 0;


  for (;;) {
    unsigned long now = millis();
    if (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
      lastDisplayUpdate = now;

      // Get last message from UDP handler without blocking long
      if (UDPHandler_tryCopyDisplay(localLastMessage, sizeof(localLastMessage), localTotalMessages)) {
        // ONLY update if we have a new message since last display
        if (localLastMessage[0] != '\0' && localTotalMessages > lastDisplayedTotalMessages) {
          unihiker.canvas->canvasClear();
          unihiker.canvas->canvasText("UDP Receiver", 1, 0xFFFFFF);
          unihiker.canvas->canvasText(String("Total: ") + String(localTotalMessages), 3, 0xFFFFFF);
          unihiker.canvas->canvasText("Last: " + String(localLastMessage), 2, 0xD0D0D0);
          
          unihiker.canvas->updateCanvas();
          lastDisplayedTotalMessages = localTotalMessages;
        } 
      }
    }

    vTaskDelay(250 / portTICK_PERIOD_MS); // Check display every 250ms
  }
}

// FreeRTOS Task: Handle web server on Core 1
void webServerTask(void *pvParameters) {
  for (;;) {
    WebServerModule_handleClient(&server);
    vTaskDelay(10 / portTICK_PERIOD_MS); // Small delay to prevent watchdog
  }
}





void setup() {
  // Small delay to ensure system stabilizes
  delay(1000);
  
  
  // Initialize the display
  unihiker.begin();
  unihiker.initScreen(1, 15);
  unihiker.creatCanvas();

  unihiker.canvas->canvasClear();
  unihiker.setScreenBackground( 0);
  unihiker.canvas->canvasText("Starting...", 1, 0xFFFFFF);
  unihiker.canvas->updateCanvas();
  delay(500);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  unihiker.canvas->canvasText("Connecting WiFi...", 2, 0xFFFFFF);
  unihiker.canvas->updateCanvas();
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
    unihiker.canvas->canvasText(String(ssid)+" (#" + String(attempts) + ")", 2, 0xFFFFFF);
    unihiker.canvas->updateCanvas();
  }
  
  unihiker.canvas->canvasClear();
  
  if (WiFi.status() == WL_CONNECTED) {
    unihiker.canvas->canvasText("WiFi Connected!", 1, 0x00FF00);
    unihiker.canvas->canvasText(WiFi.localIP().toString().c_str(), 2, 0xFFFFFF);
    
    // Start UDP listener - listen on UDP_PORT
    if (UDPHandler_begin(&udp, UDP_PORT)) {
      unihiker.canvas->canvasText("UDP:"+String(UDP_PORT), 3, 0x00FFFF);
    } else {
      unihiker.canvas->canvasText("NO UDP:"+String(UDP_PORT), 3, 0xFF0000);
    }
    
    // Start the web server
    WebServerModule_begin(&server);
    unihiker.canvas->canvasText("HTTP:"+String(WEB_PORT), 4, 0x00FF00);
    
    // Create FreeRTOS tasks
    // UDP task on Core 0 (priority 3) - handles async UDP listener - HIGHEST priority
    xTaskCreatePinnedToCore(udpTask, "UDP_Task", 2048, NULL, 3, NULL, 0);
    
    // Display task on Core 1 (priority 1) - MOVED to Core 1 to not block UDP
    xTaskCreatePinnedToCore(displayTask, "Display_Task", 4096, NULL, 1, NULL, 1);
    
    // Web server task on Core 1 (priority 2) - higher priority than display
    xTaskCreatePinnedToCore(webServerTask, "WebServer_Task", 4096, NULL, 2, NULL, 1);
    
    unihiker.canvas->canvasText("Tasks started", 5, 0x00FF00);
  } else {
    unihiker.canvas->canvasText("WiFi Failed.", 1, 0xFF0000);
  }
  
  unihiker.canvas->updateCanvas();
}

void loop() {
  // FreeRTOS tasks handle everything - main loop just idles
  delay(1000);
  
}