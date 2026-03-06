/**
 * @file AmakerBotService.cpp
 * @brief Master-registration service for K10-Bot
 * @details Exposed routes:
 *          - POST /api/amakerbot/v1/register?token=<token>  Register caller IP as master (if token valid)
 *          - GET  /api/amakerbot/v1/master                  Query current master info
 *          - POST /api/amakerbot/v1/unregister              Clear master (master IP only)
 *          - GET  /api/amakerbot/v1/token                   Retrieve server-generated token
 *
 *          UDP protocol (service_id 0x4):
 *          - [0x41]<token>  Register UDP sender as master (if token valid); 
 *          - [0x42]         Clear master (master IP only);                  
 *          - [0x43]         Heartbeat keep-alive — must arrive every ≤50 ms or all motors are stopped
 *
 *          On service init, a random 5-character alphanumeric token is generated
 *          and logged to app_info_logger so it appears in MODE_APP_LOG on the screen.
 */

#include "services/AmakerBotService.h"
#include "services/ServoService.h"
#include "FlashStringHelper.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <random>

// Access app_info_logger and servo_service defined in main.cpp
extern RollingLogger app_info_logger;
extern ServoService  servo_service;

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
namespace AmakerBotConsts
{
    constexpr const char path_service[]       PROGMEM = "amakerbot/v1";
    constexpr const char str_service_name[]   PROGMEM = "AmakerBot";
    constexpr const char tag_service[]        PROGMEM = "AmakerBot";

    // JSON field names
    constexpr const char field_ip[]           PROGMEM = "ip";
    constexpr const char field_token[]        PROGMEM = "token";
    constexpr const char field_registered[]   PROGMEM = "registered";

    // HTTP parameter names
    constexpr const char param_token[]        PROGMEM = "token";

    // Log messages (written to app_info_logger → visible in MODE_APP_LOG)
    constexpr const char msg_token_generated[] PROGMEM = "[AMAKERBOT] Token: ";
    constexpr const char msg_registered[]     PROGMEM = "[MASTER] registered from ";
    constexpr const char msg_unregistered[]   PROGMEM = "[MASTER] unregistered";

    // Service-internal log messages (debug_logger)
    constexpr const char msg_mutex_failed[]   PROGMEM = "AmakerBot: mutex creation failed";

    // HTTP response descriptions
    constexpr const char resp_registered[]    PROGMEM = "Master registered successfully";
    constexpr const char resp_unregistered[]  PROGMEM = "Master unregistered";
    constexpr const char resp_master_info[]   PROGMEM = "Current master info retrieved";
    constexpr const char resp_token_info[]    PROGMEM = "Server token retrieved";
    constexpr const char resp_missing_token[] PROGMEM = "Missing token parameter";
    constexpr const char resp_invalid_token[] PROGMEM = "Invalid token";
    constexpr const char resp_unauthorized[]  PROGMEM = "Not authorized: you are not the master";

    // Heartbeat watchdog
    constexpr uint32_t heartbeat_timeout_ms = 50; ///< ms without heartbeat before all motors are stopped
    constexpr const char msg_heartbeat_timeout[] PROGMEM = "[AMAKERBOT] Heartbeat timeout - stopping motors";
    constexpr const char msg_heartbeat_restored[] PROGMEM = "[AMAKERBOT] Heartbeat restored";

    // UDP command prefixes
    constexpr uint8_t udp_service_id = 0x04;  ///< Unique ID for DFR1216 Service (high nibble of action byte)
    constexpr uint8_t udp_action_master_register = (udp_service_id << 4) | 0x01;  ///< followed by token string, registers sender IP as master if token valid; server responds with udp_action_master_register + 0x00 on success or + 0x01 on failure (invalid token) — e.g. [0x41][0x00] for success, [0x41][0x01] for invalid token
    constexpr uint8_t udp_action_master_unregister = (udp_service_id << 4) | 0x02;  ///< 
    constexpr uint8_t udp_action_heartbeat = (udp_service_id << 4) | 0x03;  ///< (no params), used to detect connection loss to master :  must send at least one heartbeat every ~30ms or all motors are stopped

    // OpenAPI route metadata
    constexpr const char desc_register[]      PROGMEM = "Register the calling client (identified by its IP address) as the master controller. Must provide the server-generated token shown in MODE_APP_LOG.";
    constexpr const char desc_unregister[]    PROGMEM = "Unregister the current master. Only the currently registered master IP can call this endpoint.";
    constexpr const char desc_master[]        PROGMEM = "Return the IP address of the currently registered master and the master status.";
    constexpr const char desc_token[]         PROGMEM = "Retrieve the server-generated token required for master registration.";
}

// ---------------------------------------------------------------------------
// Random token generation
// ---------------------------------------------------------------------------

std::string AmakerBotService::generateRandomToken()
{
    const char charset[] = "0123456789ABCDEF";
    const int charset_size = 16;
    std::string token;
    
    for (int i = 0; i < 5; i++)
    {
        int rand_idx = esp_random() % charset_size;
        token += charset[rand_idx];
    }
    return token;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

/**
 * @brief Register a client as master (thread-safe) and log to app_info_logger.
 */
void AmakerBotService::registerMaster(const std::string &ip)
{
    if (master_mutex_ && xSemaphoreTake(master_mutex_, pdMS_TO_TICKS(100)))
    {
        master_ip_ = ip;
        xSemaphoreGive(master_mutex_);
    }

    // Reset heartbeat watchdog for the new master session
    last_heartbeat_ms_    = 0;
    heartbeat_active_     = false;
    heartbeat_timed_out_  = false;

    // Log to app_info_logger — visible in MODE_APP_LOG on the screen
    app_info_logger.info(
        progmem_to_string(AmakerBotConsts::msg_registered) + ip);

#ifdef VERBOSE_DEBUG
    if (logger)
        logger->debug(std::string("AmakerBot: master set ip=") + ip);
#endif
}

/**
 * @brief Clear master registration (thread-safe) and log to app_info_logger.
 */
void AmakerBotService::unregisterMaster()
{
    std::string prev_ip;
    if (master_mutex_ && xSemaphoreTake(master_mutex_, pdMS_TO_TICKS(100)))
    {
        prev_ip    = master_ip_;
        master_ip_ = "";
        xSemaphoreGive(master_mutex_);
    }

    // Clear heartbeat watchdog
    last_heartbeat_ms_    = 0;
    heartbeat_active_     = false;
    heartbeat_timed_out_  = false;

    app_info_logger.info(progmem_to_string(AmakerBotConsts::msg_unregistered));

#ifdef VERBOSE_DEBUG
    if (logger)
        logger->debug(std::string("AmakerBot: master cleared (was ") + prev_ip + ")");
#endif
}

// ---------------------------------------------------------------------------
// IsServiceInterface
// ---------------------------------------------------------------------------

bool AmakerBotService::initializeService()
{
    master_mutex_ = xSemaphoreCreateMutex();
    if (!master_mutex_)
    {
        if (logger)
            logger->error(progmem_to_string(AmakerBotConsts::msg_mutex_failed));
        return false;
    }

    // Generate the random server token once
    server_token_ = generateRandomToken();

    // Log the token to app_info_logger so it's visible in MODE_APP_LOG
    app_info_logger.info(
        progmem_to_string(AmakerBotConsts::msg_token_generated) + server_token_);

#ifdef VERBOSE_DEBUG
    if (logger)
        logger->debug(std::string("AmakerBot: token generated: ") + server_token_);
#endif

    setServiceStatus(INITIALIZED);
    return true;
}

bool AmakerBotService::stopService()
{
    if (master_mutex_)
    {
        vSemaphoreDelete(master_mutex_);
        master_mutex_ = nullptr;
    }
    master_ip_.clear();
    setServiceStatus(STOPPED);
    return true;
}

std::string AmakerBotService::getServiceName()
{
    return progmem_to_string(AmakerBotConsts::str_service_name);
}

std::string AmakerBotService::getServiceSubPath()
{
    return progmem_to_string(AmakerBotConsts::path_service);
}

// ---------------------------------------------------------------------------
// Public accessors
// ---------------------------------------------------------------------------

bool AmakerBotService::isMaster(const std::string &ip) const
{
    return !master_ip_.empty() && master_ip_ == ip;
}

std::string AmakerBotService::getMasterIP() const
{
    return master_ip_;
}

std::string AmakerBotService::getServerToken() const
{
    return server_token_;
}

// ---------------------------------------------------------------------------
// Public programmatic control
// ---------------------------------------------------------------------------

bool AmakerBotService::setMasterIfTokenValid(const std::string &ip, const std::string &token)
{
    if (token != server_token_)
        return false; // Token mismatch
    registerMaster(ip);
    return true;
}

void AmakerBotService::clearMaster()
{
    unregisterMaster();
}

// ---------------------------------------------------------------------------
// UDP message handler
// ---------------------------------------------------------------------------

bool AmakerBotService::messageHandler(const std::string &message,
                                       const IPAddress &remoteIP,
                                       uint16_t remotePort)
{
    
    

    if (message[0] == AmakerBotConsts::udp_action_master_register)
    {

        std::string token = message.substr(1);
        if (token.empty())
            return true; // claimed but ignored — no token provided

        // Validate token against server_token_
        if (token != server_token_)
            return true; // claimed but ignored — invalid token

        registerMaster(remoteIP.toString().c_str());
        return true;
    }

    if (message[0] == AmakerBotConsts::udp_action_master_unregister)
    {
        if (!isMaster(remoteIP.toString().c_str()))
            return true; // claimed but ignored — not the master
        unregisterMaster();
        return true;
    }
    if (message[0] == AmakerBotConsts::udp_action_heartbeat)
    {
        // Only accept heartbeats from the registered master
        if (!isMaster(remoteIP.toString().c_str()))
            return false; // not our message

        last_heartbeat_ms_   = millis();
        heartbeat_active_    = true;
        heartbeat_timed_out_ = false; // restore flag so timeout can fire again

        return true;
    }
    return false; // not our message
}

// ---------------------------------------------------------------------------
// Heartbeat watchdog
// ---------------------------------------------------------------------------

/**
 * @brief Stop all motors and servos when the master heartbeat times out.
 * @details Must be called periodically (e.g. every 10 ms from the UDP task).
 *          The emergency stop fires only on the transition into the timed-out
 *          state to avoid calling setAllMotorsSpeed(0) on every tick.
 *          The flag resets automatically when the next valid heartbeat arrives.
 */
void AmakerBotService::checkHeartbeatTimeout()
{
    // Nothing to watch if no master is registered or no heartbeat has been sent yet
    if (master_ip_.empty() || !heartbeat_active_)
        return;

    // we're not at risk of millis() overflow during a game session as millis() rolls over every ~50 days :)
    if ((millis() - last_heartbeat_ms_) > AmakerBotConsts::heartbeat_timeout_ms)
    {
        if (!heartbeat_timed_out_)
        {
            heartbeat_timed_out_ = true;

            // Emergency stop — halt all DC motors and continuous servos
            servo_service.setAllMotorsSpeed(0);
            servo_service.setAllServoSpeed(0);

            app_info_logger.error(progmem_to_string(AmakerBotConsts::msg_heartbeat_timeout));
#ifdef VERBOSE_DEBUG
            if (logger)
                logger->error(progmem_to_string(AmakerBotConsts::msg_heartbeat_timeout));
#endif
        }
    }
    else if (heartbeat_timed_out_)
    {
        // Heartbeat just came back — clear the timed-out flag
        heartbeat_timed_out_ = false;
        app_info_logger.info(progmem_to_string(AmakerBotConsts::msg_heartbeat_restored));
    }
}

// ---------------------------------------------------------------------------
// HTTP routes
// ---------------------------------------------------------------------------

bool AmakerBotService::registerRoutes()
{
    std::string path_register   = getPath("register");
    std::string path_unregister = getPath("unregister");
    std::string path_master     = getPath("master");
    std::string path_token      = getPath("token");

#ifdef VERBOSE_DEBUG
    if (logger)
        logger->debug(progmem_to_string(RoutesConsts::msg_registering) + path_register);
#endif

    // ------------------------------------------------------------------
    // POST /api/amakerbot/v1/register?token=<token>
    // ------------------------------------------------------------------
    std::vector<OpenAPIParameter> register_params;
    register_params.push_back(
        OpenAPIParameter(AmakerBotConsts::param_token,
                         RoutesConsts::type_string,
                         RoutesConsts::in_query,
                         "Server-generated token (shown in MODE_APP_LOG)",
                         true));

    std::vector<OpenAPIResponse> register_responses;
    OpenAPIResponse reg_ok(200, AmakerBotConsts::resp_registered);
    reg_ok.schema  = R"({"type":"object","properties":{"result":{"type":"string"},"ip":{"type":"string"}}})";
    reg_ok.example = R"({"result":"ok","ip":"192.168.1.42"})";
    register_responses.push_back(reg_ok);
    register_responses.push_back(OpenAPIResponse(400, AmakerBotConsts::resp_missing_token));
    register_responses.push_back(OpenAPIResponse(401, AmakerBotConsts::resp_invalid_token));
    register_responses.push_back(createServiceNotStartedResponse());

    registerOpenAPIRoute(
        OpenAPIRoute(path_register.c_str(), RoutesConsts::method_post,
                     AmakerBotConsts::desc_register,
                     AmakerBotConsts::tag_service, false,
                     register_params, register_responses));

    webserver.on(path_register.c_str(), HTTP_POST,
        [this](AsyncWebServerRequest *request)
        {
            if (!checkServiceStarted(request)) return;

            if (!request->hasParam(FPSTR(AmakerBotConsts::param_token), true) &&
                !request->hasParam(FPSTR(AmakerBotConsts::param_token)))
            {
                JsonDocument err;
                err[FPSTR(RoutesConsts::result)]  = FPSTR(RoutesConsts::result_err);
                err[FPSTR(RoutesConsts::message)] = FPSTR(AmakerBotConsts::resp_missing_token);
                String out;
                serializeJson(err, out);
                request->send(400, FPSTR(RoutesConsts::mime_json), out);
                return;
            }

            // Accept token from query string or POST body param
            const AsyncWebParameter *p = request->hasParam(FPSTR(AmakerBotConsts::param_token), true)
                ? request->getParam(FPSTR(AmakerBotConsts::param_token), true)
                : request->getParam(FPSTR(AmakerBotConsts::param_token));

            std::string token = p->value().c_str();
            std::string ip    = request->client()->remoteIP().toString().c_str();

            // Validate token against server_token_
            if (token != server_token_)
            {
                JsonDocument err;
                err[FPSTR(RoutesConsts::result)]  = FPSTR(RoutesConsts::result_err);
                err[FPSTR(RoutesConsts::message)] = FPSTR(AmakerBotConsts::resp_invalid_token);
                String out;
                serializeJson(err, out);
                request->send(401, FPSTR(RoutesConsts::mime_json), out);
                return;
            }

            registerMaster(ip);

            JsonDocument doc;
            doc[FPSTR(RoutesConsts::result)]         = FPSTR(RoutesConsts::result_ok);
            doc[FPSTR(AmakerBotConsts::field_ip)]    = ip.c_str();
            String out;
            serializeJson(doc, out);
            request->send(200, FPSTR(RoutesConsts::mime_json), out);
        });

    // ------------------------------------------------------------------
    // GET /api/amakerbot/v1/master
    // ------------------------------------------------------------------
    std::vector<OpenAPIResponse> master_responses;
    OpenAPIResponse mst_ok(200, AmakerBotConsts::resp_master_info);
    mst_ok.schema  = R"({"type":"object","properties":{"registered":{"type":"boolean"},"ip":{"type":"string","description":"Master IP address, empty when no master is registered"}}})";  
    mst_ok.example = R"({"registered":true,"ip":"192.168.1.42"})";
    master_responses.push_back(mst_ok);
    master_responses.push_back(createServiceNotStartedResponse());

    registerOpenAPIRoute(
        OpenAPIRoute(path_master.c_str(), RoutesConsts::method_get,
                     AmakerBotConsts::desc_master,
                     AmakerBotConsts::tag_service, false,
                     {}, master_responses));

    webserver.on(path_master.c_str(), HTTP_GET,
        [this](AsyncWebServerRequest *request)
        {
            if (!checkServiceStarted(request)) return;

            std::string ip;
            bool registered = false;
            if (master_mutex_ && xSemaphoreTake(master_mutex_, pdMS_TO_TICKS(50)))
            {
                ip         = master_ip_;
                registered = !master_ip_.empty();
                xSemaphoreGive(master_mutex_);
            }

            JsonDocument doc;
            doc[FPSTR(AmakerBotConsts::field_registered)] = registered;
            doc[FPSTR(AmakerBotConsts::field_ip)]         = ip.c_str();
            String out;
            serializeJson(doc, out);
            request->send(200, FPSTR(RoutesConsts::mime_json), out);
        });

    // ------------------------------------------------------------------
    // GET /api/amakerbot/v1/token
    // ------------------------------------------------------------------
    std::vector<OpenAPIResponse> token_responses;
    OpenAPIResponse tok_ok(200, AmakerBotConsts::resp_token_info);
    tok_ok.schema  = R"({"type":"object","properties":{"token":{"type":"string","description":"Server-generated token for master registration"}}})";  
    tok_ok.example = R"({"token":"A3K9B"})";
    token_responses.push_back(tok_ok);
    token_responses.push_back(createServiceNotStartedResponse());

    registerOpenAPIRoute(
        OpenAPIRoute(path_token.c_str(), RoutesConsts::method_get,
                     AmakerBotConsts::desc_token,
                     AmakerBotConsts::tag_service, false,
                     {}, token_responses));

    webserver.on(path_token.c_str(), HTTP_GET,
        [this](AsyncWebServerRequest *request)
        {
            if (!checkServiceStarted(request)) return;

            JsonDocument doc;
            doc[FPSTR(AmakerBotConsts::field_token)] = server_token_.c_str();
            String out;
            serializeJson(doc, out);
            request->send(200, FPSTR(RoutesConsts::mime_json), out);
        });

    // ------------------------------------------------------------------
    // POST /api/amakerbot/v1/unregister
    // ------------------------------------------------------------------
    std::vector<OpenAPIResponse> unreg_responses;
    unreg_responses.push_back(OpenAPIResponse(200, AmakerBotConsts::resp_unregistered));
    unreg_responses.push_back(OpenAPIResponse(403, AmakerBotConsts::resp_unauthorized));
    unreg_responses.push_back(createServiceNotStartedResponse());

    registerOpenAPIRoute(
        OpenAPIRoute(path_unregister.c_str(), RoutesConsts::method_post,
                     AmakerBotConsts::desc_unregister,
                     AmakerBotConsts::tag_service, false,
                     {}, unreg_responses));

    webserver.on(path_unregister.c_str(), HTTP_POST,
        [this](AsyncWebServerRequest *request)
        {
            if (!checkServiceStarted(request)) return;

            std::string caller_ip = request->client()->remoteIP().toString().c_str();

            // Accept: matching IP only
            if (!isMaster(caller_ip))
            {
                JsonDocument err;
                err[FPSTR(RoutesConsts::result)]  = FPSTR(RoutesConsts::result_err);
                err[FPSTR(RoutesConsts::message)] = FPSTR(AmakerBotConsts::resp_unauthorized);
                String out;
                serializeJson(err, out);
                request->send(403, FPSTR(RoutesConsts::mime_json), out);
                return;
            }

            unregisterMaster();

            JsonDocument doc;
            doc[FPSTR(RoutesConsts::result)] = FPSTR(RoutesConsts::result_ok);
            String out;
            serializeJson(doc, out);
            request->send(200, FPSTR(RoutesConsts::mime_json), out);
        });

    registerServiceStatusRoute(this);
    registerSettingsRoutes(this);
    return true;
}
