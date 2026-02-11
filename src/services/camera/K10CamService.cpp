#include "K10CamService.h"
#include <WebServer.h>
#include <img_converters.h>
#include <ArduinoJson.h>
#include "../FlashStringHelper.h"

// External webserver instance
extern WebServer webserver;

// Forward declare register_camera from unihiker_k10 library
extern "C" void register_camera(pixformat_t pixel_format, framesize_t frame_size, uint8_t fb_count, QueueHandle_t queue);

// K10 Board Camera Pin Definitions (from who_camera.h)
#define CAMERA_PIN_PWDN    -1
#define CAMERA_PIN_RESET   -1
#define CAMERA_PIN_XCLK     7
#define CAMERA_PIN_SIOD    47
#define CAMERA_PIN_SIOC    48
#define CAMERA_PIN_D7       6
#define CAMERA_PIN_D6      15
#define CAMERA_PIN_D5      16
#define CAMERA_PIN_D4      18
#define CAMERA_PIN_D3       9
#define CAMERA_PIN_D2      11
#define CAMERA_PIN_D1      10
#define CAMERA_PIN_D0       8
#define CAMERA_PIN_VSYNC    4
#define CAMERA_PIN_HREF     5
#define CAMERA_PIN_PCLK    17
#define XCLK_FREQ_HZ 16000000

namespace CamConsts
{   constexpr const char resp_access_control[] PROGMEM = "Access-Control-Allow-Origin";
    constexpr const char resp_content_disposition[] PROGMEM = "Content-Disposition";
constexpr const char resp_inline_filename[] PROGMEM = "inline; filename=snapshot.jpg";
constexpr const char resp_boundary_end[] PROGMEM = "\r\n\r\n";
constexpr const char resp_boundary_start[] PROGMEM = "\r\n--frame\r\nContent-Type: image/jpeg\r\nContent-Length: ";
    constexpr uint16_t stream_delay_ms = 10;        
        
    constexpr const char service_name[] PROGMEM = "Camera";
    constexpr const char service_path[] PROGMEM = "mycam/v1";
    constexpr const char action_snapshot[] PROGMEM = "snapshot";
    constexpr const char action_stream[] PROGMEM = "stream";
    constexpr const char action_framesize[] PROGMEM = "framesize";
    constexpr const char action_vflip[] PROGMEM = "vflip";
    constexpr const char action_hmirror[] PROGMEM = "hmirror";
    constexpr const char action_contrast[] PROGMEM = "contrast";
    constexpr const char action_brightness[] PROGMEM = "brightness";
    constexpr const char desc_snapshot[] PROGMEM = "Capture and return a JPEG snapshot from camera";
    constexpr const char desc_stream[] PROGMEM = "Stream MJPEG video from camera using multipart/x-mixed-replace protocol";
    constexpr const char desc_get_framesize[] PROGMEM = "Get current camera resolution";
    constexpr const char desc_set_framesize[] PROGMEM = "Set camera resolution (0-13: QQVGA to QSXGA)";
    constexpr const char desc_get_vflip[] PROGMEM = "Get vertical flip state";
    constexpr const char desc_set_vflip[] PROGMEM = "Set vertical flip (true/false)";
    constexpr const char desc_get_hmirror[] PROGMEM = "Get horizontal mirror state";
    constexpr const char desc_set_hmirror[] PROGMEM = "Set horizontal mirror (true/false)";
    constexpr const char desc_get_contrast[] PROGMEM = "Get current camera contrast level";
    constexpr const char desc_set_contrast[] PROGMEM = "Set camera contrast level (-2 to +2)";
    constexpr const char desc_get_brightness[] PROGMEM = "Get current camera brightness level";
    constexpr const char desc_set_brightness[] PROGMEM = "Set camera brightness level (-2 to +2)";
    constexpr const char resp_snapshot_ok[] PROGMEM = "Snapshot captured successfully";
    constexpr const char resp_stream_ok[] PROGMEM = "MJPEG stream started successfully";
    constexpr const char resp_setting_ok[] PROGMEM = "Setting updated successfully";
    constexpr const char resp_setting_retrieved[] PROGMEM = "Setting retrieved successfully";
    constexpr const char resp_camera_not_init[] PROGMEM = "Camera not initialized";
    constexpr const char resp_capture_failed[] PROGMEM = "Failed to capture frame";
    constexpr const char resp_invalid_value[] PROGMEM = "Invalid value";
    constexpr const char tag[] PROGMEM = "My Camera";
    constexpr const char mime_image_jpeg[] PROGMEM = "image/jpeg";
    constexpr const char mime_multipart[] PROGMEM = "multipart/x-mixed-replace; boundary=frame";
    constexpr const char boundary_start[] PROGMEM = "\r\n--frame\r\nContent-Type: image/jpeg\r\nContent-Length: ";
    constexpr const char boundary_end[] PROGMEM = "\r\n\r\n";
    constexpr const char content_disposition[] PROGMEM = "Content-Disposition";
    constexpr const char inline_filename[] PROGMEM = "inline; filename=snapshot.jpg";
    constexpr const char access_control[] PROGMEM = "Access-Control-Allow-Origin";
    constexpr const char field_framesize[] PROGMEM = "framesize";
    constexpr const char field_vflip[] PROGMEM = "vflip";
    constexpr const char field_hmirror[] PROGMEM = "hmirror";
    constexpr const char field_contrast[] PROGMEM = "contrast";
    constexpr const char field_brightness[] PROGMEM = "brightness";
    constexpr const char field_enabled[] PROGMEM = "enabled";
    constexpr const char field_level[] PROGMEM = "level";
    constexpr const char field_value[] PROGMEM = "value";

}
bool camera_initialized_ = false;
camera_config_t K10CamService::getCameraConfig()
{
    camera_config_t config;
    
    // Pin configuration for UniHiker K10 board
    config.pin_pwdn     = CAMERA_PIN_PWDN;
    config.pin_reset    = CAMERA_PIN_RESET;
    config.pin_xclk     = CAMERA_PIN_XCLK;
    config.pin_sscb_sda = CAMERA_PIN_SIOD;
    config.pin_sscb_scl = CAMERA_PIN_SIOC;
    
    config.pin_d7       = CAMERA_PIN_D7;
    config.pin_d6       = CAMERA_PIN_D6;
    config.pin_d5       = CAMERA_PIN_D5;
    config.pin_d4       = CAMERA_PIN_D4;
    config.pin_d3       = CAMERA_PIN_D3;
    config.pin_d2       = CAMERA_PIN_D2;
    config.pin_d1       = CAMERA_PIN_D1;
    config.pin_d0       = CAMERA_PIN_D0;
    
    config.pin_vsync    = CAMERA_PIN_VSYNC;
    config.pin_href     = CAMERA_PIN_HREF;
    config.pin_pclk     = CAMERA_PIN_PCLK;
    
    // XCLK settings
    config.xclk_freq_hz = XCLK_FREQ_HZ;
    config.ledc_timer   = LEDC_TIMER_0;
    config.ledc_channel = LEDC_CHANNEL_0;
    
    // Image settings - GC2145 supports JPEG output natively
    config.pixel_format = PIXFORMAT_RGB565;   // GC2145 can output JPEG directly
    config.frame_size   = FRAMESIZE_QVGA;   // 320x240
    config.jpeg_quality = 10;               // 0-63, lower means higher quality
    config.fb_count     = 1;                // Single buffer for JPEG
    config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
    
    return config;
}

bool K10CamService::initializeService()
{
    logger->info("Initializing " + getServiceName() + "...");
    
    // Get camera configuration for K10 board
    camera_config_t config = getCameraConfig();
    
    // Initialize camera
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        logger->error("Camera init failed with error 0x" + std::string(String(err, HEX).c_str()));
        setServiceStatus(INITIALIZED_FAILED);
        return false;
    }
    
    // Get sensor handle for additional configuration
    sensor_t *s = esp_camera_sensor_get();
    if (s)
    {
        // GC2145 color and exposure configuration
        s->set_whitebal(s, 1);      // Enable auto white balance
        s->set_awb_gain(s, 1);      // Enable AWB gain for better colors
        s->set_gain_ctrl(s, 1);     // Enable AGC
        s->set_exposure_ctrl(s, 1); // Enable AEC
        
        // Image quality settings for GC2145
        s->set_brightness(s, 0);    // -2 to 2
        s->set_contrast(s, 0);      // -2 to 2
        s->set_saturation(s, 0);    // -2 to 2 (important for color accuracy)
        s->set_sharpness(s, 0);     // -2 to 2
        
        // Set JPEG quality
        if (s->set_quality)
            s->set_quality(s, 10);  // Lower = higher quality
    }
    
    if (!s)
    {
        logger->error("Failed to get camera sensor");
        setServiceStatus(INITIALIZED_FAILED);
        return false;
    }
    
    // Optional: Configure sensor settings
    // s->set_vflip(s, 1);        // Flip vertically
    // s->set_hmirror(s, 1);      // Mirror horizontally
    // s->set_brightness(s, 0);   // -2 to 2
    // s->set_contrast(s, 0);     // -2 to 2
    // s->set_saturation(s, 0);   // -2 to 2
    
    camera_initialized_ = true;
    setServiceStatus(INITIALIZED);
    logger->info(getServiceName() + " initialized successfully");
    
    return true;
}

bool K10CamService::startService()
{
    logger->info("Starting " + getServiceName() + "...");
    
    if (!camera_initialized_)
    {
        logger->error("Cannot start service - camera not initialized");
        setServiceStatus(START_FAILED);
        return false;
    }
    
    setServiceStatus(STARTED);
    logger->info(getServiceName() + " started successfully");
    
    return true;
}

bool K10CamService::stopService()
{
    logger->info("Stopping " + getServiceName() + "...");
    
    if (isServiceInitialized())
    {
        // Deinitialize camera
        esp_err_t err = esp_camera_deinit();
        if (err != ESP_OK)
        {
            logger->error("Camera deinit failed with error 0x" + std::string(String(err, HEX).c_str()));
            return false;
        }
         }
    
    setServiceStatus(STOPPED);
    logger->info(getServiceName() + " stopped successfully");
    
    return true;
}

std::string K10CamService::getServiceName()
{
    return String(FPSTR(CamConsts::service_name)).c_str();
}

std::string K10CamService::getServiceSubPath()
{
    return String(FPSTR(CamConsts::service_path)).c_str();
}

bool K10CamService::setFramesize(framesize_t framesize)
{
    if (!isServiceStarted())
    {
        logger->error("Cannot set framesize: camera service not started");
        return false;
    }
    
    sensor_t *s = esp_camera_sensor_get();
    if (!s || !s->set_framesize)
    {
        logger->error("Failed to get camera sensor or framesize not supported");
        return false;
    }
    
    s->set_framesize(s, framesize);
    logger->info("Camera framesize set to: " + std::to_string(framesize));
    return true;
}

framesize_t K10CamService::getFramesize()
{
    if (!isServiceStarted())
    {
        return FRAMESIZE_QVGA;
    }
    
    sensor_t *s = esp_camera_sensor_get();
    if (!s)
    {
        return FRAMESIZE_QVGA;
    }
    
    return s->status.framesize;
}

bool K10CamService::setVFlip(bool enable)
{
    if (!isServiceStarted())
    {
        logger->error("Cannot set vflip: camera service not started");
        return false;
    }
    
    sensor_t *s = esp_camera_sensor_get();
    if (!s || !s->set_vflip)
    {
        logger->error("Failed to get camera sensor or vflip not supported");
        return false;
    }
    
    s->set_vflip(s, enable ? 1 : 0);
    logger->info("Camera vflip set to: " + std::string(enable ? "true" : "false"));
    return true;
}

bool K10CamService::getVFlip()
{
    if (!isServiceStarted())
    {
        return false;
    }
    
    sensor_t *s = esp_camera_sensor_get();
    if (!s)
    {
        return false;
    }
    
    return s->status.vflip;
}

bool K10CamService::setHMirror(bool enable)
{
    if (!isServiceStarted())
    {
        logger->error("Cannot set hmirror: camera service not started");
        return false;
    }
    
    sensor_t *s = esp_camera_sensor_get();
    if (!s || !s->set_hmirror)
    {
        logger->error("Failed to get camera sensor or hmirror not supported");
        return false;
    }
    
    s->set_hmirror(s, enable ? 1 : 0);
    logger->info("Camera hmirror set to: " + std::string(enable ? "true" : "false"));
    return true;
}

bool K10CamService::getHMirror()
{
    if (!isServiceStarted())
    {
        return false;
    }
    
    sensor_t *s = esp_camera_sensor_get();
    if (!s)
    {
        return false;
    }
    
    return s->status.hmirror;
}

bool K10CamService::setContrast(int8_t level)
{
    if (!isServiceStarted())
    {
        logger->error("Cannot set contrast: camera service not started");
        return false;
    }
    
    if (level < -2) level = -2;
    if (level > 2) level = 2;
    
    sensor_t *s = esp_camera_sensor_get();
    if (!s || !s->set_contrast)
    {
        logger->error("Failed to get camera sensor or contrast not supported");
        return false;
    }
    
    s->set_contrast(s, level);
    logger->info("Camera contrast set to: " + std::to_string(level));
    return true;
}

int8_t K10CamService::getContrast()
{
    if (!isServiceStarted())
    {
        return 0;
    }
    
    sensor_t *s = esp_camera_sensor_get();
    if (!s)
    {
        return 0;
    }
    
    return s->status.contrast;
}

bool K10CamService::setBrightness(int8_t level)
{
    if (!isServiceStarted())
    {
        logger->error("Cannot set brightness: camera service not started");
        return false;
    }
    
    if (level < -2) level = -2;
    if (level > 2) level = 2;
    
    sensor_t *s = esp_camera_sensor_get();
    if (!s || !s->set_brightness)
    {
        logger->error("Failed to get camera sensor or brightness not supported");
        return false;
    }
    
    s->set_brightness(s, level);
    logger->info("Camera brightness set to: " + std::to_string(level));
    return true;
}

int8_t K10CamService::getBrightness()
{
    if (!isServiceStarted())
    {
        return 0;
    }
    
    sensor_t *s = esp_camera_sensor_get();
    if (!s)
    {
        return 0;
    }
    
    return s->status.brightness;
}

void K10CamService::handleSnapshot()
{
    logger->info("Handling snapshot request for " + getServiceName() + "...");
    
    if (!isServiceStarted())
    {
        webserver.send(503, RoutesConsts::mime_plain_text, FPSTR(CamConsts::resp_camera_not_init));
        return;
    }
    
    // Capture frame from camera
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
        logger->error("Camera capture failed");
        webserver.send(503, RoutesConsts::mime_plain_text, FPSTR(CamConsts::resp_capture_failed));
        return;
    }
    
    // Validate frame buffer
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
    webserver.sendHeader(FPSTR(CamConsts::resp_content_disposition), FPSTR(CamConsts::resp_inline_filename));
    webserver.sendHeader(FPSTR(CamConsts::resp_access_control), "*");
    webserver.setContentLength(jpg_len);
    webserver.send(200, FPSTR(RoutesConsts::mime_image_jpeg), "");
    webserver.sendContent(reinterpret_cast<const char *>(jpg_buf), jpg_len);

    // Clean up
    if (needs_free && jpg_buf)
    {
        free(jpg_buf);
    }
    esp_camera_fb_return(fb);
}

void K10CamService::handleStream()
{    logger->info("Handling streaming request for " + getServiceName() + "...");
    if (!isServiceStarted())
    {
        webserver.send(503, RoutesConsts::mime_plain_text, FPSTR(RoutesConsts::resp_not_initialized));
        return;
    }

    // Set streaming flag to block snapshot requests
    streaming_active_ = true;

    // Send multipart stream headers
    webserver.setContentLength(CONTENT_LENGTH_UNKNOWN);
    webserver.send(200, FPSTR(RoutesConsts::mime_multipart_x_mixed_replace), "");

    // Continuous streaming loop
    while (webserver.client().connected())
    {
        // Capture frame from camera
        camera_fb_t *fb = esp_camera_fb_get();
       if (!fb)
        {
            delay(CamConsts::stream_delay_ms);
            continue;
        }

        // Validate frame buffer
        if (!fb->buf || fb->len == 0)
        {
            esp_camera_fb_return(fb);
            delay(CamConsts::stream_delay_ms);
            continue;
        }
        
        // Validate frame buffer
        if (!fb->buf || fb->len == 0)
        {
            esp_camera_fb_return(fb);
            delay(CamConsts::stream_delay_ms);
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
                delay(CamConsts::stream_delay_ms);
                continue;
            }
            needs_free = true;
        }

        // Send multipart boundary and frame
        webserver.sendContent(FPSTR(CamConsts::resp_boundary_start));
        webserver.sendContent(String(jpg_len));
        webserver.sendContent(FPSTR(CamConsts::resp_boundary_end));
        webserver.sendContent(reinterpret_cast<const char *>(jpg_buf), jpg_len);

        // Cleanup
        if (needs_free && jpg_buf)
        {
            free(jpg_buf);
        }
        esp_camera_fb_return(fb);

        // Small delay to control frame rate
        delay(CamConsts::stream_delay_ms);
    }
    
    logger->info("Stream ended for " + getServiceName());
}

void K10CamService::handleStatus()
{
    JsonDocument doc;
    
    if (isServiceStarted())
    {
        doc[RoutesConsts::field_status] = "started";
        doc[FPSTR(CamConsts::field_framesize)] = getFramesize();
        doc[FPSTR(CamConsts::field_vflip)] = getVFlip();
        doc[FPSTR(CamConsts::field_hmirror)] = getHMirror();
        doc[FPSTR(CamConsts::field_contrast)] = getContrast();
        doc[FPSTR(CamConsts::field_brightness)] = getBrightness();
    }
    else
    {
        doc[RoutesConsts::field_status] = "not_started";
    }
    
    String response;
    serializeJson(doc, response);
    webserver.send(200, RoutesConsts::mime_json, response);
}

bool K10CamService::registerRoutes()
{
    // Snapshot endpoint - returns JPEG image
    std::string path = getPath(CamConsts::action_snapshot);
    
#ifdef VERBOSE_DEBUG
    logger->debug("+" + path);
#endif
    
    logger->info("Add " + path + " route");
    
    // Define OpenAPI responses
    std::vector<OpenAPIResponse> snapshotResponses;
    OpenAPIResponse snapshotOk(200, CamConsts::resp_snapshot_ok);
    snapshotOk.contentType = CamConsts::mime_image_jpeg;
    snapshotOk.schema = "{\"type\":\"string\",\"format\":\"binary\",\"description\":\"JPEG image data\"}";
    snapshotResponses.push_back(snapshotOk);
    snapshotResponses.push_back(OpenAPIResponse(503, CamConsts::resp_camera_not_init));
    snapshotResponses.push_back(createServiceNotStartedResponse());
    
    // Register OpenAPI route
    registerOpenAPIRoute(OpenAPIRoute(path.c_str(), RoutesConsts::method_get,
                                      CamConsts::desc_snapshot,
                                      CamConsts::tag, false, {}, snapshotResponses));
    
    // Register WebServer handler
    webserver.on(path.c_str(), HTTP_GET, [this]()
                 { 
        if (!checkServiceStarted()) return;
        handleSnapshot(); });
    
    // Stream endpoint - returns MJPEG stream
    path = getPath(CamConsts::action_stream);
    
#ifdef VERBOSE_DEBUG
    logger->debug("+" + path);
#endif
    
    logger->info("Add " + path + " route");
    
    // Define OpenAPI responses for stream
    std::vector<OpenAPIResponse> streamResponses;
    OpenAPIResponse streamOk(200, CamConsts::resp_stream_ok);
    streamOk.contentType = CamConsts::mime_multipart;
    streamOk.schema = "{\"type\":\"string\",\"format\":\"binary\",\"description\":\"Continuous MJPEG video stream\"}";
    streamResponses.push_back(streamOk);
    streamResponses.push_back(OpenAPIResponse(503, CamConsts::resp_camera_not_init));
    streamResponses.push_back(createServiceNotStartedResponse());
    
    // Register OpenAPI route for stream
    registerOpenAPIRoute(OpenAPIRoute(path.c_str(), RoutesConsts::method_get,
                                      CamConsts::desc_stream,
                                      CamConsts::tag, false, {}, streamResponses));
    
    // Register WebServer handler for stream
    webserver.on(path.c_str(), HTTP_GET, [this]()
                 { 
        if (!checkServiceStarted()) return;
        handleStream(); });
    
    // Framesize GET/POST endpoints
    path = getPath(CamConsts::action_framesize);
    logger->info("Add " + path + " GET route");
    
    std::vector<OpenAPIResponse> framesizeGetResponses;
    OpenAPIResponse framesizeGetOk(200, CamConsts::resp_setting_retrieved);
    framesizeGetOk.contentType = RoutesConsts::mime_json;
    framesizeGetOk.schema = "{\"type\":\"object\",\"properties\":{\"framesize\":{\"type\":\"integer\"}}}";
    framesizeGetResponses.push_back(framesizeGetOk);
    framesizeGetResponses.push_back(OpenAPIResponse(503, CamConsts::resp_camera_not_init));
    framesizeGetResponses.push_back(createServiceNotStartedResponse());
    
    registerOpenAPIRoute(OpenAPIRoute(path.c_str(), RoutesConsts::method_get,
                                      CamConsts::desc_get_framesize,
                                      CamConsts::tag, false, {}, framesizeGetResponses));
    webserver.on(path.c_str(), HTTP_GET, [this]()
                 {
        if (!checkServiceStarted()) return;
        JsonDocument doc;
        doc[FPSTR(CamConsts::field_framesize)] = getFramesize();
        String response;
        serializeJson(doc, response);
        webserver.send(200, RoutesConsts::mime_json, response); });
    
    logger->info("Add " + path + " POST route");
    std::vector<OpenAPIParameter> framesizeParams;
    framesizeParams.push_back(OpenAPIParameter("value", "integer", "body", "Framesize value (0-13)", true));
    
    std::vector<OpenAPIResponse> framesizeSetResponses;
    framesizeSetResponses.push_back(OpenAPIResponse(200, CamConsts::resp_setting_ok));
    framesizeSetResponses.push_back(OpenAPIResponse(400, CamConsts::resp_invalid_value));
    framesizeSetResponses.push_back(createServiceNotStartedResponse());
    
    registerOpenAPIRoute(OpenAPIRoute(path.c_str(), RoutesConsts::method_post,
                                      CamConsts::desc_set_framesize,
                                      CamConsts::tag, false, framesizeParams, framesizeSetResponses));
    webserver.on(path.c_str(), HTTP_POST, [this]()
                 {
        if (!checkServiceStarted()) return;
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, webserver.arg("plain"));
        if (error || !doc[FPSTR(CamConsts::field_value)].is<int>()) {
            webserver.send(400, RoutesConsts::mime_json, "{\"error\":\"Invalid JSON or missing value\"}");
            return;
        }
        int value = doc[FPSTR(CamConsts::field_value)];
        if (setFramesize((framesize_t)value)) {
            webserver.send(200, RoutesConsts::mime_json, "{\"message\":\"Framesize updated\"}");
        } else {
            webserver.send(400, RoutesConsts::mime_json, "{\"error\":\"Failed to set framesize\"}");
        } });
    
    // VFlip GET/POST endpoints
    path = getPath(CamConsts::action_vflip);
    logger->info("Add " + path + " GET route");
    
    std::vector<OpenAPIResponse> vflipGetResponses;
    OpenAPIResponse vflipGetOk(200, CamConsts::resp_setting_retrieved);
    vflipGetOk.contentType = RoutesConsts::mime_json;
    vflipGetOk.schema = "{\"type\":\"object\",\"properties\":{\"enabled\":{\"type\":\"boolean\"}}}";
    vflipGetResponses.push_back(vflipGetOk);
    vflipGetResponses.push_back(OpenAPIResponse(503, CamConsts::resp_camera_not_init));
    vflipGetResponses.push_back(createServiceNotStartedResponse());
    
    registerOpenAPIRoute(OpenAPIRoute(path.c_str(), RoutesConsts::method_get,
                                      CamConsts::desc_get_vflip,
                                      CamConsts::tag, false, {}, vflipGetResponses));
    webserver.on(path.c_str(), HTTP_GET, [this]()
                 {
        if (!checkServiceStarted()) return;
        JsonDocument doc;
        doc[FPSTR(CamConsts::field_enabled)] = getVFlip();
        String response;
        serializeJson(doc, response);
        webserver.send(200, RoutesConsts::mime_json, response); });
    
    logger->info("Add " + path + " POST route");
    std::vector<OpenAPIParameter> vflipParams;
    vflipParams.push_back(OpenAPIParameter("enabled", "boolean", "body", "Enable vertical flip", true));
    
    std::vector<OpenAPIResponse> vflipSetResponses;
    vflipSetResponses.push_back(OpenAPIResponse(200, CamConsts::resp_setting_ok));
    vflipSetResponses.push_back(OpenAPIResponse(400, CamConsts::resp_invalid_value));
    vflipSetResponses.push_back(createServiceNotStartedResponse());
    
    registerOpenAPIRoute(OpenAPIRoute(path.c_str(), RoutesConsts::method_post,
                                      CamConsts::desc_set_vflip,
                                      CamConsts::tag, false, vflipParams, vflipSetResponses));
    webserver.on(path.c_str(), HTTP_POST, [this]()
                 {
        if (!checkServiceStarted()) return;
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, webserver.arg("plain"));
        if (error || !doc[FPSTR(CamConsts::field_enabled)].is<bool>()) {
            webserver.send(400, RoutesConsts::mime_json, "{\"error\":\"Invalid JSON or missing enabled\"}");
            return;
        }
        bool enabled = doc[FPSTR(CamConsts::field_enabled)];
        if (setVFlip(enabled)) {
            webserver.send(200, RoutesConsts::mime_json, "{\"message\":\"VFlip updated\"}");
        } else {
            webserver.send(400, RoutesConsts::mime_json, "{\"error\":\"Failed to set vflip\"}");
        } });
    
    // HMirror GET/POST endpoints
    path = getPath(CamConsts::action_hmirror);
    logger->info("Add " + path + " GET route");
    
    std::vector<OpenAPIResponse> hmirrorGetResponses;
    OpenAPIResponse hmirrorGetOk(200, CamConsts::resp_setting_retrieved);
    hmirrorGetOk.contentType = RoutesConsts::mime_json;
    hmirrorGetOk.schema = "{\"type\":\"object\",\"properties\":{\"enabled\":{\"type\":\"boolean\"}}}";
    hmirrorGetResponses.push_back(hmirrorGetOk);
    hmirrorGetResponses.push_back(OpenAPIResponse(503, CamConsts::resp_camera_not_init));
    hmirrorGetResponses.push_back(createServiceNotStartedResponse());
    
    registerOpenAPIRoute(OpenAPIRoute(path.c_str(), RoutesConsts::method_get,
                                      CamConsts::desc_get_hmirror,
                                      CamConsts::tag, false, {}, hmirrorGetResponses));
    webserver.on(path.c_str(), HTTP_GET, [this]()
                 {
        if (!checkServiceStarted()) return;
        JsonDocument doc;
        doc[FPSTR(CamConsts::field_enabled)] = getHMirror();
        String response;
        serializeJson(doc, response);
        webserver.send(200, RoutesConsts::mime_json, response); });
    
    logger->info("Add " + path + " POST route");
    std::vector<OpenAPIParameter> hmirrorParams;
    hmirrorParams.push_back(OpenAPIParameter("enabled", "boolean", "body", "Enable horizontal mirror", true));
    
    std::vector<OpenAPIResponse> hmirrorSetResponses;
    hmirrorSetResponses.push_back(OpenAPIResponse(200, CamConsts::resp_setting_ok));
    hmirrorSetResponses.push_back(OpenAPIResponse(400, CamConsts::resp_invalid_value));
    hmirrorSetResponses.push_back(createServiceNotStartedResponse());
    
    registerOpenAPIRoute(OpenAPIRoute(path.c_str(), RoutesConsts::method_post,
                                      CamConsts::desc_set_hmirror,
                                      CamConsts::tag, false, hmirrorParams, hmirrorSetResponses));
    webserver.on(path.c_str(), HTTP_POST, [this]()
                 {
        if (!checkServiceStarted()) return;
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, webserver.arg("plain"));
        if (error || !doc[FPSTR(CamConsts::field_enabled)].is<bool>()) {
            webserver.send(400, RoutesConsts::mime_json, "{\"error\":\"Invalid JSON or missing enabled\"}");
            return;
        }
        bool enabled = doc[FPSTR(CamConsts::field_enabled)];
        if (setHMirror(enabled)) {
            webserver.send(200, RoutesConsts::mime_json, "{\"message\":\"HMirror updated\"}");
        } else {
            webserver.send(400, RoutesConsts::mime_json, "{\"error\":\"Failed to set hmirror\"}");
        } });
    
    // Contrast GET/POST endpoints
    path = getPath(CamConsts::action_contrast);
    logger->info("Add " + path + " GET route");
    
    std::vector<OpenAPIResponse> contrastGetResponses;
    OpenAPIResponse contrastGetOk(200, CamConsts::resp_setting_retrieved);
    contrastGetOk.contentType = RoutesConsts::mime_json;
    contrastGetOk.schema = "{\"type\":\"object\",\"properties\":{\"level\":{\"type\":\"integer\",\"minimum\":-2,\"maximum\":2}}}";
    contrastGetResponses.push_back(contrastGetOk);
    contrastGetResponses.push_back(OpenAPIResponse(503, CamConsts::resp_camera_not_init));
    contrastGetResponses.push_back(createServiceNotStartedResponse());
    
    registerOpenAPIRoute(OpenAPIRoute(path.c_str(), RoutesConsts::method_get,
                                      CamConsts::desc_get_contrast,
                                      CamConsts::tag, false, {}, contrastGetResponses));
    webserver.on(path.c_str(), HTTP_GET, [this]()
                 {
        if (!checkServiceStarted()) return;
        JsonDocument doc;
        doc[FPSTR(CamConsts::field_level)] = getContrast();
        String response;
        serializeJson(doc, response);
        webserver.send(200, RoutesConsts::mime_json, response); });
    
    logger->info("Add " + path + " POST route");
    std::vector<OpenAPIParameter> contrastParams;
    contrastParams.push_back(OpenAPIParameter("level", "integer", "body", "Contrast level (-2 to +2)", true));
    
    std::vector<OpenAPIResponse> contrastSetResponses;
    contrastSetResponses.push_back(OpenAPIResponse(200, CamConsts::resp_setting_ok));
    contrastSetResponses.push_back(OpenAPIResponse(400, CamConsts::resp_invalid_value));
    contrastSetResponses.push_back(createServiceNotStartedResponse());
    
    registerOpenAPIRoute(OpenAPIRoute(path.c_str(), RoutesConsts::method_post,
                                      CamConsts::desc_set_contrast,
                                      CamConsts::tag, false, contrastParams, contrastSetResponses));
    webserver.on(path.c_str(), HTTP_POST, [this]()
                 {
        if (!checkServiceStarted()) return;
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, webserver.arg("plain"));
        if (error || !doc[FPSTR(CamConsts::field_level)].is<int>()) {
            webserver.send(400, RoutesConsts::mime_json, "{\"error\":\"Invalid JSON or missing level\"}");
            return;
        }
        int8_t level = doc[FPSTR(CamConsts::field_level)];
        if (setContrast(level)) {
            webserver.send(200, RoutesConsts::mime_json, "{\"message\":\"Contrast updated\"}");
        } else {
            webserver.send(400, RoutesConsts::mime_json, "{\"error\":\"Failed to set contrast\"}");
        } });
    
    // Brightness GET/POST endpoints
    path = getPath(CamConsts::action_brightness);
    logger->info("Add " + path + " GET route");
    
    std::vector<OpenAPIResponse> brightnessGetResponses;
    OpenAPIResponse brightnessGetOk(200, CamConsts::resp_setting_retrieved);
    brightnessGetOk.contentType = RoutesConsts::mime_json;
    brightnessGetOk.schema = "{\"type\":\"object\",\"properties\":{\"level\":{\"type\":\"integer\",\"minimum\":-2,\"maximum\":2}}}";
    brightnessGetResponses.push_back(brightnessGetOk);
    brightnessGetResponses.push_back(OpenAPIResponse(503, CamConsts::resp_camera_not_init));
    brightnessGetResponses.push_back(createServiceNotStartedResponse());
    
    registerOpenAPIRoute(OpenAPIRoute(path.c_str(), RoutesConsts::method_get,
                                      CamConsts::desc_get_brightness,
                                      CamConsts::tag, false, {}, brightnessGetResponses));
    webserver.on(path.c_str(), HTTP_GET, [this]()
                 {
        if (!checkServiceStarted()) return;
        JsonDocument doc;
        doc[FPSTR(CamConsts::field_level)] = getBrightness();
        String response;
        serializeJson(doc, response);
        webserver.send(200, RoutesConsts::mime_json, response); });
    
    logger->info("Add " + path + " POST route");
    std::vector<OpenAPIParameter> brightnessParams;
    brightnessParams.push_back(OpenAPIParameter("level", "integer", "body", "Brightness level (-2 to +2)", true));
    
    std::vector<OpenAPIResponse> brightnessSetResponses;
    brightnessSetResponses.push_back(OpenAPIResponse(200, CamConsts::resp_setting_ok));
    brightnessSetResponses.push_back(OpenAPIResponse(400, CamConsts::resp_invalid_value));
    brightnessSetResponses.push_back(createServiceNotStartedResponse());
    
    registerOpenAPIRoute(OpenAPIRoute(path.c_str(), RoutesConsts::method_post,
                                      CamConsts::desc_set_brightness,
                                      CamConsts::tag, false, brightnessParams, brightnessSetResponses));
    webserver.on(path.c_str(), HTTP_POST, [this]()
                 {
        if (!checkServiceStarted()) return;
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, webserver.arg("plain"));
        if (error || !doc[FPSTR(CamConsts::field_level)].is<int>()) {
            webserver.send(400, RoutesConsts::mime_json, "{\"error\":\"Invalid JSON or missing level\"}");
            return;
        }
        int8_t level = doc[FPSTR(CamConsts::field_level)];
        if (setBrightness(level)) {
            webserver.send(200, RoutesConsts::mime_json, "{\"message\":\"Brightness updated\"}");
        } else {
            webserver.send(400, RoutesConsts::mime_json, "{\"error\":\"Failed to set brightness\"}");
        } });
    
    // Register service status route
    registerServiceStatusRoute(CamConsts::tag, this);
    
    // Register settings routes
    registerSettingsRoutes(getServiceName().c_str(), this);
    
    return true;
}

bool K10CamService::saveSettings()
{
    // TODO: Implement settings persistence
    return true;
}

bool K10CamService::loadSettings()
{
    // TODO: Implement settings loading
    return true;
}