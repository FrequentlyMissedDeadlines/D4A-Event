#include <WiFi.h>
#include <Preferences.h>
#include "WiFiService.h"
#include "../RollingLogger.h"
#include "../FlashStringHelper.h"
#include "../settings/SettingsService.h"
#include "../../ui/utb2026.h"

// SECURITY WARNING: Default WiFi credentials below are hardcoded for development/testing.
// In production, these should be moved to:
// - Environment variables passed during compilation
// - Secure NVS storage encrypted with device-specific keys
// - Configuration files with restricted file permissions
// - External secure configuration management systems
// DO NOT commit real credentials to source control.

// WiFi Access Point credentials
#define DEFAULT_AP_SSID "aMaker-"
#define DEFAULT_AP_PASSWORD "amaker-club"
#define DEFAULT_WIFI_SSID "Freebox-A35871"
#define DEFAULT_WIFI_PASSWORD "azerQSDF1234"
#define DEFAULT_HOSTNAME "amaker-bot"
#define WIFI_CONN_MAX_ATTEMPTS 8
#define WIFI_CONN_SLEEP_MS 500
// ref documentsettings
//  https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/preferences.html

extern SettingsService settings_service;

// WiFiService constants namespace
namespace WiFiConsts
{
    constexpr const char msg_activation_of_wifi[] PROGMEM = "Activation of WiFi.";
    constexpr const char msg_ap_password[] PROGMEM = "AP Password: ";
    constexpr const char msg_ap_pwd_prefix[] PROGMEM = "-AP PWD: ";
    constexpr const char msg_ap_ssid[] PROGMEM = "AP SSID: ";
    constexpr const char msg_ap_ssid_prefix[] PROGMEM = "-AP SSID: ";
    constexpr const char msg_attempt[] PROGMEM = " Attempt #";
    constexpr const char msg_connecting_to[] PROGMEM = "Connecting to ";
    constexpr const char msg_failed_to_connect[] PROGMEM = "Failed to connect to ";
    constexpr const char msg_failed_to_create_ap[] PROGMEM = "Failed to create AP ";
    constexpr const char msg_falling_back_to_ap[] PROGMEM = "Falling back to Access Point mode.";
    constexpr const char msg_hostname[] PROGMEM = "Hostname: ";
    constexpr const char msg_hostname_prefix[] PROGMEM = "-Hostname: ";
    constexpr const char msg_missing_password[] PROGMEM = "Missing password.";
    constexpr const char msg_missing_ssid[] PROGMEM = "Missing SSID.";
    constexpr const char msg_settings_loaded[] PROGMEM = "WiFi settings loaded:";
    constexpr const char msg_starting[] PROGMEM = "Starting WiFi service...";
    constexpr const char msg_wifi_ap[] PROGMEM = "WiFi: AP ";
    constexpr const char msg_wifi_connected[] PROGMEM = "WiFi: ";
    constexpr const char msg_wifi_pwd_prefix[] PROGMEM = "-WiFi PWD: ";
    constexpr const char msg_wifi_ssid_prefix[] PROGMEM = "-WiFi SSID: ";
    constexpr const char msg_wifi_start_failed[] PROGMEM = "WiFi start failed";
    constexpr const char msg_wifi_start_success[] PROGMEM = "WiFi started successfully";
    constexpr const char settings_key_ap_password[] = "AP_PASSWORD";
    constexpr const char settings_key_ap_ssid[] = "AP_SSID";
    constexpr const char settings_key_hostname[] = "HOSTNAME";
    constexpr const char settings_key_wifi_password[] = "WIFI_PASSWORD";
    constexpr const char settings_key_wifi_ssid[] = "WIFI_SSID";
    constexpr const char str_service_name[] PROGMEM = "wifi/v1";
    constexpr const char str_slash[] PROGMEM = "/";
}
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
    return progmem_to_string(WiFiConsts::str_service_name);
}
// create wifi access point
bool WifiService::open_access_point()
{

    logger->info(progmem_to_string(WiFiConsts::msg_ap_ssid) + AP_SSID + ESP_SUFFIX);
    logger->info(progmem_to_string(WiFiConsts::msg_ap_password) + AP_PASSWORD);
    logger->info(progmem_to_string(WiFiConsts::msg_hostname) + HOSTNAME + ESP_SUFFIX);
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
        logger->error(progmem_to_string(WiFiConsts::msg_failed_to_create_ap) + AP_SSID);
        return false;
    }

    // Get and print the AP IP address
    IPAddress apIP = WiFi.softAPIP();
    IP = std::string(apIP.toString().c_str());
    CONNECTED_SSID = AP_SSID+ESP_SUFFIX;
    if (logger)
    {
        logger->warning(progmem_to_string(WiFiConsts::msg_wifi_ap) + AP_SSID + ESP_SUFFIX + progmem_to_string(RoutesConsts::str_space) + IP);
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
            logger->warning(FPSTR(WiFiConsts::msg_missing_ssid));
        }
        return false;
    }
    if (password.length() == 0)
    {
        if (logger)
        {
            logger->warning(FPSTR(WiFiConsts::msg_missing_password));
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
    const int max_attempts = WIFI_CONN_MAX_ATTEMPTS; // 20 * 500ms = 10 seconds
    logger->info(progmem_to_string(WiFiConsts::msg_connecting_to) + ssid);
    while (WiFi.status() != WL_CONNECTED && attempts < max_attempts)
    {  logger->info(progmem_to_string(WiFiConsts::msg_attempt) + std::to_string(attempts + 1) + progmem_to_string(WiFiConsts::str_slash) + std::to_string(max_attempts));
        delay(WIFI_CONN_SLEEP_MS);
        attempts++;
    }

    // Check if connection was successful
    if (WiFi.status() == WL_CONNECTED)
    {
        IP = std::string(WiFi.localIP().toString().c_str());
        logger->info(progmem_to_string(WiFiConsts::msg_wifi_connected) + ssid + progmem_to_string(RoutesConsts::str_space) + IP);
        
        CONNECTED_SSID = ssid;
        return true;
    }
    else
    {
        logger->error(progmem_to_string(WiFiConsts::msg_failed_to_connect) + ssid);
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
    setServiceStatus(INITIALIZED);
    logger->debug(getServiceName() + progmem_to_string(RoutesConsts::str_space) +  getStatusString());   
    return true;
}
bool WifiService::startService()
{
   
    
#ifdef VERBOSE_DEBUG
    if (logger)
        logger->info(progmem_to_string(WiFiConsts::msg_starting));
#endif
    // test with hardcoded credentials for now (you can't reuse this)
    bool result = connect_and_fallback(WIFI_SSID, WIFI_PWD);
    if (logger)
        logger->info(result ? FPSTR(WiFiConsts::msg_wifi_start_success) : FPSTR(WiFiConsts::msg_wifi_start_failed));
    // Set unique hostname based on MAC address

    if (result)
    {
        setServiceStatus(STARTED);
#ifdef VERBOSE_DEBUG
        logger->debug(getServiceName() + " "  + getStatusString());   
#endif
    }
    else
    {
        setServiceStatus(START_FAILED);
        logger->error(getServiceName() + " "  + getStatusString());   
    }
    return result;
}
bool WifiService::stopService()
{
    bool result = disconnect_from_wifi();
    if (result)
    {
        setServiceStatus(STOPPED);
#ifdef VERBOSE_DEBUG
        logger->debug(getServiceName() + " " + getStatusString());
#endif
    }
    else
    {
        setServiceStatus(STOP_FAILED);
        logger->error(getServiceName() + " " + getStatusString());
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
        logger->info(FPSTR(WiFiConsts::msg_activation_of_wifi));
    }

    if (connect_to_wifi(ssid, password))
    {
        return true;
    }

    logger->info(FPSTR(WiFiConsts::msg_falling_back_to_ap));
    return open_access_point();
}

bool WifiService::saveSettings()
{
    if (!settings_service_) {
        if (logger) {
            logger->error("WiFi Service: Settings service not available");
        }
        return false;
    }
    
    bool saved = true;
    saved = settings_service_->setSetting(getServiceName(), WiFiConsts::settings_key_wifi_ssid, WIFI_SSID) && saved;
    saved = settings_service_->setSetting(getServiceName(), WiFiConsts::settings_key_wifi_password, WIFI_PWD) && saved;
    saved = settings_service_->setSetting(getServiceName(), WiFiConsts::settings_key_ap_ssid, AP_SSID) && saved;
    saved = settings_service_->setSetting(getServiceName(), WiFiConsts::settings_key_ap_password, AP_PASSWORD) && saved;
    saved = settings_service_->setSetting(getServiceName(), WiFiConsts::settings_key_hostname, HOSTNAME) && saved;

    return saved;
}

bool WifiService::loadSettings()
{
    if (!settings_service_) {
        if (logger) {
            logger->error("WiFi Service: Settings service not available");
        }
        return false;
    }
    
    WIFI_SSID = settings_service_->getSetting(getServiceName(), WiFiConsts::settings_key_wifi_ssid, DEFAULT_WIFI_SSID);
    WIFI_PWD = settings_service_->getSetting(getServiceName(), WiFiConsts::settings_key_wifi_password, DEFAULT_WIFI_PASSWORD);
    AP_SSID = settings_service_->getSetting(getServiceName(), WiFiConsts::settings_key_ap_ssid, DEFAULT_AP_SSID);
    AP_PASSWORD = settings_service_->getSetting(getServiceName(), WiFiConsts::settings_key_ap_password, DEFAULT_AP_PASSWORD);
    HOSTNAME = settings_service_->getSetting(getServiceName(), WiFiConsts::settings_key_hostname, DEFAULT_HOSTNAME);
    logger->info(FPSTR(WiFiConsts::msg_settings_loaded));
    logger->info(progmem_to_string(WiFiConsts::msg_wifi_ssid_prefix) + WIFI_SSID);
    logger->info(progmem_to_string(WiFiConsts::msg_wifi_pwd_prefix) + WIFI_PWD);
    logger->info(progmem_to_string(WiFiConsts::msg_ap_ssid_prefix) + AP_SSID);
    logger->info(progmem_to_string(WiFiConsts::msg_ap_pwd_prefix) + AP_PASSWORD);
    logger->info(progmem_to_string(WiFiConsts::msg_hostname_prefix) + HOSTNAME);     
    return true;
}
