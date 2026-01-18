/**
 * @file web_server.h
 * @brief Header for WebServer module integration with the main application
 */
#pragma once
#include <WebServer.h>
#include "HasLoggerInterface.h"
#include "IsServiceInterface.h"
/**
 * @file web_server.h
 * @brief Header for WebServer module.
 * @details Provides methods to initialize and handle a WebServer instance. 
 * 
 */
class HTTPService : public HasLoggerInterface, public IsServiceInterface
{ 
public:
/**
    Initialize the web server with given WebServer instance
    */
   
    bool begin(WebServer *server);
    /**
    Handle incoming client requests - to be called in main loop
    */
    void handleClient(WebServer *server);
    /**
     Master registration conflict handling - called by display task
    */
    void handleMasterConflict(void);
    /**
     Button handlers for master conflict resolution
    */
    void acceptMasterConflict(void); // Call when button A pressed
    /**
     Button handlers for master conflict resolution
    */
    void denyMasterConflict(void);   // Call when button B pressed



    virtual bool init() override;
    virtual bool start() override;
    virtual bool stop() override; 
};