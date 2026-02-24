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

### 2. Plain-text command format (RemoteControlService)

Used by `RemoteControlService`. Commands are single lowercase words, trimmed of surrounding whitespace.

```
<command>
```

**Example**

```
forward
```

---

## Active Handlers (registered at boot)

Handler registration happens in `main.cpp` `setup()`. Services must implement `IsUDPMessageHandlerInterface` to be auto-registered.

| Handler order | Service | Format |
|---|---|---|
| 1 | **ServoService** | Structured (`Servo Service:<cmd>:<json>`) |

> `K10SensorsService`, `BoardInfoService`, and `MusicService` are listed as UDP-aware candidates in `main.cpp` but do **not** currently implement `IsUDPMessageHandlerInterface`, so they receive no UDP traffic.

> `RemoteControlService` is **defined** in the codebase but is **not instantiated** in `main.cpp`. Its plain-text commands are therefore not active. See the [Not-yet-active handlers](#not-yet-active-handlers-remotecontrolservice) section below.

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

## Not-yet-active handlers: RemoteControlService

`RemoteControlService` is fully implemented in [src/services/implementations/remotecontrol/RemoteControlService.cpp](../../src/services/implementations/remotecontrol/RemoteControlService.cpp) but is **not instantiated** in `main.cpp`. The commands below will be active once `RemoteControlService` is wired up.

All commands are plain lowercase strings, whitespace-trimmed, case-insensitive.

| Command | Action | Implementation status |
|---|---|---|
| `forward` | Move forward | ⚠️ stub (`executeForward` — TODO) |
| `backward` | Move backward | ⚠️ stub (`executeBackward` — TODO) |
| `turn_left` | Turn left | ⚠️ stub (`executeTurnLeft` — TODO) |
| `turn_right` | Turn right | ⚠️ stub (`executeTurnRight` — TODO) |
| `stop` | Stop all movement | ⚠️ stub (`executeStop` — TODO) |

The following constants are also defined in `RemoteControlConsts` but are **not handled** in the current `handleMessage()` implementation:

| Constant | Value | Status |
|---|---|---|
| `cmd_up` | `up` | Defined, not handled |
| `cmd_down` | `down` | Defined, not handled |
| `cmd_left` | `left` | Defined, not handled |
| `cmd_right` | `right` | Defined, not handled |
| `cmd_circle` | `circle` | Defined, not handled |
| `cmd_square` | `square` | Defined, not handled |
| `cmd_triangle` | `triangle` | Defined, not handled |
| `cmd_cross` | `cross` | Defined, not handled |

---

## Reply Format

All `ServoService` commands reply synchronously via UDP to the sender's IP/port.

**Success reply**

```json
{"status":"ok","action":"<command>"}
```

**Error reply**

```json
{"status":"error","message":"<reason>"}
```

---

## Sending UDP Messages — Quick Reference

### Linux / macOS shell

```bash
# Structured format (ServoService)
echo -n "Servo Service:setServoAngle:{\"channel\":0,\"angle\":90}" | nc -u -w1 <robot-ip> 24642

# Stop all servos
echo -n "Servo Service:stopAll" | nc -u -w1 <robot-ip> 24642
```

### Python

```python
import socket, json

ROBOT_IP = "<robot-ip>"
UDP_PORT = 24642

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

def send_servo_command(command: str, payload: dict | None = None):
    msg = f"Servo Service:{command}"
    if payload:
        msg += ":" + json.dumps(payload, separators=(",", ":"))
    sock.sendto(msg.encode(), (ROBOT_IP, UDP_PORT))

# Examples
send_servo_command("setServoAngle", {"channel": 0, "angle": 90})
send_servo_command("setServoSpeed", {"channel": 1, "speed": 50})
send_servo_command("stopAll")
send_servo_command("setServosAngleMultiple", {"servos": [{"channel": 0, "angle": 90}, {"channel": 1, "angle": 45}]})
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
    &my_service,   // ← add here
};
```

See [docs/user guides/UDPServiceHandlers.md](UDPServiceHandlers.md) for detailed registration patterns and best practices.
