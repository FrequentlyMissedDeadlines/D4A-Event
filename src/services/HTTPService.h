/**
 * @file HTTPService.h
 * @brief Header for WebServer module integration with the main application
 * @details Provides methods to initialize and handle a WebServer instance.
 */
#pragma once
#include <WebServer.h>
#include <vector>
#include "IsServiceInterface.h"
#include "IsOpenAPIInterface.h"

/**
 * @class HTTPService
 * @brief Service for managing HTTP web server and OpenAPI routes
 */
class HTTPService : public IsServiceInterface, public IsOpenAPIInterface
{
public:
    /**
     * @fn begin
     * @brief Initialize the web server with given WebServer instance
     * @param webserver Pointer to WebServer instance
     * @return true if initialization was successful, false otherwise
     */
    bool begin(WebServer *webserver);
    /**
     * @fn handleClient
     * @brief Handle incoming client requests - to be called in main loop
     * @param webserver Pointer to WebServer instance
     */
    void handleClient(WebServer *webserver);

    /**
     * @fn logRequest
     * @brief Log HTTP request method and URI
     * @param webserver Pointer to WebServer instance
     */
    void logRequest(WebServer *webserver);
    
    /**
     * Handle home page request with route listing
     */
    void handleHomeClient(WebServer *webserver);

    /**
     * Handle test page request with interactive forms for all routes
     */
    void handleTestClient(WebServer *webserver);

    /**
     * @fn handleMasterConflict
     * @brief Master registration conflict handling - called by display task
     */
    void handleMasterConflict(void);
    
    /**
     * @fn acceptMasterConflict
     * @brief Accept master conflict resolution
     */
    void acceptMasterConflict(void);
    
    /**
     * @fn denyMasterConflict
     * @brief Deny master conflict resolution
     */
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
     * Get service subpath component
     * @return Service subpath
     */
    std::string getServiceSubPath() override;

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
     * @brief Handle requests without a registered route
     * @param webserver Pointer to WebServer instance
     */
    void handleNotFoundClient(WebServer *webserver);

    /**
     * Start the webserver after all routes are registered
     * @return true if server started successfully
     */
    bool startWebServer();

    virtual bool initializeService() override;
    virtual bool startService() override;
    virtual bool stopService() override;
    std::string getServiceName() override;

    bool saveSettings() override;
    bool loadSettings() override;

protected:
    std::vector<IsOpenAPIInterface *> openAPIServices;
    bool routesRegistered = false;
    bool serverRunning = false;

    /**
     * @brief Attempt to serve a file from LittleFS for the current request
     * @param webserver Pointer to WebServer instance
     * @return true if a file was found and sent
     */
    bool tryServeLittleFS(WebServer *webserver);

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