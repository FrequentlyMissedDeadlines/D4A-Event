/**
 * @file WebcamService.cpp
 * @brief Implementation for webcam service integration with the main application
 * @details Exposed routes:
 *          - GET /api/webcam/snapshot - Capture and return a JPEG snapshot from the camera
 *          - GET /api/webcam/stream - Stream MJPEG video using multipart/x-mixed-replace
 *          - GET /api/webcam/status - Get camera initialization status and current settings
 *
 */

#include "services/WebcamService.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <memory>
#include <driver/i2s.h>
#include <unihiker_k10.h>
#include "IsOpenAPIInterface.h"
#include "FlashStringHelper.h"
#include "services/AmakerBotService.h"
#include <img_converters.h>
#include <Preferences.h>
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include <esp_log.h>

// External webserver instance
extern AsyncWebServer webserver;
extern UNIHIKER_K10 unihiker;
extern AmakerBotService amakerbot_service;
QueueHandle_t xQueueCamera; // Camera frame queue from unihiker_k10

// WebcamService constants namespace
namespace WebcamConsts
{
    constexpr uint8_t camera_queue_length = 2;
    constexpr uint16_t camera_rate_ms = 50;            // 50ms timeout -> 20 per second
    constexpr framesize_t frame_size = FRAMESIZE_HVGA; // 480x320
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
    constexpr const char mime_multipart[] PROGMEM = "multipart/x-mixed-replace; boundary=frame";
    constexpr const char boundary_start[] PROGMEM = "\r\n--frame\r\nContent-Type: image/jpeg\r\nContent-Length: ";
    constexpr const char boundary_end[] PROGMEM = "\r\n\r\n";
    constexpr const char tag[] PROGMEM = "Webcam";
    constexpr const char desc_snapshot[] PROGMEM = "Capture and return a JPEG snapshot from the camera in real-time. Image format is SVGA (800x600) by default with quality setting of 12.";
    constexpr const char desc_status[] PROGMEM = "Get camera initialization status, current settings including frame size, quality, brightness, contrast, and saturation levels";
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
    constexpr uint16_t stream_delay_ms = 33; // ~30fps target frame rate

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

    // Status strings
    constexpr const char status_unknown[] PROGMEM = "unknown";
    constexpr const char status_init_failed[] PROGMEM = "init failed";
    constexpr const char status_start_failed[] PROGMEM = "start failed";
    constexpr const char status_started[] PROGMEM = "started";
    constexpr const char status_stopped[] PROGMEM = "stopped";
    constexpr const char status_stop_failed[] PROGMEM = "stop failed";
    constexpr const char service_name_value[] PROGMEM = "WebcamService";

    // Validation error messages
    constexpr const char err_quality_range[] PROGMEM = "quality must be 0-63";
    constexpr const char err_brightness_range[] PROGMEM = "brightness must be -2 to 2";
    constexpr const char err_contrast_range[] PROGMEM = "contrast must be -2 to 2";
    constexpr const char err_saturation_range[] PROGMEM = "saturation must be -2 to 2";
    constexpr const char err_framesize_range[] PROGMEM = "framesize must be 0-13 or a valid name (VGA, SVGA, etc.)";
    constexpr const char err_framesize_type[] PROGMEM = "framesize must be a number (0-13) or name (VGA, SVGA, etc.)";
    constexpr const char err_no_valid_settings[] PROGMEM = "No valid settings provided";
    constexpr const char err_framesize_pref_save[] PROGMEM = "Failed to save framesize preference";
    constexpr const char err_invalid_framesize_prefix[] PROGMEM = "Invalid framesize name: ";
    constexpr const char err_invalid_framesize_suffix[] PROGMEM = ". Valid names: 96X96, QQVGA, QCIF, HQVGA, 240X240, QVGA, CIF, HVGA, VGA, SVGA, XGA, HD, SXGA, UXGA";

    // Response messages for route registration
    constexpr const char resp_service_stopped_ok[] PROGMEM = "Service stopped successfully";
    constexpr const char resp_service_stop_failed[] PROGMEM = "Failed to stop service";
    constexpr const char resp_service_started_ok[] PROGMEM = "Service started successfully";
    constexpr const char resp_service_start_failed[] PROGMEM = "Failed to start service";
    constexpr const char desc_stop_service[] PROGMEM = "Stop the camera service (useful before changing framesize)";
    constexpr const char desc_start_service[] PROGMEM = "Start the camera service (applies saved framesize from preferences)";
    constexpr const char msg_service_stopped[] PROGMEM = "Service stopped";
    constexpr const char msg_service_start_failed[] PROGMEM = "Failed to start service";
    constexpr const char msg_framesize_restart_prefix[] PROGMEM = "Framesize ";
    constexpr const char msg_framesize_restart_suffix[] PROGMEM = " saved. Restart camera service to apply: POST /api/webcam/v1/stop then POST /api/webcam/v1/start";
    constexpr const char action_stop[] PROGMEM = "stop";
    constexpr const char action_start[] PROGMEM = "start";

    // ─── Audio streaming constants ───────────────────────────────────
    constexpr const char action_audio[] PROGMEM = "audio";
    constexpr const char desc_audio_stream[] PROGMEM = "Stream raw audio from the on-board microphone as WAV (16 kHz, 16-bit, mono PCM). "
                                                       "Runs continuously until the client disconnects. Cannot be used simultaneously with music playback.";
    constexpr const char resp_audio_ok[] PROGMEM = "WAV audio stream started successfully";
    constexpr const char resp_audio_busy[] PROGMEM = "Audio stream already active from another client";
    constexpr const char mime_wav[] PROGMEM = "audio/wav";
    constexpr const char msg_audio_started[] PROGMEM = "Audio capture task started";
    constexpr const char msg_audio_stopped[] PROGMEM = "Audio capture task stopped";
    constexpr const char msg_audio_ring_alloc_fail[] PROGMEM = "Failed to allocate audio ring buffer";
    constexpr const char msg_audio_task_fail[] PROGMEM = "Failed to create audio capture task";
    constexpr const char msg_audio_client_disconnect[] PROGMEM = "Audio stream client disconnected";
    constexpr uint32_t audio_sample_rate = 16000; ///< I2S sample rate (Hz)
    constexpr uint16_t audio_bits_per_sample = 16;
    constexpr uint16_t audio_channels = 1;   ///< Mono output (right channel = mic)
    constexpr size_t audio_i2s_chunk = 1280; ///< 20 ms of stereo 16-bit at 16 kHz
    constexpr size_t audio_task_stack = 4096;
    constexpr uint8_t audio_task_priority = 3;
    constexpr uint8_t audio_task_core = 1; ///< Core 0 — real-time
    constexpr uint8_t wav_header_size = 44;
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

    // Suppress noisy "failed to get the frame" warnings from camera HAL.
    // These occur whenever the internal capture task cannot enqueue a frame
    // (queue full) which is normal when no client is consuming frames.
    esp_log_level_set("cam_hal", ESP_LOG_ERROR);

    // Verify camera sensor is accessible
    sensor_t *s = esp_camera_sensor_get();
    if (!s)
    {
        logger->error("Failed to get camera sensor");
        setServiceStatus(INITIALIZED_FAILED);
        return false;
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
 *          and suspend frame capture
 */
bool WebcamService::stopService()
{
    logger->info("Stopping " + getServiceName() + "...");
    if (initialized_)
    {
        // Stop audio capture if running
        stopAudioCapture();

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

    // Re-suppress cam_hal warnings after camera re-registration
    esp_log_level_set("cam_hal", ESP_LOG_ERROR);

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
        logger->debug(progmem_to_string(WebcamConsts::msg_camera_capture_failed));
#endif
        return nullptr;
    }

    return latest_fb;
}

/**
 * @brief Handle MJPEG streaming request
 * @details Continuously captures frames and sends them as multipart/x-mixed-replace stream
 *          Each frame is sent with proper MIME boundaries for MJPEG protocol.
 *          Uses AsyncWebServer's chunked response with RESPONSE_TRY_AGAIN to stream
 *          JPEG frames that are larger than the TCP send buffer (~2.8 KB) by splitting
 *          each frame (boundary header + JPEG data) across multiple callback invocations.
 *
 * @note AsyncWebServer uses AsyncWebServerResponse with chunked sending
 * @note Reference: https://github.com/ESP32Async/ESPAsyncWebServer
 */
void WebcamService::handleStream(AsyncWebServerRequest *request)
{
    logger->info("Start streaming " + getServiceName() + "...");
    if (!initialized_)
    {
        request->send(503, RoutesConsts::mime_plain_text, progmem_to_string(ServiceInterfaceConsts::service_status_not_initialized).c_str());
        return;
    }

    // Mark streaming as active to prevent snapshot conflicts
    streaming_active_ = true;

    /**
     * @brief Persistent state for streaming across multiple chunked callback invocations
     * @details Each JPEG frame (typically 10-50 KB) is much larger than the TCP
     *          send buffer (~2.8 KB). This struct tracks our position within the
     *          current frame so we can send it in pieces across multiple callbacks.
     *          Freed automatically via shared_ptr when the response is destroyed.
     */
    struct StreamState
    {
        uint8_t *jpg_buf = nullptr; ///< Current JPEG frame data (malloc'd)
        size_t jpg_len = 0;         ///< Total JPEG data length
        size_t offset = 0;          ///< Bytes already sent from (header + jpeg)
        char header[80] = {};       ///< MJPEG part boundary header for current frame
        size_t header_len = 0;      ///< Length of header string

        ~StreamState()
        {
            if (jpg_buf)
            {
                free(jpg_buf);
                jpg_buf = nullptr;
            }
        }
    };

    auto state = std::make_shared<StreamState>();

    // Create async chunked response for MJPEG streaming
    AsyncWebServerResponse *response = request->beginChunkedResponse(
        progmem_to_string(WebcamConsts::mime_multipart).c_str(),
        [this, state](uint8_t *buffer, size_t maxLen, size_t index) -> size_t
        {
            // Stop streaming if service is stopped
            if (!initialized_ || service_status_ == STOPPED)
            {
                streaming_active_ = false;
                return 0; // End stream
            }

            // --- Acquire a new frame if we don't have one in progress ---
            if (!state->jpg_buf)
            {
                // IMPORTANT: This lambda runs on the async network task (lwIP).
                // Never block here - drain queue with zero timeout only.
                camera_fb_t *fb = nullptr;
                camera_fb_t *latest_fb = nullptr;

                // Drain stale frames, keep only the latest (all non-blocking)
                while (xQueueReceive(xQueueCamera, &fb, 0) == pdTRUE)
                {
                    if (latest_fb)
                    {
                        esp_camera_fb_return(latest_fb);
                    }
                    latest_fb = fb;
                }

                // No frame ready yet - tell AsyncWebServer to call us again
                if (!latest_fb || !latest_fb->buf || latest_fb->len == 0)
                {
                    if (latest_fb)
                    {
                        esp_camera_fb_return(latest_fb);
                    }
                    return RESPONSE_TRY_AGAIN;
                }

                // Convert to JPEG if needed, always copying data out so we
                // can return the camera frame buffer immediately.
                bool is_valid_jpeg = (latest_fb->len >= 2 &&
                                      latest_fb->buf[0] == 0xFF &&
                                      latest_fb->buf[1] == 0xD8);

                if (is_valid_jpeg)
                {
                    // Already JPEG - copy data so we can return the fb
                    state->jpg_len = latest_fb->len;
                    state->jpg_buf = (uint8_t *)malloc(state->jpg_len);
                    if (!state->jpg_buf)
                    {
                        esp_camera_fb_return(latest_fb);
                        return RESPONSE_TRY_AGAIN;
                    }
                    memcpy(state->jpg_buf, latest_fb->buf, state->jpg_len);
                }
                else
                {
                    // Convert RGB565 to JPEG (frame2jpg allocates via malloc)
                    bool conversion_ok = frame2jpg(latest_fb, 80,
                                                   &state->jpg_buf, &state->jpg_len);
                    if (!conversion_ok || !state->jpg_buf || state->jpg_len == 0)
                    {
                        esp_camera_fb_return(latest_fb);
                        if (state->jpg_buf)
                        {
                            free(state->jpg_buf);
                            state->jpg_buf = nullptr;
                        }
                        return RESPONSE_TRY_AGAIN;
                    }
                }

                // Return camera frame buffer as soon as possible
                esp_camera_fb_return(latest_fb);

                // Build the MJPEG part boundary header for this frame
                state->header_len = snprintf(state->header, sizeof(state->header),
                                             "\r\n--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                                             (unsigned)state->jpg_len);
                state->offset = 0;
            }

            // --- Stream current frame in chunks that fit within maxLen ---
            size_t total_frame_size = state->header_len + state->jpg_len;
            size_t remaining = total_frame_size - state->offset;
            size_t to_write = (remaining < maxLen) ? remaining : maxLen;
            size_t written = 0;
            size_t pos = state->offset;

            // Copy from header and/or JPEG data into the output buffer
            while (written < to_write)
            {
                if (pos < state->header_len)
                {
                    // Still within the boundary header
                    size_t hdr_remaining = state->header_len - pos;
                    size_t chunk = ((to_write - written) < hdr_remaining)
                                       ? (to_write - written)
                                       : hdr_remaining;
                    memcpy(buffer + written, state->header + pos, chunk);
                    written += chunk;
                    pos += chunk;
                }
                else
                {
                    // Sending JPEG payload
                    size_t jpg_offset = pos - state->header_len;
                    size_t jpg_remaining = state->jpg_len - jpg_offset;
                    size_t chunk = ((to_write - written) < jpg_remaining)
                                       ? (to_write - written)
                                       : jpg_remaining;
                    memcpy(buffer + written, state->jpg_buf + jpg_offset, chunk);
                    written += chunk;
                    pos += chunk;
                }
            }

            state->offset += written;

            // If entire frame has been sent, free JPEG buffer and prepare for next frame
            if (state->offset >= total_frame_size)
            {
                free(state->jpg_buf);
                state->jpg_buf = nullptr;
                state->jpg_len = 0;
                state->header_len = 0;
                state->offset = 0;
            }

            return written;
        });

    // Add headers for streaming
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "0");
    response->addHeader(progmem_to_string(RoutesConsts::header_access_control).c_str(), "*");

    // Set callback to clear streaming flag when connection closes
    response->setCode(200);
    request->onDisconnect([this]()
                          {
        streaming_active_ = false;
        logger->info("Client disconnected from stream"); });

    request->send(response);
    logger->info("MJPEG stream started");
}

// ═══════════════════════════════════════════════════════════════════════════
// Audio streaming implementation
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Write data into the SPSC ring buffer (producer / audio task)
 * @param data  Source bytes
 * @param len   Number of bytes to write
 * @return Number of bytes actually written (may be less if buffer full)
 */
size_t WebcamService::audioRingWrite(const uint8_t *data, size_t len)
{
    size_t w = audio_ring_w_;
    size_t r = audio_ring_r_;
    size_t free_space = (r == 0)
                            ? AUDIO_RING_SIZE - w - 1
                            : (r > w ? r - w - 1 : AUDIO_RING_SIZE - w + r - 1);
    if (len > free_space)
        len = free_space;
    if (len == 0)
        return 0;

    size_t first = AUDIO_RING_SIZE - w;
    if (first > len)
        first = len;
    memcpy(audio_ring_buf_ + w, data, first);
    if (len > first)
        memcpy(audio_ring_buf_, data + first, len - first);
    audio_ring_w_ = (w + len) % AUDIO_RING_SIZE;
    return len;
}

/**
 * @brief Read data from the SPSC ring buffer (consumer / HTTP callback)
 * @param dest  Destination buffer
 * @param len   Maximum bytes to read
 * @return Number of bytes actually read
 */
size_t WebcamService::audioRingRead(uint8_t *dest, size_t len)
{
    size_t w = audio_ring_w_;
    size_t r = audio_ring_r_;
    size_t avail = (w >= r) ? (w - r) : (AUDIO_RING_SIZE - r + w);
    if (len > avail)
        len = avail;
    if (len == 0)
        return 0;

    size_t first = AUDIO_RING_SIZE - r;
    if (first > len)
        first = len;
    memcpy(dest, audio_ring_buf_ + r, first);
    if (len > first)
        memcpy(dest + first, audio_ring_buf_, len - first);
    audio_ring_r_ = (r + len) % AUDIO_RING_SIZE;
    return len;
}

/**
 * @brief Bytes available to read from ring buffer
 */
size_t WebcamService::audioRingAvailable() const
{
    size_t w = audio_ring_w_;
    size_t r = audio_ring_r_;
    return (w >= r) ? (w - r) : (AUDIO_RING_SIZE - r + w);
}

/**
 * @brief Static FreeRTOS task entry point
 */
void WebcamService::audioCaptureTaskStatic(void *param)
{
    static_cast<WebcamService *>(param)->audioCaptureLoop();
    vTaskDelete(nullptr);
}

/**
 * @brief Continuously reads I2S stereo data, extracts the right channel
 *        (microphone) and writes mono PCM into the ring buffer.
 * @note  Runs on Core 0.  The I2S peripheral is already initialised by
 *        UNIHIKER_K10::begin().  Do NOT use this while MusicService is
 *        actively playing — the shared I2S_NUM_0 bus would conflict.
 */
void WebcamService::audioCaptureLoop()
{
    uint8_t i2s_buf[WebcamConsts::audio_i2s_chunk];      // stereo interleaved
    uint8_t mono_buf[WebcamConsts::audio_i2s_chunk / 2]; // mono right-channel
    size_t bytes_read = 0;

    // Enable the audio amplifier / microphone preamp circuit.
    // The UNIHIKER K10 board powers down the audio path by default;
    // recordSaveToTFCard() does the same before recording.
    digital_write(eAmp_Gain, 1);

    while (audio_capturing_)
    {
        // Acquire the shared I2S mutex used by the UNIHIKER library
        // (readMICData, playTone, recordSaveToTFCard all take this lock).
        if (xSemaphoreTake(xI2SMutex, pdMS_TO_TICKS(100)) != pdTRUE)
            continue; // mutex busy — retry next iteration

        esp_err_t err = i2s_read(I2S_NUM_0, i2s_buf, sizeof(i2s_buf),
                                 &bytes_read, pdMS_TO_TICKS(50));
        xSemaphoreGive(xI2SMutex);

        if (err != ESP_OK || bytes_read == 0)
            continue;

        // Extract right channel from interleaved stereo 16-bit samples
        // Layout: [L_lo, L_hi, R_lo, R_hi, L_lo, L_hi, R_lo, R_hi, ...]
        size_t stereo_frames = bytes_read / 4;
        for (size_t i = 0; i < stereo_frames; i++)
        {
            mono_buf[i * 2] = i2s_buf[i * 4 + 2];     // R low byte
            mono_buf[i * 2 + 1] = i2s_buf[i * 4 + 3]; // R high byte
        }

        audioRingWrite(mono_buf, stereo_frames * 2);
    }

    // Disable the amplifier when capture stops to save power
    digital_write(eAmp_Gain, 0);
}

/**
 * @brief Allocate ring buffer and spawn the I2S capture task
 * @return true on success
 */
bool WebcamService::startAudioCapture()
{ return false;
    //crap code
    if (audio_capturing_)
        return true; // already running

    // Allocate ring buffer on heap (freed in stopAudioCapture)
    if (!audio_ring_buf_)
    {
        audio_ring_buf_ = (uint8_t *)malloc(AUDIO_RING_SIZE);
        if (!audio_ring_buf_)
        {
            logger->error(progmem_to_string(WebcamConsts::msg_audio_ring_alloc_fail));
            return false;
        }
    }
    audio_ring_w_ = 0;
    audio_ring_r_ = 0;
    audio_capturing_ = true;

    BaseType_t ret = xTaskCreatePinnedToCore(
        audioCaptureTaskStatic, "audio_cap",
        WebcamConsts::audio_task_stack,
        this,
        WebcamConsts::audio_task_priority,
        &audio_task_,
        WebcamConsts::audio_task_core);

    if (ret != pdPASS)
    {
        logger->error(progmem_to_string(WebcamConsts::msg_audio_task_fail));
        audio_capturing_ = false;
        free(audio_ring_buf_);
        audio_ring_buf_ = nullptr;
        return false;
    }

    logger->info(progmem_to_string(WebcamConsts::msg_audio_started));
    return true;
}

/**
 * @brief Signal the capture task to stop, wait for it, and free resources
 */
void WebcamService::stopAudioCapture()
{  return ;
    //crap code
    if (!audio_capturing_)
        return;

    audio_capturing_ = false;

    // Give the task time to exit (it checks the flag every ≤50 ms)
    if (audio_task_)
    {
        // Wait up to 200 ms for the task to self-delete
        vTaskDelay(pdMS_TO_TICKS(200));
        audio_task_ = nullptr;
    }

    if (audio_ring_buf_)
    {
        free(audio_ring_buf_);
        audio_ring_buf_ = nullptr;
    }
    audio_ring_w_ = 0;
    audio_ring_r_ = 0;
    audio_streaming_active_ = false;

    logger->info(progmem_to_string(WebcamConsts::msg_audio_stopped));
}

/**
 * @brief Build a standard 44-byte WAV header for infinite PCM streaming
 * @param header  Destination buffer (must be >= 44 bytes)
 * @param rate    Sample rate in Hz
 * @param bits    Bits per sample (8 or 16)
 * @param ch      Number of channels (1 = mono, 2 = stereo)
 */
static void buildWavHeader(uint8_t *header, uint32_t rate, uint16_t bits, uint16_t ch)
{
    uint32_t byteRate = rate * ch * bits / 8;
    uint16_t blockAlign = ch * bits / 8;
    uint32_t dataSize = 0xFFFFFFFF; // "infinite" stream
    uint32_t fileSize = 0xFFFFFFFF;
    uint16_t pcmFormat = 1;
    uint32_t fmtSize = 16;

    memcpy(header + 0, "RIFF", 4);
    memcpy(header + 4, &fileSize, 4);
    memcpy(header + 8, "WAVE", 4);
    memcpy(header + 12, "fmt ", 4);
    memcpy(header + 16, &fmtSize, 4);
    memcpy(header + 20, &pcmFormat, 2);
    memcpy(header + 22, &ch, 2);
    memcpy(header + 24, &rate, 4);
    memcpy(header + 28, &byteRate, 4);
    memcpy(header + 32, &blockAlign, 2);
    memcpy(header + 34, &bits, 2);
    memcpy(header + 36, "data", 4);
    memcpy(header + 40, &dataSize, 4);
}

/**
 * @brief Handle audio streaming HTTP request
 * @details Streams raw PCM audio as an infinite WAV file (16 kHz, 16-bit, mono).
 *          A dedicated FreeRTOS task captures I2S data from the on-board microphone
 *          into a ring buffer; the AsyncWebServer chunked-response callback drains
 *          it without blocking the network stack.
 *
 *          The audio and video (MJPEG) streams use completely independent peripherals
 *          (I2S vs camera DMA) and can run simultaneously.
 *
 * @note    Only one audio stream client is supported at a time.
 * @note    Do NOT use simultaneously with MusicService playback — they share I2S_NUM_0.
 */
void WebcamService::handleAudioStream(AsyncWebServerRequest *request)
{   return;
    //crap code
    logger->info("Start audio streaming " + getServiceName() + "...");

    if (!initialized_)
    {
        request->send(503, RoutesConsts::mime_plain_text,
                      progmem_to_string(ServiceInterfaceConsts::service_status_not_initialized).c_str());
        return;
    }

    if (audio_streaming_active_)
    {
        request->send(409, RoutesConsts::mime_plain_text,
                      progmem_to_string(WebcamConsts::resp_audio_busy).c_str());
        return;
    }

    // Start the I2S capture task + ring buffer
    if (!startAudioCapture())
    {
        request->send(503, RoutesConsts::mime_plain_text,
                      progmem_to_string(WebcamConsts::msg_audio_task_fail).c_str());
        return;
    }

    audio_streaming_active_ = true;

    // Shared state for the chunked-response lambda.
    // The disconnected flag is shared between the onDisconnect callback
    // and the chunked-response lambda so the lambda can detect client
    // disconnection even when the AsyncWebServer callback is delayed.
    struct AudioState
    {
        bool header_sent = false;
        volatile bool disconnected = false;
        unsigned long last_data_sent_ms = 0;
        uint8_t wav_header[WebcamConsts::wav_header_size] = {};
    };
    auto state = std::make_shared<AudioState>();
    state->last_data_sent_ms = millis();
    buildWavHeader(state->wav_header,
                   WebcamConsts::audio_sample_rate,
                   WebcamConsts::audio_bits_per_sample,
                   WebcamConsts::audio_channels);

    AsyncWebServerResponse *response = request->beginChunkedResponse(
        progmem_to_string(WebcamConsts::mime_wav).c_str(),
        [this, state](uint8_t *buffer, size_t maxLen, size_t index) -> size_t
        {
            // Check if client disconnected (flag set by onDisconnect callback)
            // or if the capture task was stopped externally
            if (state->disconnected || !audio_capturing_)
            {
                stopAudioCapture();
                return 0; // end of stream
            }

            // First invocation — prepend the 44-byte WAV header
            if (!state->header_sent)
            {
                memcpy(buffer, state->wav_header, WebcamConsts::wav_header_size);
                size_t pcm = audioRingRead(buffer + WebcamConsts::wav_header_size,
                                           maxLen - WebcamConsts::wav_header_size);
                state->header_sent = true;
                state->last_data_sent_ms = millis();
                return WebcamConsts::wav_header_size + pcm;
            }

            // Subsequent invocations — drain ring buffer
            size_t avail = audioRingRead(buffer, maxLen);
            if (avail > 0)
            {
                state->last_data_sent_ms = millis();
                return avail;
            }

            // Ring buffer empty — check for stall timeout.
            // If the capture task is running but no data has been consumed
            // for several seconds, the client is likely gone (TCP RST lost).
            constexpr unsigned long stall_timeout_ms = 5000;
            if ((millis() - state->last_data_sent_ms) > stall_timeout_ms)
            {
                logger->info("Audio stream stall detected — stopping capture");
                stopAudioCapture();
                return 0; // end of stream
            }

            return RESPONSE_TRY_AGAIN;
        });

    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "0");
    response->addHeader(progmem_to_string(RoutesConsts::header_access_control).c_str(), "*");
    response->setCode(200);

    request->onDisconnect([this, state]()
                          {
        logger->info(progmem_to_string(WebcamConsts::msg_audio_client_disconnect));
        state->disconnected = true;
        stopAudioCapture(); });

    request->send(response);
    logger->info("WAV audio stream started");
}

void WebcamService::handleSnapshot(AsyncWebServerRequest *request)
{
    if (!checkServiceStarted(request) || (!checkIsRequestFromMaster(request, &amakerbot_service)))
        return;
    logger->info("Handling snapshot request for " + getServiceName() + "...");
    if (!initialized_)
    {
        request->send(503, RoutesConsts::mime_plain_text, progmem_to_string(WebcamConsts::msg_not_initialized).c_str());
        return;
    }

    // Reject snapshot if streaming is active
    if (streaming_active_)
    {
        request->send(409, RoutesConsts::mime_plain_text, progmem_to_string(WebcamConsts::msg_streaming_active).c_str());
        return;
    }

    camera_fb_t *fb = captureSnapshot();
    if (!fb)
    {
        request->send(503, RoutesConsts::mime_plain_text, progmem_to_string(WebcamConsts::msg_capture_error).c_str());
        return;
    }

    // Validate frame buffer content
    if (!fb->buf || fb->len == 0)
    {
        logger->error("Frame buffer is empty or invalid");
        esp_camera_fb_return(fb);
        request->send(503, RoutesConsts::mime_plain_text, "Frame buffer is empty");
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
            request->send(503, RoutesConsts::mime_plain_text, "JPEG conversion failed");
            return;
        }
        needs_free = true;
        logger->info("Converted to JPEG, size: " + std::to_string(jpg_len) + " bytes");
    }

    // Send JPEG image using AsyncWebServer
    AsyncWebServerResponse *response = request->beginResponse(200, RoutesConsts::mime_image_jpeg, jpg_buf, jpg_len);
    response->addHeader(progmem_to_string(RoutesConsts::header_content_disposition).c_str(), progmem_to_string(WebcamConsts::inline_filename).c_str());
    response->addHeader(progmem_to_string(RoutesConsts::header_access_control).c_str(), "*");
    request->send(response);

    // Clean up
    if (needs_free && jpg_buf)
    {
        free(jpg_buf);
    }
    esp_camera_fb_return(fb);
}

void WebcamService::handleStatus(AsyncWebServerRequest *request)
{
    if (!checkServiceStarted(request) || (!checkIsRequestFromMaster(request, &amakerbot_service)))
        return;
    JsonDocument doc;

    // Add standardized status fields
    doc[PSTR("servicename")] = progmem_to_string(WebcamConsts::service_name_value);

    const char *status_str = progmem_to_string(WebcamConsts::status_unknown).c_str();
    switch (service_status_)
    {
    case ServiceStatus::INITIALIZED_FAILED:
        status_str = progmem_to_string(WebcamConsts::status_init_failed).c_str();
        break;
    case ServiceStatus::START_FAILED:
        status_str = progmem_to_string(WebcamConsts::status_start_failed).c_str();
        break;
    case ServiceStatus::STARTED:
        status_str = progmem_to_string(WebcamConsts::status_started).c_str();
        break;
    case ServiceStatus::STOPPED:
        status_str = progmem_to_string(WebcamConsts::status_stopped).c_str();
        break;
    case ServiceStatus::STOP_FAILED:
        status_str = progmem_to_string(WebcamConsts::status_stop_failed).c_str();
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
            const char *frameSizes[] PROGMEM = {
                WebcamConsts::fs_96x96, WebcamConsts::fs_qqvga, WebcamConsts::fs_qcif,
                WebcamConsts::fs_hqvga, WebcamConsts::fs_240x240, WebcamConsts::fs_qvga,
                WebcamConsts::fs_cif, WebcamConsts::fs_hvga, WebcamConsts::fs_vga,
                WebcamConsts::fs_svga, WebcamConsts::fs_xga, WebcamConsts::fs_hd,
                WebcamConsts::fs_sxga, WebcamConsts::fs_uxga};
            int fsIndex = s->status.framesize;
            if (fsIndex >= 0 && fsIndex < 14)
            {
                settings[progmem_to_string(WebcamConsts::field_framesize_name)] = FPSTR(frameSizes[fsIndex]);
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
    request->send(200, RoutesConsts::mime_json, response);
}

/**
 * @brief Handle camera settings update request
 * @details Accepts JSON body with optional fields: quality, framesize, brightness, contrast, saturation
 *          Only provided fields will be updated. Values are validated before applying.
 */
void WebcamService::handleSettings(AsyncWebServerRequest *request)
{
    if (!checkServiceStarted(request) || (!checkIsRequestFromMaster(request, &amakerbot_service)))
        return;
    logger->info("Handling settings update request for " + getServiceName() + "...");

    if (!initialized_)
    {
        request->send(503, RoutesConsts::mime_json,
                      getResultJsonString(RoutesConsts::result_err,
                                          progmem_to_string(WebcamConsts::msg_not_initialized))
                          .c_str());
        return;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (!s)
    {
        request->send(503, RoutesConsts::mime_json,
                      getResultJsonString(RoutesConsts::result_err,
                                          "Failed to get camera sensor")
                          .c_str());
        return;
    }

    // Parse JSON body from request
    if (!request->hasParam("plain", true))
    {
        request->send(400, RoutesConsts::mime_json,
                      getResultJsonString(RoutesConsts::result_err,
                                          progmem_to_string(RoutesConsts::msg_invalid_json))
                          .c_str());
        return;
    }

    JsonDocument doc;
    const AsyncWebParameter *p = request->getParam("plain", true);
    DeserializationError error = deserializeJson(doc, p->value());

    if (error)
    {
        logger->error("JSON parse error: " + std::string(error.c_str()));
        request->send(400, RoutesConsts::mime_json,
                      getResultJsonString(RoutesConsts::result_err,
                                          progmem_to_string(WebcamConsts::resp_invalid_json))
                          .c_str());
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
            request->send(422, RoutesConsts::mime_json,
                          getResultJsonString(RoutesConsts::result_err,
                                              progmem_to_string(WebcamConsts::err_quality_range).c_str())
                              .c_str());
            return;
        }
    }

    // Framesize changes are DISABLED at runtime due to buffer reallocation issues
    // To change framesize: save settings, then restart the WebcamService
    if (doc[FPSTR(WebcamConsts::field_framesize)].is<JsonVariant>())
    {
        int framesize = -1;

        // Check if framesize is a string (name) or integer (numeric value)
        if (doc[FPSTR(WebcamConsts::field_framesize)].is<const char *>())
        {
            std::string fs_name = doc[FPSTR(WebcamConsts::field_framesize)].as<const char *>();
            // Convert to uppercase for case-insensitive comparison
            for (char &c : fs_name)
                c = toupper(c);

            // Map name to framesize value
            if (fs_name == "96X96")
                framesize = 0;
            else if (fs_name == "QQVGA")
                framesize = 1;
            else if (fs_name == "QCIF")
                framesize = 2;
            else if (fs_name == "HQVGA")
                framesize = 3;
            else if (fs_name == "240X240")
                framesize = 4;
            else if (fs_name == "QVGA")
                framesize = 5;
            else if (fs_name == "CIF")
                framesize = 6;
            else if (fs_name == "HVGA")
                framesize = 7;
            else if (fs_name == "VGA")
                framesize = 8;
            else if (fs_name == "SVGA")
                framesize = 9;
            else if (fs_name == "XGA")
                framesize = 10;
            else if (fs_name == "HD")
                framesize = 11;
            else if (fs_name == "SXGA")
                framesize = 12;
            else if (fs_name == "UXGA")
                framesize = 13;
            else
            {
                std::string err_msg = progmem_to_string(WebcamConsts::err_invalid_framesize_prefix) + fs_name + progmem_to_string(WebcamConsts::err_invalid_framesize_suffix);
                request->send(422, RoutesConsts::mime_json,
                              getResultJsonString(RoutesConsts::result_err, err_msg.c_str()).c_str());
                return;
            }
        }
        else if (doc[FPSTR(WebcamConsts::field_framesize)].is<int>())
        {
            framesize = doc[FPSTR(WebcamConsts::field_framesize)];
            if (framesize < 0 || framesize > 13)
            {
                request->send(422, RoutesConsts::mime_json,
                              getResultJsonString(RoutesConsts::result_err,
                                                  progmem_to_string(WebcamConsts::err_framesize_range).c_str())
                                  .c_str());
                return;
            }
        }
        else
        {
            request->send(422, RoutesConsts::mime_json,
                          getResultJsonString(RoutesConsts::result_err,
                                              progmem_to_string(WebcamConsts::err_framesize_type).c_str())
                              .c_str());
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
                const char *fs_names[] PROGMEM = {
                    WebcamConsts::fs_96x96, WebcamConsts::fs_qqvga, WebcamConsts::fs_qcif,
                    WebcamConsts::fs_hqvga, WebcamConsts::fs_240x240, WebcamConsts::fs_qvga,
                    WebcamConsts::fs_cif, WebcamConsts::fs_hvga, WebcamConsts::fs_vga,
                    WebcamConsts::fs_svga, WebcamConsts::fs_xga, WebcamConsts::fs_hd,
                    WebcamConsts::fs_sxga, WebcamConsts::fs_uxga};
                std::string fs_name = progmem_to_string(fs_names[framesize]);

                updateMsg += "framesize=" + fs_name + " (will apply on service restart) ";
                logger->info("Framesize " + fs_name + " (" + std::to_string(framesize) + ") saved to preferences - restart service to apply");

                std::string restart_msg = progmem_to_string(WebcamConsts::msg_framesize_restart_prefix) + fs_name + progmem_to_string(WebcamConsts::msg_framesize_restart_suffix);
                request->send(200, RoutesConsts::mime_json,
                              getResultJsonString(RoutesConsts::result_ok, restart_msg.c_str()).c_str());
                return;
            }
            else
            {
                request->send(503, RoutesConsts::mime_json,
                              getResultJsonString(RoutesConsts::result_err,
                                                  progmem_to_string(WebcamConsts::err_framesize_pref_save).c_str())
                                  .c_str());
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
            request->send(422, RoutesConsts::mime_json,
                          getResultJsonString(RoutesConsts::result_err,
                                              progmem_to_string(WebcamConsts::err_brightness_range).c_str())
                              .c_str());
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
            request->send(422, RoutesConsts::mime_json,
                          getResultJsonString(RoutesConsts::result_err,
                                              progmem_to_string(WebcamConsts::err_contrast_range).c_str())
                              .c_str());
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
            request->send(422, RoutesConsts::mime_json,
                          getResultJsonString(RoutesConsts::result_err,
                                              progmem_to_string(WebcamConsts::err_saturation_range).c_str())
                              .c_str());
            return;
        }
    }

    if (updated)
    {
        request->send(200, RoutesConsts::mime_json,
                      getResultJsonString(RoutesConsts::result_ok,
                                          updateMsg.c_str())
                          .c_str());
    }
    else
    {
        request->send(400, RoutesConsts::mime_json,
                      getResultJsonString(RoutesConsts::result_err,
                                          progmem_to_string(WebcamConsts::err_no_valid_settings).c_str())
                          .c_str());
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
    snapshotOk.contentType = RoutesConsts::mime_image_jpeg;
    snapshotOk.schema = "{\"type\":\"string\",\"format\":\"binary\",\"description\":\"JPEG image data\"}";
    snapshotResponses.push_back(snapshotOk);
    snapshotResponses.push_back(OpenAPIResponse(503, WebcamConsts::resp_camera_not_init));
    snapshotResponses.push_back(createServiceNotStartedResponse());
    snapshotResponses.push_back(createForbiddenResponse());
    registerOpenAPIRoute(OpenAPIRoute(path.c_str(), RoutesConsts::method_get,
                                      WebcamConsts::desc_snapshot,
                                      WebcamConsts::tag, false, {}, snapshotResponses));
    webserver.on(path.c_str(), HTTP_GET, [this](AsyncWebServerRequest *request)
                 { 
        if (!checkServiceStarted(request) || (!checkIsRequestFromMaster(request, &amakerbot_service))) return;
        handleSnapshot(request); });

    // Settings endpoint - accepts JSON body to update camera settings
    path = getPath(WebcamConsts::action_settings);
#ifdef VERBOSE_DEBUG
    logger->debug("+" + path);
#endif

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
        false);

    settingsResponses.push_back(OpenAPIResponse(200, WebcamConsts::resp_settings_ok));
    settingsResponses.push_back(OpenAPIResponse(400, WebcamConsts::resp_invalid_json));
    settingsResponses.push_back(OpenAPIResponse(422, RoutesConsts::resp_missing_params));
    settingsResponses.push_back(OpenAPIResponse(503, WebcamConsts::resp_camera_not_init));
    settingsResponses.push_back(createServiceNotStartedResponse());
    settingsResponses.push_back(createForbiddenResponse());

    OpenAPIRoute settingsRoute(path.c_str(), RoutesConsts::method_put,
                               progmem_to_string(WebcamConsts::desc_settings).c_str(),
                               WebcamConsts::tag, false, settingsParams, settingsResponses);
    settingsRoute.requestBody = settingsBody;
    registerOpenAPIRoute(settingsRoute);

    webserver.on(path.c_str(), HTTP_PUT, [this](AsyncWebServerRequest *request)
                 { 
            if (!checkServiceStarted(request) ||  (!checkIsRequestFromMaster(request, &amakerbot_service))) return;
        handleSettings(request); });

    // Stream endpoint - returns MJPEG stream
    path = getPath(WebcamConsts::action_stream);
#ifdef VERBOSE_DEBUG
    logger->debug("+" + path);
#endif
    std::vector<OpenAPIResponse> streamResponses;
    OpenAPIResponse streamOk(200, WebcamConsts::resp_stream_ok);
    streamOk.contentType = WebcamConsts::mime_multipart;
    streamOk.schema = "{\"type\":\"string\",\"format\":\"binary\",\"description\":\"Continuous MJPEG video stream\"}";
    streamResponses.push_back(streamOk);
    streamResponses.push_back(OpenAPIResponse(503, WebcamConsts::resp_camera_not_init));
    streamResponses.push_back(createServiceNotStartedResponse());
    streamResponses.push_back(createForbiddenResponse());

    registerOpenAPIRoute(OpenAPIRoute(path.c_str(), RoutesConsts::method_get,
                                      WebcamConsts::desc_stream,
                                      WebcamConsts::tag, false, {}, streamResponses));
    webserver.on(path.c_str(), HTTP_GET, [this](AsyncWebServerRequest *request)
                 { 
            if (!checkServiceStarted(request) ||  (!checkIsRequestFromMaster(request, &amakerbot_service))) return;
        handleStream(request); });

    // Audio stream endpoint - streams WAV from on-board microphone
    path = getPath(WebcamConsts::action_audio);
#ifdef VERBOSE_DEBUG
    logger->debug("+" + path);
#endif
    std::vector<OpenAPIResponse> audioResponses;
    OpenAPIResponse audioOk(200, WebcamConsts::resp_audio_ok);
    audioOk.contentType = WebcamConsts::mime_wav;
    audioOk.schema = "{\"type\":\"string\",\"format\":\"binary\",\"description\":\"Continuous WAV audio stream (16 kHz, 16-bit, mono PCM)\"}";
    audioResponses.push_back(audioOk);
    audioResponses.push_back(OpenAPIResponse(409, WebcamConsts::resp_audio_busy));
    audioResponses.push_back(OpenAPIResponse(503, WebcamConsts::resp_camera_not_init));
    audioResponses.push_back(createServiceNotStartedResponse());
    audioResponses.push_back(createForbiddenResponse());

    registerOpenAPIRoute(OpenAPIRoute(path.c_str(), RoutesConsts::method_get,
                                      WebcamConsts::desc_audio_stream,
                                      WebcamConsts::tag, false, {}, audioResponses));
    webserver.on(path.c_str(), HTTP_GET, [this](AsyncWebServerRequest *request)
                 {
            if (!checkServiceStarted(request) ||  (!checkIsRequestFromMaster(request, &amakerbot_service))) return;
        handleAudioStream(request); });

    // Stop service route - allows stopping the camera service via HTTP
    path = getPath(WebcamConsts::action_stop);
    std::vector<OpenAPIResponse> stopResponses;
    stopResponses.push_back(OpenAPIResponse(200, WebcamConsts::resp_service_stopped_ok));
    stopResponses.push_back(OpenAPIResponse(500, WebcamConsts::resp_service_stop_failed));
    stopResponses.push_back(createForbiddenResponse());
    registerOpenAPIRoute(OpenAPIRoute(path.c_str(), RoutesConsts::method_post,
                                      progmem_to_string(WebcamConsts::desc_stop_service).c_str(),
                                      WebcamConsts::tag, false, {}, stopResponses));
    webserver.on(path.c_str(), HTTP_POST, [this](AsyncWebServerRequest *request)
                 {
            if (!checkServiceStarted(request) ||  (!checkIsRequestFromMaster(request, &amakerbot_service))) return;

        bool success = this->stopService();
        request->send(success ? 200 : 500, RoutesConsts::mime_json,
                      getResultJsonString(success ? RoutesConsts::result_ok : RoutesConsts::result_err,
                                        success ? progmem_to_string(WebcamConsts::msg_service_stopped).c_str() : progmem_to_string(WebcamConsts::resp_service_stop_failed).c_str()).c_str()); });

    // Start service route - allows starting the camera service via HTTP
    path = getPath(WebcamConsts::action_start);
    std::vector<OpenAPIResponse> startResponses;
    startResponses.push_back(OpenAPIResponse(200, WebcamConsts::resp_service_started_ok));
    startResponses.push_back(OpenAPIResponse(500, WebcamConsts::resp_service_start_failed));
    startResponses.push_back(createForbiddenResponse());
    registerOpenAPIRoute(OpenAPIRoute(path.c_str(), RoutesConsts::method_post,
                                      progmem_to_string(WebcamConsts::desc_start_service).c_str(),
                                      WebcamConsts::tag, false, {}, startResponses));
    webserver.on(path.c_str(), HTTP_POST, [this](AsyncWebServerRequest *request)
                 {
            if (!checkServiceStarted(request) ||  (!checkIsRequestFromMaster(request, &amakerbot_service))) return;

        bool success = this->startService();
        request->send(success ? 200 : 500, RoutesConsts::mime_json,
                      getResultJsonString(success ? RoutesConsts::result_ok : RoutesConsts::result_err,
                                        success ? progmem_to_string(WebcamConsts::resp_service_started_ok).c_str() : progmem_to_string(WebcamConsts::msg_service_start_failed).c_str()).c_str()); });

    registerServiceStatusRoute(this);
    registerSettingsRoutes(this);

    return true;
}

std::string WebcamService::getServiceName()
{
    return std::string(WebcamConsts::service_name);
}

std::string WebcamService::getServiceSubPath()
{
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
        return true; // Not an error - just no saved settings yet
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
