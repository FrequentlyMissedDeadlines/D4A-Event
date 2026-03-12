#pragma once

#include <ESPAsyncWebServer.h>
#include "IsOpenAPIInterface.h"
#include "isUDPMessageHandlerInterface.h"
#include "IsMasterRegistryInterface.h"
#include "services/UDPService.h"
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
 *   GET  /api/amakerbot/v1/display                 — get current TFT display mode
 *   POST /api/amakerbot/v1/display/next            — cycle to next display mode (same as button A)
 *   POST /api/amakerbot/v1/display?mode=<mode>     — set display mode (APP_UI|APP_LOG|DEBUG_LOG|ESP_LOG)
 *   GET  /api/amakerbot/v1/name                    — get current bot name
 *   POST /api/amakerbot/v1/name?name=<name>        — set bot name (max 32 chars)
 *
 * UDP protocol (service_id 0x4):
 *   [0x41]<token>  — register UDP sender IP as master (if token matches);
 *   [0x42]         — clear master (master IP only);
 *   [0x43]         — heartbeat keep-alive; must be sent every ≤50 ms or all motors are stopped
 *   [0x45]         — get bot name; server replies with [0x45][name bytes];
 *   [0x46]<name>   — set bot name (master only); server replies with [0x46][status];
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
     * @brief Return the current bot name.
     * @return Bot name string (default: "K10-Bot").
     */
    std::string getBotName() const;

    /**
     * @brief Set the bot name (thread-safe).
     * @param name New name (1–32 characters); ignored if empty or > 32 chars.
     */
    void setBotName(const std::string &name);

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
     * @brief Check whether the master heartbeat has timed out (>50 ms without a
     *        heartbeat from the registered master) and, if so, stop all motors.
     * @details Must be called periodically (e.g. every 10 ms from the UDP task).
     *          The stop is triggered only once per timeout event; it resets
     *          automatically when the next heartbeat arrives.
     */
    void checkHeartbeatTimeout();

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
    bool registerMaster(const std::string &ip);

    /**
     * @brief Internal helper — atomically clear master registration and log to app_info_logger.
     */
    bool unregisterMaster(const std::string &ip);

    /**
     * @brief Send a UDP reply echoing the original message followed by a status byte.
     * @param message Original incoming message (echoed back as-is)
     * @param status  UDPResponseStatus value appended as trailing byte
     * @param remoteIP    Destination IP
     * @param remotePort  Destination port
     */
    void udp_reply(const std::string &message, UDPResponseStatus status,
                   const IPAddress &remoteIP, uint16_t remotePort);

    std::string server_token_;    // Generated once on init, never changes
    std::string master_ip_;       // Current master's IP, empty if none
    std::string bot_name_;        // Bot name (default: "K10-Bot", protected by master_mutex_)
    SemaphoreHandle_t master_mutex_ = nullptr;

    // Heartbeat watchdog state
    volatile unsigned long last_heartbeat_ms_ = 0;  ///< millis() of last valid heartbeat
    volatile bool heartbeat_active_    = false;     ///< true once ≥1 heartbeat received from current master
    volatile bool heartbeat_timed_out_ = false;     ///< true while in timed-out state (avoids log spam)
};
