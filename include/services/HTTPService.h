/**
 * @file HTTPService.h
 * @brief Header for AsyncWebServer module integration with the main application
 * @details Provides methods to initialize and handle an AsyncWebServer instance.
 */
#pragma once

// Include ESPAsyncWebServer first to avoid HTTP method enum conflicts
#include <ESPAsyncWebServer.h>
#include <vector>
#include "IsServiceInterface.h"
#include "IsOpenAPIInterface.h"

/**
 * @class HTTPService
 * @brief Service for managing asynchronous HTTP web server and OpenAPI routes
 */
class HTTPService : public IsOpenAPIInterface
{
public:
    /**
     * @fn logRequest
     * @brief Log HTTP request method and URI
     * @param request Pointer to AsyncWebServerRequest
     */
    void logRequest(AsyncWebServerRequest *request);
    
    /**
     * @brief Handle home page request with route listing
     * @details Renders the home page with available routes and interactive test forms
     * @param request Pointer to AsyncWebServerRequest
     */
    void handleHomeClient(AsyncWebServerRequest *request);

    /**
     * @brief Handle test page request with interactive forms for all routes
     * @details Serves the API test page with dynamically generated forms for testing endpoints
     * @param request Pointer to AsyncWebServerRequest
     */
    void handleTestClient(AsyncWebServerRequest *request);

    /**
     * @brief Handle master registration conflict
     * @details Called by display task to handle service registration conflicts
     * @return true if conflict handling was successful
     */
    void handleMasterConflict(void);
    
    /**
     * @brief Accept master conflict resolution
     * @details Accepts the proposed master conflict resolution
     */
    void acceptMasterConflict(void);
    
    /**
     * @brief Deny master conflict resolution
     * @details Rejects the proposed master conflict resolution
     */
    void denyMasterConflict(void);

    /**
     * @brief Register HTTP routes for HTTPService itself
     * @details Registers the home page, test page, and OpenAPI documentation routes
     * @return true if registration was successful
     */
    bool registerRoutes() override;

    /**
     * @brief Get service subpath component
     * @return Service subpath (e.g., "http/v1")
     */
    std::string getServiceSubPath() override;

    /**
     * Register an OpenAPI-enabled service
     * @param service Pointer to service implementing IsOpenAPIInterface
     */
    void registerOpenAPIService(IsOpenAPIInterface *service);

    /**
     * Handle OpenAPI spec request
     * @param request Pointer to AsyncWebServerRequest
     */
    void handleOpenAPIRequest(AsyncWebServerRequest *request);


    /**
     * @brief Handle requests without a registered route
     * @param request Pointer to AsyncWebServerRequest
     */
    void handleNotFoundClient(AsyncWebServerRequest *request);

    /**
     * Start the webserver after all routes are registered
     * @return true if server started successfully
     */
    bool startWebServer();


    virtual bool startService() override;
    virtual bool stopService() override;
    std::string getServiceName() override;

protected:
    std::vector<IsOpenAPIInterface *> openAPIServices;
    bool routesRegistered = false;



    /**
     * @brief Attempt to serve a file from LittleFS for the current request
     * @param request Pointer to AsyncWebServerRequest
     * @return true if a file was found and sent
     */
    bool tryServeLittleFS(AsyncWebServerRequest *request);

    /**
     * @brief Read file content from LittleFS into a string
     * @param path Path to the file in LittleFS
     * @return File content as string, empty string on failure
     */
    std::string readFileToString(const char *path);

    /**
     * @brief Resolve MIME type based on file extension
     * @param path Request path
     * @return MIME type string
     */
    String getContentTypeForPath(const String &path);

    /**
     * @brief List all files in LittleFS recursively (for debugging)
     * @param fs Filesystem to list
     * @param dirname Directory to start from
     * @param levels Max recursion levels
     */
    void listFilesInFS(fs::FS &fs, const char *dirname, uint8_t levels = 5, uint8_t currentLevel = 0);

};