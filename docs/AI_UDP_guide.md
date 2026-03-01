# K10-Bot UDP API — AI Reference Guide

> **Target audience**: AI agents and code-generation tools.  
> **Purpose**: Authoritative, machine-friendly reference for all UDP messages accepted by the K10 Bot firmware.  
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

### A. Fast : Binary Protocol (ServoService, K10SensorsService)

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

| service_id | Service |
|---|---|
| `0x1` | K10SensorsService |
| `0x2` | ServoService |

#### Binary response codes (byte 1 of every binary response)

| Code | Constant | Meaning |
|---|---|---|
| `0x00` | `udp_resp_ok` | Success |
| `0x01` | `udp_resp_invalid_params` | Required byte/field missing |
| `0x02` | `udp_resp_invalid_values` | Field present but value out of range |
| `0x03` | `udp_resp_operation_failed` | Hardware-level failure |
| `0x04` | `udp_resp_not_started` | Service not yet started |
| `0x05` | `udp_resp_unknown_cmd` | Action code not recognised |

---

### B. Text Protocol (MusicService)

```
<ServiceName>:<command>[:<JSON>]
```

- `<ServiceName>` — **exact, case-sensitive** service name string.
- `<command>` — action token (ASCII, no spaces).
- `:<JSON>` — optional compact JSON payload. **Omit the colon entirely** if no payload is needed.

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
| `"Music action failed"` | Hardware-level failure (MusicService) |
| `"UDP unknown command"` | Command token not recognised |

---

### Routing order (registered at boot in `main.cpp`)

| Priority | Service | Protocol | Claim condition |
|---|---|---|---|
| 1 | ServoService | Binary | `action` byte in `0x21`–`0x29` |
| 2 | K10SensorsService | Binary | `action` byte in `0x11`–`0x11` |
| 3 | MusicService | Text | message starts with `Music:` |

---

## 1. ServoService — Binary Protocol

**service_id**: `0x2`  
**Hardware**: DFR0548 I²C servo controller  
**Servo channels**: 0–7 · **DC motors**: 1–4

### Channel / motor masks

- **Servo mask** (1 byte): bit N selects servo channel N (N = 0–7). `0xFF` = all channels.
- **Motor mask** (1 byte): bit N selects motor N+1 (N = 0–3). Bits 4–7 are ignored. `0x0F` = all motors.

### Connection type enum (used in ATTACH_SERVO)

| Value | Label | Meaning |
|---|---|---|
| `0` | `"Not Connected"` | No servo attached |
| `1` | `"Rotational"` | Continuous-rotation servo |
| `2` | `"Angular 180"` | 180° positional servo |
| `3` | `"Angular 270"` | 270° positional servo |

### Angle encoding (centre-zero int16 LE)

Angles are sent as a **signed 16-bit integer in little-endian** representing the offset from centre:

| Servo type | Valid range | Physical result |
|---|---|---|
| Angular 180 | −90 … +90 | 0° … 180° |
| Angular 270 | −135 … +135 | 0° … 270° |

---

### `0x21` SET_SERVO_ANGLE

Set the angle of individual angular servos. The payload is one `int16` (LE) per servo channel,
starting at channel 0. Missing trailing entries leave those channels unchanged.

**Per-channel int16 encoding:**
- `bit 0 = 0` → skip this channel (do nothing)
- `bit 0 = 1` → angle = `raw >> 1` (arithmetic right-shift, centre-zero degrees)

```
REQUEST  : [0x21][ch0:int16_LE][ch1:int16_LE]...[chN:int16_LE]   1 + N×2 B  (N = 1..8)
RESPONSE : [0x21][resp_code:1B]
```

| Bytes | Field | Type | Notes |
|---|---|---|---|
| 0 | action | uint8 | `0x21` |
| 1–2 | ch0 | int16 LE | channel 0 encoded value |
| 3–4 | ch1 | int16 LE | channel 1 encoded value (optional) |
| … | … | … | up to 8 channels total |

**Encoding example** – set channel 0 to +45°:  `raw = (45 << 1) | 1 = 91` → bytes `0x5B 0x00`

**Encoding example** – set channel 1 to −90°: `raw = (−90 << 1) | 1 = −179` → bytes `0x4D 0xFF`

**Encoding example** – skip channel 2:          `raw = 0` → bytes `0x00 0x00`

| Servo type | Valid centre-zero range | Hardware range |
|---|---|---|
| Angular 180 | −90 … +90 | 0° … 180° |
| Angular 270 | −135 … +135 | 0° … 270° |

Response `resp_code`: `ok` · `operation_failed` (channel type mismatch or angle out of range)

---

### `0x22` SET_SERVO_SPEED

Set the speed of one or more continuous-rotation servos selected by mask.

```
REQUEST  : [0x22][mask:1B][encoded_speed0:1B][encoded_speed1:1B]...   variable length
RESPONSE : [0x22][resp_code:1B]
```

| Byte | Field | Type | Notes |
|---|---|---|---|
| 0 | action | uint8 | `0x22` |
| 1 | mask | uint8 | servo bitmask; each bit selects corresponding servo channel |
| 2+ | encoded_speeds | uint8 | One byte per servo in order (0–7); **Formula**: `encoded_byte = speed + 128` |

**Speed Encoding**: Each speed byte encodes the actual speed via `speed = encoded_byte − 128`, shifting range 28–228 (0x1C–0xE4) to −100 … +100:
- Speed −100 → byte 0x1C (28)
- Speed 0 → byte 0x80 (128)
- Speed +100 → byte 0xE4 (228)

**Variable payload**: Send one byte per servo corresponding to each bit set in mask. Example: mask=0x05 (servos 0,2) → send 2 bytes (servo0, servo2).

Response `resp_code`: `ok` · `invalid_params` (< 3 B or insufficient speed bytes) · `invalid_values` (decoded speed out of range) · `operation_failed` (channel not ROTATIONAL)

---

### `0x23` STOP_SERVOS

Stop all continuous-rotation servos selected by mask (sets speed to 0).

```
REQUEST  : [0x23][mask:1B]   total 2 B
RESPONSE : [0x23][resp_code:1B]
```

| Byte | Field | Type | Notes |
|---|---|---|---|
| 0 | action | uint8 | `0x23` |
| 1 | mask | uint8 | servo bitmask; `0xFF` = all |

Response `resp_code`: `ok` · `invalid_params` (< 2 B)

---

### `0x24` ATTACH_SERVO

Set the servo type on one or more channels. Must be called **before** angle/speed commands.

```
REQUEST  : [0x24][mask:1B][type:1B]   total 3 B
RESPONSE : [0x24][resp_code:1B]
```

| Byte | Field | Type | Notes |
|---|---|---|---|
| 0 | action | uint8 | `0x24` |
| 1 | mask | uint8 | servo bitmask |
| 2 | type | uint8 | connection enum 0–3 |

Response `resp_code`: `ok` · `invalid_params` (< 3 B) · `invalid_values` (type > 3)

---

### `0x25` SET_MOTOR_SPEED

Set the speed of individual DC motors. The payload is one encoded speed byte per motor channel,
starting at motor 0 (= motor 1 on the board). Missing trailing entries leave those motors unchanged.

**Per-motor byte encoding**: Each speed byte encodes the actual speed via `speed = encoded_byte − 128`, 
shifting range 28–228 (0x1C–0xE4) to −100 … +100:
- Speed −100 → byte 0x1C (28)
- Speed 0 → byte 0x80 (128)
- Speed +100 → byte 0xE4 (228)

```
REQUEST  : [0x25][encoded_m0:1B][encoded_m1:1B]...[encoded_mN:1B]   1 + N B  (N = 1..4)
RESPONSE : [0x25][resp_code:1B]
```

| Bytes | Field | Type | Notes |
|---|---|---|---|
| 0 | action | uint8 | `0x25` |
| 1 | m0 | uint8 | motor 0 (board motor 1) encoded speed |
| 2 | m1 | uint8 | motor 1 (board motor 2) encoded speed (optional) |
| … | … | … | up to 4 motors total |

**Encoding example** – motor 0 at +75: `encoded = 75 + 128 = 203` → byte 0xCB

**Encoding example** – motor 1 at −50: `encoded = -50 + 128 = 78` → byte 0x4E

Response `resp_code`: `ok` · `invalid_params` (< 2 B) · `invalid_values` (decoded speed out of range) · `operation_failed` (hardware error)

---

### `0x26` STOP_MOTORS

Stop DC motors selected by motor mask.

```
REQUEST  : [0x26][mask:1B]   total 2 B
RESPONSE : [0x26][resp_code:1B]
```

| Byte | Field | Type | Notes |
|---|---|---|---|
| 0 | action | uint8 | `0x26` |
| 1 | mask | uint8 | motor bitmask; `0x0F` = all |

Response `resp_code`: `ok` · `invalid_params` (< 2 B)

---

### `0x27` GET_SERVO_STATUS

Get connection status of servo channels selected by mask as JSON.

```
REQUEST  : [0x27][mask:1B]   total 2 B
RESPONSE : [0x27][0x00][JSON payload]
```

Response JSON shape:
```json
{"attached_servos":[{"channel":0,"connection":"Not Connected"},{"channel":2,"connection":"Angular 180"}]}
```
Only channels whose bit is set in `mask` appear in the array.

---

### `0x28` GET_ALL_STATUS

Get connection status of all 8 servo channels as JSON.

```
REQUEST  : [0x28]   total 1 B
RESPONSE : [0x28][0x00][JSON payload]
```

Response JSON shape (always 8 entries, channels 0–7 in order):
```json
{"attached_servos":[{"channel":0,"connection":"Not Connected"},{"channel":1,"connection":"Rotational"},{"channel":2,"connection":"Angular 180"},{"channel":3,"connection":"Angular 270"},{"channel":4,"connection":"Not Connected"},{"channel":5,"connection":"Not Connected"},{"channel":6,"connection":"Not Connected"},{"channel":7,"connection":"Not Connected"}]}
```

---

### `0x29` GET_BATTERY

Read the board battery level (0–100).

```
REQUEST  : [0x29]   total 1 B
RESPONSE : [0x29][0x00][batt:1B]
```

`batt` is a uint8 in the range 0–100 (%).

---

## 2. K10SensorsService — Binary Protocol

**service_id**: `0x1`  
**Hardware**: AHT20 (temperature/humidity) + onboard light sensor, microphone, accelerometer.

---

### `0x11` GET_SENSORS

Read all onboard sensor values. No payload.

```
REQUEST  : [0x11]   total 1 B
RESPONSE : [0x11][resp_code:1B][JSON payload]
```

Response `resp_code`: `ok` · `operation_failed` (AHT20 not ready) · `not_started`

Response JSON shape (on `ok`):
```json
{"light":125.5,"hum_rel":45.2,"celcius":23.8,"mic_data":512,"accelerometer":[0.12,-0.05,9.81]}
```

| Field | Unit |
|---|---|
| `light` | Ambient light (ALS raw) |
| `hum_rel` | Relative humidity % (AHT20) |
| `celcius` | Temperature °C (AHT20) |
| `mic_data` | Raw microphone ADC value |
| `accelerometer` | `[x, y, z]` m/s² (approx.) |

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
| `beat` | int | > 0 (1 beat ≈ 8000 units) | ❌ | `8000` |

**Response**: standard text success/error.

**Examples**:
```
Music:tone:{"freq":440}
Music:tone:{"freq":880,"beat":4000}
```

---

### `stop`

Stop current playback. No JSON payload.

```
Music:stop
```

**Response**: standard text success/error.

---

### `melodies`

Get the list of all available built-in melody names. No JSON payload.

```
Music:melodies
```

**Response** (raw JSON array, no wrapper):
```json
["DADADADUM","ENTERTAINER","PRELUDE","ODE","NYAN","RINGTONE","FUNK","BLUES","BIRTHDAY","WEDDING","FUNERAL","PUNCHLINE","BADDY","CHASE","BA_DING","WAWAWAWAA","JUMP_UP","JUMP_DOWN","POWER_UP","POWER_DOWN"]
```

---

## Quick-Reference Table

### Binary commands

| Action byte | Service | Command | Request bytes (after action) | Response payload |
|---|---|---|---|---|
| `0x21` | Servo | SET_SERVO_ANGLE | `[ch0:int16_LE]...[chN:int16_LE]`  bit0=0→skip, bit0=1→angle=raw>>1 | — |
| `0x22` | Servo | SET_SERVO_SPEED | `[mask][encoded_sp0:uint8]...[encoded_spN:uint8]`  formula: `encoded = speed + 128` | — |
| `0x23` | Servo | STOP_SERVOS | `[mask]` | — |
| `0x24` | Servo | ATTACH_SERVO | `[mask][type:0-3]` | — |
| `0x25` | Servo | SET_MOTOR_SPEED | `[encoded_m0:uint8]...[encoded_mN:uint8]`  formula: `encoded = speed + 128` | — |
| `0x26` | Servo | STOP_MOTORS | `[mask]` | — |
| `0x27` | Servo | GET_SERVO_STATUS | `[mask]` | JSON (selected channels) |
| `0x28` | Servo | GET_ALL_STATUS | _(none)_ | JSON (all 8 channels) |
| `0x29` | Servo | GET_BATTERY | _(none)_ | `[batt:uint8]` 0-100 |
| `0x11` | Sensors | GET_SENSORS | _(none)_ | JSON sensor object |

### Text commands (MusicService)

| Prefix | Command | Required JSON fields | Response |
|---|---|---|---|
| `Music` | `play` | `melody`(0-19) [, `option`] | standard text |
| `Music` | `tone` | `freq`(>0) [, `beat`] | standard text |
| `Music` | `stop` | _(none)_ | standard text |
| `Music` | `melodies` | _(none)_ | JSON string array |

---

## Code Generation Guidelines

### Binary protocol rules

1. **Action byte first**: always send the action byte as the very first byte.
2. **Little-endian int16**: angles are 2-byte signed integers, LSB first.
3. **Bitmasks**: build the mask from target channel/motor indices — do not send channel numbers directly.
4. **Servo workflow**: send `0x24` ATTACH_SERVO for each channel before any angle/speed command.
5. **Motor mask** is 0-indexed (bit 0 → motor 1, bit 1 → motor 2, …); servo mask is also 0-indexed.
6. **No text encoding**: binary frames must **not** be UTF-8 encoded or null-terminated.

### Common rules (both protocols)

7. **Port**: always send to port `24642` unless the device reports a different port via the HTTP API.
8. **Listen for reply**: bind your socket before sending — the device replies to the **sender IP and port**.
9. **Timeout**: if the service was not yet started, the binary protocol returns `resp_not_started`; the text protocol silently drops the message. Always implement a receive timeout.
10. **Max payload**: keep requests and responses within **256 bytes**.

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

# ── Binary examples ───────────────────────────────────────────────────────────

# ATTACH_SERVO: channel 0 → Angular 180 (mask=0x01, type=2)
send_raw(bytes([0x24, 0x01, 0x02]))

# SET_SERVO_ANGLE: channel 0 → +45°, channel 1 → skip, channel 2 → −90°
# Encode: raw = (angle << 1) | 1  to activate, or 0 to skip
ch0 = struct.pack('<h', (45  << 1) | 1)   # b'\x5b\x00'
ch1 = struct.pack('<h', 0)                 # skip
ch2 = struct.pack('<h', (-90 << 1) | 1)   # b'\x4d\xff'
send_raw(bytes([0x21]) + ch0 + ch1 + ch2)

# SET_SERVO_SPEED: channels 0 and 1 → speed +50 each (mask=0x03)
# Encode: speed_byte = speed + 128  (50 + 128 = 178 = 0xB2)
send_raw(bytes([0x22, 0x03, 0xB2, 0xB2]))

# STOP_SERVOS: all channels (mask=0xFF)
send_raw(bytes([0x23, 0xFF]))

# ATTACH_SERVO: channel 0 → Rotational (mask=0x01, type=1)
send_raw(bytes([0x24, 0x01, 0x01]))

# SET_MOTOR_SPEED: motor 0 → +75, motor 1 → −50
# Encode: motor_byte = speed + 128  (75 + 128 = 203 = 0xCB, −50 + 128 = 78 = 0x4E)
send_raw(bytes([0x25, 0xCB, 0x4E]))

# STOP_MOTORS: all motors (mask=0x0F)
send_raw(bytes([0x26, 0x0F]))

# GET_SENSORS
resp = send_raw(bytes([0x11]))
if resp and len(resp) >= 2 and resp[1] == 0x00:
    import json
    sensors = json.loads(resp[2:].decode())

# GET_BATTERY
resp = send_raw(bytes([0x29]))
if resp and len(resp) == 3 and resp[1] == 0x00:
    battery_pct = resp[2]

# ── Text examples (MusicService) ──────────────────────────────────────────────

send_text('Music:play:{"melody":8,"option":4}')
send_text('Music:tone:{"freq":440}')
send_text('Music:stop')
send_text('Music:melodies')
```
