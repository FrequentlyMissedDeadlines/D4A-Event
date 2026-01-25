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

---

## Sensors Service

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
  "status": "success",
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
  "status": "success",
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
  "service": "DFR1216Service",
  "initialized": true,
  "status": "running"
}
```

---

## Servo Service

### POST /api/servos/v1/setServoAngle
Set servo angle for angular servos (180° or 270°).

**Tags:** Servos  
**Authentication:** Required

**Parameters:**
- `channel` (integer, required) - Servo channel (0-7)
- `angle` (integer, required) - Angle in degrees (0-180 for 180° servos, 0-270 for 270° servos)

**Request Body:**
```json
{
  "channel": 0,
  "angle": 90
}
```

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

**Parameters:**
- `channel` (integer, required) - Servo channel (0-7)
- `speed` (integer, required) - Speed percentage (-100 to +100, negative is reverse)

**Request Body:**
```json
{
  "channel": 0,
  "speed": 50
}
```

**Response Codes:**
- `200` - Operation successful
- `422` - Missing or invalid parameters
- `456` - Operation failed
- `503` - Servo controller not initialized

### POST /api/servos/v1/stopAll
Stop all servos by setting speed to 0.

**Tags:** Servos  
**Authentication:** None

**Response Codes:**
- `200` - Operation successful
- `422` - Missing or invalid parameters
- `456` - Operation failed
- `503` - Servo controller not initialized

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
  "status": "ANGULAR180"
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
  "servos": [
    {"channel": 0, "status": "ANGULAR180"},
    {"channel": 1, "status": "NOT_CONNECTED"}
  ]
}
```

### POST /api/servos/v1/setAllServoAngle
Set all attached angular servos to the same angle simultaneously.

**Tags:** Servos  
**Authentication:** Required

**Parameters:**
- `angle` (integer, required) - Angle in degrees (0-360)

**Request Body:**
```json
{
  "angle": 90
}
```

**Response Codes:**
- `200` - Operation successful
- `422` - Missing or invalid parameters
- `456` - Operation failed
- `503` - Servo controller not initialized

### POST /api/servos/v1/setAllServoSpeed
Set all attached continuous rotation servos to the same speed simultaneously.

**Tags:** Servos  
**Authentication:** Required

**Parameters:**
- `speed` (integer, required) - Speed percentage (-100 to +100)

**Request Body:**
```json
{
  "speed": 50
}
```

**Response Codes:**
- `200` - Operation successful
- `422` - Missing or invalid parameters
- `456` - Operation failed
- `503` - Servo controller not initialized

### POST /api/servos/v1/attachServo
Register a servo type to a channel before use.

**Tags:** Servos  
**Authentication:** Required

**Parameters:**
- `channel` (integer, required) - Servo channel (0-7)
- `model` (integer, required) - Servo model type (0=180°, 1=270°, 2=continuous)

**Request Body:**
```json
{
  "channel": 0,
  "model": 0
}
```

**Response Codes:**
- `200` - Operation successful
- `422` - Missing or invalid parameters
- `456` - Operation failed
- `503` - Servo controller not initialized

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
Capture and return a JPEG snapshot from the camera in real-time. Image format is SVGA (800x600) by default with quality setting of 12.

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
    "framesize": 9,
    "framesize_name": "SVGA",
    "quality": 12,
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
  "result": "err",
  "message": "<error_description>"
}
```

---

## HTTP Status Codes

- `200 OK` - Request successful
- `400 Bad Request` - Invalid request format
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
3. Accelerometer readings are in m/s² (gravity units)
4. Servo channels are 0-indexed (0-7 for servo service, 0-5 for DFR1216)
5. Motor numbers are 1-indexed (1-4)
6. Speed values are percentages: -100 (full reverse) to +100 (full forward)
7. JPEG quality: 0-63, where lower values mean higher quality
8. Frame sizes: 0-13 (96x96 to UXGA)

---

## Getting Started

1. Connect to the K10 Bot WiFi network or ensure device is on your network
2. Access the OpenAPI specification at `http://<device-ip>/api/openapi.json`
3. Use the endpoints documented above to interact with the device
4. Monitor the UDP service for real-time telemetry data

---

## Support

For issues or questions, please refer to the project documentation or open an issue in the project repository.
