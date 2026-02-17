/**
 * @file WebcamService.h
 * @brief Header for webcam service integration with the main application
 * @details Provides webcam initialization, snapshot capture, and HTTP route registration
 */
#pragma once

#include "../IsServiceInterface.h"
#include "../IsOpenAPIInterface.h"
#include "esp_camera.h"

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
     */
    void handleSnapshot();
    /**
     * @brief Handle streaming HTTP request
     */
    void handleStream();
    /**
     * @brief Handle camera status HTTP request
     */
    void handleStatus();
    /**
     * @brief Handle camera settings update HTTP request
     */
    void handleSettings();

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
};
