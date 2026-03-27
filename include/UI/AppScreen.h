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
#include "BotCommunication/BotServerWeb.h"
#include "BotCommunication/BotServerWebSocket.h"

/**
 * @class AppScreencells
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
              BotServerWeb       &web,
              BotServerUDP       &udp,
              BotServerWebSocket &ws,
              MotorServoService  &motor_servo);

    /** @brief Clear screen to black, then draw all static chrome (borders, fixed labels) once. */
    void initScreen() override;

    /** @brief Repaint dynamic cells only (live values); chrome is drawn once in initScreen(). */
    void updateScreen() override;

    /** @return Always true — live counters change every tick. */
    bool needsUpdate() const override { return true; }

private:
    WifiService        &wifi_;
    AmakerBotService   &amakerbot_;
    BotServerWeb       &web_;
    BotServerUDP       &udp_;
    BotServerWebSocket &ws_;
    MotorServoService  &motor_servo_;

    /**
     * @brief Draw left info panel (bot name, network, master IP).
     * @param chrome_only true → static borders and fixed-value rows only;
     *                    false → dynamic value cells only.
     */
    void drawInfoPanel(bool chrome_only);

    /**
     * @brief Draw transport counters panel (UDP / Web / WebSocket).
     * @param chrome_only true → header borders; false → live counter values.
     */
    void drawCountersPanel(bool chrome_only);

    /**
     * @brief Draw right-panel servo table.
     * @param chrome_only true → static borders; false → status header + angles.
     */
    void drawServoTable(bool chrome_only);

    /**
     * @brief Draw right-panel motor table.
     * @param chrome_only true → static borders; false → status header + speeds.
     */
    void drawMotorTable(bool chrome_only);

    /**
     * @brief Draw ESP info panel (chip model, memory stats).
     * @param chrome_only true → static rows (chip, fixed sizes); false → heap/PSRAM free.
     */
    void drawESPInfo(bool chrome_only);
    void drawBatteryInfo(bool chrome_only);
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

    /**
     * @brief Format an integer with space thousands separators (e.g. 1234567 → "1 234 567").
     * @param value  Integer value to format.
     * @return std::string formatted with space-separated digit groups of three.
     */
    static std::string formatThousands(uint32_t value);

    uint8_t battery_level_ = 0; ///< Last battery reading (0–100 %) from DFR1216Board
};