#pragma once

// Include ESPAsyncWebServer first to avoid HTTP method enum conflicts with ESP-IDF
#include <ESPAsyncWebServer.h>
#include <vector>
#include <string>
#include <set>
#include <ArduinoJson.h>
#include "IsServiceInterface.h"
#include "IsMasterRegistryInterface.h"
/**
 * @file IsOpenAPIInterface.h
 * @brief Interface for services that provide OpenAPI specification data and HTTP route registration
 * @details Classes implementing this interface can contribute to the aggregated OpenAPI spec
 *          and register their HTTP routes with an AsyncWebServer instance.
 */

// Common constants for OpenAPI routes (stored in PROGMEM to save RAM)
namespace RoutesConsts
{
    constexpr const char result[] PROGMEM = "result";
    constexpr const char result_ok[] PROGMEM = "ok";
    constexpr const char result_err[] PROGMEM = "error";
    constexpr const char path_api[] PROGMEM = "/api/";
    constexpr const char path_openapi[] PROGMEM = "/api/openapi.json";
    constexpr const char message[] PROGMEM = "message";
    constexpr const char mime_json[] PROGMEM = "application/json";
    constexpr const char mime_plain_text[] PROGMEM = "text/plain";
    constexpr const char mime_image_jpeg[] PROGMEM = "image/jpeg";
    constexpr const char mime_multipart_x_mixed_replace[] PROGMEM = "multipart/x-mixed-replace; boundary=frame";
    
    // HTTP headers
    constexpr const char header_access_control[] PROGMEM = "Access-Control-Allow-Origin";
    constexpr const char header_content_disposition[] PROGMEM = "Content-Disposition";
    
    constexpr const char msg_invalid_params[] PROGMEM = "Invalid or missing parameter(s).";
    constexpr const char msg_invalid_request[] PROGMEM = "Invalid or missing request or query .";
    constexpr const char msg_invalid_json[] PROGMEM = "Invalid JSON in request body.";
    constexpr const char msg_invalid_values[] PROGMEM = "Invalid parameter(s) values.";
    constexpr const char param_domain[] PROGMEM = "domain";
    constexpr const char param_key[] PROGMEM = "key";
    constexpr const char param_value[] PROGMEM = "value";
    constexpr const char field_error[] PROGMEM = "error";
    constexpr const char field_status[] PROGMEM = "status";
    constexpr const char status_ready[] PROGMEM = "ready";
    constexpr const char status_not_initialized[] PROGMEM = "not_initialized";
    constexpr const char status_sensor_error[] PROGMEM = "sensor_error";
    constexpr const char method_get[] PROGMEM = "GET";
    constexpr const char method_post[] PROGMEM = "POST";
    constexpr const char method_put[] PROGMEM = "PUT";
    constexpr const char method_delete[] PROGMEM = "DELETE";
    
    // OpenAPI type and location strings
    constexpr const char type_string[] PROGMEM = "string";
    constexpr const char type_integer[] PROGMEM = "integer";
    constexpr const char type_number[] PROGMEM = "number";
    constexpr const char type_boolean[] PROGMEM = "boolean";
    constexpr const char type_array[] PROGMEM = "array";
    constexpr const char type_object[] PROGMEM = "object";
    constexpr const char in_query[] PROGMEM = "query";
    constexpr const char in_path[] PROGMEM = "path";
    constexpr const char in_header[] PROGMEM = "header";
    constexpr const char in_body[] PROGMEM = "body";
    constexpr const char resp_missing_params[] PROGMEM = "Missing or invalid parameters";
    constexpr const char resp_not_initialized[] PROGMEM = "Service not initialized";
    constexpr const char resp_operation_success[] PROGMEM = "Operation successful";
    constexpr const char resp_operation_failed[] PROGMEM = "Operation failed";
    constexpr const char resp_service_not_started[] PROGMEM = "Service not started";
    constexpr const char resp_not_master[] PROGMEM = "Forbidden: request IP is not the registered master";
    // Common JSON schemas stored in PROGMEM
    constexpr const char json_object_result[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"result\":{\"type\":\"string\"},\"message\":{\"type\":\"string\"}}}";
    
    // Common strings used across services
    constexpr const char str_plus[] PROGMEM = "+";
    constexpr const char str_space[] PROGMEM = " ";
    constexpr const char str_colon[] PROGMEM = ": ";
    constexpr const char str_empty[] PROGMEM = "";
    constexpr const char str_slash[] PROGMEM = "/";
    constexpr const char msg_registering[] PROGMEM = "Registering ";
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
    // Overload to support FPSTR (const __FlashStringHelper*)
    OpenAPIParameter(const char* n, const char* t, const char* loc, const __FlashStringHelper* desc, bool req = false)
        : name(n), type(t), in(loc), description(reinterpret_cast<const char*>(desc)), required(req), defaultValue(""), example("") {}
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
    
    OpenAPIResponse() : statusCode(200), description(""), contentType(RoutesConsts::mime_json), schema(""), example("") {}
    OpenAPIResponse(int code, const char* desc, const char* ctype = RoutesConsts::mime_json)
        : statusCode(code), description(desc), contentType(ctype), schema(""), example("") {}
    // Overload to support FPSTR (const __FlashStringHelper*)
    OpenAPIResponse(int code, const __FlashStringHelper* desc, const char* ctype = RoutesConsts::mime_json)
        : statusCode(code), description(reinterpret_cast<const char*>(desc)), contentType(ctype), schema(""), example("") {}
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
    
    OpenAPIRequestBody() : contentType(RoutesConsts::mime_json), description(""), schema(""), required(false), example("") {}
    OpenAPIRequestBody(const char* desc, const char* sch, bool req = true)
        : contentType(RoutesConsts::mime_json), description(desc), schema(sch), required(req), example("") {}
    // Overload to support FPSTR (const __FlashStringHelper*)
    OpenAPIRequestBody(const __FlashStringHelper* desc, const char* sch, bool req = true)
        : contentType(RoutesConsts::mime_json), description(reinterpret_cast<const char*>(desc)), schema(sch), required(req), example("") {}
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
    
    // Overload to support FPSTR summary
    OpenAPIRoute(const char *p, const char *m, const char *desc, const __FlashStringHelper *summ, bool req,
                 const std::vector<OpenAPIParameter>& params,
                 const std::vector<OpenAPIResponse>& resps)
        : path(p), method(m), description(desc), summary(reinterpret_cast<const char*>(summ)), requiresAuth(req), 
          parameters(params), responses(resps), deprecated(false)
    {
        const char* summStr = reinterpret_cast<const char*>(summ);
        if (summStr && strlen(summStr) > 0) {
            tags.push_back(summStr);
        }
    }
};

// Forward declaration - webserver is instantiated in main.cpp
extern AsyncWebServer webserver;

struct IsOpenAPIInterface : public IsServiceInterface, public virtual IsMasterRegistryInterface 
{
public:
    /**
     * @brief Register HTTP routes with the webserver instance.
     * @return true if registration was successful, false otherwise.
     */
    virtual bool registerRoutes() = 0;
    
    /**
     * @brief Get the service's subpath component used in API routes
     * @return Service subpath (e.g., "servos/v1", "sensors/v1")
     */
    virtual std::string getServiceSubPath() = 0;
    
    /**
     * @brief Construct full API path from service name and final path segment
     * @param finalpathstring The final path segment to append
     * @return Full path in format /api/<servicename>/<finalpathstring>
     * @note Requires implementing class to also implement IsServiceInterface for getServiceName()
     */
    virtual std::string getPath(const std::string& finalpathstring = "") {
        if (baseServicePath_.empty()) {
            baseServicePath_ = std::string(RoutesConsts::path_api) + getServiceSubPath() + "/";
        }
        return baseServicePath_ + finalpathstring;
    }
    

    /**
     * @brief Get the OpenAPI route definitions provided by this service
     * @return A vector of OpenAPIRoute structures describing the service's endpoints
     */
    std::vector<OpenAPIRoute> getOpenAPIRoutes()
    {
        return openAPIRoutes;
    };

    /**
     * @fn asOpenAPIInterface
     * @brief Returns this pointer since this class implements IsOpenAPIInterface
     * @return Pointer to this IsOpenAPIInterface instance
     */
    IsOpenAPIInterface* asOpenAPIInterface() override { return this; }

    /**
     * @brief Default implementation — returns false (no master registered at this level).
     * @details Override in services that actually track a master (e.g. AmakerBotService).
     */
    bool isMaster(const std::string & /*ip*/) const override { return false; }

    /**
     * @brief Default implementation — returns empty string (no master registered at this level).
     * @details Override in services that actually track a master (e.g. AmakerBotService).
     */
    std::string getMasterIP() const override { return ""; }

    virtual ~IsOpenAPIInterface() = default;

protected:
    mutable std::string baseServicePath_;  // Cached base path for performance
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
        return OpenAPIResponse(422, RoutesConsts::resp_missing_params);
    }
    
    /**
     * @brief Create a standard error response with code 503 for uninitialized service
     */
    static OpenAPIResponse createNotInitializedResponse()
    {
        return OpenAPIResponse(503, RoutesConsts::resp_not_initialized);
    }
    
    /**
     * @brief Create a standard success response with code 200 and custom description
     */
    static OpenAPIResponse createSuccessResponse(const char* description)
    {
        OpenAPIResponse resp(200, description);
        resp.schema = RoutesConsts::json_object_result;
        return resp;
    }
    
    /**
     * @brief Create a standard error response with code 456 for operation failure
     */
    static OpenAPIResponse createOperationFailedResponse()
    {
        return OpenAPIResponse(456, RoutesConsts::resp_operation_failed);
    }
    
    /**
     * @brief Create a standard error response with code 423 for service not started
     */
    static OpenAPIResponse createServiceNotStartedResponse()
    {
        return OpenAPIResponse(423, RoutesConsts::resp_service_not_started);
    }

    /**
     * @brief Create a standard error response with code 403 for non-master requests
     */
    static OpenAPIResponse createForbiddenResponse()
    {
        return OpenAPIResponse(403, RoutesConsts::resp_not_master);
    }

    /**
     * @brief Log route registration path for debugging (only in VERBOSE_DEBUG mode)
     * @param path The API route path being registered
     */
    void logRouteRegistration(const std::string& path)
    {
#ifdef VERBOSE_DEBUG
        if (logger)
        {
            logger->debug(progmem_to_string(RoutesConsts::msg_registering) + path);
        }
#endif
    }

    std::string getResultJsonString(std::string result, std::string message)
    {
        JsonDocument doc = JsonDocument();
        doc[RoutesConsts::result] = result;
        doc[RoutesConsts::message] = message;
        String output;
        serializeJson(doc, output);
        return std::string(output.c_str());
    }
    
    /**
     * @brief Check if the HTTP request originates from the registered master IP.
     * @details Extracts the remote IP from the request and compares it against the
     *          master registered in the provided IsMasterRegistryInterface. Sends a
     *          403 JSON error response and returns false when the check fails.
     * @param request        Pointer to AsyncWebServerRequest
     * @param masterRegistry Pointer to IsMasterRegistryInterface holding the master IP
     * @return true if the request IP matches the registered master, false otherwise (403 sent)
     */
    bool checkIsRequestFromMaster(AsyncWebServerRequest *request, const IsMasterRegistryInterface *masterRegistry)
    {
        std::string ip = request->client()->remoteIP().toString().c_str();
        if (!masterRegistry || !masterRegistry->isMaster(ip))
        {
            request->send(403, RoutesConsts::mime_json,
                          getResultJsonString(RoutesConsts::result_err,
                                             reinterpret_cast<const char *>(FPSTR(RoutesConsts::resp_not_master))).c_str());
            return false;
        }
        return true;
    }

    /**
     * @brief Check if service is started and send 423 error if not
     * @param request Pointer to AsyncWebServerRequest
     * @return true if service is started, false otherwise (and 423 response sent)
     */
    bool checkServiceStarted(AsyncWebServerRequest *request)
    {
        if (!isServiceStarted())
        {
            request->send(423, RoutesConsts::mime_json, 
                          getResultJsonString(RoutesConsts::result_err, 
                                            reinterpret_cast<const char*>(FPSTR(RoutesConsts::resp_service_not_started))).c_str());
            return false;
        }
        return true;
    }
    bool checkLoggerDefined(const std::shared_ptr<RollingLogger>& loggerPtr, AsyncWebServerRequest *request)
    {
        if (!loggerPtr)
        {
            request->send(423, RoutesConsts::mime_json, 
                          getResultJsonString(RoutesConsts::result_err, "Logger not defined").c_str());
            return false;
        }
        return true;
    }
    /**
     * @brief Register standard save/load settings routes for a service
     * @param serviceName Service name for OpenAPI grouping
     * @param serviceInstance Pointer to IsServiceInterface instance
     */
    void registerSettingsRoutes( IsServiceInterface* serviceInstance)
    {
        // Save settings route
        std::string savePath = getPath("saveSettings");
        std::vector<OpenAPIResponse> saveResponses;
        saveResponses.push_back(OpenAPIResponse(200, "Settings saved successfully"));
        registerOpenAPIRoute(OpenAPIRoute(savePath.c_str(), RoutesConsts::method_get, "Save own service settings (if exists).", getServiceName().c_str(), false, {}, saveResponses));
        webserver.on(savePath.c_str(), HTTP_GET, [this, serviceInstance](AsyncWebServerRequest *request) {
            bool success = serviceInstance->saveSettings();
            std::string response = this->getResultJsonString(
                success ? RoutesConsts::result_ok : RoutesConsts::result_err,
                "saveSettings");
            request->send(success ? 200 : 500, RoutesConsts::mime_json, response.c_str());
        });

        // Load settings route
        std::string loadPath = getPath("loadSettings");
        std::vector<OpenAPIResponse> loadResponses;
        loadResponses.push_back(OpenAPIResponse(200, "Settings loaded successfully"));
        registerOpenAPIRoute(OpenAPIRoute(loadPath.c_str(), RoutesConsts::method_get, "Load own service settings (if exists).", getServiceName().c_str(), false, {}, loadResponses));
        webserver.on(loadPath.c_str(), HTTP_GET, [this, serviceInstance](AsyncWebServerRequest *request) {
            bool success = serviceInstance->loadSettings();
            std::string response = this->getResultJsonString(
                success ? RoutesConsts::result_ok : RoutesConsts::result_err,
                "loadSettings");
            request->send(success ? 200 : 500, RoutesConsts::mime_json, response.c_str());
        });
    }

    /**
     * @brief Register standard service status route
     * @param serviceName Service name for OpenAPI grouping
     * @param serviceInstance Pointer to IsServiceInterface instance
     */
    void registerServiceStatusRoute(IsServiceInterface* serviceInstance)
    {
        std::string statusPath = getPath("serviceStatus");
        std::vector<OpenAPIResponse> statusResponses;
        OpenAPIResponse statusOk(200, "Service status retrieved");
        statusOk.schema = "{\"type\":\"object\",\"properties\":{\"service\":{\"type\":\"string\"},\"status\":{\"type\":\"string\"},\"initialized\":{\"type\":\"boolean\"}}}";
        statusOk.example = "{\"service\":\"Example Service\",\"status\":\"started\",\"initialized\":true}";
        statusResponses.push_back(statusOk);
        
        registerOpenAPIRoute(OpenAPIRoute(statusPath.c_str(), RoutesConsts::method_get, 
                                         "Get service status", getServiceName().c_str(), false, {}, statusResponses));
        
        webserver.on(statusPath.c_str(), HTTP_GET, [this, serviceInstance](AsyncWebServerRequest *request) {
            JsonDocument doc;
            doc["service"] = serviceInstance->getServiceName();
            doc["status"] = serviceInstance->getStatusString();
            doc["initialized"] = (serviceInstance->getStatus() != UNINITIALIZED);
            
            String output;
            serializeJson(doc, output);
            request->send(200, RoutesConsts::mime_json, output.c_str());
        });
    }
};
