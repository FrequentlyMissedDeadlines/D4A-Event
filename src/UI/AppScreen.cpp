/**
 * @file AppScreen.cpp
 * @brief AmakerBot application dashboard: info panel, servo table, motor table.
 */

#include "UI/AppScreen.h"
#include <TFT_eSPI.h>
#include <cstdio>

extern TFT_eSPI tft;

// Layout shorthand macros (local to this translation unit)
#define LH  UILayout::LINE_H        // 8 px per line
#define RPX UILayout::RIGHT_PANEL_PX // 126 px (char 21)

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

AppScreen::AppScreen(WifiService        &wifi,
                     AmakerBotService   &amakerbot,
                     BotServerUDP       &udp,
                     BotServerWebSocket &ws,
                     MotorServoService  &motor_servo)
    : wifi_(wifi), amakerbot_(amakerbot), udp_(udp), ws_(ws), motor_servo_(motor_servo)
{
}

// ---------------------------------------------------------------------------
// IsScreen
// ---------------------------------------------------------------------------

void AppScreen::initScreen()
{
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.fillScreen(UIColors::CLR_BLACK);
}

void AppScreen::updateScreen()
{
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.resetViewport();
    tft.setTextDatum(TL_DATUM);

    drawInfoPanel();
    drawServoTable();
    drawMotorTable();
}

// ---------------------------------------------------------------------------
// colorsForStatus
// ---------------------------------------------------------------------------

void AppScreen::colorsForStatus(ServiceStatus status, uint16_t &fg, uint16_t &bg)
{
    switch (status)
    {
    case STARTED:
        fg = UIColors::CLR_STATUS_STARTED_FG;
        bg = UIColors::CLR_STATUS_STARTED_BG;
        break;
    case STOPPED:
        fg = UIColors::CLR_STATUS_STOPPED_FG;
        bg = UIColors::CLR_STATUS_STOPPED_BG;
        break;
    case INITIALIZED:
        fg = UIColors::CLR_STATUS_INIT_FG;
        bg = UIColors::CLR_STATUS_INIT_BG;
        break;
    default:
        fg = UIColors::CLR_STATUS_ERR_FG;
        bg = UIColors::CLR_STATUS_ERR_BG;
        break;
    }
}

// ---------------------------------------------------------------------------
// servoTypeLabel
// ---------------------------------------------------------------------------

const char *AppScreen::servoTypeLabel(ServoType t)
{
    switch (t)
    {
    case ServoType::SERVO_180:  return "180";
    case ServoType::SERVO_270:  return "270";
    case ServoType::CONTINUOUS: return "rot";
    default:                    return "???";
    }
}

// ---------------------------------------------------------------------------
// drawInfoPanel — left panel (chars 0–20, full-width ASCII-art table)
// ---------------------------------------------------------------------------

void AppScreen::drawInfoPanel()
{
    uint16_t fg, bg;
    colorsForStatus(amakerbot_.getStatus(), fg, bg);

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(fg, bg);

    int line = 0;
    constexpr int COL = 0;
    char buf[42]; // 41 chars + NUL

    // ── Header ──────────────────────────────────────────────────────────────
    tft.setCursor(COL, line * LH); ++line;
    tft.print("+--------------------------------------+");

    const std::string bot_name = amakerbot_.getBotName();
    const std::string token    = amakerbot_.getServerToken();
    snprintf(buf, sizeof(buf), "| %-22s [%-5s]  |",
             bot_name.substr(0, 22).c_str(), token.substr(0, 5).c_str());
    tft.setCursor(COL, line * LH); ++line;
    tft.print(buf);

    tft.setCursor(COL, line * LH); ++line;
    tft.print("+----------------+---------------------+");

    // ── Network rows ────────────────────────────────────────────────────────
    tft.setTextColor(UIColors::CLR_STATUS_DEFAULT_FG, UIColors::CLR_STATUS_DEFAULT_BG);

    const std::string ssid = wifi_.getSSID();
    snprintf(buf, sizeof(buf), "|%-16s|%-21s|", "SSID", ssid.substr(0, 21).c_str());
    tft.setCursor(COL, line * LH); ++line;
    tft.print(buf);

    const std::string ip = wifi_.getIP();
    snprintf(buf, sizeof(buf), "|%-16s|%-21s|", "IP", ip.substr(0, 21).c_str());
    tft.setCursor(COL, line * LH); ++line;
    tft.print(buf);

    const std::string host = wifi_.getHostname();
    snprintf(buf, sizeof(buf), "|%-16s|%-21s|", "Hostname", host.substr(0, 21).c_str());
    tft.setCursor(COL, line * LH); ++line;
    tft.print(buf);

    snprintf(buf, sizeof(buf), "|%-16s|%-21u|", "UDP port", udp_.getPort());
    tft.setCursor(COL, line * LH); ++line;
    tft.print(buf);

    const std::string master_ip = amakerbot_.getMasterIP();
    if (master_ip.empty())
    {
        std::string prompt = "REG: " + token;
        snprintf(buf, sizeof(buf), "|%-16s|%-21s|", "Master", prompt.substr(0, 21).c_str());
    }
    else
    {
        snprintf(buf, sizeof(buf), "|%-16s|%-21s|", "Master IP", master_ip.substr(0, 21).c_str());
    }
    tft.setCursor(COL, line * LH); ++line;
    tft.print(buf);

    // ── Transport counters ──────────────────────────────────────────────────
    tft.setCursor(COL, line * LH); ++line;
    tft.print("+----------------+---------------------+");

    snprintf(buf, sizeof(buf), "|UDP %-12s|%7lu  rej %6lu|",
             "in / rej",
             static_cast<unsigned long>(udp_.getRxCount()),
             static_cast<unsigned long>(udp_.getDroppedCount()));
    tft.setCursor(COL, line * LH); ++line;
    tft.print(buf);

    snprintf(buf, sizeof(buf), "|UDP %-12s|%7lu               |",
             "out",
             static_cast<unsigned long>(udp_.getTxCount()));
    tft.setCursor(COL, line * LH); ++line;
    tft.print(buf);

    snprintf(buf, sizeof(buf), "|WS  %-12s|%7lu  rej %6lu|",
             "in / rej",
             static_cast<unsigned long>(ws_.getRxCount()),
             static_cast<unsigned long>(ws_.getDroppedCount()));
    tft.setCursor(COL, line * LH); ++line;
    tft.print(buf);

    snprintf(buf, sizeof(buf), "|WS  %-12s|%7lu               |",
             "out",
             static_cast<unsigned long>(ws_.getTxCount()));
    tft.setCursor(COL, line * LH); ++line;
    tft.print(buf);

    // Footer aligns with the servo table header on the right panel
    tft.setCursor(COL, line * LH);
    tft.print("+--------------------------------------+");
}

// ---------------------------------------------------------------------------
// drawServoTable — right column (chars 21+, lines 0–10)
// ---------------------------------------------------------------------------

void AppScreen::drawServoTable()
{
    uint16_t fg, bg;
    colorsForStatus(motor_servo_.getStatus(), fg, bg);

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(fg, bg);

    int line = 0;
    constexpr int COL = RPX;

    tft.setCursor(COL, line * LH); ++line;
    tft.print("+----+-----+------+");
    tft.setCursor(COL, line * LH); ++line;
    tft.print("| Servos          |");
    tft.setCursor(COL, line * LH); ++line;
    tft.print("+--+---+----------+");

    tft.setTextColor(UIColors::CLR_INFO, UIColors::CLR_BLACK);

    for (uint8_t ch = 0; ch < MotorServoConsts::SERVO_COUNT; ++ch)
    {
        char linebuf[20]; // right panel = 19 chars + NUL
        const ServoType stype = motor_servo_.getServoType(ch);
        int16_t angle = 0;
        motor_servo_.getServosAngle(1u << ch, &angle);

        const char *type_str = servoTypeLabel(stype);
        if (stype == ServoType::CONTINUOUS)
            snprintf(linebuf, sizeof(linebuf), "|S%u|%3s|%+6d  |", ch, type_str, static_cast<int>(angle));
        else
            snprintf(linebuf, sizeof(linebuf), "|S%u|%3s|%6d  |",  ch, type_str, static_cast<int>(angle));

        tft.setCursor(COL, line * LH); ++line;
        tft.print(linebuf);
    }

    tft.setCursor(COL, line * LH);
    tft.print("+--+---+----------+");
}

// ---------------------------------------------------------------------------
// drawMotorTable — right column (chars 21+, below servo table)
// ---------------------------------------------------------------------------

void AppScreen::drawMotorTable()
{
    uint16_t fg, bg;
    colorsForStatus(motor_servo_.getStatus(), fg, bg);

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(fg, bg);

    // servo header (3 lines) + 6 servo rows + footer (1 line) = 10 lines → start at line 10
    int line = 10;
    constexpr int COL = RPX;

    tft.setCursor(COL, line * LH); ++line;
    tft.print("+-----------+    ");
    tft.setCursor(COL, line * LH); ++line;
    tft.print("| DC Motors |    ");
    tft.setCursor(COL, line * LH); ++line;
    tft.print("+---+-------+    ");

    tft.setTextColor(UIColors::CLR_INFO, UIColors::CLR_BLACK);

    for (uint8_t motor = 1; motor <= MotorServoConsts::MOTOR_COUNT; ++motor)
    {
        char linebuf[20];
        int8_t speed = 0;
        motor_servo_.getMotorSpeeds(1u << (motor - 1), &speed);

        snprintf(linebuf, sizeof(linebuf), "|M%u |%+5d  |    ", motor, static_cast<int>(speed));
        tft.setCursor(COL, line * LH); ++line;
        tft.print(linebuf);
    }

    tft.setCursor(COL, line * LH);
    tft.print("+---+-------+    ");
}

