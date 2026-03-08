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
#include <esp_system.h>
#include <esp_camera.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <unihiker_k10.h>
#include <pgmspace.h>
#include <FS.h>
#include <LittleFS.h>
#include <esp_partition.h>
#include <sstream>
#include <algorithm>
#include "RollingLogger.h"
#include "IsOpenAPIInterface.h"
#include "services/HTTPService.h"
#include "FlashStringHelper.h"

// Forward declarations for external variables from main.cpp
std::string master_IP;
std::string master_TOKEN;

extern UNIHIKER_K10 unihiker;


bool HTTPService::startService()
{
  if (logger)
    logger->info("Starting HTTP service...");

  // Initialize LittleFS on partition 'voice_data'
  bool mounted = LittleFS.begin(false, "/littlefs", 10, "voice_data");

  if (!mounted)
  {
    if (logger)
      logger->error("Failed to mount LittleFS 'voice_data'");
    setServiceStatus(START_FAILED);
    return false;
  }

  if (logger)
  {
    logger->info("LittleFS mounted successfully");
#ifdef VERBOSE_DEBUG
    listFilesInFS(LittleFS, "/");
#endif
  }

  // Force connection close after every response to prevent lwIP PCB table exhaustion.
  // With only 16 PCB slots and TIME_WAIT lasting 2×MSL=120s, keep-alive connections
  // from browsers can fill the table in seconds. Connection:close ensures each PCB
  // is freed promptly after the response is sent.
  DefaultHeaders::Instance().addHeader("Connection", "close");

  // AsyncWebServer starts automatically when routes are registered
  try
  {
    webserver.begin();
  }
  catch (const std::exception &e)
  {
    setServiceStatus(START_FAILED);
    if (logger)
      logger->error(std::string("Failed webserver.begin: ") + e.what());
    return false;
  }

  setServiceStatus(STARTED);

  if (logger)
    logger->info("AsyncWebServer started");

#ifdef VERBOSE_DEBUG
  logger->debug(getServiceName() + " " + getStatusString());
#endif
  return true;
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
    LittleFS.end();
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
 * @brief Read file content from LittleFS into a string
 */
std::string HTTPService::readFileToString(const char *path)
{
  if (!LittleFS.exists(path))
  {
    if (logger)
    {
      logger->warning(std::string("Template not found: ") + path);
    }
    return "";
  }

  File file = LittleFS.open(path, "r");
  if (!file || file.isDirectory())
  {
    if (logger)
    {
      logger->warning(std::string("Cannot read template: ") + path);
    }
    if (file)
      file.close();
    return "";
  }

  size_t file_size = file.size();
  std::string content;
  content.reserve(file_size);

  constexpr size_t kBufferSize = 512;
  char buffer[kBufferSize];
  while (file.available())
  {
    size_t bytes_read = file.readBytes(buffer, kBufferSize);
    content.append(buffer, bytes_read);
  }

  file.close();
  return content;
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

  if (tryServeLittleFS(request))
  {
    return;
  }

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

  // Test interface endpoint
  // webserver.on("/api/docs", HTTP_GET, [this](AsyncWebServerRequest *request)
  //              {
  //                if (!checkServiceStarted(request)) return;
  //                this->handleTestClient(request);
  //              });

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
 * @brief Attempt to serve a file from LittleFS
 */
bool HTTPService::tryServeLittleFS(AsyncWebServerRequest *request)
{
  if (!request)
  {
    return false;
  }

  String path = request->url();

#ifdef VERBOSE_DEBUG
  if (logger)
    logger->debug(std::string("Serving: ") + path.c_str());
#endif

  // Strip query parameters
  const int queryIndex = path.indexOf('?');
  if (queryIndex >= 0)
  {
    path = path.substring(0, queryIndex);
  }

  // Handle directory requests
  if (path.endsWith("/"))
  {
    path += "index.html";
  }

  // Ensure path starts with /
  if (!path.startsWith("/"))
  {
    path = "/" + path;
  }

  // Security: block path traversal
  if (path.indexOf("..") >= 0)
  {
    if (logger)
      logger->warning("Path traversal blocked!");
    return false;
  }

  // Prefer the gzip-compressed variant when the client accepts it and the
  // file exists on LittleFS.  The compress_data.py pre-build script produces
  // a <file>.gz sibling for every HTML/CSS/JS/JSON asset, so for most static
  // assets this branch will be taken, saving flash-read bandwidth and RAM.
  bool useGzip = false;
  String filePath = path;
  if (request->hasHeader("Accept-Encoding"))
  {
    String acceptEncoding = request->getHeader("Accept-Encoding")->value();
    if (acceptEncoding.indexOf("gzip") >= 0)
    {
      String gzPath = path + ".gz";
      if (LittleFS.exists(gzPath))
      {
        filePath = gzPath;
        useGzip  = true;
      }
    }
  }

  // Read file fully into memory, then close the LittleFS handle immediately.
  // This prevents holding SPI flash file descriptors open during the entire
  // TCP transmission, which blocks the async event loop and causes hangs
  // when concurrent HTTP requests arrive.
  File file = LittleFS.open(filePath, "r");
  if (!file || file.isDirectory())
  {
    if (file)
      file.close();
    // If the .gz open failed, fall back to the plain file
    if (useGzip)
    {
      useGzip  = false;
      filePath = path;
      file     = LittleFS.open(filePath, "r");
      if (!file || file.isDirectory())
      {
        if (file)
          file.close();
        return false;
      }
    }
    else
    {
      return false;
    }
  }

  size_t fileSize = file.size();
  if (fileSize == 0)
  {
    file.close();
    return false;
  }

  // Read entire file into a heap buffer, then close the flash handle.
  // We use beginResponse(code, type, uint8_t*, len) which creates an
  // AsyncProgmemResponse — this uses AsyncAbstractResponse::write_send_buffs()
  // with chunk-inflight flow control, properly sharing TCP window space across
  // concurrent connections. AsyncBasicResponse bypasses this flow control and
  // can deadlock when multiple responses compete for TCP buffer space.
  uint8_t *buf = static_cast<uint8_t *>(malloc(fileSize));
  if (!buf)
  {
    file.close();
    if (logger)
      logger->error("OOM serving " + std::string(filePath.c_str()) + " (" + std::to_string(fileSize) + "B)");
    request->send(503, RoutesConsts::mime_plain_text, "Out of memory");
    return true; // handled (with error), don't fall through to 404
  }

  size_t totalRead = file.read(buf, fileSize);
  file.close(); // LittleFS handle released — SPI flash is free for other requests

  if (totalRead != fileSize)
  {
    free(buf);
    if (logger)
      logger->error("Short read on " + std::string(filePath.c_str()));
    return false;
  }

  // AsyncProgmemResponse reads from buf pointer during TCP send via _fillBuffer(),
  // so buf must stay alive until the response is fully sent. Free on disconnect.
  // Use the original (non-.gz) path to derive the correct Content-Type.
  const String contentType = getContentTypeForPath(path);
  AsyncWebServerResponse *response = request->beginResponse(200, contentType, buf, fileSize);
  response->addHeader("Cache-Control", "max-age=600");
  if (useGzip)
  {
    response->addHeader("Content-Encoding", "gzip");
  }

  // Release heap buffer when connection closes (response fully sent or aborted)
  uint8_t *captured_buf = buf;
  request->onDisconnect([captured_buf]()
                        { free(captured_buf); });
  request->send(response);

#ifdef VERBOSE_DEBUG
  if (logger)
    logger->debug(std::string(filePath.c_str()) + " sent (" + std::to_string(totalRead) + "B from RAM" + (useGzip ? ", gzip" : "") + ")");
#endif

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

/**
 * @brief List all files in LittleFS recursively (for debugging)
 */
void HTTPService::listFilesInFS(fs::FS &fs, const char *dirname, uint8_t levels, uint8_t currentLevel)
{
  if (!logger)
    return;
  if (currentLevel > levels)
    return;

  File root = fs.open(dirname);
  if (!root || !root.isDirectory())
  {
    logger->warning("Failed to open directory: " + std::string(dirname));
    return;
  }

  File file = root.openNextFile();
  while (file)
  {
    std::string indent = "";
    for (uint8_t i = 0; i < currentLevel; i++)
      indent += "  ";

    if (file.isDirectory())
    {
      logger->info(indent + std::string(file.name()) + "/");
      if (levels > 0)
      {
        listFilesInFS(fs, file.path(), levels, currentLevel + 1);
      }
    }
    else
    {
      logger->info(indent + std::string(file.name()) + " (" + std::to_string(file.size()) + "B)");
    }
    file = root.openNextFile();
  }
}
