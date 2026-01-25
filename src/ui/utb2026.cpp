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

constexpr int MAX_IP_LEN = 15;
constexpr int MAX_NETWORK_LEN = 24;   // adjust as needed
constexpr int OUTPUT_LEN = 40;
constexpr int LINE_HEIGHT = 10;
constexpr int CHAR_WIDTH = 6;
extern TFT_eSPI tft;



ServoInfo servos[5] = {
    ServoInfo(NOT_CONNECTED, 0),
    ServoInfo(NOT_CONNECTED, 0),
    ServoInfo(NOT_CONNECTED, 0),
    ServoInfo(NOT_CONNECTED, 0),
    ServoInfo(NOT_CONNECTED, 0)
    };

const int R_OUTER = 24; // * - 3 strings, len 60: strin 240/num/2 servos per row

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
std::string format_with_equal_spacing(const std::vector<std::string> &values, int output_len) {
    if (output_len <= 0) return "";
    if (values.empty()) {
        return std::string(output_len, ' ');
    }
    
    // Pre-allocate output string with spaces
    std::string output(output_len, ' ');
    
    const size_t count = values.size();
    const int spacing = output_len / count;
    
    // Place each string at its calculated position
    for (size_t i = 0; i < count; ++i) {
        const int start_pos = i * spacing;
        if (start_pos >= output_len) break;
        
        const std::string& value = values[i];
        if (value.length() == 0) continue;
        
        // Calculate how much space is available for this string
        int available_space;
        if (i < count - 1) {
            // Not the last string: can use up to next string's start position
            available_space = spacing;
        } else {
            // Last string: can use remaining space
            available_space = output_len - start_pos;
        }
        
        // Copy the string (or as much as fits)
        const int copy_len = std::min(static_cast<int>(value.length()), available_space);
        value.copy(&output[start_pos], copy_len, 0);
    }
    
    return output;
}

void format_networkInfoString(const char *network, const char *ip, char *output, int total_width) {
    char netbuf[MAX_NETWORK_LEN + 1];
    char ipbuf[MAX_IP_LEN + 1];

    // Truncate network name if too long
    strncpy(netbuf, network, MAX_NETWORK_LEN);
    netbuf[MAX_NETWORK_LEN] = '\0';

    // Truncate IP if too long
    strncpy(ipbuf, ip, MAX_IP_LEN);
    ipbuf[MAX_IP_LEN] = '\0';

    // Calculate space padding
    int net_len = strlen(netbuf);
    int ip_len = strlen(ipbuf);

    // Ensure at least one space
    int spaces = total_width - net_len - ip_len;
    if (spaces < 1) spaces = 1;

    // Build final string
    snprintf(output, OUTPUT_LEN + total_width, "%s%*s%s", netbuf, spaces, "", ipbuf);
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

void UTB2026::update_servo(const char &number, int &value, ConnectionStatus &status)
{
    if (number < 1 || number > 5)
    { return;
    }
    servos[number - 1].setValue(value);
    servos[number - 1].connectionStatus = status;
};

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
void UTB2026::draw_servos() { 
    std::vector<std::string> servoLabels = {"S1", "S2", "S3", "S4", "S5"};
    char line = 4;
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    
    std::string output = format_with_equal_spacing(servoLabels, OUTPUT_LEN);
    tft.setCursor(0, LINE_HEIGHT * line++);
    tft.print(output.c_str());
    
    // Build status strings
    std::vector<std::string> statusStrings;
    for (char i = 0; i < 5; i++) {
        switch (servos[i].connectionStatus) {
            case ROTATIONAL:
                statusStrings.push_back("R360");
                break;
            case ANGULAR180:
                statusStrings.push_back("A180");
                break;
            case ANGULAR270:
                statusStrings.push_back("A270");
                break;
            default:
                statusStrings.push_back("----");
                break;
        }
    }
    output = format_with_equal_spacing(statusStrings, OUTPUT_LEN);
    tft.setCursor(0, LINE_HEIGHT * line++);
    tft.print(output.c_str());

    // Build value strings
    std::vector<std::string> valueString;
    for (char i = 0; i < 5; i++) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%4d", servos[i].value);
        valueString.push_back(buf);
    }
    output = format_with_equal_spacing(valueString, OUTPUT_LEN);
    tft.setCursor(0, LINE_HEIGHT * line++);
    tft.print(output.c_str());
};

void UTB2026::draw_network_info() {

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    char line=0;

    std::string wifi_name = UTB2026::get_info(KEY_WIFI_NAME).substr(0,MAX_NETWORK_LEN);
    std::string ip_addr = UTB2026::get_info(KEY_IP_ADDRESS).substr(0,MAX_IP_LEN);
    
    std::vector<std::string> wifivalues = {wifi_name, ip_addr};
    std::string output = format_with_equal_spacing(wifivalues, OUTPUT_LEN);
    tft.setCursor(0* CHAR_WIDTH,LINE_HEIGHT*line++);
    tft.print(output.c_str());
    
    tft.setCursor(0,LINE_HEIGHT*line++);
    std::vector<std::string> udplabels = {KEY_UDP_STATE, KEY_UDP_PORT, KEY_UDP_IN, KEY_UDP_DROP};
    std::string textline = format_with_equal_spacing(udplabels, 40);
    tft.print(textline.c_str());
    tft.setCursor(0,LINE_HEIGHT*line++);
    
    std::string state = UTB2026::get_info(KEY_UDP_STATE, "?");
    std::string port = UTB2026::get_info(KEY_UDP_PORT, "?");
    std::string in = std::to_string(UTB2026::get_counter(KEY_UDP_IN));
    std::string drop = std::to_string(UTB2026::get_counter(KEY_UDP_DROP));
    
    std::vector<std::string> udpvalues = {state, port, in, drop};
    textline = format_with_equal_spacing(udpvalues, 40);
    
    tft.print(textline.c_str());

    // tft.setTextDatum(TC_DATUM);
    // tft.setCursor(20* CHAR_WIDTH,LINE_HEIGHT*line);
    // tft.print("Msg in.");
    // tft.setCursor(20* CHAR_WIDTH,LINE_HEIGHT*line+1);
    // tft.print( getCounter(KEY_UDP_IN));
    
    // tft.setTextDatum(TR_DATUM);
    // tft.setCursor(40* CHAR_WIDTH,LINE_HEIGHT*line);
    // tft.print("Msg drop.");
    // tft.setCursor(40* CHAR_WIDTH,LINE_HEIGHT*line+1);
    // tft.print( getCounter(KEY_UDP_DROP));


    // line 2 : HTTP: [status] [port] Req:[req]
};

void UTB2026::draw_all()

{   tft.setViewport(0, 0, 240, 80);
    tft.fillScreen(TFT_BROWN);
    draw_network_info();
    draw_servos();
};

