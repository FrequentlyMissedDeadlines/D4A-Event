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
- Service status tracking with `ServiceStatus` enum
- Optional OpenAPI endpoint exposure (see [IsOpenAPIInterface.md](IsOpenAPIInterface.md))

## Core Structure

The `IsServiceInterface` is a **struct** (not a class) that provides:
- Pure virtual methods for lifecycle management
- Default implementations for optional features
- Protected logger member for debug output
- Standard message constants in `ServiceInterfaceConsts` namespace

### ServiceStatus Enum

```cpp
enum ServiceStatus { 
    UNINITIALIZED,        // Service not yet initialized
    INITIALIZED,          // Service initialized but not started
    INITIALIZED_FAILED,   // Initialization failed
    STARTED,              // Service is running
    START_FAILED,         // Service failed to start
    STOPPED,              // Service stopped gracefully
    STOP_FAILED           // Service failed to stop cleanly
};
```

**Best Practice**: Track service status using protected members:
```cpp
protected:
    ServiceStatus service_status_ = UNINITIALIZED;
    unsigned long status_timestamp_ = 0;  // Optional: Last status change time (millis)
```

### ServiceInterfaceConsts Namespace

Provides PROGMEM constants for common messages:

```cpp
namespace ServiceInterfaceConsts {
    constexpr const char msg_initialize_done[] PROGMEM = "initialize done";
    constexpr const char msg_initialize_failed[] PROGMEM = "initialize failed";
    constexpr const char msg_start_done[] PROGMEM = "start done";
    constexpr const char msg_start_failed[] PROGMEM = "start failed";
    constexpr const char msg_stop_done[] PROGMEM = "stop done";
    constexpr const char msg_stop_failed[] PROGMEM = "stop failed";
}
```

**Usage**: Use `FPSTR()` macro when logging these constants:
```cpp
logger->debug(getServiceName() + " " + FPSTR(ServiceInterfaceConsts::msg_initialize_done));
```

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

#### `getStatus()`
```cpp
ServiceStatus getStatus();
```
**Purpose**: Returns the current status of the service.

**Returns**: Current `ServiceStatus` enum value (UNINITIALIZED, INITIALIZED, STARTED, etc.)

**Usage**: Check service state before performing operations.

---

#### `isStarted()`
```cpp
bool isStarted() const;
```
**Purpose**: Check if the service is currently running.

**Returns**: 
- `true` if service status is `STARTED`
- `false` otherwise

**Usage**: Quick check if service is operational.

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
    #ifdef VERBOSE_DEBUG
    logger->debug(getServiceName() + " initializing...");
    #endif
    
    // Load settings
    if (!loadSettings()) {
        service_status_ = INITIALIZED_FAILED;
        status_timestamp_ = millis();
        logger->error("Failed to load settings");
        return false;
    }
    
    // Initialize hardware
    if (!setupHardware()) {
        service_status_ = INITIALIZED_FAILED;
        status_timestamp_ = millis();
        logger->error("Hardware initialization failed");
        return false;
    }
    
    service_status_ = INITIALIZED;
    status_timestamp_ = millis();
    #ifdef VERBOSE_DEBUG
    logger->debug(getServiceName() + " " + FPSTR(ServiceInterfaceConsts::msg_initialize_done));
    #endif
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
    if (service_status_ == INITIALIZED_FAILED) {
        logger->error("Cannot start uninitialized service");
        service_status_ = START_FAILED;
        status_timestamp_ = millis();
        return false;
    }
    
    // Start operation
    if (!performStartup()) {
        service_status_ = START_FAILED;
        status_timestamp_ = millis();
        return false;
    }
    
    service_status_ = STARTED;
    status_timestamp_ = millis();
    #ifdef VERBOSE_DEBUG
    logger->debug(getServiceName() + " " + FPSTR(ServiceInterfaceConsts::msg_start_done));
    #endif
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
    if (service_status_ == STOPPED) {
        return true;  // Already stopped
    }
    
    // Clean up resources
    if (!closeConnections() || !releaseHardware()) {
        service_status_ = STOP_FAILED;
        status_timestamp_ = millis();
        logger->error("Failed to stop service cleanly");
        return false;
    }
    
    service_status_ = STOPPED;
    status_timestamp_ = millis();
    #ifdef VERBOSE_DEBUG
    logger->debug(getServiceName() + " " + FPSTR(ServiceInterfaceConsts::msg_stop_done));
    #endif
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
    // SettingsService uses ESP32 Preferences API
    // Settings are automatically saved to NVS (non-volatile storage)
    
    // Services typically implement custom save logic here
    // For example, saving to JSON file or using SettingsService::setString/setInt
    
    #ifdef VERBOSE_DEBUG
    logger->debug(getServiceName() + " settings saved");
    #endif
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
    // Load configuration from persistent storage
    // This is typically called during initializeService()
    
    // Use defaults if settings don't exist
    port = 8080;  // default value
    mode = "auto";
    
    // Services can load from SettingsService or JSON files
    
    #ifdef VERBOSE_DEBUG
    logger->debug(getServiceName() + " settings loaded");
    #endif
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
- Logger is always available after `setLogger()` - no need to check for null
- Use logger instead of `Serial.print()` for debug output
- Use `VERBOSE_DEBUG` guards for debug-level logging
- Use appropriate log levels: `debug()`, `info()`, `warning()`, `error()`

**Example**:
```cpp
// In main.cpp or service initialization
RollingLogger* logger = RollingLogger::getInstance();
myService->setLogger(logger);

// In service methods
#ifdef VERBOSE_DEBUG
logger->debug("Debug information");
#endif
logger->info("Service started");
logger->warning("Configuration issue detected");
logger->error("Critical failure occurred");
```

---

## Implementation Guide

### Step 1: Declare Your Service Class

```cpp
#include "IsServiceInterface.h"
#include "IsOpenAPIInterface.h"  // If exposing HTTP endpoints

class MyService : public IsServiceInterface, public IsOpenAPIInterface {
public:
    // Implement required methods
    std::string getServiceName() override;
    bool initializeService() override;
    bool startService() override;
    bool stopService() override;
    
    // Override optional methods if needed
    bool saveSettings() override;
    bool loadSettings() override;
    
    // Implement OpenAPI interface if exposing HTTP endpoints
    IsOpenAPIInterface* asOpenAPIInterface() override { return this; }
    bool registerRoutes() override;
    std::string getServiceSubPath() override;
    
protected:
    ServiceStatus service_status_ = UNINITIALIZED;
    unsigned long status_timestamp_ = 0;
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
    if (!loadSettings()) {
        service_status_ = INIT_FAILED;
        status_timestamp_ = millis();
        return false;
    }
    
    // Initialize resources
    // ...
    
    service_status_ = STARTED;
    status_timestamp_ = millis();
    #ifdef VERBOSE_DEBUG
    logger->debug(getServiceName() + " " + FPSTR(ServiceInterfaceConsts::msg_initialize_done));
    #endif
    return true;
}

bool MyService::startService() {
    if (service_status_ == STARTED) {
        return true;  // Already started
    }
    
    // Start operations
    // ...
    
    service_status_ = STARTED;
    status_timestamp_ = millis();
    #ifdef VERBOSE_DEBUG
    logger->debug(getServiceName() + " " + FPSTR(ServiceInterfaceConsts::msg_start_done));
    #endif
    return true;
}

bool MyService::stopService() {
    if (service_status_ == STOPPED) {
        return true;  // Already stopped
    }
    
    // Clean up resources
    // ...
    
    service_status_ = STOPPED;
    status_timestamp_ = millis();
    #ifdef VERBOSE_DEBUG
    logger->debug(getServiceName() + " " + FPSTR(ServiceInterfaceConsts::msg_stop_done));
    #endif
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
- ✅ Use logger for all debug output (not `Serial.print()`)
- ✅ Track service state with `ServiceStatus` enum
- ✅ Update `status_timestamp_` on status changes
- ✅ Use `VERBOSE_DEBUG` guards for debug logging
- ✅ Use `FPSTR()` macro for PROGMEM constants
- ✅ Implement graceful shutdown in `stopService()`
- ✅ Use `constexpr` for constants in dedicated namespaces
- ✅ Store strings in PROGMEM to save RAM
- ✅ Use meaningful prefixes for constant names (lowercase with underscores)
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
- [BoardInfoService](src/services/BoardInfoService.h) - Simple service exposing system info via OpenAPI
- [WiFiService](src/services/WiFiService.h) - Network connectivity
- [HTTPService](src/services/HTTPService.h) - Web server with OpenAPI aggregation
- [ServoService](src/services/ServoService.h) - Servo motor control with OpenAPI
- [K10CamService](src/services/camera/K10CamService.h) - Camera frame streaming
- [UDPService](src/services/UDPService.h) - UDP communication
- [SettingsService](src/services/SettingsService.h) - Configuration persistence
- [DFR1216Service](src/services/DFR1216Service.h) - UniHiker expansion board interface
- [MusicService](src/services/MusicService.h) - Audio/buzzer control

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


