#include "../remotecontrol/RemoteControlService.h"
#include "../FlashStringHelper.h"

/**
 * @file RemoteControlService.cpp
 * @brief Implementation of remote control service
 * @details Handles UDP commands for robot movement control
 */

RemoteControlService::RemoteControlService(UDPService* udpService, ServoService* servoService)
    : udp_service(udpService), servo_service(servoService), handler_id(-1)
{
}

RemoteControlService::~RemoteControlService()
{
    if (handler_id >= 0 && udp_service)
    {
        udp_service->unregisterMessageHandler(handler_id);
    }
}

bool RemoteControlService::initializeService()
{
    if (!udp_service || !servo_service)
    {
        logger->error(getServiceName() + fpstr_to_string(FPSTR(RemoteControlConsts::msg_missing_services)));
        return false;
    }

    #ifdef VERBOSE_DEBUG
    logger->debug(getServiceName() + " " +getStatusString());   
    #endif
    return true;
}

bool RemoteControlService::startService()
{
    // Register message handler with UDP service
    handler_id = udp_service->registerMessageHandler(
        [this](const std::string& msg, const IPAddress& ip, uint16_t port) {
            return this->handleMessage(msg, ip, port);
        }
    );

    if (handler_id < 0)
    {
        logger->error(getServiceName() + fpstr_to_string(FPSTR(RemoteControlConsts::msg_failed_register_handler)));
        return false;
    }

    #ifdef VERBOSE_DEBUG
    logger->debug(getServiceName() + " " + getStatusString());   
    #endif
    return true;
}

bool RemoteControlService::stopService()
{
    if (handler_id >= 0 && udp_service)
    {
        udp_service->unregisterMessageHandler(handler_id);
        handler_id = -1;
    }

    #ifdef VERBOSE_DEBUG
    logger->debug(getServiceName() + " " + getStatusString());   
    #endif
    return true;
}

std::string RemoteControlService::getServiceName()
{
    return fpstr_to_string(FPSTR(RemoteControlConsts::str_service_name));
}

bool RemoteControlService::handleMessage(const std::string& message, const IPAddress& remoteIP, uint16_t remotePort)
{
    // Trim whitespace and convert to lowercase for comparison
    std::string cmd = message;
    
    // Remove leading/trailing whitespace
    std::string whitespace = fpstr_to_string(FPSTR(RemoteControlConsts::str_whitespace));
    size_t start = cmd.find_first_not_of(whitespace);
    size_t end = cmd.find_last_not_of(whitespace);
    if (start != std::string::npos && end != std::string::npos)
    {
        cmd = cmd.substr(start, end - start + 1);
    }

    // Convert to lowercase
    for (char& c : cmd)
    {
        c = tolower(c);
    }

    // Check if this is a remote control command
    if (cmd == fpstr_to_string(FPSTR(RemoteControlConsts::cmd_forward)))
    {
        executeForward();
        #ifdef VERBOSE_DEBUG
        logger->debug(fpstr_to_string(FPSTR(RemoteControlConsts::msg_remote_forward)) + std::string(remoteIP.toString().c_str()));
        #endif
        return true;
    }
    else if (cmd == fpstr_to_string(FPSTR(RemoteControlConsts::cmd_backward)))
    {
        executeBackward();
        #ifdef VERBOSE_DEBUG
        logger->debug(fpstr_to_string(FPSTR(RemoteControlConsts::msg_remote_backward)) + std::string(remoteIP.toString().c_str()));
        #endif
        return true;
    }
    else if (cmd == fpstr_to_string(FPSTR(RemoteControlConsts::cmd_turn_left)))
    {
        executeTurnLeft();
        #ifdef VERBOSE_DEBUG
        logger->debug(fpstr_to_string(FPSTR(RemoteControlConsts::msg_remote_turn_left)) + std::string(remoteIP.toString().c_str()));
        #endif
        return true;
    }
    else if (cmd == fpstr_to_string(FPSTR(RemoteControlConsts::cmd_turn_right)))
    {
        executeTurnRight();
        #ifdef VERBOSE_DEBUG
        logger->debug(fpstr_to_string(FPSTR(RemoteControlConsts::msg_remote_turn_right)) + std::string(remoteIP.toString().c_str()));
        #endif
        return true;
    }
    else if (cmd == fpstr_to_string(FPSTR(RemoteControlConsts::cmd_stop)))
    {
        executeStop();
        #ifdef VERBOSE_DEBUG
        logger->debug(fpstr_to_string(FPSTR(RemoteControlConsts::msg_remote_stop)) + std::string(remoteIP.toString().c_str()));
        #endif
        return true;
    }

    // Not a remote control command
    return false;
}

void RemoteControlService::executeForward()
{
    if (!servo_service)
    {
        return;
    }
    // TODO: Implement forward movement using servo_service
    // Example: Set both servos to move forward at medium speed
    // servo_service->setServoAngle(left_servo_id, forward_angle);
    // servo_service->setServoAngle(right_servo_id, forward_angle);
}

void RemoteControlService::executeBackward()
{
    if (!servo_service)
    {
        return;
    }
    // TODO: Implement backward movement using servo_service
}

void RemoteControlService::executeTurnLeft()
{
    if (!servo_service)
    {
        return;
    }
    // TODO: Implement left turn using servo_service
    // Example: Left servo slower, right servo faster
}

void RemoteControlService::executeTurnRight()
{
    if (!servo_service)
    {
        return;
    }
    // TODO: Implement right turn using servo_service
}

void RemoteControlService::executeStop()
{
    if (!servo_service)
    {
        return;
    }
    // TODO: Implement stop using servo_service
    // Example: Set both servos to neutral position
}
