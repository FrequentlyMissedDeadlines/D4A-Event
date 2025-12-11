/*!
 * @file unihiker_k10_webcam.cpp
 * @brief This is a driver library for a network camera (adapted for WebServer)
 * @copyright   Copyright (c) 2025 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license     The MIT License (MIT)
 * @author [TangJie](jie.tang@dfrobot.com)
 * @version  V1.0
 * @date  2025-03-21 
 * @url https://github.com/DFRobot/unihiker_k10_webcam
 */
#include "unihiker_k10_webcam.h"
#include "app_httpd.hpp"
#include <esp_camera.h>

extern QueueHandle_t xQueueCamer;

/**
 * @brief WebServer handler for video streaming. Streams JPEG frames over HTTP using multipart response.
 * @note This uses WebServer's client connection for streaming
 */
static void webserver_stream_handler(WebServer* server) {
    DBG("Stream handler triggered!");
    
    WiFiClient client = server->client();
    camera_fb_t *frame = NULL;
    size_t jpg_buf_len = 0;
    uint8_t *jpg_buf = NULL;
    
    // Set headers for multipart streaming
    server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server->send(200, "multipart/x-mixed-replace; boundary=frame", "");
    
    while (client.connected()) {
        // Receive frame data from the queue (with timeout to check client connection)
        if (xQueueReceive(xQueueCamer, &frame, pdMS_TO_TICKS(100)) != pdTRUE) {
            continue;  // If receiving fails, continue waiting
        }

        // Convert frame to JPEG
        if (!frame2jpg(frame, 80, &jpg_buf, &jpg_buf_len)) {
            DBG("JPEG compression failed");
            esp_camera_fb_return(frame);
            break;
        }

        // Release frame data
        esp_camera_fb_return(frame);
        
        // Send frame boundary and headers
        String header = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " + String(jpg_buf_len) + "\r\n\r\n";
        client.print(header);
        
        // Send JPEG data
        client.write(jpg_buf, jpg_buf_len);
        client.print("\r\n");
        
        free(jpg_buf);
        jpg_buf = NULL;
        
        // Small delay to prevent overwhelming the client
        delay(10);
    }
    
    DBG("Stream handler ended");
}

/**
 * @brief WebServer handler for single image capture. Captures a frame and sends as JPEG.
 * @note This uses WebServer's send methods for single-shot capture
 */
static void webserver_capture_handler(WebServer* server) {
    DBG("Capture handler triggered!");
    
    camera_fb_t *frame = NULL;
    
    // Try to receive frame from queue with timeout
    if (xQueueReceive(xQueueCamer, &frame, pdMS_TO_TICKS(1000)) != pdTRUE) {
        DBG("Failed to receive frame from queue");
        server->send(500, "text/plain", "Failed to capture frame");
        return;
    }

    size_t jpg_buf_len = 0;
    uint8_t *jpg_buf = NULL;

    // Convert to JPEG format
    if (!frame2jpg(frame, 80, &jpg_buf, &jpg_buf_len)) {
        DBG("JPEG compression failed");
        esp_camera_fb_return(frame);
        server->send(500, "text/plain", "JPEG compression failed");
        return;
    }

    // Release frame data
    esp_camera_fb_return(frame);
    
    // Set HTTP response headers and send image data
    server->sendHeader("Access-Control-Allow-Origin", "*");
    server->sendHeader("Content-Disposition", "attachment; filename=capture.jpg");
    server->send_P(200, "image/jpeg", (const char *)jpg_buf, jpg_buf_len);

    free(jpg_buf);
    DBG("Capture completed successfully");
}

/**
 * @brief HTML page for camera interface with video stream and capture button
 */
const char camera_index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>K10 Camera</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            background-color: #1e1e1e;
            color: #e0e0e0;
            margin: 0;
            padding: 20px;
            text-align: center;
        }
        h1 {
            color: #00d4ff;
        }
        .container {
            max-width: 800px;
            margin: 0 auto;
            background-color: #2d2d2d;
            padding: 20px;
            border-radius: 8px;
        }
        img {
            max-width: 100%;
            border: 2px solid #00d4ff;
            border-radius: 4px;
        }
        button {
            background-color: #00d4ff;
            color: #1e1e1e;
            border: none;
            padding: 10px 20px;
            margin: 10px;
            border-radius: 4px;
            cursor: pointer;
            font-size: 16px;
        }
        button:hover {
            background-color: #00a8cc;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>K10 Camera</h1>
        <img id="stream" src="/video/stream" alt="Video Stream">
        <br>
        <button id="captureBtn">Capture & Download</button>
    </div>
    
    <script>
        document.getElementById("captureBtn").addEventListener("click", () => {
            fetch("/video/capture")
                .then(response => response.blob())
                .then(blob => {
                    const url = URL.createObjectURL(blob);
                    const link = document.createElement("a");
                    link.href = url;
                    link.download = "k10_snapshot_" + Date.now() + ".jpg";
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

/**
 * @brief WebServer handler for camera index page. Serves the HTML interface.
 */
static void webserver_camera_index_handler(WebServer* server) {
    DBG("Handling camera index request...");
    server->send_P(200, "text/html", camera_index_html);
}

/**
 * @brief Constructor for unihiker_k10_webcam class.
 */
unihiker_k10_webcam::unihiker_k10_webcam(void) : _server(nullptr), _enabled(false) {
    DBG("unihiker_k10_webcam constructor");
}

/**
 * @brief Enables the webcam and registers URI handlers with the provided WebServer.
 * @param server Pointer to the WebServer instance to register camera routes
 * @return true if routes registered successfully, false otherwise
 */
bool unihiker_k10_webcam::enableWebcam(WebServer* server) {
    if (!server) {
        DBG("ERROR: WebServer pointer is NULL!");
        return false;
    }
    
    if (_enabled) {
        DBG("Webcam already enabled");
        return true;
    }
    
    _server = server;
    
    DBG("Registering camera routes with WebServer");
    
    // Register camera index page at /video
    _server->on("/video", HTTP_GET, [this]() {
        webserver_camera_index_handler(_server);
    });
    
    // Register video stream endpoint
    _server->on("/video/stream", HTTP_GET, [this]() {
        webserver_stream_handler(_server);
    });
    
    // Register single image capture endpoint
    _server->on("/video/capture", HTTP_GET, [this]() {
        webserver_capture_handler(_server);
    });
    
    _enabled = true;
    DBG("Camera routes registered successfully");
    return true;
}

/**
 * @brief Disables the webcam. Note: Routes remain registered with WebServer.
 * @return true if disabled successfully, false otherwise
 */
bool unihiker_k10_webcam::disableWebcam(void) {
    if (!_enabled) {
        DBG("Webcam not enabled");
        return false;
    }
    
    // Note: WebServer doesn't have a way to unregister routes
    // So we just mark as disabled
    _enabled = false;
    _server = nullptr;
    
    DBG("Webcam disabled");
    return true;
}