/**
 * @file AmakerBotUIService.cpp
 * @brief K10 TFT screen manager — delegates rendering to AppScreen and LogScreen.
 *
 * @note All TFT calls use the global `tft` (TFT_eSPI) declared in main.cpp.
 *       The display task (Core 1) must call tick() at ~500 ms intervals.
 */

#include "services/AmakerBotUIService.h"
#include "FlashStringHelper.h"
#include <TFT_eSPI.h>

extern TFT_eSPI tft;

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

AmakerBotUIService::AmakerBotUIService(UNIHIKER_K10        &k10,
                                        WifiService         &wifi,
                                        AmakerBotService    &amakerbot,
                                        BotServerUDP        &udp,
                                        BotServerWebSocket  &ws,
                                        MotorServoService   &motor_servo)
    : k10_(k10),
      app_screen_(wifi, amakerbot, udp, ws, motor_servo)
{
}

AmakerBotUIService::~AmakerBotUIService()
{
    for (auto *&s : log_screens_)
    {
        delete s;
        s = nullptr;
    }
}

// ---------------------------------------------------------------------------
// setLoggers — create one LogScreen per non-null logger
// ---------------------------------------------------------------------------

void AmakerBotUIService::setLoggers(RollingLogger *app,
                                     RollingLogger *svc,
                                     RollingLogger *debug,
                                     RollingLogger *esp)
{
    // Release any previously created screens
    for (auto *&s : log_screens_)
    {
        delete s;
        s = nullptr;
    }

    if (app)   log_screens_[0] = new LogScreen(*app,   "2: App Log");
    if (svc)   log_screens_[1] = new LogScreen(*svc,   "3: Svc Log");
    if (debug) log_screens_[2] = new LogScreen(*debug, "4: Debug Log");
    if (esp)   log_screens_[3] = new LogScreen(*esp,   "5: ESP Log");
}

// ---------------------------------------------------------------------------
// IsServiceInterface
// ---------------------------------------------------------------------------

std::string AmakerBotUIService::getServiceName()
{
    return progmem_to_string(AmakerBotUIConsts::str_service_name);
}

bool AmakerBotUIService::initializeService()
{
    if (isServiceInitialized())
        return true;

    previous_screen_ = static_cast<Screen>(0xFF);
    current_screen_  = SCREEN_APP_INFO;
    btn_a_prev_      = false;
    btn_a_last_ms_   = 0;

    setServiceStatus(INITIALIZED);
    logger->info(FPSTR(AmakerBotUIConsts::msg_init_ok));
    return true;
}

bool AmakerBotUIService::startService()
{
    if (!isServiceInitialized())
    {
        setServiceStatus(START_FAILED);
        return false;
    }

    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.fillScreen(UIColors::CLR_BLACK);
    app_screen_.initScreen();

    setServiceStatus(STARTED);
    logger->info(FPSTR(AmakerBotUIConsts::msg_started));
    return true;
}

bool AmakerBotUIService::stopService()
{
    tft.fillScreen(UIColors::CLR_BLACK);
    setServiceStatus(STOPPED);
    logger->info(FPSTR(AmakerBotUIConsts::msg_stopped));
    return true;
}

// ---------------------------------------------------------------------------
// Screen navigation
// ---------------------------------------------------------------------------

void AmakerBotUIService::nextScreen()
{
    current_screen_ = static_cast<Screen>(
        (static_cast<uint8_t>(current_screen_) + 1) % SCREEN_COUNT);
}

void AmakerBotUIService::setScreen(Screen s)
{
    if (static_cast<uint8_t>(s) < SCREEN_COUNT)
        current_screen_ = s;
}

IsScreen *AmakerBotUIService::activeScreen()
{
    if (current_screen_ == SCREEN_APP_INFO)
        return &app_screen_;

    const int idx = static_cast<int>(current_screen_) - 1;
    if (idx >= 0 && idx < 4)
        return log_screens_[idx]; // may be nullptr if logger was not attached

    return nullptr;
}

// ---------------------------------------------------------------------------
// tick()
// ---------------------------------------------------------------------------

void AmakerBotUIService::tick()
{
    if (!isServiceStarted())
        return;

    pollButtonA();

    const bool screen_changed = (current_screen_ != previous_screen_);

    if (screen_changed)
    {
        tft.fillScreen(UIColors::CLR_BLACK);
        previous_screen_ = current_screen_;

        IsScreen *scr = activeScreen();
        if (scr)
            scr->initScreen();
    }

    if (!screen_changed && !needsRedraw())
        return;

    IsScreen *scr = activeScreen();
    if (scr)
        scr->updateScreen();
}

// ---------------------------------------------------------------------------
// needsRedraw()
// ---------------------------------------------------------------------------

bool AmakerBotUIService::needsRedraw() const
{
    if (current_screen_ == SCREEN_APP_INFO)
        return app_screen_.needsUpdate();

    const int idx = static_cast<int>(current_screen_) - 1;
    if (idx >= 0 && idx < 4 && log_screens_[idx])
        return log_screens_[idx]->needsUpdate();

    return false;
}

// ---------------------------------------------------------------------------
// pollButtonA()
// ---------------------------------------------------------------------------

void AmakerBotUIService::pollButtonA()
{
    if (!k10_.buttonA)
        return;

    const bool pressed      = k10_.buttonA->isPressed();
    const unsigned long now = millis();

    // Rising edge + debounce
    if (pressed && !btn_a_prev_ &&
        (now - btn_a_last_ms_ >= AmakerBotUIConsts::BTN_A_DEBOUNCE_MS))
    {
        btn_a_last_ms_ = now;
        nextScreen();
    }
    btn_a_prev_ = pressed;
}

