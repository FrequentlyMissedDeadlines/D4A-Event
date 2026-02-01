#pragma once
/**
 * @file MusicService.h
 * @brief OpenAPI service for music playback on K10
 * @details Exposed routes:
 *          - POST /api/music/v1/play - Play built-in melody with options
 *          - POST /api/music/v1/tone - Play a tone at specified frequency and duration
 *          - POST /api/music/v1/stop - Stop current tone playback
 */
#include "IsServiceInterface.h"
#include "IsOpenAPIInterface.h"

class MusicService : public IsServiceInterface, public IsOpenAPIInterface
{
public:
    bool registerRoutes() override;
    std::string getServiceSubPath() override;
    bool initializeService() override;
    bool startService() override;
    bool stopService() override;
    std::string getServiceName() override;
    IsOpenAPIInterface* asOpenAPIInterface() override { return this; }
    bool saveSettings() override;
    bool loadSettings() override;

private:
    ServiceStatus service_status_ = STOP_FAILED;
    unsigned long status_timestamp_ = 0;
};
