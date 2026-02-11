#pragma once
#include <WebServer.h>
#include "../IsOpenAPIInterface.h"


/**
 * @file BoardInfoService.h
 * @brief Header for board information service
 * @details Provides system metrics and board details via HTTP routes.
 */
class BoardInfoService : public IsOpenAPIInterface
{
public:
    bool registerRoutes() override;
    std::string getServiceSubPath() override;
    std::string getServiceName() override;



};