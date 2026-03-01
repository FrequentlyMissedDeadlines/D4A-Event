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
| 3 | **MusicService** | `Music` | play, tone, stop, melodies |

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

Set the speed of one continuous-rotation servo.

**Full message**

```
Servo Service:setServoSpeed:{"channel":<uint8>,"speed":<int8>}
```

| JSON field | Type | Range | Description |
|---|---|---|---|
| `channel` | `uint8` | 0–7 | Target servo channel |
| `speed` | `int8` | -100–100 | Speed percentage; negative = reverse |

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

Set all attached continuous-rotation servos to the same speed simultaneously.

**Full message**

```
Servo Service:setAllServoSpeed:{"speed":<int8>}
```

| JSON field | Type | Range | Description |
|---|---|---|---|
| `speed` | `int8` | -100–100 | Speed percentage; negative = reverse |

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

Set the speed for multiple servos in a single message.

**Full message**

```
Servo Service:setServosSpeedMultiple:{"servos":[{"channel":<uint8>,"speed":<int8>}, ...]}
```

| JSON field | Type | Description |
|---|---|---|
| `servos` | `JsonArray` | Array of channel/speed pairs |
| `servos[].channel` | `uint8` | Servo channel (0–7) |
| `servos[].speed` | `int8` | Speed percentage (-100–100) |

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

## Reply Format

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
