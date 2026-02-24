#include <cstdint>
#include <cstring>
#include <esp_log.h>
#include "RollingLogger.h"

/**
 * @file esp_to_rolling.cpp
 * @brief Capture ESP-IDF log output and forward to RollingLogger
 * @details Implements custom vprintf handler to intercept ESP_LOGI, ESP_LOGE, etc.
 */

namespace
{
    RollingLogger* esp_logger_instance = nullptr;
    constexpr size_t max_log_line = 256;
}

/**
 * @brief Map ESP log level to RollingLogger level
 * @param esp_level ESP-IDF log level
 * @return Corresponding RollingLogger::LogLevel
 */
static RollingLogger::LogLevel map_esp_level_to_rolling(esp_log_level_t esp_level)
{
    switch (esp_level)
    {
        case ESP_LOG_ERROR:   return RollingLogger::ERROR;
        case ESP_LOG_WARN:    return RollingLogger::WARNING;
        case ESP_LOG_INFO:    return RollingLogger::INFO;
        case ESP_LOG_DEBUG:   return RollingLogger::DEBUG;
        case ESP_LOG_VERBOSE: return RollingLogger::TRACE;
        default:              return RollingLogger::INFO;
    }
}

/**
 * @brief Strip ANSI escape sequences from log string
 * @param input String with potential ANSI codes
 * @return Clean string without ANSI codes
 */
static std::string strip_ansi_codes(const char* input)
{
    std::string output;
    size_t len = strlen(input);
    output.reserve(len);
    
    for (size_t i = 0; i < len; i++)
    {
        // Check for ESC character (27 or 0x1B)
        if (input[i] == 27 && i + 1 < len && input[i + 1] == '[')
        {
            // Skip ANSI escape sequence: ESC [ ... m
            i += 2; // Skip ESC and [
            while (i < len && input[i] != 'm')
                i++;
            // i now points to 'm', loop will increment past it
        }
        else
        {
            output += input[i];
        }
    }
    
    return output;
}

/**
 * @brief Custom vprintf handler for ESP-IDF logs
 * @param format Printf format string
 * @param args Variable argument list
 * @return Number of characters written
 */
static int esp_log_vprintf_handler(const char* format, va_list args)
{
    // Always log, even if instance is null (for debugging)
    if (!esp_logger_instance)
        return vprintf(format, args); // Fallback to default output
    
    // Format the log message
    char log_buffer[max_log_line];
    int ret = vsnprintf(log_buffer, sizeof(log_buffer), format, args);
    
    // Log everything we receive to verify handler is called
    if (ret > 0)
    {
        // Strip ANSI color codes first
        std::string clean_msg = strip_ansi_codes(log_buffer);
        
        // Remove trailing newline if present
        if (!clean_msg.empty() && clean_msg.back() == '\n')
            clean_msg.pop_back();
        
        // ESP-IDF format: "X (timestamp) TAG: message"
        RollingLogger::LogLevel log_level = RollingLogger::INFO;
        
        // Try to detect log level from first character
        if (!clean_msg.empty())
        {
            switch (clean_msg[0])
            {
                case 'E': log_level = RollingLogger::ERROR;   break;
                case 'W': log_level = RollingLogger::WARNING; break;
                case 'I': log_level = RollingLogger::INFO;    break;
                case 'D': log_level = RollingLogger::DEBUG;   break;
                case 'V': log_level = RollingLogger::TRACE;   break;
            }
        }
        
        // Log the cleaned string
        esp_logger_instance->log(clean_msg, log_level);
    }
    
    return ret;
}

/**
 * @brief Initialize ESP-IDF log capture and redirect to RollingLogger
 * @param logger Pointer to RollingLogger instance to receive ESP logs
 */
void esp_log_to_rolling_init(RollingLogger* logger)
{
    if (!logger)
        return;
    
    esp_logger_instance = logger;
    
    // Redirect ESP-IDF log output to our custom handler
    esp_log_set_vprintf(esp_log_vprintf_handler);
    
    // Add a direct test message to verify the logger is working
    logger->info("[ESP_LOG] ESP-IDF log redirection initialized");
    
}