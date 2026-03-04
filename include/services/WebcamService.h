/**
 * @file WebcamService.h
 * @brief Header for webcam service integration with the main application
 * @details Provides webcam initialization, snapshot capture, and HTTP route registration
 */
#pragma once

#include "IsServiceInterface.h"
#include "IsOpenAPIInterface.h"
#include "esp_camera.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/**
 * @class WebcamService
 * @brief Service for managing ESP32 camera operations
 */
class WebcamService : public IsOpenAPIInterface
{
public:
    /**
     * @brief Initialize the camera hardware
     * @return true if successful, false otherwise
     */
    bool initializeService() override;

    /**
     * @brief Start the camera service
     * @return true if successful, false otherwise
     */
    bool startService() override;

    /**
     * @brief Stop the camera service
     * @return true if successful, false otherwise
     */
    bool stopService() override;

    /**
     * @brief Capture a snapshot from the camera
     * @return Camera frame buffer, or nullptr on failure
     */
    camera_fb_t* captureSnapshot();

    /**
     * @brief Register HTTP routes for webcam operations
     * @return true if registration was successful, false otherwise
     */
    bool registerRoutes() override;

    /**
     * @brief Get service name
     * @return Service name as string
     */
    std::string getServiceName() override;

    /**
     * @brief Get service subpath component
     * @return Service subpath
     */
    std::string getServiceSubPath() override;

    bool saveSettings() override;
    bool loadSettings() override;


private:
    bool initialized_;
    framesize_t current_framesize_ = FRAMESIZE_VGA;  // Track current frame size

    volatile bool streaming_active_ = false;

    /**
     * @brief Handle snapshot HTTP request
     * @param request Pointer to AsyncWebServerRequest
     */
    void handleSnapshot(AsyncWebServerRequest *request);
    /**
     * @brief Handle streaming HTTP request
     * @param request Pointer to AsyncWebServerRequest
     */
    void handleStream(AsyncWebServerRequest *request);
    /**
     * @brief Handle camera status HTTP request
     * @param request Pointer to AsyncWebServerRequest
     */
    void handleStatus(AsyncWebServerRequest *request);
    /**
     * @brief Handle camera settings update HTTP request
     * @param request Pointer to AsyncWebServerRequest
     */
    void handleSettings(AsyncWebServerRequest *request);

    /**
     * @brief Reinitialize camera with new framesize
     * @param framesize New framesize to apply (0-13)
     * @return true if reinitialization successful, false otherwise
     */
    bool reinitializeWithFramesize(framesize_t framesize);

    /**
     * @brief Configure camera pins for UNIHIKER K10 board
     */
    void configureCamera(camera_config_t &config);

    // ─── Audio streaming ─────────────────────────────────────────────

    /**
     * @brief Handle audio stream HTTP request (WAV 16kHz/16-bit/mono)
     * @param request Pointer to AsyncWebServerRequest
     */
    void handleAudioStream(AsyncWebServerRequest *request);

    /**
     * @brief Start audio capture task and allocate ring buffer
     * @return true if capture started successfully
     */
    bool startAudioCapture();

    /**
     * @brief Stop audio capture task and free ring buffer
     */
    void stopAudioCapture();

    /** @brief Static wrapper for FreeRTOS task creation */
    static void audioCaptureTaskStatic(void *param);

    /** @brief Audio capture loop — reads I2S and writes to ring buffer */
    void audioCaptureLoop();

    /** @brief Write data into the audio ring buffer (producer side) */
    size_t audioRingWrite(const uint8_t *data, size_t len);

    /** @brief Read data from the audio ring buffer (consumer side) */
    size_t audioRingRead(uint8_t *dest, size_t len);

    /** @brief Bytes available to read from ring buffer */
    size_t audioRingAvailable() const;

    static constexpr size_t AUDIO_RING_SIZE = 16384; ///< 16 KB ≈ 500 ms at 16 kHz mono 16-bit

    uint8_t *audio_ring_buf_ = nullptr;    ///< Heap-allocated ring buffer
    volatile size_t audio_ring_w_ = 0;     ///< Write position (audio task)
    volatile size_t audio_ring_r_ = 0;     ///< Read position (HTTP callback)
    TaskHandle_t audio_task_ = nullptr;    ///< Audio capture FreeRTOS task handle
    volatile bool audio_capturing_ = false;         ///< Task loop control flag
    volatile bool audio_streaming_active_ = false;  ///< One-client-at-a-time guard
};
