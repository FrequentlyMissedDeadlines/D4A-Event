// UDP handler module - non-blocking packet callback and message buffer
#pragma once

#include <AsyncUDP.h>
#include <WebServer.h>
#include <set>
#include <string>
#include <vector>
#include <functional>
#include <Arduino.h>
#include "../IsOpenAPIInterface.h"
#include "../IsServiceInterface.h"

/**
 * @typedef UDPMessageHandler
 * @brief Callback function type for UDP message handling
 * @param message The received message as a string
 * @param remoteIP The IP address of the sender
 * @param remotePort The port of the sender
 * @return true if the message was handled, false otherwise
 */
using UDPMessageHandler = std::function<bool(const std::string& message, const IPAddress& remoteIP, uint16_t remotePort)>;

/**
 * @file UDPService.h
 * @brief Header for UDP server module.
 * @details Provides methods to receive UDP messages and maintain statistics.
 * Inherits from withRoutes to register HTTP routes with a WebServer instance.
 * Supports callback registration for message handling by other classes.
 *
 */
class UDPService : public IsOpenAPIInterface
{
public:
    bool registerRoutes() override;
    std::string getServiceSubPath() override;
    bool initializeService() override;
    bool startService() override;
    bool stopService() override;
    std::string getServiceName() override;


    bool begin(AsyncUDP *udpInstance = nullptr, int listenPort = 0);
    int getPort() const { return port; }

    /**
     * @brief Register a message handler callback
     * @param handler The callback function to register
     * @return A unique handler ID that can be used to unregister the handler
     */
    int registerMessageHandler(UDPMessageHandler handler);

    /**
     * @brief Unregister a message handler by ID
     * @param handlerId The ID returned by registerMessageHandler
     * @return true if the handler was found and removed, false otherwise
     */
    bool unregisterMessageHandler(int handlerId);

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

private:

    struct MessageHandlerEntry {
        int handler_id;
        UDPMessageHandler handler_callback;
    };
    std::vector<MessageHandlerEntry> message_handlers;
    int next_handler_id = 1;
    SemaphoreHandle_t handler_mutex = nullptr;

    friend void handleUDPPacket(AsyncUDPPacket packet);
};