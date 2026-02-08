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
#include "FlashStringHelper.h"
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
    const std::vector<std::pair<LogLevel, std::string>>& get_log_rows() const;

private:
    LogLevel current_log_level = DEBUG;
    int max_rows;
    std::vector<std::pair<LogLevel, std::string>> log_rows;
};

