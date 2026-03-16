/**
 * @file HTTPService.h
 * @brief Header for AsyncWebServer module integration with the main application
 * @details Provides methods to initialize and handle an AsyncWebServer instance.
 */
#pragma once

// Include ESPAsyncWebServer first to avoid HTTP method enum conflicts
#include <ESPAsyncWebServer.h>
#include <vector>
#include <atomic>
#include "IsServiceInterface.h"
#include "IsOpenAPIInterface.h"
#include "RollingLoggerMiddleware.h"

/**
 * @class HTTPService
 * @brief Service for managing asynchronous HTTP web server and OpenAPI routes
 */
class HTTPService : public IsOpenAPIInterface
{
public:
    /**
     * @brief Connected WebSocket client entry.
     */
    struct WSClientInfo {
        uint32_t  client_id = 0;
        IPAddress ip;
        uint32_t  rx_count  = 0;  ///< messages received from this client
        uint32_t  tx_count  = 0;  ///< messages sent to this client
    };
    static constexpr uint8_t HTTP_MAX_WS_CLIENTS = 4;

    /**
     * @brief Copy connected WebSocket client list into caller-supplied array.
     * @param out       Array of at least max_count entries.
     * @param max_count Maximum entries to copy.
     * @return Number of entries filled.
     */
    uint8_t getWSClients(WSClientInfo out[], uint8_t max_count) const;

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
     * @brief Send a binary message to a WebSocket client
     * @param clientId The WebSocket client ID
     * @param data The binary data to send
     * @param len The length of the data
     * @return true if the message was sent successfully
     */
    bool sendWebSocketMessage(uint32_t clientId, const uint8_t *data, size_t len);

    /**
     * @brief Send a binary message to a WebSocket client (string version)
     * @param clientId The WebSocket client ID
     * @param message The message string (treated as binary data)
     * @return true if the message was sent successfully
     */
    bool sendWebSocketMessage(uint32_t clientId, const std::string &message);

    /**
     * @brief Process a WebSocket message by invoking registered UDP handlers
     * @param clientId The WebSocket client ID to send responses to
     * @param data The message data
     * @param len The message length
     */
    void processWebSocketMessage(uint32_t clientId, const uint8_t *data, size_t len);
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

    /**
     * @brief Reset the web server by stopping and restarting it
     * @details Used by the watchdog to recover from deadlocked connections
     * @return true if server restarted successfully
     */
    bool resetServer();

    /**
     * @brief Get timestamp of last successfully handled request
     * @return millis() value of last request
     */
    unsigned long lastRequestTime() const { return last_request_time_.load(); }

    /**
     * @brief Cleanup stale WebSocket clients
     * @details Call periodically to free resources from disconnected clients
     *          and prevent resource exhaustion under heavy load
     */
    void cleanupWebSockets();

    virtual bool startService() override;
    virtual bool stopService() override;
    std::string getServiceName() override;

protected:
    std::vector<IsOpenAPIInterface *> openAPIServices;
    bool routesRegistered = false;
    std::atomic<unsigned long> last_request_time_{0};
    AsyncWebSocket *ws = nullptr;  // WebSocket bridge to UDP
    WSClientInfo ws_clients_[HTTP_MAX_WS_CLIENTS] = {};
    uint8_t      ws_client_count_ = 0;
    RollingLoggerMiddleware *logging_middleware_ = nullptr; ///< Request logging middleware



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