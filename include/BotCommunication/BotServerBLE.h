/**
 * @file BotServerBLE.h
 * @brief BLE UART (Nordic UART Service) transport that feeds binary bot
 *        protocol frames into AmakerBotService::dispatch().
 *
 * @details
 * The ESP32-S3 does **not** support Bluetooth Classic / SPP; this transport
 * uses the de-facto "NUS" BLE UART profile instead:
 *
 *   - Service UUID : 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
 *   - RX char UUID : 6E400002-B5A3-F393-E0A9-E50E24DCCA9E  (write from client)
 *   - TX char UUID : 6E400003-B5A3-F393-E0A9-E50E24DCCA9E  (notify to client)
 *
 * The bot device name advertised over BLE defaults to "K10-Bot" and is
 * configurable before start().
 *
 * ## Lifecycle
 * @code
 *   BotServerBLE ble_server(bot_service);
 *   ble_server.setBotMessageLogger(&debug_logger);
 *   ble_server.setDeviceName("K10-Bot");   // optional
 *   ble_server.start();
 *   // ...
 *   ble_server.stop();
 * @endcode
 *
 * ## Thread safety
 * start() / stop() must be called from a single thread (xtask_bot_transport).
 * NimBLE callbacks execute on the NimBLE host task; they call
 * AmakerBotService::dispatch() which is documented as lock-free.
 */

#pragma once

#include <cstdint>
#include <string>
#include <pgmspace.h>
#include <NimBLEDevice.h>
#include "services/AmakerBotService.h"

class RollingLogger;

// ---------------------------------------------------------------------------
// PROGMEM string constants
// ---------------------------------------------------------------------------
namespace BotServerBLEConsts
{
    constexpr const char str_service_name[]   PROGMEM = "BotServerBLE";
    constexpr const char msg_start_ok[]       PROGMEM = "BotServerBLE advertising as ";
    constexpr const char msg_stop[]           PROGMEM = "BotServerBLE stopped";
    constexpr const char msg_no_alloc[]       PROGMEM = "BotServerBLE: NimBLE alloc failed";
    constexpr const char msg_client_conn[]    PROGMEM = "BLE client connected: ";
    constexpr const char msg_client_disc[]    PROGMEM = "BLE client disconnected: ";
    constexpr const char default_device_name[] PROGMEM = "K10-Bot";

    // Nordic UART Service UUIDs
    constexpr const char nus_service_uuid[]   = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
    constexpr const char nus_rx_char_uuid[]   = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
    constexpr const char nus_tx_char_uuid[]   = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";
} // namespace BotServerBLEConsts

// ---------------------------------------------------------------------------
// BotServerBLE
// ---------------------------------------------------------------------------

/**
 * @brief BLE NUS transport layer for the binary bot protocol.
 *
 * Wraps NimBLE-Arduino: creates a GATT server with the Nordic UART Service,
 * forwards each write to AmakerBotService::dispatch(), and notifies the
 * reply back to the connected client via the TX characteristic.
 */
class BotServerBLE : private NimBLEServerCallbacks,
                     private NimBLECharacteristicCallbacks
{
public:
    // ---- Construction ----

    /**
     * @brief Construct a BotServerBLE.
     * @param bot Reference to the AmakerBotService — must outlive this object.
     */
    explicit BotServerBLE(AmakerBotService &bot);

    // Not copyable — owns NimBLE global state
    BotServerBLE(const BotServerBLE &)            = delete;
    BotServerBLE &operator=(const BotServerBLE &) = delete;

    ~BotServerBLE() { stop(); }

    // ---- Configuration (call before start()) ----

    /**
     * @brief Attach a logger for info / error output.
     * @param log May be nullptr to disable logging.
     */
    void setBotMessageLogger(RollingLogger *log) { logger_ = log; }

    /**
     * @brief Set the BLE advertised device name.
     * @note Has no effect after start() has been called.
     * @param name Device name (e.g. "K10-Bot")
     */
    void setDeviceName(const std::string &name) { device_name_ = name; }

    /** @brief Return the configured device name. */
    const std::string &getDeviceName() const { return device_name_; }

    // ---- Lifecycle ----

    /**
     * @brief Initialise NimBLE, create the NUS GATT service, and start advertising.
     * @return true  on success
     * @return false if NimBLE initialisation failed
     */
    bool start();

    /**
     * @brief Stop advertising, disconnect all clients, and deinit NimBLE.
     * Safe to call even if start() was never called.
     */
    void stop();

    /** @brief Return true if advertising / accepting connections. */
    bool isRunning() const { return running_; }

    // ---- Transport ----

    /**
     * @brief Notify a binary reply to the currently connected client.
     *
     * Called automatically by the RX callback after dispatch() returns a
     * non-empty response.  Also callable directly for unsolicited pushes.
     *
     * @param data Binary payload
     * @return true if the notification was sent successfully
     */
    bool sendReply(const std::string &data);

    // ---- Diagnostics ----

    /** @brief Number of write frames received since start(). */
    uint32_t getRxCount()      const { return rx_count_; }
    /** @brief Number of notify frames sent since start(). */
    uint32_t getTxCount()      const { return tx_count_; }
    /** @brief Number of frames dropped (zero-length or no client). */
    uint32_t getDroppedCount() const { return dropped_count_; }

private:
    // ---- NimBLE callbacks ----

    /** @brief Called by NimBLE when a central connects. */
    void onConnect(NimBLEServer *server, ble_gap_conn_desc *desc) override;

    /** @brief Called by NimBLE when a central disconnects. */
    void onDisconnect(NimBLEServer *server, ble_gap_conn_desc *desc) override;

    /** @brief Called by NimBLE when the RX characteristic is written. */
    void onWrite(NimBLECharacteristic *characteristic, ble_gap_conn_desc *desc) override;

    // ---- Members ----

    AmakerBotService       &bot_;
    RollingLogger          *logger_       = nullptr;
    std::string             device_name_  = "K10-Bot";

    NimBLEServer           *ble_server_   = nullptr;
    NimBLECharacteristic   *tx_char_      = nullptr;   ///< TX notify characteristic
    bool                    running_      = false;

    volatile uint32_t       rx_count_     = 0;
    volatile uint32_t       tx_count_     = 0;
    volatile uint32_t       dropped_count_= 0;
};
