#include <WiFi.h>
#include <Preferences.h>
#include "WiFiService.h"
#include "RollingLogger.h"

#include "../ui/utb2026.h"

// WiFi Access Point credentials
#define AP_SSID "aMaker-"
#define AP_PASSWORD "amakerclub"
#define WIFI_SETTINGS_NAMESPACE "wifi"
#define WIFI_SETTINGS_KEY_SSID "ssid"
#define WIFI_SETTINGS_KEY_PASSWORD "password"
// ref documentation:
//  https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/preferences.html
Preferences preferences;
std::string IP = "";
std::string CONNECTED_SSID = "";
std::string HOSTNAME = "";

std::string WifiService::getIP()
{
    return IP;
}
std::string WifiService::getSSID()
{
    return CONNECTED_SSID;
}
std::string WifiService::getHostname()
{
    return HOSTNAME;
}
std::string WifiService::getName()
{
    return "wifi/v1";
}
// create wifi access point
bool WifiService::open_access_point()
{
    // Set device as WiFi access point
    char mac_hex[8];
    snprintf(mac_hex, sizeof(mac_hex), "%06X", (uint32_t)(ESP.getEfuseMac() >> 24));
    std::string ap_ssid = AP_SSID + std::string(mac_hex);
    std::string ap_password = AP_PASSWORD;
    std::string hostname = "k10-bot-" + std::string(mac_hex);
    HOSTNAME = hostname;
    logger->info("AP SSID: " + ap_ssid); 
    logger->info("AP Password: " + ap_password);
    logger->info("Hostname: " + hostname);
    // Ensure WiFi is reset and set to AP mode before creating the SoftAP
    WiFi.disconnect(true);
    delay(100);
    // Use AP+STA so device can remain capable of station operations
    WiFi.mode(WIFI_AP_STA);
    WiFi.setHostname(hostname.c_str());
    
    WiFi.setHostname("k10-bot");
    // ESP WiFi softAP requires WPA2 passwords of length >= 8. If the
    // configured password is shorter, create an open AP instead.
    
    if (!WiFi.softAP(ap_ssid.c_str(), ap_password.c_str()))
    {
        logger->error("Failed to create AP " + ap_ssid);
        return false;
    }


    // Get and print the AP IP address
    IPAddress apIP = WiFi.softAPIP();
    IP = std::string(apIP.toString().c_str());
    CONNECTED_SSID = ap_ssid;
    if (logger) {
        logger->warning("AP SSID " + ap_ssid + " " + IP);
    }
    return true;
}

bool WifiService::connect_to_wifi(std::string ssid, std::string password)
{ssid = ssid;
    if (ssid.length() == 0)
    {
        if (logger) {
            logger->warning("Missing SSID.");
        }
        return false;
    }
    if (password.length() == 0)
    {
        if (logger) {
            logger->warning("Missing password.");
        }
        return false;
    }

    // Stop any existing WiFi connection
    WiFi.disconnect(true); // true = turn off WiFi radio
    delay(100);

    // Set WiFi mode to Station
    WiFi.mode(WIFI_STA);
    


    // Begin WiFi connection
    //logger->info("Connecting to WiFi: " + ssid);
    WiFi.begin(ssid.c_str(), password.c_str());

    // Wait for connection with timeout (up to 10 seconds)
    int attempts = 0;
    const int max_attempts = 20; // 20 * 500ms = 10 seconds
    logger->info("Connecting to "+ssid);
    while (WiFi.status() != WL_CONNECTED && attempts < max_attempts)
    {
#ifdef DEBUG
       logger->debug(" Attempt " + std::to_string(attempts + 1) + "/" + std::to_string(max_attempts));
#endif
        delay(1000);
        attempts++;
    }

    // Check if connection was successful
    if (WiFi.status() == WL_CONNECTED)
    {
        IP = std::string(WiFi.localIP().toString().c_str());
        logger->info("-SSID:" + ssid );
        logger->info("-IP:" + IP);
        CONNECTED_SSID = ssid;
        return true;
    }
    else
    {
        logger->error("Failed to connect to " + ssid);
        return false;
    }
}
bool WifiService::initializeService()
{
    preferences.begin(WIFI_SETTINGS_NAMESPACE, true); // Read-only mode
    std::string ssid = std::string(preferences.getString(WIFI_SETTINGS_KEY_SSID, "").c_str());
    std::string password = std::string(preferences.getString(WIFI_SETTINGS_KEY_PASSWORD, "").c_str());
    preferences.end();
    
    // Initialization successful (preferences read). Return true so start_service proceeds.
    return true;
}
bool WifiService::startService()
{
        char mac_hex[8];
    snprintf(mac_hex, sizeof(mac_hex), "%06X", (uint32_t)(ESP.getEfuseMac() >> 24));
    std::string hostname = "amaker-bot-" + std::string(mac_hex);
    WiFi.setHostname(hostname.c_str());
    logger->info("Hostname: " + hostname);
    if (logger) logger->info("Starting WiFi service...");
    // test with hardcoded credentials for now (you can't reuse this :)
    bool result = connect_and_fallback("", ""); 
    if (logger) logger->info(result ? "WiFi started successfully" : "WiFi start failed");
        // Set unique hostname based on MAC address

    return result;
}
bool WifiService::stopService()
{
    return disconnect_from_wifi();
}
bool WifiService::disconnect_from_wifi()
{
    WiFi.disconnect(true); // true = turn off WiFi radio
    delay(100);
    return true;
}

bool WifiService::connect_and_fallback(std::string ssid, std::string password)
{       
    if (logger) {
        logger->info("Activation of WiFi.");
    }

    if (connect_to_wifi(ssid, password))
    {
        return true;
    }

        logger->info("Falling back to Access Point mode.");
    return open_access_point();
}