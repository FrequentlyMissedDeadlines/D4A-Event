/**
 * @file utb2026.cpp
 * @brief UTB2026 UI Module

 * text height is 10 pixel
 * screen is 320x240
 * This shows
 * - bottom part
 */

#include "utb2026.h"
#include <TFT_eSPI.h>
#include "services/ServoService.h"
#include <stdio.h>
#include <string.h>
#include <vector>
#include "services/UDPService.h"
#include "services/HTTPService.h"
#include "services/AmakerBotService.h"
#include "services/WiFiService.h"
#include <ESPAsyncWebServer.h>

namespace UTB2026Consts
{
    constexpr int max_ip_len = 15;
    constexpr int max_network_len = 24;
    constexpr int output_len = 40;
    constexpr int line_height = 8;
    constexpr int char_width = 6;
    constexpr uint16_t color_module_started_bkg = TFT_DARKGREEN;
    constexpr uint16_t color_module_started_txt = TFT_WHITE;
    constexpr uint16_t color_module_started_failed_bkg = TFT_RED;
    constexpr uint16_t color_module_started_failed_txt = TFT_YELLOW;
    constexpr uint16_t color_module_uninitialized_bkg = TFT_RED;;
    constexpr uint16_t color_module_uninitialized_txt = TFT_LIGHTGREY;
    constexpr uint16_t color_module_initialized_bkg = TFT_GOLD;
    constexpr uint16_t color_module_initialized_txt = TFT_WHITE;
    constexpr uint16_t color_module_initialized_failed_bkg = TFT_RED;
    constexpr uint16_t color_module_initialized_failed_txt = TFT_YELLOW;
    constexpr uint16_t color_module_stopped_bkg = TFT_RED;
    constexpr uint16_t color_module_stopped_txt = TFT_WHITE;
    constexpr uint16_t color_module_stop_failed_bkg = TFT_RED;
    constexpr uint16_t color_module_stop_failed_txt = TFT_YELLOW;
    constexpr uint16_t color_error = TFT_RED;
    constexpr uint16_t color_warning = TFT_YELLOW;
    constexpr uint16_t color_info = TFT_WHITE;
    constexpr uint16_t color_debug = TFT_LIGHTGREY;
    constexpr uint16_t color_module_default_bkg = TFT_DARKGREY;
    constexpr uint16_t color_module_default_txt = TFT_WHITE;
    // line is line number (position increments by line_height), column is character position (increments by char_width)
    constexpr uint16_t table_motor_line = 23;
    constexpr uint16_t table_motor_column = 21* UTB2026Consts::char_width;
    constexpr uint16_t table_servo_line = 12;
    constexpr uint16_t table_servo_column = 21* UTB2026Consts::char_width;
    constexpr uint16_t table_amakerbot_line = 0;
    constexpr uint16_t table_amakerbot_column = 0* UTB2026Consts::char_width;
    constexpr uint16_t table_udp_line = 12;
    constexpr uint16_t table_udp_column = 0* UTB2026Consts::char_width;
    
}

extern TFT_eSPI tft;

extern AsyncWebServer webserver;
extern UDPService udp_service;
extern HTTPService http_service;
extern ServoService servo_service;
extern AmakerBotService amakerbot_service;
extern WifiService wifi_service;

// const int R_OUTER = 24; // * - 3 strings, len 60: strin 240/num/2 servos per row

/**
 * Formats strings with first characters equally positioned across output length.
 * @param values Vector of strings to format
 * @param output_len Expected output string length
 * @return std::string of exactly output_len with values positioned at equal intervals
 *
 * Examples:
 * - 1 string, len 40: string at position 0
 * - 2 strings, len 40: strings at positions 0, 20
 * - 3 strings, len 60: strings at positions 0, 20, 40
 */
std::string format_with_equal_spacing(const std::vector<std::string> &values, int output_len)
{
    if (output_len <= 0)
        return "";
    if (values.empty())
    {
        return std::string(output_len, ' ');
    }

    // Pre-allocate output string with spaces
    std::string output(output_len, ' ');

    const size_t count = values.size();
    const int spacing = output_len / count;

    // Place each string at its calculated position
    for (size_t i = 0; i < count; ++i)
    {
        const int start_pos = i * spacing;
        if (start_pos >= output_len)
            break;

        const std::string &value = values[i];
        if (value.length() == 0)
            continue;

        // Calculate how much space is available for this string
        int available_space;
        if (i < count - 1)
        {
            // Not the last string: can use up to next string's start position
            available_space = spacing;
        }
        else
        {
            // Last string: can use remaining space
            available_space = output_len - start_pos;
        }

        // Copy the string (or as much as fits)
        const int copy_len = std::min(static_cast<int>(value.length()), available_space);
        value.copy(&output[start_pos], copy_len, 0);
    }

    return output;
}

void format_networkInfoString(const char *network, const char *ip, char *output, int total_width)
{
    char netbuf[UTB2026Consts::max_network_len + 1];
    char ipbuf[UTB2026Consts::max_ip_len + 1];

    // Truncate network name if too long
    strncpy(netbuf, network, UTB2026Consts::max_network_len);
    netbuf[UTB2026Consts::max_network_len] = '\0';

    // Truncate IP if too long
    strncpy(ipbuf, ip, UTB2026Consts::max_ip_len);
    ipbuf[UTB2026Consts::max_ip_len] = '\0';

    // Calculate space padding
    int net_len = strlen(netbuf);
    int ip_len = strlen(ipbuf);

    // Ensure at least one space
    int spaces = total_width - net_len - ip_len;
    if (spaces < 1)
        spaces = 1;

    // Build final string
    snprintf(output, UTB2026Consts::output_len + total_width, "%s%*s%s", netbuf, spaces, "", ipbuf);
}

std::map<std::string, long> counters;
std::map<std::string, std::string> infos;
void UTB2026::init()
{
    inc_counter(KEY_UDP_IN, 0);
    inc_counter(KEY_UDP_OUT, 0);
    inc_counter(KEY_UDP_DROP, 0);
    inc_counter(KEY_HTTP_REQ, 0);
};

void UTB2026::set_logger_instances(RollingLogger *debug_log, RollingLogger *app_log, RollingLogger *esp_log)
{
    debug_logger_ = debug_log;
    app_logger_ = app_log;
    esp_logger_ = esp_log;
}

void UTB2026::next_display_mode()
{
    previous_display_mode_ = current_display_mode_;
    switch (current_display_mode_)
    {
    case MODE_APP_UI:
        current_display_mode_ = MODE_APP_LOG;
        break;
    case MODE_APP_LOG:
        current_display_mode_ = MODE_DEBUG_LOG;
        break;
    case MODE_DEBUG_LOG:
        current_display_mode_ = MODE_ESP_LOG;
        break;
    case MODE_ESP_LOG:
        current_display_mode_ = MODE_APP_UI;
        break;
    }
}

UTB2026::DisplayMode UTB2026::get_display_mode() const
{
    return current_display_mode_;
}

void UTB2026::set_display_mode(DisplayMode mode)
{
    current_display_mode_ = mode;
    // previous_display_mode_ is intentionally left unchanged so draw_all()
    // detects the transition and clears the screen on the next frame.
}

void UTB2026::set_info(const std::string &key, const std::string &value)
{
    infos[key] = value;
};
void UTB2026::inc_counter(const std::string &name, long increment)
{
    if (counters.find(name) == counters.end())
    {
        counters[name] = increment;
    }
    else
    {
        counters[name] += increment;
    }
};

// void UTB2026::update_servo(const char &number, int &value, ServoConnection &status)
// {
//     if (number < 1 || number > 5)
//     {
//         return;
//     }
//     // servos[number - 1].setValue(value);
//     // servos[number - 1].connectionStatus = status;
// };

std::map<std::string, std::string> UTB2026::get_infos()
{
    return infos;
};

std::string UTB2026::get_info(std::string key, std::string default_value)
{
    if (infos.find(key) == infos.end())
    {
        return default_value;
    }
    return infos[key];
};
std::map<std::string, long> UTB2026::get_counters()
{
    return counters;
};

long UTB2026::get_counter(std::string key)
{
    if (counters.find(key) == counters.end())
    {
        return 0;
    }
    return counters[key];
};

void  set_colors_for_module_status(IsServiceInterface &service)
{
    switch (service.getStatus())
    {    case UNINITIALIZED:
        tft.setTextColor(UTB2026Consts::color_module_uninitialized_txt, UTB2026Consts::color_module_uninitialized_bkg);
        break;
    case INITIALIZED:
        tft.setTextColor(UTB2026Consts::color_module_initialized_txt, UTB2026Consts::color_module_initialized_bkg);
        break;
    case INITIALIZED_FAILED:
        tft.setTextColor(UTB2026Consts::color_module_initialized_failed_txt, UTB2026Consts::color_module_initialized_failed_bkg);
        break;
    case STARTED:
        tft.setTextColor(UTB2026Consts::color_module_started_txt, UTB2026Consts::color_module_started_bkg);
        break;
    case START_FAILED:
        tft.setTextColor(UTB2026Consts::color_module_started_failed_txt, UTB2026Consts::color_module_started_failed_bkg);
        break;  
    case STOPPED:
        tft.setTextColor(UTB2026Consts::color_module_stopped_txt, UTB2026Consts::color_module_stopped_bkg);
        break;
    case STOP_FAILED:
        tft.setTextColor(UTB2026Consts::color_module_stop_failed_txt, UTB2026Consts::color_module_stop_failed_bkg);
        break;
    default:
        tft.setTextColor(UTB2026Consts::color_module_default_txt, UTB2026Consts::color_module_default_bkg);
        break;  

    }
}
/**
 * @brief Render servo status — one line per channel (0-7)
 * Format:
 *   not connected: "S[n] not connected"
 *   rotational:    "S[n] cont  spd: +50"
 *   angular 180:   "S[n] 180   ang: 090"
 *   angular 270:   "S[n] 270   ang: 135"
 * Starts at line 7 (after 3 network/UDP lines + 4 motor lines).
 * formated in table |<servo number : 2 char> | <type/status 4 char> | <value if applicable> |
 *
 */
void UTB2026::draw_servos()
{
    int LINE = UTB2026Consts::table_servo_line;
    constexpr int START_CHAR = UTB2026Consts::table_servo_column; // indent from left for servo section
    set_colors_for_module_status(servo_service);
    tft.setTextDatum(TL_DATUM);

    tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
    tft.print("+-----------------+");
    tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
    tft.print("| Servos          |");
    tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
    tft.print("+----+-----+------+");

    for (uint8_t ch = 0; ch < 6; ++ch)
    {
        const ServoConnection conn = servo_service.getServoConnection(ch);
        char linebuf[41];

        switch (conn)
        {
        case NOT_CONNECTED:
            snprintf(linebuf, sizeof(linebuf), "| S%u |     |      |", ch);
            break;
        case ROTATIONAL:
        {
            const int8_t spd = servo_service.getServoSpeed(ch);
            if (spd == -128)
                snprintf(linebuf, sizeof(linebuf), "| S%u | rot |      |", ch);
            else
                snprintf(linebuf, sizeof(linebuf), "| S%u | rot | %+4d |", ch, static_cast<int>(spd));
            break;
        }
        case ANGULAR_180:
        {
            const int16_t ang = servo_service.getServoAngle(ch);
            if (ang < 0)
                snprintf(linebuf, sizeof(linebuf), "| S%u | 180 |      |", ch);
            else
                snprintf(linebuf, sizeof(linebuf), "| S%u | 180 | %4d |", ch, static_cast<int>(ang));
            break;
        }
        case ANGULAR_270:
        {
            const int16_t ang = servo_service.getServoAngle(ch);
            if (ang < 0)
                snprintf(linebuf, sizeof(linebuf), "| S%u | 270 |      |", ch);
            else
                snprintf(linebuf, sizeof(linebuf), "| S%u | 270 | %4d |", ch, static_cast<int>(ang));
            break;
        }
        default:
            snprintf(linebuf, sizeof(linebuf), "| S%u |  ?  |      |", ch);
            break;
        }

        tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
        tft.print(linebuf);


    }
    tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
    tft.print("+----+-----+------+");
}

/**
    * @brief Render UDP handler statistics — one line per action code seen
 */
void UTB2026::draw_udp_handlers()
{   set_colors_for_module_status(udp_service);
    tft.setTextDatum(TL_DATUM);
    int LINE = UTB2026Consts::table_udp_line;
    constexpr int START_CHAR = UTB2026Consts::table_udp_column;
    const int MAX_DISPLAY = 15; // Max lines to display to avoid overflowing screen
    tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
    tft.print("+----+------+------+");
    tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
    tft.print("|UDP |  kept|denied|");
    tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
    tft.print("+----+------+------+");
    
    UDPService::UDPActionStat stats[UDPService::UDP_MAX_ACTION_STATS];
    const uint8_t count = udp_service.getActionStats(stats, UDPService::UDP_MAX_ACTION_STATS);
    for (int i = 0; i < MAX_DISPLAY; ++i)
    {
        char linebuf[41];
        if (i < count)
        {
            snprintf(linebuf, sizeof(linebuf), "|0x%02X|%6lu|%6lu|",
                     static_cast<unsigned>(stats[i].action_code),
                     static_cast<unsigned long>(stats[i].accepted),
                     static_cast<unsigned long>(stats[i].rejected));
                      tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
                        tft.print(linebuf);
    
        }
        // else
        // {
        //     linebuf[0] = '\0';
        // }
        // Pad to 40 chars to overwrite stale content
    }
    tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
    tft.print("+----+------+------+");
    
}

void UTB2026::draw_motors()
{

    int LINE = UTB2026Consts::table_motor_line;
    constexpr int START_CHAR = UTB2026Consts::table_motor_column ;

    set_colors_for_module_status(servo_service);
    tft.setTextDatum(TL_DATUM);

    tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
    tft.print("+-----------+");
    tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
    tft.print("| DC Motors |");
    tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
    tft.print("+-----+-----+");

    for (uint8_t motor = 1; motor <= 4; ++motor)
    {
        const int8_t spd = servo_service.getMotorSpeed(motor);
        char linebuf[41];

        if (spd == -128)
        {
            snprintf(linebuf, sizeof(linebuf), "| M%u |   nc |", motor);
        }
        else
        {
            snprintf(linebuf, sizeof(linebuf), "| M%u | %+4d |", motor, static_cast<int>(spd));
        }

        // Pad to 40 chars to overwrite any stale characters
        tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
        tft.print(linebuf);
    }
    tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
    tft.print("+----+------+");
}

void UTB2026::draw_amakerbot_info()
{
    set_colors_for_module_status(amakerbot_service);
    tft.setTextDatum(TL_DATUM);
    int LINE = UTB2026Consts::table_amakerbot_line;
    constexpr int START_CHAR = UTB2026Consts::table_amakerbot_column;
    char linebuf[41];
    tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
    tft.print("+--------------------------------------+");
    tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
    tft.print("| Amaker Bot                           |");
    tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
    tft.print("+----------------+---------------------+");

    snprintf(linebuf, sizeof(linebuf), "|%-16s|%-21s|", "Wifi SSID", get_info(KEY_WIFI_NAME, "-").c_str());
    tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
    tft.print(linebuf);

    snprintf(linebuf, sizeof(linebuf), "|%-16s|%-21s|", "Host name", wifi_service.getHostname().c_str());
    tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
    tft.print(linebuf);
    snprintf(linebuf, sizeof(linebuf), "|%-16s|%-21s|", "Host IP", get_info(KEY_IP_ADDRESS, "-").c_str());
    tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
    tft.print(linebuf);
    snprintf(linebuf, sizeof(linebuf), "|%-16s|%-21d|", "UDP port", static_cast<int>(udp_service.getPort()));
    tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
    tft.print(linebuf);
    if (amakerbot_service.getMasterIP().empty())
    {
        snprintf(linebuf, sizeof(linebuf), "|%-16s|%-21s|", "Master IP", ("REGISTER WITH " + amakerbot_service.getServerToken()).c_str());
        tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
        tft.print(linebuf);
        snprintf(linebuf, sizeof(linebuf), "|%-16s|%-21s|", "Master UDP link","N/A" );
        tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
        tft.print(linebuf);
    }
    else
    {
        snprintf(linebuf, sizeof(linebuf), "|%-16s|%-21s|", "Master IP", amakerbot_service.getMasterIP().c_str());
        tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
        tft.print(linebuf);
        snprintf(linebuf, sizeof(linebuf), "|%-16s|%-21s|", "Master UDP link", get_info(KEY_UDP_STATE, "None").c_str());
        tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
        tft.print(linebuf);
    }
    tft.setCursor(START_CHAR, UTB2026Consts::line_height * (LINE++));
    tft.print("+----------------+---------------------+");
};

void UTB2026::add_logger_view(RollingLogger *logger, int x1, int y1, int x2, int y2, uint16_t text_color, uint16_t bg_color)
{
    logger_view view;
    view.logger_instance = logger;
    view.vp_x = x1;
    view.vp_y = y1;
    view.vp_width = x2 - x1;
    view.vp_height = y2 - y1;
    view.text_color = text_color;
    view.bg_color = bg_color;
    logger_views.push_back(view);
}

void UTB2026::draw_logger()
{
    for (const auto &view : logger_views)
    {
        if (view.logger_instance == nullptr)
            continue;

        tft.setViewport(view.vp_x, view.vp_y, view.vp_width, view.vp_height);
        tft.fillRect(view.vp_x, view.vp_y, view.vp_width, view.vp_height, view.bg_color);

        const auto &log_rows = view.logger_instance->get_log_rows();
        int max_rows = view.logger_instance->get_max_rows();
        int n_rows = log_rows.size();
        int start_index = (n_rows > max_rows) ? (n_rows - max_rows) : 0;

        for (int i = 0; i < max_rows; ++i)
        {
            int u = start_index + i;
            if (u >= 0 && u < n_rows)
            {
                const auto &line = log_rows[u];

                // Use custom text color if different from background, otherwise use log-level colors
                if (view.text_color != view.bg_color)
                {
                    tft.setTextColor(view.text_color);
                }
                else
                {
                    // Set color based on log level
                    switch (line.level)
                    {
                    case RollingLogger::DEBUG:
                        tft.setTextColor(UTB2026Consts::color_debug);
                        break;
                    case RollingLogger::INFO:
                        tft.setTextColor(UTB2026Consts::color_info);
                        break;
                    case RollingLogger::WARNING:
                        tft.setTextColor(UTB2026Consts::color_warning);
                        break;
                    case RollingLogger::ERROR:
                        tft.setTextColor(UTB2026Consts::color_warning);
                        break;
                    default:
                        tft.setTextColor(UTB2026Consts::color_info);
                        break;
                    }
                }

#ifdef VERBOSE_DEBUG
                const char *level_name = "?";
                switch (line.level)
                {
                case RollingLogger::DEBUG:
                    level_name = "d";
                    break;
                case RollingLogger::INFO:
                    level_name = "i";
                    break;
                case RollingLogger::WARNING:
                    level_name = "w";
                    break;
                case RollingLogger::ERROR:
                    level_name = "e";
                    break;
                case RollingLogger::TRACE:
                    level_name = "t";
                    break;
                }
                tft.setCursor(view.vp_x, view.vp_y + i * LINE_HEIGHT);
                tft.print(level_name);
                tft.print("|");
#else
                tft.setCursor(view.vp_x, view.vp_y + i * UTB2026Consts::line_height);
#endif
                tft.print(line.message.c_str());
            }
        }
    }
}

void UTB2026::draw_all()
{
    tft.resetViewport();

    // Only clear screen when mode changes to avoid flicker
    bool mode_changed = (current_display_mode_ != previous_display_mode_);
    if (mode_changed)
    {
        tft.fillScreen(TFT_BLACK);
        previous_display_mode_ = current_display_mode_;
    }

    switch (current_display_mode_)
    {
    case MODE_APP_UI:
        // Show application UI (network info + servos) without logger views
        tft.setViewport(0, 0, 240, 320);
        if (mode_changed)
        {
            tft.fillRect(0, 0, 240, 320, TFT_BLACK);
        }
        draw_amakerbot_info();
        draw_motors();
        draw_servos();
        draw_udp_handlers();
        break;

    case MODE_APP_LOG:
        // Show app log full screen
        if (app_logger_ != nullptr)
        {
            unsigned long current_version = app_logger_->get_version();
            if (!mode_changed && current_version == last_drawn_app_log_version_)
                break;
            last_drawn_app_log_version_ = current_version;

            tft.setViewport(0, 0, 240, 320);
            const auto &log_rows = app_logger_->get_log_rows();
            int max_rows = 32; // 320 pixels / 10 pixels per line
            int n_rows = log_rows.size();
            int start_index = (n_rows > max_rows) ? (n_rows - max_rows) : 0;

            for (int i = 0; i < max_rows; ++i)
            {
                int y_pos = i * UTB2026Consts::line_height;
                // Clear line first to remove old text
                tft.fillRect(0, y_pos, 240, UTB2026Consts::line_height, TFT_BLACK);

                if ((start_index + i) < n_rows)
                {
                    const auto &line = log_rows[start_index + i];
                    tft.setTextColor(TFT_WHITE, TFT_BLACK);
                    tft.setCursor(0, y_pos);
                    tft.print(line.message.c_str());
                }
            }
        }
        break;

    case MODE_DEBUG_LOG:
        // Show debug log full screen
        if (debug_logger_ != nullptr)
        {
            unsigned long current_version = debug_logger_->get_version();
            if (!mode_changed && current_version == last_drawn_debug_log_version_)
                break;
            last_drawn_debug_log_version_ = current_version;

            tft.setViewport(0, 0, 240, 320);
            const auto &log_rows = debug_logger_->get_log_rows();
            int max_rows = 32; // 320 pixels / 10 pixels per line
            int n_rows = log_rows.size();
            int start_index = (n_rows > max_rows) ? (n_rows - max_rows) : 0;

            for (int i = 0; i < max_rows; ++i)
            {
                int y_pos = i * UTB2026Consts::line_height;
                // Clear line first to remove old text
                tft.fillRect(0, y_pos, 240, UTB2026Consts::line_height, TFT_BLACK);

                if ((start_index + i) < n_rows)
                {
                    const auto &line = log_rows[start_index + i];
                    // Color by log level
                    uint16_t text_color;
                    switch (line.level)
                    {
                    case RollingLogger::DEBUG:
                        text_color = UTB2026Consts::color_debug;
                        break;
                    case RollingLogger::INFO:
                        text_color = UTB2026Consts::color_info;
                        break;
                    case RollingLogger::WARNING:
                        text_color = UTB2026Consts::color_warning;
                        break;
                    case RollingLogger::ERROR:
                        text_color = UTB2026Consts::color_error;
                        break;
                    default:
                        text_color = UTB2026Consts::color_info;
                        break;
                    }
                    tft.setTextColor(text_color, TFT_BLACK);
                    tft.setCursor(0, y_pos);
                    tft.print(line.message.c_str());
                }
            }
        }
        break;

    case MODE_ESP_LOG:
        // Show ESP log full screen
        if (esp_logger_ != nullptr)
        {
            unsigned long current_version = esp_logger_->get_version();
            if (!mode_changed && current_version == last_drawn_esp_log_version_)
                break;
            last_drawn_esp_log_version_ = current_version;

            tft.setViewport(0, 0, 240, 320);
            const auto &log_rows = esp_logger_->get_log_rows();
            int max_rows = 32; // 320 pixels / 10 pixels per line
            int n_rows = log_rows.size();
            int start_index = (n_rows > max_rows) ? (n_rows - max_rows) : 0;

            for (int i = 0; i < max_rows; ++i)
            {
                int y_pos = i * UTB2026Consts::line_height;
                // Clear line first to remove old text
                tft.fillRect(0, y_pos, 240, UTB2026Consts::line_height, TFT_BLACK);

                if ((start_index + i) < n_rows)
                {
                    const auto &line = log_rows[start_index + i];
                    // Color by log level
                    uint16_t text_color;
                    switch (line.level)
                    {
                    case RollingLogger::DEBUG:
                        text_color = UTB2026Consts::color_debug;
                        break;
                    case RollingLogger::INFO:
                        text_color = UTB2026Consts::color_info;
                        break;
                    case RollingLogger::WARNING:
                        text_color = UTB2026Consts::color_warning;
                        break;
                    case RollingLogger::ERROR:
                        text_color = UTB2026Consts::color_error;
                        break;
                    default:
                        text_color = UTB2026Consts::color_info;
                        break;
                    }
                    tft.setTextColor(text_color, TFT_BLACK);
                    tft.setCursor(0, y_pos);
                    tft.print(line.message.c_str());
                }
            }
        }
        break;
    }
};
