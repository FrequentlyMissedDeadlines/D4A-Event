# WebSocket UDP Bridge User Guide

## Overview

The K10 bot provides a **WebSocket bridge** that allows your HTML/JavaScript web interface to send UDP commands directly to the robot in real-time. This guide explains how to use this bridge to control servos, read sensors, and manage the bot's state.

## What is the WebSocket Bridge?

The WebSocket bridge is an HTTP server feature that:
- **Accepts WebSocket connections** from your browser at `/ws` endpoint
- **Forwards UDP commands** from JavaScript to the bot's UDP service
- **Routes responses back** to the browser via WebSocket
- **Handles real-time communication** without HTTP request/response overhead

### Why WebSocket Instead of HTTP?

| Feature | HTTP | WebSocket |
|---------|------|-----------|
| **Connection** | Request-response | Persistent |
| **Latency** | ~100ms per command | <10ms |
| **Real-time updates** | Polling only | Instant |
| **Overhead** | High (headers each request) | Low (binary frames) |
| **Hardware ideal for** | Periodic checks | Real-time control |

---

## Architecture Overview

```
┌───────────────────────────────────────────────┐
│                 Browser (HTML/JS)             │
│                                               │
│  ┌─────────────────────────────────────────┐  │
│  │        WebSocket Connection (Binary)    │  │
│  │         ws://robot-ip/ws                │  │
│  └───────────────┬─────────────────────────┘  │
└──────────────────┼────────────────────────────┘
                   │
                   │ (Binary frames)
                   │
┌──────────────────┼──────────────────────────────────────┐
│                  │  aMaker bot                          │
│           ┌──────▼─────────┐                            │
│           │  HTTP Service  │                            │
│           │ (WebSocket /ws)│                            │
│           └──────┬─────────┘                            │
│                  │                                      │
│           ┌──────▼─────────┐                            │
│           │  UDP Service   │                            │
│           │ (Port 24642)   │◄─── Handles bot commands   │
│           └────────────────┘                            │
│                  │                                      │
│           ┌──────▼─────────────┐                        │
│           │  Servo/Sensor I/O  │                        │
│           │  Motor control     │                        │
│           └────────────────────┘                        │
└─────────────────────────────────────────────────────────┘
```

---

## Getting Started with demo page

### 1. Connect to the Robot

Open your browser and navigate to:
```
http://<robot-ip>:80
```

Replace `<robot-ip>` with your robot's IP address (e.g., `192.168.4.1`).

### 2. Register as Master

Before sending commands, you **must register as master**:

1. Enter your **Master Token** in the text field
2. Click **"Register as Master"**
3. You should see: `"Successfully registered as master"` ✅

**Why?** The bot requires master authentication to prevent unauthorized control.

### 3. Send Your First Command

Once registered:
1. Click any servo control button (e.g., **"Set Angles"**)
2. Adjust sliders to desired positions
3. Click the action button (e.g., **"Apply"**)
4. Monitor the status bar for success/error messages

---

## HTML Interface Guide

### Main Sections

The HTML interface (`UDPService.html`) provides these control panels:

#### **Master Registration Panel**
```
┌─ Master Registration ──────────────────┐
│                                        │
│ Master Token: [________________]       │
│                                        │
│  [Register as Master]  [Unregister]    │
│                                        │
│ Status: Ready                          │
└────────────────────────────────────────┘
```

- **Master Token**: Your authorization token (check robot docs)
- **Register**: Becomes master (gain control)
- **Unregister**: Release master status (allow others to control)

#### **Servo Control Panel** 
```
┌─ Servo Control ───────────────────┐
│                                   │
│ Angular Servos (0-3):             │
│  ☐ Ch0: [-90°────●────+90°] 45°   │
│  ☐ Ch1: [-90°────●────+90°] 0°    │
│  ☐ Ch2: [-90°────●────+90°] -30°  │
│  ☐ Ch3: [-90°────●────+90°] 60°   │
│                                   │
│  [Center All]  [Set Angles]       │
│                                   │
│ Rotational Servos (4-7):          │
│  Ch4: [←──────●──────→] 0 RPM     │
│  Ch5: [←──────●──────→] 50 RPM    │
│  Ch6: [←──────●──────→] -75 RPM   │
│  Ch7: [←──────●──────→] 0 RPM     │
│                                   │
│  [Set Speeds]  [Stop All]         │
│                                   │
└───────────────────────────────────┘
```

- **Angular Servos** (0-3): Position control (-90° to +90°)
  - Check the box to include in next command
  - Adjust slider to desired angle
  - Click "Set Angles" to apply

- **Rotational Servos** (4-7): Speed control
  - Adjust speed slider (-127 to +127)
  - Click "Set Speeds" to apply

#### **Heartbeat Panel**
```
┌─ Heartbeat ───────────────────┐
│                               │
│ Sends 40ms keep-alive pulses  │
│ to maintain master status     │
│                               │
│  [Start Heartbeat]            │
│  [Stop Heartbeat]             │
│                               │
│ Status: Running               │
└───────────────────────────────┘
```

#### **Ping/Latency Panel**
```
┌─ Ping Test ────────────────────┐
│                                │
│ Current: 12ms                  │
│ Min: 8ms, Max: 45ms            │
│ Avg: 15ms                      │
│                                │
│ [Ping Bot]                     │
│                                │
│ Real-time latency graph...     │
└────────────────────────────────┘
```

---

## JavaScript API Reference

### Connection Management

#### `initializeWebSocket()`
Automatically called on page load. Establishes WebSocket connection.

```javascript
// Manual reconnect if needed
initializeWebSocket();
```

#### `ws` (global variable)
The WebSocket connection object. Check connection status:

```javascript
if (ws && ws.readyState === WebSocket.OPEN) {
  console.log('Connected to bridge');
}
```

### Sending Commands

#### `sendUDPPacket(packet)`
Send a UDP command packet to the robot.

```javascript
const packet = new Uint8Array([0x41, 0x03]); // Example: register as master
const response = await sendUDPPacket(packet);
console.log(response); // Uint8Array with response data
```

**Parameters:**
- `packet` (Uint8Array): Binary command packet

**Returns:**
- Promise that resolves with response Uint8Array
- Rejects if timeout (1000ms) or WebSocket disconnected

**Example: Set servo angle**
```javascript
async function setAngle(channel, angle) {
  const packet = new Uint8Array(3);
  packet[0] = 0x21;              // SET_SERVO_ANGLE command
  packet[1] = channel;           // Channel 0-7
  packet[2] = angle & 0xFF;      // Angle value
  
  try {
    const response = await sendUDPPacket(packet);
    console.log('Angle set successfully');
  } catch (error) {
    console.error('Failed to set angle:', error);
  }
}

setAngle(0, 45); // Set channel 0 to 45°
```

### High-Level Control Functions

#### `setServoAngles()`
Set positions of angular servos (channels 0-3).

**How to use:**
1. Adjust sliders for channels 0-3
2. Check the channels you want to include
3. Call `setServoAngles()`

```javascript
// JavaScript API (for custom code)
async function setServoAngles() {
  // Automatically reads UI sliders
  // Sends 0x21 command with selected channels
}
```

#### `setServoSpeeds()`
Set speeds of rotational servos (channels 4-7).

```javascript
// Automatically reads UI sliders for speed4-7
// Sends 0x22 command with speed values
```

#### `stopAllServos()`
Stop all servo motion immediately.

```javascript
// Sends 0x23 STOP_SERVOS command
await stopAllServos();
```

#### `centerAllAngles()`
Reset all angular servos to 0°.

```javascript
// Resets angle0-3 sliders to 0
// Call setServoAngles() to apply
centerAllAngles();
```

#### `registerAsMaster(token)`
Register as master with authentication token.

```javascript
try {
  await registerAsMaster('your-master-token');
  console.log('Master registered!');
} catch (error) {
  console.error('Registration failed:', error);
}
```

#### `unregisterMaster()`
Release master status.

```javascript
await unregisterMaster();
```

#### `sendHeartbeat()`
Send keep-alive pulse (automatic if heartbeat enabled).

```javascript
// Sends 0x43 HEARTBEAT command
// Usually called automatically by startHeartbeat()
```

---

## UDP Command Reference

The WebSocket bridge forwards these UDP commands to the robot:

### Master Control Commands

| Command | Code | Payload | Response |
|---------|------|---------|----------|
| **Register Master** | 0x41 | `[token_4_bytes]` | `[0x41, status_code]` |
| **Unregister Master** | 0x42 | None | `[0x42, status_code]` |
| **Heartbeat** | 0x43 | None | `[0x43, status_code]` |
| **Ping** | 0x44 | `[seq_byte]` | `[0x44, seq_byte, latency_ms]` |

### Servo Control Commands

| Command | Code | Payload | Response |
|---------|------|---------|----------|
| **Set Servo Angles** | 0x21 | `[ch0, angle0, ch1, angle1, ...]` | `[0x21, status_code]` |
| **Set Servo Speeds** | 0x22 | `[mask_byte, speed0, speed1, ...]` | `[0x22, status_code]` |
| **Stop Servos** | 0x23 | None | `[0x23, status_code]` |

### Status Codes

| Code | Meaning | Action |
|------|---------|--------|
| `0x00` | SUCCESS | Command executed |
| `0x01` | IGNORED | Not master; command ignored |
| `0x02` | DENIED | Permission denied |
| `0x03` | ERROR | Invalid parameters |

---

## Complete Workflow Examples

### Example 1: Simple Angle Control

```javascript
async function moveServoToCenter() {
  // Ensure registered as master
  if (!isMasterRegistered) {
    console.log('Not master! Register first.');
    return;
  }
  
  // Create angle command: channel 0 to 0°
  const packet = new Uint8Array([
    0x21,        // SET_SERVO_ANGLE
    0, 0,        // Channel 0, angle 0°
    1, 0,        // Channel 1, angle 0°
    2, 0,        // Channel 2, angle 0°
    3, 0         // Channel 3, angle 0°
  ]);
  
  try {
    const response = await sendUDPPacket(packet);
    if (response[1] === 0x00) {
      console.log('✓ Servos centered');
    } else {
      console.error('✗ Failed: code 0x' + response[1].toString(16));
    }
  } catch (error) {
    console.error('Timeout or connection error:', error);
  }
}

moveServoToCenter();
```

### Example 2: Continuous Speed Control

```javascript
async function spinServo(channel, speed) {
  // Create speed command
  const mask = 1 << (channel - 4); // Servo 4-7 use mask bits
  const packet = new Uint8Array([
    0x22,        // SET_SERVO_SPEED
    mask,        // Which servos to update
    speed & 0xFF // Speed (-127 to +127)
  ]);
  
  const response = await sendUDPPacket(packet);
  return response[1] === 0x00; // True if success
}

// Spin servo 4 at 50 RPM
await spinServo(4, 50);

// Spin servo 5 backwards at 75 RPM
await spinServo(5, -75);

// Stop all
await stopAllServos();
```

### Example 3: Automated Sequence

```javascript
async function danceSequence() {
  if (!isMasterRegistered) {
    console.log('Master registration required');
    return;
  }
  
  try {
    // Step 1: Move to position A
    let packet = new Uint8Array([0x21, 0, 45, 1, -45, 2, 90, 3, -90]);
    await sendUDPPacket(packet);
    await sleep(500); // Wait 500ms
    
    // Step 2: Move to position B
    packet = new Uint8Array([0x21, 0, -45, 1, 45, 2, -90, 3, 90]);
    await sendUDPPacket(packet);
    await sleep(500);
    
    // Step 3: Return to center
    centerAllAngles();
    await setServoAngles();
    
    console.log('Dance complete!');
  } catch (error) {
    console.error('Dance failed:', error);
  }
}

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

danceSequence();
```

---

## Troubleshooting

### Problem: "WebSocket bridge not connected"

**Solution:**
1. Check your robot IP in the browser address bar
2. Ensure the HTTP server is running on the robot
3. Check browser console (F12) for connection errors
4. Try refreshing the page

```javascript
// Check connection status
console.log(ws.readyState);
// 0 = CONNECTING, 1 = OPEN, 2 = CLOSING, 3 = CLOSED
```

### Problem: Commands accepted but servos don't move

**Causes & Solutions:**
1. **Not registered as master**
   - Status bar will show "DENIED"
   - Solution: Enter correct master token and register

2. **Servo not enabled**
   - For angles: checkbox must be checked
   - Solution: Check the servo channel checkbox before sending

3. **Servo out of range**
   - Angles: -90° to +90°
   - Speeds: -127 to +127
   - Solution: Use valid values

### Problem: High latency / timeouts

**Diagnostics:**
- Use the **Ping panel** to measure latency
- Look for WiFi interference
- Check robot's WiFi signal strength

**Solutions:**
1. Move robot closer to WiFi router
2. Reduce interference (2.4 GHz band crowded)
3. Check for heavy network traffic
4. Verify heartbeat is enabled (maintains connection)

### Problem: Intermittent command failures

**Common causes:**
1. **Lost master status**
   - Heartbeat stopped or failed
   - Solution: Auto-restart heartbeat in `stopService()`

2. **WebSocket connection dropped**
   - Robot rebooted or WiFi disconnected
   - Solution: Page auto-reconnects after 3 seconds

3. **Packet loss**
   - Heavy network traffic
   - Solution: Retry failed commands (timeout = 1000ms)

**Enable debug logging:**
```javascript
// Open browser DevTools (F12)
// All WebSocket traffic logged to console
// Look for [WS] prefix messages
```

---

## Advanced Topics

### Custom UDP Handlers

If you need commands beyond servo control, you can:

1. **Add new command handler** in C++ (UDPService)
2. **Send from JavaScript:**

```javascript
const customPacket = new Uint8Array([
  0xAB,           // Your command code
  0x01, 0x02,     // Custom payload
  0x03, 0x04
]);

const response = await sendUDPPacket(customPacket);
```

### Binary Packet Format

All WebSocket frames are **binary**:
- Byte 0: Command code (0x21, 0x22, 0x23, etc.)
- Bytes 1+: Command-specific payload
- Response includes status byte at index 1

**Example packet builder:**
```javascript
function buildServoPacket(commands) {
  const packet = new Uint8Array(1 + commands.length * 2);
  packet[0] = 0x21; // SET_SERVO_ANGLE
  
  for (let i = 0; i < commands.length; i++) {
    packet[1 + i * 2] = commands[i].channel;
    packet[2 + i * 2] = commands[i].angle;
  }
  
  return packet;
}

const cmds = [
  {channel: 0, angle: 45},
  {channel: 1, angle: -30}
];

sendUDPPacket(buildServoPacket(cmds));
```

### Performance Optimization

- **Batch commands:** Send multiple servos in one packet vs. multiple packets
- **Disable polling:** Turn off heartbeat if not needed (reduces traffic)
- **Response tracking:** `sendUDPPacket()` automatically times out at 1000ms

---

## Reference

### File Locations
- **HTML UI:** `/data/UDPService.html`
- **JavaScript:** `/data/UDPService.js`
- **C++ Backend:** `src/services/HTTPService.cpp`, `src/services/UDPService.cpp`

### Key Variables (JavaScript)
```javascript
ws                 // WebSocket connection
wsConnected        // Boolean: connection status
isMasterRegistered // Boolean: master status
pingHistory        // Array: latency samples
heartbeatInterval  // Timer ID: keep-alive
```

### Settings Persistence
- Master token, servo angles, and ping history are **NOT saved** between sessions
- Re-enter token and reconfigure on page reload

---

## Support & Further Reading

For more information:
- **WiFi Setup:** See [WiFiService.md](WiFiService.md)
- **UDP Protocol Details:** See [UDP_guide.md](../UDP_guide.md)
- **Hardware API:** See contributor guides in `/docs/contributor guides/`


