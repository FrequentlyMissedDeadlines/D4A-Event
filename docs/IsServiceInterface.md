# IsServiceInterface Documentation

## Overview

`IsServiceInterface` is the base interface that all services in the K10 bot project must implement. It defines a standard contract for service lifecycle management, configuration persistence, and logging integration.

**Location**: [src/services/IsServiceInterface.h](src/services/IsServiceInterface.h)

## Purpose

This interface ensures that all services in the system follow a consistent pattern for:
- Initialization and startup
- Graceful shutdown
- Configuration management (save/load settings)
- Integration with the logging system
- Optional OpenAPI endpoint exposure (see [IsOpenAPIInterface.md](IsOpenAPIInterface.md))

## Interface Methods

### Core Lifecycle Methods

#### `getServiceName()`
```cpp
virtual std::string getServiceName() = 0;
```
**Purpose**: Returns a human-readable name for the service.

**Returns**: Service name as a string (e.g., "WiFi Service", "Webcam Service")

**Usage**: Used for logging, debugging, and service identification.

---

#### `initializeService()`
```cpp
virtual bool initializeService() = 0;
```
**Purpose**: Initializes the service and allocates necessary resources.

**Returns**: 
- `true` if initialization was successful
- `false` if initialization failed

**Best Practices**:
- Load configuration from persistent storage
- Allocate buffers and resources
- Initialize hardware peripherals
- Do NOT start RTOS tasks here (use `startService()` instead)
- Log initialization status using the logger

**Example**:
```cpp
bool MyService::initializeService() {
    if (logger) {
        logger->log("Initializing MyService...");
    }
    
    // Load settings
    if (!loadSettings()) {
        if (logger) logger->log("Failed to load settings");
        return false;
    }
    
    // Initialize hardware
    if (!setupHardware()) {
        if (logger) logger->log("Hardware initialization failed");
        return false;
    }
    
    if (logger) logger->log("MyService initialized successfully");
    return true;
}
```

---

#### `startService()`
```cpp
virtual bool startService() = 0;
```
**Purpose**: Starts the service and begins normal operation.

**Returns**: 
- `true` if service started successfully
- `false` if service failed to start

**Best Practices**:
- Create RTOS tasks if needed (but avoid creating new tasks per architectural guidelines)
- Start background processes
- Begin listening for events or requests
- Service should be fully operational after this call

**Important**: Ensure `initializeService()` has been called successfully before calling this method.

**Example**:
```cpp
bool MyService::startService() {
    if (!initialized) {
        if (logger) logger->log("Cannot start uninitialized service");
        return false;
    }
    
    // Start operation
    isRunning = true;
    
    if (logger) logger->log("MyService started");
    return true;
}
```

---

#### `stopService()`
```cpp
virtual bool stopService() = 0;
```
**Purpose**: Gracefully stops the service and releases resources.

**Returns**: 
- `true` if service stopped successfully
- `false` if service failed to stop cleanly

**Best Practices**:
- Stop background tasks
- Close network connections
- Release hardware resources
- Ensure graceful shutdown to avoid memory leaks
- Log shutdown status

**Example**:
```cpp
bool MyService::stopService() {
    if (!isRunning) {
        return true;
    }
    
    isRunning = false;
    
    // Clean up resources
    closeConnections();
    releaseHardware();
    
    if (logger) logger->log("MyService stopped");
    return true;
}
```

---

### Configuration Methods

#### `saveSettings()`
```cpp
virtual bool saveSettings() { return true; }
```
**Purpose**: Persists service configuration to non-volatile storage.

**Returns**: 
- `true` if settings were saved successfully
- `false` if save operation failed

**Default Implementation**: Returns `true` (no-op)

**Override When**: Your service has configurable parameters that should persist across reboots.

**Example**:
```cpp
bool MyService::saveSettings() {
    SettingsService* settings = SettingsService::getInstance();
    
    settings->setInt("myservice_port", port);
    settings->setString("myservice_mode", mode.c_str());
    
    if (logger) logger->log("Settings saved");
    return true;
}
```

---

#### `loadSettings()`
```cpp
virtual bool loadSettings() { return true; }
```
**Purpose**: Loads service configuration from persistent storage.

**Returns**: 
- `true` if settings were loaded successfully
- `false` if load operation failed

**Default Implementation**: Returns `true` (no-op)

**Best Practices**:
- Call this during `initializeService()`
- Provide sensible defaults if settings don't exist
- Validate loaded values

**Example**:
```cpp
bool MyService::loadSettings() {
    SettingsService* settings = SettingsService::getInstance();
    
    port = settings->getInt("myservice_port", 8080); // 8080 is default
    mode = settings->getString("myservice_mode", "auto");
    
    if (logger) logger->log("Settings loaded");
    return true;
}
```

---

### Optional Features

#### `asOpenAPIInterface()`
```cpp
virtual IsOpenAPIInterface* asOpenAPIInterface() { return nullptr; }
```
**Purpose**: Returns a pointer to the OpenAPI interface if the service exposes HTTP endpoints.

**Returns**: 
- Pointer to `IsOpenAPIInterface` if the service supports OpenAPI
- `nullptr` if the service does not expose HTTP endpoints

**Default Implementation**: Returns `nullptr`

**Override When**: Your service needs to expose REST API endpoints through the HTTP service.

**Example**:
```cpp
class MyService : public IsServiceInterface, public IsOpenAPIInterface {
public:
    IsOpenAPIInterface* asOpenAPIInterface() override {
        return this;
    }
    
    // Implement IsOpenAPIInterface methods...
    void registerRoutes(WebServer* server) override {
        // Register HTTP endpoints
    }
};
```

---

### Logger Integration

#### `setLogger()`
```cpp
bool setLogger(RollingLogger* rollingLogger)
```
**Purpose**: Sets the logger instance for the service.

**Parameters**:
- `rollingLogger`: Pointer to the logger instance

**Returns**: 
- `true` if logger was set successfully
- `false` if logger pointer is null

**Best Practices**:
- Always call this before `initializeService()`
- Check if logger is available before logging: `if (logger) logger->log(...)`
- Use logger instead of `Serial.print()` for debug output

---

## Implementation Guide

### Step 1: Declare Your Service Class

```cpp
#include "IsServiceInterface.h"

class MyService : public IsServiceInterface {
public:
    // Implement required methods
    std::string getServiceName() override;
    bool initializeService() override;
    bool startService() override;
    bool stopService() override;
    
    // Override optional methods if needed
    bool saveSettings() override;
    bool loadSettings() override;
    
    // Optionally implement OpenAPI interface
    IsOpenAPIInterface* asOpenAPIInterface() override;
    
private:
    bool isRunning = false;
    // Your service-specific members
};
```

### Step 2: Implement Core Methods

```cpp
std::string MyService::getServiceName() {
    return "My Service";
}

bool MyService::initializeService() {
    // Load configuration
    loadSettings();
    
    // Initialize resources
    // ...
    
    return true;
}

bool MyService::startService() {
    if (!isRunning) {
        isRunning = true;
        // Start operations
    }
    return true;
}

bool MyService::stopService() {
    if (isRunning) {
        isRunning = false;
        // Clean up
    }
    return true;
}
```

### Step 3: Use Your Service

```cpp
// Create and setup service
MyService* service = new MyService();
RollingLogger* logger = RollingLogger::getInstance();

// Set logger
service->setLogger(logger);

// Initialize
if (!service->initializeService()) {
    // Handle initialization failure
}

// Start
if (!service->startService()) {
    // Handle start failure
}

// Later, stop service
service->stopService();
```

---

## Service Lifecycle

```
┌─────────────┐
│   Created   │
└──────┬──────┘
       │
       │ setLogger()
       ▼
┌─────────────┐
│   Logger    │
│     Set     │
└──────┬──────┘
       │
       │ initializeService()
       ▼
┌─────────────┐
│ Initialized │
└──────┬──────┘
       │
       │ startService()
       ▼
┌─────────────┐
│   Running   │
└──────┬──────┘
       │
       │ stopService()
       ▼
┌─────────────┐
│   Stopped   │
└─────────────┘
```

---

## Architectural Guidelines

### DO:
- ✅ Keep services modular and loosely coupled
- ✅ Use logger for all debug output
- ✅ Implement graceful shutdown in `stopService()`
- ✅ Load/save settings through `SettingsService`
- ✅ Use `constexpr` for constants
- ✅ Validate inputs and return appropriate status
- ✅ Log important state changes

### DON'T:
- ❌ Don't create new RTOS tasks without architectural review
- ❌ Don't use `Serial.print()` - use logger instead
- ❌ Don't use `#define` for constants - use `constexpr`
- ❌ Don't allocate memory in ISR paths
- ❌ Don't ignore return values from lifecycle methods

---

## Example Services

Refer to existing service implementations:
- [WiFiService](src/services/WiFiService.h) - Network connectivity
- [HTTPService](src/services/HTTPService.h) - Web server with OpenAPI
- [ServoService](src/services/ServoService.h) - Servo motor control with OpenAPI
- [WebcamService](src/services/WebcamService.h) - Camera frame streaming
- [UDPService](src/services/UDPService.h) - UDP communication

---

## Related Interfaces

### [IsOpenAPIInterface](IsOpenAPIInterface.md)
Services that expose HTTP endpoints should also implement `IsOpenAPIInterface` and return `this` from `asOpenAPIInterface()`.

**See**: [IsOpenAPIInterface.h](src/services/IsOpenAPIInterface.h) for details on implementing REST API endpoints.

---

## Memory Considerations

The K10 bot runs on ESP32-S3 with limited memory. When implementing services:

- Prefer `PROGMEM` for static data
- Reuse buffers when possible
- Avoid dynamic allocation in critical paths
- Minimize memory copies (especially for camera frames)
- Clean up resources in `stopService()`

---

## Real-Time Constraints

Services must be aware of FreeRTOS real-time requirements:

- Avoid blocking operations in time-critical paths
- Use appropriate task priorities
- Be mindful of ISR context
- UDP service runs on dedicated core for responsiveness

---

## Thread Safety

If your service is accessed from multiple RTOS tasks:

- Protect shared resources with mutexes
- Document thread-safety guarantees
- Be aware of priority inversion
- Keep critical sections short


