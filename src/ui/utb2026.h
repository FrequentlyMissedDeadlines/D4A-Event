/**
 * @file utb2026.h
 * @brief Header for UnleashTheBBricks 2026 UI
 */
#include <Arduino.h>
#include <map>
#include "services/ServoService.h"
#pragma once



class UTB2026
{
public:
    void init();
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

    const std::string KEY_UDP_STATE = "udp_status";
    const std::string KEY_UDP_PORT = "udp_port";
    const std::string KEY_UDP_IN = "udp_in";
    const std::string KEY_UDP_OUT = "udp_out";
    const std::string KEY_UDP_DROP = "udp_drop";
    const std::string KEY_HTTP_STATE = "http_status";
    const std::string KEY_HTTP_PORT = "http_port";
    const std::string KEY_HTTP_REQ = "http_req";
    const std::string KEY_IP_ADDRESS = "ip_address";
    const std::string KEY_WIFI_STATE = "wifi_status";
    const std::string KEY_WIFI_NAME = "wifi_name";
};