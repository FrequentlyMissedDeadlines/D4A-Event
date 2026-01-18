// Servo controller handler for DFR0548 board
/**
 * @file ServoService.h
 * @brief Header for servo controller integration with the main application
 * @details Handles servo initialization, control, and HTTP route registration.
 */
#pragma once

#include "../services/HasRoutesInterface.h"
#include "../services/IsServiceInterface.h"
#include "../services/HasLoggerInterface.h"
// Global servo controller instance

class ServoService : public HasRoutesInterface, public IsServiceInterface, public HasLoggerInterface
{
public:
    bool init() override;
    bool start() override;
    bool stop() override; 

    // Set servo angle (wrapper for easier integration)
     bool setAngle(uint8_t channel, uint16_t angle);

    // Set servo speed for continuous rotation
     bool setSpeed(uint8_t channel, int8_t speed);

    // Stop servo
     bool stopServo(uint8_t channel);

    // Get servo status
     std::string getStatus(uint8_t channel);



    bool registerRoutes(WebServer *server, std::string basePath) override;
};
