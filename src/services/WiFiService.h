#pragma once
#include <WebServer.h>
#include "HasLoggerInterface.h"
/**
 * @file wifi_handler.h
 * @brief Header for WiFi module integration with the main application
 * @details Provides methods to manage WiFi connections and access points.
 */
class WifiService : public HasLoggerInterface  {
public:
    bool wifi_activation();
    bool registerRoutes(WebServer* server);
    std::string getIP();
    std::string getSSID();

private:
    bool open_access_point();
    bool connect_to_wifi(std::string ssid, std::string password);
    bool disconnect_from_wifi(  );
    bool connect_and_fallback(std::string ssid, std::string password);
    LoggerService logger ;

};