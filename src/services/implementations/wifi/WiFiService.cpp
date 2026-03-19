/**
 * @file WiFiService.cpp
 * @brief WiFi connectivity service — STA mode with automatic AP fallback.
 *
 * @details Credentials default to compile-time values (development only).
 *          At runtime they are loaded from / saved to NVS via the ESP32
 *          Preferences library (namespace "wifi").
 *
 * @warning SECURITY — the default STA credentials below are for development
 *          and testing only.  Do NOT commit real credentials to source control.
 *          Use loadSettings() / saveSettings() to manage credentials securely
 *          at provisioning time.
 */

#include <WiFi.h>
#include <Preferences.h>
#include "services/WiFiService.h"
#include "RollingLogger.h"
#include "FlashStringHelper.h"
#include <Arduino.h>    // millis(), delay(), ESP

// ---------------------------------------------------------------------------
// Compile-time default credentials
// ---------------------------------------------------------------------------
#define DEFAULT_WIFI_SSID       "Freebox-A35871"
#define DEFAULT_WIFI_PASSWORD   "azerQSDF1234"
#define DEFAULT_AP_SSID         "aMaker-"
#define DEFAULT_AP_PASSWORD     "amaker-club"
#define DEFAULT_HOSTNAME        "amakerbot-"

#define WIFI_CONN_MAX_ATTEMPTS  8    ///< Attempts before giving up on STA
#define WIFI_CONN_SLEEP_MS      500  ///< ms between connection attempts

// ---------------------------------------------------------------------------
// PROGMEM string constants
// ---------------------------------------------------------------------------
namespace WiFiConsts
{
    constexpr const char str_service_name[]     PROGMEM = "WiFi Service";
    constexpr const char path_service[]         PROGMEM = "wifi/v1";

    constexpr const char msg_starting[]         PROGMEM = "Starting WiFi service...";
    constexpr const char msg_activation[]       PROGMEM = "Activating WiFi.";
    constexpr const char msg_connecting_to[]    PROGMEM = "Connecting to ";
    constexpr const char msg_attempt[]          PROGMEM = " attempt #";
    constexpr const char msg_connected[]        PROGMEM = "WiFi STA: ";
    constexpr const char msg_failed_connect[]   PROGMEM = "Failed to connect: ";
    constexpr const char msg_fallback_ap[]      PROGMEM = "Falling back to AP mode.";
    constexpr const char msg_ap_created[]       PROGMEM = "WiFi AP: ";
    constexpr const char msg_ap_failed[]        PROGMEM = "Failed to create AP: ";
    constexpr const char msg_missing_ssid[]     PROGMEM = "Missing SSID.";
    constexpr const char msg_missing_password[] PROGMEM = "Missing password.";
    constexpr const char msg_start_ok[]         PROGMEM = "WiFi started successfully.";
    constexpr const char msg_start_failed[]     PROGMEM = "WiFi start failed.";
    constexpr const char msg_settings_loaded[]  PROGMEM = "WiFi settings loaded.";
    constexpr const char msg_settings_saved[]   PROGMEM = "WiFi settings saved.";
    constexpr const char msg_prefs_open_fail[]  PROGMEM = "WiFi: Preferences open failed.";

    // NVS keys — plain C strings (Preferences uses them directly)
    constexpr const char nvs_namespace[]   = "wifi";
    constexpr const char nvs_ssid[]        = "ssid";
    constexpr const char nvs_password[]    = "password";
    constexpr const char nvs_ap_ssid[]     = "ap_ssid";
    constexpr const char nvs_ap_password[] = "ap_password";
    constexpr const char nvs_hostname[]    = "hostname";
}

// ---------------------------------------------------------------------------
// Module-level state (scoped to this translation unit)
// ---------------------------------------------------------------------------
static std::string s_wifi_ssid    = DEFAULT_WIFI_SSID;
static std::string s_wifi_pwd     = DEFAULT_WIFI_PASSWORD;
static std::string s_ap_ssid      = DEFAULT_AP_SSID;
static std::string s_ap_password  = DEFAULT_AP_PASSWORD;
static std::string s_hostname     = DEFAULT_HOSTNAME;
static std::string s_connected_ip;
static std::string s_connected_ssid;
static std::string s_mac_suffix;   ///< 6-char uppercase hex suffix derived from MAC

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

std::string WifiService::getIP()
{
    return s_connected_ip;
}

std::string WifiService::getSSID()
{
    return s_connected_ssid;
}

std::string WifiService::getHostname()
{
    return s_hostname + s_mac_suffix;
}

std::string WifiService::getServiceName()
{
    return progmem_to_string(WiFiConsts::str_service_name);
}

// ---------------------------------------------------------------------------
// open_access_point
// ---------------------------------------------------------------------------

/**
 * @brief Create a WiFi SoftAP using the configured AP credentials.
 *        The full SSID is AP_SSID + MAC suffix (e.g. "aMaker-A3F2B1").
 */
bool WifiService::open_access_point()
{
    const std::string full_ap_ssid = s_ap_ssid + s_mac_suffix;

    if (logger)
    {
        logger->info(std::string("AP SSID: ") + full_ap_ssid);
        logger->info(std::string("Hostname: ") + getHostname());
    }

    WiFi.disconnect(true);
    delay(100);

    // AP+STA keeps the radio available for both modes
    WiFi.mode(WIFI_AP_STA);
    WiFi.setHostname(getHostname().c_str());

    if (!WiFi.softAP(full_ap_ssid.c_str(), s_ap_password.c_str()))
    {
        if (logger)
            logger->error(FPSTR(WiFiConsts::msg_ap_failed) + full_ap_ssid);
        return false;
    }

    s_connected_ip   = WiFi.softAPIP().toString().c_str();
    s_connected_ssid = full_ap_ssid;

    if (logger)
        logger->warning(FPSTR(WiFiConsts::msg_ap_created) + full_ap_ssid
                        + " " + s_connected_ip);
    return true;
}

// ---------------------------------------------------------------------------
// connect_to_wifi
// ---------------------------------------------------------------------------

/**
 * @brief Attempt to connect to an existing WiFi network.
 *        Retries WIFI_CONN_MAX_ATTEMPTS times before returning false.
 */
bool WifiService::connect_to_wifi(const std::string &ssid,
                                  const std::string &password)
{
    if (ssid.empty())
    {
        if (logger)
            logger->warning(FPSTR(WiFiConsts::msg_missing_ssid));
        return false;
    }
    if (password.empty())
    {
        if (logger)
            logger->warning(FPSTR(WiFiConsts::msg_missing_password));
        return false;
    }

    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    if (logger)
        logger->info(FPSTR(WiFiConsts::msg_connecting_to) + ssid);

    for (int i = 0; i < WIFI_CONN_MAX_ATTEMPTS && WiFi.status() != WL_CONNECTED; ++i)
    {
        if (logger)
            logger->info(FPSTR(WiFiConsts::msg_attempt)
                         + std::to_string(i + 1)
                         + std::string("/")
                         + std::to_string(WIFI_CONN_MAX_ATTEMPTS));
        delay(WIFI_CONN_SLEEP_MS);
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        s_connected_ip   = WiFi.localIP().toString().c_str();
        s_connected_ssid = ssid;
        if (logger)
            logger->info(FPSTR(WiFiConsts::msg_connected) + ssid + " " + s_connected_ip);
        return true;
    }

    if (logger)
        logger->error(FPSTR(WiFiConsts::msg_failed_connect) + ssid);
    return false;
}

// ---------------------------------------------------------------------------
// disconnect_from_wifi
// ---------------------------------------------------------------------------

bool WifiService::disconnect_from_wifi()
{
    WiFi.disconnect(true);
    delay(100);
    s_connected_ip.clear();
    s_connected_ssid.clear();
    return true;
}

// ---------------------------------------------------------------------------
// connect_and_fallback
// ---------------------------------------------------------------------------

/**
 * @brief Try STA connection; open AP on failure.
 */
bool WifiService::connect_and_fallback(const std::string &ssid,
                                       const std::string &password)
{
    if (logger)
        logger->info(FPSTR(WiFiConsts::msg_activation));

    if (connect_to_wifi(ssid, password))
        return true;

    if (logger)
        logger->info(FPSTR(WiFiConsts::msg_fallback_ap));

    return open_access_point();
}

// ---------------------------------------------------------------------------
// wifi_activation  (public convenience wrapper)
// ---------------------------------------------------------------------------

bool WifiService::wifi_activation()
{
    return connect_and_fallback(s_wifi_ssid, s_wifi_pwd);
}

// ---------------------------------------------------------------------------
// initializeService
// ---------------------------------------------------------------------------

bool WifiService::initializeService()
{
    // Load credentials first so startService() uses up-to-date values
    loadSettings();

    // Build a 6-char uppercase hex suffix from the upper 3 bytes of the ESP32 MAC
    char mac_buf[8];
    snprintf(mac_buf, sizeof(mac_buf), "%06X",
             static_cast<uint32_t>(ESP.getEfuseMac() >> 24));
    s_mac_suffix = std::string(mac_buf);

    WiFi.setHostname(getHostname().c_str());

    setServiceStatus(INITIALIZED);
#ifdef VERBOSE_DEBUG
    if (logger)
        logger->debug(getServiceName() + " " + getStatusString());
#endif
    return true;
}

// ---------------------------------------------------------------------------
// startService
// ---------------------------------------------------------------------------

bool WifiService::startService()
{
#ifdef VERBOSE_DEBUG
    if (logger)
        logger->info(FPSTR(WiFiConsts::msg_starting));
#endif

    const bool ok = connect_and_fallback(s_wifi_ssid, s_wifi_pwd);

    if (logger)
        logger->info(ok ? FPSTR(WiFiConsts::msg_start_ok)
                        : FPSTR(WiFiConsts::msg_start_failed));

    setServiceStatus(ok ? STARTED : START_FAILED);

#ifdef VERBOSE_DEBUG
    if (logger)
        logger->debug(getServiceName() + " " + getStatusString());
#endif
    return ok;
}

// ---------------------------------------------------------------------------
// stopService
// ---------------------------------------------------------------------------

bool WifiService::stopService()
{
    const bool ok = disconnect_from_wifi();
    setServiceStatus(ok ? STOPPED : STOP_FAILED);
#ifdef VERBOSE_DEBUG
    if (logger)
        logger->debug(getServiceName() + " " + getStatusString());
#endif
    return ok;
}

// ---------------------------------------------------------------------------
// saveSettings  — NVS via Preferences
// ---------------------------------------------------------------------------

bool WifiService::saveSettings()
{
    Preferences prefs;
    if (!prefs.begin(WiFiConsts::nvs_namespace, /* readOnly= */ false))
    {
        if (logger)
            logger->error(FPSTR(WiFiConsts::msg_prefs_open_fail));
        return false;
    }

    prefs.putString(WiFiConsts::nvs_ssid,        s_wifi_ssid.c_str());
    prefs.putString(WiFiConsts::nvs_password,     s_wifi_pwd.c_str());
    prefs.putString(WiFiConsts::nvs_ap_ssid,      s_ap_ssid.c_str());
    prefs.putString(WiFiConsts::nvs_ap_password,  s_ap_password.c_str());
    prefs.putString(WiFiConsts::nvs_hostname,     s_hostname.c_str());
    prefs.end();

    if (logger)
        logger->info(FPSTR(WiFiConsts::msg_settings_saved));
    return true;
}

// ---------------------------------------------------------------------------
// loadSettings  — NVS via Preferences
// ---------------------------------------------------------------------------

bool WifiService::loadSettings()
{
    Preferences prefs;
    if (!prefs.begin(WiFiConsts::nvs_namespace, /* readOnly= */ true))
    {
        // No saved settings yet — silently keep compile-time defaults
        return false;
    }

    s_wifi_ssid   = prefs.getString(WiFiConsts::nvs_ssid,        DEFAULT_WIFI_SSID).c_str();
    s_wifi_pwd    = prefs.getString(WiFiConsts::nvs_password,     DEFAULT_WIFI_PASSWORD).c_str();
    s_ap_ssid     = prefs.getString(WiFiConsts::nvs_ap_ssid,      DEFAULT_AP_SSID).c_str();
    s_ap_password = prefs.getString(WiFiConsts::nvs_ap_password,  DEFAULT_AP_PASSWORD).c_str();
    s_hostname    = prefs.getString(WiFiConsts::nvs_hostname,     DEFAULT_HOSTNAME).c_str();
    prefs.end();

    if (logger)
    {
        logger->info(FPSTR(WiFiConsts::msg_settings_loaded));
        logger->info(std::string("- STA SSID: ")  + s_wifi_ssid);
        logger->info(std::string("- AP  SSID: ")  + s_ap_ssid + s_mac_suffix);
        logger->info(std::string("- Hostname: ")  + getHostname());
    }
    return true;
}
