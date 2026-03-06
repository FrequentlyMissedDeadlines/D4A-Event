# K10-Bot UDP API — AI Reference Guide

> **Target audience**: AI agents and code-generation tools.  
> **Purpose**: Authoritative, machine-friendly reference for all UDP messages accepted by the K10 Bot firmware.  
> **Source of truth**: Generated from firmware source as of 2026-03-02.  
> Binary values are exact — do not interpret them as text.

---

## Transport Layer

| Property | Value |
|---|---|
| Protocol | UDP unicast (IPv4) |
| Default port | **24642** |
| Max payload | **256 bytes** |
| Direction | Client → Device (request); Device → Client (response, sent to sender IP:port) |

---

## Wire Formats

The firmware supports **two protocols** simultaneously, dispatched in the routing order below.

### A. Fast: Binary Protocol

```
REQUEST  : [action:1B][...payload bytes]
RESPONSE : [action:1B][resp_code:1B][...optional payload bytes]
```

#### Action byte encoding

```
 7   6   5   4   3   2   1   0
┌───────────────┬───────────────┐
│  service_id   │  base_action  │
│   (4 bits)    │   (4 bits)    │
└───────────────┴───────────────┘
```

`action = (service_id << 4) | base_action`

| service_id | Service | Action range |
|---|---|---|
| `0x1` | BoardInfoService | `0x11`–`0x14` |
| `0x2` | ServoService | `0x21`–`0x29` |
| `0x3` | DFR1216Service | `0x31`–`0x34` |

> ⚠️ **Known bug — K10SensorsService**: The `K10SensorsService` has `service_id = 0x02` hardcoded in the current firmware (should be `0x01`), making its `udp_action_get_sensors = 0x21`. Since `ServoService` is registered first and also claims `0x21`, **K10SensorsService's UDP handler is permanently shadowed and unreachable**. Do not generate code that sends `0x21` expecting sensor data. The GET_SENSORS command is not usable via UDP in the current firmware.

#### Binary response codes (byte 1 of every binary response)

| Code | Constant | Meaning |
|---|---|---|
| `0x00` | `udp_resp_ok` | Success |
| `0x01` | `udp_resp_invalid_params` | Required byte/field missing or packet too short |
| `0x02` | `udp_resp_invalid_values` | Field present but value out of range |
| `0x03` | `udp_resp_operation_failed` | Hardware-level failure |
| `0x04` | `udp_resp_not_started` | Service not yet started |
| `0x05` | `udp_resp_unknown_cmd` | Action code not recognised |
| `0x06` | `udp_resp_not_master` | Sender IP is not the registered master (requires prior registration via AmakerBotService) |

---

### B. Text Protocol (MusicService)

```
<ServiceName>:<command>[:<payload>]
```

- `<ServiceName>` — **exact, case-sensitive** service name string.
- `<command>` — action token (ASCII, no spaces).
- `:<payload>` — optional payload. **Omit the colon entirely** if no payload is needed.
  - For most commands: compact JSON.
  - For `playnotes`: hex-encoded binary string (see section 3).

#### Text response shapes

**Standard success**
```json
{"result":"ok","message":"<command>"}
```

**Standard error**
```json
{"result":"error","message":"<reason>"}
```

Common `<reason>` strings:

| Reason string | Trigger |
|---|---|
| `"Invalid parameters"` | Required JSON field missing or wrong type |
| `"Invalid values"` | Field present but value outside allowed range |
| `"Music action failed"` | Hardware-level failure |
| `"UDP unknown command"` | Command token not recognised |

---

### Routing order (registered at boot in `main.cpp`)

| Priority | Service | Protocol | Claim condition |
|---|---|---|---|
| 1 | ServoService | Binary | `action` byte `0x21`–`0x29` |
| 2 | K10SensorsService | Binary | `action` byte `0x21` — **⚠️ shadowed by ServoService, unreachable** |
| 3 | BoardInfoService | Binary | `action` byte `0x11`–`0x14` |
| 4 | MusicService | Text | message starts with `"Music:"` |
| 5 | DFR1216Service | Binary | `action` byte `0x31`–`0x34` |
| 6 | AmakerBotService | Binary + Text | binary `action` byte `0x41`–`0x43`; text `"AMAKERBOT:"` is coincidentally routed via byte `0x41` (`'A'`) |

---

## 1. ServoService — Binary Protocol

**service_id**: `0x2`  
**Hardware**: DFR1216 I²C servo/motor controller  
**Servo channels**: 0–7 · **DC motors**: 0–3 (internal index; board labels them 1–4)

### Channel / motor masks

- **Servo mask** (1 byte): bit N selects servo channel N (N = 0–7). `0xFF` = all channels.
- **Motor mask** (1 byte): bit N selects motor N (internal index 0–3 = board motor N+1). Bits 4–7 ignored. `0x0F` = all motors.

### Connection type enum (used in ATTACH_SERVO)

| Value | Label | Meaning |
|---|---|---|
| `0` | `"Not Connected"` | No servo attached |
| `1` | `"Rotational"` | Continuous-rotation servo |
| `2` | `"Angular 180"` | 180° positional servo |
| `3` | `"Angular 270"` | 270° positional servo |

### Angle encoding (centre-zero int16 LE, used in SET_SERVO_ANGLE)

Angles are sent as a **signed 16-bit integer in little-endian** with a skip flag in bit 0:

- `bit 0 = 0` → skip this channel (do nothing)
- `bit 0 = 1` → angle = `raw >> 1` (arithmetic right-shift, centre-zero degrees)

| Servo type | Valid centre-zero range | Hardware range |
|---|---|---|
| Angular 180 | −90 … +90 | 0° … 180° |
| Angular 270 | −135 … +135 | 0° … 270° |

### Speed encoding (used in SET_SERVO_SPEED and SET_MOTOR_SPEED)

Each speed is encoded as a single unsigned byte: `encoded_byte = speed + 128`

| Speed | Encoded byte (decimal) | Hex |
|---|---|---|
| −100 | 28 | `0x1C` |
| 0 | 128 | `0x80` |
| +100 | 228 | `0xE4` |

Valid decoded range: −100 … +100.

---

### `0x21` SET_SERVO_ANGLE

Set the angle of individual angular servos. One signed int16 LE per channel position.

```
REQUEST  : [0x21][ch0:int16_LE]...[chN:int16_LE]   1 + (N+1)×2 bytes  (N = 0..7)
RESPONSE : [0x21][resp_code:1B]
```

| Bytes | Field | Type | Notes |
|---|---|---|---|
| 0 | action | uint8 | `0x21` |
| 1–2 | ch0 | int16 LE | channel 0 encoded value |
| 3–4 | ch1 | int16 LE | channel 1 encoded value |
| … | … | … | up to 8 channels total |

**Payload rule**: Include one int16 per channel **from 0 to the highest channel you want to address**. Channels you want to skip must still occupy their slot in the payload with value `0x0000` (bit0=0 = skip). Missing trailing channels are left unchanged.

**Encoding formula**: `raw = (angle_degrees << 1) | 1` to set, `raw = 0` to skip.

**Examples**:
- Set channel 0 to +45°: `raw = (45 << 1) | 1 = 91` → bytes `0x5B 0x00`
- Set channel 1 to −90°: `raw = (-90 << 1) | 1 = -179` → bytes `0x4D 0xFF`
- Skip channel 2: `raw = 0` → bytes `0x00 0x00`

Response `resp_code`: `ok` · `operation_failed` (channel type mismatch or angle out of range for its servo type)

---

### `0x22` SET_SERVO_SPEED

Set the speed of one or more continuous-rotation servos. An optional 2-byte trailing field schedules an automatic stop after a given number of milliseconds.

```
REQUEST  : [0x22][mask:1B][sp_ch0:1B]...[sp_ch7:1B]              2–10 bytes (no timeout)
           [0x22][mask:1B][sp_ch0:1B]...[sp_ch7:1B][dur:uint16_LE]  12 bytes  (with timeout)
RESPONSE : [0x22][resp_code:1B]
```

| Byte | Field | Type | Notes |
|---|---|---|---|
| 0 | action | uint8 | `0x22` |
| 1 | mask | uint8 | servo bitmask; bit N selects channel N |
| 2 | sp_ch0 | uint8 | encoded speed for channel 0 |
| 3 | sp_ch1 | uint8 | encoded speed for channel 1 |
| … | … | … | one byte per channel position (sequential, channels 0–7) |
| 10 | duration_ms_lo | uint8 | *(optional)* low byte of auto-stop delay (ms) |
| 11 | duration_ms_hi | uint8 | *(optional)* high byte of auto-stop delay (ms) |

**Critical payload rule**: Speed bytes are consumed **one per channel position**, sequentially from channel 0, regardless of the mask. To address channel N, you must provide N+1 speed bytes — intermediate bytes for unmasked channels are consumed but ignored. The safest and recommended pattern is to **always send all 8 speed bytes**, using `0x80` (speed=0) for don't-care channels:

```
[0x22][mask][sp_ch0][sp_ch1][sp_ch2][sp_ch3][sp_ch4][sp_ch5][sp_ch6][sp_ch7]
```

**Speed encoding**: `encoded = speed + 128`

**Optional auto-stop (`duration_ms`)**: When bytes 10–11 are present and non-zero (uint16 little-endian, 1–65535 ms), the firmware schedules a FreeRTOS one-shot timer for each masked `ROTATIONAL` channel that is *running*. When the timer fires the servo is stopped. The timer is reset if a new speed command arrives before it fires. Sending `duration_ms = 0` (or omitting the field) cancels any pending stop.

```python
import struct
# Drive channels 0 & 1 forward at 60% for 2 seconds, then auto-stop
mask = 0b00000011
speeds = [60 + 128, 60 + 128] + [128] * 6   # 128 = speed 0 for unused channels
duration_ms = 2000
pkt = bytes([0x22, mask]) + bytes(speeds) + struct.pack('<H', duration_ms)
sock.sendto(pkt, (ROBOT_IP, UDP_PORT))
```

Response `resp_code`: `ok` · `invalid_params` (< 3 bytes) · `invalid_values` (decoded speed outside −100…+100) · `operation_failed` (channel not `ROTATIONAL`)

---

### `0x23` STOP_SERVOS

Stop continuous-rotation servos selected by mask (sets speed to 0).

```
REQUEST  : [0x23][mask:1B]   2 bytes
RESPONSE : [0x23][resp_code:1B]
```

| Byte | Field | Type | Notes |
|---|---|---|---|
| 0 | action | uint8 | `0x23` |
| 1 | mask | uint8 | servo bitmask; `0xFF` = all |

Only `ROTATIONAL` channels in the mask are stopped; other types are silently skipped.

Response `resp_code`: `ok` · `invalid_params` (< 2 bytes)

---

### `0x24` ATTACH_SERVO

Set the servo type on one or more channels. Must be called **before** any angle or speed command.

```
REQUEST  : [0x24][mask:1B][type:1B]   3 bytes
RESPONSE : [0x24][resp_code:1B]
```

| Byte | Field | Type | Notes |
|---|---|---|---|
| 0 | action | uint8 | `0x24` |
| 1 | mask | uint8 | servo bitmask |
| 2 | type | uint8 | connection enum 0–3 |

All channels whose bit is set in `mask` are set to the same `type`.

Response `resp_code`: `ok` · `invalid_params` (< 3 bytes) · `invalid_values` (type > 3)

---

### `0x25` SET_MOTOR_SPEED

Set the speed of DC motors sequentially, starting from motor 0 (board motor 1). No mask — bytes are applied to motors 0, 1, 2, 3 in order.

```
REQUEST  : [0x25][sp_m0:1B]...[sp_mN:1B]   2–5 bytes  (N = 0..3)
RESPONSE : [0x25][resp_code:1B]
```

| Bytes | Field | Type | Notes |
|---|---|---|---|
| 0 | action | uint8 | `0x25` |
| 1 | sp_m0 | uint8 | encoded speed for motor 0 (board motor 1) |
| 2 | sp_m1 | uint8 | encoded speed for motor 1 (board motor 2) — optional |
| … | … | … | up to 4 motors total |

Sending fewer than 4 speed bytes leaves the remaining motors unchanged.

**Speed encoding**: `encoded = speed + 128`

**Examples**:
- Motor 0 at +75: `encoded = 75 + 128 = 203` → `0xCB`
- Motor 1 at −50: `encoded = -50 + 128 = 78` → `0x4E`

Response `resp_code`: `ok` · `invalid_params` (< 2 bytes) · `invalid_values` (decoded speed outside −100…+100)

---

### `0x26` STOP_MOTORS

Stop DC motors selected by motor mask.

```
REQUEST  : [0x26][mask:1B]   2 bytes
RESPONSE : [0x26][resp_code:1B]
```

| Byte | Field | Type | Notes |
|---|---|---|---|
| 0 | action | uint8 | `0x26` |
| 1 | mask | uint8 | motor bitmask (bit 0 = motor 0 / board motor 1); `0x0F` = all |

Response `resp_code`: `ok` · `invalid_params` (< 2 bytes)

---

### `0x27` GET_SERVO_STATUS

Get connection type of servo channels selected by mask, returned as JSON.

```
REQUEST  : [0x27][mask:1B]   2 bytes
RESPONSE : [0x27][0x00][JSON payload]
```

Response JSON shape (only channels whose bit is set in mask appear):
```json
{"attached_servos":[{"channel":0,"connection":"Not Connected"},{"channel":2,"connection":"Angular 180"}]}
```

Response `resp_code`: `ok` · `invalid_params` (< 2 bytes)

---

### `0x28` GET_ALL_STATUS

Get connection type of all 8 servo channels as JSON.

```
REQUEST  : [0x28]   1 byte
RESPONSE : [0x28][0x00][JSON payload]
```

Response JSON shape (always 8 entries, channels 0–7 in order):
```json
{"attached_servos":[{"channel":0,"connection":"Not Connected"},{"channel":1,"connection":"Rotational"},{"channel":2,"connection":"Angular 180"},{"channel":3,"connection":"Angular 270"},{"channel":4,"connection":"Not Connected"},{"channel":5,"connection":"Not Connected"},{"channel":6,"connection":"Not Connected"},{"channel":7,"connection":"Not Connected"}]}
```

---

### `0x29` GET_BATTERY

Read the board battery level.

```
REQUEST  : [0x29]   1 byte
RESPONSE : [0x29][0x00][batt:1B]
```

`batt` is a uint8 in the range 0–100 (%).

---

## 2. BoardInfoService — Binary Protocol

**service_id**: `0x1`  
**Hardware**: 3 onboard RGB LEDs on the UNIHIKER K10 board (indices 0–2).

---

### `0x11` SET_LED_COLOR

Set the RGB color of one onboard LED.

```
REQUEST  : [0x11][led:1B][r:1B][g:1B][b:1B]   5 bytes
RESPONSE : [0x11][resp_code:1B]
```

| Byte | Field | Type | Notes |
|---|---|---|---|
| 0 | action | uint8 | `0x11` |
| 1 | led | uint8 | LED index 0–2 |
| 2 | r | uint8 | Red channel 0–255 |
| 3 | g | uint8 | Green channel 0–255 |
| 4 | b | uint8 | Blue channel 0–255 |

Response `resp_code`: `ok` · `invalid_params` (< 5 bytes) · `invalid_values` (led > 2) · `operation_failed`

---

### `0x12` TURN_OFF_LED

Turn off (set to black) one onboard LED.

```
REQUEST  : [0x12][led:1B]   2 bytes
RESPONSE : [0x12][resp_code:1B]
```

| Byte | Field | Type | Notes |
|---|---|---|---|
| 0 | action | uint8 | `0x12` |
| 1 | led | uint8 | LED index 0–2 |

Response `resp_code`: `ok` · `invalid_params` (< 2 bytes) · `invalid_values` (led > 2) · `operation_failed`

---

### `0x13` TURN_OFF_ALL_LEDS

Turn off all 3 onboard LEDs.

```
REQUEST  : [0x13]   1 byte
RESPONSE : [0x13][resp_code:1B]
```

Response `resp_code`: `ok` · `operation_failed`

---

### `0x14` GET_LED_STATUS

Get the current RGB state of all 3 onboard LEDs as JSON.

```
REQUEST  : [0x14]   1 byte
RESPONSE : [0x14][0x00][JSON payload]
```

Response JSON shape (always 3 entries):
```json
{"leds":[{"led":0,"red":255,"green":0,"blue":0},{"led":1,"red":0,"green":255,"blue":0},{"led":2,"red":0,"green":0,"blue":255}]}
```

Field names: `led` (index), `red`, `green`, `blue`.

---

## 3. MusicService — Text Protocol

**Prefix**: `Music`  
**Hardware**: Onboard buzzer via `UNIHIKER_K10` Music API.

### Melody index table

| Index | Name | Index | Name |
|---|---|---|---|
| 0 | `DADADADUM` | 10 | `FUNERAL` |
| 1 | `ENTERTAINER` | 11 | `PUNCHLINE` |
| 2 | `PRELUDE` | 12 | `BADDY` |
| 3 | `ODE` | 13 | `CHASE` |
| 4 | `NYAN` | 14 | `BA_DING` |
| 5 | `RINGTONE` | 15 | `WAWAWAWAA` |
| 6 | `FUNK` | 16 | `JUMP_UP` |
| 7 | `BLUES` | 17 | `JUMP_DOWN` |
| 8 | `BIRTHDAY` | 18 | `POWER_UP` |
| 9 | `WEDDING` | 19 | `POWER_DOWN` |

### Playback option enum

| Integer | Name | Behaviour |
|---|---|---|
| `1` | `Once` | Play once, blocking |
| `2` | `Forever` | Loop, blocking |
| `4` | `OnceInBackground` | Play once (non-blocking) — **default** |
| `8` | `ForeverInBackground` | Loop (non-blocking) |

---

### `play`

Play a built-in melody.

```
Music:play:{"melody":<int>,"option":<int>}
```

| Field | Type | Range | Required | Default |
|---|---|---|---|---|
| `melody` | int | 0–19 | ✅ | — |
| `option` | int | 1, 2, 4, 8 | ❌ | `4` (OnceInBackground) |

**Response**: standard text success/error.

**Examples**:
```
Music:play:{"melody":8}
Music:play:{"melody":0,"option":4}
Music:play:{"melody":4,"option":8}
```

---

### `tone`

Play a single tone at a given frequency.

```
Music:tone:{"freq":<int>,"beat":<int>}
```

| Field | Type | Constraint | Required | Default |
|---|---|---|---|---|
| `freq` | int | > 0 (Hz) | ✅ | — |
| `beat` | int | > 0 (sample units; 8000 units ≈ 1 s at 8 kHz sample rate) | ❌ | `8000` |

**Response**: standard text success/error.

**Examples**:
```
Music:tone:{"freq":440}
Music:tone:{"freq":880,"beat":4000}
```

---

### `stop`

Stop current playback. No payload.

```
Music:stop
```

**Response**: standard text success/error.

---

### `melodies`

Get the list of all available built-in melody names. No payload.

```
Music:melodies
```

**Response** (raw JSON array — no wrapper object):
```json
["DADADADUM","ENTERTAINER","PRELUDE","ODE","NYAN","RINGTONE","FUNK","BLUES","BIRTHDAY","WEDDING","FUNERAL","PUNCHLINE","BADDY","CHASE","BA_DING","WAWAWAWAA","JUMP_UP","JUMP_DOWN","POWER_UP","POWER_DOWN"]
```

---

### `playnotes`

Play a custom sequence of MIDI notes with per-note duration control. The payload is a **hex-encoded binary** string (uppercase or lowercase hex accepted).

```
Music:playnotes:<hex_string>
```

#### Binary layout (each byte encoded as 2 hex characters)

| Byte index | Field | Description |
|---|---|---|
| 0 | `tempo` | Tempo in BPM: 1–240. `0x00` → 120 BPM (default). Values > 240 → clamped to 240. |
| 1 | `note_byte` (note 0) | `bit7=1` → silence; `bits6-0` = MIDI note number (0–127) |
| 2 | `duration_byte` (note 0) | Duration in 16th notes (1–255). `0x00` treated as 1. |
| 3 | `note_byte` (note 1) | Next note |
| 4 | `duration_byte` (note 1) | Next duration |
| … | … | … |

#### Validation rules

All of the following must hold, or the device returns `{"result":"error","message":"UDP invalid notes payload"}`:

- Total hex string length ≥ **6** characters (= 3 binary bytes minimum: 1 tempo + 1 note pair).
- Hex string length must be **even** (complete hex bytes).
- `(total_bytes − 1)` must be **even**: one tempo byte followed by an even number of note bytes (i.e. complete note/duration pairs).

#### MIDI reference

Standard 12-tone equal temperament, A4 = MIDI note 69 = 440 Hz.  
Silence: set `bit7` of the note byte (`note_byte | 0x80`); MIDI note bits are irrelevant for silences.

#### Duration reference

| `duration_byte` | Musical value at given BPM |
|---|---|
| 1 | 1 sixteenth note |
| 2 | 1 eighth note |
| 4 | 1 quarter note |
| 8 | 1 half note |
| 16 | 1 whole note |

One sixteenth note duration in ms: `15000 / BPM`

#### Encoding example

Play C4 (MIDI 60) as a quarter note, then a silence of one eighth note, at 120 BPM:

```
tempo      = 0x78  (120 BPM)     →  "78"
note C4    = 0x3C  (MIDI 60)     →  "3C"   duration 4 sixteenths → "04"
silence    = 0x80                →  "80"   duration 2 sixteenths → "02"

hex_string = "783C048002"
```

```
Music:playnotes:783C048002
```

**Response**: `{"result":"ok","message":"playnotes"}` on success.

---

## 4. DFR1216Service — Binary Protocol

**service_id**: `0x3`  
**Hardware**: DFR1216 expansion board, 3 RGB LEDs (indices 0–2) with per-LED brightness control.

> ℹ️ **Distinct from BoardInfoService LEDs** (section 2). These are the LEDs on the DFR1216 I²C expansion board, not the onboard K10 LEDs. Note also that the GET_LED_STATUS response uses the key `"id"` (not `"led"` as in BoardInfoService).

---

### `0x31` SET_LED_COLOR

Set the RGB color and brightness of one DFR1216 LED.

```
REQUEST  : [0x31][led:1B][r:1B][g:1B][b:1B][brightness:1B]   6 bytes
RESPONSE : [0x31][resp_code:1B]
```

| Byte | Field | Type | Notes |
|---|---|---|---|
| 0 | action | uint8 | `0x31` |
| 1 | led | uint8 | LED index 0–2 |
| 2 | r | uint8 | Red channel 0–255 |
| 3 | g | uint8 | Green channel 0–255 |
| 4 | b | uint8 | Blue channel 0–255 |
| 5 | brightness | uint8 | Global brightness 0–255 |

Response `resp_code`: `ok` · `invalid_params` (< 6 bytes) · `invalid_values` (led > 2) · `operation_failed`

---

### `0x32` TURN_OFF_LED

Turn off one DFR1216 LED.

```
REQUEST  : [0x32][led:1B]   2 bytes
RESPONSE : [0x32][resp_code:1B]
```

| Byte | Field | Type | Notes |
|---|---|---|---|
| 0 | action | uint8 | `0x32` |
| 1 | led | uint8 | LED index 0–2 |

Response `resp_code`: `ok` · `invalid_params` (< 2 bytes) · `invalid_values` (led > 2) · `operation_failed`

---

### `0x33` TURN_OFF_ALL_LEDS

Turn off all 3 DFR1216 LEDs.

```
REQUEST  : [0x33]   1 byte
RESPONSE : [0x33][resp_code:1B]
```

Response `resp_code`: `ok` · `operation_failed`

---

### `0x34` GET_LED_STATUS

Get the current RGB state of all 3 DFR1216 LEDs as JSON.

```
REQUEST  : [0x34]   1 byte
RESPONSE : [0x34][0x00][JSON payload]
```

Response JSON shape (always 3 entries):
```json
{"leds":[{"id":0,"red":255,"green":0,"blue":0},{"id":1,"red":0,"green":255,"blue":0},{"id":2,"red":0,"green":0,"blue":255}]}
```

Field names: `id` (index), `red`, `green`, `blue`. *(Key is `"id"`, not `"led"` — different from BoardInfoService's `0x14`.)*

---

---

## 5. AmakerBotService — Binary + Text Protocol

**service_id**: `0x4`  
**Purpose**: Register/unregister an external client IP as the "master" controller and maintain the heartbeat watchdog.  
**Authentication**: Token-based. The 5-character hex token is generated at boot and displayed on the TFT screen in `MODE_APP_LOG`.  
**Master enforcement**: ServoService and DFR1216Service check the registered master IP; commands from non-master IPs return `udp_resp_not_master` (`0x06`).


---

### `0x41` MASTER_REGISTER

Register the **sender's IP** as the master controller.

```
REQUEST  : [0x41][token_bytes…]   1 + N bytes  (N = token length, typically 5)
RESPONSE : none
```

| Bytes | Field | Type | Notes |
|---|---|---|---|
| 0 | action | uint8 | `0x41` |
| 1… | token | ASCII bytes | 5-character hex token shown on device screen at boot |

**Behaviour**:
- If `token` matches the server-generated token, the sender IP is registered as master and the heartbeat watchdog is reset.
- If `token` is empty or does not match, the message is silently ignored.
- Replaces any previously registered master.

**No reply** is sent. Registration success is visible on the device screen (`[MASTER] registered from <ip>`).


### `0x42` MASTER_UNREGISTER

Clear the current master registration. Only the currently registered master IP can call this.

```
REQUEST  : [0x42]   1 byte
RESPONSE : none
```

**Behaviour**:
- If the sender IP matches the current master, the registration is cleared and the heartbeat watchdog is reset.
- If the sender is not the current master, the message is silently ignored.

**Text-equivalent** (also accepted):
```
AMAKERBOT:unregister
```

---

### `0x43` HEARTBEAT

Keep-alive packet that the registered master **must** send at least once every **50 ms**. If no heartbeat is received for more than 50 ms, the firmware performs an emergency motor stop and logs `[AMAKERBOT] Heartbeat timeout - stopping motors`.

```
REQUEST  : [0x43]   1 byte
RESPONSE : none
```

**Behaviour**:
- Accepted only from the currently registered master IP. Packets from other IPs are ignored.
- On receipt, updates the internal timestamp; clears the timed-out state if a previous timeout had fired.
- On timeout (> 50 ms without a heartbeat): calls `setAllMotorsSpeed(0)` and `setAllServoSpeed(0)` **once** (edge-triggered — no repeated calls until the next timeout event).
- The watchdog is active only while a master is registered **and** at least one heartbeat has been received in the current session.

> ⚠️ **Critical for robot operation**: start sending heartbeats immediately after successful registration. The 50 ms deadline is wall-clock time (checked every ~10 ms from the Core 0 UDP task).

**No reply** is sent.

---

## Quick-Reference Table

### Binary commands

| Action byte | Service | Command | Min bytes | Request payload (after action byte) | Response payload |
|---|---|---|---|---|---|
| `0x11` | BoardInfo | SET_LED_COLOR | 5 | `[led:0-2][r][g][b]` | — |
| `0x12` | BoardInfo | TURN_OFF_LED | 2 | `[led:0-2]` | — |
| `0x13` | BoardInfo | TURN_OFF_ALL_LEDS | 1 | _(none)_ | — |
| `0x14` | BoardInfo | GET_LED_STATUS | 1 | _(none)_ | JSON `{leds:[{led,red,green,blue}×3]}` |
| `0x21` | Servo | SET_SERVO_ANGLE | 3 | `[ch0:int16_LE]...[chN:int16_LE]` — bit0=0 skip, bit0=1 angle=`raw>>1` (centre-zero °) | — |
| `0x22` | Servo | SET_SERVO_SPEED | 3 | `[mask][sp_ch0]...[sp_ch7]` — 1 byte per channel position, `encoded=speed+128`; optional `[dur_lo][dur_hi]` uint16 LE auto-stop delay (ms) | — |
| `0x23` | Servo | STOP_SERVOS | 2 | `[mask]` | — |
| `0x24` | Servo | ATTACH_SERVO | 3 | `[mask][type:0-3]` | — |
| `0x25` | Servo | SET_MOTOR_SPEED | 2 | `[sp_m0]...[sp_mN]` — sequential, no mask, `encoded=speed+128` | — |
| `0x26` | Servo | STOP_MOTORS | 2 | `[mask]` | — |
| `0x27` | Servo | GET_SERVO_STATUS | 2 | `[mask]` | JSON `{attached_servos:[{channel,connection}]}` |
| `0x28` | Servo | GET_ALL_STATUS | 1 | _(none)_ | JSON (all 8 channels) |
| `0x29` | Servo | GET_BATTERY | 1 | _(none)_ | `[batt:uint8]` 0–100 |
| `0x31` | DFR1216 | SET_LED_COLOR | 6 | `[led:0-2][r][g][b][brightness]` | — |
| `0x32` | DFR1216 | TURN_OFF_LED | 2 | `[led:0-2]` | — |
| `0x33` | DFR1216 | TURN_OFF_ALL_LEDS | 1 | _(none)_ | — |
| `0x34` | DFR1216 | GET_LED_STATUS | 1 | _(none)_ | JSON `{leds:[{id,red,green,blue}×3]}` |
| `0x41` | AmakerBot | MASTER_REGISTER | 2 | `[token bytes…]` (ASCII, typically 5 chars) | _(none)_ — silent |
| `0x42` | AmakerBot | MASTER_UNREGISTER | 1 | _(none)_ | _(none)_ — silent |
| `0x43` | AmakerBot | HEARTBEAT | 1 | _(none)_ | _(none)_ — silent |

### Text commands (MusicService prefix `Music`; AmakerBotService prefix `AMAKERBOT`)

| Command | Payload | Required fields | Response |
|---|---|---|---|
| `Music:play` | JSON | `melody` (0–19) [, `option` (1/2/4/8)] | standard text |
| `Music:tone` | JSON | `freq` (>0 Hz) [, `beat` (>0)] | standard text |
| `Music:stop` | _(none)_ | — | standard text |
| `Music:melodies` | _(none)_ | — | JSON string array (raw, no wrapper) |
| `Music:playnotes` | hex string | tempo byte + note/duration pairs | standard text |


> Prefer the binary equivalents `0x41` / `0x42` / `0x43` for new code.

---

## Code Generation Guidelines

### Binary protocol rules

1. **Action byte first**: always send the action byte as the very first byte.
2. **Little-endian int16**: servo angles are 2-byte signed integers, LSB first.
3. **SET_SERVO_SPEED payload alignment**: speed bytes are consumed **one per channel position** (0, 1, 2, …). To address channel N you must provide N+1 bytes. Recommended: always send all 8 channel bytes, using `0x80` (speed=0) for don't-care channels.
4. **SET_MOTOR_SPEED is maskless**: bytes set motors 0, 1, 2, 3 in sequence. No mask byte is present.
5. **Servo workflow**: always send `0x24` ATTACH_SERVO before any angle or speed command.
6. **Motor mask** is 0-indexed (bit 0 → board motor 1); servo mask is 0-indexed (bit 0 → channel 0).
7. **No text encoding**: binary frames must **not** be null-terminated.
8. **K10SensorsService GET_SENSORS is broken via UDP**: do not send `0x21` expecting sensor data — that action byte is owned by ServoService's SET_SERVO_ANGLE.

### Master registration rules

9. **Register before using protected routes**: ServoService and DFR1216Service enforce master-IP checks. Send `[0x41][token]` (token visible on device screen in `MODE_APP_LOG`) from your client before issuing servo/motor/LED commands.
10. **Master is IP-bound**: registration locks to the sender's IP address. If your client's IP changes, re-register.
11. **Binary `0x06` response**: if you receive `[action][0x06]` from a binary command, your IP is not the registered master — register first.
12. **AmakerBotService has no reply**: `0x41` / `0x42` / `0x43` never send a UDP reply. Confirm registration via HTTP `GET /api/amakerbot/v1/master`.
13. **Heartbeat is mandatory**: after registration, send `[0x43]` at least every **50 ms** or all motors and servos will be stopped automatically. The watchdog only activates after the first heartbeat is received in a session — but start sending immediately to avoid races.

### Common rules (both protocols)

13. **Port**: always send to port `24642` unless the device reports a different port via the HTTP API.
14. **Listen for reply**: bind your socket before sending — the device replies to the **sender IP and port**.
15. **Timeout**: always implement a receive timeout (recommended: 2 s). If the service was not yet started, the binary protocol returns `resp_not_started`; MusicService silently drops the message when not started.
16. **Max payload**: keep requests and responses within **256 bytes**.

### Python examples

```python
import socket
import struct

UDP_IP   = "192.168.x.x"   # K10 device IP
UDP_PORT = 24642
TIMEOUT  = 2.0

def send_raw(data: bytes) -> bytes | None:
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.settimeout(TIMEOUT)
        s.sendto(data, (UDP_IP, UDP_PORT))
        try:
            resp, _ = s.recvfrom(512)
            return resp
        except socket.timeout:
            return None

def send_text(message: str) -> str | None:
    resp = send_raw(message.encode())
    return resp.decode() if resp else None

# ── ServoService (0x2x) ───────────────────────────────────────────────────────

# ATTACH_SERVO: channel 0 → Angular 180 (mask=0x01, type=2)
send_raw(bytes([0x24, 0x01, 0x02]))

# SET_SERVO_ANGLE: channel 0 → +45°, skip channel 1, channel 2 → −90°
ch0 = struct.pack('<h', (45  << 1) | 1)   # 0x5B 0x00
ch1 = struct.pack('<h', 0)                 # skip
ch2 = struct.pack('<h', (-90 << 1) | 1)   # 0x4D 0xFF
send_raw(bytes([0x21]) + ch0 + ch1 + ch2)

# SET_SERVO_SPEED: channels 0 and 2 → +50 each (mask=0x05)
# Safe pattern: always send all 8 bytes, 0x80 for don't-care
# encoded = speed + 128  →  50 + 128 = 178 = 0xB2
send_raw(bytes([0x22, 0x05, 0xB2, 0x80, 0xB2, 0x80, 0x80, 0x80, 0x80, 0x80]))

# STOP_SERVOS: channels 0 and 2 (mask=0x05)
send_raw(bytes([0x23, 0x05]))

# ATTACH_SERVO: channel 0 → Rotational (mask=0x01, type=1)
send_raw(bytes([0x24, 0x01, 0x01]))

# SET_MOTOR_SPEED: motor 0 → +75, motor 1 → −50 (no mask)
# encoded: 75+128=203=0xCB,  -50+128=78=0x4E
send_raw(bytes([0x25, 0xCB, 0x4E]))

# STOP_MOTORS: all motors (mask=0x0F)
send_raw(bytes([0x26, 0x0F]))

# GET_ALL_STATUS
resp = send_raw(bytes([0x28]))
if resp and len(resp) >= 2 and resp[1] == 0x00:
    import json
    status = json.loads(resp[2:].decode())

# GET_BATTERY
resp = send_raw(bytes([0x29]))
if resp and len(resp) == 3 and resp[1] == 0x00:
    battery_pct = resp[2]

# ── BoardInfoService (0x1x) — onboard RGB LEDs ────────────────────────────────

# SET_LED_COLOR: LED 0 → red
send_raw(bytes([0x11, 0, 255, 0, 0]))

# SET_LED_COLOR: LED 1 → green
send_raw(bytes([0x11, 1, 0, 255, 0]))

# TURN_OFF_LED: LED 1
send_raw(bytes([0x12, 1]))

# TURN_OFF_ALL_LEDS
send_raw(bytes([0x13]))

# GET_LED_STATUS  →  {"leds":[{"led":0,"red":...}, ...]}
resp = send_raw(bytes([0x14]))
if resp and len(resp) >= 2 and resp[1] == 0x00:
    import json
    leds = json.loads(resp[2:].decode())["leds"]

# ── DFR1216Service (0x3x) — expansion board RGB LEDs ─────────────────────────

# SET_LED_COLOR: LED 0 → green, brightness 128
send_raw(bytes([0x31, 0, 0, 255, 0, 128]))

# TURN_OFF_LED: LED 2
send_raw(bytes([0x32, 2]))

# TURN_OFF_ALL_LEDS
send_raw(bytes([0x33]))

# GET_LED_STATUS  →  {"leds":[{"id":0,"red":...}, ...]}
resp = send_raw(bytes([0x34]))
if resp and len(resp) >= 2 and resp[1] == 0x00:
    import json
    leds = json.loads(resp[2:].decode())["leds"]

# ── MusicService — text protocol ──────────────────────────────────────────────

send_text('Music:play:{"melody":8,"option":4}')
send_text('Music:tone:{"freq":440}')
send_text('Music:stop')
send_text('Music:melodies')

# playnotes helper
def build_playnotes(bpm: int, notes: list[tuple[int | None, int]]) -> str:
    """
    Build a hex payload for Music:playnotes.
    notes: list of (midi_note, sixteenths) where midi_note=None → silence.
    """
    tempo = min(max(bpm, 1), 240)
    payload = bytes([tempo])
    for midi, dur in notes:
        note_byte = 0x80 if midi is None else (midi & 0x7F)
        dur_byte  = max(1, min(dur, 255))
        payload  += bytes([note_byte, dur_byte])
    return payload.hex().upper()

# Play A4 (MIDI 69) quarter note then silence eighth at 140 BPM
hex_str = build_playnotes(140, [(69, 4), (None, 2)])
send_text(f'Music:playnotes:{hex_str}')

# Manual example: C4 quarter note + silence eighth at 120 BPM
# tempo=0x78, note=0x3C dur=0x04, silence=0x80 dur=0x02  →  "783C048002"
send_text('Music:playnotes:783C048002')

# ── AmakerBotService — master registration & heartbeat ──────────────────────

# Token is shown on the K10 screen in MODE_APP_LOG at boot.
# You can also retrieve it via HTTP GET /api/amakerbot/v1/token

MY_TOKEN = "A3K9B"  # Replace with actual token from device screen

# Register this machine as master: binary 0x41 + token bytes (no reply expected)
send_raw(bytes([0x41]) + MY_TOKEN.encode())

# Confirm via HTTP that registration succeeded:
#   GET http://<device-ip>/api/amakerbot/v1/master

# Heartbeat — must be sent at least every 50 ms while connected.
# Failure to do so triggers an emergency motor stop on the device.
# Typical pattern: run a background thread that sends [0x43] every 20–30 ms.

import threading, time

_hb_running = False
_hb_thread  = None

def _heartbeat_loop():
    hb = bytes([0x43])
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        while _hb_running:
            s.sendto(hb, (UDP_IP, UDP_PORT))
            time.sleep(0.025)   # 25 ms → well within the 50 ms deadline

def start_heartbeat():
    global _hb_running, _hb_thread
    _hb_running = True
    _hb_thread  = threading.Thread(target=_heartbeat_loop, daemon=True)
    _hb_thread.start()

def stop_heartbeat():
    global _hb_running
    _hb_running = False

# Usage:
start_heartbeat()       # start after successful registration
# ... drive the robot ...
stop_heartbeat()        # stop heartbeat when done

# Unregister (only works if we are the current master — no reply expected)
send_raw(bytes([0x42]))
```
