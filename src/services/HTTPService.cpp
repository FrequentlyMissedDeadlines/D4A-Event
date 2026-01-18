#include "HTTPService.h"
#include <Arduino.h>
#include <esp_system.h>
#include <esp_camera.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <unihiker_k10.h>
#include <pgmspace.h>
#include "../camera/K10Webcam.h"
#include "../services/LoggerService.h"


// Forward declarations for external variables from main.cpp
std::string master_IP;
std::string master_TOKEN;

extern UNIHIKER_K10 unihiker;

// JSON Key Constants
static const std::string JSON_STATUS = "status";
static const std::string JSON_ERROR = "error";
static const std::string JSON_OK = "ok";
static const std::string JSON_REGISTERED = "registered";
static const std::string JSON_UPTIME_MS = "uptime_ms";
static const std::string JSON_MASTER_IP = "master_ip";
static const std::string JSON_MASTER_TOKEN = "master_token";
static const std::string JSON_HEAP_TOTAL = "heapTotal";
static const std::string JSON_HEAP_USED = "heapUsed";
static const std::string JSON_HEAP_FREE = "heapFree";
static const std::string JSON_HEAP_USAGE_PERCENT = "heapUsagePercent";
static const std::string JSON_UPTIME_SEC = "uptimeSec";
static const std::string JSON_UPTIME_MIN = "uptimeMin";
static const std::string JSON_UPTIME_HOUR = "uptimeHour";
static const std::string JSON_MAC_ADDRESS = "macAddress";
static const std::string JSON_WIFI_RSSI = "wifiRssi";
static const std::string JSON_FREE_STACK_BYTES = "freeStackBytes";

// Pending master registration (when there's a conflict and waiting for user decision)
static std::string pendingMasterIP = "";
static std::string pendingMasterToken = "";
static volatile bool masterConflictPending = false;
static volatile bool masterConflictAccepted = false;
static unsigned long conflictStartTime = 0;
static const unsigned long CONFLICT_TIMEOUT_MS = 15000; // 15 second timeout

static const char WEB_ROOT_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html>
<head>
  <title>K10 UDP Receiver</title>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    body {
      font-family: Arial, sans-serif;
      background-color: #1e1e1e;
      color: #e0e0e0;
      margin: 20px;
      padding: 20px;
    }
    h1 {
      color: #00d4ff;
      text-align: center;
    }
    .container {
      max-width: 800px;
      margin: 0 auto;
      background-color: #2d2d2d;
      padding: 20px;
      border-radius: 8px;
      box-shadow: 0 4px 6px rgba(0, 0, 0, 0.3);
    }
    .stats {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 20px;
      margin: 20px 0;
    }
    .stat-box {
      background-color: #3d3d3d;
      padding: 15px;
      border-left: 4px solid #00d4ff;
      border-radius: 4px;
    }
    .stat-label {
      font-size: 12px;
      color: #888;
      text-transform: uppercase;
    }
    .stat-value {
      font-size: 28px;
      font-weight: bold;
      color: #00ff00;
    }
    .messages {
      background-color: #3d3d3d;
      padding: 15px;
      border-radius: 4px;
      margin-top: 20px;
      max-height: 400px;
      overflow-y: auto;
    }
    .message {
      background-color: #d0d0d0;
      padding: 10px;
      margin: 8px 0;
      border-left: 3px solid #00ff00;
      font-family: 'Courier New', monospace;
      font-size: 12px;
      word-break: break-all;
    }
    .message-time {
      color: #888;
      font-size: 10px;
      margin-bottom: 4px;
    }
    .refresh-info {
      text-align: center;
      color: #666;
      font-size: 12px;
      margin-top: 20px;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>📡 UDP Receiver</h1>
    
    <div class="stats">
      <div class="stat-box">
        <div class="stat-label">Total Messages</div>
        <div class="stat-value" id="total">0</div>
      </div>
      <div class="stat-box">
        <div class="stat-label">Dropped Messages</div>
        <div class="stat-value" id="dropped">0</div>
      </div>
      <div class="stat-box">
        <div class="stat-label">Buffer Status</div>
        <div class="stat-value" id="buffer">0/20</div>
      </div>
    </div>
    
    <div class="messages" id="messagesContainer">
      <div class="message">Waiting for messages...</div>
    </div>
    

  </div>
  
  <div class="container">
    <h1>🧰 Board Status</h1>
    <div class="stats">
      <div class="stat-box">
        <div class="stat-label">Heap Total</div>
        <div class="stat-value" id="heapTotal">0</div>
      </div>

      <div class="stat-box">
        <div class="stat-label">Heap Free</div>
        <div class="stat-value" id="heapFree">0</div>
      </div>
      <div class="stat-box">
        <div class="stat-label">Uptime (ms)</div>
        <div class="stat-value" id="uptimeMs">0</div>
      </div>
      <div class="stat-box">
        <div class="stat-label">Free Stack (bytes)</div>
        <div class="stat-value" id="freeStackBytes">0</div>
      </div>

    </div>
  </div>
  
  <div class="container">
    <h1>⚙️ Controls</h1>
    <button onclick="fetchStats()">Refresh Stats</button>
    <button onclick="fetchUdpMessages()">Refresh Messages</button>
  </div>
  
  <script>
    async function fetchStats() {
      try {
        const response = await fetch('/api/board');
        const data = await response.json();

          document.getElementById('heapTotal').textContent = data.heapTotal;
          document.getElemeclass HTTPService : public HasLoggerInterfacentById('heapFree').textContent = data.heapFree;
          document.getElementById('uptimeMs').textContent = data.uptimeMs;
          document.getElementById('freeStackBytes').textContent = data.freeStackBytes;
      } catch (error) {
        console.error('Failed to fetch stats:', error);
      }
    }

    async function fetchUdpMessages() {
      try {
        const response = await fetch('/api/messages');
        const data = await response.json();
        const container = document.getElementById('messagesContainer');
        container.innerHTML = '';
        data.messages.forEach(msg => {
          const div = document.createElement('div');
          div.className = 'message';
          div.innerHTML = `<div class="message">${msg}</div>`;
          container.appendChild(div);
        });
        document.getElementById('total').textContent = data.total;
        document.getElementById('dropped').textContent = data.dropped;
        document.getElementById('buffer').textContent = `${data.buffer}`;
      } catch (error) {
        console.error('Failed to fetch UDP messages:', error);
      }
    }

    fetchStats();
    fetchUdpMessages();
    setInterval(fetchStats, 5000);
    setInterval(fetchUdpMessages, 3000);
  </script>
</body>
</html>)rawliteral";





bool HTTPService::init() {return true; }
bool HTTPService::start() {return true; }
bool HTTPService::stop() {return true; }

bool HTTPService::begin(WebServer *server)
{
  if (!server)
  {

    return false;
  }



  // Root route - serve embedded index.html
  server->on("/", HTTP_GET, [server]()
             {

    server->send_P(200, "text/html", WEB_ROOT_HTML); });

  // Simple status endpoint
  server->on("/status", HTTP_GET, [server]() {

    std::string status = "{\"" + JSON_STATUS + "\": \"" + JSON_OK + "\", \"" + JSON_UPTIME_MS + "\": " + std::to_string(millis()) + "}";
    server->send(200, "application/json", status.c_str()); });

  // API endpoint for ESP32 system metrics
  server->on("/api/system", HTTP_GET, [server]()
             {

    // Get ESP32 chip information
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t totalHeap = ESP.getHeapSize();
    uint32_t usedHeap = totalHeap - freeHeap;
    float heapUsagePercent = (float)usedHeap / totalHeap * 100.0;
    
    // Get uptime
    uint32_t uptimeMs = millis();
    uint32_t uptimeSec = uptimeMs / 1000;
    uint32_t uptimeMin = uptimeSec / 60;
    uint32_t uptimeHour = uptimeMin / 60;
    
    // Get WiFi information
    uint8_t macAddr[6];
    WiFi.macAddress(macAddr);
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
             macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
    
    // Get task information
    uint32_t freeStackCore0 = uxTaskGetStackHighWaterMark(NULL);
    
    // Build JSON response
    std::string json = "{";
    json += "\"" + JSON_HEAP_TOTAL + "\":" + std::to_string(totalHeap) + ",";
    json += "\"" + JSON_HEAP_USED + "\":" + std::to_string(usedHeap) + ",";
    json += "\"" + JSON_HEAP_FREE + "\":" + std::to_string(freeHeap) + ",";
    json += "\"" + JSON_HEAP_USAGE_PERCENT + "\":" + std::to_string(heapUsagePercent) + ",";
    json += "\"" + JSON_UPTIME_MS + "\":" + std::to_string(uptimeMs) + ",";
    json += "\"" + JSON_UPTIME_SEC + "\":" + std::to_string(uptimeSec) + ",";
    json += "\"" + JSON_UPTIME_MIN + "\":" + std::to_string(uptimeMin) + ",";
    json += "\"" + JSON_UPTIME_HOUR + "\":" + std::to_string(uptimeHour) + ",";
    json += "\"" + JSON_MAC_ADDRESS + "\":\"" + std::string(macStr) + "\",";
    json += "\"" + JSON_WIFI_RSSI + "\":" + std::to_string(WiFi.RSSI()) + ",";
    json += "\"" + JSON_FREE_STACK_BYTES + "\":" + std::to_string(freeStackCore0);
    json += "}";
    
    server->send(200, "application/json", json.c_str()); });

  // API endpoint for JSON data - returns all UDP messages and statistics
  

  // API endpoint for master registration - PUT to set master IP and generate token
  server->on("/api/master", HTTP_PUT, [server]()
             {

    
    // Check if master_IP is already set
    if (master_IP.length() > 0) {
      // Master already registered - show conflict dialog on display

      
      // Get the client IP address from the request
      IPAddress clientIP = server->client().remoteIP();
      pendingMasterIP = clientIP.toString().c_str();
      
      // Generate token for the pending master
      uint8_t macAddr[6];
      WiFi.macAddress(macAddr);
      uint32_t randomValue = random(1000000, 9999999);
      uint32_t timestamp = millis();
      
      char tokenBuffer[64];
      snprintf(tokenBuffer, sizeof(tokenBuffer), 
               "%02x%02x%02x%02x-%lu-%lu",
               macAddr[3], macAddr[4], macAddr[5], 
               (randomValue >> 8) & 0xFF,
               randomValue, timestamp);
      
      pendingMasterToken = tokenBuffer;
      masterConflictPending = true;
      masterConflictAccepted = false;
      conflictStartTime = millis();
      
      // Keep the connection open and wait for conflict resolution
      // Timeout after 2 seconds with default (deny) response
      while (masterConflictPending && (millis() - conflictStartTime < CONFLICT_TIMEOUT_MS)) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
      }
      
      // Timeout occurred - ensure masterConflictPending is cleared
      if (masterConflictPending) {
        masterConflictPending = false;
        masterConflictAccepted = false;
      }
      
      // Check if user accepted or timeout occurred
      if (masterConflictAccepted) {
        // Accept new master
        master_IP = pendingMasterIP;
        master_TOKEN = pendingMasterToken;
        masterConflictPending = false;
        
        std::string json = "{\"" + JSON_STATUS + "\": \"" + JSON_REGISTERED + "\", \"" + JSON_MASTER_IP + "\": \"" + master_IP + "\", \"" + JSON_MASTER_TOKEN + "\": \"" + master_TOKEN + "\"}";
          server->send(200, "application/json", json.c_str());  
      } else {
        
        masterConflictPending = false;
        
        // Deny new master
        std::string json = "{\"" + JSON_ERROR + "\": \"New master registration denied\", \"" + JSON_MASTER_IP + "\": \"" + master_IP + "\", \"" + JSON_MASTER_TOKEN + "\": \"\"}";
        server->send(403, "application/json", json.c_str());
      }
      return;
    }
    
    // No conflict - register the new master
    IPAddress clientIP = server->client().remoteIP();
    master_IP = clientIP.toString().c_str() ;
    
    // Generate a new master token - using MAC address + random value + timestamp
    uint8_t macAddr[6];
    WiFi.macAddress(macAddr);
    uint32_t randomValue = random(1000000, 9999999);
    uint32_t timestamp = millis();
    
    char tokenBuffer[64];
    snprintf(tokenBuffer, sizeof(tokenBuffer), 
             "%02x%02x%02x%02x-%lu-%lu",
             macAddr[3], macAddr[4], macAddr[5], 
             (randomValue >> 8) & 0xFF,
             randomValue, timestamp);
    
    master_TOKEN = tokenBuffer;
    
    Serial.printf("Master registered - IP: %s, Token: %s\n", master_IP.c_str(), master_TOKEN.c_str());
    
    // Return the token and master IP
    std::string json = "{\"" + JSON_STATUS + "\": \"" + JSON_REGISTERED + "\", \"" + JSON_MASTER_IP + "\": \"" + master_IP + "\", \"" + JSON_MASTER_TOKEN + "\": \"" + master_TOKEN + "\"}";
    server->send(200, "application/json", json.c_str()); });


  // 404 handler
  server->onNotFound([server]()
                     {
    Serial.printf("404 Not Found: %s\n", server->uri().c_str());
    server->send(404, "text/plain", "Not Found"); });


  server->begin();
  return true;
}

void HTTPService::handleClient(WebServer *server)
{
  if (server)
  {
    server->handleClient();
  }
}

// Static callback wrappers for button callbacks
static void acceptMasterConflictCallback(void)
{
  if (masterConflictPending)
  {

    masterConflictAccepted = true;
    masterConflictPending = false;
  }
}

static void denyMasterConflictCallback(void)
{
  if (masterConflictPending)
  {

    masterConflictAccepted = false;
    masterConflictPending = false;
  }
}

// Handle master registration conflict - called by display task
void HTTPService::handleMasterConflict(void)
{
  if (!masterConflictPending)
  {
    return; // No conflict to handle
  }

  unsigned long elapsedMs = millis() - conflictStartTime;

  // Display conflict dialog on K10 screen
  
  logger->info("Current Master IP: " + master_IP);
  unihiker.buttonA->setPressedCallback(acceptMasterConflictCallback);
  unihiker.buttonB->setPressedCallback(denyMasterConflictCallback);

  logger->info("New Master: " + pendingMasterIP + " ? (A=accept  B=deny)");




  // Show timeout countdown
  unsigned long remainingMs = (elapsedMs < CONFLICT_TIMEOUT_MS) ? (CONFLICT_TIMEOUT_MS - elapsedMs) : 0;
  unsigned long remainingSec = remainingMs / 1000;


  // Check for button presses - the K10 should send key events
  // This will be handled by the main task or key handler
  // For now, we rely on external code to call:
  // WebServerModule_acceptMasterConflict() for button A
  // WebServerModule_denyMasterConflict() for button
}

// Call this when button A is pressed to accept new master
void HTTPService::acceptMasterConflict(void)
{
  acceptMasterConflictCallback();
}

// Call this when button B is pressed to deny new master
void HTTPService::denyMasterConflict(void)
{
  denyMasterConflictCallback();
}