// DFR1216 (Unihiker Expansion) Service
/**
 * @file DFR1216Service.h
 * @brief Header for DFR1216 expansion board integration with the main application
 * @details
 * - Provides DFR1216 initialization, control, and HTTP route registration.
 * - Implements IsServiceInterface and IsOpenAPIInterface
 * - Uses the global `WebServer webserver` instance that is created in `main.cpp`.
 */
#pragma once

#include "../services/IsServiceInterface.h"
#include "../services/IsOpenAPIInterface.h"
#include "../DFR1216/DFRobot_UnihikerExpansion.h"

class DFR1216Service : public IsServiceInterface, public IsOpenAPIInterface
{
public:
    /**
     * @brief Returns this service's OpenAPI interface
     * @return Pointer to the IsOpenAPIInterface implementation (this object)
     */
    IsOpenAPIInterface* asOpenAPIInterface() override { return this; }

    /**
     * @fn: getName
     * @brief: Get the name of the service.
     * @return: Service name as a string.
     */
    std::string getName() override { return "DFR1216/v1"; }

    /**
     * @fn: getPath
     * @brief: Construct full API path from service name and final path segment
     * @param finalpathstring: The final path segment to append
     * @return: Full path in format /api/<servicename>/<finalpathstring>
     */
    std::string getPath(const std::string& finalpathstring) override
    {
        if (baseServicePath.empty()) {
            // Cache base path on first call
            std::string serviceName = getName();
            size_t slashPos = serviceName.find('/');
            if (slashPos != std::string::npos) {
                serviceName = serviceName.substr(0, slashPos);
            }
            baseServicePath = std::string(RoutesConsts::kPathAPI) + serviceName + "/";
        }
        return baseServicePath + finalpathstring;
    }

    /**
     * @fn: initializeService
     * @brief: Initialize the DFR1216 expansion board.
     * @return: true if initialization was successful, false otherwise.
     */
    bool initializeService() override;

    /**
     * @fn: startService
     * @brief: Start the DFR1216 service.
     * @return: true if the service started successfully, false otherwise.
     */
    bool startService() override;

    /**
     * @fn: stopService
     * @brief: Stop the DFR1216 service.
     * @return: true if the service stopped successfully, false otherwise.
     */
    bool stopService() override;

    /**
     * @fn: registerRoutes
     * @brief: Register HTTP routes for DFR1216 control.
     * @details Uses the global `webserver` instance (see `IsOpenAPIInterface.h`).
     * @return: true if route registration was successful, false otherwise.
     */
    bool registerRoutes() override;

    /**
     * @brief Set servo angle
     * @param channel Servo channel (0-5)
     * @param angle Angle in degrees (0-180)
     * @return true if successful, false otherwise
     */
    bool setServoAngle(uint8_t channel, uint16_t angle);

    /**
     * @brief Set motor speed
     * @param motor Motor number (1-4)
     * @param speed Speed value (-100 to +100)
     * @return true if successful, false otherwise
     */
    bool setMotorSpeed(uint8_t motor, int8_t speed);

    /**
     * @brief Get the status of the DFR1216 controller
     * @return JSON string with status information
     */
    std::string getStatus();

private:
    bool initialized = false;
    DFRobot_UnihikerExpansion_I2C controller;
    mutable std::string baseServicePath;  // Cached for optimization (mutable for const method)

    // HTTP handler methods
    void handleSetServoAngle();
    void handleSetMotorSpeed();
    void handleGetStatus();
};
