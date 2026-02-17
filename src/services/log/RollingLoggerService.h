#pragma once
#include <WebServer.h>
#include "../IsOpenAPIInterface.h"
#include "../RollingLogger.h"
#include <vector>

/**
 * @file RollingLoggerService.h
 * @brief Header for rolling logger service
 * @details Provides HTTP access to rolling log entries via OpenAPI routes.
 */

namespace RollingLoggerConsts
{
    constexpr const char str_service_name[] PROGMEM = "Rolling logger";
    constexpr const char path_service[] PROGMEM = "logs/v1";
    constexpr const char path_all_logs[] PROGMEM = "all";
    constexpr const char path_log_debug_json[] PROGMEM = "debug.json";
    constexpr const char path_log_app_info_json[] PROGMEM = "app_info.json";
    constexpr const char path_log_esp_json[] PROGMEM = "esp.json";
    constexpr const char path_log_debug_txt[] PROGMEM = "debug.log";
    constexpr const char path_log_app_info_txt[] PROGMEM = "app_info.log";
    constexpr const char path_log_esp_txt[] PROGMEM = "esp.log";
    constexpr const char route_esp_desc[] PROGMEM = "Retrieves log entries from the ESP-IDF logger only";
}

class RollingLoggerService : public IsOpenAPIInterface
{
public:
    bool registerRoutes() override;
    std::string getServiceSubPath() override;
    std::string getServiceName() override;

    /**
     * @brief Set the logger instances to be exposed via API
     * @param debug_log Pointer to debug logger instance
     * @param app_info_log Pointer to app info logger instance
     * @param esp_log Pointer to ESP-IDF logger instance (optional)
     */
    void set_logger_instances(RollingLogger* debug_log, RollingLogger* app_info_log, RollingLogger* esp_log = nullptr);

private:

    
    // Pointers to logger instances
    static RollingLogger* debug_logger_ptr_;
    static RollingLogger* app_info_logger_ptr_;
    static RollingLogger* esp_logger_ptr_;
    
    /**
     * @brief Helper to convert log level to string
     * @param level Log level enum value
     * @return String representation of log level
     */
    static const char* log_level_to_string(RollingLogger::LogLevel level);
    
    /**
     * @brief Helper to serialize a logger's entries to JSON
     * @param logger Pointer to the logger instance
     * @return JSON string containing all log entries
     */
    static String serialize_logger_to_json(RollingLogger* logger);
    
    /**
     * @brief Helper to serialize a logger's entries to plain text
     * @param logger Pointer to the logger instance
     * @return Plain text string with format "LEVEL: message\n" per entry
     */
    static String serialize_logger_to_text(RollingLogger* logger);
};
