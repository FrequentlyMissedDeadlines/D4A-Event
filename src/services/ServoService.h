// Servo controller handler for DFR0548 board
/**
 * @file ServoService.h
 * @brief Header for servo controller integration with the main application
 * @details
 * - Provides servo initialization, control, and HTTP route registration.
 * - `registerRoutes()` no longer accepts a `WebServer*` argument; services use
 *   the global `WebServer webserver` instance that is created in `main.cpp`.
 *   An `extern WebServer webserver` declaration is provided in
 *   `IsOpenAPIInterface.h` to make the global accessible to services.
 */
#pragma once

#include "../services/IsServiceInterface.h"
#include "../services/IsOpenAPIInterface.h"


enum ServoConnection
{
    NOT_CONNECTED=0,
    ROTATIONAL=1,
    ANGULAR_180=2,
    ANGULAR_270=3
};

class ServoInfo 
{
public:
    ServoConnection connectionStatus;
    int value;
    ServoInfo(ServoConnection connectionStatus, int val) : value(val), connectionStatus(connectionStatus) {}
    void setValue(int newval) { value = newval; }
};

class ServoService : public IsServiceInterface, public IsOpenAPIInterface
{
public:
    IsOpenAPIInterface* asOpenAPIInterface() override { return this; }
    /**
     * @brief Attach a servo model to a channel.
     * @param channel Servo channel (0-7 or 5)
     * @param connection ServoConnection enum value
     * @return true if successful, false otherwise
     */
    bool attachServo(uint8_t channel, ServoConnection connection);

    /**
     * @brief Set servo angle
     * @param channel Servo channel (0-7 or 5)
     * @param angle Angle in degrees (0-180 or 0-270 depending on servo)
     * @return true if successful, false otherwise
     */
    bool setServoAngle(uint8_t channel, uint16_t angle);
    /**
     * @brief Set servo speed for continuous rotation
     * @param channel Servo channel (0-7 or 5)
     * @param speed Speed value (-100 to +100)
     * @return true if successful, false otherwise
     */
    bool setServoSpeed(uint8_t channel, int8_t speed);

    /**
     * @brief Set all attached continuous servos to the same speed
     * @param speed Speed value (-100 to +100) 
     * @return true if successful, false otherwise
     */
    bool setAllServoSpeed(int8_t speed);

    /**
     * 
     * @brief Set all attachedangular servos to middle position
     * @return true if successful, false otherwise
     */
    bool setAllServoAngle(u_int16_t angle);
    
    // Get servo status
    /**
     * @brief Get the status of a servo channel
     * @param channel Servo channel (0-7 or 5)
     * @return Json string 
     */
    std::string getAttachedServo(uint8_t channel);
    /**
     * @brief Get the status of all servo channels
     * @return Json string
     */
    std::string getAllAttachedServos();

    /**
     * @brief Register HTTP routes for servo control.
     *
     * Notes:
     * - Uses the global `webserver` instance (see `IsOpenAPIInterface.h`).
     * - Lambdas registered with `webserver.on()` should capture only `this`.
     *
     * @return true if registration was successful, false otherwise.
     */
    bool registerRoutes() override;
    std::string getServiceSubPath() override;

    // Service lifecycle methods (implemented in the .cpp file)
    bool initializeService() override;
    bool startService() override;
    bool stopService() override;
    std::string getServiceName() override;

    bool saveSettings() override;
    bool loadSettings() override;

private:
    ServiceStatus service_status_ = STOP_FAILED;
    unsigned long status_timestamp_ = 0;
};
