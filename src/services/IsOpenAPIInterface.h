#pragma once

#include <vector>
#include <string>
#include <set>
#include <WebServer.h>
#include <ArduinoJson.h>

/**
 * @file IsOpenAPIInterface.h
 * @brief Interface for services that provide OpenAPI specification data and HTTP route registration
 * @details Classes implementing this interface can contribute to the aggregated OpenAPI spec
 *          and register their HTTP routes with a WebServer instance.
 */

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
    
    OpenAPIResponse() : statusCode(200), description(""), contentType("application/json"), schema(""), example("") {}
    OpenAPIResponse(int code, const char* desc, const char* ctype = "application/json")
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
    
    OpenAPIRequestBody() : contentType("application/json"), description(""), schema(""), required(false), example("") {}
    OpenAPIRequestBody(const char* desc, const char* sch, bool req = true)
        : contentType("application/json"), description(desc), schema(sch), required(req), example("") {}
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
     * @fn: getPath
     * @brief: Construct full API path from service name and final path segment
     * @param finalpathstring: The final path segment to append
     * @return: Full path in format /api/<servicename>/<finalpathstring>
     * @note: Requires implementing class to also implement IsServiceInterface for getName()
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

    std::string getResultJsonString(std::string result, std::string message)
    {
        JsonDocument doc = JsonDocument();
        doc["result"] = result;
        doc["message"] = message;
        String output;
        serializeJson(doc, output);
        return std::string(output.c_str());
    }
};

namespace RoutesConsts
{
    constexpr const char kResult[] = "result";
    constexpr const char kResultOk[] = "ok";
    constexpr const char kResultErr[] = "err";
    constexpr const char kPathAPI[] = "/api/";
    constexpr const char kMessage[] = "message";
    constexpr const char kMimeJSON[] = "application/json";
    constexpr const char kMsgInvalidParams[] = "Invalid or missing parameter(s).";
    constexpr const char kMsgInvalidValues[] = "Invalid parameter(s) values.";
}
