# Remote UI Control via Bot Protocol

The K10 Bot's TFT display can be controlled remotely using the bot protocol. This allows you to navigate between screens (splash, app info, and log screens) from a remote master controller.

> **📌 New in Latest Version:** Use the `getBotControl(ip, port, token)` function for easy programmatic bot control. See [JavaScript User Guide](javascript_user_guide.html) for examples.

## Bot Service ID
- **Service ID**: `0x05` (UI Service)
- **Transport**: UDP, WebSocket, or HTTP `/botserver` endpoint

## Commands

### Next Screen (0x01)
Advance to the next screen (wraps from last screen back to first).

**Request Format:**
```
[action=0x51] [no payload]
```

**Response:**
```
[0x51] [resp_ok=0x00]
```

**Example (curl via HTTP):**
```bash
# Hex: 51 (action byte only)
curl "http://<device-ip>/botserver?cmd=51"
```

### Previous Screen (0x02)
Go back to the previous screen (wraps from first screen to last).

**Request Format:**
```
[action=0x52] [no payload]
```

**Response:**
```
[0x52] [resp_ok=0x00]
```

**Example (curl via HTTP):**
```bash
# Hex: 52
curl "http://<device-ip>/botserver?cmd=52"
```

### Set Screen (0x03)
Jump directly to a specific screen by index.

**Request Format:**
```
[action=0x53] [screen_index:1B]
```

Where `screen_index` is:
- `0` = Splash Screen
- `1` = App Info / Dashboard
- `2` = App Log
- `3` = Service Log
- `4` = Debug Log
- `5` = ESP-IDF Log

**Response:**
```
[0x53] [resp_ok=0x00 or resp_invalid_values=0x02]
```

**Example (curl via HTTP):**
```bash
# Jump to App Info (screen 1): Hex: 53 01
curl "http://<device-ip>/botserver?cmd=5301"

# Jump to Debug Log (screen 4): Hex: 53 04
curl "http://<device-ip>/botserver?cmd=5304"
```

## Python Example

```python
import socket
import struct

def send_ui_command(device_ip, command):
    """Send a UI control command via UDP."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(bytes([command]), (device_ip, 81))
    sock.close()

def set_screen(device_ip, screen_index):
    """Set the display to a specific screen."""
    action_byte = 0x53
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(bytes([action_byte, screen_index]), (device_ip, 81))
    sock.close()

# Example usage
device_ip = "192.168.1.100"

# Go to next screen
send_ui_command(device_ip, 0x51)

# Go to previous screen
send_ui_command(device_ip, 0x52)

# Jump to App Info (screen 1)
set_screen(device_ip, 1)

# Jump to Debug Log (screen 4)
set_screen(device_ip, 4)
```

## JavaScript Example (WebSocket)

```javascript
// Assuming ws_conn is an established WebSocket connection
function nextScreen() {
    const action = 0x51;
    const frame = new Uint8Array([action]);
    ws_conn.send(frame);
}

function previousScreen() {
    const action = 0x52;
    const frame = new Uint8Array([action]);
    ws_conn.send(frame);
}

function setScreen(screenIndex) {
    const action = 0x53;
    const frame = new Uint8Array([action, screenIndex]);
    ws_conn.send(frame);
}

// Examples
nextScreen();              // Go to next screen
previousScreen();          // Go to previous screen
setScreen(4);              // Jump to Debug Log (screen 4)
```

## Response Codes

| Code | Meaning |
|------|---------|
| `0x00` | OK — command executed successfully |
| `0x01` | Invalid parameters — malformed request |
| `0x02` | Invalid values — screen index out of range |
| `0x05` | Unknown command — unrecognized command byte |

## Integration with BotScript

If you're running a BotScript, you can control the UI from your custom functions:

```javascript
// In your BotScript actions file or HTML page
async function nextScreenViaBot() {
    const action = 0x51;
    const frame = new Uint8Array([action]);
    // Send via your bot protocol transport (UDP, WebSocket, or HTTP)
    await fetch('http://<device-ip>/botserver?cmd=51');
}
```

## Notes

- The UI service always responds with an empty payload (only action + response code)
- Screen navigation does NOT require master registration — it's available to all clients
- The button A on the K10 board continues to work alongside remote control
- Remote commands and button presses are processed asynchronously in the display task
