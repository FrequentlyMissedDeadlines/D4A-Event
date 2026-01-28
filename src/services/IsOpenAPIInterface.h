#pragma once

#include <vector>
#include <string>
#include <set>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "IsServiceInterface.h"

/**
 * @file IsOpenAPIInterface.h
 * @brief Interface for services that provide OpenAPI specification data and HTTP route registration
 * @details Classes implementing this interface can contribute to the aggregated OpenAPI spec
 *          and register their HTTP routes with a WebServer instance.
 */

// Common constants for OpenAPI routes (stored in PROGMEM to save RAM)
namespace RoutesConsts
{
    constexpr const char Result[] PROGMEM = "result";
    constexpr const char ResultOk[] PROGMEM = "ok";
    constexpr const char ResultErr[] PROGMEM = "err";
    constexpr const char PathAPI[] PROGMEM = "/api/";
    constexpr const char PathOpenAPI[] PROGMEM = "/api/openapi.json";
    constexpr const char Message[] PROGMEM = "message";
    constexpr const char MimeJSON[] PROGMEM = "application/json";
    constexpr const char MimePlainText[] PROGMEM = "text/plain";
    constexpr const char MsgInvalidParams[] PROGMEM = "Invalid or missing parameter(s).";
    constexpr const char MsgInvalidValues[] PROGMEM = "Invalid parameter(s) values.";
    constexpr const char ParamDomain[] PROGMEM = "domain";
    constexpr const char ParamKey[] PROGMEM = "key";
    constexpr const char ParamValue[] PROGMEM = "value";
    constexpr const char FieldError[] PROGMEM = "error";
    constexpr const char FieldStatus[] PROGMEM = "status";
    constexpr const char StatusReady[] PROGMEM = "ready";
    constexpr const char StatusNotInitialized[] PROGMEM = "not_initialized";
    constexpr const char StatusSensorError[] PROGMEM = "sensor_error";
    constexpr const char MethodGET[] PROGMEM = "GET";
    constexpr const char MethodPOST[] PROGMEM = "POST";
    constexpr const char MethodPUT[] PROGMEM = "PUT";
    constexpr const char MethodDELETE[] PROGMEM = "DELETE";
    
    // OpenAPI type and location strings
    constexpr const char TypeString[] PROGMEM = "string";
    constexpr const char TypeInteger[] PROGMEM = "integer";
    constexpr const char TypeNumber[] PROGMEM = "number";
    constexpr const char TypeBoolean[] PROGMEM = "boolean";
    constexpr const char TypeArray[] PROGMEM = "array";
    constexpr const char TypeObject[] PROGMEM = "object";
    constexpr const char InQuery[] PROGMEM = "query";
    constexpr const char InPath[] PROGMEM = "path";
    constexpr const char InHeader[] PROGMEM = "header";
    constexpr const char InBody[] PROGMEM = "body";
    constexpr const char RespMissingParams[] PROGMEM = "Missing or invalid parameters";
    constexpr const char RespNotInitialized[] PROGMEM = "Service not initialized";
    constexpr const char RespOperationSuccess[] PROGMEM = "Operation successful";
    constexpr const char RespOperationFailed[] PROGMEM = "Operation failed";
    // Common JSON schemas stored in PROGMEM
    constexpr const char JsonObjectResult[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"result\":{\"type\":\"string\"},\"message\":{\"type\":\"string\"}}}";
}

/**
 * @struct OpenAPIParameter
 * @brief Describes a parameter for an OpenAPI route (query, path, header, or body field)
 */
struct OpenAPIParameter
{
    std::string name;         // Parameter name
    std::string type;         // Data type: "string", "integer", "number", "boolean", "array", "object"
    std::string in;           // Location: "query", "path", "header", "body"
    std::string description;  // Parameter description
    bool required;            // Whether this parameter is required
    std::string defaultValue; // Default value (optional)
    std::string example;      // Example value (optional)
    
    OpenAPIParameter() : name(""), type("string"), in("query"), description(""), required(false), defaultValue(""), example("") {}
    OpenAPIParameter(const char* n, const char* t, const char* loc, const char* desc, bool req = false)
        : name(n), type(t), in(loc), description(desc), required(req), defaultValue(""), example("") {}
};

/**
 * @struct OpenAPIResponse
 * @brief Describes a response for an OpenAPI route
 */
struct OpenAPIResponse
{
    int statusCode;           // HTTP status code (e.g., 200, 404, 500)
    std::string description;  // Response description
    std::string contentType;  // Content type (e.g., "application/json")
    std::string schema;       // JSON schema or description of response structure
    std::string example;      // Example response (optional)
    
    OpenAPIResponse() : statusCode(200), description(""), contentType(RoutesConsts::MimeJSON), schema(""), example("") {}
    OpenAPIResponse(int code, const char* desc, const char* ctype = RoutesConsts::MimeJSON)
        : statusCode(code), description(desc), contentType(ctype), schema(""), example("") {}
};

/**
 * @struct OpenAPIRequestBody
 * @brief Describes the request body schema for POST/PUT/PATCH operations
 */
struct OpenAPIRequestBody
{
    std::string contentType;  // Content type (e.g., "application/json")
    std::string description;  // Request body description
    std::string schema;       // JSON schema or description of request structure
    bool required;            // Whether request body is required
    std::string example;      // Example request body (optional)
    
    OpenAPIRequestBody() : contentType(RoutesConsts::MimeJSON), description(""), schema(""), required(false), example("") {}
    OpenAPIRequestBody(const char* desc, const char* sch, bool req = true)
        : contentType(RoutesConsts::MimeJSON), description(desc), schema(sch), required(req), example("") {}
};

/**
 * @struct OpenAPIRoute
 * @brief Comprehensive OpenAPI route definition with parameters, request body, and responses
 */
struct OpenAPIRoute
{
    std::string path;                          // Route path (e.g., "/api/servo")
    std::string method;                        // HTTP method (GET, POST, PUT, DELETE, PATCH)
    std::string description;                   // Detailed description of the route
    std::string summary;                       // Short summary
    std::vector<std::string> tags;             // Tags for grouping endpoints
    bool requiresAuth;                         // Whether this route requires authentication
    std::vector<OpenAPIParameter> parameters;  // Query, path, and header parameters
    OpenAPIRequestBody requestBody;            // Request body schema (for POST/PUT/PATCH)
    std::vector<OpenAPIResponse> responses;    // Possible responses
    bool deprecated;                           // Whether this endpoint is deprecated
    
    // Default constructor
    OpenAPIRoute() : path(""), method(""), description(""), summary(""), requiresAuth(false), deprecated(false) {}
    
    // Convenience constructor matching common call sites (backward compatible)
    OpenAPIRoute(const char *p, const char *m, const char *desc, const char *summ, bool req)
        : path(p), method(m), description(desc), summary(summ), requiresAuth(req), deprecated(false) 
    {
        if (summ && strlen(summ) > 0) {
            tags.push_back(summ);
        }
    }
    
    // Enhanced constructor with full OpenAPI details
    OpenAPIRoute(const char *p, const char *m, const char *desc, const char *summ, bool req,
                 const std::vector<OpenAPIParameter>& params,
                 const std::vector<OpenAPIResponse>& resps)
        : path(p), method(m), description(desc), summary(summ), requiresAuth(req), 
          parameters(params), responses(resps), deprecated(false)
    {
        if (summ && strlen(summ) > 0) {
            tags.push_back(summ);
        }
    }
};

// Forward declaration - webserver is instantiated in main.cpp
extern WebServer webserver;

struct IsOpenAPIInterface
{
public:
    /**
     * @fn: registerRoutes
     * @brief: Register HTTP routes with the webserver instance.
     * @return: true if registration was successful, false otherwise.
     */
    virtual bool registerRoutes() = 0;
    
    /**
     * @fn: getServiceSubPath
     * @brief: Get the service's subpath component used in API routes
     * @return: Service subpath (e.g., "servos/v1", "sensors/v1")
     */
    virtual std::string getServiceSubPath() = 0;
    
    /**
     * @fn: getPath
     * @brief: Construct full API path from service name and final path segment
     * @param finalpathstring: The final path segment to append
     * @return: Full path in format /api/<servicename>/<finalpathstring>
     * @note: Requires implementing class to also implement IsServiceInterface for getSericeName()
     */
    virtual std::string getPath(const std::string& finalpathstring) = 0;
    
    /**
     * @fn: getOpenAPIRoutes
     * @brief: Get the OpenAPI route definitions provided by this service
     * @return: A vector of OpenAPIRoute structures describing the service's endpoints
     */
    std::vector<OpenAPIRoute> getOpenAPIRoutes()
    {
        return openAPIRoutes;
    };

    virtual ~IsOpenAPIInterface() = default;

protected:
    std::vector<OpenAPIRoute> openAPIRoutes = {};
    bool registerOpenAPIRoute(const OpenAPIRoute &route)
    {
        openAPIRoutes.push_back(route);
        return true;
    }
    
    /**
     * @brief Create a standard error response with code 422 for missing/invalid parameters
     */
    static OpenAPIResponse createMissingParamsResponse()
    {
        return OpenAPIResponse(422, RoutesConsts::RespMissingParams);
    }
    
    /**
     * @brief Create a standard error response with code 503 for uninitialized service
     */
    static OpenAPIResponse createNotInitializedResponse()
    {
        return OpenAPIResponse(503, RoutesConsts::RespNotInitialized);
    }
    
    /**
     * @brief Create a standard success response with code 200 and custom description
     */
    static OpenAPIResponse createSuccessResponse(const char* description)
    {
        OpenAPIResponse resp(200, description);
        resp.schema = RoutesConsts::JsonObjectResult;
        return resp;
    }
    
    /**
     * @brief Create a standard error response with code 456 for operation failure
     */
    static OpenAPIResponse createOperationFailedResponse()
    {
        return OpenAPIResponse(456, RoutesConsts::RespOperationFailed);
    }

    std::string getResultJsonString(std::string result, std::string message)
    {
        JsonDocument doc = JsonDocument();
        doc[RoutesConsts::Result] = result;
        doc[RoutesConsts::Message] = message;
        String output;
        serializeJson(doc, output);
        return std::string(output.c_str());
    }

    /**
     * @brief Register standard save/load settings routes for a service
     * @param serviceName Service name for OpenAPI grouping
     * @param serviceInstance Pointer to IsServiceInterface instance
     */
    void registerSettingsRoutes(const char* serviceName, IsServiceInterface* serviceInstance)
    {
        // Save settings route
        std::string savePath = getPath("saveSettings");
        std::vector<OpenAPIResponse> saveResponses;
        saveResponses.push_back(OpenAPIResponse(200, "Settings saved successfully"));
        registerOpenAPIRoute(OpenAPIRoute(savePath.c_str(), RoutesConsts::MethodGET, "Save own service settings (if exists).", serviceName, false, {}, saveResponses));
        webserver.on(savePath.c_str(), HTTP_GET, [this, serviceInstance]() {
            bool success = serviceInstance->saveSettings();
            std::string response = this->getResultJsonString(
                success ? RoutesConsts::ResultOk : RoutesConsts::ResultErr,
                "saveSettings");
            webserver.send(success ? 200 : 500, RoutesConsts::MimeJSON, response.c_str());
        });

        // Load settings route
        std::string loadPath = getPath("loadSettings");
        std::vector<OpenAPIResponse> loadResponses;
        loadResponses.push_back(OpenAPIResponse(200, "Settings loaded successfully"));
        registerOpenAPIRoute(OpenAPIRoute(loadPath.c_str(), RoutesConsts::MethodGET, "Load own service settings (if exists).", serviceName, false, {}, loadResponses));
        webserver.on(loadPath.c_str(), HTTP_GET, [this, serviceInstance]() {
            bool success = serviceInstance->loadSettings();
            std::string response = this->getResultJsonString(
                success ? RoutesConsts::ResultOk : RoutesConsts::ResultErr,
                "loadSettings");
            webserver.send(success ? 200 : 500, RoutesConsts::MimeJSON, response.c_str());
        });
    }
};
