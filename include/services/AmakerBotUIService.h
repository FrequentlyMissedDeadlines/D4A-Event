/**
 * @file AmakerBotUIService.h
 * @brief K10 TFT screen manager — delegates rendering to AppScreen and LogScreen.
 *
 * @details Owns one AppScreen (dashboard) and up to four LogScreen instances
 * (one per RollingLogger).  Button A cycles through them.  All actual drawing
 * logic lives inside the screen objects; this service only manages:
 *   - Screen switching and button-A debounce
 *   - Calling initScreen() on transition and updateScreen() when needsUpdate()
 *
 * ## Screens
 * | # | Enum               | Class       | Content                           |
 * |---|--------------------|-----------  |-----------------------------------|
 * | 0 | SCREEN_APP_INFO    | AppScreen   | Network, master, servos & motors  |
 * | 1 | SCREEN_APP_LOG     | LogScreen   | Application / bot logger          |
 * | 2 | SCREEN_SVC_LOG     | LogScreen   | Services logger                   |
 * | 3 | SCREEN_DEBUG_LOG   | LogScreen   | Debug logger                      |
 * | 4 | SCREEN_ESP_LOG     | LogScreen   | ESP-IDF logger                    |
 *
 * ## Usage
 * @code
 *   AmakerBotUIService ui(unihiker, wifi, amakerbot, udp, ws, motor_servo);
 *   ui.setLoggers(&bot_logger, &svc_logger, &debug_logger, nullptr);
 *   ui.setLogger(&debug_logger);   // IsServiceInterface logger
 *   ui.initializeService();
 *   ui.startService();
 *
 *   // In FreeRTOS display task (Core 1):
 *   for (;;) { ui.tick(); vTaskDelay(pdMS_TO_TICKS(500)); }
 * @endcode
 */
#pragma once

#include <cstdint>
#include <string>
#include <pgmspace.h>
#include <unihiker_k10.h>

#include "IsServiceInterface.h"
#include "RollingLogger.h"
#include "UI/AppScreen.h"
#include "UI/LogScreen.h"

// ---------------------------------------------------------------------------
// PROGMEM string constants
// ---------------------------------------------------------------------------
namespace AmakerBotUIConsts
{
    constexpr const char str_service_name[] PROGMEM = "AmakerBot UI";
    constexpr const char msg_init_ok[]      PROGMEM = "AmakerBotUIService: initialized";
    constexpr const char msg_started[]      PROGMEM = "AmakerBotUIService: started";
    constexpr const char msg_stopped[]      PROGMEM = "AmakerBotUIService: stopped";

    constexpr unsigned long BTN_A_DEBOUNCE_MS = 250; ///< Button A debounce period (ms)

    // Log-screen title strings (held in flash)
    constexpr const char scr_name_app_log[]   PROGMEM = "2: App Log";
    constexpr const char scr_name_svc_log[]   PROGMEM = "3: Svc Log";
    constexpr const char scr_name_debug_log[] PROGMEM = "4: Debug Log";
    constexpr const char scr_name_esp_log[]   PROGMEM = "5: ESP Log";
} // namespace AmakerBotUIConsts

// ---------------------------------------------------------------------------
// AmakerBotUIService
// ---------------------------------------------------------------------------
/**
 * @class AmakerBotUIService
 * @brief Manages five TFT screens for the AmakerBot, cycled with button A.
 *
 * Construct with all runtime service references (forwarded to AppScreen).
 * Attach RollingLogger instances via setLoggers() to enable log screens.
 * Call tick() periodically from a FreeRTOS task (e.g. every 500 ms).
 */
class AmakerBotUIService : public IsServiceInterface
{
public:
    // ---- Screen identifiers -----------------------------------------

    enum Screen : uint8_t
    {
        SCREEN_APP_INFO  = 0,   ///< AppScreen dashboard
        SCREEN_APP_LOG   = 1,   ///< LogScreen — application/bot logger
        SCREEN_SVC_LOG   = 2,   ///< LogScreen — services logger
        SCREEN_DEBUG_LOG = 3,   ///< LogScreen — debug logger
        SCREEN_ESP_LOG   = 4,   ///< LogScreen — ESP-IDF logger
        SCREEN_COUNT     = 5,
    };

    // ---- Construction / Destruction ---------------------------------

    /**
     * @brief Construct with all runtime service references.
     *
     * The references are forwarded directly to the internal AppScreen.
     */
    AmakerBotUIService(UNIHIKER_K10        &k10,
                       WifiService         &wifi,
                       AmakerBotService    &amakerbot,
                       BotServerUDP        &udp,
                       BotServerWebSocket  &ws,
                       MotorServoService   &motor_servo);

    ~AmakerBotUIService();

    // ---- Logger injection -------------------------------------------

    /**
     * @brief Attach RollingLogger instances to the four log screens.
     *
     * Creates a LogScreen for each non-null logger.  May be called before
     * or after startService().  A nullptr logger leaves that screen slot empty.
     *
     * @param app    → SCREEN_APP_LOG
     * @param svc    → SCREEN_SVC_LOG
     * @param debug  → SCREEN_DEBUG_LOG
     * @param esp    → SCREEN_ESP_LOG
     */
    void setLoggers(RollingLogger *app,
                    RollingLogger *svc,
                    RollingLogger *debug,
                    RollingLogger *esp);

    // ---- IsServiceInterface -----------------------------------------

    /** @return "AmakerBot UI" */
    std::string getServiceName() override;

    /** @brief Reset navigation state. */
    bool initializeService() override;

    /** @brief Clear the screen and initialise the active screen. */
    bool startService() override;

    /** @brief Clear the screen and mark stopped. */
    bool stopService() override;

    // ---- Runtime API ------------------------------------------------

    /**
     * @brief Update the display.  Call from FreeRTOS display task (~500 ms).
     *
     * Polls button A, detects screen changes, and delegates to the active
     * screen's initScreen() / updateScreen() as needed.
     */
    void tick();

    /**
     * @brief Advance to the next screen (wraps from SCREEN_COUNT-1 → 0).
     * Also triggered automatically by button A.
     */
    void nextScreen();

    /** @brief Return the currently active screen. */
    Screen getScreen() const { return current_screen_; }

    /**
     * @brief Jump directly to a specific screen.
     * @param s Target screen; ignored if out of range.
     */
    void setScreen(Screen s);

private:
    UNIHIKER_K10 &k10_;

    // ---- Owned screen objects ---------------------------------------
    AppScreen   app_screen_;          ///< Screen 0 — always present
    LogScreen  *log_screens_[4] = {}; ///< Screens 1–4 — created in setLoggers()

    // ---- Navigation state -------------------------------------------
    Screen        current_screen_  = SCREEN_APP_INFO;
    Screen        previous_screen_ = static_cast<Screen>(0xFF); ///< Force clear on first tick

    // ---- Button A debounce ------------------------------------------
    bool          btn_a_prev_    = false;
    unsigned long btn_a_last_ms_ = 0;

    // ---- Helpers ----------------------------------------------------

    /**
     * @brief Return a pointer to the active IsScreen, or nullptr if the slot
     *        is empty (e.g. a log screen with no attached logger).
     */
    IsScreen *activeScreen();

    /** @brief Poll button A and call nextScreen() on a debounced rising edge. */
    void pollButtonA();

    /** @brief Return true if the active screen needs a redraw this tick. */
    bool needsRedraw() const;
};



