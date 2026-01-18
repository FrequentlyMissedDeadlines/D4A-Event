#include <WiFi.h>
#include <Preferences.h>
#include "WiFiService.h"
#include "LoggerService.h"
#include "HasLoggerInterface.h"
#include "../ui/utb2026.h"

// WiFi Access Point credentials
#define AP_SSID "aMaker-"
#define AP_PASSWORD "amaker"
#define WIFI_SETTINGS_NAMESPACE "wifi"
#define WIFI_SETTINGS_KEY_SSID "ssid"
#define WIFI_SETTINGS_KEY_PASSWORD "password"
//ref documentation: 
// https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/preferences.html
 Preferences preferences;
 std::string IP="";
 std::string CONNECTED_SSID="";

std::string WifiService::getIP(){
    return IP;
}
std::string WifiService::getSSID(){
    return CONNECTED_SSID;
}
// create wifi access point
bool WifiService::open_access_point()
{
    // Set device as WiFi access point
    char mac_hex[8];
    snprintf(mac_hex, sizeof(mac_hex), "%06X", (uint32_t)(ESP.getEfuseMac() >> 24));
    std::string ap_ssid = AP_SSID + std::string(mac_hex);
    std::string ap_password = AP_PASSWORD;
    logger.info("Opening Access Point: " + ap_ssid);
    // Configure WiFi as Access Point
    if (!WiFi.softAP(ap_ssid.c_str(), ap_password.c_str()))
    {   logger.error("Failed to open Access Point: " + ap_ssid);
        return false;
    }
    logger.info("AP Password: " + ap_password);
    
    // Get and print the AP IP address
    IPAddress apIP = WiFi.softAPIP();
    logger.info("AP IP Address: " + std::string(apIP.toString().c_str()));
    
    IP = std::string(apIP.toString().c_str());
    CONNECTED_SSID = ap_ssid;
    return true;
}

bool WifiService::connect_to_wifi(std::string ssid, std::string password)
{   if (ssid.length() == 0) {
        logger.warning("SSID is empty, cannot connect to WiFi.");
        return false;
    }
    if (password.length() == 0) {
        logger.warning("Password is empty, cannot connect to WiFi.");
        return false;
    }

    // Stop any existing WiFi connection
    WiFi.disconnect(true); // true = turn off WiFi radio
    delay(100);

    // Set WiFi mode to Station
    WiFi.mode(WIFI_STA);

    // Begin WiFi connection
    logger.info("Connecting to WiFi: " + ssid);    
    WiFi.begin(ssid.c_str(), password.c_str());

    // Wait for connection with timeout (up to 10 seconds)
    int attempts = 0;
    const int max_attempts = 20; // 20 * 500ms = 10 seconds

    while (WiFi.status() != WL_CONNECTED && attempts < max_attempts)
    {
        delay(500);

        attempts++;
    }



    // Check if connection was successful
    if (WiFi.status() == WL_CONNECTED)
    {   logger.info("Connected to WiFi: " + ssid + " IP: " + std::string(WiFi.localIP().toString().c_str()));

        return true;
    }
    else
    {logger.error("Failed to connect to WiFi: " + ssid);
        
        return false;
    }
}

bool WifiService::disconnect_from_wifi()
{
    WiFi.disconnect(true); // true = turn off WiFi radio
    delay(100);
    return true;
}

bool WifiService::connect_and_fallback(std::string ssid, std::string password)
{
    if (connect_to_wifi(ssid, password))
    { return true;
    }
    logger.info("Falling back to Access Point mode.");
    return open_access_point();

}

bool WifiService::registerRoutes(WebServer* server) {
    if (!server) {
        return false; 
    }
    server->on("/api/wifi/settings", HTTP_POST, [ server]() {
        try
        {
            /* code */
        
        
        std::string ssid = std::string(server->arg("ssid").c_str());
        std::string password = std::string(server->arg("password").c_str());
        if (ssid.length() == 0) {
            server->send(400, "application/json", "{\"error\":\"SSID cannot be empty.\"}");
            return;
        }
        preferences.begin(WIFI_SETTINGS_NAMESPACE, false); // Read-only mode
        
        preferences.putString(WIFI_SETTINGS_KEY_SSID, ssid.c_str());
        preferences.putString(WIFI_SETTINGS_KEY_PASSWORD, password.c_str());
        preferences.end();
        server->send(200, "application/json", "{\"status\":\"Wifi settings saved successfully.\"}");
    }
    catch(const std::exception& e)
        {
            std::string errorMsg = "{\"error\":\""+std::string(e.what())+".\"}";
            server->send(400, "application/json", errorMsg.c_str());        
        }
    });

    return true;
}


/**
 * @brief Activate WiFi module by loading settings from preferences.
 * If preferences are missing or connection fails, opens an access point.
 * @return true if WiFi is activated (either connected or AP mode), false on failure
 */
bool WifiService::wifi_activation() {
    preferences.begin(WIFI_SETTINGS_NAMESPACE, true); // Read-only mode
    std::string ssid = std::string(preferences.getString(WIFI_SETTINGS_KEY_SSID, "").c_str());
    std::string password = std::string(preferences.getString(WIFI_SETTINGS_KEY_PASSWORD, "").c_str());
    preferences.end();


    return this->connect_and_fallback(ssid, password);
}    