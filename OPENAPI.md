# K10 Bot API Documentation

## Overview
This document describes all REST API endpoints available in the K10 Bot system. The API follows OpenAPI 3.0.0 specification and uses JSON for request/response payloads.

**Base URL:** `http://<device-ip>/api/`

**OpenAPI Specification:** Available at `/api/openapi.json`

---

## Board Info Service

### GET /api/board/v1
Retrieves comprehensive board information including system metrics, memory usage, chip details, and firmware version.

**Tags:** Board Info  
**Authentication:** None

**Response Codes:**
- `200` - Board information retrieved successfully

**Response Schema (200):**
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

### GET /api/board/v1/settings
Get service-specific settings for Board Info service.

**Tags:** Board Info  
**Authentication:** None

**Response Codes:**
- `200` - Settings retrieved successfully

### POST /api/board/v1/settings
Update service-specific settings for Board Info service.

**Tags:** Board Info  
**Authentication:** None

**Response Codes:**
- `200` - Settings updated successfully

---

## K10 Sensors Service

### GET /api/sensors/v1
Retrieves all K10 sensor readings including light, temperature, humidity, microphone, and accelerometer data.

**Tags:** Sensors  
**Authentication:** None

**Response Codes:**
- `200` - Sensor data retrieved successfully
- `503` - Sensor initialization or reading failed

**Response Schema (200):**
```json
{
  "light": 125.5,
  "hum_rel": 45.2,
  "celcius": 23.8,
  "mic_data": 512,
  "accelerometer": [0.12, -0.05, 9.81]
}
```

### GET /api/sensors/v1/settings
Get service-specific settings for K10 Sensors service.

**Tags:** Sensors  
**Authentication:** None

**Response Codes:**
- `200` - Settings retrieved successfully

---

## Music Service

### POST /api/music/v1/play
Play built-in melody with playback options.

**Tags:** Music  
**Authentication:** None

**Query Parameters:**
- `melody` (string, required) - Melody enum value (0-19)
- `option` (string, optional) - Playback option (1=Once, 2=Forever, 4=OnceInBackground, 8=ForeverInBackground)

**Request Example:**
```json
{
  "melody": 0,
  "option": 4
}
```

**Response Codes:**
- `200` - Operation completed successfully
- `400` - Missing required parameter

**Available Melodies:**
0=DADADADUM, 1=ENTERTAINER, 2=PRELUDE, 3=ODE, 4=NYAN, 5=RINGTONE, 6=FUNK, 7=BLUES, 8=BIRTHDAY, 9=WEDDING, 10=FUNERAL, 11=PUNCHLINE, 12=BADDY, 13=CHASE, 14=BA_DING, 15=WAWAWAWAA, 16=JUMP_UP, 17=JUMP_DOWN, 18=POWER_UP, 19=POWER_DOWN

### POST /api/music/v1/tone
Play a tone at specified frequency and duration.

**Tags:** Music  
**Authentication:** None

**Query Parameters:**
- `freq` (string, required) - Frequency in Hz
- `beat` (string, optional) - Beat duration (default 8000)

**Request Example:**
```json
{
  "freq": 440,
  "beat": 8000
}
```

**Response Codes:**
- `200` - Operation completed successfully
- `400` - Missing required parameter

### POST /api/music/v1/stop
Stop current tone playback.

**Tags:** Music  
**Authentication:** None

**Response Codes:**
- `200` - Operation completed successfully

### POST /api/music/v1/melodies
Get list of available built-in melodies.

**Tags:** Music  
**Authentication:** None

**Response Codes:**
- `200` - Melody list retrieved successfully

**Response Example:**
```json
["DADADADUM","ENTERTAINER","PRELUDE","ODE","NYAN","RINGTONE","FUNK","BLUES","BIRTHDAY","WEDDING","FUNERAL","PUNCHLINE","BADDY","CHASE","BA_DING","WAWAWAWAA","JUMP_UP","JUMP_DOWN","POWER_UP","POWER_DOWN"]
```

---

## Settings Service

### GET /api/settings/v1/settings
Retrieve a single setting value or all settings in a domain.

**Tags:** Settings  
**Authentication:** None

**Query Parameters:**
- `domain` (string, required) - Settings domain/namespace (max 15 chars, alphanumeric and underscore)
- `key` (string, optional) - Setting key (max 15 chars, alphanumeric and underscore)

**Response Codes:**
- `200` - Successful operation
- `422` - Invalid parameters
- `503` - Service not initialized or operation failed

**Response Example:**
```json
{
  "domain": "wifi",
  "settings": {}
}
```

**Note:** ESP32 Preferences does not support key enumeration. Retrieving all settings in a domain returns limited information.

### POST /api/settings/v1/settings
Update or insert setting values in a domain.

**Tags:** Settings  
**Authentication:** None

**Query Parameters:**
- `domain` (string, required) - Settings domain/namespace (max 15 chars, alphanumeric and underscore)
- `key` (string, optional) - Setting key for single update (max 15 chars)
- `value` (string, optional) - Setting value for single update

**Alternative:** JSON body with multiple key-value pairs

**Response Codes:**
- `200` - Settings updated successfully
- `422` - Invalid parameters
- `503` - Operation failed

**Response Example:**
```json
{
  "success": true,
  "message": "Operation successful."
}
```

---

## DFR1216 Expansion Board Service

### POST /api/dfr1216/setServoAngle
Set the angle of a servo motor on the DFR1216 expansion board.

**Tags:** DFR1216  
**Authentication:** None

**Parameters:**
- `channel` (integer, required) - Servo channel (0-5)
- `angle` (integer, required) - Angle in degrees (0-180)

**Request Body:**
```json
{
  "channel": 0,
  "angle": 90
}
```

**Response Codes:**
- `200` - Servo angle set successfully
- `422` - Missing or invalid parameters
- `503` - DFR1216 not initialized

**Response Schema (200):**
```json
{
  "result": "ok",
  "channel": 0,
  "angle": 90
}
```

### POST /api/dfr1216/setMotorSpeed
Set the speed and direction of a DC motor on the DFR1216 expansion board.

**Tags:** DFR1216  
**Authentication:** None

**Parameters:**
- `motor` (integer, required) - Motor number (1-4)
- `speed` (integer, required) - Speed percentage (-100 to +100, negative is reverse)

**Request Body:**
```json
{
  "motor": 1,
  "speed": 75
}
```

**Response Codes:**
- `200` - Motor speed set successfully
- `422` - Missing or invalid parameters
- `503` - DFR1216 not initialized

**Response Schema (200):**
```json
{
  "result": "ok",
  "motor": 1,
  "speed": 75
}
```

### GET /api/dfr1216/getStatus
Get initialization status and operational state of the DFR1216 expansion board.

**Tags:** DFR1216  
**Authentication:** None

**Response Codes:**
- `200` - Status retrieved successfully

**Response Schema (200):**
```json
{
  "message": "DFR1216Service",
  "status": "started"
}
```

---

## Servo Service

### POST /api/servos/v1/setServoAngle
Set servo angle for angular servos (180° or 270°).

**Tags:** Servos  
**Authentication:** Required

**Request Body:**
```json
{
  "channel": 0,
  "angle": 90
}
```

**Request Body Schema:**
- `channel` (integer, required) - Servo channel (0-7)
- `angle` (integer, required) - Angle in degrees (0-180 for 180° servos, 0-270 for 270° servos)

**Response Codes:**
- `200` - Operation successful
- `422` - Missing or invalid parameters
- `456` - Operation failed
- `503` - Servo controller not initialized

**Response Schema (200):**
```json
{
  "result": "ok",
  "message": "setServoAngle"
}
```

### POST /api/servos/v1/setServoSpeed
Set continuous servo speed for rotational servos.

**Tags:** Servos  
**Authentication:** Required

**Request Body:**
```json
{
  "channel": 0,
  "speed": 50
}
```

**Request Body Schema:**
- `channel` (integer, required) - Servo channel (0-7)
- `speed` (integer, required) - Speed percentage (-100 to +100, negative is reverse)

**Response Codes:**
- `200` - Operation successful
- `422` - Missing or invalid parameters
- `456` - Operation failed
- `503` - Servo controller not initialized

**Response Schema (200):**
```json
{
  "result": "ok",
  "message": "setServoSpeed"
}
```

### POST /api/servos/v1/stopAll
Stop all servos by setting speed to 0.

**Tags:** Servos  
**Authentication:** None

**Response Codes:**
- `200` - Operation successful
- `422` - Missing or invalid parameters
- `456` - Operation failed
- `503` - Servo controller not initialized

**Response Schema (200):**
```json
{
  "result": "ok",
  "message": "stopAll"
}
```

### GET /api/servos/v1/getStatus
Get servo type and connection status for a specific channel.

**Tags:** Servos  
**Authentication:** Required

**Query Parameters:**
- `channel` (integer, required) - Servo channel (0-7)

**Response Codes:**
- `200` - Servo status retrieved
- `422` - Invalid channel

**Response Schema (200):**
```json
{
  "channel": 0,
  "connection": "Angular 180"
}
```

### GET /api/servos/v1/getAllStatus
Get connection status and type for all 8 servo channels.

**Tags:** Servos  
**Authentication:** None

**Response Codes:**
- `200` - All servos status retrieved

**Response Schema (200):**
```json
{
  "attached_servos": [
    {"channel": 0, "connection": "Angular 180"},
    {"channel": 1, "connection": "Not Connected"}
  ]
}
```

### POST /api/servos/v1/setAllServoAngle
Set all attached angular servos to the same angle simultaneously.

**Tags:** Servos  
**Authentication:** Required

**Request Body:**
```json
{
  "angle": 90
}
```

**Request Body Schema:**
- `angle` (integer, required) - Angle in degrees (0-360)

**Response Codes:**
- `200` - Operation successful
- `422` - Missing or invalid parameters
- `456` - Operation failed
- `503` - Servo controller not initialized

**Response Schema (200):**
```json
{
  "result": "ok",
  "message": "setAllServoAngle"
}
```

### POST /api/servos/v1/setAllServoSpeed
Set all attached continuous rotation servos to the same speed simultaneously.

**Tags:** Servos  
**Authentication:** Required

**Request Body:**
```json
{
  "speed": 50
}
```

**Request Body Schema:**
- `speed` (integer, required) - Speed percentage (-100 to +100)

**Response Codes:**
- `200` - Operation successful
- `422` - Missing or invalid parameters
- `456` - Operation failed
- `503` - Servo controller not initialized

**Response Schema (200):**
```json
{
  "result": "ok",
  "message": "setAllServoSpeed"
}
```

### POST /api/servos/v1/attachServo
Register a servo type to a channel before use.

**Tags:** Servos  
**Authentication:** Required

**Request Body:**
```json
{
  "channel": 0,
  "connection": 2
}
```

**Request Body Schema:**
- `channel` (integer, required) - Servo channel (0-7)
- `connection` (integer, required) - Servo connection type (0=None, 1=continuous, 2=angular 180°, 3=angular 270°)

**Response Codes:**
- `200` - Operation successful
- `422` - Missing or invalid parameters
- `456` - Operation failed
- `503` - Servo controller not initialized

**Response Schema (200):**
```json
{
  "result": "ok",
  "message": "attachServo"
}
```

---

## UDP Service

### GET /api/udp/v1
Get UDP server statistics including total messages received, dropped packets, buffer usage, and recent message history with inter-arrival times.

**Tags:** UDP  
**Authentication:** None

**Response Codes:**
- `200` - UDP server statistics retrieved successfully

**Response Schema (200):**
```json
{
  "port": 8080,
  "total": 1523,
  "dropped": 5,
  "buffer": "15/20",
  "messages": [
    "[125 ms] Hello",
    "[230 ms] World"
  ]
}
```

**Note:** If buffer is locked, the response includes an `error` field instead.

---

## Webcam Service

### GET /api/webcam/v1/snapshot
Capture and return a JPEG snapshot from the camera in real-time. Image format is QVGA (320x240) by default with quality setting of 80.

**Tags:** Webcam  
**Authentication:** None

**Response Codes:**
- `200` - JPEG image captured successfully
- `500` - Failed to capture image
- `503` - Camera not initialized

**Response Content-Type:** `image/jpeg`

### GET /api/webcam/v1/status
Get camera initialization status, current settings including frame size, quality, brightness, contrast, and saturation levels.

**Tags:** Webcam  
**Authentication:** None

**Response Codes:**
- `200` - Camera status retrieved successfully

**Response Schema (200):**
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
- `ready` - Camera operational
- `sensor_error` - Sensor communication error
- `not_initialized` - Camera not initialized

---

## MicroTF Service

### POST /api/microtf/v1/detect
Trigger object detection asynchronously using MicroTensorFlow.

**Tags:** MicroTF  
**Authentication:** None

**Response Codes:**
- `200` - Detection triggered successfully

**Response Schema (200):**
```json
{
  "status": "detection_triggered"
}
```

### GET /api/microtf/v1/results
Retrieve results from last detection inference including inference time and detected objects.

**Tags:** MicroTF  
**Authentication:** None

**Response Codes:**
- `200` - Results retrieved successfully

**Response Schema (200):**
```json
{
  "objects": []
}
```

**Note:** This service is currently in development. Full object detection functionality is not yet implemented.

---

## HTTP Service

### GET /api/openapi.json
Get OpenAPI 3.0.0 specification for all registered services including paths, parameters, request bodies, and response schemas.

**Tags:** OpenAPI  
**Authentication:** None

**Response Codes:**
- `200` - OpenAPI specification retrieved successfully

**Response Schema (200):**
```json
{
  "openapi": "3.0.0",
  "info": {
    "title": "K10 Bot API",
    "version": "1.0.0"
  },
  "paths": {}
}
```

### GET /
Home page with service dashboard and route listing.

**Authentication:** None

**Response:** HTML page with service interfaces and developer tools

### GET /api/docs
OpenAPI documentation page with interactive API test interface.

**Authentication:** None

**Response:** HTML page for testing API endpoints

---

## WiFi Service

The WiFi service manages network connectivity but does not expose direct HTTP endpoints. It handles:
- WiFi station connection to existing networks
- Access Point (AP) mode fallback
- Hostname configuration
- Network status monitoring

Configuration is managed through the Settings Service using the `wifi` domain.

---

---

## Common Response Format

Most endpoints follow a standard response format for success and error cases:

**Success Response:**
```json
{
  "result": "ok",
  "message": "<operation_name>"
}
```

**Error Response:**
```json
{
  "result": "error",
  "message": "<error_description>"
}
```

Or:

```json
{
  "error": "<error_description>"
}
```

---

## HTTP Status Codes

- `200 OK` - Request successful
- `400 Bad Request` - Invalid request format or missing required parameters
- `422 Unprocessable Entity` - Missing or invalid parameters
- `456 Custom Error` - Operation failed (device-specific error)
- `500 Internal Server Error` - Server error during operation
- `503 Service Unavailable` - Service not initialized or unavailable

---

## Authentication

Some endpoints require authentication (marked with "Authentication: Required"). The authentication mechanism uses bearer tokens.

**Security Scheme:**
- Type: HTTP
- Scheme: bearer
- Format: token

---

## Notes

1. All timestamps are in milliseconds since boot (`uptimeMs`)
2. Temperature readings are in Celsius
3. Humidity readings are in relative humidity percentage
4. Accelerometer readings are in m/s² (gravity units)
5. Servo channels are 0-indexed:
   - Servo Service (DFR0548): channels 0-7
   - DFR1216 Service: channels 0-5
6. Motor numbers are 1-indexed (1-4) for DFR1216
7. Speed values are percentages: -100 (full reverse) to +100 (full forward)
8. JPEG quality: 0-100, where higher values mean higher quality (opposite of some other APIs)
9. Frame sizes: Various formats supported including QVGA (320x240), SVGA (800x600), UXGA (1600x1200)
10. Settings service uses ESP32 Preferences with domain and key length limits of 15 characters
11. Melody IDs range from 0-19, with specific melodies mapped to each number
12. Music playback options: 1=Once, 2=Forever, 4=OnceInBackground, 8=ForeverInBackground

---

## Service Architecture

The K10 Bot uses a modular service-based architecture where each service implements both `IsServiceInterface` and `IsOpenAPIInterface`:

- **IsServiceInterface:** Provides lifecycle management (initialize, start, stop)
- **IsOpenAPIInterface:** Provides HTTP endpoint registration and OpenAPI documentation

Services are registered with the HTTP service at startup and can be queried for status via their individual status endpoints.

---

## Getting Started

1. **Connect to Network:**
   - Connect to the K10 Bot WiFi network (default: `aMaker-XXXXXX`) or
   - Ensure the device is on your network if configured with WiFi credentials

2. **Access Dashboard:**
   - Navigate to `http://<device-ip>/` for the main dashboard

3. **Explore API:**
   - View OpenAPI specification at `http://<device-ip>/api/openapi.json`
   - Use interactive API test interface at `http://<device-ip>/api/docs`

4. **Use Endpoints:**
   - Use the endpoints documented above to interact with the device
   - Monitor UDP service for real-time telemetry data if enabled

5. **Configure Settings:**
   - Use Settings Service to persist configuration across reboots
   - Store WiFi credentials, service parameters, and other persistent data

---

## Service Status Endpoints

Most services provide a `/settings` endpoint that returns service-specific configuration and status:

- `GET /api/<service>/<version>/settings` - Get service settings and status

This follows a common pattern across services implementing the settings interface.

---

## Development Notes

- Camera captures frames in RGB565 format and converts to JPEG on demand
- UDP service uses FreeRTOS tasks and runs on dedicated core for real-time performance
- All PROGMEM strings are used to minimize RAM usage on the ESP32-S3
- Services use a rolling logger for debug output (accessible via serial)
- File system uses LittleFS for static content (HTML, CSS, JS)

---

## Support

For issues or questions:
- Check the project repository documentation
- Review service-specific header files in `/src/services/`
- Consult [IsServiceInterface.md](docs/IsServiceInterface.md) and [IsOpenAPIInterface.md](docs/IsOpenAPIInterface.md) for architecture details

---

## Version History

**v1.0.0** - Initial release
- Board info, sensors, servo control (DFR0548 & DFR1216)
- Music playback service with 20 built-in melodies
- Webcam service with JPEG snapshot capture
- UDP telemetry service
- Settings service with ESP32 Preferences
- MicroTensorFlow service (in development)
- OpenAPI 3.0.0 specification generation
- Interactive API documentation interface
