/**
 * @file WiFiService.h
 * @brief WiFi connectivity service — STA mode with automatic AP fallback.
 *
 * @details Manages all WiFi connectivity for the K10 Bot:
 *   - **Station mode**: connects to a configured access point.
 *   - **AP mode**: creates its own network when the STA connection fails.
 *   - **Automatic fallback**: switches to AP mode on STA timeout.
 *   - **NVS persistence**: credentials are saved/loaded via `Preferences`.
 *
 * Typical usage in main.cpp:
 * @code
 *   WifiService wifi_service;
 *   start_service(wifi_service);          // initialise + start
 *   String ip = wifi_service.getIP().c_str();
 * @endcode
 *
 * @note STA credentials are hardcoded as compile-time defaults and loaded from
 *       NVS on every boot.  Update them at runtime with saveSettings().
 */
#pragma once

#include <string>
#include "IsServiceInterface.h"

/**
 * @class WifiService
 * @brief Service that manages WiFi connectivity with STA/AP fallback.
 */
class WifiService : public IsServiceInterface
{
public:
    // ---- Public helpers ------------------------------------------------

    /**
     * @brief Activate WiFi: try STA, fall back to AP on failure.
     * @return true if either mode is successfully active.
     */
    bool wifi_activation();

    /**
     * @brief Return the current IP address.
     * @return STA local IP in station mode; AP gateway IP in AP mode.
     */
    std::string getIP();

    /**
     * @brief Return the connected SSID.
     * @return Network SSID in STA mode; own AP SSID in AP mode.
     */
    std::string getSSID();

    /**
     * @brief Return the device hostname (base name + 6-char MAC suffix).
     */
    std::string getHostname();

    // ---- IsServiceInterface -------------------------------------------

    /** @return "WiFi Service" */
    std::string getServiceName() override;

    /**
     * @brief Load persisted credentials from NVS and prepare MAC suffix.
     * @return true on success.
     */
    bool initializeService() override;

    /**
     * @brief Connect to WiFi (STA with AP fallback).
     * @return true if either STA or AP mode is active.
     */
    bool startService() override;

    /**
     * @brief Disconnect from WiFi.
     * @return true on success.
     */
    bool stopService() override;

    /**
     * @brief Persist current credentials to NVS (Preferences namespace "wifi").
     * @return true on success.
     */
    bool saveSettings() override;

    /**
     * @brief Load credentials from NVS (Preferences namespace "wifi").
     * @return true on success.
     */
    bool loadSettings() override;

protected:
    /**
     * @brief Create a SoftAP with the configured AP credentials.
     * @return true if the access point was created successfully.
     */
    bool open_access_point();

    /**
     * @brief Attempt to join an existing WiFi network.
     * @param ssid     Network SSID (must not be empty).
     * @param password Network password (must not be empty).
     * @return true if connected (WL_CONNECTED) within the retry window.
     */
    bool connect_to_wifi(const std::string &ssid, const std::string &password);

    /**
     * @brief Disconnect from the current WiFi network and clear IP/SSID state.
     * @return true (always succeeds).
     */
    bool disconnect_from_wifi();

    /**
     * @brief Connect to STA; if it fails, open an access point.
     * @param ssid     Station SSID.
     * @param password Station password.
     * @return true if either STA or AP mode is active.
     */
    bool connect_and_fallback(const std::string &ssid, const std::string &password);
};
