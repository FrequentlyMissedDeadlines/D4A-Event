#include "web_server.h"
#include "udp_handler.h"
#include "camera_handler.h"
#include "unihiker_k10_webcam.h"
#include <Arduino.h>
#include <esp_system.h>
#include <esp_camera.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <unihiker_k10.h>

// Forward declarations for external variables from main.cpp
extern String master_IP;
extern String master_TOKEN;
extern UNIHIKER_K10 unihiker;

// JSON Key Constants
static const char* JSON_STATUS = "status";
static const char* JSON_ERROR = "error";
static const char* JSON_OK = "ok";
static const char* JSON_REGISTERED = "registered";
static const char* JSON_UPTIME_MS = "uptime_ms";
static const char* JSON_MASTER_IP = "master_ip";
static const char* JSON_MASTER_TOKEN = "master_token";
static const char* JSON_HEAP_TOTAL = "heapTotal";
static const char* JSON_HEAP_USED = "heapUsed";
static const char* JSON_HEAP_FREE = "heapFree";
static const char* JSON_HEAP_USAGE_PERCENT = "heapUsagePercent";
static const char* JSON_UPTIME_SEC = "uptimeSec";
static const char* JSON_UPTIME_MIN = "uptimeMin";
static const char* JSON_UPTIME_HOUR = "uptimeHour";
static const char* JSON_MAC_ADDRESS = "macAddress";
static const char* JSON_WIFI_RSSI = "wifiRssi";
static const char* JSON_FREE_STACK_BYTES = "freeStackBytes";

// Pending master registration (when there's a conflict and waiting for user decision)
static String pendingMasterIP = "";
static String pendingMasterToken = "";
static volatile bool masterConflictPending = false;
static volatile bool masterConflictAccepted = false;
static unsigned long conflictStartTime = 0;
static const unsigned long CONFLICT_TIMEOUT_MS = 15000; // 15 second timeout

// Webcam instance for camera streaming
static unihiker_k10_webcam webcam;

void WebServerModule_begin(WebServer* server) {
  if (!server) {
    Serial.println("ERROR: WebServer pointer is NULL!");
    return;
  }

  Serial.println("WebServer Module Init");
  Serial.printf("Server instance: %p\n", (void*)server);

  // Root route - serve embedded index.html
  server->on("/", HTTP_GET, [server]() {
    Serial.println(">>> GET / - Serving HTML");
    
    // Embedded HTML content directly in program memory
    const char* html = R"rawliteral(<!DOCTYPE html>
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
        <div class="stat-value" id="totalCount">0</div>
      </div>
      <div class="stat-box">
        <div class="stat-label">Dropped Messages</div>
        <div class="stat-value" id="droppedCount">0</div>
      </div>
      <div class="stat-box">
        <div class="stat-label">Buffer Status</div>
        <div class="stat-value" id="bufferStatus">0/20</div>
      </div>
      <div class="stat-box">
        <div class="stat-label">Loss Rate</div>
        <div class="stat-value" id="lossRate">0%</div>
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
        <div class="stat-label">Heap Used</div>
        <div class="stat-value" id="heapUsed">0</div>
      </div>
      <div class="stat-box">
        <div class="stat-label">Heap Free</div>
        <div class="stat-value" id="heapFree">0</div>
      </div>
      <div class="stat-box">
        <div class="stat-label">Heap Usage %</div>
        <div class="stat-value" id="heapUsagePercent">0</div>
      </div>
      <div class="stat-box">
        <div class="stat-label">Uptime (ms)</div>
        <div class="stat-value" id="uptimeMs">0</div>
      </div>
      <div class="stat-box">
        <div class="stat-label">WiFi RSSI</div>
        <div class="stat-value" id="wifiRssi">0</div>
      </div>
      <div class="stat-box">
        <div class="stat-label">Free Stack (bytes)</div>
        <div class="stat-value" id="freeStackBytes">0</div>
      </div>
    </div>
  </div>
        <div class="refresh-info">
      ⟳ Auto-refreshing every 2 seconds
    </div>
  <script>
    async function updateSystemInfo() {
      try {
        const response = await fetch('/api/system');
        const data = await response.json();
        
        document.getElementById('heapTotal').textContent = data.heapTotal || "?";
        document.getElementById('heapUsed').textContent = data.heapUsed || "?";
        document.getElementById('heapFree').textContent = data.heapFree || "?";
        document.getElementById('heapUsagePercent').textContent = (data.heapUsagePercent || "?") + '%';
        document.getElementById('uptimeMs').textContent = data.uptimeMs || "?";
        document.getElementById('wifiRssi').textContent = data.wifiRssi || "?";
        document.getElementById('freeStackBytes').textContent = data.freeStackBytes || "?";
      } catch (error) {
        console.error('Error fetching system info:', error);
      }
    }
    
    async function updateMessages() {
      try {
        const response = await fetch('/api/messages');
        const data = await response.json();
        
        const total = data.total || 0;
        const dropped = data.dropped || 0;
        const lossRate = total + dropped > 0 ? ((dropped / (total + dropped)) * 100).toFixed(2) : 0;
        
        document.getElementById('totalCount').textContent = total;
        document.getElementById('droppedCount').textContent = dropped;
        document.getElementById('lossRate').textContent = lossRate + '%';
        document.getElementById('bufferStatus').textContent = data.buffer || '0/20';
        
        const container = document.getElementById('messagesContainer');
        if (data.messages && data.messages.length > 0) {
          container.innerHTML = data.messages.map(msg => `
            <div class="message">
              <div class="message-time">${msg}</div>
            </div>
          `).join('');
        } else {
          container.innerHTML = '<div class="message">No messages yet...</div>';
        }
        
        updateSystemInfo();
      } catch (error) {
        console.error('Error fetching messages:', error);
      }
    }
    
    // Update on page load
    updateMessages();
    
    // Auto-refresh every 2 seconds
    setInterval(updateMessages, 2000);
  </script>
</body>
</html>)rawliteral";
    
    server->send(200, "text/html", html);
  });

  // Simple status endpoint
  server->on("/status", HTTP_GET, [server]() {
    Serial.println(">>> GET /status - Returning status");
    String status = "{\"" + String(JSON_STATUS) + "\": \"" + String(JSON_OK) + "\", \"" + String(JSON_UPTIME_MS) + "\": " + String(millis()) + "}";
    server->send(200, "application/json", status);
  });

  // API endpoint for ESP32 system metrics
  server->on("/api/system", HTTP_GET, [server]() {
    Serial.println("GET /api/system");
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
    String json = "{";
    json += "\"" + String(JSON_HEAP_TOTAL) + "\":" + String(totalHeap) + ",";
    json += "\"" + String(JSON_HEAP_USED) + "\":" + String(usedHeap) + ",";
    json += "\"" + String(JSON_HEAP_FREE) + "\":" + String(freeHeap) + ",";
    json += "\"" + String(JSON_HEAP_USAGE_PERCENT) + "\":" + String(heapUsagePercent, 2) + ",";
    json += "\"" + String(JSON_UPTIME_MS) + "\":" + String(uptimeMs) + ",";
    json += "\"" + String(JSON_UPTIME_SEC) + "\":" + String(uptimeSec) + ",";
    json += "\"" + String(JSON_UPTIME_MIN) + "\":" + String(uptimeMin) + ",";
    json += "\"" + String(JSON_UPTIME_HOUR) + "\":" + String(uptimeHour) + ",";
    json += "\"" + String(JSON_MAC_ADDRESS) + "\":\"" + String(macStr) + "\",";
    json += "\"" + String(JSON_WIFI_RSSI) + "\":" + String(WiFi.RSSI()) + ",";
    json += "\"" + String(JSON_FREE_STACK_BYTES) + "\":" + String(freeStackCore0);
    json += "}";
    
    server->send(200, "application/json", json);
  });

  // API endpoint for JSON data - returns all UDP messages and statistics
  server->on("/api/messages", HTTP_GET, [server]() {
    Serial.println("GET /api/messages");
    String json = UDPHandler_buildJson();
    server->send(200, "application/json", json);
  });

  // API endpoint for master registration - PUT to set master IP and generate token
  server->on("/api/master", HTTP_PUT, [server]() {
    Serial.println("PUT /api/master");
    
    // Check if master_IP is already set
    if (master_IP.length() > 0) {
      // Master already registered - show conflict dialog on display
      Serial.println("Master already registered - showing conflict dialog");
      
      // Get the client IP address from the request
      IPAddress clientIP = server->client().remoteIP();
      pendingMasterIP = clientIP.toString();
      
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
      
      pendingMasterToken = String(tokenBuffer);
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
        
        String json = "{\"" + String(JSON_STATUS) + "\": \"" + String(JSON_REGISTERED) + "\", \"" + String(JSON_MASTER_IP) + "\": \"" + master_IP + "\", \"" + String(JSON_MASTER_TOKEN) + "\": \"" + master_TOKEN + "\"}";
        server->send(200, "application/json", json);
      } else {
        
        masterConflictPending = false;
        
        // Deny new master
        String json = "{\"" + String(JSON_ERROR) + "\": \"New master registration denied\", \"" + String(JSON_MASTER_IP) + "\": \"" + master_IP + "\", \"" + String(JSON_MASTER_TOKEN) + "\": \"\"}";
        server->send(403, "application/json", json);
      }
      return;
    }
    
    // No conflict - register the new master
    IPAddress clientIP = server->client().remoteIP();
    master_IP = clientIP.toString();
    
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
    
    master_TOKEN = String(tokenBuffer);
    
    Serial.printf("Master registered - IP: %s, Token: %s\n", master_IP.c_str(), master_TOKEN.c_str());
    
    // Return the token and master IP
    String json = "{\"" + String(JSON_STATUS) + "\": \"" + String(JSON_REGISTERED) + "\", \"" + String(JSON_MASTER_IP) + "\": \"" + master_IP + "\", \"" + String(JSON_MASTER_TOKEN) + "\": \"" + master_TOKEN + "\"}";
    server->send(200, "application/json", json);
  });

  // Camera status endpoint - using who_camera API
  server->on("/api/camera/status", HTTP_GET, [server]() {
    Serial.println("GET /api/camera/status");
    
    // Note: This endpoint is ready for camera integration
    // Camera handler will be initialized in main.cpp
    String json = "{\"status\": \"camera_module_available\", \"note\": \"Use who_camera API only\"}";
    server->send(200, "application/json", json);
  });

  // Camera snapshot endpoint - returns frame from who_camera
  // Note: who_camera on K10 provides RGB565 format, not JPEG
  server->on("/camera/snapshot.ppm", HTTP_GET, [server]() {
    Serial.println("GET /camera/snapshot.ppm");
    
    if (!CameraHandler_isRunning()) {
      Serial.println("  Camera not running");
      server->send(503, "text/plain", "Camera not available");
      return;
    }
    
    QueueHandle_t frameQueue = CameraHandler_getFrameQueue();
    if (!frameQueue) {
      Serial.println("  Frame queue not initialized");
      server->send(503, "text/plain", "Camera not initialized");
      return;
    }
    
    // Wait up to 1 second for a frame from who_camera
    // The queue contains camera_fb_t* pointers
    camera_fb_t* fb = NULL;
    if (xQueueReceive(frameQueue, &fb, 1000 / portTICK_PERIOD_MS)) {
      if (fb != NULL && fb->len > 0) {
        // Frame received - debug output
        Serial.printf("  Frame received: %d bytes, %dx%d, format=%d\n", 
                     fb->len, fb->width, fb->height, fb->format);
        
        // DEBUG: Show first 32 bytes in hex to analyze format
        Serial.print("  First 32 bytes (hex): ");
        uint8_t* debug_buf = (uint8_t*)fb->buf;
        for (int i = 0; i < 32 && i < fb->len; i++) {
          Serial.printf("%02x ", debug_buf[i]);
        }
        Serial.println();
        
        // Show first 8 pixels as 16-bit values
        Serial.print("  First 8 pixels (16-bit LE): ");
        for (int i = 0; i < 8; i++) {
          uint16_t val = debug_buf[i*2] | ((uint16_t)debug_buf[i*2+1] << 8);
          Serial.printf("%04x ", val);
        }
        Serial.println();
        
        // K10 camera provides RGB565 format (not JPEG)
        // Send as PPM format (simple, uncompressed, but any viewer can read it)
        // PPM format: ASCII header + raw RGB24 data
        
        // Calculate PPM file size: header + (3 bytes per pixel)
        int ppm_data_size = fb->width * fb->height * 3;
        
        // Create PPM header
        char ppm_header[100];
        int header_len = snprintf(ppm_header, sizeof(ppm_header),
                                  "P6\n%d %d\n255\n",
                                  fb->width, fb->height);
        
        int total_size = header_len + ppm_data_size;
        
        Serial.printf("  Sending PPM: %d bytes (header=%d, data=%d)\n", 
                     total_size, header_len, ppm_data_size);
        
        // Send HTTP headers
        server->setContentLength(total_size);
        server->sendHeader("Content-Type", "image/x-portable-pixmap");
        server->sendHeader("X-Frame-Width", String(fb->width));
        server->sendHeader("X-Frame-Height", String(fb->height));
        server->send(200);
        
        // Send PPM header
        server->client().write((const uint8_t*)ppm_header, header_len);
        
        // Convert RGB565 to RGB24 and send
        // RGB565: RRRRRGGGGGGBBBBB (5-6-5 bits)
        // Use smaller buffer to avoid stack overflow (128 pixels = 384 bytes)
        
        uint8_t* rgb565_buf = (uint8_t*)fb->buf;
        uint8_t rgb_buffer[384];  // 128 pixels * 3 bytes = 384 bytes (safe)
        int buf_idx = 0;
        int pixels_buffered = 0;
        
        for (int i = 0; i < fb->width * fb->height; i++) {
            // Read RGB565 value (2 bytes, little-endian from camera)
            // Little-endian: byte[0] is LSB, byte[1] is MSB
            uint8_t lo = rgb565_buf[i*2];
            uint8_t hi = rgb565_buf[i*2+1];
            uint16_t rgb565 = lo | ((uint16_t)hi << 8);
            
            // Standard RGB565 format: RRRRRGGGGGGBBBBB
            // Bits 15-11: Red (5 bits)
            // Bits 10-5: Green (6 bits)
            // Bits 4-0: Blue (5 bits)
            uint8_t r5 = (rgb565 >> 11) & 0x1F;  // 5 bits
            uint8_t g6 = (rgb565 >> 5) & 0x3F;   // 6 bits
            uint8_t b5 = rgb565 & 0x1F;          // 5 bits
            
            // Scale to 8-bit: replicate MSBs to LSBs for better color precision
            // 5-bit to 8-bit: shift left 3 bits, then copy top 3 bits to bottom
            // 6-bit to 8-bit: shift left 2 bits, then copy top 2 bits to bottom
            uint8_t r8 = (r5 << 3) | (r5 >> 2);  // Scale 5-bit red to 8-bit
            uint8_t g8 = (g6 << 2) | (g6 >> 4);  // Scale 6-bit green to 8-bit
            uint8_t b8 = (b5 << 3) | (b5 >> 2);  // Scale 5-bit blue to 8-bit
            
            // Output as RGB24 (PPM format expects RGB order)
            rgb_buffer[buf_idx++] = r8;
            rgb_buffer[buf_idx++] = g8;
            rgb_buffer[buf_idx++] = b8;
            
            pixels_buffered++;
            
            // Flush buffer after 128 pixels
            if (pixels_buffered >= 128) {
                server->client().write(rgb_buffer, buf_idx);
                buf_idx = 0;
                pixels_buffered = 0;
            }
        }
        
        // Flush any remaining data
        if (buf_idx > 0) {
            server->client().write(rgb_buffer, buf_idx);
        }
        
        // Return the frame buffer to who_camera for reuse
        // CRITICAL: This must be done or who_camera will run out of buffers
        esp_camera_fb_return(fb);
        Serial.println("  PPM sent successfully");
      } else {
        Serial.println("  Invalid frame received");
        server->send(500, "text/plain", "Invalid camera frame");
      }
    } else {
      Serial.println("  Timeout waiting for frame");
      server->send(504, "text/plain", "No frame available (timeout)");
    }
  });

  // Camera raw RGB565 snapshot - returns raw frame data without conversion
  // This endpoint just sends the raw RGB565 bytes so we can debug the format
  server->on("/camera/snapshot", HTTP_GET, [server]() {
    Serial.println("GET /camera/snapshot (raw RGB565)");
    
    if (!CameraHandler_isRunning()) {
      Serial.println("  Camera not running");
      server->send(503, "text/plain", "Camera not available");
      return;
    }
    
    QueueHandle_t frameQueue = CameraHandler_getFrameQueue();
    if (!frameQueue) {
      Serial.println("  Frame queue not initialized");
      server->send(503, "text/plain", "Camera not initialized");
      return;
    }
    
    // Wait for frame
    camera_fb_t* fb = NULL;
    if (xQueueReceive(frameQueue, &fb, 1000 / portTICK_PERIOD_MS)) {
      if (fb != NULL && fb->len > 0) {
        Serial.printf("  Frame: %d bytes, %dx%d, format=%d\n", 
                     fb->len, fb->width, fb->height, fb->format);
        
        // Send raw RGB565 data with metadata headers
        server->setContentLength(fb->len);
        server->sendHeader("Content-Type", "application/octet-stream");
        server->sendHeader("X-Frame-Format", "RGB565");
        server->sendHeader("X-Frame-Width", String(fb->width));
        server->sendHeader("X-Frame-Height", String(fb->height));
        server->sendHeader("X-Frame-Bytes", String(fb->len));
        server->send(200);
        
        // Send raw frame data
        server->client().write((const uint8_t*)fb->buf, fb->len);
        
        // Return buffer
        esp_camera_fb_return(fb);
        Serial.println("  Raw RGB565 sent");
      } else {
        Serial.println("  Invalid frame");
        server->send(500, "text/plain", "Invalid frame");
      }
    } else {
      Serial.println("  Timeout");
      server->send(504, "text/plain", "Timeout");
    }
  });

  // 404 handler
  server->onNotFound([server]() {
    Serial.printf("404 Not Found: %s\n", server->uri().c_str());
    server->send(404, "text/plain", "Not Found");
  });

  Serial.println("Starting WebServer on port 80");
  server->begin();
  Serial.println("WebServer started successfully");
}

void WebServerModule_handleClient(WebServer* server) {
  if (server) {
    server->handleClient();
  }
}

// Handle master registration conflict - called by display task
void WebServerModule_handleMasterConflict(void) {
  if (!masterConflictPending) {
    return; // No conflict to handle
  }
  
  unsigned long elapsedMs = millis() - conflictStartTime;
  
  // Display conflict dialog on K10 screen
  unihiker.canvas->canvasClear();
  unihiker.canvas->canvasText("Master Conflict!", 1, 0xFFFF00); // Yellow
  unihiker.canvas->canvasText("Replace " + master_IP, 2, 0xFFFFFF);
  unihiker.canvas->canvasText("with " + pendingMasterIP + "?", 3, 0xFFFFFF);
  unihiker.buttonA->setPressedCallback(WebServerModule_acceptMasterConflict);
  unihiker.buttonB->setPressedCallback(WebServerModule_denyMasterConflict);

  unihiker.canvas->canvasText("A=Yes  B=No", 5, 0x00FF00); // Green
  
  // Show timeout countdown
  unsigned long remainingMs = (elapsedMs < CONFLICT_TIMEOUT_MS) ? (CONFLICT_TIMEOUT_MS - elapsedMs) : 0;
  unsigned long remainingSec = remainingMs / 1000;
  unihiker.canvas->canvasText("(" + String(remainingSec) + "s)", 6, 0xFF8800); // Orange
  
  unihiker.canvas->updateCanvas();
  
  // Check for button presses - the K10 should send key events
  // This will be handled by the main task or key handler
  // For now, we rely on external code to call:
  // WebServerModule_acceptMasterConflict() for button A
  // WebServerModule_denyMasterConflict() for button 
}

// Call this when button A is pressed to accept new master
void WebServerModule_acceptMasterConflict(void) {
  if (masterConflictPending) {
    Serial.println("Button A pressed - accepting new master");
    masterConflictAccepted = true;
    masterConflictPending = false;
  }
}

// Call this when button B is pressed to deny new master
void WebServerModule_denyMasterConflict(void) {
  if (masterConflictPending) {
    Serial.println("Button B pressed - denying new master");
    masterConflictAccepted = false;
    masterConflictPending = false;
  }
}

// Camera webcam registration - registers video streaming routes
void WebServerModule_registerWebcam(WebServer* server) {
  if (!server) {
    Serial.println("ERROR: Cannot register webcam - WebServer pointer is NULL!");
    return;
  }
  
  Serial.println("Registering webcam routes with WebServer...");
  
  if (webcam.enableWebcam(server)) {
    Serial.println("Webcam routes registered successfully!");
    Serial.println("  - /video (camera index page)");
    Serial.println("  - /video/stream (MJPEG stream)");
    Serial.println("  - /video/capture (single frame capture)");
  } else {
    Serial.println("ERROR: Failed to register webcam routes!");
  }
}
