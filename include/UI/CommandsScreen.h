/**
 * @file CommandsScreen.h
 * @brief TFT dashboard screen: network info, master, UDP/WS counters, servos and motors.
 *
 * The 240×320 display is split into a left info panel (chars 0–20) and a right
 * panel (chars 21–39) showing servo types/angles above and DC motor speeds below.
 *
 * Always returns needsUpdate() == true because it shows live counters.
 */
#pragma once

#include "UI/IsScreen.h"
#include "UI/UIConsts.h"
#include "services/AmakerBotService.h"

/**
 * @class CommandsScreen
 * @brief Renders the AmakerBot application info dashboard to the TFT.
 *
 * Construct with references to the services it needs to read data from.
 * Call initScreen() once on screen switch, then updateScreen() every tick.
 */
class CommandsScreen : public IsScreen
{
public:
    CommandsScreen(AmakerBotService &amakerbot);
   
    /** @brief Clear screen to black, then draw all static chrome (borders, fixed labels) once. */
    void initScreen() override;

    /** @brief Repaint dynamic cells only (live values); chrome is drawn once in initScreen(). */
    void updateScreen() override;

    /** @return Always true — live counters change every tick. */
    bool needsUpdate() const override { return true; }

private:
    AmakerBotService &amakerbot_;
    void drawCountersPanel(bool chrome_only);

    
};