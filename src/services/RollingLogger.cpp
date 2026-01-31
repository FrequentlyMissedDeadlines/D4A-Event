/**
 * Logger implementation
 */
#include "RollingLogger.h"
#include <vector>
#include <TFT_eSPI.h>
#include <Arduino.h>
#include <sstream>

constexpr int MAX_ROWS = 16;
constexpr int CHAR_HEIGHT = 10;
constexpr uint16_t COLOR_ERROR = TFT_RED;
#define COLOR_WARNING TFT_YELLOW
#define COLOR_INFO TFT_WHITE
#define COLOR_DEBUG TFT_LIGHTGREY

extern TFT_eSPI tft;

// Constructor
RollingLogger::RollingLogger()
    : current_log_level(INFO), max_rows(MAX_ROWS), vp_x(0), vp_y(0), viewport_width(320), viewport_height(MAX_ROWS * 10), text_color(TFT_BLACK), background_color(TFT_BLACK)
{
}

// Internal helper to render logs to the display
void RollingLogger::renderLogs()
{
    tft.setViewport(vp_x, vp_y, viewport_width, viewport_height);
    tft.fillRect(vp_x, vp_y, viewport_width, viewport_height, background_color);

    // Compose visible lines limited by max_rows
    int n_rows = log_rows.size();
    int startService = (n_rows > max_rows) ? (n_rows - max_rows) : 0;
    for (int i = 0; i < max_rows; ++i)
    {
        int u = startService + i;
        if (u >= 0 && u < n_rows)
        {
            std::pair<RollingLogger::LogLevel, std::string> &line = log_rows[u];
            if (text_color != background_color)
            {
                tft.setTextColor(text_color);
            }
            else
                switch (line.first)
                {
                case RollingLogger::DEBUG:
                    tft.setTextColor(COLOR_DEBUG);
                    break;
                case RollingLogger::INFO:
                    tft.setTextColor(COLOR_INFO);
                    break;
                case RollingLogger::WARNING:
                    tft.setTextColor(COLOR_WARNING);
                    break;
                case RollingLogger::ERROR:
                    tft.setTextColor(COLOR_ERROR);
                    break;
                }
#ifdef DEBUG
            const char *levelName = "?";
            switch (line.first)
            {
            case RollingLogger::DEBUG:
                levelName = "d";
                break;
            case RollingLogger::INFO:
                levelName = "i";
                break;
            case RollingLogger::WARNING:
                levelName = "w";
                break;
            case RollingLogger::ERROR:
                levelName = "e";
                break;
            }
#endif
            tft.setCursor(vp_x, vp_y + i * CHAR_HEIGHT);
#ifdef DEBUG
            tft.print(levelName);
            tft.print("|");
#endif
            tft.print(line.second.c_str());
        }
    }
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

    renderLogs(); // this is not good for performance but simple for now
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
    renderLogs();
}

int RollingLogger::get_max_rows()
{
    return max_rows;
}

void RollingLogger::set_logger_text_color(uint16_t color, uint16_t bg_color)
{
    text_color = color;
    background_color = bg_color;
}

void RollingLogger::set_logger_viewport(int x, int y, int width, int height)
{
    vp_x = x;
    vp_y = y;
    viewport_width = width;
    viewport_height = height;
}
