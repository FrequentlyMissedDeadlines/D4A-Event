// Servo controller handler for DFR0548 board
/**
 * @file ServoService.h
 * @brief Header for servo controller integration with the main application
 * @details
 * - Provides servo initialization, control, and HTTP route registration.
 * - `registerRoutes()` no longer accepts a server argument; services use
 *   the global `AsyncWebServer webserver` instance that is created in `main.cpp`.
 *   An `extern AsyncWebServer webserver` declaration is provided in
 *   `IsOpenAPIInterface.h` to make the global accessible to services.
 */
#pragma once

#include "IsOpenAPIInterface.h"
#include "isUDPMessageHandlerInterface.h"

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

class ServoService : public IsOpenAPIInterface, public IsUDPMessageHandlerInterface
{
public:
    struct ServoSpeedOp {
        uint8_t channel;
        int8_t speed;
        uint32_t duration_ms = 0;  ///< Optional auto-stop delay in ms (0 = no timeout)
    };
    struct ServoAngleOp {
        uint8_t channel;
        int16_t angle;
    };

    /**
     * @brief Attach a servo model to a channel.
     * @param channel Servo channel (0-7)
     * @param connection ServoConnection enum value
     * @return true if successful, false otherwise
     */
    bool attachServo(uint8_t channel, ServoConnection connection);

    /**
     * @brief Set servo angle
     * @param channel Servo channel (0-7 or 5)
     * @param angle Angle in degrees (-90 to 90 for 180° or -135 to 135 for 270° servo)
     * @return true if successful, false otherwise
     */
    bool setServoAngle(uint8_t channel, int16_t angle);
    /**
     * @brief Set servo speed for continuous rotation
     * @param channel      Servo channel (0-7)
     * @param speed        Speed value (-100 to +100)
     * @param duration_ms  Optional auto-stop delay in ms. If > 0 and speed != 0, the servo
     *                     is automatically stopped after this many milliseconds. 0 = no timeout.
     * @return true if successful, false otherwise
     */
    bool setServoSpeed(uint8_t channel, int8_t speed, uint32_t duration_ms = 0);

    /**
     * @brief Set all attached continuous servos to the same speed
     * @param speed        Speed value (-100 to +100)
     * @param duration_ms  Optional auto-stop delay in ms (0 = no timeout, forwarded to setServoSpeed)
     * @return true if successful, false otherwise
     */
    bool setAllServoSpeed(int8_t speed, uint32_t duration_ms = 0);

    /**
     * @brief Set speed for multiple servos at once
     * @param ops Vector of ServoSpeedOp operations
     * @return true if all operations successful, false otherwise
     */
    bool setServosSpeedMultiple(const std::vector<ServoSpeedOp>& ops);

    /**
     * 
     * @brief Set all attached angular servos to middle position
     * @return true if successful, false otherwise
     */
    bool setAllServoAngle(int16_t angle);

    /**
     * @brief Set angle for multiple servos at once
     * @param ops Vector of ServoAngleOp operations
     * @return true if all operations successful, false otherwise
     */
    bool setServosAngleMultiple(const std::vector<ServoAngleOp>& ops);

    /**
     * @brief Set DC motor speed via DFR1216 expansion board
     * @param motor Motor number (1-4)
     * @param speed Speed value (-100 to +100, negative is reverse)
     * @return true if successful, false otherwise
     */
    bool setMotorSpeed(uint8_t motor, int8_t speed);

    /**
     * @brief Set the same speed on all DC motors
     * @param speed Speed value (-100 to +100, negative is reverse)
     * @return true if all motors were set successfully, false otherwise
     */
    bool setAllMotorsSpeed(int8_t speed);

    /**
     * @brief Get the last commanded speed for a DC motor
     * @param motor Motor number (1-4)
     * @return Speed value (-100 to +100), or -128 if not yet commanded
     */
    int8_t getMotorSpeed(uint8_t motor) const;

    /**
     * @brief Get the connection type of a servo channel
     * @param channel Servo channel (0-7)
     * @return ServoConnection enum value
     */
    ServoConnection getServoConnection(uint8_t channel) const;

    /**
     * @brief Get the last commanded speed for a rotational servo
     * @param channel Servo channel (0-7)
     * @return Speed value (-100 to +100), or -128 if not yet commanded
     */
    int8_t getServoSpeed(uint8_t channel) const;

    /**
     * @brief Get the last commanded angle for an angular servo
     * @param channel Servo channel (0-7)
     * @return Angle in degrees, or -1 if not yet commanded
     */
    int16_t getServoAngle(uint8_t channel) const;

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

    bool messageHandler(const std::string &message,
                        const IPAddress &remoteIP,
                        uint16_t remotePort) override;

    IsUDPMessageHandlerInterface *asUDPMessageHandlerInterface() override { return this; }

private:

    // HTTP route registration helper methods
    bool addRouteSetServoAngle(const std::vector<OpenAPIResponse>& standard_responses);
    bool addRouteSetServoSpeed(const std::vector<OpenAPIResponse>& standard_responses);
    bool addRouteStopAll(const std::vector<OpenAPIResponse>& standard_responses);
    bool addRouteGetStatus(const std::vector<OpenAPIResponse>& standard_responses);
    bool addRouteGetAllStatus();
    bool addRouteSetAllAngle(const std::vector<OpenAPIResponse>& standard_responses);
    bool addRouteSetAllSpeed(const std::vector<OpenAPIResponse>& standard_responses);
    bool addRouteSetServosSpeedMultiple(const std::vector<OpenAPIResponse>& standard_responses);
    bool addRouteSetServosAngleMultiple(const std::vector<OpenAPIResponse>& standard_responses);
    bool addRouteAttachServo(const std::vector<OpenAPIResponse>& standard_responses);
    bool addRouteSetMotorSpeed(const std::vector<OpenAPIResponse>& standard_responses);
    bool addRouteStopAllMotors(const std::vector<OpenAPIResponse>& standard_responses);
    bool addRouteSetAllMotorsSpeed(const std::vector<OpenAPIResponse>& standard_responses);
    bool addRouteGetBattery();

    // UDP binary helpers
    std::string getAttachedServosMasked(uint8_t mask);

    /**
     * @brief Start or cancel the per-channel auto-stop FreeRTOS timer.
     * @param channel     Servo channel (0-7)
     * @param duration_ms Auto-stop delay in ms; 0 cancels any pending stop.
     */
    void scheduleChannelStop(uint8_t channel, uint32_t duration_ms);
};
