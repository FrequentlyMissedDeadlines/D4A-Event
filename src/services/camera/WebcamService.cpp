/**
 * @file WebcamService.cpp
 * @brief Implementation for webcam service integration with the main application
 * @details Exposed routes:
 *          - GET /api/webcam/snapshot - Capture and return a JPEG snapshot from the camera
 *          - GET /api/webcam/stream - Stream MJPEG video using multipart/x-mixed-replace
 *          - GET /api/webcam/status - Get camera initialization status and current settings
 *
 */

#include "WebcamService.h"
#include <WebServer.h>
#include <ArduinoJson.h>
#include <unihiker_k10.h>
#include "../IsOpenAPIInterface.h"
#include "../FlashStringHelper.h"
#include <img_converters.h>
#include <Preferences.h>
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include <esp_log.h>

// External webserver instance
extern WebServer webserver;
extern UNIHIKER_K10 unihiker;
QueueHandle_t xQueueCamera; // Camera frame queue from unihiker_k10

// WebcamService constants namespace
namespace WebcamConsts
{
    constexpr uint8_t camera_queue_length = 2;
    constexpr uint16_t camera_rate_ms = 50;  // 50ms timeout -> 20 per second
    constexpr framesize_t frame_size = FRAMESIZE_HVGA; //480x320
    constexpr const char action_snapshot[] PROGMEM = "snapshot";
    constexpr const char action_status[] PROGMEM = "status";
    constexpr const char action_stream[] PROGMEM = "stream";
    constexpr const char action_settings[] PROGMEM = "settings";
    constexpr const char msg_not_initialized[] PROGMEM = "Camera not initialized.";
    constexpr const char msg_capture_error[] PROGMEM = "Failed to capture image.";
    constexpr const char msg_camera_capture_failed[] PROGMEM = "Camera capture failed";
    constexpr const char msg_streaming_active[] PROGMEM = "Snapshot unavailable during active streaming. Stop stream first.";
    constexpr const char service_name[] PROGMEM = "Webcam Service";
    constexpr const char service_path[] PROGMEM = "webcam/v1";
    constexpr const char field_initialized[] PROGMEM = "initialized";
    constexpr const char field_ready[] PROGMEM = "ready";
    constexpr const char field_settings[] PROGMEM = "settings";
    constexpr const char field_framesize[] PROGMEM = "framesize";
    constexpr const char field_quality[] PROGMEM = "quality";
    constexpr const char field_brightness[] PROGMEM = "brightness";
    constexpr const char field_contrast[] PROGMEM = "contrast";
    constexpr const char field_saturation[] PROGMEM = "saturation";
    constexpr const char field_framesize_name[] PROGMEM = "framesize_name";
    constexpr const char inline_filename[] PROGMEM = "inline; filename=snapshot.jpg";
    constexpr const char access_control[] PROGMEM = "Access-Control-Allow-Origin";
    constexpr const char content_disposition[] PROGMEM = "Content-Disposition";
    constexpr const char mime_image_jpeg[] PROGMEM = "image/jpeg";
    constexpr const char mime_multipart[] PROGMEM = "multipart/x-mixed-replace; boundary=frame";
    constexpr const char boundary_start[] PROGMEM = "\r\n--frame\r\nContent-Type: image/jpeg\r\nContent-Length: ";
    constexpr const char boundary_end[] PROGMEM = "\r\n\r\n";
    constexpr const char tag[] PROGMEM = "Webcam";
    constexpr const char desc_snapshot[] PROGMEM = "Capture and return a JPEG snapshot from the camera in real-time. Image format is SVGA (800x600) by default with quality setting of 12.";
    constexpr const char desc_status[] PROGMEM = "Get camera iatusnitialization status, current settings including frame size, quality, brightness, contrast, and saturation levels";
    constexpr const char desc_stream[] PROGMEM = "Stream MJPEG video from camera using multipart/x-mixed-replace protocol. Continuously delivers JPEG frames for real-time video display in browser.";
    constexpr const char desc_settings[] PROGMEM = "Update camera settings including JPEG quality (0-63, lower is better), frame size (0-13), brightness (-2 to 2), contrast (-2 to 2), and saturation (-2 to 2). Only provided fields will be updated.";
    constexpr const char resp_snapshot_ok[] PROGMEM = "JPEG image captured successfully";
    constexpr const char resp_camera_not_init[] PROGMEM = "Camera not initialized";
    constexpr const char resp_status_ok[] PROGMEM = "Camera status retrieved successfully";
    constexpr const char resp_stream_ok[] PROGMEM = "MJPEG stream started successfully";
    constexpr const char resp_settings_ok[] PROGMEM = "Camera settings updated successfully";
    constexpr const char resp_invalid_json[] PROGMEM = "Invalid JSON in request body";
    constexpr const char resp_settings_error[] PROGMEM = "Failed to apply camera settings";
    constexpr const char msg_settings_updated[] PROGMEM = "Settings updated successfully";
    constexpr const char msg_settings_failed[] PROGMEM = "Failed to apply settings";
    constexpr const char pref_namespace[] PROGMEM = "webcam";
    constexpr const char pref_quality[] PROGMEM = "quality";
    constexpr const char pref_framesize[] PROGMEM = "framesize";
    constexpr const char pref_brightness[] PROGMEM = "brightness";
    constexpr const char pref_contrast[] PROGMEM = "contrast";
    constexpr const char pref_saturation[] PROGMEM = "saturation";
    constexpr uint16_t stream_delay_ms = 33;  // ~30fps target frame rate
    
    // Framesize name mappings
    constexpr const char fs_96x96[] PROGMEM = "96X96";
    constexpr const char fs_qqvga[] PROGMEM = "QQVGA";
    constexpr const char fs_qcif[] PROGMEM = "QCIF";
    constexpr const char fs_hqvga[] PROGMEM = "HQVGA";
    constexpr const char fs_240x240[] PROGMEM = "240X240";
    constexpr const char fs_qvga[] PROGMEM = "QVGA";
    constexpr const char fs_cif[] PROGMEM = "CIF";
    constexpr const char fs_hvga[] PROGMEM = "HVGA";
    constexpr const char fs_vga[] PROGMEM = "VGA";
    constexpr const char fs_svga[] PROGMEM = "SVGA";
    constexpr const char fs_xga[] PROGMEM = "XGA";
    constexpr const char fs_hd[] PROGMEM = "HD";
    constexpr const char fs_sxga[] PROGMEM = "SXGA";
    constexpr const char fs_uxga[] PROGMEM = "UXGA";
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

    if (!xQueueCamera)
    {
        logger->info("Creating camera queue...");
        try
        {
        xQueueCamera = xQueueCreate(WebcamConsts::camera_queue_length, sizeof(camera_fb_t *));
        }
        catch (const std::exception &e)
        {
            logger->error(std::string("Exception creating camera queue: ") + e.what());
        }
        if (!xQueueCamera)
        {
            logger->error("Failed to create camera queue");
            setServiceStatus(INITIALIZED_FAILED);
            return false;
        }
    }

    // Load framesize from preferences if saved, otherwise use default
    Preferences prefs;
    if (prefs.begin(progmem_to_string(WebcamConsts::pref_namespace).c_str(), true))
    {
        if (prefs.isKey(progmem_to_string(WebcamConsts::pref_framesize).c_str()))
        {
            current_framesize_ = (framesize_t)prefs.getInt(progmem_to_string(WebcamConsts::pref_framesize).c_str(), WebcamConsts::frame_size);
            logger->info("Using saved framesize: " + std::to_string(current_framesize_));
        }
        else
        {
            current_framesize_ = WebcamConsts::frame_size;
        }
        prefs.end();
    }
    else
    {
        current_framesize_ = WebcamConsts::frame_size;
    }
    
    // Register camera using low-level API (no screen display)
    // Use RGB565 format - more universally supported than JPEG
    // The frame will be converted to JPEG in handleSnapshot()
    register_camera(PIXFORMAT_RGB565, current_framesize_, WebcamConsts::camera_queue_length, xQueueCamera);
    // Verify camera sensor is accessible
    sensor_t *s = esp_camera_sensor_get();
    if (!s)
    {
        logger->error("Failed to get camera sensor");
        setServiceStatus(INITIALIZED_FAILED);
        return false;
    }
    else
    {
    }

    initialized_ = true;
    setServiceStatus(INITIALIZED);

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

    if (initialized_)
    {
        // Load saved settings if available
        loadSettings();
        
        // Start camera capture task

#ifdef VERBOSE_DEBUG
        logger->debug(getServiceName() + " " + getStatusString());
#endif
        setServiceStatus(STARTED);
    }
    else
    {
        logger->error(getServiceName() + " " + getStatusString());
        setServiceStatus(START_FAILED);
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
    if (initialized_)
    {
        // Stop camera capture task

        setServiceStatus(STOPPED);
#ifdef VERBOSE_DEBUG
        logger->debug(getServiceName() + " " + getStatusString());
#endif
        return true;
    }
    logger->error(getServiceName() + " " + getStatusString());
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
 * @brief Reinitialize camera with new framesize
 * @details Fully stops and restarts the camera service with new framesize
 *          This is the only safe way to change framesize as it reallocates buffers
 * @param framesize New framesize to apply (0-13)
 * @return true if reinitialization successful, false otherwise
 */
bool WebcamService::reinitializeWithFramesize(framesize_t framesize)
{
    logger->info("Reinitializing camera service with framesize " + std::to_string(framesize));
    
    // Check if already at requested framesize
    if (current_framesize_ == framesize)
    {
        logger->info("Already at requested framesize " + std::to_string(framesize));
        return true;
    }
    
    // Step 1: Stop the service (but keep initialized_ flag for restart)
    logger->info("Stopping camera service...");
    bool was_initialized = initialized_;
    
    // Flush and clear queue
    camera_fb_t *stale_fb = nullptr;
    while (xQueueReceive(xQueueCamera, &stale_fb, 0) == pdTRUE)
    {
        if (stale_fb)
        {
            esp_camera_fb_return(stale_fb);
        }
    }
    
    // Step 2: Deinitialize camera hardware
    logger->info("Deinitializing camera hardware...");
    esp_err_t err = esp_camera_deinit();
    if (err != ESP_OK)
    {
        logger->error("Failed to deinitialize camera: " + std::to_string(err));
        initialized_ = false;
        setServiceStatus(INITIALIZED_FAILED);
        return false;
    }
    
    // Wait for hardware to fully shut down
    delay(500);
    
    // Step 3: Reinitialize with new framesize
    logger->info("Reinitializing camera with new framesize...");
    current_framesize_ = framesize;
    register_camera(PIXFORMAT_RGB565, framesize, WebcamConsts::camera_queue_length, xQueueCamera);
    
    // Step 4: Verify sensor is accessible
    sensor_t *s = esp_camera_sensor_get();
    if (!s)
    {
        logger->error("Failed to get camera sensor after reinitialization");
        initialized_ = false;
        setServiceStatus(INITIALIZED_FAILED);
        return false;
    }
    
    // Step 5: Restore previous settings (quality, brightness, etc.) if they were saved
    if (was_initialized)
    {
        initialized_ = true;
        // Reload other settings from preferences
        loadSettings();
    }
    
    // Wait for camera to stabilize
    delay(200);
    
    // Flush initial frames
    int flushed = 0;
    while (flushed < 3 && xQueueReceive(xQueueCamera, &stale_fb, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        if (stale_fb)
        {
            esp_camera_fb_return(stale_fb);
            flushed++;
        }
    }
    
    logger->info("Camera reinitialized successfully with framesize " + std::to_string(framesize));
    return true;
}

/**
 * @brief Capture a snapshot from the camera queue
 * @details Retrieves the latest frame from xQueuWebcam queue populated by
 *          the camera task started via setBgCamerImage(true)
 * @return Camera frame buffer pointer, or nullptr if unavailable
 */
camera_fb_t *WebcamService::captureSnapshot()
{
    
    if (!initialized_)
    {
        return nullptr;
    }

    // Check if queue exists
    if (!xQueueCamera)
    {
        logger->error("Camera queue not initialized");
        return nullptr;
    }

    // Flush old frames and get the most recent one
    // This ensures we always get the latest captured image, not a stale frame
    camera_fb_t *fb = nullptr;
    camera_fb_t *latest_fb = nullptr;
    
    // Keep reading frames until queue is empty, keeping only the last one
    while (xQueueReceive(xQueueCamera, &fb, 0) == pdTRUE)
    {
        // Return previous frame if we had one
        if (latest_fb != nullptr)
        {
            esp_camera_fb_return(latest_fb);
        }
        latest_fb = fb;
    }
    
    // If we got at least one frame, return it
    if (latest_fb != nullptr)
    {
        return latest_fb;
    }
    
    // No frames available, wait for a new one
    BaseType_t result = xQueueReceive(xQueueCamera, &latest_fb, WebcamConsts::camera_rate_ms / portTICK_PERIOD_MS);
    if (result != pdTRUE)
    {
#ifdef VERBOSE_DEBUG
        logger->debug("No frame available in queue after " + std::to_string(WebcamConsts::camera_rate_ms) + "ms timeout");
#endif
        return nullptr;
    }

    if (!latest_fb)
    {
#ifdef VERBOSE_DEBUG
        logger->debug(progmem_to_string_to_string(progmem_to_string(WebcamConsts::msg_camera_capture_failed)));
#endif
        return nullptr;
    }

    return latest_fb;
}

/**
 * @brief Handle MJPEG streaming request
 * @details Continuously captures frames and sends them as multipart/x-mixed-replace stream
 *          Each frame is sent with proper MIME boundaries for MJPEG protocol
 */
void WebcamService::handleStream()
{
    logger->info("Handling streaming request for " + getServiceName() + "...");
    if (!initialized_)
    {
        webserver.send(503, RoutesConsts::mime_plain_text, progmem_to_string(ServiceInterfaceConsts::service_status_not_initialized).c_str());
        return;
    }

    // Set streaming flag to block snapshot requests
    streaming_active_ = true;

    // Send multipart stream headers
    webserver.setContentLength(CONTENT_LENGTH_UNKNOWN);
    webserver.send(200, WebcamConsts::mime_multipart, "");

    // Continuous streaming loop
    while (webserver.client().connected())
    {
        camera_fb_t *fb = captureSnapshot();
        if (!fb)
        {
            delay(WebcamConsts::stream_delay_ms);
            continue;
        }

        // Validate frame buffer
        if (!fb->buf || fb->len == 0)
        {
            esp_camera_fb_return(fb);
            delay(WebcamConsts::stream_delay_ms);
            continue;
        }

        uint8_t *jpg_buf = nullptr;
        size_t jpg_len = 0;
        bool needs_free = false;

        // Check for valid JPEG (magic bytes 0xFF 0xD8)
        bool is_valid_jpeg = (fb->len >= 2 && fb->buf[0] == 0xFF && fb->buf[1] == 0xD8);

        if (is_valid_jpeg)
        {
            jpg_buf = fb->buf;
            jpg_len = fb->len;
        }
        else
        {
            // Convert to JPEG
            bool conversion_ok = frame2jpg(fb, 80, &jpg_buf, &jpg_len);
            if (!conversion_ok || !jpg_buf || jpg_len == 0)
            {
                esp_camera_fb_return(fb);
                if (jpg_buf)
                    free(jpg_buf);
                delay(WebcamConsts::stream_delay_ms);
                continue;
            }
            needs_free = true;
        }

        // Send multipart boundary and frame
        webserver.sendContent(WebcamConsts::boundary_start);
        webserver.sendContent(String(jpg_len));
        webserver.sendContent(WebcamConsts::boundary_end);
        webserver.sendContent(reinterpret_cast<const char *>(jpg_buf), jpg_len);

        // Cleanup
        if (needs_free && jpg_buf)
        {
            free(jpg_buf);
        }
        esp_camera_fb_return(fb);

        // Small delay to control frame rate
        delay(WebcamConsts::stream_delay_ms);
    }

    // Clear streaming flag to allow snapshots again
    streaming_active_ = false;
    logger->info("Stream ended for " + getServiceName());
}
void WebcamService::handleSnapshot()
{
    logger->info("Handling snapshot request for " + getServiceName() + "...");
    if (!initialized_)
    {
        webserver.send(503, RoutesConsts::mime_plain_text, progmem_to_string(WebcamConsts::msg_not_initialized).c_str());
        return;
    }

    // Reject snapshot if streaming is active
    if (streaming_active_)
    {
        webserver.send(409, RoutesConsts::mime_plain_text, progmem_to_string(WebcamConsts::msg_streaming_active).c_str());
        return;
    }

    camera_fb_t *fb = captureSnapshot();
    if (!fb)
    {
        webserver.send(503, RoutesConsts::mime_plain_text, progmem_to_string(WebcamConsts::msg_capture_error).c_str());
        return;
    }

    // Validate frame buffer content
    if (!fb->buf || fb->len == 0)
    {
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

    if (is_valid_jpeg)
    {
        // Data is already valid JPEG
        jpg_buf = fb->buf;
        jpg_len = fb->len;
        logger->info("Frame is valid JPEG, size: " + std::to_string(jpg_len) + " bytes");
    }
    else
    {
        // Data is not JPEG - need to convert regardless of what fb->format says
        logger->info("Frame is not JPEG (format=" + std::to_string(fb->format) +
                     ", first bytes: " + std::to_string(fb->buf[0]) + " " +
                     std::to_string(fb->buf[1]) + "), converting...");

        bool conversion_ok = frame2jpg(fb, 80, &jpg_buf, &jpg_len);
        if (!conversion_ok || !jpg_buf || jpg_len == 0)
        {
            logger->error("Failed to convert frame to JPEG");
            esp_camera_fb_return(fb);
            if (jpg_buf)
                free(jpg_buf);
            webserver.send(503, RoutesConsts::mime_plain_text, "JPEG conversion failed");
            return;
        }
        needs_free = true;
        logger->info("Converted to JPEG, size: " + std::to_string(jpg_len) + " bytes");
    }

    // Send JPEG image
    webserver.sendHeader(progmem_to_string(WebcamConsts::content_disposition).c_str(), progmem_to_string(WebcamConsts::inline_filename).c_str());
    webserver.sendHeader(progmem_to_string(WebcamConsts::access_control).c_str(), "*");
    webserver.setContentLength(jpg_len);
    webserver.send(200, progmem_to_string(WebcamConsts::mime_image_jpeg).c_str(), "");
    webserver.sendContent(reinterpret_cast<const char *>(jpg_buf), jpg_len);

    // Clean up
    if (needs_free && jpg_buf)
    {
        free(jpg_buf);
    }
    esp_camera_fb_return(fb);
}

void WebcamService::handleStatus()
{
    JsonDocument doc;

    // Add standardized status fields
    doc[PSTR("servicename")] = "WebcamService";

    const char *status_str = "unknown";
    switch (service_status_)
    {
    case ServiceStatus::INITIALIZED_FAILED:
        status_str = "init failed";
        break;
    case ServiceStatus::START_FAILED:
        status_str = "start failed";
        break;
    case ServiceStatus::STARTED:
        status_str = "started";
        break;
    case ServiceStatus::STOPPED:
        status_str = "stopped";
        break;
    case ServiceStatus::STOP_FAILED:
        status_str = "stop failed";
        break;
    }
    doc[PSTR("status")] = status_str;
    doc[PSTR("ts")] = (unsigned long)status_timestamp_;

    // Add additional camera details
    doc[progmem_to_string(WebcamConsts::field_initialized)] = initialized_;

    if (initialized_)
    {
        sensor_t *s = esp_camera_sensor_get();
        if (s)
        {
            doc[RoutesConsts::field_status] = progmem_to_string(WebcamConsts::field_ready);

            // Get current camera settings
            JsonObject settings = doc[progmem_to_string(WebcamConsts::field_settings)].to<JsonObject>();
            settings[progmem_to_string(WebcamConsts::field_framesize)] = s->status.framesize;
            settings[progmem_to_string(WebcamConsts::field_quality)] = s->status.quality;
            settings[progmem_to_string(WebcamConsts::field_brightness)] = s->status.brightness;
            settings[progmem_to_string(WebcamConsts::field_contrast)] = s->status.contrast;
            settings[progmem_to_string(WebcamConsts::field_saturation)] = s->status.saturation;

            // Get frame size info
            const char *frameSizes[] = {
                "96x96", "QQVGA", "QCIF", "HQVGA", "240x240",
                "QVGA", "CIF", "HVGA", "VGA", "SVGA",
                "XGA", "HD", "SXGA", "UXGA"};
            int fsIndex = s->status.framesize;
            if (fsIndex >= 0 && fsIndex < 14)
            {
                settings[progmem_to_string(WebcamConsts::field_framesize_name)] = frameSizes[fsIndex];
            }
        }
        else
        {
            doc[RoutesConsts::field_status] = RoutesConsts::status_sensor_error;
        }
    }
    else
    {
        doc[RoutesConsts::field_status] = RoutesConsts::status_not_initialized;
    }

    String response;
    serializeJson(doc, response);
    webserver.send(200, RoutesConsts::mime_json, response);
}

/**
 * @brief Handle camera settings update request
 * @details Accepts JSON body with optional fields: quality, framesize, brightness, contrast, saturation
 *          Only provided fields will be updated. Values are validated before applying.
 */
void WebcamService::handleSettings()
{
    logger->info("Handling settings update request for " + getServiceName() + "...");
    
    if (!initialized_)
    {
        webserver.send(503, RoutesConsts::mime_json, 
                      getResultJsonString(RoutesConsts::result_err, 
                                        progmem_to_string(WebcamConsts::msg_not_initialized)).c_str());
        return;
    }
    
    sensor_t *s = esp_camera_sensor_get();
    if (!s)
    {
        webserver.send(503, RoutesConsts::mime_json,
                      getResultJsonString(RoutesConsts::result_err, 
                                        "Failed to get camera sensor").c_str());
        return;
    }
    
    // Parse JSON body
    if (!webserver.hasArg("plain"))
    {
        webserver.send(400, RoutesConsts::mime_json,
                      getResultJsonString(RoutesConsts::result_err, 
                                        progmem_to_string(RoutesConsts::msg_invalid_json)).c_str());
        return;
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, webserver.arg("plain"));
    
    if (error)
    {
        logger->error("JSON parse error: " + std::string(error.c_str()));
        webserver.send(400, RoutesConsts::mime_json,
                      getResultJsonString(RoutesConsts::result_err, 
                                        progmem_to_string(WebcamConsts::resp_invalid_json)).c_str());
        return;
    }
    
    bool updated = false;
    std::string updateMsg = "";
    // Update quality (0-63, lower is better quality)
    if (doc[FPSTR(WebcamConsts::field_quality)].is<JsonVariant>())
    {
        int quality = doc[FPSTR(WebcamConsts::field_quality)];
        if (quality >= 0 && quality <= 63)
        {
            s->set_quality(s, quality);
            updated = true;
            updateMsg += "quality=" + std::to_string(quality) + " ";
            logger->info("Updated quality to " + std::to_string(quality));
        }
        else
        {
            webserver.send(422, RoutesConsts::mime_json,
                          getResultJsonString(RoutesConsts::result_err, 
                                            "quality must be 0-63").c_str());
            return;
        }
    }
    
    // Framesize changes are DISABLED at runtime due to buffer reallocation issues
    // To change framesize: save settings, then restart the WebcamService
    if (doc[FPSTR(WebcamConsts::field_framesize)].is<JsonVariant>())
    {
        int framesize = -1;
        
        // Check if framesize is a string (name) or integer (numeric value)
        if (doc[FPSTR(WebcamConsts::field_framesize)].is<const char*>())
        {
            std::string fs_name = doc[FPSTR(WebcamConsts::field_framesize)].as<const char*>();
            // Convert to uppercase for case-insensitive comparison
            for (char &c : fs_name) c = toupper(c);
            
            // Map name to framesize value
            if (fs_name == "96X96") framesize = 0;
            else if (fs_name == "QQVGA") framesize = 1;
            else if (fs_name == "QCIF") framesize = 2;
            else if (fs_name == "HQVGA") framesize = 3;
            else if (fs_name == "240X240") framesize = 4;
            else if (fs_name == "QVGA") framesize = 5;
            else if (fs_name == "CIF") framesize = 6;
            else if (fs_name == "HVGA") framesize = 7;
            else if (fs_name == "VGA") framesize = 8;
            else if (fs_name == "SVGA") framesize = 9;
            else if (fs_name == "XGA") framesize = 10;
            else if (fs_name == "HD") framesize = 11;
            else if (fs_name == "SXGA") framesize = 12;
            else if (fs_name == "UXGA") framesize = 13;
            else
            {
                webserver.send(422, RoutesConsts::mime_json,
                              getResultJsonString(RoutesConsts::result_err, 
                                                ("Invalid framesize name: " + fs_name + ". Valid names: 96X96, QQVGA, QCIF, HQVGA, 240X240, QVGA, CIF, HVGA, VGA, SVGA, XGA, HD, SXGA, UXGA").c_str()).c_str());
                return;
            }
        }
        else if (doc[FPSTR(WebcamConsts::field_framesize)].is<int>())
        {
            framesize = doc[FPSTR(WebcamConsts::field_framesize)];
            if (framesize < 0 || framesize > 13)
            {
                webserver.send(422, RoutesConsts::mime_json,
                              getResultJsonString(RoutesConsts::result_err, 
                                                "framesize must be 0-13 or a valid name (VGA, SVGA, etc.)").c_str());
                return;
            }
        }
        else
        {
            webserver.send(422, RoutesConsts::mime_json,
                          getResultJsonString(RoutesConsts::result_err, 
                                            "framesize must be a number (0-13) or name (VGA, SVGA, etc.)").c_str());
            return;
        }
        
        if (framesize >= 0 && framesize <= 13)
        {
            // Save framesize to preferences for next initialization
            Preferences prefs;
            if (prefs.begin(progmem_to_string(WebcamConsts::pref_namespace).c_str(), false))
            {
                prefs.putInt(progmem_to_string(WebcamConsts::pref_framesize).c_str(), framesize);
                prefs.end();
                updated = true;
                
                // Map numeric value back to name for response
                const char* fs_names[] = {"96X96", "QQVGA", "QCIF", "HQVGA", "240X240", "QVGA", "CIF", "HVGA", "VGA", "SVGA", "XGA", "HD", "SXGA", "UXGA"};
                std::string fs_name = fs_names[framesize];
                
                updateMsg += "framesize=" + fs_name + " (will apply on service restart) ";
                logger->info("Framesize " + fs_name + " (" + std::to_string(framesize) + ") saved to preferences - restart service to apply");
                
                webserver.send(200, RoutesConsts::mime_json,
                              getResultJsonString(RoutesConsts::result_ok, 
                                                ("Framesize " + fs_name + " saved. Restart camera service to apply: POST /api/webcam/v1/stop then POST /api/webcam/v1/start").c_str()).c_str());
                return;
            }
            else
            {
                webserver.send(503, RoutesConsts::mime_json,
                              getResultJsonString(RoutesConsts::result_err, 
                                                "Failed to save framesize preference").c_str());
                return;
            }
        }
    }
    
    // Update brightness (-2 to 2)
    if (doc[FPSTR(WebcamConsts::field_brightness)].is<JsonVariant>())
    {
        int brightness = doc[FPSTR(WebcamConsts::field_brightness)];
        if (brightness >= -2 && brightness <= 2)
        {
            s->set_brightness(s, brightness);
            updated = true;
            updateMsg += "brightness=" + std::to_string(brightness) + " ";
            logger->info("Updated brightness to " + std::to_string(brightness));
        }
        else
        {
            webserver.send(422, RoutesConsts::mime_json,
                          getResultJsonString(RoutesConsts::result_err, 
                                            "brightness must be -2 to 2").c_str());
            return;
        }
    }
    
    // Update contrast (-2 to 2)
    if (doc[FPSTR(WebcamConsts::field_contrast)].is<JsonVariant>())
    {
        int contrast = doc[FPSTR(WebcamConsts::field_contrast)];
        if (contrast >= -2 && contrast <= 2)
        {
            s->set_contrast(s, contrast);
            updated = true;
            updateMsg += "contrast=" + std::to_string(contrast) + " ";
            logger->info("Updated contrast to " + std::to_string(contrast));
        }
        else
        {
            webserver.send(422, RoutesConsts::mime_json,
                          getResultJsonString(RoutesConsts::result_err, 
                                            "contrast must be -2 to 2").c_str());
            return;
        }
    }
    
    // Update saturation (-2 to 2)
    if (doc[FPSTR(WebcamConsts::field_saturation)].is<JsonVariant>())
    {
        int saturation = doc[FPSTR(WebcamConsts::field_saturation)];
        if (saturation >= -2 && saturation <= 2)
        {
            s->set_saturation(s, saturation);
            updated = true;
            updateMsg += "saturation=" + std::to_string(saturation) + " ";
            logger->info("Updated saturation to " + std::to_string(saturation));
        }
        else
        {
            webserver.send(422, RoutesConsts::mime_json,
                          getResultJsonString(RoutesConsts::result_err, 
                                            "saturation must be -2 to 2").c_str());
            return;
        }
    }
    
    if (updated)
    {
        webserver.send(200, RoutesConsts::mime_json,
                      getResultJsonString(RoutesConsts::result_ok, 
                                        updateMsg.c_str()).c_str());
    }
    else
    {
        webserver.send(400, RoutesConsts::mime_json,
                      getResultJsonString(RoutesConsts::result_err, 
                                        "No valid settings provided").c_str());
    }
}

bool WebcamService::registerRoutes()
{
    // Snapshot endpoint - returns JPEG image
    std::string path = getPath(WebcamConsts::action_snapshot);
#ifdef VERBOSE_DEBUG
    logger->debug("+" + path);
#endif

    std::vector<OpenAPIResponse> snapshotResponses;
    OpenAPIResponse snapshotOk(200, WebcamConsts::resp_snapshot_ok);
    snapshotOk.contentType = WebcamConsts::mime_image_jpeg;
    snapshotOk.schema = "{\"type\":\"string\",\"format\":\"binary\",\"description\":\"JPEG image data\"}";
    snapshotResponses.push_back(snapshotOk);
    snapshotResponses.push_back(OpenAPIResponse(503, WebcamConsts::resp_camera_not_init));
    snapshotResponses.push_back(createServiceNotStartedResponse());
    logger->info("Add " + path + " route");
    registerOpenAPIRoute(OpenAPIRoute(path.c_str(), RoutesConsts::method_get,
                                      WebcamConsts::desc_snapshot,
                                      WebcamConsts::tag, false, {}, snapshotResponses));
    webserver.on(path.c_str(), HTTP_GET, [this]()
                 { 
        if (!checkServiceStarted()) return;
        handleSnapshot(); });

    // Status endpoint - returns JSON with camera info
    registerServiceStatusRoute(WebcamConsts::tag, this);

    // Settings endpoint - accepts JSON body to update camera settings
    path = getPath(WebcamConsts::action_settings);
#ifdef VERBOSE_DEBUG
    logger->debug("+" + path);
#endif
    logger->info("Add " + path + " route");
    
    std::vector<OpenAPIParameter> settingsParams;
    std::vector<OpenAPIResponse> settingsResponses;
    
    // Define request body schema
    OpenAPIRequestBody settingsBody(
        progmem_to_string(WebcamConsts::desc_settings).c_str(),
        "{\"type\":\"object\",\"properties\":{\
        \"quality\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":63,\"description\":\"JPEG quality (0-63, lower is better)\"},\
        \"framesize\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":13,\"description\":\"Frame size (0-13)\"},\
        \"brightness\":{\"type\":\"integer\",\"minimum\":-2,\"maximum\":2,\"description\":\"Brightness (-2 to 2)\"},\
        \"contrast\":{\"type\":\"integer\",\"minimum\":-2,\"maximum\":2,\"description\":\"Contrast (-2 to 2)\"},\
        \"saturation\":{\"type\":\"integer\",\"minimum\":-2,\"maximum\":2,\"description\":\"Saturation (-2 to 2)\"}},\
        \"additionalProperties\":false}",
        false
    );
    
    settingsResponses.push_back(OpenAPIResponse(200, WebcamConsts::resp_settings_ok));
    settingsResponses.push_back(OpenAPIResponse(400, WebcamConsts::resp_invalid_json));
    settingsResponses.push_back(OpenAPIResponse(422, RoutesConsts::resp_missing_params));
    settingsResponses.push_back(OpenAPIResponse(503, WebcamConsts::resp_camera_not_init));
    settingsResponses.push_back(createServiceNotStartedResponse());
    
    OpenAPIRoute settingsRoute(path.c_str(), RoutesConsts::method_put,
                              progmem_to_string(WebcamConsts::desc_settings).c_str(),
                              WebcamConsts::tag, false, settingsParams, settingsResponses);
    settingsRoute.requestBody = settingsBody;
    registerOpenAPIRoute(settingsRoute);
    
    webserver.on(path.c_str(), HTTP_PUT, [this]()
                 { 
        if (!checkServiceStarted()) return;
        handleSettings(); });

    // Stream endpoint - returns MJPEG stream
    path = getPath(WebcamConsts::action_stream);
#ifdef VERBOSE_DEBUG
    logger->debug("+" + path);
#endif
    logger->info("Add " + path + " route");
    std::vector<OpenAPIResponse> streamResponses;
    OpenAPIResponse streamOk(200, WebcamConsts::resp_stream_ok);
    streamOk.contentType = WebcamConsts::mime_multipart;
    streamOk.schema = "{\"type\":\"string\",\"format\":\"binary\",\"description\":\"Continuous MJPEG video stream\"}";
    streamResponses.push_back(streamOk);
    streamResponses.push_back(OpenAPIResponse(503, WebcamConsts::resp_camera_not_init));
    streamResponses.push_back(createServiceNotStartedResponse());

    registerOpenAPIRoute(OpenAPIRoute(path.c_str(), RoutesConsts::method_get,
                                      WebcamConsts::desc_stream,
                                      WebcamConsts::tag, false, {}, streamResponses));
    webserver.on(path.c_str(), HTTP_GET, [this]()
                 { 
        if (!checkServiceStarted()) return;
        handleStream(); });

    // Stop service route - allows stopping the camera service via HTTP
    path = getPath("stop");
    logger->info("Add " + path + " route");
    std::vector<OpenAPIResponse> stopResponses;
    stopResponses.push_back(OpenAPIResponse(200, "Service stopped successfully"));
    stopResponses.push_back(OpenAPIResponse(500, "Failed to stop service"));
    registerOpenAPIRoute(OpenAPIRoute(path.c_str(), RoutesConsts::method_post,
                                      "Stop the camera service (useful before changing framesize)",
                                      WebcamConsts::tag, false, {}, stopResponses));
    webserver.on(path.c_str(), HTTP_POST, [this]()
                 {
        bool success = this->stopService();
        webserver.send(success ? 200 : 500, RoutesConsts::mime_json,
                      getResultJsonString(success ? RoutesConsts::result_ok : RoutesConsts::result_err,
                                        success ? "Service stopped" : "Failed to stop service").c_str());
    });

    // Start service route - allows starting the camera service via HTTP
    path = getPath("start");
    logger->info("Add " + path + " route");
    std::vector<OpenAPIResponse> startResponses;
    startResponses.push_back(OpenAPIResponse(200, "Service started successfully"));
    startResponses.push_back(OpenAPIResponse(500, "Failed to start service"));
    registerOpenAPIRoute(OpenAPIRoute(path.c_str(), RoutesConsts::method_post,
                                      "Start the camera service (applies saved framesize from preferences)",
                                      WebcamConsts::tag, false, {}, startResponses));
    webserver.on(path.c_str(), HTTP_POST, [this]()
                 {
        bool success = this->startService();
        webserver.send(success ? 200 : 500, RoutesConsts::mime_json,
                      getResultJsonString(success ? RoutesConsts::result_ok : RoutesConsts::result_err,
                                        success ? "Service started" : "Failed to start service").c_str());
    });

    registerSettingsRoutes("Webcam", this);

    return true;
}

std::string WebcamService::getServiceName()
{
    return std::string(WebcamConsts::service_name);
}
std::string WebcamService::getServiceSubPath()
{    // Stream endpoint - returns MJPEG stream
    return std::string(WebcamConsts::service_path);
}
bool WebcamService::saveSettings()
{
    logger->info("Saving " + getServiceName() + " settings...");
    
    if (!initialized_)
    {
        logger->error("Cannot save settings - service not initialized");
        return false;
    }
    
    sensor_t *s = esp_camera_sensor_get();
    if (!s)
    {
        logger->error("Cannot get camera sensor for saving settings");
        return false;
    }
    
    Preferences prefs;
    if (!prefs.begin(progmem_to_string(WebcamConsts::pref_namespace).c_str(), false))
    {
        logger->error("Failed to open Preferences for saving");
        return false;
    }
    
    prefs.putInt(progmem_to_string(WebcamConsts::pref_quality).c_str(), s->status.quality);
    prefs.putInt(progmem_to_string(WebcamConsts::pref_framesize).c_str(), s->status.framesize);
    prefs.putInt(progmem_to_string(WebcamConsts::pref_brightness).c_str(), s->status.brightness);
    prefs.putInt(progmem_to_string(WebcamConsts::pref_contrast).c_str(), s->status.contrast);
    prefs.putInt(progmem_to_string(WebcamConsts::pref_saturation).c_str(), s->status.saturation);
    
    prefs.end();
    logger->info("Settings saved successfully");
    return true;
}

bool WebcamService::loadSettings()
{
    logger->info("Loading " + getServiceName() + " settings...");
    
    if (!initialized_)
    {
        logger->error("Cannot load settings - service not initialized");
        return false;
    }
    
    sensor_t *s = esp_camera_sensor_get();
    if (!s)
    {
        logger->error("Cannot get camera sensor for loading settings");
        return false;
    }
    
    Preferences prefs;
    if (!prefs.begin(progmem_to_string(WebcamConsts::pref_namespace).c_str(), true))
    {
        logger->info("No saved settings found, using defaults");
        return true;  // Not an error - just no saved settings yet
    }
    
    // Load and apply settings if they exist
    // NOTE: Framesize is loaded in initializeService() before camera initialization
    // DO NOT call set_framesize() here - it would cause buffer mismatch errors
    
    if (prefs.isKey(progmem_to_string(WebcamConsts::pref_quality).c_str()))
    {
        int quality = prefs.getInt(progmem_to_string(WebcamConsts::pref_quality).c_str(), 12);
        s->set_quality(s, quality);
        logger->info("Loaded quality: " + std::to_string(quality));
    }
    
    if (prefs.isKey(progmem_to_string(WebcamConsts::pref_brightness).c_str()))
    {
        int brightness = prefs.getInt(progmem_to_string(WebcamConsts::pref_brightness).c_str(), 0);
        s->set_brightness(s, brightness);
        logger->debug("Loaded brightness: " + std::to_string(brightness));
    }
    
    if (prefs.isKey(progmem_to_string(WebcamConsts::pref_contrast).c_str()))
    {
        int contrast = prefs.getInt(progmem_to_string(WebcamConsts::pref_contrast).c_str(), 0);
        s->set_contrast(s, contrast);
        logger->debug("Loaded contrast: " + std::to_string(contrast));
    }
    
    if (prefs.isKey(progmem_to_string(WebcamConsts::pref_saturation).c_str()))
    {
        int saturation = prefs.getInt(progmem_to_string(WebcamConsts::pref_saturation).c_str(), 0);
        s->set_saturation(s, saturation);
        logger->debug("Loaded saturation: " + std::to_string(saturation));
    }
    
    prefs.end();
    logger->info("Settings loaded successfully");
    return true;
}
