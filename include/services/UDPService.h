// UDP handler module - non-blocking packet callback and message buffer
#pragma once

#include <AsyncUDP.h>
#include <ESPAsyncWebServer.h>
#include <set>
#include <string>
#include <vector>
#include <functional>
#include <Arduino.h>
#include "IsOpenAPIInterface.h"
#include "IsServiceInterface.h"

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
 * Inherits from withRoutes to register HTTP routes with a AsyncWebServer instance.
 * Supports callback registration for message handling by other classes.
 *
 */
/**
 * @enum UDPResponseStatus
 * @brief Standard status byte appended to UDP reply payloads.
 * @details Sent as the last byte of every reply: [echo of request][status].
 *   - SUCCESS : command executed successfully
 *   - IGNORED : message was valid but had no effect (e.g. no-op, already in desired state)
 *   - DENIED  : request refused (e.g. caller is not the master, wrong token)
 *   - ERROR   : command was understood but failed internally
 */
enum class UDPResponseStatus : uint8_t {
    SUCCESS = 0x01, ///< Command executed successfully
    IGNORED = 0x02, ///< Valid command, no action taken
    DENIED  = 0x03, ///< Request refused (auth / precondition failed)
    ERROR   = 0x04, ///< Internal failure
};

class UDPService : public IsOpenAPIInterface
{
public:
    /**
     * @brief Per-action-code UDP statistics entry
     */
    struct UDPActionStat {
        uint8_t  action_code = 0; ///< 0x00 = empty slot
        uint32_t accepted    = 0; ///< replies with resp_ok
        uint32_t rejected    = 0; ///< replies with any non-ok code
    };
    static constexpr uint8_t UDP_MAX_ACTION_STATS = 16;

    bool registerRoutes() override;
    std::string getServiceSubPath() override;
    bool initializeService() override;
    bool startService() override;
    bool stopService() override;
    std::string getServiceName() override;


    bool begin(AsyncUDP *udpInstance = nullptr, int listenPort = 0);
    int getPort() const { return port; }

    /**
     * @brief Send a UDP reply to a specific address and port.
     *        Called by message handlers to respond to senders.
     * @param message    Response string to send
     * @param remoteIP   Target IP address
     * @param remotePort Target port
     * @return true if the packet was sent successfully
     */
    bool sendReply(const std::string &message, const IPAddress &remoteIP, uint16_t remotePort);

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

    /**
     * @brief Copy per-action-code stats into caller-supplied array.
     * @param out       Array of at least max_count entries to fill.
     * @param max_count Maximum entries to copy.
     * @return Number of entries actually filled (active action codes seen so far).
     */
    uint8_t getActionStats(UDPActionStat out[], uint8_t max_count) const;

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

    UDPActionStat action_stats_[UDP_MAX_ACTION_STATS] = {};
    /**
     * @brief Record one reply's outcome into action_stats_.
     * @param action_byte First byte of the binary reply frame (action code).
     * @param ok          true when the second byte equals udp_resp_ok.
     */
    void recordActionResult(uint8_t action_byte, bool ok);

    friend void handleUDPPacket(AsyncUDPPacket packet);
};