#include <WebServer.h>
#include "../services/HasRoutesInterface.h"
#pragma once
/**
 * @file board_info.h
 * @brief Header for board information module.  
 * Provides system metrics and board details via HTTP routes.
 * @details Inherits from withRoutes to register HTTP routes with a WebServer instance.
 */
class BoardInfo : public HasRoutesInterface{
public:
    std::set<std::string> getRoutes() override;
    bool registerRoutes(WebServer *server, std::string basePath) override;

};