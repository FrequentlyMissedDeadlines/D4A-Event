#include <WiFi.h>
#include <Preferences.h>
#include "WiFiService.h"
#include "RollingLogger.h"
#include "SettingsService.h"
#include "../ui/utb2026.h"

// WiFi Access Point credentials
#define DEFAULT_AP_SSID "aMaker-"
#define DEFAULT_AP_PASSWORD "amaker-club"
#define DEFAULT_WIFI_SSID "missing local SSID here"
#define DEFAULT_WIFI_PASSWORD "missing local password here"
#define DEFAULT_HOSTNAME "amaker-bot"

// ref documentation:
//  https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/preferences.html

extern SettingsService settingsService;

constexpr char SETTINGS_KEY_WIFI_SSID[] = "WIFI_SSID";
constexpr char SETTINGS_KEY_WIFI_PASSWORD[] = "WIFI_PASSWORD";
constexpr char SETTINGS_KEY_AP_SSID[] = "AP_SSID";
constexpr char SETTINGS_KEY_AP_PASSWORD[] = "AP_PASSWORD";
constexpr char SETTINGS_KEY_HOSTNAME[] = "HOSTNAME";
std::string WIFI_SSID = DEFAULT_WIFI_SSID;
std::string WIFI_PWD = DEFAULT_WIFI_PASSWORD;
std::string AP_SSID = DEFAULT_AP_SSID;
std::string AP_PASSWORD = DEFAULT_AP_PASSWORD;
std::string HOSTNAME = DEFAULT_HOSTNAME;
std::string IP = "";
std::string CONNECTED_SSID = "";
std::string ESP_SUFFIX = "";

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
    return HOSTNAME + ESP_SUFFIX;
}
std::string WifiService::getServiceName()
{
    return "wifi/v1";
}
// create wifi access point
bool WifiService::open_access_point()
{

    logger->info("AP SSID: " + AP_SSID + ESP_SUFFIX);
    logger->info("AP Password: " + AP_PASSWORD);
    logger->info("Hostname: " + HOSTNAME + ESP_SUFFIX);
    // Ensure WiFi is reset and set to AP mode before creating the SoftAP
    WiFi.disconnect(true);
    delay(100);
    // Use AP+STA so device can remain capable of station operations
    WiFi.mode(WIFI_AP_STA);
    WiFi.setHostname((HOSTNAME + ESP_SUFFIX).c_str());

    // ESP WiFi softAP requires WPA2 passwords of length >= 8. If the
    // configured password is shorter, create an open AP instead.

    if (!WiFi.softAP((AP_SSID + ESP_SUFFIX).c_str(), AP_PASSWORD.c_str()))
    {
        logger->error("Failed to create AP " + AP_SSID);
        return false;
    }

    // Get and print the AP IP address
    IPAddress apIP = WiFi.softAPIP();
    IP = std::string(apIP.toString().c_str());
    CONNECTED_SSID = AP_SSID+ESP_SUFFIX;
    if (logger)
    {
        logger->warning("WiFi: AP " + AP_SSID + ESP_SUFFIX + " " + IP);
    }
    return true;
}

bool WifiService::connect_to_wifi(std::string ssid, std::string password)
{
    ssid = ssid;
    if (ssid.length() == 0)
    {
        if (logger)
        {
            logger->warning("Missing SSID.");
        }
        return false;
    }
    if (password.length() == 0)
    {
        if (logger)
        {
            logger->warning("Missing password.");
        }
        return false;
    }

    // Stop any existing WiFi connection
    WiFi.disconnect(true); // true = turn off WiFi radio
    delay(100);  // Register 404 handler before starting services


    // Set WiFi mode to Station
    WiFi.mode(WIFI_STA);

    // Begin WiFi connection
    // logger->info("Connecting to WiFi: " + ssid);
    WiFi.begin(ssid.c_str(), password.c_str());

    // Wait for connection with timeout (up to 10 seconds)
    int attempts = 0;
    const int max_attempts = 20; // 20 * 500ms = 10 seconds
    logger->info("Connecting to " + ssid);
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
        logger->info("WiFi: " + ssid + " " + IP);
        
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
    loadSettings();
    char mac_hex[8];
    snprintf(mac_hex, sizeof(mac_hex), "%06X", (uint32_t)(ESP.getEfuseMac() >> 24));
    ESP_SUFFIX = std::string(mac_hex);
    WiFi.setHostname((HOSTNAME+ESP_SUFFIX).c_str());
    // Initialization successful (preferences read). Return true so start_service proceeds.
    logger->debug(getServiceName() + " initialize done");
    return true;
}
bool WifiService::startService()
{
   
    
#ifdef DEBUG
    if (logger)
        logger->info("Starting WiFi service...");
#endif
    // test with hardcoded credentials for now (you can't reuse this)
    bool result = connect_and_fallback(WIFI_SSID, WIFI_PWD);
    if (logger)
        logger->info(result ? "WiFi started successfully" : "WiFi start failed");
    // Set unique hostname based on MAC address

    if (result)
    {
#ifdef DEBUG
        logger->debug(getName() + " start done");
#endif
    }
    else
    {
        logger->error(getServiceName() + " start failed");
    }
    return result;
}
bool WifiService::stopService()
{
    bool result = disconnect_from_wifi();
    if (result)
    {
#ifdef DEBUG
        logger->debug(getName() + " stop done");
#endif
    }
    else
    {
        logger->error(getServiceName() + " stop failed");
    }
    return result;
}
bool WifiService::disconnect_from_wifi()
{
    WiFi.disconnect(true); // true = turn off WiFi radio
    delay(100);
    return true;
}

bool WifiService::connect_and_fallback(std::string ssid, std::string password)
{
    if (logger)
    {
        logger->info("Activation of WiFi.");
    }

    if (connect_to_wifi(ssid, password))
    {
        return true;
    }

    logger->info("Falling back to Access Point mode.");
    return open_access_point();
}

bool WifiService::saveSettings()
{
    bool saved = true;
    saved = settingsService.setSetting(getServiceName(), SETTINGS_KEY_WIFI_SSID, WIFI_SSID) && saved;
    saved = settingsService.setSetting(getServiceName(), SETTINGS_KEY_WIFI_PASSWORD, WIFI_PWD) && saved;
    saved = settingsService.setSetting(getServiceName(), SETTINGS_KEY_AP_SSID, AP_SSID) && saved;
    saved = settingsService.setSetting(getServiceName(), SETTINGS_KEY_AP_PASSWORD, AP_PASSWORD) && saved;
    saved = settingsService.setSetting(getServiceName(), SETTINGS_KEY_HOSTNAME, HOSTNAME) && saved;

    return true;
}

bool WifiService::loadSettings()
{
    WIFI_SSID = settingsService.getSetting(getServiceName(), SETTINGS_KEY_WIFI_SSID, DEFAULT_WIFI_SSID);
    WIFI_PWD = settingsService.getSetting(getServiceName(), SETTINGS_KEY_WIFI_PASSWORD, DEFAULT_WIFI_PASSWORD);
    AP_SSID = settingsService.getSetting(getServiceName(), SETTINGS_KEY_AP_SSID, DEFAULT_AP_SSID);
    AP_PASSWORD = settingsService.getSetting(getServiceName(), SETTINGS_KEY_AP_PASSWORD, DEFAULT_AP_PASSWORD);
    HOSTNAME = settingsService.getSetting(getServiceName(), SETTINGS_KEY_HOSTNAME, DEFAULT_HOSTNAME);
    logger->info("WiFi settings loaded:");
    logger->info("-WiFi SSID: " + WIFI_SSID);
    logger->info("-WiFi PWD: " + WIFI_PWD);
    logger->info("-AP SSID: " + AP_SSID);
    logger->info("-AP PWD: " + AP_PASSWORD);
    logger->info("-Hostname: " + HOSTNAME);     
    return true;
}
