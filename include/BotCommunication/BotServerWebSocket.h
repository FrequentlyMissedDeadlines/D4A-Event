/**
 * @file BotServerWebSocket.h
 * @brief Standalone WebSocket server that feeds binary bot protocol frames
 *        into BotMessageHandler.
 *
 * @details
 * Runs its own `AsyncWebServer` on a dedicated port (default 81) with an
 * `AsyncWebSocket` endpoint at `/ws`.  On every complete binary frame:
 *   1. Calls `BotMessageHandler::dispatch()`
 *   2. Sends the returned response back to **the same client** (if non-empty)
 *
 * Because `dispatch()` returns the response synchronously, no global context
 * variables or cross-service routing tricks are needed — the reply is sent
 * directly inside the WebSocket event callback.
 *
 * ## Lifecycle
 * @code
 *   BotServerWebSocket ws_server(bot_handler);
 *   ws_server.setBotMessageLogger(&debug_logger);
 *   ws_server.start();                // call once in setup()
 *
 *   // In the FreeRTOS display/main task, call periodically:
 *   ws_server.cleanupClients();       // frees TCP resources of stale clients
 * @endcode
 *
 * ## Fragmentation
 * Only complete, single-frame messages are dispatched.  Fragmented WebSocket
 * frames are silently dropped; binary bot messages are small enough that
 * fragmentation should never occur in practice.
 *
 * ## Thread safety
 * start() / stop() must be called from setup() (single-threaded).
 * sendReply() and the event callback run in the AsyncTCP task; they access
 * only the AsyncWebSocket handle, which is designed for that context.
 * cleanupClients() is safe to call from any task.
 */

#pragma once

#include <cstdint>
#include <string>
#include <ESPAsyncWebServer.h>
#include <pgmspace.h>
#include "services/AmakerBotService.h"

class RollingLogger;

// ---------------------------------------------------------------------------
// PROGMEM string constants
// ---------------------------------------------------------------------------
namespace BotServerWSConsts
{
    constexpr const char str_service_name[]  PROGMEM = "BotServerWebSocket";
    constexpr const char ws_path[]           PROGMEM = "/ws";
    constexpr const char msg_start_ok[]      PROGMEM = "BotServerWebSocket listening on port ";
    constexpr const char msg_stop[]          PROGMEM = "BotServerWebSocket stopped";
    constexpr const char msg_no_alloc[]      PROGMEM = "BotServerWebSocket: allocation failed";
    constexpr const char msg_client_conn[]   PROGMEM = "WS client connected: ";
    constexpr const char msg_client_disc[]   PROGMEM = "WS client disconnected: ";
    constexpr const char msg_client_err[]    PROGMEM = "WS client error on id: ";
    constexpr const char msg_frag_dropped[]  PROGMEM = "WS fragmented frame dropped";
    constexpr uint16_t   default_port        = 81;
} // namespace BotServerWSConsts

// ---------------------------------------------------------------------------
// BotServerWebSocket
// ---------------------------------------------------------------------------

/**
 * @brief Thin WebSocket transport layer for the binary bot protocol.
 *
 * Owns one `AsyncWebServer` + one `AsyncWebSocket` created at start().
 * Each complete binary frame received from any connected client is forwarded
 * to `BotMessageHandler::dispatch()`; the reply (if any) is sent back to
 * that client only.
 */
class BotServerWebSocket
{
public:
    // ---- Construction ----

    /**
     * @brief Construct a BotServerWebSocket.
     * @param handler Central message dispatcher — must outlive this object.
     * @param port    WebSocket server listen port (default 81).
     */
    explicit BotServerWebSocket(AmakerBotService &bot,
                                uint16_t port = BotServerWSConsts::default_port);

    // Not copyable — owns heap-allocated server objects
    BotServerWebSocket(const BotServerWebSocket &)            = delete;
    BotServerWebSocket &operator=(const BotServerWebSocket &) = delete;

    ~BotServerWebSocket() { stop(); }

    // ---- Configuration (call before start()) ----

    /**
     * @brief Attach a logger for info / error output.
     * @param log May be nullptr to disable logging.
     */
    void setBotMessageLogger(RollingLogger *log) { logger_ = log; }

    /**
     * @brief Override the listen port.
     * @note Has no effect after start() has been called.
     */
    void setPort(uint16_t port) { port_ = port; }

    /** @brief Return the configured listen port. */
    uint16_t getPort() const { return port_; }

    // ---- Lifecycle ----

    /**
     * @brief Allocate the AsyncWebServer + AsyncWebSocket and start listening.
     * @return true  on success
     * @return false if allocation failed
     */
    bool start();

    /**
     * @brief Close all WebSocket connections and free resources.
     * Safe to call even if start() was never called.
     */
    void stop();

    /** @brief Return true if the server is currently running. */
    bool isRunning() const { return ws_ != nullptr; }

    // ---- Transport ----

    /**
     * @brief Send a binary reply to a specific connected client.
     *
     * Called automatically by the event callback when dispatch() returns a
     * non-empty response.  Also callable directly for unsolicited pushes.
     *
     * @param client_id  AsyncWebSocketClient ID
     * @param data       Binary payload
     * @return true if the message was queued successfully
     */
    bool sendReply(uint32_t client_id, const std::string &data);

    /**
     * @brief Broadcast a binary frame to all connected clients.
     * @param data Binary payload
     */
    void broadcast(const std::string &data);

    /**
     * @brief Release resources held by stale / disconnected clients.
     *
     * Should be called periodically (e.g. every ~1 s from the display task)
     * to prevent TCP resource exhaustion when browsers open multiple tabs.
     */
    void cleanupClients();

    // ---- Diagnostics ----

    /** @brief Number of complete frames received since start(). */
    uint32_t getRxCount()      const { return rx_count_; }
    /** @brief Number of reply frames sent since start(). */
    uint32_t getTxCount()      const { return tx_count_; }
    /** @brief Number of frames dropped (fragmented or zero-length). */
    uint32_t getDroppedCount() const { return dropped_count_; }

private:
    AmakerBotService &bot_;
    RollingLogger      *logger_        = nullptr;
    uint16_t            port_;
    AsyncWebServer     *server_        = nullptr;
    AsyncWebSocket     *ws_            = nullptr;

    volatile uint32_t   rx_count_      = 0;
    volatile uint32_t   tx_count_      = 0;
    volatile uint32_t   dropped_count_ = 0;

    /**
     * @brief Attach the onEvent lambda to ws_.
     * Split out of start() to keep it readable.
     */
    void attachEventHandler();
};
