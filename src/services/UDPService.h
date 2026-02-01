// UDP handler module - non-blocking packet callback and message buffer
#pragma once

#include <AsyncUDP.h>
#include <WebServer.h>
#include <set>
#include <string>
#include <vector>
#include <Arduino.h>
#include "IsOpenAPIInterface.h"
#include "IsServiceInterface.h"
/**
 * @file UDPService.h
 * @brief Header for UDP server module.
 * @details Provides methods to receive UDP messages and maintain statistics.
 * Inherits from withRoutes to register HTTP routes with a WebServer instance.
 *
 */
class UDPService : public IsOpenAPIInterface, public IsServiceInterface
{
public:
    bool registerRoutes() override;
    std::string getServiceSubPath() override;
    bool initializeService() override;
    bool startService() override;
    bool stopService() override;
    std::string getServiceName() override;
    IsOpenAPIInterface* asOpenAPIInterface() override { return this; }

    bool saveSettings() override;
    bool loadSettings() override;

    bool begin(AsyncUDP *udpInstance = nullptr, int listenPort = 0);
    int getPort() const { return port; }

protected:
    AsyncUDP *udp = nullptr;
    AsyncUDP *udpHandle = nullptr;
    bool udpOwned = false;
    int port = 24642;

    /**
     * @fn buildJson
     * @brief Build a JSON message containing UDP server statistics
     * @return JSON string with server information
     */
    std::string buildJson();
    std::vector<OpenAPIRoute> routes = {};
    // Get the number of dropped packets (thread-safe)
    unsigned long getDroppedPackets();
    // Get the number of handled packets (thread-safe)
    unsigned long getHandledPackets();
};