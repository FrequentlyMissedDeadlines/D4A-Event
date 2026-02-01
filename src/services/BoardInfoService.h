#pragma once
#include <WebServer.h>
#include "../services/IsOpenAPIInterface.h"
#include "../services/IsServiceInterface.h"

/**
 * @file BoardInfoService.h
 * @brief Header for board information service
 * @details Provides system metrics and board details via HTTP routes.
 */
class BoardInfoService : public IsOpenAPIInterface, public IsServiceInterface
{
public:
    bool registerRoutes() override;
    std::string getServiceSubPath() override;
    bool initializeService() override ;
    bool startService() override ;
    bool stopService() override ;
    std::string getServiceName() override;
    IsOpenAPIInterface* asOpenAPIInterface() override { return this; }
    bool saveSettings() override;
    bool loadSettings() override;

private:
    enum ServiceStatus { INIT_FAILED, START_FAILED, STARTED, STOPPED, STOP_FAILED };
    ServiceStatus service_status_ = STOP_FAILED;
    unsigned long status_timestamp_ = 0;
};