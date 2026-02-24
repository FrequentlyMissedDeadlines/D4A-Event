/**
 * Logger implementation
 */
#include "RollingLogger.h"
#include "FlashStringHelper.h"
#include <vector>
#include <TFT_eSPI.h>
#include <Arduino.h>
#include <sstream>

constexpr int MAX_ROWS = 16;


// Constructor
RollingLogger::RollingLogger()
    : current_log_level(INFO), max_rows(MAX_ROWS)
{
}

void RollingLogger::log(std::string message, const LogLevel level)
{
    if (level > current_log_level)
        return;
    log_rows.push_back(std::make_pair(level, message));

    // Prevent unbounded memory growth - trim to max_rows
    if ((int)log_rows.size() > max_rows)
    {
        int excess = log_rows.size() - max_rows;
        log_rows.erase(log_rows.begin(), log_rows.begin() + excess);
    }
}

void RollingLogger::log(const __FlashStringHelper* message, const LogLevel level)
{
    log(fpstr_to_string(message), level);
}

void RollingLogger::set_log_level(const LogLevel level)
{
    current_log_level = level;
}

RollingLogger::LogLevel RollingLogger::get_log_level()
{
    return current_log_level;
}

void RollingLogger::set_max_rows(int rows)
{
    if (rows <= 0)
        return;
    max_rows = rows;
    // Trim if needed
    if ((int)log_rows.size() > max_rows)
    {
        int drop = log_rows.size() - max_rows;
        log_rows.erase(log_rows.begin(), log_rows.begin() + drop);
    }
}

int RollingLogger::get_max_rows()
{
    return max_rows;
}

const std::vector<std::pair<RollingLogger::LogLevel, std::string>>& RollingLogger::get_log_rows() const
{
    return log_rows;
}
