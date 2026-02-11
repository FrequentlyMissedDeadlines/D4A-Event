// Remote control service - handles UDP commands for robot movement
#pragma once

#include "../udp/UDPService.h"
#include "../servo/ServoService.h"
#include "../IsServiceInterface.h"
#include <Arduino.h>
#include <string>

/**
 * @file RemoteControlService.h
 * @brief Remote control service for handling UDP movement commands
 * @details Registers with UDPService to receive commands like "forward", "turn_left", etc.
 *          and translates them into servo control actions.
 */

namespace RemoteControlConsts
{
    constexpr const char str_service_name[] PROGMEM = "Remote Control";
    constexpr const char cmd_up[] PROGMEM = "up";
    constexpr const char cmd_down[] PROGMEM = "down";
    constexpr const char cmd_left[] PROGMEM = "left";
    constexpr const char cmd_right[] PROGMEM = "right";
    constexpr const char cmd_circle[] PROGMEM = "circle";
    constexpr const char cmd_square[] PROGMEM = "square";
    constexpr const char cmd_triangle[] PROGMEM = "triangle";
    constexpr const char cmd_cross[] PROGMEM = "cross";
    constexpr const char cmd_forward[] PROGMEM = "forward";
    constexpr const char cmd_backward[] PROGMEM = "backward";
    constexpr const char cmd_turn_left[] PROGMEM = "turn_left";
    constexpr const char cmd_turn_right[] PROGMEM = "turn_right";
    constexpr const char cmd_stop[] PROGMEM = "stop";
    
    // Error and debug messages
    constexpr const char msg_missing_services[] PROGMEM = ": missing required services";
    constexpr const char msg_failed_register_handler[] PROGMEM = ": failed to register UDP handler";
    constexpr const char msg_remote_forward[] PROGMEM = "Remote control: forward command from ";
    constexpr const char msg_remote_backward[] PROGMEM = "Remote control: backward command from ";
    constexpr const char msg_remote_turn_left[] PROGMEM = "Remote control: turn_left command from ";
    constexpr const char msg_remote_turn_right[] PROGMEM = "Remote control: turn_right command from ";
    constexpr const char msg_remote_stop[] PROGMEM = "Remote control: stop command from ";
    constexpr const char str_whitespace[] PROGMEM = " \t\r\n";
}

class RemoteControlService : public IsServiceInterface
{
public:
    /**
     * @brief Constructor
     * @param udpService Pointer to the UDP service to register with
     * @param servoService Pointer to the servo service for motor control
     */
    RemoteControlService(UDPService* udpService, ServoService* servoService);
    
    /**
     * @brief Destructor - unregisters from UDP service
     */
    ~RemoteControlService();

    bool initializeService() override;
    bool startService() override;
    bool stopService() override;
    std::string getServiceName() override;


private:

    UDPService* udp_service;
    ServoService* servo_service;
    int handler_id;

    /**
     * @brief Handle incoming UDP messages for remote control
     * @param message The received message
     * @param remoteIP IP address of sender
     * @param remotePort Port of sender
     * @return true if message was handled
     */
    bool handleMessage(const std::string& message, const IPAddress& remoteIP, uint16_t remotePort);

    /**
     * @brief Execute forward movement
     */
    void executeForward();

    /**
     * @brief Execute backward movement
     */
    void executeBackward();

    /**
     * @brief Execute left turn
     */
    void executeTurnLeft();

    /**
     * @brief Execute right turn
     */
    void executeTurnRight();

    /**
     * @brief Stop all movement
     */
    void executeStop();
};
