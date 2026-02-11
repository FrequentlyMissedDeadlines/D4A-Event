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
    constexpr const char path_debug_log[] PROGMEM = "debug";
    constexpr const char path_app_info_log[] PROGMEM = "app_info";
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
     */
    void set_logger_instances(RollingLogger* debug_log, RollingLogger* app_info_log);

private:

    
    // Pointers to logger instances
    static RollingLogger* debug_logger_ptr_;
    static RollingLogger* app_info_logger_ptr_;
    
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
};
