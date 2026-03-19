/**
 * @file LogScreen.h
 * @brief TFT screen that scrolls a RollingLogger's entries across the full display.
 *
 * Renders a colour-coded, word-wrapped log view.  Only redraws when the
 * logger's version counter has advanced since the last call to updateScreen()
 * (needsUpdate() returns false when nothing has changed).
 */
#pragma once

#include <climits>
#include <string>
#include <vector>
#include <cstdint>
#include "UI/IsScreen.h"
#include "UI/UIConsts.h"
#include "RollingLogger.h"

/**
 * @class LogScreen
 * @brief Renders a single RollingLogger to the full 240×320 TFT display.
 */
class LogScreen : public IsScreen
{
public:
    /**
     * @brief Construct with a logger reference and an optional title.
     * @param log    Logger whose entries are displayed.
     * @param title  Short title shown in the header bar (\u226440 chars).
     */
    LogScreen(RollingLogger &log, const char *title = "Log");

    /** @brief Clear the screen to black. */
    void initScreen() override;

    /** @brief Redraw all log entries and update the cached version. */
    void updateScreen() override;

    /**
     * @return true if the logger has new entries since the last updateScreen().
     */
    bool needsUpdate() const override;

private:
    RollingLogger  &log_;
    std::string     title_;            ///< Owned copy of the title string
    unsigned long   cached_version_ = ULONG_MAX; ///< Version at last draw

    /** @brief Map a log level to a 16-bit TFT colour. */
    uint16_t colorForLevel(RollingLogger::LogLevel level) const;

    /**
     * @brief Wrap a string into lines of at most max_chars characters.
     * @param text      Source text.
     * @param max_chars Maximum characters per output line.
     * @return Vector of line strings, each \u2264 max_chars chars.
     */
    static std::vector<std::string> wrapText(const std::string &text, int max_chars);
};