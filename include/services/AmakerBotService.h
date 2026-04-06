#pragma once
#include "IsServiceInterface.h"
#include "BotCommunication/BotMessageHandler.h"
#include "services/WiFiService.h"
#include <freertos/semphr.h>
#include <functional>
#include <vector>
#include <string>

/**
 * @file AmakerBotService.h
 * @brief Token-based master-registration and heartbeat-watchdog service.
 *
 * @details Generates a random 5-character alphanumeric token on startup.
 *          Clients must present this token to register as the master controller.
 *          All master-IP tracking is delegated to the injected `BotMaster`
 *          instance; `AmakerBotService` only owns the token, bot name, and
 *          heartbeat watchdog.
 *
 * ## Binary protocol — service_id: 0x04
 *
 * | Action | Cmd  | Payload                      | Reply                               |
 * |--------|------|------------------------------|-------------------------------------|
 * | 0x41   | 0x01 | [token bytes…]               | [0x41][resp_ok or resp_not_master]  |
 * | 0x42   | 0x02 | (none)                       | [0x42][resp_ok or resp_not_master]  |
 * | 0x43   | 0x03 | (none — heartbeat)           | (no reply)                          |
 * | 0x44   | 0x04 | [id:4B ping payload]         | [0x44][id:4B] echo                  |
 * | 0x45   | 0x05 | (none)                       | [0x45][resp_ok][name bytes…]        |
 * | 0x46   | 0x06 | [name bytes…] (master only)  | [0x46][resp_ok or resp_not_master]  |
 * | 0x47   | 0x07 | (none)                       | [0x47][resp_ok][ssid_len:1B][ssid…][pass_len:1B][pass…] |
 * | 0x48   | 0x08 | [ssid_len:1B][ssid…][pass_len:1B][pass…] (master) | [0x48][resp_ok or err] |
 * | 0x49   | 0x09 | (none, master only)          | [0x49][resp_ok or resp_not_master]  |
 * | 0x4A   | 0x0A | (none, master only)          | (no reply; emergency reboot)        |
 */
class AmakerBotService : public IsServiceInterface,
                         public IsBotActionHandlerInterface
{
public:
    /**
     * @brief Construct with the list of action handlers to dispatch to.
     * @param handlers Zero or more IsBotActionHandlerInterface* instances to register.
     *                 AmakerBotService always registers itself (service_id 0x04) first,
     *                 then each handler in order.  All pointed-to objects must outlive
     *                 this AmakerBotService.
     */
    explicit AmakerBotService(std::initializer_list<IsBotActionHandlerInterface *> handlers)
    {
        bot_message_handlers.assign(handlers.begin(), handlers.end());
    }
    // ---- IsServiceInterface ------------------------------------------

    /** @return "AmakerBot Service" */
    std::string getServiceName() override;

    /**
     * @brief Generate the server token and set the default bot name.
     * @return Always true.
     */
    bool initializeService() override;

    /**
     * @brief Mark service started.
     * @return false (START_FAILED) if BotMaster is not yet started.
     */
    bool startService() override;

    /**
     * @brief Release the bot-name mutex and mark service stopped.
     * @return Always true.
     */
    bool stopService() override;

    // ---- IsBotActionHandlerInterface ---------------------------------

    /** @return 0x04 */
    uint8_t getBotServiceId() const override;

    /**
     * @brief Dispatch a binary bot frame to the appropriate bot message handler (handleBotMessage)
     * @param data      Raw frame; byte[0] is the destination+action byte
     * @param len       Frame length in bytes
     * @param senderIP  IP address (or unique identifier) of the sender — used by
     *                  CMD_REGISTER to record the master.  Pass an empty string
     *                  when the sender IP is not available.
     */
    std::string dispatch(const uint8_t *data, size_t len,
                         const std::string &senderIP = {});

    /**
     * @brief Dispatch a binary bot frame to the appropriate command.
     * @param data      Raw frame; byte[0] is the destination+action byte
     * @param len       Frame length in bytes
     * @param senderIP  IP address (or unique identifier) of the sender.
     * @return Binary response string
     */
    std::string handleBotMessage(const uint8_t *data, size_t len,
                                 const std::string &senderIP);

    /** @brief Satisfies IsBotActionHandlerInterface (no sender IP available). */
    std::string handleBotMessage(const uint8_t *data, size_t len) override
    {
        return handleBotMessage(data, len, {});
    }

    // ---- Public accessors -------------------------------------------



    /** @return The 5-character alphanumeric token generated on init. */
    std::string getServerToken() const;

    /** @return Current bot name (default: "K10-Bot"). */
    std::string getBotName() const;

    /**
     * @brief Stop outputs via the emergency callback, then reboot the board.
     */
    void reboot();

    /**
     * @brief Register a new bot message handler.
     * @param handler Pointer to handler (must outlive this AmakerBotService).
     */
    void registerHandler(IsBotActionHandlerInterface *handler);

    /**
     * @brief Set the bot name (thread-safe, max 32 chars).
     * @param name New name; ignored if empty or longer than 32 characters.
     */
    void setBotName(const std::string &name);

    /**
     * @brief Register @p ip as master only if @p token matches the server token.
     * @return true if the token was valid and master was set.
     */
    bool setMasterIfTokenValid(const std::string &ip, const std::string &token);

    /**
     * @brief Inject the WifiService used by CMD_GET/SET/RESET_WIFI_SETTINGS.
     * @param wifi Pointer to the WifiService; must outlive this AmakerBotService.
     *             Pass nullptr to disable WiFi settings commands.
     */
    void setWifiService(WifiService *wifi) { wifi_service_ = wifi; }

    /**
     * @brief Watchdog: invoke the timeout callback if the master heartbeat has
     *        timed out (> 50 ms without a heartbeat from the registered master).
     *
     * @details Must be called periodically (e.g. every 10 ms from the transport
     *          task).  The callback fires only once per timeout event and resets
     *          automatically when the next valid heartbeat arrives.
     */
    void checkHeartbeatTimeout();

    /**
     * @brief Set the callback invoked when the heartbeat watchdog fires.
     * @param cb Callable `void()` — called exactly once per timeout event.
     *           Typical use: `[&]{ motor_servo.stopAllMotors(); }`
     */
    void setHeartbeatTimeoutCallback(std::function<void()> cb);


      /**
     * @brief Register @p requesterIP as master — only if no master is currently set.
     * @param requesterIP IP address of the registration requester.
     * @return true  if registration succeeded (was not set, now set).
     * @return false if a master is already registered.
     */
    bool registerMaster(const std::string &requesterIP);

    /**
     * @brief Unregister the master — only if @p requesterIP is the current master.
     * @param requesterIP IP address of the unregistration requester.
     * @return true  if the master was cleared.
     * @return false if @p requesterIP is not the current master.
     */
    bool unregister(const std::string &requesterIP);

    /**
     * @brief Check whether @p requesterIP is the registered master.
     * @param requesterIP IP address to test.
     * @return true if it matches the current master IP.
     */
    bool isMaster(const std::string &requesterIP) const;

    /**
     * @brief Return the registered master IP address.
     * @return Current master IP string, or empty string if no master is registered.
     */
    std::string getMasterIP() const;

    // ---- Keep-alive ---------------------------------------------------

    /**
     * @brief Refresh the "last seen" timestamp to millis().
     * @details Call this whenever a keepalive or message is received from the master.
     */
    void updateMasterLastSeen();

    /**
     * @brief Return the millis() value of the last updateMasterLastSeen() call.
     * @return 0 if updateMasterLastSeen() has never been called.
     */
    unsigned long getMasterLastSeen() const;

private:
    std::string       server_token_;        ///< Random token, set once in initializeService()
    std::string       bot_name_;            ///< Display name, protected by name_mutex_
    SemaphoreHandle_t name_mutex_ = nullptr; ///< Guards bot_name_ only
    
    std::string            master_ip_;       ///< Current master IP; empty = no master
    volatile unsigned long last_seen_ms_ = 0; ///< millis() of last updateMasterLastSeen()
    SemaphoreHandle_t      master_mutex_        = nullptr; ///< Protects master_ip_

    // ---- Heartbeat watchdog (AmakerBot-specific) ----------------------
    volatile bool heartbeat_active_    = false; ///< true once ≥1 heartbeat received from master
    volatile bool heartbeat_timed_out_ = false; ///< true while in timed-out state (avoids log spam)

    std::function<void()> heartbeat_timeout_cb_; ///< Called once on timeout

    WifiService *wifi_service_ = nullptr; ///< Injected via setWifiService(); may be nullptr

    std::vector<IsBotActionHandlerInterface *> bot_message_handlers;

    /** @brief Generate a random 5-character alphanumeric token via esp_random(). */
    std::string generateRandomToken();

public:
    /**
     * @brief Return the dispatch count for a given action byte.
     * @param action  The action byte (data[0]) to query.
     * @return Number of times dispatch() was called with that action byte.
     */
    uint32_t getDispatchCount(uint8_t action) const { return dispatch_count_[action]; }

    /**
     * @brief Return a pointer to the full 256-entry dispatch count table.
     * @return Const pointer to dispatch_count_[0].
     */
    const uint32_t *getDispatchCounts() const { return dispatch_count_; }

    /** @brief Reset all dispatch counters to zero. */
    void resetDispatchCounts() { memset(dispatch_count_, 0, sizeof(dispatch_count_)); }

    // ---- Script filesystem operations -----------------------------------

    /**
     * @brief List all scripts stored in the /scripts folder on LittleFS.
     * @return Vector of file names (without directory prefix).
     *         Returns an empty vector if the directory does not exist or
     *         LittleFS is not mounted.
     */
    std::vector<std::string> listScripts();

    /**
     * @brief Read a script from /scripts/<name> on LittleFS.
     * @param name Script file name (must not contain path separators or '..').
     * @return File content as a UTF-8 string, or empty string if the file
     *         is not found or the name fails validation.
     */
    std::string getScript(const std::string &name);

    /**
     * @brief Write (create or overwrite) /scripts/<name> on LittleFS.
     *        Creates the /scripts directory automatically if needed.
     * @param name    Script file name (must not contain path separators or '..').
     * @param content UTF-8 script content.
     * @return true on success, false on invalid name or write error.
     */
    bool saveScript(const std::string &name, const std::string &content);

    /**
     * @brief Delete /scripts/<name> from LittleFS.
     * @param name Script file name.
     * @return true on success, false if the file was not found or the name
     *         fails validation.
     */
    bool deleteScript(const std::string &name);

    /**
     * @brief Check whether /scripts/<name> exists on LittleFS.
     * @param name Script file name (validated before checking).
     * @return true if the file exists, false otherwise.
     */
    bool scriptExists(const std::string &name);

private:
    /**
     * @brief Validate that @p name is a safe script filename:
     *        non-empty, ≤64 chars, no path separators, no null bytes, no '..'.
     */
    static bool isValidScriptName(const std::string &name);

    uint32_t dispatch_count_[256] = {}; ///< Per-action-byte dispatch counter
};
