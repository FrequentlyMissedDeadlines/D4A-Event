/**
 * @file RollingLogger.cpp
 * @brief Implementation of RollingLogger.
 */

#include "RollingLogger.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

RollingLogger::RollingLogger()
    : current_log_level(INFO), max_rows(16)
{
}

// ---------------------------------------------------------------------------
// log
// ---------------------------------------------------------------------------

void RollingLogger::log(const std::string &message, LogLevel level)
{
    // Drop messages below the current filter threshold.
    // Levels are ordered ERROR(0) < WARNING(1) < INFO(2) < DEBUG(3) < TRACE(4).
    // A message is accepted when its level ≤ current_log_level.
    if (level > current_log_level)
        return;

    std::lock_guard<std::mutex> lock(mutex_);

    log_rows.push_back({level, millis(), message});
    ++log_version_;

    // Evict oldest entries if the buffer is over capacity
    if (static_cast<int>(log_rows.size()) > max_rows)
    {
        const int excess = static_cast<int>(log_rows.size()) - max_rows;
        log_rows.erase(log_rows.begin(), log_rows.begin() + excess);
    }
}

void RollingLogger::log(const __FlashStringHelper *message, LogLevel level)
{
    log(fpstr_to_string(message), level);
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void RollingLogger::set_log_level(LogLevel level)
{
    std::lock_guard<std::mutex> lock(mutex_);
    current_log_level = level;
}

RollingLogger::LogLevel RollingLogger::get_log_level()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return current_log_level;
}

void RollingLogger::set_max_rows(int rows)
{
    if (rows <= 0)
        return;

    std::lock_guard<std::mutex> lock(mutex_);
    max_rows = rows;

    if (static_cast<int>(log_rows.size()) > max_rows)
    {
        const int excess = static_cast<int>(log_rows.size()) - max_rows;
        log_rows.erase(log_rows.begin(), log_rows.begin() + excess);
    }
}

int RollingLogger::get_max_rows()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return max_rows;
}

// ---------------------------------------------------------------------------
// Access
// ---------------------------------------------------------------------------

std::vector<RollingLogger::LogEntry> RollingLogger::get_log_rows() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return log_rows;   // returns a copy — safe for callers outside the lock
}

unsigned long RollingLogger::get_version() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return log_version_;
}
