/**
 * @file AmakerBotService.cpp
 * @brief Token-based master-registration and heartbeat-watchdog service.
 *
 * @details All master-IP state lives in BotMaster (injected at construction).
 *          AmakerBotService only owns: the one-time token, the bot name, and
 *          the heartbeat watchdog.
 */

#include "services/AmakerBotService.h"
#include "FlashStringHelper.h"
#include <Arduino.h>          // millis(), esp_random()

// ---------------------------------------------------------------------------
// PROGMEM constants (translation-unit local)
// ---------------------------------------------------------------------------

namespace AmakerBotConsts
{   
    constexpr const char str_service_name[]     PROGMEM = "AmakerBot Service";
    constexpr const char default_bot_name[]     PROGMEM = "K10-Bot";
    constexpr const char msg_registered[]          PROGMEM = "Master registered: ";
    constexpr const char msg_already_registered[]  PROGMEM = "Master already registered: ";
    constexpr const char msg_unregistered[]        PROGMEM = "Master unregistered: ";
    constexpr const char msg_unregister_denied[]   PROGMEM = "Unregister denied (not master): ";


    constexpr const char msg_token[]            PROGMEM = "AmakerBot: token=";
    constexpr const char msg_master_set[]       PROGMEM = "AmakerBot: master registered: ";
    constexpr const char msg_master_cleared[]   PROGMEM = "AmakerBot: master unregistered";
    constexpr const char msg_name_changed[]     PROGMEM = "AmakerBot: bot name=";
    constexpr const char msg_hb_timeout[]       PROGMEM = "AmakerBot: heartbeat timeout";
    constexpr const char msg_hb_restored[]      PROGMEM = "AmakerBot: heartbeat restored";
    constexpr const char msg_master_needed[]    PROGMEM = "AmakerBot: BotMaster must be started first";
    constexpr const char msg_mutex_failed[]     PROGMEM = "AmakerBot: name_mutex alloc failed";

    constexpr uint8_t  BOT_SERVICE_ID      = 0x04;
    constexpr uint8_t  CMD_REGISTER        = 0x01; ///< [token bytes…]
    constexpr uint8_t  CMD_UNREGISTER      = 0x02; ///< (no payload)
    constexpr uint8_t  CMD_HEARTBEAT       = 0x03; ///< (no payload, no reply)
    constexpr uint8_t  CMD_PING            = 0x04; ///< [id:4B] → echo
    constexpr uint8_t  CMD_GET_NAME        = 0x05; ///< → [resp_ok][name…]
    constexpr uint8_t  CMD_SET_NAME        = 0x06; ///< [name…] (master only)

    constexpr uint32_t HEARTBEAT_TIMEOUT_MS = 50;  ///< ms without heartbeat → emergency stop
} // namespace AmakerBotConsts

char svccode[6] = "proxy";
// ---------------------------------------------------------------------------
// IsServiceInterface
// ---------------------------------------------------------------------------

std::string AmakerBotService::getServiceName()
{
    return progmem_to_string(AmakerBotConsts::str_service_name);
}

bool AmakerBotService::initializeService()
{   
    if (isServiceInitialized())
        return true;


    name_mutex_ = xSemaphoreCreateMutex();
    if (!name_mutex_)
    {
        if (debugLogger)
            debugLogger->error(FPSTR(AmakerBotConsts::msg_mutex_failed));
        setServiceStatus(INITIALIZED_FAILED);
        return false;
    }
    master_mutex_ = xSemaphoreCreateMutex();
    if (!master_mutex_)
    {
        if (debugLogger)
            debugLogger->error(progmem_to_string(AmakerBotConsts::msg_mutex_failed).c_str());
        setServiceStatus(INITIALIZED_FAILED);
        return false;
    }
    server_token_  = generateRandomToken();
    bot_name_      = progmem_to_string(AmakerBotConsts::default_bot_name);

    heartbeat_active_    = false;
    heartbeat_timed_out_ = false;

    if (debugLogger)
        debugLogger->info(progmem_to_string(AmakerBotConsts::msg_token) + server_token_);
    master_ip_    = "";
    last_seen_ms_ = 0;

    setServiceStatus(INITIALIZED);

    if (serviceLogger) serviceLogger->info((getServiceName() + " " + FPSTR(ServiceConst::msg_init_ok)).c_str(),svccode);
    return true;
}

bool AmakerBotService::startService()
{
    if (!isServiceInitialized())
    {
        setServiceStatus(START_FAILED);
        return false;
    }

    setServiceStatus(STARTED);
    serviceLogger->info((getServiceName() + " " + FPSTR(ServiceConst::msg_start_ok)).c_str(),svccode);
    return true;
}

bool AmakerBotService::stopService()
{
    if (name_mutex_)
    {
        vSemaphoreDelete(name_mutex_);
        name_mutex_ = nullptr;
    }
     if (master_mutex_)
    {
        vSemaphoreDelete(master_mutex_);
        master_mutex_ = nullptr;
    }
    setServiceStatus(STOPPED);
    return true;
}

// ---------------------------------------------------------------------------
// IsBotActionHandlerInterface
// ---------------------------------------------------------------------------

uint8_t AmakerBotService::getBotServiceId() const
{
    return AmakerBotConsts::BOT_SERVICE_ID;
}

std::string AmakerBotService::handleBotMessage(const uint8_t *data, size_t len,
                                                const std::string &senderIP)
{
    if (!data || len < 1)
        return BotProto::make_ack(0x00, BotProto::resp_invalid_params);

    const uint8_t action = data[0];
    const uint8_t cmd    = BotProto::command(action);

    // ---- CMD_HEARTBEAT 0x03 : keep-alive, no reply --------------------
    if (cmd == AmakerBotConsts::CMD_HEARTBEAT)
    {
        updateMasterLastSeen();
        heartbeat_active_    = true;
        heartbeat_timed_out_ = false;
        return {};  // intentionally no reply to keep latency low
    }

    // ---- CMD_REGISTER 0x01 : [token bytes…] ---------------------------
    if (cmd == AmakerBotConsts::CMD_REGISTER)
    {
        if (len < 2)
            return BotProto::make_ack(action, BotProto::resp_invalid_params);

        const std::string token(reinterpret_cast<const char *>(data + 1), len - 1);

        if (token != server_token_)
            return BotProto::make_ack(action, BotProto::resp_not_master);

        // Idempotent: already the master → just confirm
        if (isMaster(senderIP))
            return BotProto::make_ack(action, BotProto::resp_ok);

        // Attempt to register; fails only if a *different* master is active
        const bool ok = registerMaster(senderIP);
        if (ok)
        {
            // Reset heartbeat watchdog for the new master session
            heartbeat_active_    = false;
            heartbeat_timed_out_ = false;
        }
        return BotProto::make_ack(action, ok ? BotProto::resp_ok
                                             : BotProto::resp_operation_failed);
    }

    // ---- CMD_UNREGISTER 0x02 : (no payload, sender must be master) ----
    if (cmd == AmakerBotConsts::CMD_UNREGISTER)
    {
        const bool ok = unregister(senderIP);
        if (ok)
            debugLogger->info(FPSTR(AmakerBotConsts::msg_master_cleared));
        return BotProto::make_ack(action, ok ? BotProto::resp_ok : BotProto::resp_not_master);
    }


    // ---- CMD_PING 0x04 : echo 4-byte ID payload -----------------------
    if (cmd == AmakerBotConsts::CMD_PING)
    {
        if (len < 5)
            return BotProto::make_ack(action, BotProto::resp_invalid_params);

        // Echo: [action][id:4B]
        std::string reply;
        reply.reserve(5);
        reply += static_cast<char>(action);
        reply.append(reinterpret_cast<const char *>(data + 1), 4);
        return reply;
    }

    // ---- CMD_GET_NAME 0x05 : reply with [action][resp_ok][name…] ------
    if (cmd == AmakerBotConsts::CMD_GET_NAME)
    {
        const std::string name = getBotName();
        std::string reply;
        reply.reserve(2 + name.size());
        reply += static_cast<char>(action);
        reply += static_cast<char>(BotProto::resp_ok);
        reply += name;
        return reply;
    }

    // ---- CMD_SET_NAME 0x06 : [name…] (master only) --------------------
    if (cmd == AmakerBotConsts::CMD_SET_NAME)
    {
        if (!isMaster(senderIP))
            return BotProto::make_ack(action, BotProto::resp_not_master);

        if (len < 2)
            return BotProto::make_ack(action, BotProto::resp_invalid_params);

        const std::string name(reinterpret_cast<const char *>(data + 1), len - 1);
        if (name.empty() || name.size() > 32)
            return BotProto::make_ack(action, BotProto::resp_invalid_values);

        setBotName(name);
        return BotProto::make_ack(action, BotProto::resp_ok);
    }

    return BotProto::make_ack(action, BotProto::resp_unknown_cmd);
}

// ---------------------------------------------------------------------------
// Public accessors — all master operations delegate to BotMaster
// ---------------------------------------------------------------------------

bool AmakerBotService::isMaster(const std::string &requesterIP) const
{
if (!master_mutex_)
        return false;

    xSemaphoreTake(master_mutex_, portMAX_DELAY);
    const bool result = (!master_ip_.empty() && master_ip_ == requesterIP);
    xSemaphoreGive(master_mutex_);
    return result;
}

std::string AmakerBotService::getServerToken() const
{
    return server_token_;
}

std::string AmakerBotService::getBotName() const
{
    if (!name_mutex_)
        return progmem_to_string(AmakerBotConsts::default_bot_name);

    xSemaphoreTake(name_mutex_, portMAX_DELAY);
    const std::string name = bot_name_;
    xSemaphoreGive(name_mutex_);
    return name;
}

void AmakerBotService::setBotName(const std::string &name)
{
    if (name.empty() || name.size() > 32 || !name_mutex_)
        return;

    xSemaphoreTake(name_mutex_, portMAX_DELAY);
    bot_name_ = name;
    xSemaphoreGive(name_mutex_);

    debugLogger->info(progmem_to_string(AmakerBotConsts::msg_name_changed) + name);
}

void AmakerBotService::registerHandler(IsBotActionHandlerInterface *handler)
{
    if (handler)
        bot_message_handlers.push_back(handler);
}

bool AmakerBotService::setMasterIfTokenValid(const std::string &ip, const std::string &token)
{
    if (token != server_token_)
        return false;

    const bool ok = registerMaster(ip);
    if (ok)
    {
        // Reset watchdog for the new master session
        heartbeat_active_    = false;
        heartbeat_timed_out_ = false;
        debugLogger->info(progmem_to_string(AmakerBotConsts::msg_master_set) + ip);
    }
    return ok;
}

// ---------------------------------------------------------------------------
// Heartbeat watchdog
// ---------------------------------------------------------------------------

void AmakerBotService::setHeartbeatTimeoutCallback(std::function<void()> cb)
{
    heartbeat_timeout_cb_ = std::move(cb);
}

void AmakerBotService::checkHeartbeatTimeout()
{
    // Only watch if a master is registered and at least one heartbeat arrived
    if (getMasterIP().empty() || !heartbeat_active_)
        return;

    const unsigned long elapsed = millis() - getMasterLastSeen();

    if (elapsed > AmakerBotConsts::HEARTBEAT_TIMEOUT_MS)
    {
        if (!heartbeat_timed_out_)
        {
            heartbeat_timed_out_ = true;
            debugLogger->error(FPSTR(AmakerBotConsts::msg_hb_timeout));
            if (heartbeat_timeout_cb_)
                heartbeat_timeout_cb_();
        }
    }
    else if (heartbeat_timed_out_)
    {
        // Heartbeat restored
        heartbeat_timed_out_ = false;
        debugLogger->info(FPSTR(AmakerBotConsts::msg_hb_restored));
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

std::string AmakerBotService::generateRandomToken()
{
    constexpr const char charset[] =
        "0";
    constexpr int charset_size = sizeof(charset) - 1;

    std::string token;
    token.reserve(5);
    for (int i = 0; i < 5; ++i)
        token += charset[esp_random() % charset_size];
    return token;
}



// ---------------------------------------------------------------------------
// AmakerBotService::dispatch
// ---------------------------------------------------------------------------

std::string AmakerBotService::dispatch(const uint8_t *data, size_t len,
                                        const std::string &senderIP)
{
    if (!data || len == 0)
    {
        if (debugLogger)
            debugLogger->debug(FPSTR(BotMessageHandlerConsts::msg_empty_message));
        return {};
    }
    
    ++dispatch_count_[data[0]]; // count every call by action byte
    if ((data[0]>>4) == getBotServiceId())
        return handleBotMessage(data, len, senderIP);
    else {


        
    }
    for (IsBotActionHandlerInterface *handler : bot_message_handlers) {
        if (handler && (data[0]>>4) == handler->getBotServiceId()) {
                    // Convert data to hex string format
        std::string hex_data;
        for (size_t i = 0; i < len; ++i) {
            char buf[3];
            snprintf(buf, sizeof(buf), "%02X", data[i]);
            // if (i > 0) hex_data += " ";
            hex_data += buf;
        }
            serviceLogger->info(("RX " + hex_data).c_str(), svccode);
            return handler->handleBotMessage(data, len);
        }
    }
    return BotProto::make_ack(data[0], BotProto::resp_unknown_service);
}


//------------------------------------------------------------------
// Master registry
// ---------------------------------------------------------------------------

bool AmakerBotService::registerMaster(const std::string &requesterIP)
{
    if (!master_mutex_)
        return false;

    xSemaphoreTake(master_mutex_, portMAX_DELAY);
    const bool already_set = !master_ip_.empty();
    if (!already_set)
        master_ip_ = requesterIP;
    xSemaphoreGive(master_mutex_);
    if (debugLogger)
    {
        if (already_set)
            debugLogger->info((progmem_to_string(AmakerBotConsts::msg_already_registered) + requesterIP).c_str());
        else
            debugLogger->info((progmem_to_string(AmakerBotConsts::msg_registered) + requesterIP).c_str());
    }
    return !already_set;
}

bool AmakerBotService::unregister(const std::string &requesterIP)
{
    if (!master_mutex_)
        return false;

    xSemaphoreTake(master_mutex_, portMAX_DELAY);
    const bool is_master = (master_ip_ == requesterIP);
    if (is_master)
        master_ip_ = "";
    xSemaphoreGive(master_mutex_);
    if (debugLogger)
    {
        if (is_master)
            debugLogger->info((progmem_to_string(AmakerBotConsts::msg_unregistered) + requesterIP).c_str());
        else
            debugLogger->info((progmem_to_string(AmakerBotConsts::msg_unregister_denied) + requesterIP).c_str());
    }
    return is_master;
}


std::string AmakerBotService::getMasterIP() const
{
    if (!master_mutex_)
        return "";

    xSemaphoreTake(master_mutex_, portMAX_DELAY);
    const std::string ip = master_ip_;
    xSemaphoreGive(master_mutex_);
    return ip;
}

void AmakerBotService::updateMasterLastSeen()
{
    last_seen_ms_ = millis();
}

unsigned long AmakerBotService::getMasterLastSeen() const
{
    return last_seen_ms_;
}
