// UDP handler module - non-blocking packet callback and message buffer
#pragma once

#include <AsyncUDP.h>
#include <WebServer.h>
#include <set>
#include <string>
#include <Arduino.h>
#include "HasRoutesInterface.h"
#include "IsServiceInterface.h"
/**
 * @file udp_server.h
 * @brief Header for UDP server module.
 * @details Provides methods to receive UDP messages and maintain statistics.   
 * Inherits from withRoutes to register HTTP routes with a WebServer instance.
 * 
 */
class UDPService : public HasRoutesInterface, public    IsServiceInterface
{
public:
/**
    // Initialize UDP server on given port; returns true if successful
    */
    bool begin(AsyncUDP *udp, int port);
    /**
    Try to copy the latest message for display; returns true if copied
    */

    bool registerRoutes(WebServer *server, std::string basePath) override;
    bool init() override;
    bool start() override;
    bool stop() override; 

private:
    AsyncUDP *udp = nullptr;
    int port = 0;
    
    /**
    Build a JSON message containing all infos.

    */
    std::string buildJson();

    // Get the number of dropped packets (thread-safe)
    unsigned long getDroppedPackets();
    // Get the number of handled packets (thread-safe)
    unsigned long getHandledPackets();
};