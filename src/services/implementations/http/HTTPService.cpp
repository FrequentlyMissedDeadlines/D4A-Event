/**
 * @file HTTPService.cpp
 * @brief Implementation for asynchronous HTTP web server integration
 * @details Exposed routes:
 *          - GET / - Home page with service dashboard
 *          - GET /api/docs - OpenAPI documentation page
 *          - GET /api/openapi.json - Dynamic OpenAPI specification
 *          Static files (HTML, CSS, JS) served from LittleFS filesystem
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <esp_system.h>
#include <esp_camera.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <unihiker_k10.h>
#include <pgmspace.h>
#include <sstream>
#include <algorithm>
#include "RollingLogger.h"
#include "RollingLoggerMiddleware.h"
#include "IsOpenAPIInterface.h"
#include "services/HTTPService.h"
#include "services/UDPService.h"
#include "FlashStringHelper.h"
#define TRACE_WEBSOCKET_MESSAGES 1
// Forward declarations for external variables from main.cpp
extern AsyncWebServer webserver;

std::string master_IP;
std::string master_TOKEN;

extern UNIHIKER_K10 unihiker;

// Forward declaration for UDP service (used for WebSocket bridge)
extern class UDPService udp_service;

// WebSocket context storage for message handling
// These are set by processWebSocketMessage and cleared after handler execution
uint32_t ws_client_id_context = 0;
AsyncWebSocket* ws_context = nullptr;

namespace
{
}

bool HTTPService::sendWebSocketMessage(uint32_t clientId, const uint8_t *data, size_t len)
{
  if (!ws || !ws->hasClient(clientId))
  {
    if (logger!=nullptr)
      logger->warning(std::string("skip send WebSocket to client ") + std::to_string(clientId));
    return false;
  }
  
  // Check if the client's queue can accept the message to prevent overflow
  // When queue is full and closeWhenFull=true (default), the connection is closed!
  if (!ws->availableForWrite(clientId))
  {
    if (logger != nullptr)
      logger->warning(std::string("WS queue full for client ") + std::to_string(clientId) + ", dropping message");
    return false;
  }
  
  // Increment tx_count for the matching client
  for (uint8_t ci = 0; ci < ws_client_count_; ++ci)
  {
    if (ws_clients_[ci].client_id == clientId)
    {
      ++ws_clients_[ci].tx_count;
      break;
    }
  }

  ws->binary(clientId, data, len);
  #ifdef TRACE_WEBSOCKET_MESSAGES
    if (logger != nullptr) {
      constexpr size_t max_hex_bytes = 32;
      const size_t show = (len < max_hex_bytes) ? len : max_hex_bytes;
      char linebuf[30 + max_hex_bytes * 3 + 4];
      int pos = snprintf(linebuf, sizeof(linebuf), "%7u ws%u Tx>%s ", millis(), ws_client_id_context, std::to_string(clientId).c_str());
      for (size_t i = 0; i < show && pos < (int)sizeof(linebuf) - 4; ++i)
        pos += snprintf(linebuf + pos, sizeof(linebuf) - pos, "%02X", data[i]);
      if (len > max_hex_bytes)
        pos += snprintf(linebuf + pos, sizeof(linebuf) - pos, "...");
      logger->info(linebuf);
    }
  #endif
  return true;
}

bool HTTPService::sendWebSocketMessage(uint32_t clientId, const std::string &message)
{
  return sendWebSocketMessage(clientId, reinterpret_cast<const uint8_t*>(message.c_str()), message.length());
}

void HTTPService::processWebSocketMessage(uint32_t clientId, const uint8_t *data, size_t len)
{
  // Store the WebSocket client context in thread-local storage
  // so that sendReply() in UDPService can route responses back to this client
  ws_client_id_context = clientId;
  ws_context = ws;
  // Convert message to string for handler
  std::string msg(reinterpret_cast<const char*>(data), len);
  #ifdef TRACE_WEBSOCKET_MESSAGES
   if (logger)
      logger->trace(std::string("Received WebSocket message from client ") + std::to_string(clientId) + ": " + msg);
  if (logger!=nullptr) {
        // Header: "[ws  ID] rx (  LENb) : " then hex dump of payload
        constexpr size_t max_hex_bytes = 32; // cap hex output to avoid large buffers
        const size_t show = (len < max_hex_bytes) ? len : max_hex_bytes;
        // Each byte = 2 hex chars + 1 space, plus header (~30 chars) + ellipsis
        char linebuf[30 + max_hex_bytes * 3 + 4];
        int pos = snprintf(linebuf, sizeof(linebuf), "%7u ws%u Rx<", millis(), ws_client_id_context);
        for (size_t i = 0; i < show && pos < (int)sizeof(linebuf) - 4; ++i)
          pos += snprintf(linebuf + pos, sizeof(linebuf) - pos, "%02X ", data[i]);
        if (len > max_hex_bytes)
          pos += snprintf(linebuf + pos, sizeof(linebuf) - pos, "...");
        logger->info(linebuf);
}
#endif
  
  // Invoke registered UDP message handlers with the WebSocket context set
  // Handlers will process the message and call sendReply()
  // sendReply() will detect the ws_maRxrker IP and route to WebSocket
  udp_service.invokeMessageHandlers(msg, remoteIP, remotePort);
  
  // Clear the context after handlers are done
  ws_client_id_context = 0;
  ws_context = nullptr;
}

/**
 * @brief WebSocket event handler for UDP bridge
 * @details Forwards WebSocket messages to UDP service and broadcasts UDP responses
 */
void handleWebSocketEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    // Client connected - no action needed, connection established
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    // Client disconnected - cleanup handled automatically
  }
  else if (type == WS_EVT_ERROR)
  {
    // Error occurred - log if logger available
  }
  else if (type == WS_EVT_PONG)
  {
    // Pong response - client is alive
  }
  else if (type == WS_EVT_DATA)
  {
    // Data received from client - forward to UDP handlers
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len)
    {
      // Complete message received - forward to UDP message processing
      // The handler will invoke registered message handlers
      // Handlers will automatically route responses back via WebSocket
      // (HTTPService is not available as a pointer here, so we need to work with it differently)
      // For now, this is a complete frame ready to process
    }
  }
}

bool HTTPService::startService()
{
  if (logger)
    logger->info("Starting HTTP service...");

  // Initialize LittleFS filesystem BEFORE serving static files
      if (! LittleFS.begin(false, "/littlefs", 10, "voice_data"))
  {
    if (logger)
      logger->error("LittleFS mount failed - run 'pio run --target uploadfs'");
    setServiceStatus(START_FAILED);
    return false;
  }
  if (logger)
    logger->info("LittleFS mounted");

  // Keep normal HTTP keep-alive behavior for static assets.
  // Browsers reuse a small number of TCP sockets, which is much cheaper than
  // forcing a brand-new connection for every CSS/JS/HTML request.

  // Initialize WebSocket bridge at /ws endpoint
  ws = new AsyncWebSocket("/ws");
  if (!ws)
  {
    if (logger)
      logger->error("Failed to allocate AsyncWebSocket");
    setServiceStatus(START_FAILED);
    return false;
  }
  
  // IMPORTANT: Disable "close connection when queue is full" behavior.
  // By default, AsyncWebSocket closes connections when the message queue fills up.
  // This causes problems when browser HTTP traffic competes with WebSocket messages.
  // Instead, we'll drop messages (handled in sendWebSocketMessage) rather than
  // closing the entire connection.
  // Note: This only affects new client connections, not the global socket.
  
  // Attach event handler as lambda that captures 'this'
  ws->onEvent([this](AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT)
    {
      if (logger) {
        linebuf = ];
        logger->info(std::string("%7u ws%u  ") + std::to_string(client->id()));   logger->info(std::string("WebSocket client connected: ") + std::to_string(client->id()));
      }
      
      
      // Disable "close on queue full" for this client - drop messages instead.
      // This prevents connection termination when browser HTTP traffic causes
      // the WebSocket queue to back up temporarily.
      client->setCloseClientOnQueueFull(false);
      
      if (ws_client_count_ < HTTP_MAX_WS_CLIENTS)
      {
        WSClientInfo &entry = ws_clients_[ws_client_count_++];
        entry.client_id = client->id();
        entry.ip        = client->remoteIP();
        entry.rx_count  = 0;
        entry.tx_count  = 0;
      }
    }
    else if (type == WS_EVT_DISCONNECT)
    {
      if (logger)
        logger->info(std::string("WebSocket client disconnected: ") + std::to_string(client->id()));
      // Remove entry by shifting remaining elements left
      for (uint8_t ci = 0; ci < ws_client_count_; ++ci)
      {
        if (ws_clients_[ci].client_id == client->id())
        {
          for (uint8_t cj = ci; cj < ws_client_count_ - 1; ++cj)
            ws_clients_[cj] = ws_clients_[cj + 1];
          --ws_client_count_;
          break;
        }
      }
    }
    else if (type == WS_EVT_ERROR)
    {
      if (logger)
      {
        // arg  = pointer to uint16_t WebSocket close code (RFC 6455, only codes > 1001)
        // data = reason string from the close frame
        // len  = reason string length
        uint16_t errCode = arg ? *reinterpret_cast<uint16_t *>(arg) : 0;
        std::string reason = (data && len) ? std::string(reinterpret_cast<const char *>(data), len) : "(none)";
        logger->error(std::string("WebSocket error on client ") + std::to_string(client->id()) +
                      " code=" + std::to_string(errCode) + " reason=" + reason);
      }
    }
    else if (type == WS_EVT_DATA)
    {
      last_request_time_.store(millis());
      AwsFrameInfo * info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len)
      {
        // Increment rx_count for the matching client
        for (uint8_t ci = 0; ci < ws_client_count_; ++ci)
        {
          if (ws_clients_[ci].client_id == client->id())
          {
            ++ws_clients_[ci].rx_count;
            break;
          }
        }
        // Complete message received - forward to UDP message processing
        this->processWebSocketMessage(client->id(), data, len);
      }
    }
  });
  
  webserver.addHandler(ws);
  
  if (logger) {
      char linebuf[40];
      snprintf(linebuf, sizeof(linebuf), "%7u ws initialized at /ws", millis());
      logger->info(linebuf);
}
    

  // Register global request-logging middleware
  logging_middleware_ = new RollingLoggerMiddleware(logger);
  logging_middleware_->onRequest([this]() { last_request_time_.store(millis()); });
  webserver.addMiddleware(logging_middleware_);

  // Serve all static files from LittleFS root with aggressive caching.
  // Cache static assets for 1 hour (3600s) to reduce HTTP requests during
  // WebSocket sessions. This prevents TCP contention between file requests
  // and real-time WebSocket traffic.
  // During development, use browser hard-refresh (Ctrl+Shift+R) to bypass cache.
  webserver.serveStatic("/", LittleFS, "/").setDefaultFile("index.html").setCacheControl("max-age=3600");
  
  if (logger)
    logger->info("Static files configured from LittleFS (cached 1h)");

  setServiceStatus(STARTED);
  if (logger)
    logger->info("HTTPService started (routes configured, server will begin after all services register)");

#ifdef VERBOSE_DEBUG
  logger->debug(getServiceName() + " " + getStatusString());
#endif
  return true;
}

bool HTTPService::startWebServer()
{
  if (!isServiceStarted())
  {
    if (logger)
      logger->error("Cannot start web server - HTTPService not initialized");
    return false;
  }

  try
  {
    // Simple test route to verify server is working
    webserver.on("/test", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(200, "text/plain", "Server is alive!");
    });
    webserver.begin();
    last_request_time_.store(millis());
    if (logger)
      logger->info("AsyncWebServer listening on port 80");
    
    // Give server time to initialize
    delay(100);
    return true;
  }
  catch (const std::exception &e)
  {
    if (logger)
      logger->error(std::string("Failed to start web server: ") + e.what());
    setServiceStatus(START_FAILED);
    return false;
  }
}

uint8_t HTTPService::getWSClients(WSClientInfo out[], uint8_t max_count) const
{
  const uint8_t n = (ws_client_count_ < max_count) ? ws_client_count_ : max_count;
  for (uint8_t i = 0; i < n; ++i)
    out[i] = ws_clients_[i];
  return n;
}

void HTTPService::cleanupWebSockets()
{
  if (ws)
  {
    // Clean up disconnected clients to free TCP resources
    // This is important when browsers have multiple tabs open
    // competing for TCP connections with WebSocket traffic
    ws->cleanupClients(HTTP_MAX_WS_CLIENTS);
  }
}

bool HTTPService::stopService()
{
  try
  {
    // Close all WebSocket connections
    if (ws)
    {
      ws->closeAll();
      // Don't delete ws as webserver owns it after addHandler()
      ws = nullptr;
    }
    
    if (isServiceStarted())
    {
      webserver.end();
      setServiceStatus(STOPPED);
    }
    
    setServiceStatus(STOPPED);

#ifdef VERBOSE_DEBUG
    logger->debug(getServiceName() + " " + getStatusString());
#endif
    return true;
  }
  catch (const std::exception &e)
  {
    setServiceStatus(STOP_FAILED);
    if (logger)
      logger->error(std::string(e.what()));
  }
  if (logger)
    logger->error(getServiceName() + " " + getStatusString());
  return false;
}

bool HTTPService::resetServer()
{
  if (logger)
    logger->warning("Watchdog: resetting web server...");

  try
  {
    webserver.end();
    vTaskDelay(pdMS_TO_TICKS(100));
    webserver.begin();
    last_request_time_.store(millis());
    setServiceStatus(STARTED);
    if (logger)
      logger->info("Web server reset complete");
    return true;
  }
  catch (const std::exception &e)
  {
    if (logger)
      logger->error(std::string("Reset failed: ") + e.what());
    return false;
  }
}

void HTTPService::registerOpenAPIService(IsOpenAPIInterface *service)
{
  if (service)
  {
    openAPIServices.push_back(service);
#ifdef VERBOSE_DEBUG
    if (logger)
      logger->debug("Registered services #" + std::to_string(openAPIServices.size()) +
                    ": " + std::to_string(service->getOpenAPIRoutes().size()));
#endif
  }
}

/**
 * Handle home page request with route listing
 */
void HTTPService::handleHomeClient(AsyncWebServerRequest *request)
{
  if (!request)
  {
    return;
  }
  // Redirect root to index.html
  request->redirect("/index.html");
}

/**
 * Handle OpenAPI spec request
 */
void HTTPService::handleOpenAPIRequest(AsyncWebServerRequest *request)
{
  if (!request)
  {
    return;
  }
  JsonDocument doc;

  // OpenAPI 3.0.0 header
  doc["openapi"] = "3.0.0";
  doc["info"]["title"] = "K10 Bot API";
  doc["info"]["version"] = "1.0.0";
  doc["info"]["description"] = "REST API for K10 Bot services";
  doc["info"]["contact"]["name"] = "aMaker club";
  doc["info"]["contact"]["url"] = "https://amadeus.atlassian.net/wiki/spaces/aMaker";
  doc["info"]["contact"]["email"] = "";
  doc["info"]["contact"]["description"] = "For support, contact Thierry.";

  JsonObject paths = doc["paths"].to<JsonObject>();
#ifdef VERBOSE_DEBUG
  if (logger)
    logger->debug("Generating OpenAPI services: " + std::to_string(openAPIServices.size()));
#endif

  // Aggregate all routes from registered OpenAPI services
  for (auto service : openAPIServices)
  {
    std::vector<OpenAPIRoute> routes = service->getOpenAPIRoutes();
#ifdef VERBOSE_DEBUG
    if (logger)
      logger->debug("Generating OpenAPI routes: " + std::to_string(routes.size()));
#endif
    for (const auto &route : routes)
    {
      // Create path object if it doesn't exist
      JsonObject pathObj = paths[route.path.c_str()].to<JsonObject>();

      // Create method object
      std::string methodLower = route.method;
      for (char &c : methodLower)
        c = tolower(c);

      JsonObject methodObj = pathObj[methodLower.c_str()].to<JsonObject>();
      methodObj["summary"] = route.summary.empty() ? route.description.c_str() : route.summary.c_str();
      methodObj["description"] = route.description.c_str();

      // Add tags
      if (!route.tags.empty())
      {
        JsonArray tagsArr = methodObj["tags"].to<JsonArray>();
        for (const auto &tag : route.tags)
        {
          tagsArr.add(tag.c_str());
        }
      }

      if (route.deprecated)
      {
        methodObj["deprecated"] = true;
      }

      // Add parameters
      if (!route.parameters.empty())
      {
        JsonArray paramsArr = methodObj["parameters"].to<JsonArray>();
        for (const auto &param : route.parameters)
        {
          JsonObject paramObj = paramsArr.add<JsonObject>();
          paramObj["name"] = param.name.c_str();
          paramObj["in"] = param.in.c_str();
          paramObj["description"] = param.description.c_str();
          paramObj["required"] = param.required;

          JsonObject schemaObj = paramObj["schema"].to<JsonObject>();
          schemaObj["type"] = param.type.c_str();

          if (!param.defaultValue.empty())
          {
            schemaObj["default"] = param.defaultValue.c_str();
          }
          if (!param.example.empty())
          {
            paramObj["example"] = param.example.c_str();
          }
        }
      }

      // Add request body if present
      if (!route.requestBody.schema.empty())
      {
        JsonObject reqBodyObj = methodObj["requestBody"].to<JsonObject>();
        reqBodyObj["description"] = route.requestBody.description.c_str();
        reqBodyObj["required"] = route.requestBody.required;

        JsonObject contentObj = reqBodyObj["content"].to<JsonObject>();
        JsonObject mediaTypeObj = contentObj[route.requestBody.contentType.c_str()].to<JsonObject>();

        if (!route.requestBody.schema.empty())
        {
          mediaTypeObj["schema"] = serialized(route.requestBody.schema.c_str());
        }

        if (!route.requestBody.example.empty())
        {
          mediaTypeObj["example"] = serialized(route.requestBody.example.c_str());
        }
      }

      // Add responses
      JsonObject responses = methodObj["responses"].to<JsonObject>();
      if (!route.responses.empty())
      {
        for (const auto &resp : route.responses)
        {
          char statusCodeStr[4];
          snprintf(statusCodeStr, sizeof(statusCodeStr), "%d", resp.statusCode);

          JsonObject respObj = responses[statusCodeStr].to<JsonObject>();
          respObj["description"] = resp.description.c_str();

          if (!resp.schema.empty())
          {
            JsonObject contentObj = respObj["content"].to<JsonObject>();
            JsonObject mediaTypeObj = contentObj[resp.contentType.c_str()].to<JsonObject>();
            mediaTypeObj["schema"] = serialized(resp.schema.c_str());

            if (!resp.example.empty())
            {
              mediaTypeObj["example"] = serialized(resp.example.c_str());
            }
          }
        }
      }
      else
      {
        JsonObject response200 = responses["200"].to<JsonObject>();
        response200["description"] = "Successful response";
      }

      if (route.requiresAuth)
      {
        JsonArray securityArr = methodObj["security"].to<JsonArray>();
        JsonObject secObj = securityArr.add<JsonObject>();
        secObj["bearerAuth"] = JsonArray();
      }
    }
  }

  // Add security schemes if any service requires auth
  bool hasAuthServices = false;
  for (auto service : openAPIServices)
  {
    std::vector<OpenAPIRoute> routes = service->getOpenAPIRoutes();
    for (const auto &route : routes)
    {
      if (route.requiresAuth)
      {
        hasAuthServices = true;
        break;
      }
    }
    if (hasAuthServices)
      break;
  }

  if (hasAuthServices)
  {
    JsonObject components = doc["components"].to<JsonObject>();
    JsonObject secSchemes = components["securitySchemes"].to<JsonObject>();
    JsonObject bearerAuth = secSchemes["bearerAuth"].to<JsonObject>();
    bearerAuth["type"] = "http";
    bearerAuth["scheme"] = "bearer";
    bearerAuth["bearerFormat"] = "token";
  }

  String output;
  serializeJson(doc, output);
  request->send(200, RoutesConsts::mime_json, output.c_str());
}

/**
 * @brief Handle requests without a registered route
 */
void HTTPService::handleNotFoundClient(AsyncWebServerRequest *request)
{
  if (!request)
  {
    return;
  }
  String path = request->url();


  if (logger)
    logger->warning(std::string(path.c_str()) + " 404");
  request->send(404, RoutesConsts::mime_plain_text, (path + ": Not Found").c_str());
}

bool HTTPService::registerRoutes()
{
  // Register OpenAPI routes for HTTPService
  std::vector<OpenAPIResponse> openApiResponses;
  OpenAPIResponse openApiOk(200, "OpenAPI specification retrieved successfully");
  openApiOk.schema = "{\"type\":\"object\",\"properties\":{\"openapi\":{\"type\":\"string\"},\"info\":{\"type\":\"object\"},\"paths\":{\"type\":\"object\"}}}";
  openApiOk.example = "{\"openapi\":\"3.0.0\",\"info\":{\"title\":\"K10 Bot API\",\"version\":\"1.0.0\"},\"paths\":{}}";
  openApiResponses.push_back(openApiOk);
  openApiResponses.push_back(createServiceNotStartedResponse());

  std::string path = RoutesConsts::path_openapi;
  registerOpenAPIRoute(OpenAPIRoute(
      path.c_str(),
      RoutesConsts::method_get,
      "Get OpenAPI 3.0.0 specification for all registered services",
      "OpenAPI",
      false, {}, openApiResponses));

  webserver.on("/", HTTP_GET, [this](AsyncWebServerRequest *request)
               { 
                 if (!checkServiceStarted(request)) return;
                 this->handleHomeClient(request); });

  // Lightweight health-check endpoint — no JSON, no flash I/O
  webserver.on("/ping", HTTP_GET, [this](AsyncWebServerRequest *request)
               {
                 last_request_time_.store(millis());
                 request->send(200, RoutesConsts::mime_plain_text, "pong"); });

  
  // OpenAPI specification endpoint
  webserver.on(path.c_str(), HTTP_GET, [this](AsyncWebServerRequest *request)
               { 
                 if (!checkServiceStarted(request)) return;
                 this->handleOpenAPIRequest(request); });

  webserver.onNotFound([this](AsyncWebServerRequest *request)
                       { this->handleNotFoundClient(request); });

  registerServiceStatusRoute(this);
  registerSettingsRoutes(this);

  return true;
}


/**
 * @brief Resolve MIME type based on file extension
 */
String HTTPService::getContentTypeForPath(const String &path)
{
  struct MimeMap
  {
    const char *extension;
    const char *mimeType;
  };

  static constexpr MimeMap mime_types[] PROGMEM = {
      {".html", "text/html"},
      {".htm", "text/html"},
      {".css", "text/css"},
      {".js", "application/javascript"},
      {".json", "application/json"},
      {".png", "image/png"},
      {".jpg", "image/jpeg"},
      {".jpeg", "image/jpeg"},
      {".svg", "image/svg+xml"},
      {".ico", "image/x-icon"},
      {".txt", "text/plain"},
      {".map", "application/json"},
      {".woff", "font/woff"},
      {".woff2", "font/woff2"},
      {".ttf", "font/ttf"},
      {".otf", "font/otf"},
      {".wasm", "application/wasm"}};

  for (const auto &entry : mime_types)
  {
    if (path.endsWith(entry.extension))
    {
      return String(entry.mimeType);
    }
  }

  return String("application/octet-stream");
}

std::string HTTPService::getServiceName()
{
  return "HTTP Service";
}

std::string HTTPService::getServiceSubPath()
{
  return "http/v1";
}
