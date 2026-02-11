/**
 * @file K10CamService.h
 * @brief Header for K10 camera service integration with the main application
 * @details Provides camera control and streaming functionality via HTTP routes
 */
#pragma once
#include "../IsOpenAPIInterface.h"
#include <esp_camera.h>

class K10CamService : public IsOpenAPIInterface{
public:
    bool initializeService() override;
    bool startService() override;
    bool stopService() override;
    std::string getServiceName() override;
    std::string getServiceSubPath() override;
    bool registerRoutes() override;
    bool saveSettings() override;
    bool loadSettings() override;
    
    /**
     * @brief Set camera frame size (resolution)
     * @param framesize Frame size constant (e.g., FRAMESIZE_QVGA, FRAMESIZE_VGA)
     * @return true if successful, false otherwise
     */
    bool setFramesize(framesize_t framesize);
    
    /**
     * @brief Get current camera frame size
     * @return Current framesize_t value
     */
    framesize_t getFramesize();
    
    /**
     * @brief Set vertical flip
     * @param enable true to flip vertically, false to disable
     * @return true if successful, false otherwise
     */
    bool setVFlip(bool enable);
    
    /**
     * @brief Get current vertical flip state
     * @return true if enabled, false otherwise
     */
    bool getVFlip();
    
    /**
     * @brief Set horizontal mirror
     * @param enable true to mirror horizontally, false to disable
     * @return true if successful, false otherwise
     */
    bool setHMirror(bool enable);
    
    /**
     * @brief Get current horizontal mirror state
     * @return true if enabled, false otherwise
     */
    bool getHMirror();
    
    /**
     * @brief Set camera contrast level
     * @param level Contrast level (-2 to +2)
     * @return true if successful, false otherwise
     */
    bool setContrast(int8_t level);
    
    /**
     * @brief Get current camera contrast level
     * @return Contrast level (-2 to +2)
     */
    int8_t getContrast();
    
    /**
     * @brief Set camera brightness level
     * @param level Brightness level (-2 to +2)
     * @return true if successful, false otherwise
     */
    bool setBrightness(int8_t level);
    
    /**
     * @brief Get current camera brightness level
     * @return Brightness level (-2 to +2)
     */
    int8_t getBrightness();

private:
    volatile bool streaming_active_ = false;
    /**
     * @brief Configure camera for K10 board
     * @return Pointer to camera_config_t structure with K10 pin configuration
     */
    camera_config_t getCameraConfig();
    
    /**
     * @brief Handle snapshot HTTP request
     * @details Captures a frame and sends it as JPEG image
     */
    void handleSnapshot();
    
    /**
     * @brief Handle streaming HTTP request
     * @details Continuously streams MJPEG video using multipart/x-mixed-replace
     */
    void handleStream();
    
    /**
     * @brief Handle camera status HTTP request
     * @details Returns current camera settings and status
     */
    void handleStatus();
    
};