/*!
 * @file unihiker_k10_webcam.cpp
 * @brief This is a driver library for a network camera module based on the ESP32-CAM board, designed for the DFRobot UniHiker K10 platform.
 * @copyright   Copyright (c) 2025 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license     The MIT License (MIT)
 * @author [TangJie](jie.tang@dfrobot.com)
 * @version  V1.0
 * @date  2025-03-21 
 * @url https://github.com/DFRobot/unihiker_k10_webcam
 */
#include "unihiker_k10_webcam.h"
#include "app_httpd.hpp"
#include "esp_http_server.h"

extern QueueHandle_t xQueueCamer;

static camera_fb_t *frame = NULL;

const char index_html[] = R"rawliteral(
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>K10</title>
    </head>
    <body>
        <h1>K10 Cam</h1>
        <br>
        <button id="captureBtn">Capture</button>
    
        <script>
            document.getElementById("captureBtn").addEventListener("click", () => {
                fetch("/api/camera/capture")
                    .then(response => response.blob())
                    .then(blob => {
                    const url = URL.createObjectURL(blob);
                    const link = document.createElement("a");
                    link.href = url;
                    link.download = "esp32_snapshot.jpg"; // Force specified filename
                    document.body.appendChild(link);
                    link.click();
                    document.body.removeChild(link);
                    URL.revokeObjectURL(url);
                })
                .catch(error => console.error("Error downloading image:", error));
            });

        </script>
    </body>
    </html>
    )rawliteral";




uint8_t *capture_handler_with_size(size_t* out_len) {
    DEBUG_TO_SERIAL("Capture handler triggered!");
    
    *out_len=0;
        
    if (xQueueReceive(xQueueCamer, &frame, portMAX_DELAY) != pdTRUE) {
        DEBUG_TO_SERIAL("continue");
        return NULL;
    }

    size_t jpg_buf_len = 0;
    uint8_t *jpg_buf = NULL;

    //  Convert to JPEG format
    if (!frame2jpg(frame, 80, &jpg_buf, &jpg_buf_len)) {
        DEBUG_TO_SERIAL("JPEG compression failed");
        esp_camera_fb_return(frame);
        free(frame);
        return NULL;
    }

    esp_camera_fb_return(frame);
    
    //  Set HTTP response headers
    
    *out_len=jpg_buf_len;
    free(jpg_buf);
    return jpg_buf;
}


bool ServerModule_registerCamera(WebServer* server, UNIHIKER_K10 &k10) {
      if (!server) {
    DEBUG_TO_SERIAL("ERROR: WebServer pointer is NULL!");
    return false;
  }
  try
  {
     k10.initBgCamerImage();
  }
  catch(const std::exception& e)
  {
    DEBUG_TO_SERIAL("ERROR: initBgCamerImage failed!");
    return false;
  }
  
 
  server->on("/camera", HTTP_GET, [server]() {
    server->send(200, "text/plain", index_html);
  });

  server->on("/api/camera/capture", HTTP_GET, [server]() {
    size_t jpg_len = 0;
    uint8_t* image = capture_handler_with_size(&jpg_len);
    if (image==NULL){
      server->send(500, "text/plain", "Cam Server Error");
      return;
    }

    server->sendHeader("Content-Type", "image/jpeg");
    server->sendHeader("Content-Length", String(jpg_len));
    server->sendHeader("Access-Control-Allow-Origin", "*");
    server->sendHeader("Content-Disposition", "attachment; filename=k10cam.jpg");
    server->send(200, "image/jpeg", (const char*)image);
    free(image);
  });
  
  server->on("/api/camera/stream", HTTP_GET, [server]() {
    WiFiClient client = server->client();
    
    // Send initial HTTP response with proper headers
    server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server->send(200, "multipart/x-mixed-replace; boundary=frame", "");
    
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char part_buf[128];
    
    while (client.connected()) {
        // Receive frame data from queue
        if (xQueueReceive(xQueueCamer, &frame, pdMS_TO_TICKS(100)) != pdTRUE) {
            DEBUG_TO_SERIAL("Queue receive timeout, continuing...");
            continue;   // If receiving fails, continue waiting
        }

        // Convert frame to JPEG
        if (!frame2jpg(frame, 80, &_jpg_buf, &_jpg_buf_len)) {
            DEBUG_TO_SERIAL("JPEG compression failed");
            esp_camera_fb_return(frame);
            continue;  // Skip this frame and continue streaming
        }

        // Send multipart boundary and headers
        int hlen = snprintf(part_buf, sizeof(part_buf),
                           "--frame\r\n"
                           "Content-Type: image/jpeg\r\n"
                           "Content-Length: %u\r\n"
                           "\r\n",
                           _jpg_buf_len);
        
        server->sendContent(part_buf, hlen);
        server->sendContent((const char*)_jpg_buf, _jpg_buf_len);
        server->sendContent("\r\n");
        
        free(_jpg_buf);
        _jpg_buf = NULL;
        esp_camera_fb_return(frame);
    }
    
    DEBUG_TO_SERIAL("Client disconnected from stream");
  });
  return true;
}