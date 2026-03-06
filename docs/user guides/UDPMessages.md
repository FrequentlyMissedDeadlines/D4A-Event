# UDP Messages Reference

> **Transport**: UDP unicast on port **24642** (default, defined in `UDPService::port`).  
> **Max payload**: 256 bytes (hard limit in `UDPService.cpp`).  
> **Max buffered messages**: 20 (rolling window kept for the HTTP statistics API).

---

## Message Formats

Two message formats coexist in the application:

### 1. Structured service format (ServoService)

Used by services that implement `IsUDPMessageHandlerInterface`.

```
<ServiceName>:<command>:<JSON payload>
```

| Part | Description |
|---|---|
| `ServiceName` | Exact service name string (case-sensitive, e.g. `Servo Service`) |
| `command` | Action token (see tables below) |
| `JSON payload` | Optional ArduinoJson document; omit entirely if not needed |

**Example**

```
Servo Service:setServoAngle:{"channel":0,"angle":90}
```


---

## Active Handlers (registered at boot)

Handler registration happens in `main.cpp` `setup()`. Services must implement `IsUDPMessageHandlerInterface` to be auto-registered.

| Handler order | Service | Prefix | Commands |
|---|---|---|---|
| 1 | **ServoService** | `Servo Service` | setServoAngle, setServoSpeed, stopAll, setAllServoAngle, setAllServoSpeed, setServosAngleMultiple, setServosSpeedMultiple, attachServo, getStatus, getAllStatus, setMotorSpeed, stopAllMotors |
| 2 | **K10SensorsService** | `K10 Sensors Service` | getSensors |
| 3 | **BoardInfoService** | Binary `0x11`–`0x14` | SET_LED_COLOR, TURN_OFF_LED, TURN_OFF_ALL_LEDS, GET_LED_STATUS |
| 4 | **MusicService** | `Music` | play, tone, stop, melodies, playnotes |
| 5 | **DFR1216Service** | Binary `0x31`–`0x34` | SET_LED_COLOR, TURN_OFF_LED, TURN_OFF_ALL_LEDS, GET_LED_STATUS |
| 6 | **AmakerBotService** | Binary `0x41`–`0x43` / Text `AMAKERBOT` | MASTER_REGISTER, MASTER_UNREGISTER, HEARTBEAT |

> **Note on binary protocol**: BoardInfoService and DFR1216Service use a compact binary framing instead of the text `<Service>:<command>:<JSON>` format. See [docs/AI_UDP_guide.md](../../docs/AI_UDP_guide.md) for full binary encoding details.

---

## ServoService — Structured Commands

**Prefix**: `Servo Service`  
**Source file**: [src/services/implementations/servo/ServoService.cpp](../../src/services/implementations/servo/ServoService.cpp)

All commands send a JSON reply back to the sender's IP and port.

### `setServoAngle`

Set the angle of one angular servo (180° or 270°).

**Full message**

```
Servo Service:setServoAngle:{"channel":<uint8>,"angle":<uint16>}
```

| JSON field | Type | Range | Description |
|---|---|---|---|
| `channel` | `uint8` | 0–7 | Target servo channel |
| `angle` | `uint16` | 0–360 | Target angle in degrees |

**Notes**: Only meaningful for servos attached as angular (180° or 270°) type.

---

### `setServoSpeed`

Set the speed of one continuous-rotation servo. An optional `duration_ms` field schedules an automatic stop after the given number of milliseconds.

**Full message**

```
Servo Service:setServoSpeed:{"channel":<uint8>,"speed":<int8>}
Servo Service:setServoSpeed:{"channel":<uint8>,"speed":<int8>,"duration_ms":<uint32>}
```

| JSON field | Type | Range | Required | Description |
|---|---|---|---|---|
| `channel` | `uint8` | 0–7 | ✅ | Target servo channel |
| `speed` | `int8` | -100–100 | ✅ | Speed percentage; negative = reverse |
| `duration_ms` | `uint32` | ≥ 1 | ❌ | Auto-stop delay in ms. Servo stops automatically after this many ms. Omit for no timeout. |

---

### `stopAll`

Stop all servos immediately by setting speed to 0. No JSON payload required.

**Full message**

```
Servo Service:stopAll
```

---

### `setAllServoAngle`

Set all attached angular servos to the same angle simultaneously.

**Full message**

```
Servo Service:setAllServoAngle:{"angle":<uint16>}
```

| JSON field | Type | Range | Description |
|---|---|---|---|
| `angle` | `uint16` | 0–360 | Target angle applied to all angular servos |

---

### `setAllServoSpeed`

Set all attached continuous-rotation servos to the same speed simultaneously. An optional `duration_ms` field auto-stops all of them after the given number of milliseconds.

**Full message**

```
Servo Service:setAllServoSpeed:{"speed":<int8>}
Servo Service:setAllServoSpeed:{"speed":<int8>,"duration_ms":<uint32>}
```

| JSON field | Type | Range | Required | Description |
|---|---|---|---|---|
| `speed` | `int8` | -100–100 | ✅ | Speed percentage; negative = reverse |
| `duration_ms` | `uint32` | ≥ 1 | ❌ | Auto-stop delay in ms applied to all servos. Omit for no timeout. |

---

### `setServosAngleMultiple`

Set the angle for multiple servos in a single message.

**Full message**

```
Servo Service:setServosAngleMultiple:{"servos":[{"channel":<uint8>,"angle":<uint16>}, ...]}
```

| JSON field | Type | Description |
|---|---|---|
| `servos` | `JsonArray` | Array of channel/angle pairs |
| `servos[].channel` | `uint8` | Servo channel (0–7) |
| `servos[].angle` | `uint16` | Target angle (0–360) |

**Example**

```
Servo Service:setServosAngleMultiple:{"servos":[{"channel":0,"angle":90},{"channel":1,"angle":45}]}
```

---

### `setServosSpeedMultiple`

Set the speed for multiple servos in a single message. Each servo entry may include an independent `duration_ms` for per-servo auto-stop.

**Full message**

```
Servo Service:setServosSpeedMultiple:{"servos":[{"channel":<uint8>,"speed":<int8>}, ...]}
Servo Service:setServosSpeedMultiple:{"servos":[{"channel":<uint8>,"speed":<int8>,"duration_ms":<uint32>}, ...]}
```

| JSON field | Type | Required | Description |
|---|---|---|---|
| `servos` | `JsonArray` | ✅ | Array of channel/speed operations |
| `servos[].channel` | `uint8` | ✅ | Servo channel (0–7) |
| `servos[].speed` | `int8` | ✅ | Speed percentage (-100–100) |
| `servos[].duration_ms` | `uint32` | ❌ | Per-servo auto-stop delay in ms. Omit for no timeout. |

**Example**

```
Servo Service:setServosSpeedMultiple:{"servos":[{"channel":0,"speed":50,"duration_ms":500},{"channel":1,"speed":-30}]}
```

---

### `attachServo`

Register a servo type on a channel. Must be called before controlling a servo.

**Full message**

```
Servo Service:attachServo:{"channel":<uint8>,"connection":<uint8>}
```

| JSON field | Type | Range | Description |
|---|---|---|---|
| `channel` | `uint8` | 0–7 | Servo channel to configure |
| `connection` | `uint8` | 0–3 | Servo type: `0`=None, `1`=Continuous rotation, `2`=Angular 180°, `3`=Angular 270° |

---

### `getStatus`

Query the type and connection status of a single servo channel. Returns a JSON reply.

**Full message**

```
Servo Service:getStatus:{"channel":<uint8>}
```

| JSON field | Type | Range | Description |
|---|---|---|---|
| `channel` | `uint8` | 0–7 | Servo channel to query |

---

### `getAllStatus`

Query connection status and type for all 8 servo channels. No JSON payload required.

**Full message**

```
Servo Service:getAllStatus
```

---

### `setMotorSpeed`

Set DC motor speed on the DFR1216 expansion board (motors 1–4).

**Full message**

```
Servo Service:setMotorSpeed:{"motor":<uint8>,"speed":<int8>}
```

| JSON field | Type | Range | Description |
|---|---|---|---|
| `motor` | `uint8` | 1–4 | Motor channel |
| `speed` | `int8` | -100–100 | Speed percentage; negative = reverse |

---

### `stopAllMotors`

Stop all 4 DC motors immediately. No JSON payload required.

**Full message**

```
Servo Service:stopAllMotors
```

---

## K10SensorsService — Structured Commands

**Prefix**: `K10 Sensors Service`  
**Source file**: [src/services/implementations/sensor/K10sensorsService.cpp](../../src/services/implementations/sensor/K10sensorsService.cpp)

### `getSensors`

Read all on-board sensors in one shot: ambient light, temperature, humidity, microphone and 3-axis accelerometer.
No JSON payload required.

**Full message**

```
K10 Sensors Service:getSensors
```

**Reply (success)**

```json
{"light":125.5,"hum_rel":45.2,"celcius":23.8,"mic_data":512,"accelerometer":[0.12,-0.05,9.81]}
```

**Reply (sensor not ready)**

```json
{"result":"error","message":"AHT20 sensor not ready yet"}
```

---

## MusicService — Structured Commands

**Prefix**: `Music`  
**Source file**: [src/services/implementations/music/MusicService.cpp](../../src/services/implementations/music/MusicService.cpp)

### `play`

Play a built-in melody.

**Full message**

```
Music:play:{"melody":<int>,"option":<int>}
```

| JSON field | Type | Range | Required | Description |
|---|---|---|---|---|
| `melody` | `int` | 0–19 | ✅ | Melody index (see `melodies` command for names) |
| `option` | `int` | 1/2/4/8 | ❌ | Playback mode: `1`=Once, `2`=Forever, `4`=OnceInBackground *(default)*, `8`=ForeverInBackground |

**Example**

```
Music:play:{"melody":8,"option":4}
```

---

### `tone`

Play a raw tone at a given frequency for a given duration.

**Full message**

```
Music:tone:{"freq":<int>,"beat":<int>}
```

| JSON field | Type | Range | Required | Description |
|---|---|---|---|---|
| `freq` | `int` | > 0 | ✅ | Frequency in Hz |
| `beat` | `int` | > 0 | ❌ | Duration; 1 beat = 8 000 units *(default: 8000)* |

**Example**

```
Music:tone:{"freq":440,"beat":8000}
```

---

### `stop`

Stop any currently playing tone or melody. No JSON payload required.

**Full message**

```
Music:stop
```

---

### `melodies`

Return the list of built-in melody names. No JSON payload required.

**Full message**

```
Music:melodies
```

**Reply**

```json
["DADADADUM","ENTERTAINER","PRELUDE","ODE","NYAN","RINGTONE","FUNK","BLUES","BIRTHDAY","WEDDING","FUNERAL","PUNCHLINE","BADDY","CHASE","BA_DING","WAWAWAWAA","JUMP_UP","JUMP_DOWN","POWER_UP","POWER_DOWN"]
```

---

### `playnotes`

Play a custom note sequence defined as a hex-encoded binary string. See [docs/AI_UDP_guide.md](../../docs/AI_UDP_guide.md#playnotes) for full encoding details.

**Full message**

```
Music:playnotes:<hex_string>
```

**Example** (C4 quarter note + silence eighth at 120 BPM):

```
Music:playnotes:783C048002
```

---

## AmakerBotService — Master Registration & Heartbeat

**service_id**: `0x4`  
**Prefix**: `AMAKERBOT` (text) / binary action bytes `0x41`–`0x43`  
**Source file**: [src/services/implementations/amakerbot/AmakerBotService.cpp](../../src/services/implementations/amakerbot/AmakerBotService.cpp)

This service manages the "master controller" concept. Only the registered master IP is allowed to issue servo, motor, and LED commands to the robot.

### Registration flow

1. On boot, the device generates a 5-character hex token and logs it to the display in `MODE_APP_LOG`.
2. Your client registers by sending `[0x41]<token>` from its IP.
3. Start sending heartbeat packets (`[0x43]`) at least every **50 ms** to keep the motors armed. If heartbeats stop, all motors and continuous servos are stopped automatically.
4. While registered and heartbeating, protected services (ServoService, DFR1216Service) accept commands only from your IP.
5. Send `[0x42]` to release the registration.

---

### `0x41` MASTER_REGISTER

Register the sender’s IP as master (if token matches).

```
[0x41][token bytes…]
```

| Bytes | Description |
|---|---|
| `0x41` | Action byte |
| `token` | 5-character hex token from device screen (raw ASCII bytes, no null terminator) |

> No UDP reply. Silent on invalid token.

---

### `0x42` MASTER_UNREGISTER

Release master registration (sender must be current master).

```
[0x42]
```

> No UDP reply. Silent if sender is not master. Resets the heartbeat watchdog.

**Text-equivalent** (also accepted):
```
AMAKERBOT:unregister
```

---

### `0x43` HEARTBEAT

Keep-alive packet. **Must** be sent at least once every **50 ms** by the registered master or all motors and continuous servos are stopped.

```
[0x43]
```

> no UDP reply.

**Timeout behaviour**:
- The watchdog activates only after the **first** heartbeat is received in a session (i.e. safe to register before starting the heartbeat loop, but start it immediately).
- On timeout: `setAllMotorsSpeed(0)` + `setAllServoSpeed(0)` are called once (edge-triggered).
- The watchdog resets automatically when the next valid heartbeat arrives.
- Timeout event is logged on the device screen: `[AMAKERBOT] Heartbeat timeout - stopping motors`.

---

All structured-service commands reply synchronously via UDP to the sender's IP/port.

**Success reply**

```json
{"result":"ok","message":"<command>"}
```

**Error reply**

```json
{"result":"error","message":"<reason>"}
```

> **Note**: Query commands (`getStatus`, `getAllStatus`, `getSensors`, `melodies`) return their data payload directly instead of the `result`/`message` envelope.

---

## Sending UDP Messages — Quick Reference

### Linux / macOS shell

```bash
ROBOT=<robot-ip>
PORT=24642

# ServoService
echo -n 'Servo Service:setServoAngle:{"channel":0,"angle":90}' | nc -u -w1 $ROBOT $PORT
echo -n 'Servo Service:stopAll'                                | nc -u -w1 $ROBOT $PORT
echo -n 'Servo Service:setMotorSpeed:{"motor":1,"speed":75}'  | nc -u -w1 $ROBOT $PORT
echo -n 'Servo Service:stopAllMotors'                         | nc -u -w1 $ROBOT $PORT

# K10SensorsService
echo -n 'K10 Sensors Service:getSensors'                      | nc -u -w1 $ROBOT $PORT

# MusicService
echo -n 'Music:play:{"melody":0,"option":4}'                  | nc -u -w1 $ROBOT $PORT
echo -n 'Music:tone:{"freq":440,"beat":8000}'                 | nc -u -w1 $ROBOT $PORT
echo -n 'Music:stop'                                          | nc -u -w1 $ROBOT $PORT
```

### Python

```python
import socket, json

ROBOT_IP = "<robot-ip>"
UDP_PORT = 24642

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

def send(service: str, command: str, payload: dict | None = None) -> None:
    msg = f"{service}:{command}"
    if payload:
        msg += ":" + json.dumps(payload, separators=(",", ":"))
    sock.sendto(msg.encode(), (ROBOT_IP, UDP_PORT))

# ServoService
send("Servo Service", "setServoAngle",          {"channel": 0, "angle": 90})
send("Servo Service", "setServoSpeed",          {"channel": 1, "speed": 50})
send("Servo Service", "stopAll")
send("Servo Service", "setAllServoAngle",       {"angle": 90})
send("Servo Service", "setAllServoSpeed",       {"speed": 0})
send("Servo Service", "setServosAngleMultiple", {"servos": [{"channel": 0, "angle": 90}, {"channel": 1, "angle": 45}]})
send("Servo Service", "setServosSpeedMultiple", {"servos": [{"channel": 0, "speed": 50}, {"channel": 1, "speed": -30}]})
send("Servo Service", "attachServo",            {"channel": 0, "connection": 2})
send("Servo Service", "getStatus",              {"channel": 0})
send("Servo Service", "getAllStatus")
send("Servo Service", "setMotorSpeed",          {"motor": 1, "speed": 75})
send("Servo Service", "stopAllMotors")

# K10SensorsService
send("K10 Sensors Service", "getSensors")

# MusicService
send("Music", "play",     {"melody": 8, "option": 4})   # Birthday, once in background
send("Music", "tone",     {"freq": 440, "beat": 8000})
send("Music", "stop")
send("Music", "melodies")
```

### AmakerBotService — binary registration and heartbeat

```python
import socket, threading, time

ROBOT_IP = "<robot-ip>"
UDP_PORT  = 24642
MY_TOKEN  = "A3K9B"   # replace with the token shown on the device screen

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# 1. Register as master: binary 0x41 + token bytes (no reply)
sock.sendto(bytes([0x41]) + MY_TOKEN.encode(), (ROBOT_IP, UDP_PORT))

# 2. Start heartbeat thread — must fire at least every 50 ms
_running = True

def _hb_loop():
    hb = bytes([0x43])
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        while _running:
            s.sendto(hb, (ROBOT_IP, UDP_PORT))
            time.sleep(0.025)   # 25 ms → safe margin

hb_thread = threading.Thread(target=_hb_loop, daemon=True)
hb_thread.start()

# 3. Drive the robot…

# 4. Stop heartbeat and unregister (binary 0x42, no reply)
_running = False
hb_thread.join()
sock.sendto(bytes([0x42]), (ROBOT_IP, UDP_PORT))
```

---

## How to Add a New UDP Handler

1. Include `isUDPMessageHandlerInterface.h` in your service header.
2. Inherit from `IsUDPMessageHandlerInterface` (in addition to `IsServiceInterface`).
3. Override `messageHandler()` and `asUDPMessageHandlerInterface()`.
4. Add the service pointer to the `udp_aware_services[]` array in `main.cpp` `setup()`.

```cpp
// MyService.h
#include "isUDPMessageHandlerInterface.h"

class MyService : public IsServiceInterface, public IsUDPMessageHandlerInterface
{
public:
    bool messageHandler(const std::string &message,
                        const IPAddress &remoteIP,
                        uint16_t remotePort) override;

    IsUDPMessageHandlerInterface *asUDPMessageHandlerInterface() override { return this; }
};
```

```cpp
// main.cpp — add to udp_aware_services[]
IsServiceInterface *udp_aware_services[] = {
    &servo_service,
    &k10sensors_service,
    &music_service,
    &my_service,   // ← add here
};
```

See [docs/user guides/UDPServiceHandlers.md](UDPServiceHandlers.md) for detailed registration patterns and best practices.
