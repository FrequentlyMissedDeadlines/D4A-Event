/**
 * @file BotServerUDP.h
 * @brief UDP server that feeds binary bot protocol frames into BotMessageHandler.
 *
 * @details
 * Responsibilities:
 *   - Listen on a UDP port
 *   - On each incoming packet, call BotMessageHandler::dispatch()
 *   - Send the returned response back to the sender (if non-empty)
 *
 * Everything else — protocol decoding, service routing, master checks — is
 * handled by BotMessageHandler and the individual service handlers.
 * BotServerUDP has no knowledge of the protocol content.
 *
 * ## Lifecycle
 * @code
 *   BotServerUDP udp_server(bot_handler);
 *   udp_server.setBotMessageLogger(&debug_logger);
 *   udp_server.setPort(24642);          // optional, default is 24642
 *   udp_server.start();
 *
 *   // ... FreeRTOS tasks run, packets arrive, dispatch() is called ...
 *
 *   udp_server.stop();
 * @endcode
 *
 * ## Thread safety
 * start() / stop() must be called from a single thread (setup()).
 * sendReply() and the onPacket callback may run on Core 0's AsyncUDP task;
 * they access only immutable config (port_, handler_ reference) and the
 * AsyncUDP handle, which is internally thread-safe for writeTo().
 */

#pragma once

#include <cstdint>
#include <string>
#include <AsyncUDP.h>
#include <pgmspace.h>
#include "services/AmakerBotService.h"

class RollingLogger;

// ---------------------------------------------------------------------------
// PROGMEM string constants
// ---------------------------------------------------------------------------
namespace BotServerUDPConsts
{
    constexpr const char str_service_name[]       PROGMEM = "BotServerUDP";
    constexpr const char msg_start_ok[]            PROGMEM = "BotServerUDP listening on port ";
    constexpr const char msg_start_failed[]        PROGMEM = "BotServerUDP failed to listen on port ";
    constexpr const char msg_stop[]                PROGMEM = "BotServerUDP stopped";
    constexpr const char msg_no_udp[]              PROGMEM = "BotServerUDP: AsyncUDP alloc failed";
    constexpr uint16_t   default_port              = 24642;
} // namespace BotServerUDPConsts

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
// ---------------------------------------------------------------------------
// BotServerUDP
// ---------------------------------------------------------------------------

/**
 * @brief Thin UDP transport layer for the binary bot protocol.
 *
 * Owns one AsyncUDP instance created at start() and destroyed at stop().
 * Every received packet is forwarded synchronously to BotMessageHandler::dispatch()
 * in the AsyncUDP callback context (Core 0).
 */
class BotServerUDP
{
public:
    // ---- Construction ----

    /**
     * @brief Construct a BotServerUDP.
     * @param bot Reference to the AmakerBotService — must outlive this object.
     * @param port UDP listen port (default 24642).
     */
    explicit BotServerUDP(AmakerBotService &bot,
                          uint16_t port = BotServerUDPConsts::default_port);

    // Not copyable — owns an AsyncUDP handle
    BotServerUDP(const BotServerUDP &)            = delete;
    BotServerUDP &operator=(const BotServerUDP &) = delete;

    ~BotServerUDP() { stop(); }

    // ---- Configuration (call before start()) ----

    /**
     * @brief Attach a logger for info / error output.
     * @param log May be nullptr to disable logging.
     */
    void setBotMessageLogger(RollingLogger *log) { logger_ = log; }

    /**
     * @brief Override the UDP listen port.
     * @note Has no effect after start() has been called.
     */
    void setPort(uint16_t port) { port_ = port; }

    /** @brief Return the configured listen port. */
    uint16_t getPort() const { return port_; }

    // ---- Lifecycle ----

    /**
     * @brief Allocate the AsyncUDP instance and start listening.
     * @return true  on success
     * @return false if AsyncUDP allocation or listen() failed
     */
    bool start();

    /**
     * @brief Close the UDP socket and free the AsyncUDP instance.
     * Safe to call even if start() was never called or already failed.
     */
    void stop();

    /** @brief Return true if the server is currently listening. */
    bool isRunning() const { return udp_ != nullptr; }

    // ---- Transport ----

    /**
     * @brief Send a binary reply to a specific remote endpoint.
     *
     * Called automatically by the onPacket callback when dispatch() returns
     * a non-empty response.  Also callable directly for unsolicited messages.
     *
     * @param data       Binary payload to send
     * @param remote_ip  Destination IP
     * @param remote_port Destination port
     * @return true if all bytes were written successfully
     */
    bool sendReply(const std::string  &data,
                   const IPAddress    &remote_ip,
                   uint16_t            remote_port);

    // ---- Diagnostics ----

    /** @brief Number of packets received since start(). */
    uint32_t getRxCount()      const { return rx_count_; }
    /** @brief Number of reply packets sent since start(). */
    uint32_t getTxCount()      const { return tx_count_; }
    /** @brief Number of packets dropped (zero-length or dispatch error). */
    uint32_t getDroppedCount() const { return dropped_count_; }

private:
    AmakerBotService &bot_;
    RollingLogger     *logger_        = nullptr;
    uint16_t           port_;
    AsyncUDP          *udp_           = nullptr;

    // Counters — written only from the AsyncUDP callback (single-threaded
    // within that context), read from any task for diagnostics.
    volatile uint32_t  rx_count_      = 0;
    volatile uint32_t  tx_count_      = 0;
    volatile uint32_t  dropped_count_ = 0;
};
