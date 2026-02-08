# Copilot Instructions for K10 Bot

## Project Context
- **Hardware**: UniHiker K10 board (ESP32-S3, dual-core)
- **Platform**: PlatformIO (activate with `source venv/bin/activate`, command: `pio`)
- **Framework**: Arduino + FreeRTOS, C++17, real-time constraints
- **Memory**: Low-memory device; optimize for RAM/flash usage

## Architecture Overview

### Core Design Pattern: Service-based Modular Architecture
All features are implemented as **services** inheriting from [IsServiceInterface.h](src/services/IsServiceInterface.h):
- **Lifecycle**: `initializeService()` → `startService()` → `stopService()`
- **Logging**: Each service gets a `RollingLogger` instance via `setLogger()`
- **Settings**: Optional `saveSettings()` / `loadSettings()` persistence
- **OpenAPI**: Services expose HTTP routes via [IsOpenAPIInterface.h](src/services/IsOpenAPIInterface.h)

**Example services**: `BoardInfoService`, `WebcamService`, `ServoService`, `UDPService`, `HTTPService`, `K10SensorsService`, `SettingsService`, `RollingLoggerService`, `MusicService`

### Service Initialization Pattern (from [main.cpp](src/main.cpp))
```cpp
bool start_service(IsServiceInterface &service) {
    service.setLogger(&debug_logger);           // Attach logger
    if (!service.initializeService()) return false;
    if (!service.startService()) return false;
    
    // Auto-register OpenAPI routes if implemented
    IsOpenAPIInterface *openapi = service.asOpenAPIInterface();
    if (openapi) {
        openapi->registerRoutes();
        http_service.registerOpenAPIService(openapi);
    }
    return true;
}
```

### FreeRTOS Task Architecture
**Fixed task allocation** (defined in [main.cpp](src/main.cpp) `setup()`):
- **Core 0** (real-time critical): `udp_svr_task` - UDP message handling
- **Core 1** (UI/HTTP): `display_task`, `http_svr_task` - Display updates and web server
- **DO NOT** introduce new RTOS tasks without explicit justification

### OpenAPI Route Registration Pattern
All HTTP routes follow this pattern (see [BoardInfoService.cpp](src/services/BoardInfoService.cpp)):
```cpp
bool MyService::registerRoutes() {
    std::string path = getPath("");  // e.g., "/api/myservice/v1"
    
    // 1. Define OpenAPI metadata
    OpenAPIRoute route(path.c_str(), RoutesConsts::method_get, 
                      "Route description", "Tag", false, {params}, {responses});
    registerOpenAPIRoute(route);
    
    // 2. Register WebServer handler
    webserver.on(path.c_str(), HTTP_GET, []() {
        JsonDocument doc;
        doc["data"] = "value";
        String output;
        serializeJson(doc, output);
        webserver.send(200, RoutesConsts::mime_json, output.c_str());
    });
    
    // 3. Register settings routes (optional)
    registerSettingsRoutes("Service Name", this);
    return true;
}
```

**CRITICAL**: After changing `registerRoutes()`, update [/data/static_openapi.json](data/static_openapi.json)

## Coding Conventions

### Memory & Performance
- **PROGMEM mandatory**: All string constants → `constexpr const char name[] PROGMEM = "value";`
- **Namespace constants**: Group related constants (e.g., `BoardInfoConsts`, `RoutesConsts`, `ServiceInterfaceConsts`)
- **Avoid heap allocation**: Especially in ISR paths; reuse static buffers (e.g., `messages[MAX_MESSAGES][MAX_MESSAGE_LEN]`)
- **Use common constants**: `RoutesConsts` for OpenAPI, `ServiceInterfaceConsts` for service messages

### Naming Conventions
- **Variables/constants**: `lowercase_with_underscores` with meaningful prefix (e.g., `msg_initialize_done`, `path_service`, `field_total`)
- **Functions**: camelCase (e.g., `getServiceName()`, `initializeService()`)
- **Constants**: `constexpr` over `#define`

### Code Style
- **GNU C style** formatting
- **Doxygen comments** for all functions:
  ```cpp
  /**
   * @brief Brief description
   * @param param_name Description
   * @return Description
   */
  ```
- **Logging**: Use `logger->debug()`, `logger->info()`, `logger->error()` (never `Serial.print()`)
- **Flash strings**: Use `FPSTR()` macro to access PROGMEM strings at runtime

### Common Patterns
```cpp
// Service constants namespace
namespace MyServiceConsts {
    constexpr const char str_service_name[] PROGMEM = "My Service";
    constexpr const char path_service[] PROGMEM = "myservice/v1";
}

// Service status tracking
protected:
    ServiceStatus service_status_ = UNINITIALIZED;
    unsigned long status_timestamp_ = millis();
```

## Development Workflows

### Build & Upload
```bash
source venv/bin/activate  # Activate PlatformIO environment
pio run                   # Build project
pio run --target upload   # Upload to device
pio run --target uploadfs # Upload filesystem (on changes to /data/)
pio device monitor        # View serial output (115200 baud)
```

### Testing
- Tests go under `test/` using Unity framework
- Enable with `test_build_src = yes` in `platformio.ini`
- **Note**: Test directory currently empty

### OpenAPI Development Cycle
1. Implement `IsOpenAPIInterface` in your service
2. Define routes in `registerRoutes()` with OpenAPI metadata
3. Test endpoints via web UI at `http://<device-ip>/`
4. Update [/data/static_openapi.json](data/static_openapi.json) manually
5. Verify docs at `http://<device-ip>/api/openapi.json`

### Debugging
- Use `#ifdef VERBOSE_DEBUG` for detailed logs (defined in [main.cpp](src/main.cpp))
- Access logs via: `RollingLoggerService` HTTP endpoint or web UI at `/logservice.html`
- LED indicators: RGB LEDs (0-2) on UniHiker show service initialization status

## Key Integration Points

### WebServer Registration
- Global `WebServer webserver(80)` in [main.cpp](src/main.cpp)
- Services call `webserver.on(path, HTTP_GET, handler)` in `registerRoutes()`
- HTTPService aggregates all routes via `registerOpenAPIService()`

### Logger System
- Two global loggers: `debug_logger` (verbose) and `app_info_logger` (user-facing)
- Services receive logger via `setLogger()` during `start_service()`
- Logs displayed on TFT screen via `UTB2026` UI manager

### External Dependencies
- **ArduinoJson v7.2.1**: JSON serialization (in `lib/`)
- **AsyncUDP**: UDP packet handling
- **TFT_eSPI**: Display driver
- **UNIHIKER_K10**: Hardware abstraction library
- **MicroTFLite**: TensorFlow Lite inference (optional)

## Forbidden Practices
- ❌ Dynamic casts (not supported in runtime)
- ❌ New RTOS tasks without justification
- ❌ Deleting files/folders without user confirmation
- ❌ `Serial.print()` for logging wihtout `#ifdef SERIAL_DEBUG`
- ❌ `#define` for constants (use `constexpr`)
- ❌ Heap allocation in interrupt handlers

## Key Files Reference
- [src/main.cpp](src/main.cpp) - Entry point, service initialization, RTOS tasks
- [src/services/IsServiceInterface.h](src/services/IsServiceInterface.h) - Service base interface
- [src/services/IsOpenAPIInterface.h](src/services/IsOpenAPIInterface.h) - OpenAPI route interface that extends IsServiceInterface
integration guide
- [platformio.ini](platformio.ini) - Build configuration

