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
#include "IsOpenAPIInterface.h"
#include "services/HTTPService.h"
#include "FlashStringHelper.h"

// Forward declarations for external variables from main.cpp
extern AsyncWebServer webserver;
std::string master_IP;
std::string master_TOKEN;

extern UNIHIKER_K10 unihiker;


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

  // Force connection close after every response to prevent lwIP PCB table exhaustion.
  // With only 16 PCB slots and TIME_WAIT lasting 2×MSL=120s, keep-alive connections
  // from browsers can fill the table in seconds. Connection:close ensures each PCB
  // is freed promptly after the response is sent.
  DefaultHeaders::Instance().addHeader("Connection", "close");

  // Serve all static files from LittleFS root
  webserver.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  
  if (logger)
    logger->info("Static files configured from LittleFS");

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

bool HTTPService::stopService()
{
  try
  {
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
  logRequest(request);
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
  logRequest(request);

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
  logRequest(request);

  String path = request->url();


  if (logger)
    logger->warning(std::string(path.c_str()) + " 404");
  request->send(404, RoutesConsts::mime_plain_text, (path + ": Not Found").c_str());
}

/**
 * @brief Log incoming HTTP request method and URI
 */
void HTTPService::logRequest(AsyncWebServerRequest *request)
{
  if (request)
  {
    // Update watchdog timestamp — proves the async event loop is still serving requests
    last_request_time_.store(millis());

    if (logger)
    {
    std::string method_str;
    switch (request->method())
    {
    case HTTP_GET:
      method_str = "GET";
      break;
    case HTTP_POST:
      method_str = "POST";
      break;
    case HTTP_PUT:
      method_str = "PUT";
      break;
    case HTTP_DELETE:
      method_str = "DELETE";
      break;
    case HTTP_PATCH:
      method_str = "PATCH";
      break;
    case HTTP_HEAD:
      method_str = "HEAD";
      break;
    case HTTP_OPTIONS:
      method_str = "OPTIONS";
      break;
    default:
      method_str = "UNKNOWN";
      break;
    }

    std::string log_msg = method_str + " " + std::string(request->url().c_str());
    logger->info(log_msg);
    }
  }
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