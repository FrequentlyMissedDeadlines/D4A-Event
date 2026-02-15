/**
 * @file ResponseHelper.h
 * @brief Utility class for standardized HTTP JSON responses
 * @details Provides helper methods to reduce code duplication in webserver response handling
 */

#pragma once

#include <WebServer.h>
#include <ArduinoJson.h>
#include "FlashStringHelper.h"
#include "IsServiceInterface.h"

// Forward declaration
extern WebServer webserver;

/**
 * @class ResponseHelper
 * @brief Centralized HTTP response helper for JSON serialization and error handling
 */
class ResponseHelper {
public:
    // Error types as HTTP status codes
    enum ErrorType {
        BAD_REQUEST = 400,
        UNAUTHORIZED = 401,
        NOT_FOUND = 404,
        INVALID_PARAMS = 422,      // Client error: invalid input
        OPERATION_FAILED = 456,    // Server error: operation failed
        SERVICE_UNAVAILABLE = 503  // Service not ready
    };

    /**
     * @brief Send JSON response with pre-built JsonDocument
     * @param statusCode HTTP status code
     * @param doc JsonDocument containing response data
     */
    static void sendJsonResponse(int statusCode, const JsonDocument& doc) {
        String output;
        serializeJson(doc, output);
        webserver.send(statusCode, "application/json", output.c_str());
    }
    
    /**
     * @brief Send JSON success response with optional message
     * @param message Optional success message (PROGMEM)
     * @param statusCode HTTP status code (default 200)
     */
    static void sendSuccess(const char* message = nullptr, int statusCode = 200) {
        JsonDocument doc;
        doc["result"] = "ok";
        if (message) {
            doc["message"] = message;
        }
        sendJsonResponse(statusCode, doc);
    }
    
    /**
     * @brief Send JSON success response with __FlashStringHelper* message
     * @param message Success message from FPSTR/F() macro
     * @param statusCode HTTP status code (default 200)
     */
    static void sendSuccess(const __FlashStringHelper* message, int statusCode = 200) {
        JsonDocument doc;
        doc["result"] = "ok";
        if (message) {
            doc["message"] = message;
        }
        sendJsonResponse(statusCode, doc);
    }
    
    /**
     * @brief Send JSON success response with custom data
     * @param statusCode HTTP status code
     * @param doc JsonDocument with custom success data
     */
    static void sendSuccessWithData(int statusCode, const JsonDocument& doc) {
        sendJsonResponse(statusCode, doc);
    }
    
    /**
     * @brief Send JSON error response with standard format
     * @param errorType HTTP error status code
     * @param message Error message (PROGMEM)
     */
    static void sendError(ErrorType errorType, const char* message) {
        JsonDocument doc;
        doc["error"] = message;
        doc["result"] = "error";
        sendJsonResponse((int)errorType, doc);
    }
    
    /**
     * @brief Send JSON error response with __FlashStringHelper*
     * @param errorType HTTP error status code
     * @param message Error message from FPSTR/F() macro
     */
    static void sendError(ErrorType errorType, const __FlashStringHelper* message) {
        JsonDocument doc;
        doc["error"] = message;
        doc["result"] = "error";
        sendJsonResponse((int)errorType, doc);
    }
    
    /**
     * @brief Send JSON error response with std::string message
     * @param errorType HTTP error status code
     * @param message Error message as std::string
     */
    static void sendError(ErrorType errorType, const std::string& message) {
        sendError(errorType, message.c_str());
    }
    
    /**
     * @brief Create standard error JSON string
     * @param message Error message (PROGMEM)
     * @return JSON string with error structure
     */
    static String createJsonError(const char* message) {
        JsonDocument doc;
        doc["error"] = message;
        doc["result"] = "error";
        
        String output;
        serializeJson(doc, output);
        return output;
    }
};

/**
 * @class JsonBodyParser
 * @brief Helper for parsing and validating JSON request bodies
 */
class JsonBodyParser {
public:
    /**
     * @brief Parse and validate JSON request body
     * @param doc Reference to JsonDocument to populate
     * @param validator Optional validation function
     * @return true if body valid, false and sends error response if invalid
     */
    static bool parseBody(JsonDocument& doc, 
                         std::function<bool(const JsonDocument&)> validator = nullptr) {
        String body = webserver.arg("plain");
        
        if (body.isEmpty()) {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, "Empty request body");
            return false;
        }
        
        DeserializationError error = deserializeJson(doc, body.c_str());
        if (error) {
            std::string errorMsg = std::string("Invalid JSON: ") + error.c_str();
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, errorMsg);
            return false;
        }
        
        if (validator && !validator(doc)) {
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, 
                                     "Invalid payload schema");
            return false;
        }
        
        return true;
    }
};

/**
 * @class ParamValidator
 * @brief Helper for validating URL/query parameters
 */
class ParamValidator {
public:
    /**
     * @brief Check if parameter exists and optionally validate it
     * @param paramName Parameter name to check
     * @param errorMessage Optional custom error message (const char*)
     * @param validator Optional validation function
     * @return Parameter value if valid, empty string and sends error if invalid
     */
    static std::string getValidatedParam(
        const char* paramName,
        const char* errorMessage = nullptr,
        std::function<bool(const std::string&)> validator = nullptr) 
    {
        if (!webserver.hasArg(paramName)) {
            std::string msg = errorMessage ? std::string(errorMessage) 
                                          : std::string("Missing parameter: ") + paramName;
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, msg);
            return "";
        }
        
        std::string value = webserver.arg(paramName).c_str();
        
        if (validator && !validator(value)) {
            std::string msg = errorMessage ? std::string(errorMessage)
                                          : std::string("Invalid ") + paramName;
            ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, msg);
            return "";
        }
        
        return value;
    }
    
    /**
     * @brief Check if parameter exists and optionally validate it (__FlashStringHelper* version)
     * @param paramName Parameter name to check
     * @param errorMessage Optional custom error message from FPSTR/F()
     * @param validator Optional validation function
     * @return Parameter value if valid, empty string and sends error if invalid
     */
    static std::string getValidatedParam(
        const char* paramName,
        const __FlashStringHelper* errorMessage,
        std::function<bool(const std::string&)> validator = nullptr) 
    {
        if (!webserver.hasArg(paramName)) {
            if (errorMessage) {
                ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, errorMessage);
            } else {
                std::string msg = std::string("Missing parameter: ") + paramName;
                ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, msg);
            }
            return "";
        }
        
        std::string value = webserver.arg(paramName).c_str();
        
        if (validator && !validator(value)) {
            if (errorMessage) {
                ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, errorMessage);
            } else {
                std::string msg = std::string("Invalid ") + paramName;
                ResponseHelper::sendError(ResponseHelper::INVALID_PARAMS, msg);
            }
            return "";
        }
        
        return value;
    }
    
    /**
     * @brief Check if parameter exists (no validation)
     * @param paramName Parameter name to check
     * @return Parameter value if exists, empty string and sends error if not
     */
    static std::string requireParam(const char* paramName) {
        return getValidatedParam(paramName);
    }
};

/**
 * @class ServiceStatusHelper
 * @brief Helper for checking service running status and sending standardized error responses
 * @details Consolidates service status validation patterns across all OpenAPI-enabled services.
 *          Replaces inconsistent checkServiceStarted() implementations that used 423 status codes.
 */
class ServiceStatusHelper {
public:
    /**
-     * @brief Check if service is running, send 503 error if not
     * @param service Pointer to service instance (can be nullptr, will be checked)
     * @param serviceName Service name for error message (PROGMEM string safe)
     * @return true if service is running and started, false if not (error response already sent)
     * 
     * @details Validates both service pointer and startup status, automatically sending
     *          HTTP 503 SERVICE_UNAVAILABLE error with descriptive message if either check fails.
     *          This replaces the older checkServiceStarted() method which incorrectly used 423 Locked.
     * 
     * @note Thread-safe: Only checks service state, does not modify it
     * @note Low memory overhead: Only allocates error message string on failure path
     * 
     * @example Usage within service class (check self)
     * @code
     * void MyService::handleRoute() {
     *     if (!ServiceStatusHelper::ensureServiceRunning(this, "MyService")) return;
     *     // Service is guaranteed running here
     *     // ... route handler logic ...
     * }
     * @endcode
     * 
     * @example Usage for external service dependency
     * @code
     * void MyService::handleRoute() {
     *     if (!ServiceStatusHelper::ensureServiceRunning(g_settingsServiceInstance, "Settings")) return;
     *     // SettingsService is guaranteed running here
     *     // ... route handler logic ...
     * }
     * @endcode
     * 
     * @see IsServiceInterface::isServiceStarted()
     * @see ResponseHelper::sendError()
     */
    static bool ensureServiceRunning(IsServiceInterface* service, const char* serviceName) {
        if (!service || !service->isServiceStarted()) {
            std::string msg = std::string(serviceName) + " service not initialized";
            ResponseHelper::sendError(ResponseHelper::SERVICE_UNAVAILABLE, msg);
            return false;
        }
        return true;
    }
};
