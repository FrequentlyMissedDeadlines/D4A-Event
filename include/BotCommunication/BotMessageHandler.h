/**
 * @file BotMessageHandler.h
 * @brief Central dispatcher for binary bot protocol messages.
 *
 * @details
 * Defines two types:
 *
 *   1. **IsBotActionHandlerInterface** — pure-virtual interface that any
 *      service must implement to receive binary bot messages.
 *
 *   2. **BotMessageHandler** — registry + router that holds one
 *      IsBotActionHandlerInterface* per service_id slot and routes every
 *      incoming binary frame to the correct handler.
 *
 * ## Binary protocol layout
 *
 * | Byte offset | Content                                        |
 * |-------------|------------------------------------------------|
 * | 0           | action = `(service_id << 4) \| cmd_id`         |
 * | 1 … N       | payload — service-defined, may be empty        |
 *
 * ## Response layout (produced by each service handler)
 *
 * | Byte offset | Content                                        |
 * |-------------|------------------------------------------------|
 * | 0           | echoed action byte                             |
 * | 1           | BotProto::resp_* status code                   |
 * | 2 … N       | optional payload — service-defined             |
 *
 * An empty response string signals "no reply needed".
 *
 * ## Usage
 *

 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <pgmspace.h>
#include <freertos/FreeRTOS.h>

// Forward declaration — include RollingLogger.h in .cpp files that call setBotMessageLogger()
class RollingLogger;

// ---------------------------------------------------------------------------
// Binary protocol constants
// ---------------------------------------------------------------------------

/**
 * @namespace BotProto
 * @brief Transport-agnostic binary protocol helpers for the bot message system.
 *
 * All BotServer* classes (UDP, WebSocket, Web) and all service handlers share
 * these constants to ensure consistent framing and error reporting.
 */
namespace BotProto
{
    // ---- Response codes (byte[1] of every reply frame) ----
    constexpr uint8_t resp_ok               = 0x00; ///< Command executed successfully
    constexpr uint8_t resp_invalid_params   = 0x01; ///< Missing or malformed parameters
    constexpr uint8_t resp_invalid_values   = 0x02; ///< Parameters present but out of range
    constexpr uint8_t resp_operation_failed = 0x03; ///< Command understood, hardware/logic failed
    constexpr uint8_t resp_not_started      = 0x04; ///< Target service is not yet running
    constexpr uint8_t resp_unknown_service  = 0x05; ///< No handler registered for this service_id
    constexpr uint8_t resp_unknown_cmd      = 0x06; ///< No handler registered for this command
    constexpr uint8_t resp_not_master       = 0x07; ///< Sender is not the registered master

    // ---- Action-byte helpers ----

    /**
     * @brief Extract the service_id (high nibble) from an action byte.
     * @param action_byte First byte of a bot message
     * @return uint8_t Service identifier (0–15)
     */
    constexpr uint8_t service_id(uint8_t action_byte) { return action_byte >> 4; }

    /**
     * @brief Extract the command (low nibble) from an action byte.
     * @param action_byte First byte of a bot message
     * @return uint8_t Command identifier (0–15)
     */
    constexpr uint8_t command(uint8_t action_byte) { return action_byte & 0x0F; }

    /**
     * @brief Build an action byte from a service_id and command.
     * @param svc_id Service identifier (0–15)
     * @param cmd    Command identifier (0–15)
     * @return uint8_t Encoded action byte
     */
    constexpr uint8_t make_action(uint8_t svc_id, uint8_t cmd)
    {
        return static_cast<uint8_t>((svc_id << 4) | (cmd & 0x0F));
    }

    /**
     * @brief Build a minimal two-byte acknowledgement frame: [action][resp_code].
     *        Works for both success (resp_ok) and error codes.
     * @param action_byte The action byte to echo in the reply
     * @param resp_code   BotProto::resp_* status code
     * @return std::string Two-byte binary response
     */
    inline std::string make_ack(uint8_t action_byte, uint8_t resp_code)
    {
        std::string frame;
        frame += static_cast<char>(action_byte);
        frame += static_cast<char>(resp_code);
        return frame;
    }
} // namespace BotProto

// ---------------------------------------------------------------------------
// String constants (PROGMEM)
// ---------------------------------------------------------------------------

namespace BotMessageHandlerConsts
{
    constexpr const char str_service_name[]      PROGMEM = "BotMessageHandler";
    constexpr const char msg_null_handler[]       PROGMEM = "BotMessageHandler: null handler rejected";
    constexpr const char msg_id_out_of_range[]    PROGMEM = "BotMessageHandler: service_id out of range: ";
    constexpr const char msg_slot_occupied[]      PROGMEM = "BotMessageHandler: slot already occupied for service_id: ";
    constexpr const char msg_handler_registered[] PROGMEM = "BotMessageHandler: handler registered for service_id: ";
    constexpr const char msg_no_handler[]         PROGMEM = "BotMessageHandler: no handler for service_id: ";
    constexpr const char msg_empty_message[]      PROGMEM = "BotMessageHandler: processMessage called with empty message";
} // namespace BotMessageHandlerConsts

// ---------------------------------------------------------------------------
// IsBotActionHandlerInterface
// ---------------------------------------------------------------------------

/**
 * @brief Pure-virtual interface for services that process binary bot messages.
 *
 * @details
 * Example action byte mapping for service_id == 0x02 (servo):
 *   0x20 — attach servo
 *   0x21 — set angle
 *   0x22 — set speed
 *   ...
 *
 * @note handleBotMessage() is called with the full frame starting at byte[0]
 *       (the action byte) so that the implementation can verify the service_id
 *       and extract the command itself.
 */
struct IsBotActionHandlerInterface
{
    virtual ~IsBotActionHandlerInterface() = default;

    /**
     * @brief Return the service identifier (0–15).
     *
     * This value is encoded in the high nibble of every action byte addressed
     * to this service.  Must be unique across all registered handlers.
     *
     * @return uint8_t Service identifier
     */
    virtual uint8_t getBotServiceId() const = 0;

    /**
     * @brief Handle a binary bot message frame.
     *
     * @details
     * Called by BotMessageHandler::processMessage() after the service_id has been
     * verified.  The full frame (starting at the action byte) is passed so
     * the implementation has access to the command nibble and all payload bytes.
     *
     * The implementation must:
     *   - Validate the command (low nibble of data[0])
     *   - Parse the payload (data[1..len-1])
     *   - Return a binary response or an empty string if no reply is needed
     *
     * Response format:
     *   byte[0] = echoed action byte (data[0])
     *   byte[1] = BotProto::resp_* status code
     *   byte[2..N] = optional payload
     *
     * @param data Pointer to the raw binary frame (byte[0] is the action byte).
     *             Guaranteed non-null and len ≥ 1 by the caller.
     * @param len  Frame length in bytes
     * @return std::string Binary response — empty means "send no reply"
     */
    virtual std::string handleBotMessage(const uint8_t *data, size_t len) = 0;
};

// ---------------------------------------------------------------------------
// BotMessageHandler
// ---------------------------------------------------------------------------

/**
 * @brief Central registry and router for binary bot protocol messages.
 * 
 */
class BotMessageHandler
{
public:
    /// Maximum number of concurrently registered handlers.
    /// Matches the 4-bit service_id space (high nibble of the action byte).
    static constexpr uint8_t MAX_HANDLERS = 16;

    // ---- Lifecycle ----

    /**
     * @brief Attach a logger for debug / error output.
     * @param log Pointer to a RollingLogger instance (may be nullptr to disable logging)
     */
    void setBotMessageLogger(RollingLogger *log) { logger_ = log; }

    // ---- Registration ----

    // ---- Dispatch ----

    /**
     * @brief Route a binary frame to the appropriate service handler.
     *
     * Reads the service_id from the high nibble of data[0] and calls
     * handler->handleBotMessage(data, len).  If no handler is registered,
     * returns a two-byte error frame `[action][resp_unknown_cmd]`.
     *
     * Safe to call concurrently from multiple FreeRTOS tasks.
     *
     * @param data Non-null pointer to raw binary frame
     * @param len  Frame length in bytes (must be ≥ 1)
     * @return std::string Binary response — empty string means "no reply needed"
     */
    std::string processMessage(const uint8_t *data, size_t len);

private:
    IsBotActionHandlerInterface *handlers_[MAX_HANDLERS] = {};
    RollingLogger               *logger_                 = nullptr;
};
