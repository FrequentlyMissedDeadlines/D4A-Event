/**
 * @file LogScreen.cpp
 * @brief Scrolling colour-coded log display for the TFT screen.
 */

#include "UI/LogScreen.h"
#include <TFT_eSPI.h>
#include <cstdio>
#include <cstring>

extern TFT_eSPI tft;

#define LH UILayout::LINE_H  // 8 px per line

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

LogScreen::LogScreen(RollingLogger &log, const char *title)
    : log_(log), title_(title ? title : "Log")
{
}

// ---------------------------------------------------------------------------
// IsScreen
// ---------------------------------------------------------------------------

void LogScreen::initScreen()
{
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.fillScreen(UIColors::CLR_BLACK);
}

bool LogScreen::needsUpdate() const
{
    return log_.get_version() != cached_version_;
}

void LogScreen::updateScreen()
{
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.resetViewport();
    tft.setTextDatum(TL_DATUM);

    // ── Title bar ─────────────────────────────────────────────────────────────
    tft.setTextColor(UIColors::CLR_BLACK, UIColors::CLR_TITLE);
    tft.setCursor(0, 0);
    char header[41];
    snprintf(header, sizeof(header), "%-40s", title_.c_str());
    tft.print(header);

    // ── Build wrapped + colour-coded line list ────────────────────────────────
    constexpr int MAX_CONTENT_LINES = UILayout::LINES_PER_SCREEN - 1; // 39
    constexpr int CHARS             = UILayout::CHARS_PER_LINE;        // 40

    struct ColoredLine
    {
        std::string text;
        uint16_t    color;
    };

    std::vector<ColoredLine> lines;
    lines.reserve(static_cast<size_t>(MAX_CONTENT_LINES) * 2);

    for (const auto &entry : log_.get_log_rows())
    {
        char prefix[12];
        snprintf(prefix, sizeof(prefix), "%c|%6lu|",
                 (entry.level == RollingLogger::ERROR)   ? 'E' :
                 (entry.level == RollingLogger::WARNING) ? 'W' :
                 (entry.level == RollingLogger::INFO)    ? 'I' :
                 (entry.level == RollingLogger::DEBUG)   ? 'D' : 'T',
                 entry.timestamp_ms % 100000UL);

        std::string full_line = std::string(prefix) + entry.message;
        uint16_t c = colorForLevel(entry.level);
        for (auto &w : wrapText(full_line, CHARS))
            lines.push_back({std::move(w), c});
    }

    // Show only the last MAX_CONTENT_LINES wrapped lines
    const int n     = static_cast<int>(lines.size());
    const int start = (n > MAX_CONTENT_LINES) ? (n - MAX_CONTENT_LINES) : 0;

    for (int i = 0; i < MAX_CONTENT_LINES; ++i)
    {
        const int y = (i + 1) * LH; // +1 to skip title bar
        tft.fillRect(0, y, UILayout::SCREEN_W, LH, UIColors::CLR_BLACK);

        const int idx = start + i;
        if (idx < n)
        {
            tft.setTextColor(lines[idx].color, UIColors::CLR_BLACK);
            tft.setCursor(0, y);
            tft.print(lines[idx].text.c_str());
        }
    }

    cached_version_ = log_.get_version();
}

// ---------------------------------------------------------------------------
// colorForLevel
// ---------------------------------------------------------------------------

uint16_t LogScreen::colorForLevel(RollingLogger::LogLevel level) const
{
    switch (level)
    {
    case RollingLogger::ERROR:   return UIColors::CLR_RED;
    case RollingLogger::WARNING: return UIColors::CLR_YELLOW;
    case RollingLogger::INFO:    return UIColors::CLR_WHITE;
    case RollingLogger::DEBUG:   return UIColors::CLR_LIGHTGREY;
    case RollingLogger::TRACE:   return UIColors::CLR_DARKGREY;
    default:                     return UIColors::CLR_WHITE;
    }
}

// ---------------------------------------------------------------------------
// wrapText
// ---------------------------------------------------------------------------

std::vector<std::string> LogScreen::wrapText(const std::string &text, int max_chars)
{
    std::vector<std::string> result;
    if (text.empty() || max_chars <= 0)
    {
        result.emplace_back("");
        return result;
    }
    size_t pos = 0;
    const size_t len = text.length();
    while (pos < len)
    {
        result.push_back(text.substr(pos, static_cast<size_t>(max_chars)));
        pos += static_cast<size_t>(max_chars);
    }
    return result;
}

