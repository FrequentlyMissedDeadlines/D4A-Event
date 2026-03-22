/**
 * @file RollingLogger.h
 * @brief Rolling in-memory logger with severity filtering.
 *
 * @details
 * Stores log entries in a fixed-size rolling vector (oldest entries evicted
 * when the buffer is full).  Each entry carries the severity level, a
 * millisecond timestamp, and the message text.
 *
 * All public methods are thread-safe (protected by an internal mutex).
 *
 * Log levels (most → least verbose):
 *   TRACE > DEBUG > INFO > WARNING > ERROR
 *
 * Only entries whose level is ≤ the current `log_level` filter are stored.
 *
 * ## Usage
 * @code
 *   RollingLogger logger;
 *   logger.set_log_level(RollingLogger::DEBUG);
 *   logger.set_max_rows(40);
 *
 *   logger.info("Service started");
 *   logger.error(FPSTR(MyConsts::msg_failed));
 *
 *   for (const auto &entry : logger.get_log_rows())
 *       Serial.println(entry.message.c_str());  // entry.source holds the 5-letter tag
 * @endcode
 */

#pragma once

#include <Arduino.h>
#include <mutex>
#include <vector>
#include <string>
#include "FlashStringHelper.h"

class RollingLogger
{
public:
    // ---- Log level ----

    enum LogLevel
    {
        ERROR   = 0,
        WARNING = 1,
        INFO    = 2,
        DEBUG   = 3,
        TRACE   = 4,
    };

    // ---- Log entry ----

    /**
     * @brief A single log entry.
     */
    struct LogEntry
    {
        LogLevel      level;        ///< Severity level
        unsigned long timestamp_ms; ///< millis() at log time
        std::string   message;      ///< Message text
        char          source[6];    ///< 5-letter source tag (null-terminated), e.g. "MOTOR" or "WiFi "
    };

    // ---- Construction ----

    /**
     * @brief Construct a RollingLogger.
     * Default level: INFO, default max rows: 16.
     */
    RollingLogger();

    // ---- Configuration ----

    /**
     * @brief Set the minimum severity to store.
     * Messages with a level numerically greater than this are dropped.
     * @param level New filter level.
     */
    void set_log_level(LogLevel level);

    /** @brief Return the current filter level. */
    LogLevel get_log_level();

    /**
     * @brief Set the rolling buffer size.
     * If the current buffer exceeds the new size, the oldest entries are dropped.
     * @param rows Maximum number of entries to retain (must be > 0).
     */
    void set_max_rows(int rows);

    /** @brief Return the current maximum buffer size. */
    int get_max_rows();

    // ---- Logging ----

    /**
     * @brief Log a message at the given level.
     * @param source Optional 5-letter source tag (e.g. "MOTOR", "WiFi "). Truncated to 5 chars.
     */
    void log(const std::string &message, LogLevel level, const char *source = "     ");

    /** @brief Log a PROGMEM message at the given level. */
    void log(const __FlashStringHelper *message, LogLevel level, const char *source = "     ");

    // void trace(const std::string &message)             { log(message, TRACE);   }
    // void trace(const __FlashStringHelper *message)     { log(message, TRACE);   }
    // void debug(const std::string &message)             { log(message, DEBUG);   }
    // void debug(const __FlashStringHelper *message)     { log(message, DEBUG);   }
    // void info(const std::string &message)              { log(message, INFO);    }
    // void info(const __FlashStringHelper *message)      { log(message, INFO);    }
    // void warning(const std::string &message)           { log(message, WARNING); }
    // void warning(const __FlashStringHelper *message)   { log(message, WARNING); }
    // void error(const std::string &message)             { log(message, ERROR);   }
    // void error(const __FlashStringHelper *message)     { log(message, ERROR);   }
    
    void trace(const std::string &message, const char *source = "     ")             { log(message, TRACE, source);   }
    void trace(const __FlashStringHelper *message, const char *source = "     ")     { log(message, TRACE, source);   }
    void debug(const std::string &message, const char *source = "     ")             { log(message, DEBUG, source);   }
    void debug(const __FlashStringHelper *message, const char *source = "     ")     { log(message, DEBUG, source);   }
    void info(const std::string &message, const char *source = "     ")              { log(message, INFO, source);    }
    void info(const __FlashStringHelper *message, const char *source = "     ")      { log(message, INFO, source);    }
    void warning(const std::string &message, const char *source = "     ")           { log(message, WARNING, source); }
    void warning(const __FlashStringHelper *message, const char *source = "     ")   { log(message, WARNING, source); }
    void error(const std::string &message, const char *source = "     ")             { log(message, ERROR, source);   }
    void error(const __FlashStringHelper *message, const char *source = "     ")     { log(message, ERROR, source);   }
    // ---- Access ----

    /**
     * @brief Return a snapshot copy of all retained log entries.
     * @return Vector of LogEntry, oldest first.
     */
    std::vector<LogEntry> get_log_rows() const;

    /**
     * @brief Return the version counter.
     *
     * Incremented on every accepted log call.  Callers can use this to
     * detect new entries without comparing the full buffer.
     *
     * @return Monotonically increasing counter.
     */
    unsigned long get_version() const;

private:
    LogLevel              current_log_level = INFO;
    int                   max_rows          = 16;
    std::vector<LogEntry> log_rows;
    unsigned long         log_version_      = 0;
    mutable std::mutex    mutex_;
};
