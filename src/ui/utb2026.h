/**
 * @file utb2026.h
 * @brief Header for UnleashTheBBricks 2026 UI
 */
#include <Arduino.h>
#include <map>
#include <TFT_eSPI.h>
#include "services/ServoService.h"
#include "services/RollingLogger.h"
#pragma once



class UTB2026
{
public:
    void init();
    void add_logger_view(RollingLogger* logger, int x1, int y1, int x2, int y2, uint16_t text_color = TFT_BLACK, uint16_t bg_color = TFT_BLACK);
    void set_info(const std::string &key, const std::string &value);
    void inc_counter(const std::string &name, long increment = 1);
    void update_servo(const char &number, int &value, ServoConnection &status);
    std::map<std::string, std::string> get_infos();
    std::string get_info(std::string key, std::string default_value = "?");
    std::map<std::string, long> get_counters();
    long get_counter(std::string key);
    void draw_all();
    void draw_servos();
    void draw_network_info();
    void draw_logger();

    const std::string KEY_UDP_STATE = "udp?";
    const std::string KEY_UDP_PORT = "udp#";
    const std::string KEY_UDP_IN = "udp->";
    const std::string KEY_UDP_OUT = "udp<-";
    const std::string KEY_UDP_DROP = "udp_drop";
    const std::string KEY_HTTP_STATE = "http?";
    const std::string KEY_HTTP_PORT = "http#";
    const std::string KEY_HTTP_REQ = "http<-";
    const std::string KEY_IP_ADDRESS = "IP";
    const std::string KEY_WIFI_STATE = "wifi?";
    const std::string KEY_WIFI_NAME = "SSID";

private:
    struct logger_view {
        RollingLogger* logger_instance;
        int vp_x;
        int vp_y;
        int vp_width;
        int vp_height;
        uint16_t text_color;
        uint16_t bg_color;
    };
    std::vector<logger_view> logger_views;
};