#include "HTTPService.h"
/**
 * @file HTTPService.cpp
 * @brief Implementation for HTTP web server integration
 * @details Exposed routes:
 *          - GET / - Home page with service dashboard
 *          - GET /api/docs - OpenAPI documentation page
 *          - GET /docs/static_openapi.json - Static OpenAPI schema (serves as fallback)
 * 
 */

#include <Arduino.h>
#include <esp_system.h>
#include <esp_camera.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <unihiker_k10.h>
#include <pgmspace.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <esp_partition.h>
#include <sstream>
#include <algorithm>
#include "RollingLogger.h"
#include "IsOpenAPIInterface.h"

// Forward declarations for external variables from main.cpp
std::string master_IP;
std::string master_TOKEN;

extern UNIHIKER_K10 unihiker;

// PROGMEM HTML/CSS constants for home page
static const char HOME_HTML_HEAD[] PROGMEM = R"(<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>K10 Bot Control Panel</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
    h1 { color: #333; }
    h2 { color: #555; margin-top: 30px; }
    .panel { background: white; padding: 20px; margin: 15px 0; border-radius: 5px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
    .service-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(250px, 1fr)); gap: 15px; margin: 20px 0; }
    .service-card { background: white; padding: 15px; border-radius: 5px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); text-decoration: none; color: #333; display: block; transition: transform 0.2s; }
    .service-card:hover { transform: translateY(-2px); box-shadow: 0 4px 8px rgba(0,0,0,0.2); }
    .service-title { font-weight: bold; font-size: 16px; margin-bottom: 8px; color: #4CAF50; }
    .service-desc { font-size: 13px; color: #666; }
    .route-list { list-style: none; padding: 0; }
    .route-list li { padding: 8px 0; border-bottom: 1px solid #eee; }
    .route-list li:last-child { border-bottom: none; }
    .method { display: inline-block; padding: 3px 6px; border-radius: 3px; font-weight: bold; color: white; margin-right: 8px; font-size: 11px; }
    .method-GET { background: #61affe; }
    .method-POST { background: #49cc90; }
    .method-PUT { background: #fca130; }
    .method-DELETE { background: #f93e3e; }
    .path { font-family: monospace; font-size: 13px; color: #333; }
    .btn { display: inline-block; padding: 10px 20px; background: #4CAF50; color: white; text-decoration: none; border-radius: 3px; margin: 5px; }
    .btn:hover { background: #45a049; }
  </style>
</head>
<body>
  <h1>K10 Bot Control Panel</h1>
  
  <div class="panel">
    <h2>Service Interfaces</h2>
    <div class="service-grid">
      <a href="/ServoService.html" class="service-card">
        <div class="service-title">Servo Control</div>
        <div class="service-desc">Control servo motors (channels 0-7)</div>
      </a>
      <a href="/K10webcam.html" class="service-card">
        <div class="service-title">Webcam</div>
        <div class="service-desc">View camera feed and capture images</div>
      </a>
      <a href="/HTTPService.html" class="service-card">
        <div class="service-title">HTTP Service</div>
        <div class="service-desc">Configure HTTP service settings</div>
      </a>
    </div>
  </div>

  <div class="panel">
    <h2>Developer Tools</h2>
    <a href="/api/docs" class="btn">API Test Interface</a>
    <a href="/api/openapi.json" class="btn">OpenAPI Spec</a>
  </div>

  <div class="panel">
    <h2>API Routes</h2>
    <ul class="route-list">
)";

static const char HOME_HTML_ROUTE_ITEM[] PROGMEM = "      <li>\n        <span class=\"method method-%s\">%s</span>\n        <a href=\"%s\" class=\"path\">%s</a>\n        <div style=\"margin-left: 50px; color: #666; font-size: 13px;\">%s</div>\n      </li>\n";

static const char HOME_HTML_TAIL[] PROGMEM = "    </ul>\n  </div>\n</body>\n</html>\n";

// PROGMEM HTML/CSS constants for test page
static const char TEST_HTML_HEAD[] PROGMEM = R"(<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>K10 Bot API Test</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
    h1 { color: #333; }
    .route-container { background: white; margin: 15px 0; padding: 15px; border-radius: 5px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
    .route-header { margin-bottom: 10px; }
    .method { display: inline-block; padding: 4px 8px; border-radius: 3px; font-weight: bold; color: white; margin-right: 10px; }
    .method-GET { background: #61affe; }
    .method-POST { background: #49cc90; }
    .method-PUT { background: #fca130; }
    .method-DELETE { background: #f93e3e; }
    .path { font-family: monospace; font-size: 14px; }
    .description { color: #666; margin: 5px 0; }
    .param-group { margin: 10px 0; }
    .param-label { display: block; margin: 5px 0 3px; font-weight: bold; font-size: 13px; }
    .param-input { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 3px; box-sizing: border-box; }
    .btn { padding: 10px 20px; border: none; border-radius: 3px; cursor: pointer; font-weight: bold; margin-right: 10px; }
    .btn-primary { background: #4CAF50; color: white; }
    .btn-primary:hover { background: #45a049; }
    .response-area { margin-top: 15px; display: none; }
    .response-content { background: #f9f9f9; border: 1px solid #ddd; border-radius: 3px; padding: 10px; max-height: 300px; overflow: auto; font-family: monospace; font-size: 12px; white-space: pre-wrap; }
    .error { color: #f93e3e; }
    .success { color: #49cc90; }
  </style>
</head>
<body>
  <h1>K10 Bot API Test Interface</h1>
  <div id="routes-container">
)";

static const char TEST_HTML_TAIL[] PROGMEM = R"(  </div>
  <script>
    function testRoute(formId, method, path) {
      const form = document.getElementById('form' + formId);
      const inputs = form.querySelectorAll('input[name]');
      const responseArea = document.getElementById('response' + formId);
      const responseContent = document.getElementById('responseContent' + formId);
      
      // Build URL and separate parameters by type
      let url = path;
      const queryParams = new URLSearchParams();
      const bodyParams = {};
      const headers = {
        'Content-Type': 'application/json'
      };
      
      // Process each input based on its parameter type
      inputs.forEach(input => {
        const name = input.name;
        const value = input.value;
        const paramIn = input.getAttribute('data-param-in');
        
        if (value) {
          if (paramIn === 'path') {
            // Replace path parameter
            url = url.replace('{' + name + '}', encodeURIComponent(value));
          } else if (paramIn === 'query') {
            // Add to query string
            queryParams.append(name, value);
          } else if (paramIn === 'header') {
            // Add to headers
            headers[name] = value;
          } else {
            // Default: for GET use query, for POST/PUT use body
            if (method === 'GET') {
              queryParams.append(name, value);
            } else {
              bodyParams[name] = value;
            }
          }
        }
      });
      
      // Append query parameters to URL
      if (queryParams.toString()) {
        url += (url.includes('?') ? '&' : '?') + queryParams.toString();
      }
      
      responseContent.innerHTML = 'Loading...';
      responseArea.style.display = 'block';
      
      const fetchOptions = {
        method: method,
        headers: headers
      };
      
      // Add body for POST/PUT/PATCH methods if there are body parameters
      if (method !== 'GET' && Object.keys(bodyParams).length > 0) {
        fetchOptions.body = JSON.stringify(bodyParams);
      }
      
      fetch(url, fetchOptions)
        .then(response => {
          const contentType = response.headers.get('content-type');
          if (contentType && contentType.includes('application/json')) {
            return response.json().then(data => ({
              status: response.status,
              data: JSON.stringify(data, null, 2),
              ok: response.ok
            }));
          } else {
            return response.text().then(text => ({
              status: response.status,
              data: text,
              ok: response.ok
            }));
          }
        })
        .then(result => {
          const className = result.ok ? 'success' : 'error';
          responseContent.innerHTML = '<span class="' + className + '">Status: ' + result.status + '</span>\n\n' + result.data;
        })
        .catch(error => {
          responseContent.innerHTML = '<span class="error">Error: ' + error.message + '</span>';
        });
      
      return false;
    }
  </script>
</body>
</html>
)";

bool HTTPService::initializeService()
{
#ifdef DEBUG
    logger->debug(getServiceName() + " " + fpstr_to_string(FPSTR(ServiceInterfaceConsts::msg_initialize_done)));
#endif
  return true;
}

bool HTTPService::startService()
{
  if (logger)
    logger->info("Starting HTTP service...");

  // Initialize LittleFS on partition 'model' (from large_spiffs_16MB.csv)
  // The partition table defines: model, data, spiffs, 0x510000, 4563k
  // PlatformIO uploadfs uploads to this partition when board_build.filesystem = littlefs
  bool mounted = LittleFS.begin(false, "/littlefs", 10, "voice_data") ;
  
  if (!mounted)
  {
      if (logger) logger->error("Failed to mount LittleFS  'voice_data'");
      return false;
  }
  
  if (logger)
  {
    logger->info("LittleFS mounted successfully");
    // Debug: List files in LittleFS
    listFilesInFS(LittleFS, "/");
  }

  // Register base routes but don't start server yet (services need to register first)
  webserver.begin();
  serverRunning = true;
  if (logger)
    logger->info("WebServer started");
  
  
#ifdef DEBUG
  logger->debug(getServiceName() + " " + fpstr_to_string(FPSTR(ServiceInterfaceConsts::msg_start_done)));
#endif
  return true;
}

bool HTTPService::stopService()
{
  try
  {
    if (serverRunning)
    {
      webserver.stop();
      serverRunning = false;
    }
    LittleFS.end();
#ifdef DEBUG
    logger->debug(getServiceName() + " " + fpstr_to_string(FPSTR(ServiceInterfaceConsts::msg_stop_done)));
#endif
    return true;
  }
  catch (const std::exception &e)
  {
    logger->error(std::string(e.what()));
  }
  logger->error(getServiceName() + " " + fpstr_to_string(FPSTR(ServiceInterfaceConsts::msg_stop_failed)));
  return false;
}

void HTTPService::registerOpenAPIService(IsOpenAPIInterface *service)
{
  if (service)
  {
    openAPIServices.push_back(service);
#ifdef DEBUG
    if (logger)
      logger->debug("Registered services #" + std::to_string(openAPIServices.size()) + ": " + std::to_string(service->getOpenAPIRoutes().size()));
#endif
  }
}

/**
 * Handle home page request with route listing
 */
void HTTPService::handleHomeClient(WebServer *webserver)
{
  if (!webserver)
  {
    return;
  }
  logRequest(webserver);

  // Collect all routes from all services
  std::vector<OpenAPIRoute> allRoutes;
  for (auto service : openAPIServices)
  {
    std::vector<OpenAPIRoute> routes = service->getOpenAPIRoutes();
    allRoutes.insert(allRoutes.end(), routes.begin(), routes.end());
  }

  // Sort routes by path
  std::sort(allRoutes.begin(), allRoutes.end(),
            [](const OpenAPIRoute &a, const OpenAPIRoute &b)
            {
              return a.path < b.path;
            });

  // Use stringstream for efficient string building (avoids O(n²) concatenation)
  std::stringstream ss;
  ss << HOME_HTML_HEAD;

  for (const auto &route : allRoutes)
  {
    char buffer[512];
    snprintf_P(buffer, sizeof(buffer), HOME_HTML_ROUTE_ITEM,
               route.method.c_str(), route.method.c_str(),
               route.path.c_str(), route.path.c_str(),
               route.description.c_str());
    ss << buffer;
  }

  ss << HOME_HTML_TAIL;

  std::string allroutes = ss.str();
  webserver->send_P(200, "text/html; charset=utf-8", allroutes.c_str());
}

/**
 * Handle test page request with interactive forms for all routes
 */
void HTTPService::handleTestClient(WebServer *webserver)
{
  if (!webserver)
  {
    return;
  }
  logRequest(webserver);

  std::stringstream ss;

  // HTML header with styles and JavaScript
  ss << TEST_HTML_HEAD;

  // Generate forms for each route
  int formId = 0;
  for (auto service : openAPIServices)
  {
    std::vector<OpenAPIRoute> routes = service->getOpenAPIRoutes();
    for (const auto &route : routes)
    {
      ss << "    <div class=\"route-container\">\n";
      ss << "      <div class=\"route-header\">\n";
      ss << "        <span class=\"method method-" << route.method << "\">" << route.method << "</span>\n";
      ss << "        <span class=\"path\">" << route.path << "</span>\n";
      ss << "      </div>\n";
      ss << "      <div class=\"description\">" << route.description << "</div>\n";
      ss << "      <form id=\"form" << formId << "\" onsubmit=\"return testRoute(" << formId << ", '"
         << route.method << "', '" << route.path << "')\">\n";

      // Add input fields for parameters
      for (const auto &param : route.parameters)
      {
        ss << "        <div class=\"param-group\">\n";
        ss << "          <label class=\"param-label\">" << param.name;
        if (param.required)
          ss << " <span style=\"color:red;\">*</span>";
        ss << " (" << param.in << ", " << param.type << ")</label>\n";
        ss << "          <input type=\"text\" class=\"param-input\" name=\"" << param.name
           << "\" data-param-in=\"" << param.in << "\" placeholder=\"" << param.description;
        if (!param.example.empty())
          ss << " (e.g., " << param.example << ")";
        ss << "\"";
        if (!param.defaultValue.empty())
          ss << " value=\"" << param.defaultValue << "\"";
        if (param.required)
          ss << " required";
        ss << ">\n";
        ss << "        </div>\n";
      }

      ss << "        <button type=\"submit\" class=\"btn btn-primary\">Send " << route.method << " Request</button>\n";
      ss << "      </form>\n";
      ss << "      <div class=\"response-area\" id=\"response" << formId << "\">\n";
      ss << "        <h4>Response:</h4>\n";
      ss << "        <div class=\"response-content\" id=\"responseContent" << formId << "\"></div>\n";
      ss << "      </div>\n";
      ss << "    </div>\n";

      formId++;
    }
  }

  // JavaScript for handling form submissions
  ss << TEST_HTML_TAIL;

  std::string htmlContent = ss.str();
  webserver->send_P(200, "text/html; charset=utf-8", htmlContent.c_str());
}

/**
 * Handle OpenAPI spec request
 */
void HTTPService::handleOpenAPIRequest(WebServer *webserver)
{
  if (!webserver)
  {
    return;
  }
  logRequest(webserver);

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
#ifdef DEBUG
  logger->debug("Generating OpenAPI services: " + std::to_string(openAPIServices.size()));
#endif
  // Aggregate all routes from registered OpenAPI services
  for (auto service : openAPIServices)
  {
    std::vector<OpenAPIRoute> routes = service->getOpenAPIRoutes();
#ifdef DEBUG
    logger->debug("Generating OpenAPI routes:  " + std::to_string(routes.size()));
#endif
    for (const auto &route : routes)
    {
      // Create path object if it doesn't exist
      JsonObject pathObj = paths[route.path.c_str()].to<JsonObject>();

      // Create method object
      std::string methodLower = route.method;
      // Convert to lowercase for OpenAPI spec
      for (char &c : methodLower)
        c = tolower(c);

      JsonObject methodObj = pathObj[methodLower.c_str()].to<JsonObject>();
      methodObj["summary"] = route.summary.empty() ? route.description.c_str() : route.summary.c_str();
      methodObj["description"] = route.description.c_str();

      // Add tags if available
      if (!route.tags.empty())
      {
        JsonArray tagsArr = methodObj["tags"].to<JsonArray>();
        for (const auto &tag : route.tags)
        {
          tagsArr.add(tag.c_str());
        }
      }

      // Add deprecated flag if set
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

        // Parse and add schema if it's a JSON string
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
        // Fallback to basic response
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
  webserver->send(200, RoutesConsts::mime_json, output.c_str());
}

/**LittleFS
 * @brief Handle requests without a registered route
 * @param webserver Pointer to WebServer instance
 */
void HTTPService::handleNotFoundClient(WebServer *webserver)
{
  if (!webserver)
  {
    return;
  }
  logRequest(webserver);
  String path = webserver->uri();
  const int queryIndex = path.indexOf('?');
  if (tryServeLittleFS(webserver))
  {
    return;
  }
  
   
  logger->warning( std::string(path.c_str())+" 404 ");
  webserver->send(404, RoutesConsts::mime_plain_text, (path + ": Not Found").c_str());
}

/**
 * @brief Convert HTTPMethod enum to string representation
 * @param method HTTP method enum value
 * @return Method name as C-string
 */
static const char *http_method_to_string(HTTPMethod method)
{
  switch (method)
  {
    case HTTP_GET:     return "GET";
    case HTTP_POST:    return "POST";
    case HTTP_PUT:     return "PUT";
    case HTTP_DELETE:  return "DELETE";
    case HTTP_PATCH:   return "PATCH";
    case HTTP_HEAD:    return "HEAD";
    case HTTP_OPTIONS: return "OPTIONS";
    default:           return "UNKNOWN";
  }
}

void HTTPService::handleClient(WebServer *webserver)
{
  if (webserver)
  {
    webserver->handleClient();
  }
}

/**
 * @brief Log incoming HTTP request method and URI
 * @param webserver Pointer to WebServer instance
 */
void HTTPService::logRequest(WebServer *webserver)
{
  if (webserver && logger)
  {
    std::string log_msg = std::string(http_method_to_string(webserver->method())) +
                          " " + webserver->uri().c_str();
    logger->info(log_msg);
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

  std::string path = RoutesConsts::path_openapi;
  registerOpenAPIRoute(OpenAPIRoute(
      path.c_str(),
      RoutesConsts::method_get,
      "Get OpenAPI 3.0.0 specification for all registered services including paths, parameters, request bodies, and response schemas",
      "OpenAPI",
      false, {}, openApiResponses));

  webserver.on("/", HTTP_GET, [this]()
               { this->handleHomeClient(&webserver); });

  // Test interface endpoint
  webserver.on("/api/docs", HTTP_GET, [this]()
               { this->handleTestClient(&webserver); });

  // OpenAPI specification endpoint
  webserver.on(path.c_str(), HTTP_GET, [this]()
               { this->handleOpenAPIRequest(&webserver); });

  webserver.onNotFound([this]()
                       { this->handleNotFoundClient(&webserver); });

  registerSettingsRoutes("OpenAPI", this);

  return true;
}

/**
 * @brief Attempt to serve a file from LittleFS for the current request
 * @param webserver Pointer to WebServer instance
 * @return true if a file was found and sent
 */
bool HTTPService::tryServeLittleFS(WebServer *webserver)
{
  if (!webserver)
  {
    return false;
  }

  String path = webserver->uri();
  #ifdef DEBUG
    
  logger->debug(std::string("read ") + path.c_str() );
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
    {
      logger->warning("Traversal path blocked!" );
    }
    return false;
  }

  // Check if file exists in LittleFS
  if (!LittleFS.exists(path))
  {
    if (logger)
    {
      logger->warning(std::string(path.c_str()) + " not found.");
    }
    return false;
  }

  // Open and validate file
  File file = LittleFS.open(path, "r");
  if (!file || file.isDirectory())
  {
    if (logger)
    {
      logger->warning(std::string(path.c_str()) + " not readable.");
    }
    if (file)
      file.close();
    return false;
  }

  size_t fileSize = file.size();
  if (fileSize == 0)
  {
    if (logger)
    {
      logger->warning(std::string(path.c_str()) + " is empty.");
    }
    file.close();
    return false;
  }

  // Get content type and set response headers
  const String contentType = getContentTypeForPath(path);
  webserver->setContentLength(fileSize);
  webserver->send(200, contentType.c_str(), "");

  // Stream file in chunks
  constexpr size_t kStreamChunkSize = 2048;
  uint8_t buffer[kStreamChunkSize];
  size_t bytesRead = 0;
  
  while (file.available() && bytesRead < fileSize)
  {
    size_t toRead = (fileSize - bytesRead > kStreamChunkSize) ? kStreamChunkSize : (fileSize - bytesRead);
    size_t readBytes = file.read(buffer, toRead);
    
    if (readBytes == 0)
    {
      break;
    }
    
    webserver->sendContent(reinterpret_cast<const char *>(buffer), readBytes);
    bytesRead += readBytes;
    delay(0); // Feed watchdog
  }

  file.close();
  #ifdef DEBUG
  if (logger)
  {
    logger->debug(std::string(path.c_str()) + " sent.");
  }
  #endif

  return true;
}

/**
 * @brief Resolve MIME type based on file extension
 * @param path Request path
 * @return MIME type string
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

std::string HTTPService::getPath(const std::string &finalpathstring)
{
  if (baseServicePath.empty())
  {
    baseServicePath = std::string(RoutesConsts::path_api) + getServiceSubPath() + "/";
  }
  return baseServicePath + finalpathstring;
}

std::string HTTPService::getServiceName()
{
  return "HTTP Service";
}

std::string HTTPService::getServiceSubPath()
{
  return "http/v1";
}

bool HTTPService::saveSettings()
{
  // To be implemented if needed
  return true;
}

bool HTTPService::loadSettings()
{
  // To be implemented if needed
  return true;
}

/**
 * @brief List all files in LittleFS recursively (for debugging)
 * @param fs Filesystem to list
 * @param dirname Directory to start from
 * @param level Current recursion level
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
      indent += " ";

    if (file.isDirectory())
    {
      logger->info(indent +  std::string(file.name())+"/");
      if (levels > 0)
      {
        listFilesInFS(fs, file.path(), levels , currentLevel + 1);
      }
    }
    else
    {
      logger->info(indent + "" + std::string(file.name()) + " (" + std::to_string(file.size()) + "B)");
    }
    file = root.openNextFile();
  }
}