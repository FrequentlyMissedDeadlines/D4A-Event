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
 * | 0 | SCREEN_SPLASH      | SplashScreen| Boot splash image                 |
 * | 1 | SCREEN_APP_INFO    | AppScreen   | Network, master, servos & motors  |
 * | 2 | SCREEN_APP_LOG     | LogScreen   | Application / bot logger          |
 * | 3 | SCREEN_SVC_LOG     | LogScreen   | Services logger                   |
 * | 4 | SCREEN_DEBUG_LOG   | LogScreen   | Debug logger                      |
 * | 5 | SCREEN_ESP_LOG     | LogScreen   | ESP-IDF logger                    |
 *
 * ## Usage
 * @code
 *   AmakerBotUIService ui(unihiker, wifi, amakerbot, udp, ws, motor_servo);
 *   ui.setShownLoggers(&bot_logger, &svc_logger, &debug_logger, nullptr);
 *   ui.setDebugLogger(&debug_logger);   // IsServiceInterface logger
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
#include "IsBotMessageHandlerInterface.h"
#include "RollingLogger.h"
#include "UI/AppScreen.h"
#include "UI/LogScreen.h"
#include "UI/SplashScreen.h"

// ---------------------------------------------------------------------------
// PROGMEM string constants
// ---------------------------------------------------------------------------
namespace AmakerBotUIConsts
{
    constexpr const char str_service_name[] PROGMEM = "AmakerBot UI";

    constexpr unsigned long BTN_A_DEBOUNCE_MS  = 250;  ///< Button A debounce period (ms)
    constexpr unsigned long SPLASH_MAX_MS       = 10000; ///< Max splash screen duration (ms)

    // Log-screen title strings (held in flash)
    constexpr const char scr_name_app_log[]   PROGMEM = "2: App Log";
    constexpr const char scr_name_svc_log[]   PROGMEM = "3: Svc Log";
    constexpr const char scr_name_debug_log[] PROGMEM = "4: Debug Log";
    constexpr const char scr_name_esp_log[]   PROGMEM = "5: ESP Log";

    // Bot protocol commands for remote UI control
    constexpr uint8_t UI_SERVICE_ID      = 0x06;      ///< Service ID for UI commands
    constexpr uint8_t CMD_NEXT_SCREEN    = 0x01;      ///< Advance to next screen
    constexpr uint8_t CMD_PREV_SCREEN    = 0x02;      ///< Go back to previous screen
    constexpr uint8_t CMD_SET_SCREEN     = 0x03;      ///< Set screen to specific index [index]
} // namespace AmakerBotUIConsts

// ---------------------------------------------------------------------------
// AmakerBotUIService
// ---------------------------------------------------------------------------
/**
 * @class AmakerBotUIService
 * @brief Manages five TFT screens for the AmakerBot, cycled with button A.
 *
 * Construct with all runtime service references (forwarded to AppScreen).
 * Attach RollingLogger instances via setShownLoggers() to enable log screens.
 * Call tick() periodically from a FreeRTOS task (e.g. every 500 ms).
 */
class AmakerBotUIService : public IsServiceInterface,
                         public IsBotActionHandlerInterface
{
public:
    // ---- Screen identifiers -----------------------------------------

    enum Screen : uint8_t
    {
        SCREEN_SPLASH    = 0,   ///< SplashScreen — shown first on boot
        SCREEN_APP_INFO  = 1,   ///< AppScreen dashboard
        SCREEN_APP_LOG   = 2,   ///< LogScreen — application/bot logger
        SCREEN_SVC_LOG   = 3,   ///< LogScreen — services logger
        SCREEN_DEBUG_LOG = 4,   ///< LogScreen — debug logger
        SCREEN_ESP_LOG   = 5,   ///< LogScreen — ESP-IDF logger
        SCREEN_COUNT     = 6,
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
                       BotServerWeb        &web,
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
    void setShownLoggers(RollingLogger *app,
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

    // ---- IsBotActionHandlerInterface --------------------------------

    /** @return Service ID for remote UI control (0x05) */
    uint8_t getBotServiceId() const override;

    /**
     * @brief Handle bot protocol messages for UI control.
     * @param data Raw frame; byte[0] is the action byte
     * @param len  Frame length in bytes
     * @return Binary response (empty for all UI commands)
     */
    std::string handleBotMessage(const uint8_t *data, size_t len) override;

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
    SplashScreen splash_screen_;      ///< Shown once on startup
    AppScreen    app_screen_;         ///< Screen 0 — always present
    LogScreen   *log_screens_[4] = {}; ///< Screens 1–4 — created in setShownLoggers()

    // ---- Navigation state -------------------------------------------
    Screen        current_screen_  = SCREEN_APP_INFO;
    Screen        previous_screen_ = static_cast<Screen>(0xFF); ///< Force clear on first tick

    // ---- Splash timeout ----------------------------------------------
    unsigned long splash_start_ms_ = 0;   ///< millis() when splash was first shown

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



