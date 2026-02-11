/**
 * @file RollingLoggerService.cpp
 * @brief Implementation for rolling logger service
 * @details Exposed routes:
 *          - GET /api/logs/v1/all - Retrieve all logs from both debug and app_info loggers
 *          - GET /api/logs/v1/debug - Retrieve debug logger entries only
 *          - GET /api/logs/v1/app_info - Retrieve app_info logger entries only
 */

#include "RollingLoggerService.h"
#include <WebServer.h>
#include <ArduinoJson.h>

// Initialize static members
RollingLogger* RollingLoggerService::debug_logger_ptr_ = nullptr;
RollingLogger* RollingLoggerService::app_info_logger_ptr_ = nullptr;

void RollingLoggerService::set_logger_instances(RollingLogger* debug_log, RollingLogger* app_info_log)
{
    debug_logger_ptr_ = debug_log;
    app_info_logger_ptr_ = app_info_log;
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
        log_entry["message"] = entry.second;
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
        std::string path_all = getPath(fpstr_to_string(FPSTR(RollingLoggerConsts::path_all_logs)));
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
                    log_entry["message"] = entry.second;
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
                    log_entry["message"] = entry.second;
                }
            }
            
            String output;
            serializeJson(doc, output);
            webserver.send(200, RoutesConsts::mime_json, output.c_str());
        });
    }

    // Route 2: GET /api/logs/v1/debug - Get debug logs only
    {
        std::string path_debug = getPath(fpstr_to_string(FPSTR(RollingLoggerConsts::path_debug_log)));
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
        std::string path_app_info = getPath(fpstr_to_string(FPSTR(RollingLoggerConsts::path_app_info_log)));
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

    registerSettingsRoutes("Logs", this);

    return true;
}

std::string RollingLoggerService::getServiceName()
{
    return fpstr_to_string(FPSTR(RollingLoggerConsts::str_service_name));
}

std::string RollingLoggerService::getServiceSubPath()
{
    return fpstr_to_string(FPSTR(RollingLoggerConsts::path_service));
}
