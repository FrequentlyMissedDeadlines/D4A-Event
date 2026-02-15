/**
 * @file RollingLoggerService.cpp
 * @brief Implementation for rolling logger service
 * @details Exposed routes:
 *          - GET /api/logs/v1/all - Retrieve all logs from both debug and app_info loggers
 *          - GET /api/logs/v1/debug - Retrieve debug logger entries only
 *          - GET /api/logs/v1/app_info - Retrieve app_info logger entries only
 *          - GET /api/logs/v1/esp - Retrieve ESP-IDF logger entries only
 */

#include "RollingLoggerService.h"
#include <WebServer.h>
#include <ArduinoJson.h>

// Initialize static members
RollingLogger* RollingLoggerService::debug_logger_ptr_ = nullptr;
RollingLogger* RollingLoggerService::app_info_logger_ptr_ = nullptr;
RollingLogger* RollingLoggerService::esp_logger_ptr_ = nullptr;

void RollingLoggerService::set_logger_instances(RollingLogger* debug_log, RollingLogger* app_info_log, RollingLogger* esp_log)
{
    debug_logger_ptr_ = debug_log;
    app_info_logger_ptr_ = app_info_log;
    esp_logger_ptr_ = esp_log;
}

const char* RollingLoggerService::log_level_to_string(RollingLogger::LogLevel level)
{
    switch (level)
    {
        case RollingLogger::TRACE:   return "TRACE";
        case RollingLogger::DEBUG:   return "DEBUG";
        case RollingLogger::INFO:    return "INFO";
        case RollingLogger::WARNING: return "WARNING";
        case RollingLogger::ERROR:   return "ERROR";
        default:                     return "UNKNOWN";
    }
}

/**
 * @brief Sanitize log message by removing/escaping control characters
 * @param input Original log message
 * @return Sanitized message safe for JSON
 */
static std::string sanitize_log_message(const std::string& input)
{
    std::string output;
    output.reserve(input.length());
    
    for (char c : input)
    {
        unsigned char uc = static_cast<unsigned char>(c);
        
        // Remove or escape control characters (0x00-0x1F, 0x7F)
        if (uc < 0x20 || uc == 0x7F)
        {
            // Skip ESC (27) and other ANSI escape sequences
            if (uc == 27) continue; // ESC
            if (uc == 0x00) continue; // NULL
            if (uc == 0x08) continue; // Backspace
            
            // Keep common control chars but escape them
            if (uc == '\n') output += "\\n";
            else if (uc == '\r') output += "\\r";
            else if (uc == '\t') output += "\\t";
            // Skip other control characters
        }
        else
        {
            output += c;
        }
    }
    
    return output;
}

String RollingLoggerService::serialize_logger_to_json(RollingLogger* logger)
{
    if (!logger)
    {
        return "[]";
    }
    
    JsonDocument doc;
    JsonArray entries = doc.to<JsonArray>();
    
    const auto& log_rows = logger->get_log_rows();
    for (const auto& entry : log_rows)
    {
        JsonObject log_entry = entries.add<JsonObject>();
        log_entry["level"] = log_level_to_string(entry.first);
        // Sanitize message to remove control characters before JSON serialization
        std::string sanitized = sanitize_log_message(entry.second);
        log_entry["message"] = sanitized.c_str();
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

bool RollingLoggerService::registerRoutes()
{
    static constexpr char route_all_desc[] PROGMEM = "Retrieves all log entries from both debug and app_info loggers";
    static constexpr char route_debug_desc[] PROGMEM = "Retrieves log entries from the debug logger only";
    static constexpr char route_app_info_desc[] PROGMEM = "Retrieves log entries from the app_info logger only";
    static constexpr char response_desc[] PROGMEM = "Log entries retrieved successfully";
    static constexpr char response_not_available[] PROGMEM = "Logger instance not available";
    
    static constexpr char schema_logs_array[] PROGMEM = "{\"type\":\"array\",\"items\":{\"type\":\"object\",\"properties\":{\"level\":{\"type\":\"string\",\"description\":\"Log level\"},\"message\":{\"type\":\"string\",\"description\":\"Log message content\"}}}}";
    static constexpr char schema_all_logs[] PROGMEM = "{\"type\":\"object\",\"properties\":{\"debug\":{\"type\":\"array\",\"items\":{\"type\":\"object\",\"properties\":{\"level\":{\"type\":\"string\"},\"message\":{\"type\":\"string\"}}}},\"app_info\":{\"type\":\"array\",\"items\":{\"type\":\"object\",\"properties\":{\"level\":{\"type\":\"string\"},\"message\":{\"type\":\"string\"}}}}}}";
    
    static constexpr char example_single_log[] PROGMEM = "[{\"level\":\"INFO\",\"message\":\"System initialized\"},{\"level\":\"DEBUG\",\"message\":\"Service started\"}]";
    static constexpr char example_all_logs[] PROGMEM = "{\"debug\":[{\"level\":\"DEBUG\",\"message\":\"WebServer task running...\"}],\"app_info\":[{\"level\":\"INFO\",\"message\":\"WiFi connected\"}]}";

    // Route 1: GET /api/logs/v1/all - Get all logs
    {
        std::string path_all = getPath(progmem_to_string(RollingLoggerConsts::path_all_logs));
        #ifdef VERBOSE_DEBUG
        logger->debug("Registering " + path_all);
        #endif

        std::vector<OpenAPIResponse> responses;
        OpenAPIResponse successResponse(200, response_desc);
        successResponse.schema = schema_all_logs;
        successResponse.example = example_all_logs;
        responses.push_back(successResponse);
        responses.push_back(createServiceNotStartedResponse());

        OpenAPIRoute route_all(path_all.c_str(), RoutesConsts::method_get, route_all_desc, "Logs", false, {}, responses);
        registerOpenAPIRoute(route_all);

        webserver.on(path_all.c_str(), HTTP_GET, [this]()
        {
            if (!checkServiceStarted()) return;
            JsonDocument doc;
            
            if (debug_logger_ptr_)
            {
                JsonArray debug_entries = doc["debug"].to<JsonArray>();
                const auto& debug_rows = debug_logger_ptr_->get_log_rows();
                for (const auto& entry : debug_rows)
                {
                    JsonObject log_entry = debug_entries.add<JsonObject>();
                    log_entry["level"] = log_level_to_string(entry.first);
                    std::string sanitized = sanitize_log_message(entry.second);
                    log_entry["message"] = sanitized.c_str();
                }
            }
            
            if (app_info_logger_ptr_)
            {
                JsonArray app_info_entries = doc["app_info"].to<JsonArray>();
                const auto& app_info_rows = app_info_logger_ptr_->get_log_rows();
                for (const auto& entry : app_info_rows)
                {
                    JsonObject log_entry = app_info_entries.add<JsonObject>();
                    log_entry["level"] = log_level_to_string(entry.first);
                    std::string sanitized = sanitize_log_message(entry.second);
                    log_entry["message"] = sanitized.c_str();
                }
            }
            
            if (esp_logger_ptr_)
            {
                JsonArray esp_entries = doc["esp"].to<JsonArray>();
                const auto& esp_rows = esp_logger_ptr_->get_log_rows();
                for (const auto& entry : esp_rows)
                {
                    JsonObject log_entry = esp_entries.add<JsonObject>();
                    log_entry["level"] = log_level_to_string(entry.first);
                    std::string sanitized = sanitize_log_message(entry.second);
                    log_entry["message"] = sanitized.c_str();
                }
            }
            
            String output;
            serializeJson(doc, output);
            webserver.send(200, RoutesConsts::mime_json, output.c_str());
        });
    }

    // Route 2: GET /api/logs/v1/debug - Get debug logs only
    {
        std::string path_debug = getPath(progmem_to_string(RollingLoggerConsts::path_debug_log));
        #ifdef VERBOSE_DEBUG
        logger->debug("Registering " + path_debug);
        #endif

        std::vector<OpenAPIResponse> responses;
        OpenAPIResponse successResponse(200, response_desc);
        successResponse.schema = schema_logs_array;
        successResponse.example = example_single_log;
        responses.push_back(successResponse);
        
        OpenAPIResponse notAvailableResponse(404, response_not_available);
        responses.push_back(notAvailableResponse);

        OpenAPIRoute route_debug(path_debug.c_str(), RoutesConsts::method_get, route_debug_desc, "Logs", false, {}, responses);
        registerOpenAPIRoute(route_debug);

        webserver.on(path_debug.c_str(), HTTP_GET, [this]()
        {
            if (!checkServiceStarted()) return;
            if (!debug_logger_ptr_)
            {
                JsonDocument doc;
                doc[FPSTR(RoutesConsts::result)] = FPSTR(RoutesConsts::result_err);
                doc[FPSTR(RoutesConsts::message)] = "Debug logger not available";
                String output;
                serializeJson(doc, output);
                webserver.send(404, RoutesConsts::mime_json, output.c_str());
                return;
            }
            
            String output = serialize_logger_to_json(debug_logger_ptr_);
            webserver.send(200, RoutesConsts::mime_json, output.c_str());
        });
    }

    // Route 3: GET /api/logs/v1/app_info - Get app_info logs only
    {
        std::string path_app_info = getPath(progmem_to_string(RollingLoggerConsts::path_app_info_log));
        #ifdef VERBOSE_DEBUG
        logger->debug("Registering " + path_app_info);
        #endif

        std::vector<OpenAPIResponse> responses;
        OpenAPIResponse successResponse(200, response_desc);
        successResponse.schema = schema_logs_array;
        successResponse.example = example_single_log;
        responses.push_back(successResponse);
        
        OpenAPIResponse notAvailableResponse(404, response_not_available);
        responses.push_back(notAvailableResponse);
        responses.push_back(createServiceNotStartedResponse());

        OpenAPIRoute route_app_info(path_app_info.c_str(), RoutesConsts::method_get, route_app_info_desc, "Logs", false, {}, responses);
        registerOpenAPIRoute(route_app_info);

        webserver.on(path_app_info.c_str(), HTTP_GET, [this]()
        {
            if (!checkServiceStarted()) return;
            if (!app_info_logger_ptr_)
            {
                JsonDocument doc;
                doc[FPSTR(RoutesConsts::result)] = FPSTR(RoutesConsts::result_err);
                doc[FPSTR(RoutesConsts::message)] = "App info logger not available";
                String output;
                serializeJson(doc, output);
                webserver.send(404, RoutesConsts::mime_json, output.c_str());
                return;
            }
            
            String output = serialize_logger_to_json(app_info_logger_ptr_);
            webserver.send(200, RoutesConsts::mime_json, output.c_str());
        });
    }

    // Route 4: GET /api/logs/v1/esp - Get ESP-IDF logs only
    {
        static constexpr char route_esp_desc[] PROGMEM = "Retrieves log entries from the ESP-IDF logger only";
        std::string path_esp = getPath(progmem_to_string(RollingLoggerConsts::path_esp_log));
        #ifdef VERBOSE_DEBUG
        logger->debug("Registering " + path_esp);
        #endif

        std::vector<OpenAPIResponse> responses;
        OpenAPIResponse successResponse(200, response_desc);
        successResponse.schema = schema_logs_array;
        successResponse.example = example_single_log;
        responses.push_back(successResponse);
        
        OpenAPIResponse notAvailableResponse(404, response_not_available);
        responses.push_back(notAvailableResponse);
        responses.push_back(createServiceNotStartedResponse());

        OpenAPIRoute route_esp(path_esp.c_str(), RoutesConsts::method_get, route_esp_desc, "Logs", false, {}, responses);
        registerOpenAPIRoute(route_esp);

        webserver.on(path_esp.c_str(), HTTP_GET, [this]()
        {
            if (!checkServiceStarted()) return;
            if (!esp_logger_ptr_)
            {
                JsonDocument doc;
                doc[FPSTR(RoutesConsts::result)] = FPSTR(RoutesConsts::result_err);
                doc[FPSTR(RoutesConsts::message)] = "ESP logger not available";
                String output;
                serializeJson(doc, output);
                webserver.send(404, RoutesConsts::mime_json, output.c_str());
                return;
            }
            
            String output = serialize_logger_to_json(esp_logger_ptr_);
            webserver.send(200, RoutesConsts::mime_json, output.c_str());
        });
    }

    registerSettingsRoutes("Logs", this);

    return true;
}

std::string RollingLoggerService::getServiceName()
{
    return progmem_to_string(RollingLoggerConsts::str_service_name);
}

std::string RollingLoggerService::getServiceSubPath()
{
    return progmem_to_string(RollingLoggerConsts::path_service);
}
