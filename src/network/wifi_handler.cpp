#include <WiFi.h>
#include "wifi_handler.h"
#include "../fs/persistance.h"
#ifdef DEBUG
#define DEBUG_TO_SERIAL(x) Serial.println(x)
#define DEBUGF_TO_SERIAL(fmt, ...) Serial.printf(fmt, __VA_ARGS__)
#else
#define DEBUG_TO_SERIAL(x)
#define DEBUGF_TO_SERIAL(fmt, ...)
#endif

Persistance persistance;
// create wifi access point
bool WifiModule::open_access_point()
{
    // Set device as WiFi access point
    const char *ap_ssid = "K10-Bot";
    const char *ap_password = "12345678";

    // Configure WiFi as Access Point
    if (!WiFi.softAP(ap_ssid, ap_password))
    {
        return false;
    }

    // Get and print the AP IP address
    IPAddress apIP = WiFi.softAPIP();
    DEBUG_TO_SERIAL("Access Point IP: ");
    DEBUG_TO_SERIAL(apIP);

    return true;
}

bool WifiModule::connect_to_wifi(const char *ssid, const char *password)
{
    // Stop any existing WiFi connection
    WiFi.disconnect(true); // true = turn off WiFi radio
    delay(100);

    // Set WiFi mode to Station
    WiFi.mode(WIFI_STA);

    // Begin WiFi connection
    DEBUG_TO_SERIAL("Connecting to WiFi: ");
    DEBUG_TO_SERIAL(ssid);

    WiFi.begin(ssid, password);

    // Wait for connection with timeout (up to 10 seconds)
    int attempts = 0;
    const int max_attempts = 20; // 20 * 500ms = 10 seconds

    while (WiFi.status() != WL_CONNECTED && attempts < max_attempts)
    {
        delay(500);
        DEBUG_TO_SERIAL(".");
        attempts++;
    }

    DEBUG_TO_SERIAL("");

    // Check if connection was successful
    if (WiFi.status() == WL_CONNECTED)
    {
        DEBUG_TO_SERIAL("WiFi connected! IP address: ");
        DEBUG_TO_SERIAL(WiFi.localIP());
        return true;
    }
    else
    {
        DEBUG_TO_SERIAL("Failed to connect to WiFi");
        return false;
    }
}

bool WifiModule::disconnect_from_wifi()
{
    WiFi.disconnect(true); // true = turn off WiFi radio
    delay(100);
    DEBUG_TO_SERIAL("WiFi disconnected.");
    return true;
}

bool WifiModule::connect_and_fallback(const char *ssid, const char *password)
{

    if (!ssid || strlen(ssid) == 0)
    {
        DEBUG_TO_SERIAL("SSID was empty.");
        ssid = Persistance::getSetting("ssid").c_str();
        DEBUG_TO_SERIAL("SSID loaded from settings.");
    }
    if (!password || strlen(password) == 0)
    {
        DEBUG_TO_SERIAL("Password was empty.");
        password = Persistance::getSetting("password").c_str();
        DEBUG_TO_SERIAL("Password loaded from settings.");
    }

    if (connect_to_wifi(ssid, password))
    {
        return true;
    }
    else
    {
        DEBUG_TO_SERIAL("Falling back to Access Point mode.");
        return open_access_point();
    }
}