/**
 * @file WebcamService.h
 * @brief Header for webcam service integration with the main application
 * @details Provides webcam initialization, snapshot capture, and HTTP route registration
 */
#pragma once

#include "../services/IsServiceInterface.h"
#include "../services/IsOpenAPIInterface.h"
#include "esp_camera.h"

/**
 * @class WebcamService
 * @brief Service for managing ESP32 camera operations
 */
class WebcamService : public IsServiceInterface, public IsOpenAPIInterface
{
public:
    IsOpenAPIInterface* asOpenAPIInterface() override { return this; }

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
    enum ServiceStatus { INIT_FAILED, START_FAILED, STARTED, STOPPED, STOP_FAILED };
    ServiceStatus service_status_ = STOP_FAILED;
    unsigned long status_timestamp_ = 0;

    /**
     * @brief Handle snapshot HTTP request
     */
    void handleSnapshot();

    /**
     * @brief Handle camera status HTTP request
     */
    void handleStatus();

    /**
     * @brief Configure camera pins for UNIHIKER K10 board
     */
    void configureCamera(camera_config_t &config);
};
