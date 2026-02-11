# K10 Bot LLM Integration Guide

## Purpose
This document provides AI/LLM engines with essential information to programmatically interact with the K10 Bot. It covers all service endpoints, UDP command protocols, data formats, and common integration patterns.

## Quick Start

### Device Connection
1. **Network Discovery**: Device IP typically obtained via WiFi connection
2. **Access Point Mode**: Connect to `aMaker-XXXXXX` if device not configured
3. **Base URL**: All HTTP endpoints use `http://<device-ip>/api/`
4. **UDP Port**: Default UDP listener on port `24642`

### Authentication
- All endpoints: **No authentication required**
- Protected endpoints: Bearer token (check individual endpoint docs)

---

## HTTP API Reference

### Content-Type
- **Request**: `application/json` for POST/PUT bodies
- **Response**: `application/json` (except webcam snapshots: `image/jpeg`)

### Standard Response Format

**Success:**
```json
{
  "result": "ok",
  "message": "<operation_name>",
  "data": { /* optional payload */ }
}
```

**Error:**
```json
{
  "result": "error",
  "message": "<error_description>",
  "error": "<technical_details>"
}
```

### HTTP Status Codes
| Code | Meaning |
|------|---------|
| 200 | Success |
| 400 | Invalid request format |
| 422 | Missing/invalid parameters |
| 456 | Device-specific operation failed |
| 500 | Internal server error |
| 503 | Service not initialized |

---

## Service Endpoints

### 1. Board Info Service

#### `GET /api/board/v1`
Get system metrics, memory, chip info, firmware version.

**Response Example:**
```json
{
  "uptimeMs": 123456,
  "board": "UNIHIKER_K10",
  "version": "1.0.0",
  "heapTotal": 327680,
  "heapFree": 280000,
  "freeStackBytes": 2048,
  "chipCores": 2,
  "chipModel": "ESP32-S3",
  "chipRevision": 1,
  "cpuFreqMHz": 240,
  "freeSketchSpace": 1310720,
  "sdkVersion": "v4.4.2"
}
```

**Use Cases:**
- Health monitoring
- Memory diagnostics
- Uptime tracking
- Firmware version verification

---

### 2. K10 Sensors Service

#### `GET /api/sensors/v1`
Read all onboard sensors in one call.

**Response Example:**
```json
{
  "light": 125.5,
  "hum_rel": 45.2,
  "celcius": 23.8,
  "mic_data": 512,
  "accelerometer": [0.12, -0.05, 9.81]
}
```

**Sensor Details:**
- `light`: Ambient light level (arbitrary units)
- `hum_rel`: Relative humidity (%)
- `celcius`: Temperature (°C)
- `mic_data`: Microphone ADC value (0-1023)
- `accelerometer`: [X, Y, Z] in m/s² (gravity units)

**Error Response (503):**
```json
{
  "error": "Sensor initialization failed"
}
```

**Polling Recommendations:**
- Normal monitoring: 1-5 seconds
- Motion detection: 100-500ms
- Real-time control: Use UDP for lower latency

---

### 3. Servo Service (DFR0548 Controller)

**Supports 8 channels (0-7)** with three servo types:
- **Type 1**: Continuous rotation (speed control)
- **Type 2**: Angular 180° (position control)
- **Type 3**: Angular 270° (position control)

#### `POST /api/servos/v1/attachServo`
Register servo type before first use.

**Request:**
```json
{
  "channel": 0,
  "connection": 2
}
```

**Connection Types:**
- `0`: Not connected
- `1`: Continuous rotation
- `2`: Angular 180°
- `3`: Angular 270°

**Response (200):**
```json
{
  "result": "ok",
  "message": "attachServo"
}
```

#### `POST /api/servos/v1/setServoAngle`
Set position for angular servos (types 2, 3).

**Request:**
```json
{
  "channel": 0,
  "angle": 90
}
```

**Angle Ranges:**
- 180° servo: 0-180
- 270° servo: 0-270

**Response Codes:**
- `200`: Success
- `422`: Invalid channel/angle
- `456`: Servo not attached or wrong type
- `503`: Controller not initialized

#### `POST /api/servos/v1/setServoSpeed`
Set speed for continuous servos (type 1).

**Request:**
```json
{
  "channel": 0,
  "speed": 50
}
```

**Speed Range:** -100 to +100
- Negative: Reverse direction
- 0: Stop
- Positive: Forward direction

#### `POST /api/servos/v1/setAllServoAngle`
Set all angular servos to same position.

**Request:**
```json
{
  "angle": 90
}
```

#### `POST /api/servos/v1/setAllServoSpeed`
Set all continuous servos to same speed.

**Request:**
```json
{
  "speed": 0
}
```

#### `POST /api/servos/v1/stopAll`
Emergency stop - sets all servos to speed 0.

**Response:**
```json
{
  "result": "ok",
  "message": "stopAll"
}
```

#### `GET /api/servos/v1/getStatus?channel=0`
Query servo type/connection for specific channel.

**Response:**
```json
{
  "channel": 0,
  "connection": "Angular 180"
}
```

**Connection Strings:**
- `"Not Connected"`
- `"Rotational"`
- `"Angular 180"`
- `"Angular 270"`

#### `GET /api/servos/v1/getAllStatus`
Get status for all 8 channels at once.

**Response:**
```json
{
  "attached_servos": [
    {"channel": 0, "connection": "Angular 180"},
    {"channel": 1, "connection": "Rotational"},
    {"channel": 2, "connection": "Not Connected"}
  ]
}
```

**Workflow Example:**
```python
# 1. Attach servos
requests.post(f"{base_url}/servos/v1/attachServo", 
              json={"channel": 0, "connection": 2})  # 180° servo
requests.post(f"{base_url}/servos/v1/attachServo", 
              json={"channel": 1, "connection": 1})  # Continuous

# 2. Control servos
requests.post(f"{base_url}/servos/v1/setServoAngle", 
              json={"channel": 0, "angle": 45})
requests.post(f"{base_url}/servos/v1/setServoSpeed", 
              json={"channel": 1, "speed": 75})

# 3. Stop when done
requests.post(f"{base_url}/servos/v1/stopAll")
```

---

### 4. DFR1216 Expansion Board Service

**Alternative servo controller** with 6 channels (0-5) and 4 DC motors (1-4).

#### `POST /api/dfr1216/setServoAngle`
Control servo on expansion board.

**Request:**
```json
{
  "channel": 0,
  "angle": 90
}
```

**Channels:** 0-5  
**Angle Range:** 0-180

#### `POST /api/dfr1216/setMotorSpeed`
Control DC motor speed/direction.

**Request:**
```json
{
  "motor": 1,
  "speed": 75
}
```

**Motor Numbers:** 1-4 (1-indexed, not 0-indexed!)  
**Speed Range:** -100 to +100

#### `GET /api/dfr1216/getStatus`
Check initialization status.

**Response:**
```json
{
  "message": "DFR1216Service",
  "status": "started"
}
```

---

### 5. Music Service

#### `POST /api/music/v1/play`
Play built-in melody.

**Request:**
```json
{
  "melody": 8,
  "option": 1
}
```

**Melody IDs (0-19):**
| ID | Name | ID | Name |
|----|------|----|------|
| 0 | DADADADUM | 10 | FUNERAL |
| 1 | ENTERTAINER | 11 | PUNCHLINE |
| 2 | PRELUDE | 12 | BADDY |
| 3 | ODE | 13 | CHASE |
| 4 | NYAN | 14 | BA_DING |
| 5 | RINGTONE | 15 | WAWAWAWAA |
| 6 | FUNK | 16 | JUMP_UP |
| 7 | BLUES | 17 | JUMP_DOWN |
| 8 | BIRTHDAY | 18 | POWER_UP |
| 9 | WEDDING | 19 | POWER_DOWN |

**Playback Options:**
- `1`: Once (blocking)
- `2`: Forever (blocking loop)
- `4`: Once in background
- `8`: Forever in background

**Response (200):**
```json
{
  "result": "ok",
  "message": "Melody started"
}
```

#### `POST /api/music/v1/tone`
Play custom frequency tone.

**Request:**
```json
{
  "freq": 440,
  "beat": 8000
}
```

**Parameters:**
- `freq`: Frequency in Hz (20-20000 recommended)
- `beat`: Duration in milliseconds (default: 8000)

#### `POST /api/music/v1/stop`
Stop current playback.

**Response:**
```json
{
  "result": "ok",
  "message": "Tone stopped"
}
```

#### `GET /api/music/v1/melodies`
Get list of available melodies.

**Response:**
```json
["DADADADUM","ENTERTAINER","PRELUDE","ODE",...]
```

---

### 6. Webcam Service

#### `GET /api/webcam/v1/snapshot`
Capture single JPEG image.

**Response:**
- **Content-Type**: `image/jpeg`
- **Format**: QVGA (320x240)
- **Quality**: 80 (0-100 scale)

**Status Codes:**
- `200`: Image captured successfully
- `500`: Capture failed
- `503`: Camera not initialized

**Usage Example:**
```python
response = requests.get(f"{base_url}/webcam/v1/snapshot")
if response.status_code == 200:
    with open("snapshot.jpg", "wb") as f:
        f.write(response.content)
```

#### `GET /api/webcam/v1/status`
Get camera configuration and status.

**Response:**
```json
{
  "initialized": true,
  "status": "ready",
  "settings": {
    "framesize": 6,
    "framesize_name": "QVGA",
    "quality": 80,
    "brightness": 0,
    "contrast": 0,
    "saturation": 0
  }
}
```

**Status Values:**
- `ready`: Camera operational
- `sensor_error`: Hardware communication error
- `not_initialized`: Camera not started

**Frame Size Codes:**
| Code | Name | Resolution |
|------|------|------------|
| 5 | QVGA | 320x240 |
| 6 | CIF | 400x296 |
| 8 | VGA | 640x480 |
| 9 | SVGA | 800x600 |
| 10 | XGA | 1024x768 |
| 12 | UXGA | 1600x1200 |

---

### 7. UDP Service

#### `GET /api/udp/v1`
Get UDP server statistics and recent messages.

**Response:**
```json
{
  "port": 24642,
  "total": 1523,
  "dropped": 5,
  "buffer": "15/20",
  "messages": [
    "[125 ms] forward",
    "[230 ms] stop",
    "[450 ms] turn_left"
  ]
}
```

**Fields:**
- `port`: UDP listening port
- `total`: Total messages received
- `dropped`: Messages lost (buffer overflow/mutex timeout)
- `buffer`: Current/max buffer size
- `messages`: Last N messages with inter-arrival times

---

### 8. Settings Service

Persistent key-value storage using ESP32 Preferences.

#### `GET /api/settings/v1/settings?domain=<domain>&key=<key>`
Retrieve setting value.

**Query Parameters:**
- `domain` (required): Namespace (max 15 chars, alphanumeric + underscore)
- `key` (optional): Specific key (max 15 chars)

**Single Key Response:**
```json
{
  "domain": "wifi",
  "key": "ssid",
  "value": "MyNetwork"
}
```

**All Keys Response (limited info):**
```json
{
  "domain": "wifi",
  "settings": {}
}
```

**Note:** ESP32 Preferences doesn't support key enumeration. Retrieving all settings in a domain returns minimal information.

#### `POST /api/settings/v1/settings`
Update/insert settings.

**Option 1 - Single Setting:**
```json
{
  "domain": "wifi",
  "key": "ssid",
  "value": "MyNetwork"
}
```

**Option 2 - Multiple Settings (JSON body):**
```json
{
  "domain": "robot_config",
  "settings": {
    "speed": "75",
    "mode": "autonomous"
  }
}
```

**Response (200):**
```json
{
  "success": true,
  "message": "Operation successful."
}
```

**Common Domains:**
- `wifi`: Network credentials
- `servo`: Servo calibration data
- `camera`: Camera settings
- Custom: Any valid domain name

---

### 9. MicroTF Service

**Status:** In development - basic structure implemented.

#### `POST /api/microtf/v1/detect`
Trigger object detection (async).

**Response:**
```json
{
  "status": "detection_triggered"
}
```

#### `GET /api/microtf/v1/results`
Retrieve last detection results.

**Response:**
```json
{
  "objects": []
}
```

**Note:** Full TensorFlow Lite inference not yet operational.

---

### 10. HTTP Service

#### `GET /api/openapi.json`
Get complete OpenAPI 3.0.0 specification.

**Response:**
```json
{
  "openapi": "3.0.0",
  "info": {
    "title": "K10 Bot API",
    "version": "1.0.0"
  },
  "servers": [{"url": "/api"}],
  "paths": {
    "/board/v1": {...},
    "/sensors/v1": {...}
  }
}
```

**Use Cases:**
- Dynamic API discovery
- Client SDK generation
- Documentation generation
- Validation

#### `GET /`
Web dashboard (HTML).

#### `GET /api/docs`
Interactive API testing interface (HTML).

---

## UDP Protocol

### Connection Details
- **Protocol**: UDP (unreliable, no handshake)
- **Port**: 24642 (default, configurable)
- **Encoding**: UTF-8 text strings
- **Max Message Size**: ~256 bytes (check `MAX_MESSAGE_LEN`)

### Message Format
Plain text commands, newline-optional. No JSON wrapping.

**Example:**
```
forward
```

Not:
```json
{"command": "forward"}
```

### Command Reference (RemoteControlService)

**D-Pad Commands:**
- `up`
- `down`
- `left`
- `right`

**Button Commands:**
- `circle`
- `square`
- `triangle`
- `cross`

**Movement Commands:**
- `forward`: Move forward
- `backward`: Move backward
- `turn_left`: Rotate left
- `turn_right`: Rotate right
- `stop`: Emergency stop

### Custom Handler Registration

Services can register UDP message handlers programmatically:

```cpp
// In your service's startService() method
int handler_id = udp_service->registerMessageHandler(
    [this](const std::string& msg, const IPAddress& ip, uint16_t port) {
        if (msg == "my_command") {
            // Handle command
            return true;  // Message handled
        }
        return false;  // Not my command
    }
);
```

**Handler Return Values:**
- `true`: Command recognized and handled
- `false`: Command not relevant to this handler

**Handler Execution:**
- All handlers receive every message
- Handlers run synchronously on UDP task
- Keep handlers fast (<10ms)
- Mutex timeout: 10ms

### UDP Message Lifecycle
1. Message arrives on port 24642
2. Parsed as UTF-8 string
3. All registered handlers invoked in order
4. Message logged to internal buffer (20 message history)
5. Statistics updated (total/dropped counts)

### Sending UDP Commands

**Linux/macOS:**
```bash
echo "forward" | nc -u 192.168.1.100 24642
echo "stop" | nc -u 192.168.1.100 24642
```

**Python:**
```python
import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.sendto(b"forward", ("192.168.1.100", 24642))
sock.sendto(b"stop", ("192.168.1.100", 24642))
sock.close()
```

**Node.js:**
```javascript
const dgram = require('dgram');
const client = dgram.createSocket('udp4');

client.send('forward', 24642, '192.168.1.100', (err) => {
  client.close();
});
```

### UDP vs HTTP Decision Matrix

| Use Case | Protocol | Reason |
|----------|----------|--------|
| Real-time control | UDP | Low latency, no connection overhead |
| Sensor polling | HTTP | Reliable delivery, structured data |
| Configuration | HTTP | Needs confirmation, persistent state |
| Emergency stop | UDP | Fastest delivery, no TCP handshake |
| Status monitoring | HTTP | Reliable, cacheable |
| Telemetry streaming | UDP | High frequency, packet loss acceptable |

---

## Data Types and Units

### Sensor Data
- **Temperature**: Celsius (float)
- **Humidity**: Relative percentage 0-100 (float)
- **Light**: Arbitrary units 0-65535 (uint16)
- **Microphone**: ADC value 0-1023 (uint64)
- **Accelerometer**: m/s² gravity units (3x int16)

### Motor/Servo Control
- **Speed**: Percentage -100 to +100 (int8)
  - Negative = reverse
  - 0 = stop
  - Positive = forward
- **Angle**: Degrees (uint16)
  - 180° servo: 0-180
  - 270° servo: 0-270

### Time Values
- **Uptime**: Milliseconds since boot (uint64)
- **Timestamps**: Milliseconds (unsigned long)
- **Beat Duration**: Milliseconds (uint32)

### Memory Values
- **Heap/Stack**: Bytes (uint32)
- **Sketch Space**: Bytes (uint32)

---

## Common Integration Patterns

### Pattern 1: Autonomous Navigation
```python
import requests
import time

base_url = "http://192.168.1.100/api"

# 1. Check sensors periodically
while True:
    sensors = requests.get(f"{base_url}/sensors/v1").json()
    
    # 2. Analyze accelerometer for collisions
    accel = sensors["accelerometer"]
    if abs(accel[0]) > 15 or abs(accel[1]) > 15:
        # Emergency stop via UDP (fastest)
        udp_sock.sendto(b"stop", (device_ip, 24642))
        break
    
    # 3. Adjust servos based on light level
    if sensors["light"] < 50:
        requests.post(f"{base_url}/servos/v1/setServoSpeed",
                     json={"channel": 0, "speed": 30})
    else:
        requests.post(f"{base_url}/servos/v1/setServoSpeed",
                     json={"channel": 0, "speed": 60})
    
    time.sleep(0.5)
```

### Pattern 2: Remote Control
```python
import socket

# Use UDP for real-time control
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
device = ("192.168.1.100", 24642)

def send_command(cmd):
    sock.sendto(cmd.encode(), device)

# Keyboard mapping example
if key == 'w':
    send_command("forward")
elif key == 's':
    send_command("backward")
elif key == 'a':
    send_command("turn_left")
elif key == 'd':
    send_command("turn_right")
elif key == ' ':
    send_command("stop")
```

### Pattern 3: Sensor Data Logger
```python
import requests
import time
import csv

base_url = "http://192.168.1.100/api"

with open("sensor_log.csv", "w") as f:
    writer = csv.writer(f)
    writer.writerow(["timestamp", "light", "temp", "humidity", "accel_x", "accel_y", "accel_z"])
    
    while True:
        sensors = requests.get(f"{base_url}/sensors/v1").json()
        board = requests.get(f"{base_url}/board/v1").json()
        
        writer.writerow([
            board["uptimeMs"],
            sensors["light"],
            sensors["celcius"],
            sensors["hum_rel"],
            sensors["accelerometer"][0],
            sensors["accelerometer"][1],
            sensors["accelerometer"][2]
        ])
        
        time.sleep(1)
```

### Pattern 4: Configuration Management
```python
# Save robot configuration
config = {
    "domain": "robot_profile",
    "settings": {
        "max_speed": "80",
        "turn_sensitivity": "75",
        "auto_stop_distance": "20"
    }
}
requests.post(f"{base_url}/settings/v1/settings", json=config)

# Load configuration
response = requests.get(
    f"{base_url}/settings/v1/settings",
    params={"domain": "robot_profile", "key": "max_speed"}
)
max_speed = int(response.json()["value"])
```

### Pattern 5: Camera-Based Detection
```python
import requests
from PIL import Image
from io import BytesIO

# Capture image
response = requests.get(f"{base_url}/webcam/v1/snapshot")
image = Image.open(BytesIO(response.content))

# Process with computer vision
# (OpenCV, TensorFlow, etc.)
detected_objects = detect_objects(image)

# React based on detection
if "obstacle" in detected_objects:
    udp_sock.sendto(b"stop", (device_ip, 24642))
```

### Pattern 6: Status Monitoring
```python
# Health check function
def check_device_health():
    board = requests.get(f"{base_url}/board/v1").json()
    
    if board["heapFree"] < 50000:
        print("WARNING: Low memory")
    
    if board["uptimeMs"] < 10000:
        print("WARNING: Recent reboot")
    
    # Check service statuses
    services = ["sensors", "servos", "webcam"]
    for service in services:
        try:
            requests.get(f"{base_url}/{service}/v1/status", timeout=2)
        except:
            print(f"ERROR: {service} not responding")
```

---

## Error Handling

### HTTP Errors
```python
import requests

def safe_api_call(url, **kwargs):
    try:
        response = requests.request(**kwargs)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.Timeout:
        print("Device not responding (timeout)")
    except requests.exceptions.ConnectionError:
        print("Cannot connect to device")
    except requests.exceptions.HTTPError as e:
        if e.response.status_code == 503:
            print("Service not initialized")
        elif e.response.status_code == 422:
            print("Invalid parameters")
        else:
            print(f"HTTP error: {e}")
    except Exception as e:
        print(f"Unexpected error: {e}")
    return None
```

### UDP Reliability
```python
import socket

# UDP has no delivery guarantee
# Implement application-level acknowledgment if needed

def send_critical_command(cmd):
    sock.sendto(cmd.encode(), device)
    
    # Verify via HTTP status endpoint
    time.sleep(0.1)
    status = requests.get(f"{base_url}/servos/v1/getAllStatus").json()
    # Check if command was applied
```

### Service Initialization Check
```python
def wait_for_service(service_name, timeout=30):
    start = time.time()
    while time.time() - start < timeout:
        try:
            response = requests.get(
                f"{base_url}/{service_name}/v1/status",
                timeout=2
            )
            if response.status_code == 200:
                return True
        except:
            pass
        time.sleep(1)
    return False

# Usage
if wait_for_service("webcam"):
    print("Webcam ready")
else:
    print("Webcam failed to initialize")
```

---

## Performance Considerations

### HTTP Request Timing
- **Typical latency**: 20-100ms (local WiFi)
- **Sensor read**: ~30ms
- **Servo command**: ~20ms
- **Webcam snapshot**: 100-300ms (depends on JPEG encoding)

### Recommended Polling Rates
| Endpoint | Max Rate | Recommended |
|----------|----------|-------------|
| /sensors/v1 | 20 Hz | 1-5 Hz |
| /board/v1 | 10 Hz | 0.1 Hz |
| /webcam/v1/snapshot | 5 Hz | 1 Hz |
| /servos/v1/getAllStatus | 20 Hz | As needed |

### UDP Performance
- **Latency**: ~5-20ms (local WiFi)
- **Throughput**: Hundreds of commands/second
- **Packet loss**: ~0-5% (WiFi dependent)

### Memory Constraints
- **ESP32-S3**: ~327KB total heap
- **Typical free**: ~280KB after services start
- **Critical threshold**: <50KB free
- **Recommendation**: Monitor via `/api/board/v1`

### Concurrent Connections
- **Max HTTP connections**: ~4 simultaneous
- **UDP**: Unlimited senders (messages queued)
- **WebSocket**: Not implemented

---

## Network Configuration

### WiFi Settings
Managed via Settings Service:

```python
# Configure WiFi
requests.post(f"{base_url}/settings/v1/settings", json={
    "domain": "wifi",
    "key": "ssid",
    "value": "MyNetwork"
})
requests.post(f"{base_url}/settings/v1/settings", json={
    "domain": "wifi",
    "key": "password",
    "value": "MyPassword"
})
# Requires device reboot to apply
```

### Access Point Mode
- **SSID**: `aMaker-XXXXXX` (XXXXXX = chip ID)
- **Password**: Check device documentation
- **IP**: 192.168.4.1 (typical ESP32 AP default)

### Hostname
- Configurable via Settings Service
- mDNS may be supported (check `<hostname>.local`)

---

## Security Considerations

### Current Security Model
- **No encryption**: HTTP (not HTTPS)
- **Minimal authentication**: Most endpoints unprotected
- **Network isolation**: Recommended for untrusted networks

### Best Practices for LLM Integration
1. **Never expose device directly to internet**
2. **Use VPN/SSH tunnel** for remote access
3. **Validate all user inputs** before sending to device
4. **Rate limit requests** to prevent overload
5. **Monitor for unauthorized commands** (UDP logging)
6. **Store credentials securely** (don't hardcode in prompts)

### Recommended Architecture
```
Internet ← → LLM Service ← → VPN/Gateway ← → Local WiFi ← → K10 Bot
```

Not:
```
Internet ← → K10 Bot (DANGEROUS!)
```

---

## Troubleshooting

### Device Not Responding
1. Check WiFi connection
2. Ping device IP: `ping <device-ip>`
3. Check UDP port: `nc -u -v <device-ip> 24642`
4. Verify device uptime: `curl http://<device-ip>/api/board/v1`

### Service Unavailable (503)
- Service not initialized
- Check logs via web UI: `http://<device-ip>/logservice.html`
- Restart device

### Invalid Parameters (422)
- Check parameter names (case-sensitive)
- Verify data types (numbers vs strings)
- Review endpoint documentation

### Servo Not Moving
1. Check attachment: `GET /api/servos/v1/getStatus?channel=X`
2. Attach if needed: `POST /api/servos/v1/attachServo`
3. Verify power supply to servo controller
4. Check servo type matches command (angle vs speed)

### Camera Errors
- `sensor_error`: Hardware communication failure
- `not_initialized`: Service not started
- Solution: Restart device, check camera cable

### UDP Messages Not Received
1. Verify port: `GET /api/udp/v1` → check `"port"`
2. Check firewall rules
3. Test with netcat: `echo "test" | nc -u <ip> 24642`
4. Check dropped packet count in UDP stats

---

## OpenAPI Specification

### Dynamic Discovery
The device exposes its full OpenAPI spec:

```python
spec = requests.get(f"{base_url}/openapi.json").json()

# Enumerate all endpoints
for path, methods in spec["paths"].items():
    print(f"{path}:")
    for method, details in methods.items():
        print(f"  {method.upper()}: {details.get('summary', 'No description')}")
```

### Schema Validation
Use OpenAPI schema to validate requests before sending:

```python
from jsonschema import validate

# Get schema for endpoint
endpoint_schema = spec["paths"]["/api/servos/v1/setServoAngle"]["post"]["requestBody"]["content"]["application/json"]["schema"]

# Validate request
request_data = {"channel": 0, "angle": 90}
validate(instance=request_data, schema=endpoint_schema)
```

---

## Service Architecture

### Service Lifecycle
All services follow this pattern:
1. `initializeService()` - Hardware/resource allocation
2. `startService()` - Begin operations
3. Running state - Handle requests
4. `stopService()` - Clean shutdown

### Service Status States
- `UNINITIALIZED`: Not started
- `INITIALIZED`: Hardware ready
- `STARTED`: Operational
- `STOPPED`: Gracefully stopped
- `INIT_FAILED`: Initialization error
- `START_FAILED`: Startup error

### Service Interfaces
- **IsServiceInterface**: Lifecycle management, logging
- **IsOpenAPIInterface**: HTTP routes, OpenAPI docs

### FreeRTOS Task Architecture
- **Core 0**: UDP message handling (real-time)
- **Core 1**: HTTP server, display updates
- Tasks communicate via FreeRTOS queues/mutexes

---

## Advanced Topics

### Custom Service Integration
To add UDP command handling from your LLM application:

1. Send UDP commands directly (no device code change needed)
2. Device logs all UDP messages → `/api/udp/v1`
3. Optionally implement custom handler in device firmware

### Batch Operations
Some endpoints support bulk operations:
- `setAllServoAngle` - Set multiple servos at once
- `setAllServoSpeed` - Set multiple servos at once

### Settings Persistence
All settings survive reboots via ESP32 NVS (flash storage):
- WiFi credentials
- Servo calibration
- Custom application data

### Firmware Updates
- OTA (Over-The-Air) updates possible
- Requires `/update` endpoint implementation
- Check device documentation for update procedure

---

## Complete Example: Obstacle Avoidance Robot

```python
import requests
import socket
import time

# Configuration
DEVICE_IP = "192.168.1.100"
BASE_URL = f"http://{DEVICE_IP}/api"
UDP_PORT = 24642

# Setup
udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
udp_dest = (DEVICE_IP, UDP_PORT)

def send_udp(cmd):
    udp_sock.sendto(cmd.encode(), udp_dest)

def setup_servos():
    # Attach two continuous servos for wheels
    requests.post(f"{BASE_URL}/servos/v1/attachServo", 
                  json={"channel": 0, "connection": 1})  # Left wheel
    requests.post(f"{BASE_URL}/servos/v1/attachServo", 
                  json={"channel": 1, "connection": 1})  # Right wheel

def get_distance_from_light():
    # Assume light sensor correlates with distance to object
    sensors = requests.get(f"{BASE_URL}/sensors/v1").json()
    return sensors["light"]

def main():
    setup_servos()
    
    while True:
        distance = get_distance_from_light()
        
        if distance < 30:  # Obstacle detected
            print("Obstacle! Turning...")
            send_udp("stop")
            time.sleep(0.5)
            send_udp("turn_right")
            time.sleep(1.0)
        else:
            print("Path clear, moving forward")
            send_udp("forward")
        
        time.sleep(0.2)

if __name__ == "__main__":
    main()
```

---

## Quick Reference Card

### Essential Endpoints
```
GET  /api/board/v1              → System info
GET  /api/sensors/v1            → All sensor readings
POST /api/servos/v1/setServoAngle    → Move angular servo
POST /api/servos/v1/setServoSpeed    → Set continuous servo speed
POST /api/servos/v1/stopAll     → Emergency stop
GET  /api/webcam/v1/snapshot    → Capture image
POST /api/music/v1/play         → Play melody
GET  /api/openapi.json          → Full API spec
```

### UDP Commands (port 24642)
```
forward, backward, turn_left, turn_right, stop
up, down, left, right
circle, square, triangle, cross
```

### Status Codes
```
200 OK
422 Invalid Parameters
456 Operation Failed
503 Service Unavailable
```

---

## Support and Documentation

- **Full API Spec**: `http://<device-ip>/api/openapi.json`
- **Interactive Testing**: `http://<device-ip>/api/docs`
- **Web Dashboard**: `http://<device-ip>/`
- **Logs**: `http://<device-ip>/logservice.html`
- **Service Docs**: See `/docs/` directory in source repo

---

## Changelog

**v1.0.0** (Initial Release)
- Board info, sensors, servo control
- Music service with 20 melodies
- Webcam JPEG snapshots
- UDP telemetry
- Settings persistence
- OpenAPI specification
- MicroTensorFlow (experimental)

---

## License and Safety

### Safety Notice
This device controls physical actuators (servos, motors). When integrating with LLMs:
- **Implement failsafes** (timeout-based stop)
- **Validate all commands** before execution
- **Monitor for unexpected behavior**
- **Emergency stop mechanism** (hardware button + software command)
- **Test in safe environment first**

### Ethical Considerations
- Ensure LLM-generated commands are **reviewed by humans** for safety-critical applications
- Implement **rate limiting** to prevent command flooding
- Log all actions for **audit trail**
- Design **graceful degradation** when connectivity lost

---

**Document Version:** 1.0.0  
**Last Updated:** 2026-02-08  
**Device Firmware:** 1.0.0  
**Supported Hardware:** UniHiker K10 (ESP32-S3)
