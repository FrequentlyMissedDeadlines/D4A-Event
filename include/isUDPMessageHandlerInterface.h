#pragma once
#include <string>
#include <IPAddress.h>
#include <cstdint>
#include "IsMasterRegistryInterface.h"

/**
 * @brief Shared binary UDP response codes.
 *        RESPONSE frame: [action:1B][resp_code:1B][optional_payload]
 */
namespace UDPProto
{
    constexpr uint8_t udp_resp_ok               = 0x00;
    constexpr uint8_t udp_resp_invalid_params   = 0x01;
    constexpr uint8_t udp_resp_invalid_values   = 0x02;
    constexpr uint8_t udp_resp_operation_failed = 0x03;
    constexpr uint8_t udp_resp_not_started      = 0x04;
    constexpr uint8_t udp_resp_unknown_cmd      = 0x05;
    constexpr uint8_t udp_resp_not_master       = 0x06; ///< Sender IP is not the registered master
} // namespace UDPProto

/**
 * @brief Interface for services that handle incoming UDP messages.
 *        Message format: "<ServiceName>:<command>:<JSON parameters>"
 *        Example: "Servo Service:setServoAngle:{\"channel\":0,\"angle\":90}"
 */
struct IsUDPMessageHandlerInterface : public virtual IsMasterRegistryInterface
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

protected:
    /**
     * @brief Check whether a UDP request originates from the registered master.
     * @details Compares the sender IP against the master registered in @p masterRegistry.
     *          If the check fails, fills @p errorResponse with the binary error frame
     *          `[action][udp_resp_not_master]` ready to be forwarded to the sender.
     *
     * Typical usage inside messageHandler():
     * @code
     *   std::string resp;
     *   if (!checkUDPIsMaster(action, remoteIP, &amakerbot_service, resp))
     *   {
     *       udp_service.sendReply(resp, remoteIP, remotePort);
     *       return true;   // message claimed — error reply sent
     *   }
     * @endcode
     *
     * @param action         Action byte echoed back as the first byte of the error frame
     * @param remoteIP       Sender IP extracted from the incoming UDP packet
     * @param masterRegistry Pointer to the master registry (e.g. AmakerBotService); if null, check always fails
     * @param errorResponse  Output: filled with `[action][0x06]` when the check fails; unchanged on success
     * @return true  if remoteIP matches the registered master (proceed with command)
     * @return false if remoteIP is NOT the master (errorResponse filled, caller must send and return true)
     */
    bool checkUDPIsMaster(uint8_t action,
                           const IPAddress &remoteIP,
                           const IsMasterRegistryInterface *masterRegistry,
                           std::string &errorResponse)
    {
        std::string ip = remoteIP.toString().c_str();
        if (!masterRegistry || !masterRegistry->isMaster(ip))
        {
            errorResponse.clear();
            errorResponse += static_cast<char>(action);
            errorResponse += static_cast<char>(UDPProto::udp_resp_not_master);
            return false;
        }
        return true;
    }
};