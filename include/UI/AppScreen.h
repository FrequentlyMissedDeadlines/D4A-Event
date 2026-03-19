/**
 * @file AppScreen.h
 * @brief TFT dashboard screen: network info, master, UDP/WS counters, servos and motors.
 *
 * The 240×320 display is split into a left info panel (chars 0–20) and a right
 * panel (chars 21–39) showing servo types/angles above and DC motor speeds below.
 *
 * Always returns needsUpdate() == true because it shows live counters.
 */
#pragma once

#include <cstdint>
#include "UI/IsScreen.h"
#include "UI/UIConsts.h"
#include "IsServiceInterface.h"
#include "services/WiFiService.h"
#include "services/AmakerBotService.h"
#include "services/MotorServoService.h"
#include "BotCommunication/BotServerUDP.h"
#include "BotCommunication/BotServerWebSocket.h"

/**
 * @class AppScreen
 * @brief Renders the AmakerBot application info dashboard to the TFT.
 *
 * Construct with references to the services it needs to read data from.
 * Call initScreen() once on screen switch, then updateScreen() every tick.
 */
class AppScreen : public IsScreen
{
public:
    /**
     * @brief Construct with all runtime service references required for display.
     *
     * @param wifi        WiFi service — SSID, IP, hostname
     * @param amakerbot   AmakerBot service — bot name, token, master IP
     * @param udp         UDP server — RX / TX / dropped counters, port
     * @param ws          WebSocket server — RX / TX / dropped counters
     * @param motor_servo MotorServo service — servo types / angles, motor speeds
     */
    AppScreen(WifiService        &wifi,
              AmakerBotService   &amakerbot,
              BotServerUDP       &udp,
              BotServerWebSocket &ws,
              MotorServoService  &motor_servo);

    /** @brief Clear screen to black and set up font. */
    void initScreen() override;

    /** @brief Redraw the full dashboard: info panel, servo table, motor table. */
    void updateScreen() override;

    /** @return Always true — live counters change every tick. */
    bool needsUpdate() const override { return true; }

private:
    WifiService        &wifi_;
    AmakerBotService   &amakerbot_;
    BotServerUDP       &udp_;
    BotServerWebSocket &ws_;
    MotorServoService  &motor_servo_;

    /** @brief Draw left panel: bot name, network, master IP, transport counters. */
    void drawInfoPanel();

    /** @brief Draw right-panel servo table (chars 21+, lines 0–10). */
    void drawServoTable();

    /** @brief Draw right-panel motor table (chars 21+, below servos). */
    void drawMotorTable();

    /**
     * @brief Map a ServiceStatus to foreground/background TFT colours.
     * @param status  Service state
     * @param fg      Output: foreground colour
     * @param bg      Output: background colour
     */
    static void colorsForStatus(ServiceStatus status, uint16_t &fg, uint16_t &bg);

    /**
     * @brief Return a short 3-character type label for a servo channel.
     * @param t ServoType value
     * @return "180", "270", or "rot"
     */
    static const char *servoTypeLabel(ServoType t);
};