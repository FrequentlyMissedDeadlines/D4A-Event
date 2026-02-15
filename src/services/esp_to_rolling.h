/**
 * @file esp_to_rolling.h
 * @brief Header for ESP-IDF log capture system
 */

#pragma once

class RollingLogger;

/**
 * @brief Initialize ESP-IDF log capture and redirect to RollingLogger
 * @param logger Pointer to RollingLogger instance to receive ESP logs
 * @details Redirects all ESP_LOGI, ESP_LOGE, ESP_LOGW, ESP_LOGD, ESP_LOGV 
 *          output to the specified RollingLogger instance
 */
void esp_log_to_rolling_init(RollingLogger* logger);
