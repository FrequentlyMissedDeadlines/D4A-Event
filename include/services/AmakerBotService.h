#pragma once

#include <ESPAsyncWebServer.h>
#include "IsOpenAPIInterface.h"
#include "isUDPMessageHandlerInterface.h"
#include "IsMasterRegistryInterface.h"
#include <freertos/semphr.h>
#include <string>

/**
 * @file AmakerBotService.h
 * @brief Header for master-registration service
 * @details Generates a random 5-character alphanumeric token on startup and logs it
 *          to the device screen (MODE_APP_LOG). Clients must provide this exact token
 *          to register as the master controller. Registration locks the master to the
 *          caller's IP address.
 *
 * Exposed HTTP routes:
 *   POST /api/amakerbot/v1/register?token=<token>  — register caller as master (if token matches)
 *   GET  /api/amakerbot/v1/master                  — query current master info
 *   POST /api/amakerbot/v1/unregister              — clear master (master IP only)
 *   GET  /api/amakerbot/v1/token                   — retrieve the server-generated token (for reference)
 *
 * UDP protocol:
 *   AMAKERBOT:register:<token>  — register UDP sender IP as master (if token matches)
 *   AMAKERBOT:unregister        — clear master (master IP only)
 */
class AmakerBotService : public IsOpenAPIInterface,
                         public IsUDPMessageHandlerInterface
{
public:
    bool registerRoutes() override;
    std::string getServiceSubPath() override;
    std::string getServiceName() override;
    bool initializeService() override;
    bool stopService() override;

    /**
     * @brief Check whether the given IP string is the registered master.
     * @param ip IP address to check
     * @return true if ip matches the current master IP
     */
    bool isMaster(const std::string &ip) const override;

    /**
     * @brief Return the registered master IP address.
     * @return Master IP string, empty string if no master is registered.
     */
    std::string getMasterIP() const override;

    /**
     * @brief Return the server-generated token (for display/debug purposes).
     * @return The 5-character alphanumeric token generated on init.
     */
    std::string getServerToken() const;

    /**
     * @brief Handle incoming UDP messages for master registration.
     * @param message    Raw UDP message string
     * @param remoteIP   Sender IP address
     * @param remotePort Sender UDP port
     * @return true if message was claimed by this service
     */
    bool messageHandler(const std::string &message,
                        const IPAddress &remoteIP,
                        uint16_t remotePort) override;

    IsUDPMessageHandlerInterface *asUDPMessageHandlerInterface() override { return this; }

    /**
     * @brief Programmatically set the master IP (only if token is valid).
     * @param ip    IP address to register as master
     * @param token Token to validate
     * @return true if token matched and master was set, false if token invalid
     */
    bool setMasterIfTokenValid(const std::string &ip, const std::string &token);

    /**
     * @brief Programmatically clear the current master registration.
     */
    void clearMaster();

private:
    /**
     * @brief Generate a random 5-character alphanumeric token.
     * @return Random token string (e.g., "A3K9B")
     */
    std::string generateRandomToken();

    /**
     * @brief Register a client as master (thread-safe) and log to app_info_logger.
     * @param ip IP address of the new master
     */
    void registerMaster(const std::string &ip);

    /**
     * @brief Internal helper — atomically clear master registration and log to app_info_logger.
     */
    void unregisterMaster();

    std::string server_token_;    // Generated once on init, never changes
    std::string master_ip_;       // Current master's IP, empty if none
    SemaphoreHandle_t master_mutex_ = nullptr;
};
