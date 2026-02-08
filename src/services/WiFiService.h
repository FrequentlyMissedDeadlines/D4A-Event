/**
 * @file WiFiService.h
 * @brief Header for WiFi module integration with the main application
 * @details Provides methods to manage WiFi connections and access points.
 */
#pragma once
#include <WebServer.h>
#include "IsServiceInterface.h"

/**
 * @class WifiService
 * @brief Service for managing WiFi connections and access point functionality
 */
class WifiService : public IsServiceInterface
{
public:
    bool wifi_activation();

    bool registerRoutes(WebServer *webserver);
    
    std::string getIP();
    std::string getSSID();
    std::string getHostname();
    
    std::string getServiceName() override;
    
    bool initializeService() override;
    bool startService() override;
    bool stopService() override;

    bool saveSettings() override;
    bool loadSettings() override;


protected:

    bool open_access_point();
    bool connect_to_wifi(std::string ssid, std::string password);
    bool disconnect_from_wifi();
    bool connect_and_fallback(std::string ssid, std::string password);
};