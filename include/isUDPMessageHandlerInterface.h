#include <string>
#include <IPAddress.h>

/**
 * @brief Interface for services that handle incoming UDP messages.
 *        Message format: "<ServiceName>:<command>:<JSON parameters>"
 *        Example: "Servo Service:setServoAngle:{\"channel\":0,\"angle\":90}"
 */
struct IsUDPMessageHandlerInterface
{
    virtual ~IsUDPMessageHandlerInterface() = default;

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
     * @return this as IsUDPMessageHandlerInterface*
     */
    virtual IsUDPMessageHandlerInterface *asUDPMessageHandlerInterface() { return this; }
};