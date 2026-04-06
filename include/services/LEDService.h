/**
 * @file LEDService.h
 * @brief Unified RGB LED control for the 3 K10 on-board NeoPixels and the
 *        2 WS2812 LEDs on the DFR1216 expansion board.
 *
 * @details LED IDs are encoded in a single 5-bit **led_mask** byte:
 *
 *   Bit | LED
 *   ----|---------------------------
 *    0  | K10 NeoPixel 0
 *    1  | K10 NeoPixel 1
 *    2  | K10 NeoPixel 2
 *    3  | DFR1216 WS2812 LED 0
 *    4  | DFR1216 WS2812 LED 1
 *
 * Convenience masks: LEDConsts::MASK_ALL_K10 (0x07),
 *                    LEDConsts::MASK_ALL_DFR (0x18),
 *                    LEDConsts::MASK_ALL     (0x1F)
 *
 * ## Binary protocol — service_id: LEDConsts::BOT_SERVICE_ID (0x05)
 *
 * | Action | Cmd  | Payload                                        | Reply payload                              |
 * |--------|------|------------------------------------------------|--------------------------------------------|
 * | 0x51   | 0x01 | [led_mask][r:u8][g:u8][b:u8][brightness:u8]    | —                                          |
 * | 0x52   | 0x02 | [led_mask]                                     | —                                          |
 * | 0x53   | 0x03 | (none)                                         | —                                          |
 * | 0x54   | 0x04 | [led_mask]                                     | [led_mask][r₀][g₀][b₀][br₀]… per LED      |
 *
 * @note K10 NeoPixel brightness is a **per-write global** set via
 *       `unihiker.rgb->brightness()` immediately before each write.
 *       DFR1216 brightness is stored per-group (all DFR LEDs share one call).
 */
#pragma once

#include <pgmspace.h>
#include <cstdint>
#include <string>
#include <unihiker_k10.h>          // UNIHIKER_K10, RGB
#include "IsServiceInterface.h"
#include "BotCommunication/BotMessageHandler.h"
#include "services/DFR1216.h"

// ---------------------------------------------------------------------------
// PROGMEM string constants
// ---------------------------------------------------------------------------
namespace LEDConsts
{
    constexpr const char str_service_name[]        PROGMEM = "LED Service";
    constexpr const char msg_board_not_started[]   PROGMEM = "LEDService: DFR1216Board not started";
    constexpr const char msg_invalid_mask[]        PROGMEM = "LEDService: led_mask has no valid bits (0x1F)";
    constexpr const char msg_invalid_brightness[]  PROGMEM = "LEDService: brightness out of range (0-255)";
    constexpr const char msg_all_off[]             PROGMEM = "LEDService: all LEDs off";

    // ---------- LED counts & mask helpers ----------
    constexpr uint8_t  K10_LED_COUNT  = 3;   ///< NeoPixels on the K10 board (indices 0-2)
    constexpr uint8_t  DFR_LED_COUNT  = 2;   ///< WS2812 on the DFR1216 board (indices 0-1)
    constexpr uint8_t  TOTAL_LEDS     = 5;   ///< K10_LED_COUNT + DFR_LED_COUNT

    constexpr uint8_t  MASK_ALL_K10   = 0x07; ///< Bits 0-2
    constexpr uint8_t  MASK_ALL_DFR   = 0x18; ///< Bits 3-4
    constexpr uint8_t  MASK_ALL       = 0x1F; ///< All 5 LEDs

    // ---------- Bot protocol ----------
    /// Unique service identifier for the BotMessageHandler registry.
    constexpr uint8_t  BOT_SERVICE_ID         = 0x05;

    constexpr uint8_t  CMD_SET_COLOR          = 0x01; ///< Set R,G,B + brightness on selected LEDs
    constexpr uint8_t  CMD_TURN_OFF           = 0x02; ///< Turn off selected LEDs
    constexpr uint8_t  CMD_TURN_OFF_ALL       = 0x03; ///< Turn off all LEDs
    constexpr uint8_t  CMD_GET_COLOR          = 0x04; ///< Query cached RGBB for selected LEDs
} // namespace LEDConsts

// ---------------------------------------------------------------------------
// LedState (per-LED colour+brightness cache)
// ---------------------------------------------------------------------------
/// Per-LED cached state (reuses the struct from DFR1216.h is avoided to keep
/// LEDService self-contained; this struct adds brightness).
struct LEDState
{
    uint8_t r          = 0;
    uint8_t g          = 0;
    uint8_t b          = 0;
    uint8_t brightness = 0;
};

// ---------------------------------------------------------------------------
// LEDService
// ---------------------------------------------------------------------------
/**
 * @class LEDService
 * @brief Unified RGB LED control across K10 NeoPixels and DFR1216 WS2812 LEDs.
 *
 * Requires both a `UNIHIKER_K10` instance (for on-board NeoPixels) and a
 * `DFR1216Board` reference (for expansion-board WS2812 LEDs).
 *
 * Typical usage:
 * @code
 *   extern UNIHIKER_K10    unihiker;
 *   extern DFR1216Board    dfr_board;
 *
 *   LEDService led_service(unihiker, dfr_board);
 *   led_service.setDebugLogger(&debug_logger);
 *   led_service.initializeService();
 *   led_service.startService();
 *
 *   // Light K10 LED 0 red and DFR LED 0 blue, brightness 64
 *   const uint8_t mask = 0x01 | 0x08;        // bit0=K10[0], bit3=DFR[0]
 *   led_service.setColor(mask, 255, 0, 0, 64);
 * @endcode
 */
class LEDService : public IsServiceInterface,
                   public IsBotActionHandlerInterface
{
public:
    /**
     * @brief Construct with references to the K10 board and DFR1216 board.
     * @param k10   UNIHIKER_K10 instance (must have been initialised via begin()).
     * @param board DFR1216Board instance (must be started before LEDService::startService()).
     */
    LEDService(UNIHIKER_K10 &k10, DFR1216Board &board);

    // ---- IsServiceInterface ------------------------------------------

    /** @return "LED Service" */
    std::string getServiceName() override;

    /**
     * @brief Reset all LED state caches and turn all LEDs off.
     * @return Always true.
     */
    bool initializeService() override;

    /**
     * @brief Verify that the DFR1216Board is running, then mark this service started.
     * @return true on success; false (START_FAILED) if the board is not yet started.
     */
    bool startService() override;

    /**
     * @brief Turn all LEDs off, then mark service stopped.
     * @return Always true.
     */
    bool stopService() override;

    // ---- IsBotActionHandlerInterface ---------------------------------

    /** @return LEDConsts::BOT_SERVICE_ID (0x05) */
    uint8_t getBotServiceId() const override;

    /**
     * @brief Dispatch a binary bot frame to the appropriate LED command.
     * @param data Raw frame; byte[0] is the action byte
     * @param len  Frame length in bytes
     * @return Binary response string
     */
    std::string handleBotMessage(const uint8_t *data, size_t len) override;

    // ---- LED API -----------------------------------------------------

    /**
     * @brief Set the same RGBA colour on all LEDs selected by led_mask.
     *
     * For K10 NeoPixels, `brightness` is applied as a global pre-write level
     * via `rgb->brightness()`.  For DFR1216 WS2812 LEDs, `brightness` is
     * forwarded to the `setWS2812` call (0–255 mapped as-is).
     *
     * @param led_mask   Bitmask selecting target LEDs (valid bits 0–4)
     * @param r,g,b      Colour channels 0–255
     * @param brightness LED brightness 0–255
     * @return BotProto::resp_* status code
     */
    uint8_t setColor(uint8_t led_mask, uint8_t r, uint8_t g, uint8_t b,
                     uint8_t brightness);

    /**
     * @brief Turn off all LEDs selected by led_mask (sets RGBB to 0).
     * @param led_mask Bitmask selecting target LEDs (valid bits 0–4)
     * @return BotProto::resp_* status code
     */
    uint8_t turnOff(uint8_t led_mask);

    /**
     * @brief Turn off all 5 LEDs immediately.
     * @return BotProto::resp_ok
     */
    uint8_t turnOffAll();

    /**
     * @brief Read cached colour values for LEDs selected by led_mask.
     * @param led_mask Bitmask selecting target LEDs (valid bits 0–4)
     * @param out      Caller-provided buffer; must hold ≥ popcount(led_mask) LEDState values.
     *                 Values are stored in ascending bit order (bit 0 first).
     * @return BotProto::resp_* status code; on success, out[] is populated.
     */
    uint8_t getColor(uint8_t led_mask, LEDState *out) const;

private:
    UNIHIKER_K10  &k10_;    ///< K10 board (on-board NeoPixels)
    DFR1216Board  &board_;  ///< Extension board (DFR1216 WS2812 LEDs)

    /// Per-LED cached state — index 0–2 = K10, index 3–4 = DFR1216
    LEDState states_[LEDConsts::TOTAL_LEDS];

    // ---- Private helpers ---------------------------------------------

    /**
     * @brief Write the cached colour for one K10 NeoPixel to hardware.
     *        Sets `rgb->brightness()` to states_[idx].brightness first.
     * @param idx K10 LED index 0–2
     */
    void flushK10Led(uint8_t idx);

    /**
     * @brief Write the full DFR1216 WS2812 state (both LEDs) to hardware.
     *        Uses states_[3] and states_[4].  The brightness byte is the
     *        maximum of the two cached brightness values.
     */
    void flushDFRLeds();

    /**
     * @brief Expand a 5-bit led_mask into an array of LED indices.
     * @param mask   Source bitmask (bits 0–4)
     * @param out    Output array; must hold ≥ LEDConsts::TOTAL_LEDS elements
     * @return Number of entries written to out[]
     */
    static uint8_t unpackMask(uint8_t mask, uint8_t *out);
};
