/**
 * @file utb2026.h
 * @brief UnleashTheBBricks 2026 UI component header
 * @details Provides display management, logger views, network info, and servo status display
 */
#include <Arduino.h>
#include <map>
#include <TFT_eSPI.h>
#include "../services/servo/ServoService.h"
#include "../services/RollingLogger.h"
#pragma once

/**
 * @class UTB2026
 * @brief UI manager for K10 bot display and information display
 * @details Handles rendering of logger views, network information, servo status, and other UI elements
 */
class UTB2026 {
public:
    /**
     * @brief Initialize the UI system
     * @details Sets up TFT display, clears screen, and prepares UI components
     */
    void init();

    /**
     * @brief Add a logger view to the UI
     * @details Registers a RollingLogger instance for display in a specific viewport
     * @param logger Pointer to RollingLogger instance to display
     * @param x1 Viewport left coordinate (pixels)
     * @param y1 Viewport top coordinate (pixels)
     * @param x2 Viewport right coordinate (pixels)
     * @param y2 Viewport bottom coordinate (pixels)
     * @param text_color Text color (default: TFT_BLACK)
     * @param bg_color Background color (default: TFT_BLACK)
     */
    void add_logger_view(RollingLogger* logger, int x1, int y1, int x2, int y2, uint16_t text_color = TFT_BLACK, uint16_t bg_color = TFT_BLACK);

    /**
     * @brief Set an information value for display
     * @param key Information key identifier
     * @param value Information value to display
     */
    void set_info(const std::string &key, const std::string &value);

    /**
     * @brief Increment a counter value
     * @param name Counter identifier
     * @param increment Amount to increment (default: 1)
     */
    void inc_counter(const std::string &name, long increment = 1);

    /**
     * @brief Update servo status display
     * @param number Servo channel number
     * @param value Current servo value
     * @param status Connection status of the servo
     */
    void update_servo(const char &number, int &value, ServoConnection &status);

    /**
     * @brief Get all information values
     * @return Map of all information key-value pairs
     */
    std::map<std::string, std::string> get_infos();

    /**
     * @brief Get a specific information value
     * @param key Information key identifier
     * @param default_value Default value if key not found (default: "?")
     * @return Information value or default_value
     */
    std::string get_info(std::string key, std::string default_value = "?");

    /**
     * @brief Get all counter values
     * @return Map of all counter key-value pairs
     */
    std::map<std::string, long> get_counters();

    /**
     * @brief Get a specific counter value
     * @param key Counter identifier
     * @return Counter value
     */
    long get_counter(std::string key);

    /**
     * @brief Render all UI elements
     * @details Draws all registered views including logger, network info, and servos
     */
    void draw_all();

    /**
     * @brief Render servo status display
     * @details Updates and displays servo channel values and connection status
     */
    void draw_servos();

    /**
     * @brief Render network information display
     * @details Shows WiFi status, IP address, UDP/HTTP server info
     */
    void draw_network_info();

    /**
     * @brief Render all registered logger views
     * @details Updates and displays all active logger views with current log entries
     */
    void draw_logger();

    const std::string KEY_UDP_STATE = "udp?";
    const std::string KEY_UDP_PORT = "udp#";
    const std::string KEY_UDP_IN = "udp->";
    const std::string KEY_UDP_OUT = "udp<-";
    const std::string KEY_UDP_DROP = "udp_drop";
    const std::string KEY_HTTP_STATE = "http?";
    const std::string KEY_HTTP_PORT = "http#";
    const std::string KEY_HTTP_REQ = "http<-";
    const std::string KEY_IP_ADDRESS = "IP";
    const std::string KEY_WIFI_STATE = "wifi?";
    const std::string KEY_WIFI_NAME = "SSID";

private:
    struct logger_view {
        RollingLogger* logger_instance;
        int vp_x;
        int vp_y;
        int vp_width;
        int vp_height;
        uint16_t text_color;
        uint16_t bg_color;
    };
    std::vector<logger_view> logger_views;
};