/**
 * @file WebcamService.cpp
 * @brief Implementation for webcam service integration with the main application
 * @details Exposed routes:
 *          - GET /api/webcam/snapshot - Capture and return a JPEG snapshot from the camera
 *          - GET /api/webcam/status - Get camera initialization status and current settings
 * 
 */

#include "WebcamService.h"
#include <WebServer.h>
#include <ArduinoJson.h>
#include <unihiker_k10.h>
#include "IsOpenAPIInterface.h"
#include "FlashStringHelper.h"
#include <img_converters.h>

// External webserver instance
extern WebServer webserver;
extern UNIHIKER_K10 unihiker;
QueueHandle_t xQueueCamera; // Camera frame queue from unihiker_k10

// WebcamService constants namespace
namespace WebcamConsts
{
    constexpr uint8_t webcam_camera_queue_length = 1;
    constexpr uint8_t webcam_camera_rate_ms = 10;
    constexpr const char webcam_path_webcam[] PROGMEM = "webcam/";
    constexpr const char webcam_action_snapshot[] PROGMEM = "snapshot";
    constexpr const char webcam_action_status[] PROGMEM = "status";
    constexpr const char webcam_msg_not_initialized[] PROGMEM = "Camera not initialized.";
    constexpr const char webcam_msg_capture_error[] PROGMEM = "Failed to capture image.";
    constexpr const char webcam_msg_camera_capture_failed[] PROGMEM = "Camera capture failed";
    constexpr const char webcam_service_name[] PROGMEM = "Webcam Service";
    constexpr const char webcam_service_path[] PROGMEM = "webcam/v1";
    constexpr const char webcam_field_initialized[] PROGMEM = "initialized";
    constexpr const char webcam_field_ready[] PROGMEM = "ready";
    constexpr const char webcam_field_settings[] PROGMEM = "settings";
    constexpr const char webcam_field_framesize[] PROGMEM = "framesize";
    constexpr const char webcam_field_quality[] PROGMEM = "quality";
    constexpr const char webcam_field_brightness[] PROGMEM = "brightness";
    constexpr const char webcam_field_contrast[] PROGMEM = "contrast";
    constexpr const char webcam_field_saturation[] PROGMEM = "saturation";
    constexpr const char webcam_field_framesize_name[] PROGMEM = "framesize_name";
    constexpr const char webcam_inline_filename[] PROGMEM = "inline; filename=snapshot.jpg";
    constexpr const char webcam_access_control[] PROGMEM = "Access-Control-Allow-Origin";
    constexpr const char webcam_content_disposition[] PROGMEM = "Content-Disposition";
    constexpr const char webcam_mime_image_jpeg[] PROGMEM = "image/jpeg";
    constexpr const char webcam_tag[] PROGMEM = "Webcam";
    constexpr const char webcam_desc_snapshot[] PROGMEM = "Capture and return a JPEG snapshot from the camera in real-time. Image format is SVGA (800x600) by default with quality setting of 12.";
    constexpr const char webcam_desc_status[] PROGMEM = "Get camera initialization status, current settings including frame size, quality, brightness, contrast, and saturation levels";
    constexpr const char webcam_resp_snapshot_ok[] PROGMEM = "JPEG image captured successfully";
    constexpr const char webcam_resp_camera_not_init[] PROGMEM = "Camera not initialized";
    constexpr const char webcam_resp_status_ok[] PROGMEM = "Camera status retrieved successfully";
}


/**
 * @brief Initialize the camera service using register_camera()
 * @details Creates the camera queue and registers camera with hardware
 *          without UI display components
 */
bool WebcamService::initializeService()
{
    logger->info("Initializing " + getServiceName() + "...");
    initialized_ = false;


    if (!xQueueCamera) {
                logger->info("Creating camera queue...");
        try
        {
        xQueueCamera = xQueueCreate(WebcamConsts::webcam_camera_queue_length, sizeof(camera_fb_t *));
        }
        catch(const std::exception& e)
        {
            logger->error(std::string("Exception creating camera queue: ") + e.what());
        }
        if (!xQueueCamera) {
            logger->error("Failed to create camera queue");
            service_status_ = INIT_FAILED;
            status_timestamp_ = millis();
            return false;
        }
    }

    
    // Register camera using low-level API (no screen display)
    // Use RGB565 format - more universally supported than JPEG
    // The frame will be converted to JPEG in handleSnapshot()
    register_camera(PIXFORMAT_RGB565, FRAMESIZE_QVGA, WebcamConsts::webcam_camera_queue_length, xQueueCamera);
    // Verify camera sensor is accessible
    sensor_t *s = esp_camera_sensor_get();
    if (!s) {
        logger->error("Failed to get camera sensor");
        service_status_ = INIT_FAILED;
        status_timestamp_ = millis();
        return false;
    }else {

    }
    
    initialized_ = true;

#ifdef VERBOSE_DEBUG
    logger->debug(getServiceName() + " initialize done");
#endif

    return initialized_;
}

/**
 * @brief Start the camera capture task
 * @details Calls unihiker.setBgCamerImage(true) to activate the camera task
 *          which continuously captures frames and puts them in xQueueCamera
 */
bool WebcamService::startService()
{
    logger->info("Starting " + getServiceName() + "...");

    if (initialized_) {
        // Start camera capture task

        
#ifdef VERBOSE_DEBUG
        logger->debug(getServiceName() + " " + FPSTR(ServiceInterfaceConsts::msg_start_done));
#endif
        service_status_ = STARTED;
        status_timestamp_ = millis();
    } else {
        logger->error(getServiceName() + " start failed - not initialized");
        service_status_ = START_FAILED;
        status_timestamp_ = millis();
    }
    return initialized_;
}

/**
 * @brief Stop the camera capture task
 * @details Calls unihiker.setBgCamerImage(false) to stop the camera task
 *          and suspend frame capture
 */
bool WebcamService::stopService()
{
    logger->info("Stopping " + getServiceName() + "...");
    if (initialized_) {
        // Stop camera capture task

        
        service_status_ = STOPPED;
        status_timestamp_ = millis();
#ifdef VERBOSE_DEBUG
        logger->debug(getServiceName() + " " + FPSTR(ServiceInterfaceConsts::msg_stop_done));
#endif
        return true;
    }
    logger->error(getServiceName() + " " + std::string(ServiceInterfaceConsts::msg_stop_failed));
    return false;
}

void WebcamService::configureCamera(camera_config_t &config)
{
    // This method is no longer used - the UniHiker K10 board
    // handles camera initialization through register_camera()
    // Kept for interface compatibility
    logger->info("Note: Camera configuration is handled by UniHiker K10 board");
}

/**
 * @brief Capture a snapshot from the camera queue
 * @details Retrieves the latest frame from xQueuWebcam queue populated by
 *          the camera task started via setBgCamerImage(true)
 * @return Camera frame buffer pointer, or nullptr if unavailable
 */
camera_fb_t* WebcamService::captureSnapshot()
{
    logger->info("Capturing snapshot from " + getServiceName() + "...");
    if (!initialized_) {
        return nullptr;
    }

    // Check if queue exists
    if (!xQueueCamera) {
        logger->error("Camera queue not initialized");
        return nullptr;
    }

    // Try to get frame from queue (wait up to 1 second)
    camera_fb_t *fb = nullptr;
    if (xQueueReceive(xQueueCamera, &fb, pdMS_TO_TICKS(WebcamConsts::webcam_camera_rate_ms)) != pdTRUE) {
#ifdef VERBOSE_DEBUG
        logger->debug("No frame available in queue");
#endif
        return nullptr;
    }

    if (!fb) {
#ifdef VERBOSE_DEBUG
        Serial.println(FPSTR(WebcamConsts::webcam_msg_camera_capture_failed));
#endif
        return nullptr;
    }

    return fb;
}

void WebcamService::handleSnapshot()
{
    logger->info("Handling snapshot request for " + getServiceName() + "...");  
    if (!initialized_) {
        webserver.send(503, RoutesConsts::mime_plain_text, FPSTR(WebcamConsts::webcam_msg_not_initialized));
        return;
    }

    camera_fb_t *fb = captureSnapshot();
    if (!fb) {
        webserver.send(503, RoutesConsts::mime_plain_text, FPSTR(WebcamConsts::webcam_msg_capture_error));
        return;
    }

    // Validate frame buffer content
    if (!fb->buf || fb->len == 0) {
        logger->error("Frame buffer is empty or invalid");
        esp_camera_fb_return(fb);
        webserver.send(503, RoutesConsts::mime_plain_text, "Frame buffer is empty");
        return;
    }

    uint8_t *jpg_buf = nullptr;
    size_t jpg_len = 0;
    bool needs_free = false;

    // Check actual JPEG magic bytes (0xFF 0xD8) - don't trust fb->format field
    bool is_valid_jpeg = (fb->len >= 2 && fb->buf[0] == 0xFF && fb->buf[1] == 0xD8);
    
    if (is_valid_jpeg) {
        // Data is already valid JPEG
        jpg_buf = fb->buf;
        jpg_len = fb->len;
        logger->info("Frame is valid JPEG, size: " + std::to_string(jpg_len) + " bytes");
    } else {
        // Data is not JPEG - need to convert regardless of what fb->format says
        logger->info("Frame is not JPEG (format=" + std::to_string(fb->format) + 
                     ", first bytes: " + std::to_string(fb->buf[0]) + " " + 
                     std::to_string(fb->buf[1]) + "), converting...");
        
        bool conversion_ok = frame2jpg(fb, 80, &jpg_buf, &jpg_len);
        if (!conversion_ok || !jpg_buf || jpg_len == 0) {
            logger->error("Failed to convert frame to JPEG");
            esp_camera_fb_return(fb);
            if (jpg_buf) free(jpg_buf);
            webserver.send(503, RoutesConsts::mime_plain_text, "JPEG conversion failed");
            return;
        }
        needs_free = true;
        logger->info("Converted to JPEG, size: " + std::to_string(jpg_len) + " bytes");
    }

    // Send JPEG image
    webserver.sendHeader(FPSTR(WebcamConsts::webcam_content_disposition), FPSTR(WebcamConsts::webcam_inline_filename));
    webserver.sendHeader(FPSTR(WebcamConsts::webcam_access_control), "*");
    webserver.setContentLength(jpg_len);
    webserver.send(200, FPSTR(WebcamConsts::webcam_mime_image_jpeg), "");
    webserver.sendContent(reinterpret_cast<const char*>(jpg_buf), jpg_len);

    // Clean up
    if (needs_free && jpg_buf) {
        free(jpg_buf);
    }
    esp_camera_fb_return(fb);
}

void WebcamService::handleStatus()
{
    JsonDocument doc;
    
    // Add standardized status fields
    doc[PSTR("servicename")] = "WebcamService";
    
    const char* status_str = "unknown";
    switch (service_status_) {
        case ServiceStatus::INIT_FAILED: status_str = "init failed"; break;
        case ServiceStatus::START_FAILED: status_str = "start failed"; break;
        case ServiceStatus::STARTED: status_str = "started"; break;
        case ServiceStatus::STOPPED: status_str = "stopped"; break;
        case ServiceStatus::STOP_FAILED: status_str = "stop failed"; break;
    }
    doc[PSTR("status")] = status_str;
    doc[PSTR("ts")] = (unsigned long)status_timestamp_;
    
    // Add additional camera details
    doc[FPSTR(WebcamConsts::webcam_field_initialized)] = initialized_;
    
    if (initialized_) {
        sensor_t *s = esp_camera_sensor_get();
        if (s) {
            doc[RoutesConsts::field_status] = FPSTR(WebcamConsts::webcam_field_ready);
            
            // Get current camera settings
            JsonObject settings = doc[FPSTR(WebcamConsts::webcam_field_settings)].to<JsonObject>();
            settings[FPSTR(WebcamConsts::webcam_field_framesize)] = s->status.framesize;
            settings[FPSTR(WebcamConsts::webcam_field_quality)] = s->status.quality;
            settings[FPSTR(WebcamConsts::webcam_field_brightness)] = s->status.brightness;
            settings[FPSTR(WebcamConsts::webcam_field_contrast)] = s->status.contrast;
            settings[FPSTR(WebcamConsts::webcam_field_saturation)] = s->status.saturation;
            
            // Get frame size info
            const char* frameSizes[] = {
                "96x96", "QQVGA", "QCIF", "HQVGA", "240x240",
                "QVGA", "CIF", "HVGA", "VGA", "SVGA",
                "XGA", "HD", "SXGA", "UXGA"
            };
            int fsIndex = s->status.framesize;
            if (fsIndex >= 0 && fsIndex < 14) {
                settings[FPSTR(WebcamConsts::webcam_field_framesize_name)] = frameSizes[fsIndex];
            }
        } else {
            doc[RoutesConsts::field_status] = RoutesConsts::status_sensor_error;
        }
    } else {
        doc[RoutesConsts::field_status] = RoutesConsts::status_not_initialized;
    }

    String response;
    serializeJson(doc, response);
    webserver.send(200, RoutesConsts::mime_json, response);
}

bool WebcamService::registerRoutes()
{
    // Snapshot endpoint - returns JPEG image
    std::string path = getPath(WebcamConsts::webcam_action_snapshot);
#ifdef VERBOSE_DEBUG
    logger->debug("+" + path);
#endif
    
    std::vector<OpenAPIResponse> snapshotResponses;
    OpenAPIResponse snapshotOk(200, WebcamConsts::webcam_resp_snapshot_ok);
    snapshotOk.contentType = WebcamConsts::webcam_mime_image_jpeg;
    snapshotOk.schema = "{\"type\":\"string\",\"format\":\"binary\",\"description\":\"JPEG image data\"}";
    snapshotResponses.push_back(snapshotOk);
    snapshotResponses.push_back(OpenAPIResponse(503, WebcamConsts::webcam_resp_camera_not_init));
    logger->info("Add "+path+" route");
    registerOpenAPIRoute(OpenAPIRoute(path.c_str(), RoutesConsts::method_get, 
                                      WebcamConsts::webcam_desc_snapshot,
                                      WebcamConsts::webcam_tag, false, {}, snapshotResponses));
    webserver.on(path.c_str(), HTTP_GET, [this]() { handleSnapshot(); });

    // Status endpoint - returns JSON with camera info
    path = getPath(WebcamConsts::webcam_action_status);
#ifdef VERBOSE_DEBUG
    logger->debug("+" + path);
#endif
        logger->info("Add "+path+" route");
    std::vector<OpenAPIResponse> statusResponses;
    OpenAPIResponse statusOk(200, WebcamConsts::webcam_resp_status_ok);
    statusOk.schema = "{\"type\":\"object\",\"properties\":{\"initialized\":{\"type\":\"boolean\",\"description\":\"Whether camera is initialized\"},\"status\":{\"type\":\"string\",\"enum\":[\"ready\",\"sensor_error\",\"not_initialized\"]},\"settings\":{\"type\":\"object\",\"properties\":{\"framesize\":{\"type\":\"integer\",\"description\":\"Frame size code (0-13)\"},\"framesize_name\":{\"type\":\"string\",\"description\":\"Frame size name (e.g., SVGA, VGA)\"},\"quality\":{\"type\":\"integer\",\"description\":\"JPEG quality (0-63, lower is better)\"},\"brightness\":{\"type\":\"integer\",\"description\":\"Brightness level (-2 to 2)\"},\"contrast\":{\"type\":\"integer\",\"description\":\"Contrast level (-2 to 2)\"},\"saturation\":{\"type\":\"integer\",\"description\":\"Saturation level (-2 to 2)\"}}}}}";
    statusOk.example = "{\"initialized\":true,\"status\":\"ready\",\"settings\":{\"framesize\":9,\"framesize_name\":\"SVGA\",\"quality\":12,\"brightness\":0,\"contrast\":0,\"saturation\":0}}";
    statusResponses.push_back(statusOk);
    
    registerOpenAPIRoute(OpenAPIRoute(path.c_str(), RoutesConsts::method_get, 
                                      WebcamConsts::webcam_desc_status,
                                      WebcamConsts::webcam_tag, false, {}, statusResponses));
    webserver.on(path.c_str(), HTTP_GET, [this]() { handleStatus(); });

    registerSettingsRoutes("Webcam", this);

    return true;
}

std::string WebcamService::getServiceName()
{
    return std::string(WebcamConsts::webcam_service_name);
}
std::string WebcamService::getServiceSubPath()
{
    return std::string(WebcamConsts::webcam_service_path);
}
bool WebcamService::saveSettings()
{
    // To be implemented if needed
    return true;
}

bool WebcamService::loadSettings()
{
    // To be implemented if needed
    return true;
}
