# IsUDPMessageHandlerInterface Documentation

## Overview

`IsUDPMessageHandlerInterface` is the interface for services that handle incoming UDP messages. Services implementing it are automatically dispatched matching UDP packets by the `UDPService` handler loop registered in `main.cpp`.

**Location**: [include/isUDPMessageHandlerInterface.h](../../include/isUDPMessageHandlerInterface.h)

---

## Class Hierarchy

```
IsMasterRegistryInterface  (virtual base)
        │
        └── IsUDPMessageHandlerInterface
```

`IsUDPMessageHandlerInterface` inherits **virtually** from `IsMasterRegistryInterface`, so a service that combines it with `IsOpenAPIInterface` (which also inherits `IsMasterRegistryInterface` virtually) ends up with a single shared base — no diamond ambiguity.

Typical service declaration:

```cpp
class MyService : public IsOpenAPIInterface, public IsUDPMessageHandlerInterface
{
public:
    // IsServiceInterface lifecycle (from IsOpenAPIInterface chain)
    std::string getServiceName()   override { return "My Service"; }
    std::string getServiceSubPath() override { return "myservice/v1"; }
    bool initializeService()        override { ... }
    bool startService()             override { ... }
    bool stopService()              override { ... }
    bool registerRoutes()           override { ... }

    // IsUDPMessageHandlerInterface
    bool messageHandler(const std::string &message,
                        const IPAddress   &remoteIP,
                        uint16_t           remotePort) override;

    IsUDPMessageHandlerInterface *asUDPMessageHandlerInterface() override { return this; }

    // IsMasterRegistryInterface — defaults provided by IsOpenAPIInterface; no override needed
    // unless this service IS the master registry (e.g. AmakerBotService)
};
```

> **Note**: `asUDPMessageHandlerInterface()` has a default implementation in the base that returns `this`. You only need to override it if you need to return a different object (unusual).

---

## UDPProto — Shared Binary Response Codes

The `UDPProto` namespace defines the standard second byte of a binary reply frame:

```
RESPONSE frame: [action:1B][resp_code:1B][optional_payload…]
```

| Constant | Value | Meaning |
|---|---|---|
| `udp_resp_ok` | `0x00` | Command executed successfully |
| `udp_resp_invalid_params` | `0x01` | Missing or malformed parameters |
| `udp_resp_invalid_values` | `0x02` | Parameters present but values out of range |
| `udp_resp_operation_failed` | `0x03` | Command understood, but execution failed |
| `udp_resp_not_started` | `0x04` | Service not yet started |
| `udp_resp_unknown_cmd` | `0x05` | Unknown command byte |
| `udp_resp_not_master` | `0x06` | Sender IP is not the registered master |

> These codes are for **binary** (non-JSON) UDP protocols. Text-protocol services that reply with JSON use their own `{"result":"ok"|"error"}` format instead.

---

## Message Formats

### Text protocol (most services)

```
<ServiceName>:<command>[:<JSON>]
```

Examples:
```
Servo Service:setServoAngle:{"channel":0,"angle":90}
Music:play:{"melody":0,"option":4}
K10 Sensors Service:getSensors
```

### Binary protocol (AmakerBotService, BoardInfoService)

Raw bytes — first byte is the action code, remaining bytes are payload. Reply mirrors the request with an appended status byte (`UDPResponseStatus` for AmakerBot, `UDPProto` for others).

---

## Interface Methods

### `messageHandler()`
```cpp
virtual bool messageHandler(const std::string &message,
                             const IPAddress   &remoteIP,
                             uint16_t           remotePort) = 0;
```

**Purpose**: Dispatched for every incoming UDP packet. The handler chain in `main.cpp` calls each registered service in order; the first to return `true` stops the chain.

**Parameters**:
- `message` — Raw UDP payload. May contain null bytes (binary payloads); use `message[0]` for action dispatch, not `c_str()` comparisons.
- `remoteIP` — Sender IP, used to send replies via `udp_service.sendReply()`.
- `remotePort` — Sender port, passed through to `sendReply()`.

**Returns**:
- `true` — message was **claimed** by this handler (even if an error reply was sent). No further handlers are tried.
- `false` — message is **not for this service**. Pass to the next handler.

**Important**: always return `true` if the first byte / prefix matches your service, even when sending an error response. Returning `false` only when the message genuinely doesn't belong to you.

---

### `asUDPMessageHandlerInterface()`
```cpp
virtual IsUDPMessageHandlerInterface *asUDPMessageHandlerInterface();
```

**Purpose**: Downcast helper that avoids `dynamic_cast` (forbidden by project rules). Default implementation returns `this`.

Used in `main.cpp` to retrieve the handler pointer from a generic `IsServiceInterface*`:

```cpp
IsUDPMessageHandlerInterface *h = svc->asUDPMessageHandlerInterface();
if (h) udp_service.registerMessageHandler(...);
```

---

## Protected Helper

### `checkUDPIsMaster()`
```cpp
bool checkUDPIsMaster(uint8_t action,
                      const IPAddress &remoteIP,
                      const IsMasterRegistryInterface *masterRegistry,
                      std::string &errorResponse);
```

**Purpose**: Guards a binary UDP command so it can only be executed by the registered master. On failure it fills `errorResponse` with the two-byte error frame `[action][0x06]` ready to send.

**Parameters**:
- `action` — The action byte to echo in the error frame.
- `remoteIP` — Sender IP from the incoming packet.
- `masterRegistry` — Pointer to the master registry (typically `&amakerbot_service`). If `nullptr`, the check always fails.
- `errorResponse` — Output: set to `[action][udp_resp_not_master]` when the check fails; untouched on success.

**Returns**:
- `true` — sender is the registered master; proceed with the command.
- `false` — sender is not the master; `errorResponse` is filled, caller must send it and return `true`.

**Usage pattern**:

```cpp
bool MyService::messageHandler(const std::string &message,
                                const IPAddress   &remoteIP,
                                uint16_t           remotePort)
{
    if (message[0] != MY_ACTION_BYTE) return false;  // not our message

    std::string resp;
    if (!checkUDPIsMaster(message[0], remoteIP, &amakerbot_service, resp))
    {
        udp_service.sendReply(resp, remoteIP, remotePort);
        return true;   // claimed — error reply sent
    }

    // ... handle command ...
    return true;
}
```

---

## IsMasterRegistryInterface

`IsUDPMessageHandlerInterface` inherits the two pure-virtual methods of `IsMasterRegistryInterface`:

```cpp
virtual bool        isMaster(const std::string &ip) const = 0;
virtual std::string getMasterIP() const = 0;
```

When your service **also** extends `IsOpenAPIInterface`, those defaults (always return `false` / `""`) are already provided and you don't need to implement them.

If your service **only** extends `IsUDPMessageHandlerInterface` (no `IsOpenAPIInterface`), you must implement them yourself — or delegate:

```cpp
bool        isMaster(const std::string &ip) const override { return false; }
std::string getMasterIP()                   const override { return ""; }
```

---

## Registration in `main.cpp`

Services are wired into the UDP dispatch chain via the `udp_aware_services[]` array in `setup()`:

```cpp
// main.cpp
IsServiceInterface *udp_aware_services[] = {
    &servo_service,
    &k10sensors_service,
    &board_info_service,
    &music_service,
    &amakerbot_service,
    // add your service here
};

for (auto *svc : udp_aware_services) {
    IsUDPMessageHandlerInterface *h = svc->asUDPMessageHandlerInterface();
    if (h) {
        udp_service.registerMessageHandler(
            [h](const std::string &msg, const IPAddress &ip, uint16_t port) {
                return h->messageHandler(msg, ip, port);
            });
    }
}
```

**Handler order matters**: the first handler that returns `true` consumes the packet. Place more-specific handlers (binary single-byte protocols) before broad text-prefix handlers.

---

## Complete Implementation Example

```cpp
// include/services/MyService.h
#pragma once
#include "IsOpenAPIInterface.h"
#include "isUDPMessageHandlerInterface.h"

class MyService : public IsOpenAPIInterface, public IsUDPMessageHandlerInterface
{
public:
    std::string getServiceName()    override;
    std::string getServiceSubPath() override;
    bool initializeService()        override;
    bool startService()             override;
    bool stopService()              override;
    bool registerRoutes()           override;

    bool messageHandler(const std::string &message,
                        const IPAddress   &remoteIP,
                        uint16_t           remotePort) override;

    IsUDPMessageHandlerInterface *asUDPMessageHandlerInterface() override { return this; }
};
```

```cpp
// src/services/implementations/MyService.cpp
#include "services/MyService.h"
#include "services/UDPService.h"
#include "FlashStringHelper.h"
#include <ArduinoJson.h>

extern UDPService udp_service;
extern AmakerBotService amakerbot_service;  // for master checks

namespace MyServiceConsts {
    constexpr const char str_service_name[] PROGMEM = "My Service";
    constexpr const char path_service[]     PROGMEM = "myservice/v1";
    constexpr const char cmd_do_thing[]     PROGMEM = "doThing";
}

// ── Text-protocol message handler ────────────────────────────────────────────
bool MyService::messageHandler(const std::string &message,
                                const IPAddress   &remoteIP,
                                uint16_t           remotePort)
{
    // Text protocol: "My Service:<cmd>[:<json>]"
    const std::string prefix = "My Service:";
    if (message.rfind(prefix, 0) != 0) return false;  // not our message

    // Only the registered master may command this service
    std::string resp;
    if (!checkUDPIsMaster(0x00, remoteIP, &amakerbot_service, resp))
    {
        udp_service.sendReply(resp, remoteIP, remotePort);
        return true;
    }

    // Parse command
    std::string rest = message.substr(prefix.size());
    size_t sep = rest.find(':');
    std::string cmd = (sep != std::string::npos) ? rest.substr(0, sep) : rest;

    if (!isServiceStarted())
    {
        // JSON error reply
        JsonDocument doc;
        doc["result"]  = "error";
        doc["message"] = "Service not started";
        String out;
        serializeJson(doc, out);
        udp_service.sendReply(std::string(out.c_str()), remoteIP, remotePort);
        return true;
    }

    JsonDocument req;
    if (sep != std::string::npos)
        deserializeJson(req, rest.substr(sep + 1));

    JsonDocument respDoc;
    if (cmd == progmem_to_string(MyServiceConsts::cmd_do_thing))
    {
        // ... do the thing ...
        respDoc["result"]  = "ok";
        respDoc["message"] = "done";
    }
    else
    {
        respDoc["result"]  = "error";
        respDoc["message"] = "unknown command";
    }

    String out;
    serializeJson(respDoc, out);
    udp_service.sendReply(std::string(out.c_str()), remoteIP, remotePort);
    return true;
}
```

---

## Best Practices

### DO:
- ✅ Return `false` only when the message prefix/action byte does not belong to your service
- ✅ Return `true` whenever you send any reply (success or error)
- ✅ Use `checkUDPIsMaster()` for commands restricted to the master
- ✅ Re-use static `JsonDocument` objects for hot UDP paths (avoids heap fragmentation)
- ✅ Use `message[0]` for binary dispatch, not string comparisons on null-containing payloads
- ✅ Guard with `isServiceStarted()` before executing commands

### DON'T:
- ❌ Block inside `messageHandler()` — it runs on the UDP task (Core 0)
- ❌ Use `Serial.print()` — use `logger->debug()` inside `#ifdef VERBOSE_DEBUG`
- ❌ Allocate heap in the hot path (ISR/UDP task)
- ❌ Forget to add your service to `udp_aware_services[]` in `main.cpp`

---

## Related Documentation

- [IsOpenAPIInterface.md](IsOpenAPIInterface.md) — HTTP route registration
- [IsServiceInterface.md](IsServiceInterface.md) — Service lifecycle
- [UDPServiceHandlers.md](UDPServiceHandlers.md) — Lambda-based handler registration
- [user guides/UDPMessages.md](../user%20guides/UDPMessages.md) — Full UDP message reference
