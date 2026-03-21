/**
 * @file ESPLogToRolling.h
 * @brief Redirect ESP-IDF log output (ESP_LOGx macros) to a RollingLogger instance.
 *
 * @details
 * Installs a custom `vprintf` handler via `esp_log_set_vprintf()`.  All output
 * from `ESP_LOGI`, `ESP_LOGE`, `ESP_LOGW`, `ESP_LOGD`, and `ESP_LOGV` is
 * intercepted, ANSI escape codes are stripped, the ESP-IDF level prefix
 * character is mapped to a `RollingLogger::LogLevel`, and the entry is stored
 * in the target `RollingLogger`.
 *
 * ## Usage
 * @code
 *   #include "ESPLogToRolling.h"
 *
 *   RollingLogger esp_logger;
 *   esp_logger.set_log_level(RollingLogger::DEBUG);
 *   esp_logger.set_max_rows(40);
 *
 *   // Call once, before any ESP_LOGx output you want to capture.
 *   esp_log_to_rolling_init(&esp_logger);
 * @endcode
 *
 * @note Only one logger can be active at a time. Calling this function again
 *       with a different pointer replaces the previous target.
 *
 * @note The function must not be called from an ISR context.
 */

#pragma once

class RollingLogger;

/**
 * @brief Initialize ESP-IDF log capture and redirect to a RollingLogger.
 *
 * Installs a custom vprintf handler that forwards all ESP-IDF log output to
 * @p logger.  Must be called from a single-threaded context (e.g. `setup()`).
 *
 * @param logger  Pointer to the target RollingLogger. Must remain valid for
 *                the lifetime of the application. Passing nullptr is a no-op.
 */
void esp_log_to_rolling_init(RollingLogger *logger);
