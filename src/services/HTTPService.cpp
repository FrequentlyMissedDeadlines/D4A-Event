#include "HTTPService.h"
#include <Arduino.h>
#include <esp_system.h>
#include <esp_camera.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <unihiker_k10.h>
#include <pgmspace.h>
#include <LittleFS.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <esp_partition.h>
#include <sstream>
#include "RollingLogger.h"
#include "IsOpenAPIInterface.h"
// Forward declarations for external variables from main.cpp
std::string master_IP;
std::string master_TOKEN;

extern UNIHIKER_K10 unihiker;



bool HTTPService::initializeService()
{

  return true;
}

bool HTTPService::startService()
{
  if (logger)
    logger->info("Starting HTTP service...");

  // Register base routes but don't start server yet (services need to register first)
  webserver.begin();
  serverRunning = true;
  if (logger)
    logger->info("WebServer started");

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
    return true;
  }
  catch (const std::exception &e)
  {
    logger->error(std::string(e.what()));
  }
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

  // Use stringstream for efficient string building (avoids O(n²) concatenation)
  std::stringstream ss;
  ss << "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>K10 Bot</title></head><body><h1>Routes</h1><h2>List</h2><ul>\n";

  for (auto service : openAPIServices)
  {
    std::vector<OpenAPIRoute> routes = service->getOpenAPIRoutes();
    for (const auto &route : routes)
    {
      ss << "<li><b>" << route.method << "</b> <a href=\"" << route.path
         << "\">" << route.path << "</a>:<br>" << route.description << "</li>\n";
    }
  }
  ss << "</ul><h2>Test</h2><a href=\"/test\">Dynamic Test Interface</a></body></html>";

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

  std::stringstream ss;
  
  // HTML header with styles and JavaScript
  ss << R"(<!DOCTYPE html>
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
           << "\" placeholder=\"" << param.description;
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
  ss << R"(  </div>
  <script>
    function testRoute(formId, method, path) {
      const form = document.getElementById('form' + formId);
      const formData = new FormData(form);
      const responseArea = document.getElementById('response' + formId);
      const responseContent = document.getElementById('responseContent' + formId);
      
      // Build URL with parameters
      let url = path;
      const params = new URLSearchParams();
      
      for (let [key, value] of formData.entries()) {
        if (value) {
          // Check if parameter is in path
          if (path.includes('{' + key + '}')) {
            url = url.replace('{' + key + '}', encodeURIComponent(value));
          } else {
            params.append(key, value);
          }
        }
      }
      
      // Add query parameters for GET or append to URL
      if (method === 'GET' && params.toString()) {
        url += '?' + params.toString();
      }
      
      responseContent.innerHTML = 'Loading...';
      responseArea.style.display = 'block';
      
      const fetchOptions = {
        method: method,
        headers: {
          'Content-Type': 'application/json'
        }
      };
      
      // For POST/PUT, add body
      if (method !== 'GET' && params.toString()) {
        const body = {};
        for (let [key, value] of params.entries()) {
          body[key] = value;
        }
        fetchOptions.body = JSON.stringify(body);
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

  JsonDocument doc;

  // OpenAPI 3.0.0 header
  doc["openapi"] = "3.0.0";
  doc["info"]["title"] = "K10 Bot API";
  doc["info"]["version"] = "1.0.0";
  doc["info"]["description"] = "REST API for K10 Bot services";

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
  webserver->send(200, RoutesConsts::kMimeJSON, output.c_str());
}

void HTTPService::handleClient(WebServer *webserver)
{
  if (webserver)
  {
    webserver->handleClient();
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

  std::string path="/api/openapi.json";
  registerOpenAPIRoute(OpenAPIRoute(
      path.c_str(),
      "GET",
      "Get OpenAPI 3.0.0 specification for all registered services including paths, parameters, request bodies, and response schemas",
      "OpenAPI",
      false, {}, openApiResponses));

  
  webserver.on("/", HTTP_GET, [this]()
                { this->handleHomeClient(&webserver); });

  // Test interface endpoint
  webserver.on("/test", HTTP_GET, [this]()
                { this->handleTestClient(&webserver); });

  // OpenAPI specification endpoint
  webserver.on(path.c_str(), HTTP_GET, [this]()
                { this->handleOpenAPIRequest(&webserver); });

  
  return true;
}

std::string HTTPService::getPath(const std::string& finalpathstring)
{
  if (baseServicePath.empty()) {
    baseServicePath = std::string(RoutesConsts::kPathAPI) + getName() + "/";
  }
  return baseServicePath + finalpathstring;
}

std::string HTTPService::getName()
{
  return "http/v1";
}