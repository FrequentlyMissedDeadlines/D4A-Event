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
 * @class RollingLogger
 * @brief Simple logging utility with log levels and rolling buffer management.
 * @details Provides structured logging with multiple severity levels (TRACE, DEBUG, INFO, WARNING, ERROR).
 *          Log storage is separated from display rendering, allowing flexible UI integration.
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

    /**
     * @brief Constructor
     * @details Initializes the logger with default log level (DEBUG) and max rows (16)
     */
    RollingLogger();

    /**
     * @brief Log a message with a specific severity level
     * @param message The message to log
     * @param level The log level (TRACE, DEBUG, INFO, WARNING, ERROR)
     */
    void log(const std::string message, const LogLevel level);

    /**
     * @brief Log a PROGMEM message with a specific severity level
     * @param message The message from flash memory
     * @param level The log level (TRACE, DEBUG, INFO, WARNING, ERROR)
     */
    void log(const __FlashStringHelper* message, const LogLevel level);

    /**
     * @brief Log a trace-level message (most verbose)
     * @param message The trace message to log
     */
    void trace(const std::string message) { log(message, TRACE); };

    /**
     * @brief Log a trace-level message from PROGMEM
     * @param message The trace message from flash memory
     */
    void trace(const __FlashStringHelper* message) { log(message, TRACE); };

    /**
     * @brief Log a debug-level message
     * @param message The debug message to log
     */
    void debug(const std::string message) { log(message, DEBUG); };

    /**
     * @brief Log a debug-level message from PROGMEM
     * @param message The debug message from flash memory
     */
    void debug(const __FlashStringHelper* message) { log(message, DEBUG); };

    /**
     * @brief Log an info-level message
     * @param message The info message to log
     */
    void info(const std::string message) { log(message, INFO); };

    /**
     * @brief Log an info-level message from PROGMEM
     * @param message The info message from flash memory
     */
    void info(const __FlashStringHelper* message) { log(message, INFO); };

    /**
     * @brief Log a warning-level message
     * @param message The warning message to log
     */
    void warning(const std::string message) { log(message, WARNING); };

    /**
     * @brief Log a warning-level message from PROGMEM
     * @param message The warning message from flash memory
     */
    void warning(const __FlashStringHelper* message) { log(message, WARNING); };

    /**
     * @brief Log an error-level message
     * @param message The error message to log
     */
    void error(const std::string message) { log(message, ERROR); };

    /**
     * @brief Log an error-level message from PROGMEM
     * @param message The error message from flash memory
     */
    void error(const __FlashStringHelper* message) { log(message, ERROR); };

    /**
     * @brief Set the minimum log level for filtering messages
     * @param level The minimum log level to display (TRACE, DEBUG, INFO, WARNING, ERROR)
     */
    void set_log_level(const LogLevel level);

    /**
     * @brief Get the current log level filter
     * @return Current log level
     */
    LogLevel get_log_level();

    /**
     * @brief Set the maximum number of log rows to retain in memory
     * @param rows Maximum number of log entries (must be > 0)
     */
    void set_max_rows(int rows);

    /**
     * @brief Get the current maximum row limit
     * @return Maximum number of log entries
     */
    int get_max_rows();

    /**
     * @brief A single log entry holding the severity level, millisecond timestamp, and message.
     */
    struct LogEntry
    {
        LogLevel level;           ///< Severity level of the log entry
        unsigned long timestamp_ms; ///< Timestamp captured via millis() at log time
        std::string message;      ///< Log message text
    };

    /**
     * @brief Retrieve all log entries for display or processing
     * @return Const reference to vector of LogEntry structs
     */
    const std::vector<LogEntry>& get_log_rows() const;

    /**
     * @brief Get the log version counter
     * @details Incremented each time a new log entry is added. Useful for
     *          detecting changes without comparing full log contents.
     * @return Current version counter value
     */
    unsigned long get_version() const;

private:
    LogLevel current_log_level = DEBUG;
    int max_rows;
    std::vector<LogEntry> log_rows;
    unsigned long log_version_ = 0;
};

