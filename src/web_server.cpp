#include "web_server.h"
#include "udp_handler.h"
#include <Arduino.h>

void WebServerModule_begin(WebServer* server) {
  if (!server) return;

  // Root route - HTML page showing UDP messages
  server->on("/", HTTP_GET, [server]() {
    String html = R"(
<!DOCTYPE html>
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
      background-color: #4d4d4d;
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
    
    <div class="refresh-info">
      ⟳ Auto-refreshing every 2 seconds
    </div>
  </div>

  <script>
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
</html>
)";
    server->send(200, "text/html", html);
  });

  // Simple status endpoint
  server->on("/status", HTTP_GET, [server]() {
    String status = "{\"status\": \"ok\", \"uptime_ms\": " + String(millis()) + "}";
    server->send(200, "application/json", status);
  });

  // API endpoint for JSON data - returns all UDP messages and statistics
  server->on("/api/messages", HTTP_GET, [server]() {
    String json = UDPHandler_buildJson();
    server->send(200, "application/json", json);
  });

  // 404 handler
  server->onNotFound([server]() {
    server->send(404, "text/plain", "Not Found");
  });

  server->begin();
}

void WebServerModule_handleClient(WebServer* server) {
  if (server) {
    server->handleClient();
  }
}
