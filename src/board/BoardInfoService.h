#pragma once
#include <WebServer.h>
#include "../services/IsOpenAPIInterface.h"
#include "../services/IsServiceInterface.h"

/**
 * @file board_info.h
 * @brief Header for board information module.
 * Provides system metrics and board details via HTTP routes.
 * @details Inherits from withRoutes to register HTTP routes with a WebServer instance.
 */
class BoardInfoService : public IsOpenAPIInterface, public IsServiceInterface
{
public:
    bool registerRoutes() override;
    std::string getPath(const std::string& finalpathstring) override;
    bool initializeService() override ;
    bool startService() override ;
    bool stopService() override ;
    std::string getName() override;
    IsOpenAPIInterface* asOpenAPIInterface() override { return this; }

private:
    std::string baseServicePath;  // Cached for optimization
};