// VERSION: simple web server
// Description: WiFi connection with HTTP server responding "ok" to all requests

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <unihiker_k10.h>

const char* ssid = "Freebox-A35871";
const char* password = "azertQSDF1234";

UNIHIKER_K10 unihiker;
WebServer server(80);

// Handle any HTTP request
void handleRequest() {
  server.send(200, "text/plain", "ok");
}

void setup() {
  // Small delay to ensure system stabilizes
  delay(1000);
  
  // Initialize the display
  unihiker.begin();
  unihiker.initScreen(2, 30);
  unihiker.creatCanvas();
  unihiker.canvas->canvasClear();
  unihiker.canvas->canvasText("K10 Starting...", 0, 0xFFFFFF);
  unihiker.canvas->updateCanvas();
  delay(500);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  unihiker.canvas->canvasText("Connecting WiFi...", 1, 0xFFFFFF);
  unihiker.canvas->updateCanvas();
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
  }
  
  unihiker.canvas->canvasClear();
  
  if (WiFi.status() == WL_CONNECTED) {
    unihiker.canvas->canvasText("WiFi Connected!", 0, 0x00FF00);
    unihiker.canvas->canvasText(WiFi.localIP().toString().c_str(), 1, 0xFFFFFF);
    
    // Start the web server
    server.onNotFound(handleRequest);
    server.begin();
    unihiker.canvas->canvasText("Server started", 2, 0x00FF00);
  } else {
    unihiker.canvas->canvasText("WiFi Failed!", 0, 0xFF0000);
  }
  
  unihiker.canvas->updateCanvas();
}

void loop() {
  server.handleClient();
  delay(10);
}