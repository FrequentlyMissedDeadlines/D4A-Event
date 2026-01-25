/**
 * @file web_server.h
 * @brief Header for WebServer module integration with the main application
 */
#pragma once
#include <WebServer.h>
#include <vector>
#include "IsServiceInterface.h"
#include "IsOpenAPIInterface.h"

/**
 * @file web_server.h
 * @brief Header for WebServer module.
 * @details Provides methods to initialize and handle a WebServer instance.
 *
 */
class HTTPService : public IsServiceInterface, public IsOpenAPIInterface
{
public:
    /**
        Initialize the web webserver with given WebServer instance
        */

    bool begin(WebServer *webserver);
    /**
    Handle incoming client requests - to be called in main loop
    */
    void handleClient(WebServer *webserver);
    
    /**
     * Handle home page request with route listing
     */
    void handleHomeClient(WebServer *webserver);

    /**
     * Handle test page request with interactive forms for all routes
     */
    void handleTestClient(WebServer *webserver);

    /**
     Master registration conflict handling - called by display task
    */
    void handleMasterConflict(void);
    
    /**
     Button handlers for master conflict resolution
    */
    void acceptMasterConflict(void);
    void denyMasterConflict(void);

    /**
     * Allow HTTPService to be accessed as IsOpenAPIInterface
     */
    IsOpenAPIInterface* asOpenAPIInterface() override { return this; }

    /**
     * Register HTTP routes for HTTPService itself
     * @return true if registration was successful
     */
    bool registerRoutes() override;

    /**
     * Construct full API path from service name and final path segment
     * @param finalpathstring The final path segment to append
     * @return Full path in format /api/<servicename>/<finalpathstring>
     */
    std::string getPath(const std::string& finalpathstring) override;

    /**
     * Register an OpenAPI-enabled service
     * @param service Pointer to service implementing IsOpenAPIInterface
     */
    void registerOpenAPIService(IsOpenAPIInterface *service);

    /**
     * Handle OpenAPI spec request
     * @param webserver Pointer to WebServer instance
     */
    void handleOpenAPIRequest(WebServer *webserver);

    /**
     * Start the webserver after all routes are registered
     * @return true if server started successfully
     */
    bool startWebServer();

    virtual bool initializeService() override;
    virtual bool startService() override;
    virtual bool stopService() override;
    std::string getName() override;


protected:
    std::vector<IsOpenAPIInterface *> openAPIServices;
    bool routesRegistered = false;
    bool serverRunning = false;
    std::string baseServicePath;  // Cached for optimization

};