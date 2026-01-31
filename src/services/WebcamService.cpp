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
#include "IsOpenAPIInterface.h"

// External webserver instance
extern WebServer webserver;

constexpr char kPathWebcam[] = "webcam/";
constexpr char kActionSnapshot[] = "snapshot";
constexpr char kActionStatus[] = "status";

// WebcamService constants namespace
namespace WebcamConsts
{
    constexpr const char msg_not_initialized[] PROGMEM = "Camera not initialized.";
    constexpr const char msg_capture_error[] PROGMEM = "Failed to capture image.";
    constexpr const char msg_camera_capture_failed[] PROGMEM = "Camera capture failed";
    constexpr const char str_service_name[] PROGMEM = "Webcam Service";
    constexpr const char path_service[] PROGMEM = "webcam/v1";
    constexpr const char field_initialized[] PROGMEM = "initialized";
    constexpr const char field_ready[] PROGMEM = "ready";
    constexpr const char field_settings[] PROGMEM = "settings";
    constexpr const char field_framesize[] PROGMEM = "framesize";
    constexpr const char field_quality[] PROGMEM = "quality";
    constexpr const char field_brightness[] PROGMEM = "brightness";
    constexpr const char field_contrast[] PROGMEM = "contrast";
    constexpr const char field_saturation[] PROGMEM = "saturation";
    constexpr const char field_framesize_name[] PROGMEM = "framesize_name";
    constexpr const char str_inline_filename[] PROGMEM = "inline; filename=snapshot.jpg";
    constexpr const char str_access_control[] PROGMEM = "Access-Control-Allow-Origin";
    constexpr const char str_content_disposition[] PROGMEM = "Content-Disposition";
    constexpr const char mime_image_jpeg[] PROGMEM = "image/jpeg";
    constexpr const char tag_webcam[] PROGMEM = "Webcam";
    constexpr const char desc_snapshot[] PROGMEM = "Capture and return a JPEG snapshot from the camera in real-time. Image format is SVGA (800x600) by default with quality setting of 12.";
    constexpr const char desc_status[] PROGMEM = "Get camera initialization status, current settings including frame size, quality, brightness, contrast, and saturation levels";
    constexpr const char resp_snapshot_ok[] PROGMEM = "JPEG image captured successfully";
    constexpr const char resp_camera_not_init[] PROGMEM = "Camera not initialized";
    constexpr const char resp_status_ok[] PROGMEM = "Camera status retrieved successfully";
}

// Camera pin definitions for UNIHIKER K10
// These are typical ESP32-CAM pin configurations
// Adjust based on actual K10 hardware schematic
#define CAM_PIN_PWDN    -1  // Power down pin (not used)
#define CAM_PIN_RESET   -1  // Reset pin (not used)
#define CAM_PIN_XCLK    10  // External clock
#define CAM_PIN_SIOD    40  // I2C SDA
#define CAM_PIN_SIOC    39  // I2C SCL

#define CAM_PIN_D7      48  // Data pins
#define CAM_PIN_D6      11
#define CAM_PIN_D5      12
#define CAM_PIN_D4      14
#define CAM_PIN_D3      16
#define CAM_PIN_D2      18
#define CAM_PIN_D1      17
#define CAM_PIN_D0      15

#define CAM_PIN_VSYNC   38  // Vertical sync
#define CAM_PIN_HREF    47  // Horizontal reference
#define CAM_PIN_PCLK    13  // Pixel clock

bool WebcamService::initializeService()
{
    initialized_ = false;

    camera_config_t config;
    configureCameraPins(config);

    // Camera parameters
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.grab_mode = CAMERA_GRAB_LATEST;

    // Frame size and quality settings
    // FRAMESIZE_SVGA (800x600) is a good balance of quality and size
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12; // 0-63, lower is higher quality
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_PSRAM;

    // Allocate enough buffers for stable operation
    if (psramFound()) {
        config.fb_count = 2;
        config.grab_mode = CAMERA_GRAB_LATEST;
    }

    // Initialize camera
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
#ifdef DEBUG
        Serial.printf("Camera init failed with error 0x%x\n", err);
#endif
        logger->error(getServiceName() + " " + std::string(ServiceInterfaceConsts::msg_initialize_failed));
        return false;
    }

    // Get camera sensor
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        // Initial sensor configuration for best performance
        s->set_brightness(s, 0);     // -2 to 2
        s->set_contrast(s, 0);       // -2 to 2
        s->set_saturation(s, 0);     // -2 to 2
        s->set_special_effect(s, 0); // 0 to 6 (0 - No Effect)
        s->set_whitebal(s, 1);       // 0 = disable , 1 = enable
        s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
        s->set_wb_mode(s, 0);        // 0 to 4 - if awb_gain enabled
        s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable
        s->set_aec2(s, 0);           // 0 = disable , 1 = enable
        s->set_ae_level(s, 0);       // -2 to 2
        s->set_aec_value(s, 300);    // 0 to 1200
        s->set_gain_ctrl(s, 1);      // 0 = disable , 1 = enable
        s->set_agc_gain(s, 0);       // 0 to 30
        s->set_gainceiling(s, (gainceiling_t)0); // 0 to 6
        s->set_bpc(s, 0);            // 0 = disable , 1 = enable
        s->set_wpc(s, 1);            // 0 = disable , 1 = enable
        s->set_raw_gma(s, 1);        // 0 = disable , 1 = enable
        s->set_lenc(s, 1);           // 0 = disable , 1 = enable
        s->set_hmirror(s, 0);        // 0 = disable , 1 = enable
        s->set_vflip(s, 0);          // 0 = disable , 1 = enable
        s->set_dcw(s, 1);            // 0 = disable , 1 = enable
        s->set_colorbar(s, 0);       // 0 = disable , 1 = enable
    }

    initialized_ = true;
#ifdef DEBUG

    logger->debug(getName() + " initialize done");
    #endif

    return true;
}

bool WebcamService::startService()
{
    if (initialized_) {
    #ifdef DEBUG
        logger->debug(getName() + " " + FPSTR(ServiceInterfaceConsts::msg_start_done));
    #endif
        service_status_ = STARTED;
        status_timestamp_ = millis();
    } else {
        logger->error(getServiceName() + " start failed");
        service_status_ = START_FAILED;
        status_timestamp_ = millis();
    }
    return initialized_;
}

bool WebcamService::stopService()
{
    if (initialized_) {
        esp_camera_deinit();
        initialized_ = false;
        service_status_ = STOPPED;
        status_timestamp_ = millis();
        #ifdef DEBUG
        logger->debug(getName() + ServiceInterfaceConsts::msg_stop_done);
        #endif
        return true;
    }
    logger->error(getServiceName() + " " + std::string(ServiceInterfaceConsts::msg_stop_failed));
    return false;
}

void WebcamService::configureCameraPins(camera_config_t &config)
{
    config.pin_pwdn = CAM_PIN_PWDN;
    config.pin_reset = CAM_PIN_RESET;
    config.pin_xclk = CAM_PIN_XCLK;
    config.pin_sscb_sda = CAM_PIN_SIOD;
    config.pin_sscb_scl = CAM_PIN_SIOC;

    config.pin_d7 = CAM_PIN_D7;
    config.pin_d6 = CAM_PIN_D6;
    config.pin_d5 = CAM_PIN_D5;
    config.pin_d4 = CAM_PIN_D4;
    config.pin_d3 = CAM_PIN_D3;
    config.pin_d2 = CAM_PIN_D2;
    config.pin_d1 = CAM_PIN_D1;
    config.pin_d0 = CAM_PIN_D0;

    config.pin_vsync = CAM_PIN_VSYNC;
    config.pin_href = CAM_PIN_HREF;
    config.pin_pclk = CAM_PIN_PCLK;
}

camera_fb_t* WebcamService::captureSnapshot()
{
    if (!initialized_) {
        return nullptr;
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
#ifdef DEBUG
        Serial.println(FPSTR(WebcamConsts::msg_camera_capture_failed));
#endif
        return nullptr;
    }

    return fb;
}

void WebcamService::handleSnapshot()
{
    // Note: This must be handled via instance, not static
    // Get the global webcam service instance (defined in main.cpp)
    extern WebcamService webcamService;

    if (!webcamService.initialized_) {
        webserver.send(503, RoutesConsts::mime_plain_text, FPSTR(WebcamConsts::msg_not_initialized));
        return;
    }

    camera_fb_t *fb = webcamService.captureSnapshot();
    if (!fb) {
        webserver.send(503, RoutesConsts::mime_plain_text, FPSTR(WebcamConsts::msg_capture_error));
        return;
    }

    // Send JPEG image
    webserver.sendHeader(FPSTR(WebcamConsts::str_content_disposition), FPSTR(WebcamConsts::str_inline_filename));
    webserver.sendHeader(FPSTR(WebcamConsts::str_access_control), "*");
    webserver.setContentLength(fb->len);
    webserver.send(200, FPSTR(WebcamConsts::mime_image_jpeg), "");
    webserver.sendContent(reinterpret_cast<const char*>(fb->buf), fb->len);

    // Return frame buffer
    esp_camera_fb_return(fb);
}

void WebcamService::handleStatus()
{
    extern WebcamService webcamService;

    JsonDocument doc;
    
    // Add standardized status fields
    doc[PSTR("servicename")] = "WebcamService";
    
    const char* status_str = "unknown";
    switch (webcamService.service_status_) {
        case WebcamService::INIT_FAILED: status_str = "init failed"; break;
        case WebcamService::START_FAILED: status_str = "start failed"; break;
        case WebcamService::STARTED: status_str = "started"; break;
        case WebcamService::STOPPED: status_str = "stopped"; break;
        case WebcamService::STOP_FAILED: status_str = "stop failed"; break;
    }
    doc[PSTR("status")] = status_str;
    doc[PSTR("ts")] = (unsigned long)webcamService.status_timestamp_;
    
    // Add additional camera details
    doc[FPSTR(WebcamConsts::field_initialized)] = webcamService.initialized_;
    
    if (webcamService.initialized_) {
        sensor_t *s = esp_camera_sensor_get();
        if (s) {
            doc[RoutesConsts::field_status] = FPSTR(WebcamConsts::field_ready);
            
            // Get current camera settings
            JsonObject settings = doc[FPSTR(WebcamConsts::field_settings)].to<JsonObject>();
            settings[FPSTR(WebcamConsts::field_framesize)] = s->status.framesize;
            settings[FPSTR(WebcamConsts::field_quality)] = s->status.quality;
            settings[FPSTR(WebcamConsts::field_brightness)] = s->status.brightness;
            settings[FPSTR(WebcamConsts::field_contrast)] = s->status.contrast;
            settings[FPSTR(WebcamConsts::field_saturation)] = s->status.saturation;
            
            // Get frame size info
            const char* frameSizes[] = {
                "96x96", "QQVGA", "QCIF", "HQVGA", "240x240",
                "QVGA", "CIF", "HVGA", "VGA", "SVGA",
                "XGA", "HD", "SXGA", "UXGA"
            };
            int fsIndex = s->status.framesize;
            if (fsIndex >= 0 && fsIndex < 14) {
                settings[FPSTR(WebcamConsts::field_framesize_name)] = frameSizes[fsIndex];
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
    std::string snapshotPath = getPath(kActionSnapshot);
#ifdef DEBUG
    logger->debug("+" + snapshotPath);
#endif
    
    std::vector<OpenAPIResponse> snapshotResponses;
    OpenAPIResponse snapshotOk(200, WebcamConsts::resp_snapshot_ok);
    snapshotOk.contentType = WebcamConsts::mime_image_jpeg;
    snapshotOk.schema = "{\"type\":\"string\",\"format\":\"binary\",\"description\":\"JPEG image data\"}";
    snapshotResponses.push_back(snapshotOk);
    snapshotResponses.push_back(OpenAPIResponse(503, WebcamConsts::resp_camera_not_init));
    
    registerOpenAPIRoute(OpenAPIRoute(snapshotPath.c_str(), RoutesConsts::method_get, 
                                      WebcamConsts::desc_snapshot,
                                      WebcamConsts::tag_webcam, false, {}, snapshotResponses));
    webserver.on(snapshotPath.c_str(), HTTP_GET, handleSnapshot);

    // Status endpoint - returns JSON with camera info
    std::string statusPath = getPath(kActionStatus);
#ifdef DEBUG
    logger->debug("+" + statusPath);
#endif
    
    std::vector<OpenAPIResponse> statusResponses;
    OpenAPIResponse statusOk(200, WebcamConsts::resp_status_ok);
    statusOk.schema = "{\"type\":\"object\",\"properties\":{\"initialized\":{\"type\":\"boolean\",\"description\":\"Whether camera is initialized\"},\"status\":{\"type\":\"string\",\"enum\":[\"ready\",\"sensor_error\",\"not_initialized\"]},\"settings\":{\"type\":\"object\",\"properties\":{\"framesize\":{\"type\":\"integer\",\"description\":\"Frame size code (0-13)\"},\"framesize_name\":{\"type\":\"string\",\"description\":\"Frame size name (e.g., SVGA, VGA)\"},\"quality\":{\"type\":\"integer\",\"description\":\"JPEG quality (0-63, lower is better)\"},\"brightness\":{\"type\":\"integer\",\"description\":\"Brightness level (-2 to 2)\"},\"contrast\":{\"type\":\"integer\",\"description\":\"Contrast level (-2 to 2)\"},\"saturation\":{\"type\":\"integer\",\"description\":\"Saturation level (-2 to 2)\"}}}}}";
    statusOk.example = "{\"initialized\":true,\"status\":\"ready\",\"settings\":{\"framesize\":9,\"framesize_name\":\"SVGA\",\"quality\":12,\"brightness\":0,\"contrast\":0,\"saturation\":0}}";
    statusResponses.push_back(statusOk);
    
    registerOpenAPIRoute(OpenAPIRoute(statusPath.c_str(), RoutesConsts::method_get, 
                                      WebcamConsts::desc_status,
                                      WebcamConsts::tag_webcam, false, {}, statusResponses));
    webserver.on(statusPath.c_str(), HTTP_GET, handleStatus);

    registerSettingsRoutes("Webcam", this);

    return true;
}

std::string WebcamService::getServiceName()
{
    return std::string(WebcamConsts::str_service_name);
}
std::string WebcamService::getServiceSubPath()
{
    return std::string(WebcamConsts::path_service);
}
std::string WebcamService::getPath(const std::string& finalpathstring)
{
    if (baseServicePath_.empty()) {
        // Cache base path on first call
        std::string serviceName = getServiceSubPath();
        size_t slashPos = serviceName.find('/');
        if (slashPos != std::string::npos) {
            serviceName = serviceName.substr(0, slashPos);
        }
        baseServicePath_ = std::string(RoutesConsts::path_api) + serviceName + "/";
    }
    return baseServicePath_ + finalpathstring;
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
