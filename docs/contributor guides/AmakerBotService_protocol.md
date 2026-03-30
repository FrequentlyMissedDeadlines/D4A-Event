# AmakerBotService — Binary Protocol Reference

Source of truth: `src/services/implementations/amakerbot/AmakerBotService.cpp`

---

## Service Identity

| Field | Value |
|---|---|
| Service ID (high nibble) | `0x04` |
| Action byte formula | `(service_id << 4) \| low_nibble` → `0x4N` |

---

## UDPResponseStatus

Appended as a single byte at the end of every standard reply (see **Reply Format** below).
Numeric values are defined outside this class; only symbolic names are used here.

| Symbol | Meaning |
|---|---|
| `SUCCESS` | Operation succeeded |
| `IGNORED` | Request was valid but skipped (e.g. already master, no token provided) |
| `DENIED` | Caller is not authorised (wrong token, not master) |
| `ERROR` | Operation failed (bad params, hardware error) |

---

## Standard Reply Format (`udp_reply`)

Unless stated otherwise every response is built by `udp_reply()`:

```
[echo of full incoming message bytes] + [UDPResponseStatus byte]
```

Example for `0x41` with 5-char token `"abc12"` on success:
```
TX: 41 61 62 63 31 32          (0x41 + "abc12")
RX: 41 61 62 63 31 32 <STATUS> (echo + status byte)
```

---

## Commands

### `0x41` — MASTER_REGISTER

| Field | Detail |
|---|---|
| Byte 0 | `0x41` |
| Bytes 1…N | Token string (5 ASCII chars) |
| Auth required | No (token validates identity) |
| Fire-and-forget | **No — response always sent** |

**Server logic:**

| Condition | Response status |
|---|---|
| Token field empty (message length = 1) | `IGNORED` |
| Token present but does not match server token | `DENIED` |
| Token valid, `registerMaster()` succeeds | `SUCCESS` |
| Token valid, `registerMaster()` returns false (already has a different master, or IP already master) | `IGNORED` |

**Response format:** standard `udp_reply` (echo + status).

---

### `0x42` — MASTER_UNREGISTER

| Field | Detail |
|---|---|
| Byte 0 | `0x42` |
| Payload | None |
| Auth required | Yes — caller IP must be the registered master |
| Fire-and-forget | **No — response always sent** |

**Server logic:**

| Condition | Response status |
|---|---|
| Caller is not the master | `DENIED` |
| `unregisterMaster()` succeeds | `SUCCESS` |
| `unregisterMaster()` returns false | `ERROR` |

**Response format:** standard `udp_reply` (echo + status).

---

### `0x43` — HEARTBEAT

| Field | Detail |
|---|---|
| Byte 0 | `0x43` |
| Payload | None |
| Auth required | Yes — caller IP must be the registered master |
| Fire-and-forget | **Yes (when authorised) — no reply sent on success** |

**Server logic:**

| Condition | Response |
|---|---|
| Caller is not the master | `DENIED` reply sent via `udp_reply` |
| Caller is the master | **No reply.** Updates `last_heartbeat_ms_`, sets `heartbeat_active_ = true`, clears `heartbeat_timed_out_` |

**Watchdog behaviour** (checked periodically via `checkHeartbeatTimeout()`):
- Timeout threshold: **50 ms** (`heartbeat_timeout_ms`)
- On timeout transition (`heartbeat_timed_out_` was `false`): calls `servo_service.setAllMotorsSpeed(0)` and `servo_service.setAllServoSpeed(0)`, sets `heartbeat_timed_out_ = true` (fires only once per timeout event)
- On recovery (heartbeat arrives after timeout): clears `heartbeat_timed_out_`

---

### `0x44` — PING

| Field | Detail |
|---|---|
| Byte 0 | `0x44` |
| Bytes 1–4 | 4-byte client-generated ID |
| Minimum message size | 5 bytes |
| Auth required | Yes — caller IP must be the registered master |
| Fire-and-forget | **No — echo response always sent (when authorised)** |

**Server logic:**

| Condition | Response |
|---|---|
| Caller is not the master | `return false` — message is **not claimed**, no reply |
| Caller is master AND `message.size() >= 5` | Echoes `message.substr(0, 5)` — i.e. `[0x44][4-byte ID]` |
| Caller is master AND `message.size() < 5` | No reply (message silently consumed) |

**Response format:** custom — NOT `udp_reply`.
```
RX: 44 <ID byte 0> <ID byte 1> <ID byte 2> <ID byte 3>
```
The 4-byte ID is echoed verbatim so the client can match it to its original send and compute RTT.

---

### `0x45` — GET_NAME

| Field | Detail |
|---|---|
| Byte 0 | `0x45` |
| Payload | None |
| Auth required | **No — available to any caller** |
| Fire-and-forget | **No — response always sent** |

**Server logic:** Always replies.

**Response format:** custom — NOT `udp_reply`.
```
RX: 45 <bot name as ASCII bytes>
```
No length prefix; name ends at the packet boundary. Default name: `"K10-Bot"`.

---

### `0x46` — SET_NAME

| Field | Detail |
|---|---|
| Byte 0 | `0x46` |
| Bytes 1…N | New name (1–32 ASCII chars) |
| Auth required | Yes — caller IP must be the registered master |
| Fire-and-forget | **No — response always sent** |

**Server logic:**

| Condition | Response status |
|---|---|
| Caller is not the master | `DENIED` |
| Name empty OR name length > 32 | `ERROR` |
| Name valid, `setBotName()` called | `SUCCESS` |

**Response format:** standard `udp_reply` (echo + status).

---

### `0x47` — GET_WIFI_SETTINGS

| Field | Detail |
|---|---|
| Byte 0 | `0x47` |
| Payload | None |
| Auth required | **No — available to any caller** |
| Fire-and-forget | **No — response always sent** |

**Server logic:** Reads current in-memory STA credentials from the injected `WifiService`.

| Condition | Response |
|---|---|
| `WifiService` not injected | `[0x47][resp_not_started]` |
| Always otherwise | `[0x47][resp_ok][ssid_len:1B][ssid…][pass_len:1B][pass…]` |

**Response format:** custom — NOT `udp_reply`.
```
RX: 47 00 <ssid_len> <ssid bytes…> <pass_len> <pass bytes…>
```

---

### `0x48` — SET_WIFI_SETTINGS

| Field | Detail |
|---|---|
| Byte 0 | `0x48` |
| Bytes 1 | `ssid_len` (1 byte) |
| Bytes 2…(1+ssid_len) | SSID string |
| Byte (2+ssid_len) | `pass_len` (1 byte) |
| Bytes … | Password string |
| Auth required | **Yes — caller IP must be the registered master** |
| Fire-and-forget | **No — response always sent** |

**Constraints:** SSID 1–32 chars; password 0–64 chars.

**Server logic:** Calls `WifiService::setWifiCredentials()` then `WifiService::saveSettings()` (persists to NVS).

| Condition | Response status |
|---|---|
| Caller is not the master | `resp_not_master` |
| `WifiService` not injected | `resp_not_started` |
| Payload too short | `resp_invalid_params` |
| SSID empty / SSID > 32 / pass > 64 | `resp_invalid_values` |
| Credentials saved | `resp_ok` |

**Response format:** standard `[action][resp_code]`.

---

### `0x49` — RESET_WIFI_SETTINGS

| Field | Detail |
|---|---|
| Byte 0 | `0x49` |
| Payload | None |
| Auth required | **Yes — caller IP must be the registered master** |
| Fire-and-forget | **No — response always sent** |

**Server logic:** Calls `WifiService::resetSettings()` which clears the NVS `"wifi"` namespace and restores compile-time defaults in memory.

| Condition | Response status |
|---|---|
| Caller is not the master | `resp_not_master` |
| `WifiService` not injected | `resp_not_started` |
| Reset succeeded | `resp_ok` |

**Response format:** standard `[action][resp_code]`.

---

## Summary Table

| Code | Name | Auth | Payload (TX) | Response (RX) | Fire-and-forget |
|---|---|---|---|---|---|
| `0x41` | MASTER_REGISTER | Token | `[0x41][5-char token]` | `[echo][STATUS]` | No |
| `0x42` | MASTER_UNREGISTER | Master IP | `[0x42]` | `[echo][STATUS]` | No |
| `0x43` | HEARTBEAT | Master IP | `[0x43]` | None (if master). `[echo][DENIED]` if not master | **Yes** (when authorised) |
| `0x44` | PING | Master IP | `[0x44][4B id]` | `[0x44][4B id]` echo | No |
| `0x45` | GET_NAME | None | `[0x45]` | `[0x45][name bytes]` | No |
| `0x46` | SET_NAME | Master IP | `[0x46][name bytes]` | `[echo][STATUS]` | No |
| `0x47` | GET_WIFI_SETTINGS | None | `[0x47]` | `[0x47][resp_ok][ssid_len][ssid…][pass_len][pass…]` | No |
| `0x48` | SET_WIFI_SETTINGS | Master IP | `[0x48][ssid_len][ssid…][pass_len][pass…]` | `[0x48][resp_code]` | No |
| `0x49` | RESET_WIFI_SETTINGS | Master IP | `[0x49]` | `[0x49][resp_code]` | No |

---

## Notes for Implementors

- **Token** is 5 characters, generated once at service init, logged to the device screen (`app_info_logger`). It does not change until the device reboots.
- **Master** is exclusively IP-based. Only one master at a time is permitted.
- **`0x43` heartbeat must arrivBOT_SERVICE_IDe within 50 ms** when a master is registered and `heartbeat_active_` is true. The first heartbeat after registration activates the watchdog; before that, no timeout fires.
- **`0x44` ping** is not claimed (`return false`) when the caller is not the master. This differs from all other commands which return `true` and send `DENIED`.
- **`0x45` get name** is the only command with no auth check.
- **`0x47` get WiFi settings** is also open to unauthenticated callers (read-only, display-level info).
- **`0x48` set WiFi settings** writes credentials to NVS immediately via `WifiService::saveSettings()`. The device does **not** reconnect automatically — the user must reboot or call a separate reconnect action.
- **`0x49` reset WiFi settings** clears the NVS `"wifi"` namespace via `Preferences::clear()` and restores compile-time defaults in memory. Credentials are not re-applied until the next reboot or explicit `applySettings()` call.
- **WiFi commands require `setWifiService()`** to be called on `AmakerBotService` at startup (done in `main.cpp`). If the pointer is null, all three commands return `resp_not_started`.
- The `udp_reply` helper always **prepends the full incoming message** before the status byte. For short commands like `0x42` or `0x43` the echo is just the single action byte.
