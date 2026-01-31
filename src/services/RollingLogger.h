/**
 * @file RollingLogger.h
 * @brief Logger utility for the K10 UDP Receiver project.
 * Provides logging functionalities with different log levels and write logs to screen
 * logs to screen.
 *
 */
#pragma once

#include <Arduino.h>
#include <vector>
/**
 * @class Logger
 * @brief Simple logging utility with log levels and screen output.
 * @todo: Split the log handling and screen rendering into separate classes.
 */
class RollingLogger
{
public:
    enum LogLevel
    {
        TRACE = 4,
        DEBUG = 3,
        INFO = 2,
        WARNING = 1,
        ERROR = 0
    };

    RollingLogger();

    void log(const std::string message, const LogLevel level);
    void log(const __FlashStringHelper* message, const LogLevel level);
    void trace(const std::string message) { log(message, TRACE); };
    void trace(const __FlashStringHelper* message) { log(message, TRACE); };
    void debug(const std::string message) { log(message, DEBUG); };
    void debug(const __FlashStringHelper* message) { log(message, DEBUG); };
    void info(const std::string message) { log(message, INFO); };
    void info(const __FlashStringHelper* message) { log(message, INFO); };
    void warning(const std::string message) { log(message, WARNING); };
    void warning(const __FlashStringHelper* message) { log(message, WARNING); };
    void error(const std::string message) { log(message, ERROR); };
    void error(const __FlashStringHelper* message) { log(message, ERROR); };

    void set_log_level(const LogLevel level);
    LogLevel get_log_level();
    void set_max_rows(int rows);
    int get_max_rows();
    void set_logger_viewport(int x, int y, int width, int height);
    void set_logger_text_color(uint16_t color, uint16_t bg_color);

private:
    void renderLogs();

    LogLevel current_log_level = DEBUG;
    int max_rows;
    int vp_x, vp_y, viewport_width, viewport_height;
    uint16_t text_color;
    uint16_t background_color;
    std::vector<std::pair<LogLevel, std::string>> log_rows;
};

/**
 * @brief Utility function to convert FPSTR (Flash String Helper) to std::string
 * @param flashStr The flash string pointer from FPSTR()
 * @return std::string copy of the flash string
 */
inline std::string fpstr_to_string(const __FlashStringHelper* flashStr) {
    if (!flashStr) return std::string();
    const char* str = reinterpret_cast<const char*>(flashStr);
    return std::string(str);
}

inline std::string operator+(const std::string& lhs, const __FlashStringHelper* rhs) {
    return lhs + fpstr_to_string(rhs);
}

inline std::string operator+(const __FlashStringHelper* lhs, const std::string& rhs) {
    return fpstr_to_string(lhs) + rhs;
}