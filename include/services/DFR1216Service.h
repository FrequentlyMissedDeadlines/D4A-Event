// DFR1216 (Unihiker Expansion) Service
/**
 * @file DFR1216Service.h
 * @brief Header for DFR1216 expansion board integration with the main application
 * @details
 * - Provides DFR1216 initialization, control, and HTTP route registration.
 * - Implements IsOpenAPIInterface (which extends IsServiceInterface)
 * - Uses the global `AsyncWebServer webserver` instance that is created in `main.cpp`.
 */
#pragma once

#include "IsOpenAPIInterface.h"
#include "isUDPMessageHandlerInterface.h"
#include "DFR1216/DFR1216.h"

class DFR1216Service : public IsOpenAPIInterface, public IsUDPMessageHandlerInterface
{
public:
    /**
     * @fn: getServiceName
     * @brief: Get the name of the service.
     * @return: Service name as a string.
     */
    std::string getServiceName() override { return "DFR1216 Service"; }

    /**
     * @fn: getServiceSubPath
     * @brief: Get the service's subpath component
     * @return: Service subpath (e.g., "DFR1216/v1")
     */
    std::string getServiceSubPath() override { return "DFR1216/v1"; }

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
     * @details Uses the global `AsyncWebServer webserver` instance (see `IsOpenAPIInterface.h`).
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
     * @brief Set LED color via WS2812
     * @param led_index LED index (0, 1, or 2)
     * @param red Red value (0-255)
     * @param green Green value (0-255)
     * @param blue Blue value (0-255)
     * @param brightness Brightness level (0-255)
     * @return true if successful, false otherwise
     */
    bool setLEDColor(uint8_t led_index, uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness = 32);

    /**
     * @brief Turn off an LED
     * @param led_index LED index (0, 1, or 2)
     * @return true if successful, false otherwise
     */
    bool turnOffLED(uint8_t led_index);

    /**
     * @brief Turn off all LEDs
     * @return true if successful, false otherwise
     */
    bool turnOffAllLEDs();

    bool saveSettings() override;
    bool loadSettings() override;

    /**
     * @brief Handle incoming UDP messages for LED control
     * @param message Raw UDP message
     * @param remoteIP Sender IP address
     * @param remotePort Sender port
     * @return true if message was handled, false otherwise
     */
    bool messageHandler(const std::string &message,
                        const IPAddress &remoteIP,
                        uint16_t remotePort) override;

    IsUDPMessageHandlerInterface *asUDPMessageHandlerInterface() override { return this; }

private:

    DFR1216_I2C controller;
    
    // LED color storage (RGB values for each LED)
    struct LEDState {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    } led_states_[3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};

    // HTTP handler methods
    void handle_set_servo_angle(AsyncWebServerRequest *request);
    void handle_set_motor_speed(AsyncWebServerRequest *request);
    void handle_get_status(AsyncWebServerRequest *request);
    void handle_set_led_color(AsyncWebServerRequest *request);
    void handle_turn_off_led(AsyncWebServerRequest *request);
    void handle_get_led_status(AsyncWebServerRequest *request);
};
