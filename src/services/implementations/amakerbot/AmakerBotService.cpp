/**
 * @file AmakerBotService.cpp
 * @brief Master-registration service for K10-Bot
 * @details Exposed routes:
 *          - POST /api/amakerbot/v1/register?token=<token>  Register caller IP as master (if token valid)
 *          - GET  /api/amakerbot/v1/master                  Query current master info
 *          - POST /api/amakerbot/v1/unregister              Clear master (master IP only)
 *          - GET  /api/amakerbot/v1/token                   Retrieve server-generated token
 *          - GET  /api/amakerbot/v1/display                 Get current TFT display mode
 *          - POST /api/amakerbot/v1/display?mode=<mode>     Set TFT display mode (APP_UI|APP_LOG|DEBUG_LOG|ESP_LOG)
 *          - POST /api/amakerbot/v1/display/next            Cycle to next display mode (same as button A)
 *          - GET  /api/amakerbot/v1/name                    Get current bot name
 *          - POST /api/amakerbot/v1/name?name=<name>        Set bot name (max 32 chars)
 *
 *          UDP protocol (service_id 0x4):
 *          - [0x41]<token>  Register UDP sender as master (if token valid);
 *          - [0x42]         Clear master (master IP only);
 *          - [0x43]         Heartbeat keep-alive — must arrive every ≤50 ms or all motors are stopped
 *          - [0x45]         Get bot name — server replies with [0x45][name bytes];
 *          - [0x46]<name>   Set bot name (master only) — server replies with [0x46][status];
 *
 *          On service init, a random 5-character alphanumeric token is generated
 *          and logged to app_info_logger so it appears in MODE_APP_LOG on the screen.
 */

#include "services/AmakerBotService.h"
#include "services/ServoService.h"
#include "services/UDPService.h"
#include "FlashStringHelper.h"
#include "utb2026.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <freertos/semphr.h>
#include <random>

// Access globals defined in main.cpp
extern RollingLogger app_info_logger;
extern ServoService servo_service;
extern UDPService udp_service;
extern UTB2026 ui;

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
namespace AmakerBotConsts
{
    constexpr const char path_service[] PROGMEM = "amakerbot/v1";
    constexpr const char str_service_name[] PROGMEM = "AmakerBot";
    constexpr const char tag_service[] PROGMEM = "AmakerBot";

    // JSON field names
    constexpr const char field_ip[] PROGMEM = "ip";
    constexpr const char field_token[] PROGMEM = "token";
    constexpr const char field_registered[] PROGMEM = "registered";

    // HTTP parameter names
    constexpr const char param_token[] PROGMEM = "token";

    // Log messages (written to app_info_logger → visible in MODE_APP_LOG)
    constexpr const char msg_token_generated[] PROGMEM = "[AMAKERBOT] Token: ";
    constexpr const char msg_registered[] PROGMEM = "[MASTER] registered from ";
    constexpr const char msg_unregistered[] PROGMEM = "[MASTER] unregistered";

    // Service-internal log messages (debug_logger)
    constexpr const char msg_mutex_failed[] PROGMEM = "AmakerBot: mutex creation failed";

    // HTTP response descriptions
    constexpr const char resp_registered[] PROGMEM = "Master registered successfully";

    constexpr const char resp_already_have_master[] PROGMEM = "Already have a master.";
    constexpr const char resp_ignore_registration[] PROGMEM = "Master registration demand ignored.";
    constexpr const char resp_ignore_unregistration[] PROGMEM = "Master unregistration demand ignored.";

    constexpr const char resp_unregistered[] PROGMEM = "Master unregistered";
    constexpr const char resp_master_info[] PROGMEM = "Current master info retrieved";
    constexpr const char resp_token_info[] PROGMEM = "Server token retrieved";
    constexpr const char resp_missing_token[] PROGMEM = "Missing token parameter";
    constexpr const char resp_invalid_token[] PROGMEM = "Invalid token";
    constexpr const char resp_unauthorized[] PROGMEM = "Not authorized: you are not the master";

    // Heartbeat watchdog
    constexpr uint32_t heartbeat_timeout_ms = 50; ///< ms without heartbeat before all motors are stopped
    constexpr const char msg_heartbeat_timeout[] PROGMEM = "[AMAKERBOT] Heartbeat timeout - stopping motors";
    constexpr const char msg_heartbeat_restored[] PROGMEM = "[AMAKERBOT] Heartbeat restored";

    // UDP command prefixes
    constexpr uint8_t udp_service_id = 0x04;                                       ///< Unique ID for DFR1216 Service (high nibble of action byte)
    constexpr uint8_t udp_action_master_register = (udp_service_id << 4) | 0x01;   ///< 0x41 - followed by token string, registers sender IP as master if token valid; server responds with udp_action_master_register + 0x00 on success or + 0x01 on failure (invalid token) — e.g. [0x41][0x00] for success, [0x41][0x01] for invalid token
    constexpr uint8_t udp_action_master_unregister = (udp_service_id << 4) | 0x02; ///< 0x42 -
    constexpr uint8_t udp_action_heartbeat = (udp_service_id << 4) | 0x03;         ///< 0x43 - (no params), used to detect connection loss to master :  must send at least one heartbeat every ~30ms or all motors are stopped
    constexpr uint8_t udp_action_ping = (udp_service_id << 4) | 0x04;              ///< 0x44 - (no params), used to detect connection loss to master :  must send at least one heartbeat every ~30ms or all motors are stopped
    constexpr uint8_t udp_action_get_name = (udp_service_id << 4) | 0x05;          ///< 0x45 - (no params), server replies with [0x45][name bytes]
    constexpr uint8_t udp_action_set_name = (udp_service_id << 4) | 0x06;          ///< 0x46 - followed by name string (max 32 chars, master only); server replies with [0x46][status]

    // OpenAPI route metadata
    constexpr const char desc_register[] PROGMEM = "Register the calling client (identified by its IP address) as the master controller. Must provide the server-generated token shown in MODE_APP_LOG.";
    constexpr const char desc_unregister[] PROGMEM = "Unregister the current master. Only the currently registered master IP can call this endpoint.";
    constexpr const char desc_master[] PROGMEM = "Return the IP address of the currently registered master and the master status.";
    constexpr const char desc_token[] PROGMEM = "Retrieve the server-generated token required for master registration.";

    // Display mode routes
    constexpr const char path_display[] PROGMEM = "display";
    constexpr const char path_display_next[] PROGMEM = "display/next";
    constexpr const char field_mode[] PROGMEM = "mode";
    constexpr const char field_mode_index[] PROGMEM = "mode_index";
    constexpr const char param_mode[] PROGMEM = "mode";
    constexpr const char mode_app_ui[] PROGMEM = "APP_UI";
    constexpr const char mode_app_log[] PROGMEM = "APP_LOG";
    constexpr const char mode_debug_log[] PROGMEM = "DEBUG_LOG";
    constexpr const char mode_esp_log[] PROGMEM = "ESP_LOG";
    constexpr const char desc_display_get[] PROGMEM = "Get current TFT display mode.";
    constexpr const char desc_display_next[] PROGMEM = "Cycle TFT display to next mode (same as pressing button A).";
    constexpr const char desc_display_set[] PROGMEM = "Set TFT display mode directly. Accepted values: APP_UI, APP_LOG, DEBUG_LOG, ESP_LOG.";
    constexpr const char resp_display_ok[] PROGMEM = "Current display mode";
    constexpr const char resp_display_changed[] PROGMEM = "Display mode changed";
    constexpr const char resp_invalid_mode[] PROGMEM = "Invalid mode value";

    // Bot name
    constexpr const char path_name[] PROGMEM = "name";
    constexpr const char field_name[] PROGMEM = "name";
    constexpr const char param_name[] PROGMEM = "name";
    constexpr const char default_bot_name[] PROGMEM = "K10-Bot";
    constexpr const char desc_name_get[] PROGMEM = "Get the current bot name.";
    constexpr const char desc_name_set[] PROGMEM = "Set the bot name (max 32 characters).";
    constexpr const char resp_name_ok[] PROGMEM = "Bot name retrieved";
    constexpr const char resp_name_set[] PROGMEM = "Bot name updated";
    constexpr const char resp_missing_name[] PROGMEM = "Missing or empty name parameter";
    constexpr const char msg_name_changed[] PROGMEM = "[AMAKERBOT] Bot name set to: ";
}

// ---------------------------------------------------------------------------
// Random token generation
// ---------------------------------------------------------------------------

std::string AmakerBotService::generateRandomToken()
{
    const char charset[] = "0123456789abcdef";
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
 * @brief Register a client as master (thread-safe).
 * @details Cancels with a warning if ip is empty or ip is already the master.
 *          Rejects with an error if a different master is already registered.
 *          Token validation is the caller's responsibility.
 * @param ip IP address of the new master
 */
bool AmakerBotService::registerMaster(const std::string &ip)
{
    if (ip.empty() || ip == getMasterIP())
    {
        app_info_logger.warning(progmem_to_string(AmakerBotConsts::resp_ignore_registration));
        return false;
    }

    if (!getMasterIP().empty())
    {
        app_info_logger.error(progmem_to_string(AmakerBotConsts::resp_already_have_master));
        return false; // a different master is already registered
    }

    if (master_mutex_ && xSemaphoreTake(master_mutex_, pdMS_TO_TICKS(100)))
    {
        master_ip_ = ip;
        xSemaphoreGive(master_mutex_);
    }

    // Reset heartbeat watchdog for the new master session
    last_heartbeat_ms_ = 0;
    heartbeat_active_ = false;
    heartbeat_timed_out_ = false;

    // Log to app_info_logger — visible in MODE_APP_LOG on the screen
    app_info_logger.info(
        progmem_to_string(AmakerBotConsts::msg_registered) + ip);

#ifdef VERBOSE_DEBUG
    if (logger)
        logger->debug(std::string("AmakerBot: master set ip=") + ip);
#endif
    return true;
}

/**
 * @brief Clear master registration (thread-safe) and log to app_info_logger.
 */
bool AmakerBotService::unregisterMaster(const std::string &ip)
{
    if (ip.empty() || ip != getMasterIP())
    {
        app_info_logger.warning(progmem_to_string(AmakerBotConsts::resp_ignore_unregistration));
        return false;
    }
    std::string prev_ip;
    if (master_mutex_ && xSemaphoreTake(master_mutex_, pdMS_TO_TICKS(100)))
    {
        prev_ip = master_ip_;
        master_ip_ = "";
        xSemaphoreGive(master_mutex_);
    }

#ifdef VERBOSE_DEBUG
    if (logger)
        logger->debug(std::string("AmakerBot: master cleared (was ") + prev_ip + ")");
#endif

    app_info_logger.info(progmem_to_string(AmakerBotConsts::msg_unregistered));
    return true;
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

    // Set default bot name
    bot_name_ = progmem_to_string(AmakerBotConsts::default_bot_name);

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
    if (master_mutex_ && xSemaphoreTake(master_mutex_, pdMS_TO_TICKS(10)))
    {
        bool result = !master_ip_.empty() && master_ip_ == ip;
        xSemaphoreGive(master_mutex_);
        return result;
    }
    return false;
}

std::string AmakerBotService::getMasterIP() const
{
    if (master_mutex_ && xSemaphoreTake(master_mutex_, pdMS_TO_TICKS(10)))
    {
        std::string ip = master_ip_;
        xSemaphoreGive(master_mutex_);
        return ip;
    }
    return "";
}

std::string AmakerBotService::getServerToken() const
{
    return server_token_;
}

std::string AmakerBotService::getBotName() const
{
    if (master_mutex_ && xSemaphoreTake(master_mutex_, pdMS_TO_TICKS(10)))
    {
        std::string name = bot_name_;
        xSemaphoreGive(master_mutex_);
        return name;
    }
    return progmem_to_string(AmakerBotConsts::default_bot_name);
}

void AmakerBotService::setBotName(const std::string &name)
{
    if (name.empty() || name.size() > 32)
        return;
    if (master_mutex_ && xSemaphoreTake(master_mutex_, pdMS_TO_TICKS(100)))
    {
        bot_name_ = name;
        xSemaphoreGive(master_mutex_);
    }
    app_info_logger.info(progmem_to_string(AmakerBotConsts::msg_name_changed) + name);
}

// ---------------------------------------------------------------------------
// Public programmatic control
// ---------------------------------------------------------------------------

bool AmakerBotService::setMasterIfTokenValid(const std::string &ip, const std::string &token)
{
    if (token != getServerToken())
        return false; // Token mismatch
    registerMaster(ip);
    return true;
}

// ---------------------------------------------------------------------------
// UDP helpers
// ---------------------------------------------------------------------------

/**
 * @brief Send a UDP reply: echo of the full incoming message followed by a status byte.
 * @param message     Original incoming message payload (echoed verbatim)
 * @param status_byte 0x00 = success, 0x01 = failure/rejection
 * @param remoteIP    Destination IP address
 * @param remotePort  Destination UDP port
 */
void AmakerBotService::udp_reply(const std::string &message, UDPResponseStatus status,
                                 const IPAddress &remoteIP, uint16_t remotePort)
{
    std::string reply = message;
    reply += static_cast<char>(status);
    udp_service.sendReply(reply, remoteIP, remotePort);
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
        {
            udp_reply(message, UDPResponseStatus::IGNORED, remoteIP, remotePort); // no token provided
            return true;
        }

        if (token != getServerToken())
        {
            udp_reply(message, UDPResponseStatus::DENIED, remoteIP, remotePort); // invalid token
            return true;
        }

        bool ok = registerMaster(remoteIP.toString().c_str());
        udp_reply(message, ok ? UDPResponseStatus::SUCCESS : UDPResponseStatus::IGNORED, remoteIP, remotePort);
        return true;
    }

    if (message[0] == AmakerBotConsts::udp_action_master_unregister)
    {
        if (!isMaster(remoteIP.toString().c_str()))
        {
            udp_reply(message, UDPResponseStatus::DENIED, remoteIP, remotePort); // not the master
            return true;                                                         // not the master — let other handlers try
        }
        bool ok = unregisterMaster(remoteIP.toString().c_str());
        udp_reply(message, ok ? UDPResponseStatus::SUCCESS : UDPResponseStatus::ERROR, remoteIP, remotePort);
        return true;
    }

    if (message[0] == AmakerBotConsts::udp_action_heartbeat)
    {
        // Only accept heartbeats from the registered master
        if (!isMaster(remoteIP.toString().c_str()))
        {
            udp_reply(message, UDPResponseStatus::DENIED, remoteIP, remotePort); // not the master
            return true;                                                         // not the master — let other handlers try
        }

        last_heartbeat_ms_ = millis();
        heartbeat_active_ = true;
        heartbeat_timed_out_ = false; // restore flag so timeout can fire again

        return true;
    }
    if (message[0] == AmakerBotConsts::udp_action_ping)
    {
        // Only accept heartbeats from the registered master
        if (!isMaster(remoteIP.toString().c_str()))
            return false; // not our message

        // Echo the ping payload back: [action:1B][id:4B]
        // The client uses the echoed ID to match the reply and measure RTT.
        if (message.size() >= 5)
            udp_service.sendReply(message.substr(0, 5), remoteIP, remotePort);

        return true;
    }

    if (message[0] == AmakerBotConsts::udp_action_get_name)
    {
        // Reply: [0x45][name bytes] — available to anyone
        std::string reply;
        reply += static_cast<char>(AmakerBotConsts::udp_action_get_name);
        reply += getBotName();
        udp_service.sendReply(reply, remoteIP, remotePort);
        return true;
    }

    if (message[0] == AmakerBotConsts::udp_action_set_name)
    {
        // Only the registered master may rename the bot
        if (!isMaster(remoteIP.toString().c_str()))
        {
            udp_reply(message, UDPResponseStatus::DENIED, remoteIP, remotePort);
            return true;
        }
        std::string name = message.substr(1);
        if (name.empty() || name.size() > 32)
        {
            udp_reply(message, UDPResponseStatus::ERROR, remoteIP, remotePort);
            return true;
        }
        setBotName(name);
        udp_reply(message, UDPResponseStatus::SUCCESS, remoteIP, remotePort);
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
    // Check master_ip_ under mutex (std::string is not safe to read across cores)
    bool has_master = false;
    if (master_mutex_ && xSemaphoreTake(master_mutex_, pdMS_TO_TICKS(5)))
    {
        has_master = !master_ip_.empty();
        xSemaphoreGive(master_mutex_);
    }

    // Nothing to watch if no master is registered or no heartbeat has been sent yet
    if (!has_master || !heartbeat_active_)
        return;

    // we're not at risk of millis() overflow during a game session as millis() rolls over every ~50 days :)
    if ((millis() - last_heartbeat_ms_) > AmakerBotConsts::heartbeat_timeout_ms)
    {
        if (!heartbeat_timed_out_)
        {
            heartbeat_timed_out_ = true;

            // Emergency stop — halt all DC motors and continuous servos (once)
            servo_service.setAllMotorsSpeed(0);
            servo_service.setAllServoSpeed(0);
            ui.set_info(ui.KEY_UDP_STATE, "down");

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
        ui.set_info(ui.KEY_UDP_STATE, "up");
        app_info_logger.info(progmem_to_string(AmakerBotConsts::msg_heartbeat_restored));
    }
}

// ---------------------------------------------------------------------------
// HTTP routes
// ---------------------------------------------------------------------------

bool AmakerBotService::registerRoutes()
{
    std::string path_register = getPath("register");
    std::string path_unregister = getPath("unregister");
    std::string path_master = getPath("master");
    std::string path_token = getPath("token");

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
    reg_ok.schema = R"({"type":"object","properties":{"result":{"type":"string"},"ip":{"type":"string"}}})";
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
                     if (!checkServiceStarted(request))
                         return;

                     if (!request->hasParam(FPSTR(AmakerBotConsts::param_token), true) &&
                         !request->hasParam(FPSTR(AmakerBotConsts::param_token)))
                     {
                         JsonDocument err;
                         err[FPSTR(RoutesConsts::result)] = FPSTR(RoutesConsts::result_err);
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
                     std::string ip = request->client()->remoteIP().toString().c_str();

                     // Validate token against server_token_
                     if (token != getServerToken())
                     {
                         JsonDocument err;
                         err[FPSTR(RoutesConsts::result)] = FPSTR(RoutesConsts::result_err);
                         err[FPSTR(RoutesConsts::message)] = FPSTR(AmakerBotConsts::resp_invalid_token);
                         String out;
                         serializeJson(err, out);
                         request->send(401, FPSTR(RoutesConsts::mime_json), out);
                         return;
                     }

                     registerMaster(ip);

                     JsonDocument doc;
                     doc[FPSTR(RoutesConsts::result)] = FPSTR(RoutesConsts::result_ok);
                     doc[FPSTR(AmakerBotConsts::field_ip)] = ip.c_str();
                     String out;
                     serializeJson(doc, out);
                     request->send(200, FPSTR(RoutesConsts::mime_json), out);
                 });

    // ------------------------------------------------------------------
    // GET /api/amakerbot/v1/master
    // ------------------------------------------------------------------
    std::vector<OpenAPIResponse> master_responses;
    OpenAPIResponse mst_ok(200, AmakerBotConsts::resp_master_info);
    mst_ok.schema = R"({"type":"object","properties":{"registered":{"type":"boolean"},"ip":{"type":"string","description":"Master IP address, empty when no master is registered"}}})";
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
                     if (!checkServiceStarted(request))
                         return;

                     std::string ip;
                     bool registered = false;
                     if (master_mutex_ && xSemaphoreTake(master_mutex_, pdMS_TO_TICKS(50)))
                     {
                         ip = master_ip_;
                         registered = !master_ip_.empty();
                         xSemaphoreGive(master_mutex_);
                     }

                     JsonDocument doc;
                     doc[FPSTR(AmakerBotConsts::field_registered)] = registered;
                     doc[FPSTR(AmakerBotConsts::field_ip)] = ip.c_str();
                     String out;
                     serializeJson(doc, out);
                     request->send(200, FPSTR(RoutesConsts::mime_json), out);
                 });

    // ------------------------------------------------------------------
    // GET /api/amakerbot/v1/token
    // ------------------------------------------------------------------
    std::vector<OpenAPIResponse> token_responses;
    OpenAPIResponse tok_ok(200, AmakerBotConsts::resp_token_info);
    tok_ok.schema = R"({"type":"object","properties":{"token":{"type":"string","description":"Server-generated token for master registration"}}})";
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
                     if (!checkServiceStarted(request))
                         return;

                     JsonDocument doc;
                     doc[FPSTR(AmakerBotConsts::field_token)] = getServerToken().c_str();
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
                     if (!checkServiceStarted(request))
                         return;

                     std::string caller_ip = request->client()->remoteIP().toString().c_str();

                     // Accept: matching IP only
                     if (!isMaster(caller_ip))
                     {
                         JsonDocument err;
                         err[FPSTR(RoutesConsts::result)] = FPSTR(RoutesConsts::result_err);
                         err[FPSTR(RoutesConsts::message)] = FPSTR(AmakerBotConsts::resp_unauthorized);
                         String out;
                         serializeJson(err, out);
                         request->send(403, FPSTR(RoutesConsts::mime_json), out);
                         return;
                     }

                     unregisterMaster(request->client()->remoteIP().toString().c_str());

                     JsonDocument doc;
                     doc[FPSTR(RoutesConsts::result)] = FPSTR(RoutesConsts::result_ok);
                     String out;
                     serializeJson(doc, out);
                     request->send(200, FPSTR(RoutesConsts::mime_json), out);
                 });

    // ------------------------------------------------------------------
    // Helper: convert DisplayMode enum → name string (PROGMEM safe)
    // ------------------------------------------------------------------
    auto mode_to_string = [](UTB2026::DisplayMode m) -> const char *
    {
        switch (m)
        {
        case UTB2026::MODE_APP_UI:
            return "APP_UI";
        case UTB2026::MODE_APP_LOG:
            return "APP_LOG";
        case UTB2026::MODE_DEBUG_LOG:
            return "DEBUG_LOG";
        case UTB2026::MODE_ESP_LOG:
            return "ESP_LOG";
        default:
            return "UNKNOWN";
        }
    };

    // ------------------------------------------------------------------
    // GET /api/amakerbot/v1/display
    // ------------------------------------------------------------------
    std::string path_display = getPath(progmem_to_string(AmakerBotConsts::path_display).c_str());
    std::string path_display_next = getPath(progmem_to_string(AmakerBotConsts::path_display_next).c_str());

    std::vector<OpenAPIResponse> display_get_responses;
    OpenAPIResponse dsp_ok(200, AmakerBotConsts::resp_display_ok);
    dsp_ok.schema = R"({"type":"object","properties":{"mode":{"type":"string","enum":["APP_UI","APP_LOG","DEBUG_LOG","ESP_LOG"]},"mode_index":{"type":"integer"}}})";
    dsp_ok.example = R"({"mode":"APP_LOG","mode_index":1})";
    display_get_responses.push_back(dsp_ok);
    display_get_responses.push_back(createServiceNotStartedResponse());

    registerOpenAPIRoute(
        OpenAPIRoute(path_display.c_str(), RoutesConsts::method_get,
                     AmakerBotConsts::desc_display_get,
                     AmakerBotConsts::tag_service, false,
                     {}, display_get_responses));

    webserver.on(path_display.c_str(), HTTP_GET,
                 [this, mode_to_string](AsyncWebServerRequest *request)
                 {
                     if (!checkServiceStarted(request))
                         return;
                     UTB2026::DisplayMode m = ui.get_display_mode();
                     JsonDocument doc;
                     doc[FPSTR(AmakerBotConsts::field_mode)] = mode_to_string(m);
                     doc[FPSTR(AmakerBotConsts::field_mode_index)] = static_cast<int>(m);
                     String out;
                     serializeJson(doc, out);
                     request->send(200, FPSTR(RoutesConsts::mime_json), out);
                 });

    // ------------------------------------------------------------------
    // POST /api/amakerbot/v1/display/next
    // ------------------------------------------------------------------
    std::vector<OpenAPIResponse> display_next_responses;
    OpenAPIResponse dsp_next_ok(200, AmakerBotConsts::resp_display_changed);
    dsp_next_ok.schema = R"({"type":"object","properties":{"mode":{"type":"string"},"mode_index":{"type":"integer"}}})";
    dsp_next_ok.example = R"({"mode":"DEBUG_LOG","mode_index":2})";
    display_next_responses.push_back(dsp_next_ok);
    display_next_responses.push_back(createServiceNotStartedResponse());

    registerOpenAPIRoute(
        OpenAPIRoute(path_display_next.c_str(), RoutesConsts::method_post,
                     AmakerBotConsts::desc_display_next,
                     AmakerBotConsts::tag_service, false,
                     {}, display_next_responses));

    webserver.on(path_display_next.c_str(), HTTP_POST,
                 [this, mode_to_string](AsyncWebServerRequest *request)
                 {
                     if (!checkServiceStarted(request))
                         return;
                     ui.next_display_mode();
                     UTB2026::DisplayMode m = ui.get_display_mode();
                     JsonDocument doc;
                     doc[FPSTR(AmakerBotConsts::field_mode)] = mode_to_string(m);
                     doc[FPSTR(AmakerBotConsts::field_mode_index)] = static_cast<int>(m);
                     String out;
                     serializeJson(doc, out);
                     request->send(200, FPSTR(RoutesConsts::mime_json), out);
                 });

    // ------------------------------------------------------------------
    // POST /api/amakerbot/v1/display?mode=<APP_UI|APP_LOG|DEBUG_LOG|ESP_LOG>
    // ------------------------------------------------------------------
    std::vector<OpenAPIParameter> display_set_params;
    display_set_params.push_back(
        OpenAPIParameter(AmakerBotConsts::param_mode,
                         RoutesConsts::type_string,
                         RoutesConsts::in_query,
                         "Target mode: APP_UI, APP_LOG, DEBUG_LOG, ESP_LOG",
                         true));

    std::vector<OpenAPIResponse> display_set_responses;
    display_set_responses.push_back(OpenAPIResponse(200, AmakerBotConsts::resp_display_changed));
    display_set_responses.push_back(OpenAPIResponse(400, AmakerBotConsts::resp_invalid_mode));
    display_set_responses.push_back(createServiceNotStartedResponse());

    registerOpenAPIRoute(
        OpenAPIRoute(path_display.c_str(), RoutesConsts::method_post,
                     AmakerBotConsts::desc_display_set,
                     AmakerBotConsts::tag_service, false,
                     display_set_params, display_set_responses));

    webserver.on(path_display.c_str(), HTTP_POST,
                 [this, mode_to_string](AsyncWebServerRequest *request)
                 {
                     if (!checkServiceStarted(request))
                         return;

                     if (!request->hasParam(FPSTR(AmakerBotConsts::param_mode)) &&
                         !request->hasParam(FPSTR(AmakerBotConsts::param_mode), true))
                     {
                         JsonDocument err;
                         err[FPSTR(RoutesConsts::result)] = FPSTR(RoutesConsts::result_err);
                         err[FPSTR(RoutesConsts::message)] = FPSTR(AmakerBotConsts::resp_invalid_mode);
                         String out;
                         serializeJson(err, out);
                         request->send(400, FPSTR(RoutesConsts::mime_json), out);
                         return;
                     }

                     const AsyncWebParameter *p = request->hasParam(FPSTR(AmakerBotConsts::param_mode), true)
                                                      ? request->getParam(FPSTR(AmakerBotConsts::param_mode), true)
                                                      : request->getParam(FPSTR(AmakerBotConsts::param_mode));

                     std::string mode_str = p->value().c_str();
                     UTB2026::DisplayMode target;
                     bool valid = true;

                     if (mode_str == progmem_to_string(AmakerBotConsts::mode_app_ui))
                         target = UTB2026::MODE_APP_UI;
                     else if (mode_str == progmem_to_string(AmakerBotConsts::mode_app_log))
                         target = UTB2026::MODE_APP_LOG;
                     else if (mode_str == progmem_to_string(AmakerBotConsts::mode_debug_log))
                         target = UTB2026::MODE_DEBUG_LOG;
                     else if (mode_str == progmem_to_string(AmakerBotConsts::mode_esp_log))
                         target = UTB2026::MODE_ESP_LOG;
                     else
                         valid = false;

                     if (!valid)
                     {
                         JsonDocument err;
                         err[FPSTR(RoutesConsts::result)] = FPSTR(RoutesConsts::result_err);
                         err[FPSTR(RoutesConsts::message)] = FPSTR(AmakerBotConsts::resp_invalid_mode);
                         String out;
                         serializeJson(err, out);
                         request->send(400, FPSTR(RoutesConsts::mime_json), out);
                         return;
                     }

                     ui.set_display_mode(target);

                     JsonDocument doc;
                     doc[FPSTR(RoutesConsts::result)] = FPSTR(RoutesConsts::result_ok);
                     doc[FPSTR(AmakerBotConsts::field_mode)] = mode_to_string(target);
                     doc[FPSTR(AmakerBotConsts::field_mode_index)] = static_cast<int>(target);
                     String out;
                     serializeJson(doc, out);
                     request->send(200, FPSTR(RoutesConsts::mime_json), out);
                 });

    // ------------------------------------------------------------------
    // GET /api/amakerbot/v1/name
    // ------------------------------------------------------------------
    std::string path_name = getPath(progmem_to_string(AmakerBotConsts::path_name).c_str());

    std::vector<OpenAPIResponse> name_get_responses;
    OpenAPIResponse name_ok(200, AmakerBotConsts::resp_name_ok);
    name_ok.schema = R"({"type":"object","properties":{"name":{"type":"string"}}})";
    name_ok.example = R"({"name":"K10-Bot"})";
    name_get_responses.push_back(name_ok);
    name_get_responses.push_back(createServiceNotStartedResponse());

    registerOpenAPIRoute(
        OpenAPIRoute(path_name.c_str(), RoutesConsts::method_get,
                     AmakerBotConsts::desc_name_get,
                     AmakerBotConsts::tag_service, false,
                     {}, name_get_responses));

    webserver.on(path_name.c_str(), HTTP_GET,
                 [this](AsyncWebServerRequest *request)
                 {
                     if (!checkServiceStarted(request))
                         return;
                     JsonDocument doc;
                     doc[FPSTR(AmakerBotConsts::field_name)] = getBotName().c_str();
                     String out;
                     serializeJson(doc, out);
                     request->send(200, FPSTR(RoutesConsts::mime_json), out);
                 });

    // ------------------------------------------------------------------
    // POST /api/amakerbot/v1/name?name=<name>
    // ------------------------------------------------------------------
    std::vector<OpenAPIParameter> name_set_params;
    name_set_params.push_back(
        OpenAPIParameter(AmakerBotConsts::param_name,
                         RoutesConsts::type_string,
                         RoutesConsts::in_query,
                         "New name for the bot (max 32 characters)",
                         true));

    std::vector<OpenAPIResponse> name_set_responses;
    name_set_responses.push_back(OpenAPIResponse(200, AmakerBotConsts::resp_name_set));
    name_set_responses.push_back(OpenAPIResponse(400, AmakerBotConsts::resp_missing_name));
    name_set_responses.push_back(createServiceNotStartedResponse());

    registerOpenAPIRoute(
        OpenAPIRoute(path_name.c_str(), RoutesConsts::method_post,
                     AmakerBotConsts::desc_name_set,
                     AmakerBotConsts::tag_service, false,
                     name_set_params, name_set_responses));

    webserver.on(path_name.c_str(), HTTP_POST,
                 [this](AsyncWebServerRequest *request)
                 {
                     if (!checkServiceStarted(request))
                         return;

                     const AsyncWebParameter *p = nullptr;
                     if (request->hasParam(FPSTR(AmakerBotConsts::param_name), true))
                         p = request->getParam(FPSTR(AmakerBotConsts::param_name), true);
                     else if (request->hasParam(FPSTR(AmakerBotConsts::param_name)))
                         p = request->getParam(FPSTR(AmakerBotConsts::param_name));

                     if (!p || p->value().isEmpty())
                     {
                         JsonDocument err;
                         err[FPSTR(RoutesConsts::result)] = FPSTR(RoutesConsts::result_err);
                         err[FPSTR(RoutesConsts::message)] = FPSTR(AmakerBotConsts::resp_missing_name);
                         String out;
                         serializeJson(err, out);
                         request->send(400, FPSTR(RoutesConsts::mime_json), out);
                         return;
                     }

                     setBotName(p->value().c_str());

                     JsonDocument doc;
                     doc[FPSTR(RoutesConsts::result)] = FPSTR(RoutesConsts::result_ok);
                     doc[FPSTR(AmakerBotConsts::field_name)] = getBotName().c_str();
                     String out;
                     serializeJson(doc, out);
                     request->send(200, FPSTR(RoutesConsts::mime_json), out);
                 });

    registerServiceStatusRoute(this);
    registerSettingsRoutes(this);
    return true;
}
