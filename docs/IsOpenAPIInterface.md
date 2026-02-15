# IsOpenAPIInterface Documentation

## Overview

`IsOpenAPIInterface` is an interface that enables services to expose HTTP REST API endpoints and contribute to the system's OpenAPI specification. Services implementing this interface can register HTTP routes, define comprehensive API documentation, and participate in the aggregated OpenAPI spec.

**Location**: [src/services/IsOpenAPIInterface.h](src/services/IsOpenAPIInterface.h)

## Purpose

This interface provides:
- HTTP route registration with the web server
- Automatic OpenAPI/Swagger specification generation
- Standardized API endpoint structure
- Common response patterns and helper functions
- Consistent parameter and response definitions

## Relationship with IsServiceInterface

Services that expose HTTP endpoints typically implement **both** interfaces:
- `IsServiceInterface` - For service lifecycle management
- `IsOpenAPIInterface` - For HTTP API functionality

```cpp
class MyService : public IsServiceInterface, public IsOpenAPIInterface {
    // Implement both interfaces
    IsOpenAPIInterface* asOpenAPIInterface() override {
        return this; // Return self
    }
};
```

---

## Core Data Structures

### OpenAPIParameter

Describes a parameter for an API endpoint (query, path, header, or body field).

```cpp
struct OpenAPIParameter {
    std::string name;         // Parameter name
    std::string type;         // "string", "integer", "number", "boolean", "array", "object"
    std::string in;           // "query", "path", "header", "body"
    std::string description;  // Parameter description
    bool required;            // Whether required
    std::string defaultValue; // Default value (optional)
    std::string example;      // Example value (optional)
};
```

**Example**:
```cpp
OpenAPIParameter param("angle", RoutesConsts::type_integer, 
                      RoutesConsts::in_query, 
                      "Servo angle in degrees (0-180)", true);
param.example = "90";
param.defaultValue = "90";
```

---

### OpenAPIResponse

Describes a response for an API endpoint.

```cpp
struct OpenAPIResponse {
    int statusCode;           // HTTP status code (e.g., 200, 404, 500)
    std::string description;  // Response description
    std::string contentType;  // "application/json", "text/plain", etc.
    std::string schema;       // JSON schema
    std::string example;      // Example response (optional)
};
```

**Common Response Codes**:
- `200` - Success
- `422` - Invalid/missing parameters
- `456` - Operation failed
- `503` - Service not initialized

**Example**:
```cpp
OpenAPIResponse successResp(200, "Servo moved successfully");
successResp.schema = RoutesConsts::json_object_result;
successResp.example = "{\"result\":\"ok\",\"message\":\"Servo moved to 90 degrees\"}";
```

---

### OpenAPIRequestBody

Describes the request body for POST/PUT/PATCH operations.

```cpp
struct OpenAPIRequestBody {
    std::string contentType;  // "application/json", etc.
    std::string description;  // Request body description
    std::string schema;       // JSON schema
    bool required;            // Whether required
    std::string example;      // Example request body
};
```

**Example**:
```cpp
OpenAPIRequestBody body("Configuration data", 
                        "{\"type\":\"object\",\"properties\":{\"enabled\":{\"type\":\"boolean\"}}}", 
                        true);
body.example = "{\"enabled\":true}";
```

---

### OpenAPIRoute

Comprehensive route definition combining all OpenAPI metadata.

```cpp
struct OpenAPIRoute {
    std::string path;                          // "/api/servo/move"
    std::string method;                        // "GET", "POST", "PUT", "DELETE"
    std::string description;                   // Detailed description
    std::string summary;                       // Short summary
    std::vector<std::string> tags;             // Grouping tags
    bool requiresAuth;                         // Authentication required
    std::vector<OpenAPIParameter> parameters;  // Parameters
    OpenAPIRequestBody requestBody;            // Request body (POST/PUT)
    std::vector<OpenAPIResponse> responses;    // Possible responses
    bool deprecated;                           // Deprecation flag
};
```

**Example**:
```cpp
// Define parameters
std::vector<OpenAPIParameter> params;
params.push_back(OpenAPIParameter("id", RoutesConsts::type_integer, 
                                  RoutesConsts::in_path, 
                                  "Servo ID", true));

// Define responses
std::vector<OpenAPIResponse> responses;
OpenAPIResponse successResp(200, "Servo moved successfully");
successResp.schema = RoutesConsts::json_object_result;
responses.push_back(successResp);
responses.push_back(createServiceNotStartedResponse());

// Create route
OpenAPIRoute route("/api/servo/move", RoutesConsts::method_post,
                   "Move servo to specified angle",
                   "Servo Control", false,
                   params, responses);
```

---

## Core Interface

The `IsOpenAPIInterface` is a struct (not a class) that provides:
- Pure virtual methods that must be implemented
- Protected helper methods and data members
- Static response creator functions

### Member Variables

```cpp
protected:
    mutable std::string baseServicePath_;       // Cached base path for performance
    std::vector<OpenAPIRoute> openAPIRoutes;    // Registered route definitions
```

---

## Interface Methods

### `registerRoutes()`
```cpp
virtual bool registerRoutes() = 0;
```
**Purpose**: Register all HTTP routes with the global web server.

**Returns**: 
- `true` if registration was successful
- `false` otherwise

**Best Practices**:
- Call this after web server initialization
- Register routes in logical order
- Use helper methods for common patterns
- Register OpenAPI metadata alongside actual routes

**Example**:
```cpp
bool MyService::registerRoutes() {
    std::vector<OpenAPIResponse> responses;
    OpenAPIResponse successResp(200, "Data retrieved successfully");
    successResp.schema = "{\"type\":\"object\"}";
    responses.push_back(successResp);
    responses.push_back(createServiceNotStartedResponse());
    
    std::string path = getPath("");
    registerOpenAPIRoute(OpenAPIRoute(path.c_str(), RoutesConsts::method_get,
                                      "Get service data", "My Service", 
                                      false, {}, responses));
    
    webserver.on(path.c_str(), HTTP_GET, [this]() {
        handleGetData();
    });
    
    // Register standard settings routes
    registerSettingsRoutes("My Service", this);
    
    return true;
}
```

---

### `getServiceSubPath()`
```cpp
virtual std::string getServiceSubPath() = 0;
```
**Purpose**: Returns the service's subpath component used in API routes.

**Returns**: Service subpath (e.g., "servo/v1", "sensors/v1")

**Convention**: Use format `<service>/v<version>` for versioned APIs, or just `<service>/` for unversioned services.

**Example**:
```cpp
std::string MyService::getServiceSubPath() {
    return "myservice/v1";
}
```

---

### `getPath()`
```cpp
virtual std::string getPath(const std::string& finalpathstring = "");
```
**Purpose**: Constructs full API path from service name and final path segment.

**Parameters**:
- `finalpathstring`: The final path segment (e.g., "move", "status"). Default is empty string to get base path.

**Returns**: Full path in format `/api/<servicename>/<finalpathstring>`

**Note**: Implementation caches the base service path in `baseServicePath_` for performance. Uses `getServiceSubPath()` internally.

**Example**:
```cpp
std::string MyService::getPath(const std::string& finalpathstring) {
    if (baseServicePath_.empty()) {
        baseServicePath_ = std::string(RoutesConsts::path_api) + getServiceSubPath() + "/";
    }
    return baseServicePath_ + finalpathstring;
}

// Usage:
std::string movePath = getPath("move");  // Returns "/api/myservice/v1/move"
std::string basePath = getPath();        // Returns "/api/myservice/v1/"
```

---

### `getOpenAPIRoutes()`
```cpp
std::vector<OpenAPIRoute> getOpenAPIRoutes();
```
**Purpose**: Returns all OpenAPI route definitions registered by this service.

**Returns**: Vector of `OpenAPIRoute` structures

**Usage**: Called by HTTPService to build aggregated OpenAPI specification.

---

## Helper Methods

### `registerOpenAPIRoute()`
```cpp
bool registerOpenAPIRoute(const OpenAPIRoute& route);
```
**Purpose**: Adds a route to the service's OpenAPI documentation.

**Usage**: Call this for each endpoint to build documentation.

```cpp
registerOpenAPIRoute(route);
```

---

### Standard Response Creators

#### `createMissingParamsResponse()`
```cpp
static OpenAPIResponse createMissingParamsResponse();
```
Creates a 422 error response for missing/invalid parameters.

```cpp
responses.push_back(createMissingParamsResponse());
```

---

#### `createNotInitializedResponse()`
```cpp
static OpenAPIResponse createNotInitializedResponse();
```
Creates a 503 error response for uninitialized service.

```cpp
responses.push_back(createNotInitializedResponse());
```

---

#### `createSuccessResponse()`
```cpp
static OpenAPIResponse createSuccessResponse(const char* description);
```
Creates a 200 success response with custom description and standard JSON schema.

```cpp
responses.push_back(createSuccessResponse("Operation completed successfully"));
```

---

#### `createOperationFailedResponse()`
```cpp
static OpenAPIResponse createOperationFailedResponse();
```
Creates a 456 error response for operation failure.

```cpp
responses.push_back(createOperationFailedResponse());
```

---

### `getResultJsonString()`
```cpp
std::string getResultJsonString(std::string result, std::string message);
```
**Purpose**: Creates a standard JSON response with result and message fields.

**Parameters**: (used as tag)
- `serviceInstance`: Pointer to the service instance implementing `IsServiceInterface`

**Creates Endpoints**:
- `GET /api/<service>/saveSettings` - Calls `serviceInstance->saveSettings()`, returns 200 on success or 500 on failure
- `GET /api/<service>/loadSettings` - Calls `serviceInstance->loadSettings()`, returns 200 on success or 500 on failure

**Note**: Both endpoints return standard JSON response with `result` and `message` fields.
**Example**:
```cpp
std::string response = getResultJsonString(RoutesConsts::result_ok, "Settings saved");
webserver.send(200, RoutesConsts::mime_json, response.c_str());
```

---

### `registerSettingsRoutes()`
```cpp
void registerSettingsRoutes(const char* serviceName, IsServiceInterface* serviceInstance);
```
**Purpose**: Automatically registers standard save/load settings endpoints.

**Parameters**:
- `serviceName`: Service name for OpenAPI grouping
- `serviceInstance`: Pointer to the service instance

**Creates Endpoints**:
- `GET /api/<service>/saveSettings` - Save service settings
- `GET /api/<service>/loadSettings` - Load service settings

**Example**:
```cpp
bool MyService::registerRoutes() {
    // Register service-specific routes...
    
    // Automatically add settings routes
    registerSettingsRoutes("My Service", this);
    
    return true;
}
```

---

## Common Constants (RoutesConsts)

The `RoutesConsts` namespace provides PROGMEM-stored constants to save RAM.

### JSON Fields
```cpp
RoutesConsts::result             // "result"
RoutesConsts::result_ok          // "ok"
RoutesConsts::result_err         // "error"
RoutesConsts::message            // "message"
RoutesConsts::field_error        // "error"
RoutesConsts::field_status       // "status"
RoutesConsts::status_ready       // "ready"
RoutesConsts::status_not_initialized      // "not_initialized"
RoutesConsts::status_sensor_error         // "sensor_error"
```

### Paths
```cpp
RoutesConsts::path_api           // "/api/"
RoutesConsts::path_openapi       // "/api/openapi.json"
```

### MIME Types
```cpp
RoutesConsts::mime_json          // "application/json"
RoutesConsts::mime_plain_text    // "text/plain"
```

### HTTP Methods
```cpp
RoutesConsts::method_get         // "GET"
RoutesConsts::method_post        // "POST"
RoutesConsts::method_put         // "PUT"
RoutesConsts::method_delete      // "DELETE"
```

### OpenAPI Types
```cpp
RoutesConsts::type_string        // "string"
RoutesConsts::type_integer       // "integer"
RoutesConsts::type_number        // "number"
RoutesConsts::type_boolean       // "boolean"
RoutesConsts::type_array         // "array"
RoutesConsts::type_object        // "object"
```

### Parameter Locations
```cpp
RoutesConsts::in_query           // "query"
RoutesConsts::in_path            // "path"
RoutesConsts::in_header          // "header"
RoutesConsts::in_body            // "body"
```

### Common Messages
```cpp
RoutesConsts::msg_invalid_params      // "Invalid or missing parameter(s)."
RoutesConsts::msg_invalid_values      // "Invalid parameter(s) values."
RoutesConsts::resp_missing_params     // "Missing or invalid parameters"
RoutesConsts::resp_not_initialized    // "Service not initialized"
RoutesConsts::resp_operation_success  // "Operation successful"
RoutesConsts::resp_operation_failed   // "Operation failed"
```

### Additional Parameters
```cpp
RoutesConsts::param_domain       // "domain"
RoutesConsts::param_key          // "key"
RoutesConsts::param_value        // "value"
```

### JSON Schemas
```cpp
RoutesConsts::json_object_result // JSON schema for result/message object
```

---

## Complete Implementation Example

```cpp
#include "IsServiceInterface.h"
#include "IsOpenAPIInterface.h"

class MyService : public IsServiceInterface, public IsOpenAPIInterface {
public:
    // IsServiceInterface methods
    std::string getServiceName() override { return "My Service"; }
    bool initializeService() override { /* ... */ return true; }
    bool startService() override { /* ... */ return true; }
    bool stopService() override { /* ... */ return true; }
    
    IsOpenAPIInterface* asOpenAPIInterface() override {
        return this;
    }
    
    // IsOpenAPIInterface methods
    std::string getServiceSubPath() override {
        return "myservice/v1";
    }
    
    std::string getPath(const std::string& finalpathstring = "") override {
        if (baseServicePath_.empty()) {
            baseServicePath_ = std::string(RoutesConsts::path_api) + getServiceSubPath() + "/";
        }
        return baseServicePath_ + finalpathstring;
    }
    
    bool registerRoutes() override {
        // Route 1: GET status
        registerStatusRoute();
        
        // Route 2: POST action
        registerActionRoute();
        
        // Route 3: Standard settings routes
        registerSettingsRoutes("My Service", this);
        
        return true;
    }

private:
    void registerStatusRoute() {
        std::vector<OpenAPIResponse> responses;
        responses.push_back(createSuccessResponse("Service status retrieved"));
        responses.push_back(createNotInitializedResponse());method_get,
                                          "Get service status",
                                          "My Service", false, {}, responses));
        
        webserver.on(path.c_str(), HTTP_GET, [this]() {
            handleGetStatus();
        });
    }
    
    void registerActionRoute() {
        // Define parameters
        std::vector<OpenAPIParameter> params;
        OpenAPIParameter actionParam("action", RoutesConsts::type_string, 
                                     RoutesConsts::in_query, 
                                     "Action to perform", true);
        actionParam.example = "start";
        params.push_back(actionParam);
        
        // Define responses
        std::vector<OpenAPIResponse> responses;
        responses.push_back(createSuccessResponse("Action executed successfully"));
        responses.push_back(createMissingParamsResponse());
        responses.push_back(createOperationFailedResponse());
        
        std::string path = getPath("action");
        registerOpenAPIRoute(OpenAPIRoute(path.c_str(), RoutesConsts::method_post,
                                          "Perform an action",
                                          "My Service", false,
                                          params, responses));
        
        webserver.on(path.c_str(), HTTP_POST, [this]() {
            handleAction();
        });
    }
    
    void handleGetStatus() {
        JsonDocument doc;
        doc[RoutesConsts::field_status] = RoutesConsts::status_ready;
        doc[RoutesConsts::message] = "Service is running";
        
        String output;
        serializeJson(doc, output);
        webserver.send(200, RoutesConsts::mime_json, output.c_str());
    }
    
    void handleAction() {
        // Validate parameters
        if (!webserver.hasArg("action")) {
            std::string response = getResultJsonString(
                RoutesConsts::result_err,
                RoutesConsts::msg_invalid_params);
            webserver.send(422, RoutesConsts::mime_json, response.c_str());
            return;
        }
        
        String action = webserver.arg("action");
        
        // Perform action
        bool success = executeAction(action.c_str());
        
        std::string response = getResultJsonString(
            success ? RoutesConsts::result_ok : RoutesConsts::result_err,
            success ? "Action completed" : "Action failed");
        
        webserver.send(success ? 200 : 456, RoutesConsts::mime_jsontErr,
            success ? "Action completed" : "Action failed");
        
        webserver.send(success ? 200 : 456, RoutesConsts::MimeJSON, response.c_str());
    }
    
    bool executeAction(const char* action) {
        // Implementation...
        return true;
    }
};
```

---

## API Path Convention

All API endpoints follow this structure:

```
/api/<service-subpath>/<endpoint>
```

**Examples**:
- `/api/servos/v1/move` - ServoService move endpoint
- `/api/sensors/v1/temperature` - Sensor temperature endpoint
- `/api/servos/v1/saveSettings` - Servo settings save endpoint

The OpenAPI specification is available at:
```
/api/openapi.json
```

---

## HTTP Method Guidelines

- **GET** - Retrieve data (idempotent, no side effects)
- **POST** - Create resources or trigger actions
- **PUT** - Update existing resources (full replacement)
- **DELETE** - Remove resources

---

## Response Format Standards

### Success Response
```json
{
  "result": "ok",
  "message": "Operation completed successfully"
}
```

### Error Response
```json
{
  "result": "err",
  "message": "Error description"
}
```

### Status Response
```json
{
  "status": "ready",
  "message": "Additional information"
}
```

---

## Best Practices

### DO:
- ✅ Use `RoutesConsts` constants instead of string literals
- ✅ Validate all input parameters
- ✅ Return appropriate HTTP status codes
- ✅ Document all parameters and responses in OpenAPI
- ✅ Use `getResultJsonString()` for standard responses
- ✅ Store static strings in PROGMEM to save RAM
- ✅ Use `createSuccessResponse()` and error helpers
- ✅ Register OpenAPI metadata for all endpoints

### DON'T:
- ❌ Don't hardcode paths - use `getPath()`
- ❌ Don't forget to register routes in `registerRoutes()`
- ❌ Don't skip parameter validation
- ❌ Don't use inconsistent response formats
- ❌ Don't allocate large buffers on stack
- ❌ Don't block in HTTP handlers

---

## Memory Optimization

HTTP handlers run in constrained memory:

- Use `PROGMEM` for static data (provided by `RoutesConsts`)
- Reuse `JsonDocument` instances where possible
- Stream large responses instead of buffering
- Avoid string concatenation in hot paths
- Use references for parameters to avoid copies

---

## Error Handling

Always handle these cases:

1. **Missing Parameters**: Return 422 with descriptive message
2. **Invalid Values**: Return 422 with validation errors
3. **Service Not Ready**: Return 503 if not initialized
4. **Operation Failed**: Return 456 with failure reason
5. **Internal Errors**: Return 500 with generic message

**Example**:
```cpp
void handleRequest() {mime_json,
                      getResultJsonString(RoutesConsts::result_err, 
                                         RoutesConsts::resp_not_initialized).c_str());
        return;
    }
    
    // Validate parameters
    if (!webserver.hasArg("param")) {
        webserver.send(422, RoutesConsts::mime_json,
                      getResultJsonString(RoutesConsts::result_err, 
                                         RoutesConsts::msg_invalid_p
        webserver.send(422, RoutesConsts::MimeJSON,
                      getResultJsonString(RoutesConsts::ResultErr, 
                                         RoutesConsts::MsgInvalidParams).c_str());
        return;
    }
    
    // Process request...
}
```

---

## OpenAPI Specification Access

The aggregated OpenAPI spec is automatically generated and available at:

```
GET /api/openapi.json
```

This endpoint combines route definitions from all services implementing `IsOpenAPIInterface`.

You can use this with Swagger UI or other OpenAPI tools for interactive API documentation.

---

## Testing Endpoints

Use curl or similar tools to test endpoints:

```bash
# GET request
curl http://k10-bot.local/api/myservice/v1/status

# POST request with query parameter
curl -X POST "http://k10-bot.local/api/myservice/v1/action?action=start"

# POST request with JSON body
curl -X POST http://k10-bot.local/api/myservice/v1/config \
  -H "Content-Type: application/json" \
  -d '{"enabled":true}'

# Get OpenAPI spec
curl http://k10-bot.local/api/openapi.json | jq
```

---

## Example Services

Study these existing implementations:

- [HTTPService](src/services/HTTPService.cpp) - Web server and OpenAPI aggregation
- [ServoService](src/services/ServoService.cpp) - Servo control API
- [CameraService](src/services/camera/CameraService.cpp) - Camera streaming API

---

## Related Documentation

- [IsServiceInterface.md](IsServiceInterface.md) - Service lifecycle management
- [OPENAPI.md](OPENAPI.md) - OpenAPI specification details

---

## Architecture Notes

- Web server runs in dedicated RTOS task
- Keep HTTP handlers non-blocking
- Use logger for debugging (not Serial.print)
- Follow real-time constraints for time-critical paths
- Minimize memory usage on ESP32-S3
