// VERSION: UDP message receiver with web display (FreeRTOS dual-core)
// Description: Core 0 handles UDP, Core 1 handles web server for parallel processing

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <AsyncUDP.h>
#include <unihiker_k10.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

const char* ssid = "Freebox-A35871";
const char* password = "azertQSDF1234";

UNIHIKER_K10 unihiker;
WebServer server(80);
AsyncUDP udp;
const int UDP_PORT = 12345;

// Message buffer to store last 20 messages
const int MAX_MESSAGES = 20;
const int MAX_MESSAGE_LEN = 256;
char messages[MAX_MESSAGES][MAX_MESSAGE_LEN];
int messageCount = 0;  // Number of messages in the buffer (0-20)
int messageIndex = 0;  // Write position in circular buffer
int totalMessages = 0; // Total messages received ever
unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_UPDATE_INTERVAL = 2500; // Update display every 500ms

// Separate buffer for display (non-blocking)
char lastMessage[MAX_MESSAGE_LEN] = "";
int displayTotalMessages = 0;

// Timing for delta calculation
unsigned long lastMessageTime = 0;

// Mutex for thread-safe access to message buffer (non-blocking version)
SemaphoreHandle_t messageMutex = NULL;

// AsyncUDP packet handler callback - MUST BE NON-BLOCKING
void handleUDPPacket(AsyncUDPPacket packet) {
  if (packet.length() > 0) {
    // Get the data
    char buffer[MAX_MESSAGE_LEN];
    int len = packet.length();
    if (len >= MAX_MESSAGE_LEN) len = MAX_MESSAGE_LEN - 1;
    
    memcpy(buffer, packet.data(), len);
    buffer[len] = '\0';
    
    // Use non-blocking mutex take - if can't get lock, skip
    if (xSemaphoreTake(messageMutex, 0)) {
      // Calculate milliseconds since last message (INSIDE mutex)
      unsigned long currentTime = millis();
      unsigned long deltaMs;
      

      deltaMs = currentTime - lastMessageTime;
      lastMessageTime = currentTime;

      
      
      // Create message with delta prefix
      char fullMessage[MAX_MESSAGE_LEN];
      snprintf(fullMessage, MAX_MESSAGE_LEN, "[%lu ms] %s", deltaMs, buffer);
      
      // Store message in circular buffer (FIFO)
      strncpy(messages[messageIndex], fullMessage, MAX_MESSAGE_LEN - 1);
      messages[messageIndex][MAX_MESSAGE_LEN - 1] = '\0';
      messageIndex = (messageIndex + 1) % MAX_MESSAGES;
      
      // Increment total count
      totalMessages++;
      displayTotalMessages = totalMessages;
      
      // Increment buffer count until we reach max
      if (messageCount < MAX_MESSAGES) {
        messageCount++;
      }
      
      // Copy to display buffer (for non-blocking display update)
      strncpy(lastMessage, buffer, MAX_MESSAGE_LEN - 1);
      lastMessage[MAX_MESSAGE_LEN - 1] = '\0';
      
      xSemaphoreGive(messageMutex);
    }
  }
}

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

  for (;;) {
    unsigned long now = millis();
    if (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
      lastDisplayUpdate = now;
      
      // Safely copy the data for display
      if (xSemaphoreTake(messageMutex, 10 / portTICK_PERIOD_MS)) {
        strncpy(localLastMessage, lastMessage, MAX_MESSAGE_LEN);
        localTotalMessages = displayTotalMessages;
        xSemaphoreGive(messageMutex);
      } else {
        // If we can't get the lock, just skip this update
        continue;
      }

      // ONLY update if we have a new message (flag-based, super fast)
      // Skip most updates to avoid blocking Core 0
      if (localLastMessage[0] != '\0') {
        unihiker.canvas->canvasClear();
        unihiker.canvas->canvasText("K10 Receiver", 1, 0xFFFFFF);
        unihiker.canvas->canvasText(localLastMessage, 2, 0x00FF00);
        unihiker.canvas->canvasText(String("Total: ") + String(localTotalMessages), 3, 0xFFFFFF);
        unihiker.canvas->updateCanvas();
      }
    }
    
    vTaskDelay(100 / portTICK_PERIOD_MS); // Check display every 100ms (was 50ms)
  }
}

// FreeRTOS Task: Handle web server on Core 1
void webServerTask(void *pvParameters) {
  for (;;) {
    server.handleClient();
    vTaskDelay(10 / portTICK_PERIOD_MS); // Small delay to prevent watchdog
  }
}

// Handle any other HTTP request
void handleRequest() {
  server.send(200, "text/plain", "ok");
}

// Handle web page request - show last 20 messages
void handleWebPage() {
  String html = "<html><head><meta http-equiv=\"Refresh\" content=\"2\"><title>K10 Messages</title><style>";
  html += "body { font-family: Arial; margin: 20px; background-color: #f0f0f0; }";
  html += "h1 { color: #333; }";
  html += ".message { background-color: white; padding: 10px; margin: 5px 0; border-left: 4px solid #4CAF50; }";
  html += "</style></head><body>";
  html += "<h1>K10 UDP Messages</h1>";
  
  // Try to lock mutex with short timeout instead of blocking forever
  if (xSemaphoreTake(messageMutex, 100 / portTICK_PERIOD_MS)) {
    html += "<p>Total messages received: " + String(totalMessages) + "</p>";
    html += "<p>Messages in buffer: " + String(messageCount) + "/" + String(MAX_MESSAGES) + "</p>";
    html += "<div>";
    
    // Display messages in reverse order (newest first)
    for (int i = 0; i < (messageCount < MAX_MESSAGES ? messageCount : MAX_MESSAGES); i++) {
      int idx = (messageIndex - 1 - i + MAX_MESSAGES) % MAX_MESSAGES;
      if (messages[idx][0] != '\0') {
        html += "<div class='message'>";
        html += messages[idx];
        html += "</div>";
      }
    }
    
    html += "</div>";
    xSemaphoreGive(messageMutex);
  } else {
    html += "<p>Server busy (buffer locked), please try again</p>";
  }
  
  html += "</body></html>";
  server.send(200, "text/html", html);
}



void setup() {
  // Small delay to ensure system stabilizes
  delay(1000);
  
  // Create mutex for message buffer
  messageMutex = xSemaphoreCreateMutex();
  
  // Initialize the display
  unihiker.begin();
  unihiker.initScreen(2, 30);
  unihiker.creatCanvas();

  unihiker.canvas->canvasClear();
  unihiker.setScreenBackground( 0);
  unihiker.canvas->canvasText("K10 Starting...", 1, 0xFFFFFF);
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
  }
  
  unihiker.canvas->canvasClear();
  
  if (WiFi.status() == WL_CONNECTED) {
    unihiker.canvas->canvasText("WiFi Connected!", 1, 0x00FF00);
    unihiker.canvas->canvasText(WiFi.localIP().toString().c_str(), 2, 0xFFFFFF);
    
    // Start UDP listener - listen on UDP_PORT
    if (udp.listen(UDP_PORT)) {
      unihiker.canvas->canvasText("UDP listening...", 3, 0x00FFFF);
      // Attach callback handler
      udp.onPacket(handleUDPPacket);
    }
    
    // Start the web server
    server.on("/", handleWebPage);
    server.onNotFound(handleRequest);
    server.begin();
    unihiker.canvas->canvasText("Server started", 4, 0x00FF00);
    
    // Create FreeRTOS tasks
    // UDP task on Core 0 (priority 3) - handles async UDP listener - HIGHEST priority
    xTaskCreatePinnedToCore(udpTask, "UDP_Task", 2048, NULL, 3, NULL, 0);
    
    // Display task on Core 1 (priority 1) - MOVED to Core 1 to not block UDP
    xTaskCreatePinnedToCore(displayTask, "Display_Task", 4096, NULL, 1, NULL, 1);
    
    // Web server task on Core 1 (priority 2) - higher priority than display
    xTaskCreatePinnedToCore(webServerTask, "WebServer_Task", 4096, NULL, 2, NULL, 1);
    
    unihiker.canvas->canvasText("Tasks started", 5, 0x00FF00);
  } else {
    unihiker.canvas->canvasText("WiFi Failed!", 1, 0xFF0000);
  }
  
  unihiker.canvas->updateCanvas();
}

void loop() {
  // FreeRTOS tasks handle everything - main loop just idles
  delay(1000);
}