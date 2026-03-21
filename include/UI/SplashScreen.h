#include "UI/IsScreen.h"
class SplashScreen : public IsScreen {
public:
    void initScreen() override;

    /** @brief Repaint dynamic cells only (live values); chrome is drawn once in initScreen(). */
    void updateScreen() override {};
};