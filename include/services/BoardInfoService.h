#pragma once
#include <ESPAsyncWebServer.h>
#include "IsOpenAPIInterface.h"
#include "isUDPMessageHandlerInterface.h"


/**
 * @file BoardInfoService.h
 * @brief Header for board information service
 * @details Provides system metrics, board details, RGB LED control via HTTP routes, and UDP message handling.
 */
class BoardInfoService : public IsOpenAPIInterface, public IsUDPMessageHandlerInterface
{
public:
    bool registerRoutes() override;
    std::string getServiceSubPath() override;
    std::string getServiceName() override;

    /**
     * @brief Set RGB LED color on the board
     * @param led_index LED index (0, 1, or 2)
     * @param red Red value (0-255)
     * @param green Green value (0-255)
     * @param blue Blue value (0-255)
     * @return true if successful, false otherwise
     */
    bool setRGBLED(uint8_t led_index, uint8_t red, uint8_t green, uint8_t blue);

    /**
     * @brief Turn off an RGB LED
     * @param led_index LED index (0, 1, or 2)
     * @return true if successful, false otherwise
     */
    bool turnOffRGBLED(uint8_t led_index);

    /**
     * @brief Turn off all RGB LEDs
     * @return true if successful, false otherwise
     */
    bool turnOffAllRGBLEDs();

    /**
     * @brief Handle incoming UDP messages for RGB LED control
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
    // LED color storage (RGB values for each LED)
    struct RGBLEDState {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    } rgb_led_states_[3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};

    // HTTP handler methods
    void handle_set_rgb_led(AsyncWebServerRequest *request);
    void handle_get_rgb_leds(AsyncWebServerRequest *request);
    void handle_turn_off_rgb_led(AsyncWebServerRequest *request);
};
