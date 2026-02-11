/**
 * @file SettingsService.cpp
 * @brief Implementation of settings service using ESP32 Preferences library
 * @details Exposed routes:
 *          - GET /api/settings/v1/get - Retrieve settings from a domain (all or specific key)
 *          - POST /api/settings/v1/set - Store settings to a domain (single or multiple key-value pairs)
 * 
 */

#include "SettingsService.h"
#include "../FlashStringHelper.h"
#include "../ResponseHelper.h"
#include <WebServer.h>
#include <ArduinoJson.h>
#include <pgmspace.h>
#include <Preferences.h>
Preferences preferences_;

// SettingsService constants namespace
namespace SettingsConsts
{
    constexpr const char json_error_prefix[] PROGMEM = "{\"error\":\"";
    constexpr const char json_error_suffix[] PROGMEM = "\"}";
    constexpr const char json_settings[] PROGMEM = "settings";
    constexpr const char json_success[] PROGMEM = "success";
    constexpr size_t max_domain_length = 15;
    constexpr size_t max_key_length = 15;
    constexpr const char msg_initialized[] PROGMEM = "Settings Service initialized";
    constexpr const char msg_initializing[] PROGMEM = "Initializing Settings Service";
    constexpr const char msg_invalid_domain[] PROGMEM = "Invalid domain name.";
    constexpr const char msg_invalid_key[] PROGMEM = "Invalid key name.";
    constexpr const char msg_missing_domain[] PROGMEM = "Missing required parameter: domain";
    constexpr const char msg_missing_key[] PROGMEM = "Missing required parameter: key";
    constexpr const char msg_missing_value[] PROGMEM = "Missing required parameter: value";
    constexpr const char msg_not_started[] PROGMEM = "Settings service not started.";
    constexpr const char msg_operation_failed[] PROGMEM = "Operation failed.";
    constexpr const char msg_success[] PROGMEM = "Operation successful.";
    constexpr const char path_service[] PROGMEM = "settings/v1";
    constexpr const char path_settings[] PROGMEM = "settings";
    constexpr const char str_service_name[] PROGMEM = "Settings service";
}

// Global instance pointer for static handlers
static SettingsService* g_settingsServiceInstance = nullptr;

// External reference to global webserver
extern WebServer webserver;

// Helper function to create JSON error response from PROGMEM string
static inline String createJsonError(const __FlashStringHelper* msg)
{
    String result;
    result.reserve(64);
    result += FPSTR(SettingsConsts::json_error_prefix);
    result += msg;
    result += FPSTR(SettingsConsts::json_error_suffix);
    return result;
}

SettingsService::SettingsService() 
{
    g_settingsServiceInstance = this;
}

SettingsService::~SettingsService()
{
    if (isServiceStarted()) {
        preferences_.end();
    }
    if (g_settingsServiceInstance == this) {
        g_settingsServiceInstance = nullptr;
    }
}

std::string SettingsService::getServiceName()
{
    return fpstr_to_string(FPSTR(SettingsConsts::str_service_name));
}

std::string SettingsService::getServiceSubPath()
{
    return fpstr_to_string(FPSTR(SettingsConsts::path_service));
}




bool SettingsService::isValidDomain(const std::string& domain)
{
    if (domain.empty() || domain.length() > SettingsConsts::max_domain_length) {
        return false;
    }
    
    // Check for valid characters (alphanumeric and underscore)
    for (char c : domain) {
        if (!isalnum(c) && c != '_') {
            return false;
        }
    }
    
    return true;
}

bool SettingsService::isValidKey(const std::string& key)
{
    if (key.empty() || key.length() > SettingsConsts::max_key_length) {
        return false;
    }
    
    // Check for valid characters (alphanumeric and underscore)
    for (char c : key) {
        if (!isalnum(c) && c != '_') {
            return false;
        }
    }
    
    return true;
}

std::string SettingsService::getSetting(const std::string& domain, const std::string& key, 
                                       const std::string& defaultValue)
{
    if (!isServiceStarted() || !isValidDomain(domain) || !isValidKey(key)) {
        return defaultValue;
    }
    
    if (!preferences_.begin(domain.c_str(), true)) { // true = read-only
        return defaultValue;
    }
    
    std::string value = preferences_.getString(key.c_str(), defaultValue.c_str()).c_str();
    preferences_.end();
    
    return value;
}

bool SettingsService::setSetting(const std::string& domain, const std::string& key, 
                                const std::string& value)
{
    if (!isServiceStarted() || !isValidDomain(domain) || !isValidKey(key)) {
        return false;
    }
    
    if (!preferences_.begin(domain.c_str(), false)) { // false = read-write
        return false;
    }
    
    size_t written = preferences_.putString(key.c_str(), value.c_str());
    preferences_.end();
    
    return written > 0;
}

std::vector<Setting> SettingsService::getAllSettings(const std::string& domain)
{
    std::vector<Setting> settings;
    
    if (!isServiceStarted() || !isValidDomain(domain)) {
        return settings;
    }
    
    // ESP32 Preferences doesn't provide a way to enumerate all keys
    // This is a limitation of the library
    // For now, return empty vector with a note in logger
    logger->warning("getAllSettings: ESP32 Preferences does not support key enumeration");
    
    return settings;
}

bool SettingsService::setMultipleSettings(const std::string& domain, 
                                         const std::vector<Setting>& settings)
{
    if (!isServiceStarted() || !isValidDomain(domain)) {
        return false;
    }
    
    if (!preferences_.begin(domain.c_str(), false)) { // false = read-write
        return false;
    }
    
    bool allSuccess = true;
    for (const auto& setting : settings) {
        if (isValidKey(setting.key)) {
            size_t written = preferences_.putString(setting.key.c_str(), setting.value.c_str());
            if (written == 0) {
                allSuccess = false;
                logger->warning("Failed to write setting: " + setting.key);
            }
        } else {
            allSuccess = false;
            logger->warning("Invalid key name: " + setting.key);
        }
    }
    
    preferences_.end();
    return allSuccess;
}

bool SettingsService::deleteSetting(const std::string& domain, const std::string& key)
{
    if (!isServiceStarted() || !isValidDomain(domain) || !isValidKey(key)) {
        return false;
    }
    
    if (!preferences_.begin(domain.c_str(), false)) { // false = read-write
        return false;
    }
    
    bool success = preferences_.remove(key.c_str());
    preferences_.end();
    
    return success;
}

bool SettingsService::clearDomain(const std::string& domain)
{
    if (!isServiceStarted() || !isValidDomain(domain)) {
        return false;
    }
    
    if (!preferences_.begin(domain.c_str(), false)) { // false = read-write
        return false;
    }
    
    bool success = preferences_.clear();
    preferences_.end();
    
    return success;
}

std::string SettingsService::buildSettingsJson(const std::vector<Setting>& settings)
{
    JsonDocument doc;
    
    doc[RoutesConsts::param_domain] = settings.empty() ? "" : settings[0].domain;
    
    JsonObject settingsObj = doc[FPSTR(SettingsConsts::json_settings)].to<JsonObject>();
    
    for (const auto& setting : settings) {
        settingsObj[setting.key] = setting.value;
    }
    
    String output;
    serializeJson(doc, output);
    return std::string(output.c_str());
}

void SettingsService::handleGetSettings()
{
    // Check service status
    if (!ServiceStatusHelper::ensureServiceRunning(g_settingsServiceInstance, "Settings")) {
        return;
    }
    
    // Validate domain parameter
    std::string domain = ParamValidator::getValidatedParam(
        RoutesConsts::param_domain,
        FPSTR(SettingsConsts::msg_missing_domain),
        [](const std::string& d) { return isValidDomain(d); }
    );
    if (domain.empty()) return;
    
    // Check if specific key is requested
    if (webserver.hasArg(RoutesConsts::param_key)) {
        std::string key = ParamValidator::getValidatedParam(
            RoutesConsts::param_key,
            FPSTR(SettingsConsts::msg_invalid_key),
            [](const std::string& k) { return isValidKey(k); }
        );
        if (key.empty()) return;
        
        std::string value = g_settingsServiceInstance->getSetting(domain, key, "");
        
        // Return plain text for single value
        webserver.send(200, RoutesConsts::mime_plain_text, value.c_str());
    } else {
        // Get all settings in domain (currently limited by ESP32 Preferences)
        std::vector<Setting> settings = g_settingsServiceInstance->getAllSettings(domain);
        
        // Return empty object with warning
        JsonDocument doc;
        doc[RoutesConsts::param_domain] = domain;
        doc[RoutesConsts::message] = "ESP32 Preferences does not support key enumeration";
        doc[FPSTR(SettingsConsts::json_settings)] = serialized("{}");
        
        ResponseHelper::sendJsonResponse(503, doc);
    }
}

void SettingsService::handlePostSettings()
{
    // Check service status
    if (!ServiceStatusHelper::ensureServiceRunning(g_settingsServiceInstance, "Settings")) {
        return;
    }
    
    // Validate domain parameter
    std::string domain = ParamValidator::getValidatedParam(
        RoutesConsts::param_domain,
        FPSTR(SettingsConsts::msg_missing_domain),
        [](const std::string& d) { return isValidDomain(d); }
    );
    if (domain.empty()) return;
    
    // Check if single key/value update via query parameters
    if (webserver.hasArg(RoutesConsts::param_key) && webserver.hasArg(RoutesConsts::param_value)) {
        std::string key = ParamValidator::getValidatedParam(
            RoutesConsts::param_key,
            FPSTR(SettingsConsts::msg_invalid_key),
            [](const std::string& k) { return isValidKey(k); }
        );
        if (key.empty()) return;
        
        std::string value = webserver.arg(RoutesConsts::param_value).c_str();
        
        bool success = g_settingsServiceInstance->setSetting(domain, key, value);
        
        if (success) {
            JsonDocument doc;
            doc[FPSTR(SettingsConsts::json_success)] = true;
            doc[RoutesConsts::message] = FPSTR(SettingsConsts::msg_success);
            ResponseHelper::sendJsonResponse(200, doc);
        } else {
            ResponseHelper::sendError(ResponseHelperConsts::SERVICE_UNAVAILABLE, 
                                     FPSTR(SettingsConsts::msg_operation_failed));
        }
    } else if (webserver.hasArg("plain")) {
        // Parse JSON body for multiple settings
        JsonDocument doc;
        if (!JsonBodyParser::parseBody(doc)) {
            return;
        }
        
        std::vector<Setting> settings;
        
        // Iterate through JSON object
        for (JsonPair kv : doc.as<JsonObject>()) {
            Setting setting(domain, kv.key().c_str(), kv.value().as<std::string>());
            settings.push_back(setting);
        }
        
        bool success = g_settingsServiceInstance->setMultipleSettings(domain, settings);
        
        if (success) {
            JsonDocument responseDoc;
            responseDoc[FPSTR(SettingsConsts::json_success)] = true;
            responseDoc[RoutesConsts::message] = FPSTR(SettingsConsts::msg_success);
            ResponseHelper::sendJsonResponse(200, responseDoc);
        } else {
            ResponseHelper::sendError(ResponseHelperConsts::SERVICE_UNAVAILABLE,
                                     FPSTR(SettingsConsts::msg_operation_failed));
        }
    } else {
        ResponseHelper::sendError(ResponseHelperConsts::INVALID_PARAMS,
                                 FPSTR(RoutesConsts::msg_invalid_request));
    }
}

bool SettingsService::registerRoutes()
{
    // GET /api/SettingsService/settings - Get settings
    std::string getPath = this->getPath(SettingsConsts::path_settings);
    
#ifdef VERBOSE_DEBUG
    logger->debug("+" + getPath);
#endif
    
    std::vector<OpenAPIParameter> getParams={};
    getParams.push_back(OpenAPIParameter(RoutesConsts::param_domain, "string", "query", 
        "Settings domain/namespace (max 15 chars, alphanumeric and underscore)", true));
    getParams.push_back(OpenAPIParameter(RoutesConsts::param_key, "string", "query", 
        "Setting key (max 15 chars, alphanumeric and underscore)", false));
    
    std::vector<OpenAPIResponse> getResponses;
    OpenAPIResponse getResponse200(200, "Successful operation");
    getResponse200.example = "{\"domain\":\"wifi\",\"settings\":{}}";
    getResponses.push_back(getResponse200);
    getResponses.push_back(OpenAPIResponse(422, "Invalid parameters"));
    getResponses.push_back(createServiceNotStartedResponse());
    
    registerOpenAPIRoute(OpenAPIRoute(getPath.c_str(), RoutesConsts::method_get,
                                      "Retrieve a single setting value or all settings in a domain",
                                      "Settings", false, getParams, getResponses));
    
    webserver.on(getPath.c_str(), HTTP_GET, handleGetSettings);
    
    // POST /api/SettingsService/settings - Set settings
    std::string postPath = this->getPath(SettingsConsts::path_settings);
    
#ifdef VERBOSE_DEBUG
    logger->debug("+" + postPath);
#endif
    
    std::vector<OpenAPIParameter> postParams;
    postParams.push_back(OpenAPIParameter(RoutesConsts::param_domain, "string", "query", 
        "Settings domain/namespace (max 15 chars, alphanumeric and underscore)", true));
    postParams.push_back(OpenAPIParameter(RoutesConsts::param_key, "string", "query", 
        "Setting key for single update (max 15 chars)", false));
    postParams.push_back(OpenAPIParameter(RoutesConsts::param_value, "string", "query", 
        "Setting value for single update", false));
    
    std::vector<OpenAPIResponse> postResponses;
    OpenAPIResponse postResponse200(200, "Settings updated successfully");
    postResponse200.example = "{\"success\":true,\"message\":\"Operation successful.\"}";
    postResponses.push_back(postResponse200);
    postResponses.push_back(OpenAPIResponse(422, "Invalid parameters"));
    postResponses.push_back(OpenAPIResponse(503, "Operation failed"));
    postResponses.push_back(createServiceNotStartedResponse());
    
    registerOpenAPIRoute(OpenAPIRoute(postPath.c_str(), RoutesConsts::method_post,
                                      "Update or insert setting. Use query parameters.",
                                      "Settings", false, postParams, postResponses));
    
    webserver.on(postPath.c_str(), HTTP_POST, handlePostSettings);

    registerSettingsRoutes("Settings", this);
    

    return true;
}
