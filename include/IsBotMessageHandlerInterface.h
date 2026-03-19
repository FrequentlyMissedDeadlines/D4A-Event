#pragma once
#include <string>
#include <IPAddress.h>
#include <cstdint>

// Forward declaration — include RollingLogger.h in translation units that call setLogger()
class RollingLogger;

/**
 * @brief Shared binary UDP response codes.
 *        RESPONSE frame: [action:1B][resp_code:1B][optional_payload]
 */
namespace BotMessageProto
{
    constexpr uint8_t udp_resp_ok               = 0x00;
    constexpr uint8_t udp_resp_invalid_params   = 0x01;
    constexpr uint8_t udp_resp_invalid_values   = 0x02;
    constexpr uint8_t udp_resp_operation_failed = 0x03;
    constexpr uint8_t udp_resp_not_started      = 0x04;
    constexpr uint8_t udp_resp_unknown_cmd      = 0x05;
    constexpr uint8_t udp_resp_not_master       = 0x06; ///< Sender IP is not the registered master
} // namespace BotMessageProto

/**
 * @brief Interface for services that handle incoming messages (UDP or WebSocket).
 *        Message format: "<ServiceName>:<command>:<JSON parameters>"
 *        Example: "Servo Service:setServoAngle:{\"channel\":0,\"angle\":90}"
 */
struct IsBotMessageHandlerInterface 
{
    virtual ~IsBotMessageHandlerInterface() = default;

    /**
     * @brief Attach a logger for debug / error output.
     * @param log Pointer to a RollingLogger instance (may be nullptr to disable logging).
     */
    void setBotMessageLogger(RollingLogger *logger) { message_logger = logger; }

    /**
     * @brief Handle an incoming UDP message.
     * @param message    Raw UDP message string
     * @param remoteIP   Sender IP address (for reply)
     * @param remotePort Sender port (for reply)
     * @return true if message was claimed by this service, false to pass to next handler
     */
    virtual bool messageHandler(const std::string &message,
                                const IPAddress &remoteIP,
                                uint16_t remotePort) = 0;

    /**
     * @brief Downcast helper — avoids dynamic_cast per project rules.
     * @return this as IsBotMessageHandlerInterface*
     */
    virtual IsBotMessageHandlerInterface *asotMessageHandlerInterface() { return this; }
    
    char getServiceBytePrefix(); ///< Each service that handles UDP messages must return a unique byte prefix (e.g., 'S' for Servo Service) to identify its messages.
    
        
protected:
    /// Logger instance — set via setLogger(); may be nullptr.
    RollingLogger *message_logger = nullptr;

};