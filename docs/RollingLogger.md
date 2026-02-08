# RollingLogger Documentation

## Overview

`RollingLogger` is a logging utility designed for the K10 bot project that provides structured logging with multiple severity levels. It maintains a rolling buffer of log messages that can be retrieved and displayed by UI components.

**Location**: [src/services/RollingLogger.h](src/services/RollingLogger.h), [src/services/RollingLogger.cpp](src/services/RollingLogger.cpp)

## Purpose

The logger serves as a centralized logging facility:
1. **Structured logging** - Provides leveled logging (TRACE, DEBUG, INFO, WARNING, ERROR) for debugging and monitoring
2. **Message buffering** - Maintains a rolling buffer of log entries with automatic memory management
3. **Separation of concerns** - Log storage is separate from display rendering, allowing flexible UI integration

## Architecture

The logging system follows a clean separation of concerns:
- **RollingLogger** - Handles log message storage and buffering
- **UTB2026 UI** - Renders logs to the TFT display (see [src/ui/utb2026.h](src/ui/utb2026.h))

This separation allows:
- Logging without screen dependency
- Multiple views of the same log buffer
- Alternative output methods (file, network) in the future
- Better testability and maintainability

## Key Features

- ✅ Multiple log levels with filtering (TRACE, DEBUG, INFO, WARNING, ERROR)
- ✅ Automatic rolling buffer (prevents memory overflow)
- ✅ Convenience methods for each log level
- ✅ Support for both `std::string` and PROGMEM strings (`__FlashStringHelper*`)
- ✅ Memory-bounded operation (automatic trimming)
- ✅ Thread-safe log retrieval via `get_log_rows()`
- ✅ Clean separation from UI rendering
- ✅ Lightweight and efficient

---

## Log Levels

```cpp
enum LogLevel {
    TRACE = 4,      // Most verbose - trace execution flow
    DEBUG = 3,      // Debug information
    INFO = 2,       // General information (default)
    WARNING = 1,    // Warning messages
    ERROR = 0       // Error messages (always shown)
};
```

**Filtering**: Only messages at or below the current log level are displayed.

**Example**: If log level is set to `INFO`, only INFO, WARNING, and ERROR messages will be shown. DEBUG and TRACE messages will be filtered out.

---

## Constructor

```cpp
RollingLogger();
```

**Default Configuration**:
- Log level: `DEBUG`
- Max rows: `16`

**Note**: Display configuration (viewport, colors) is now handled by the UI component when adding logger views.

**Usage**:
```cpp
RollingLogger* logger = new RollingLogger();
```

---

## Logging Methods

### `log()`
```cpp
void log(const std::string message, const LogLevel level);
void log(const __FlashStringHelper* message, const LogLevel level);
```
**Purpose**: Log a message with a specific severity level.

**Parameters**:
- `message`: The log message to record (either `std::string` or PROGMEM string)
- `level`: Severity level (TRACE, DEBUG, INFO, WARNING, ERROR)

**Behavior**:
- Filters messages based on current log level
- Adds message to rolling buffer
- Automatically trims buffer if it exceeds max_rows
- Does NOT trigger screen rendering (handled separately by UI)

**Example**:
```cpp
// Using std::string
logger->log("Connection established", RollingLogger::INFO);

// Using PROGMEM string
logger->log(FPSTR(MyConsts::msg_error), RollingLogger::ERROR);
```

---

### Convenience Methods

#### `trace()`
```cpp
void trace(const std::string message);
void trace(const __FlashStringHelper* message);
```
**Purpose**: Log a trace-level message (most verbose).

**Usage**: Track detailed execution flow, function entry/exit.

**Example**:
```cpp
logger->trace("Entering initializeService()");
logger->trace(FPSTR(ServiceConsts::msg_trace_entry));
```

---

#### `debug()`
```cpp
void debug(const std::string message);
void debug(const __FlashStringHelper* message);
```
**Purpose**: Log debug information.

**Usage**: Development and troubleshooting information.

**Example**:
```cpp
logger->debug("Received UDP packet: 128 bytes");
logger->debug(FPSTR(ServiceInterfaceConsts::msg_initialize_done));
```

---

#### `info()`
```cpp
void info(const std::string message);
void info(const __FlashStringHelper* message);
```
**Purpose**: Log general informational messages.

**Usage**: Normal operational messages, status updates.

**Example**:
```cpp
logger->info("WiFi connected to network");
logger->info(FPSTR(WiFiConsts::msg_connected));
```

---

#### `warning()`
```cpp
void warning(const std::string message);
void warning(const __FlashStringHelper* message);
```
**Purpose**: Log warning messages.

**Usage**: Potentially problematic situations that don't prevent operation.

**Example**:
```cpp
logger->warning("Low memory: 10KB free");
logger->warning(FPSTR(SystemConsts::msg_low_memory));
```

---

#### `error()`
```cpp
void error(const std::string message);
void error(const __FlashStringHelper* message);
```
**Purpose**: Log error messages.

**Usage**: Failures and critical issues.

**Example**:
```cpp
logger->error("Failed to connect to WiFi");
logger->error(FPSTR(WiFiConsts::msg_connection_failed));
```

---

## Configuration Methods

### `set_log_level()`
```cpp
void set_log_level(const LogLevel level);
```
**Purpose**: Set the minimum log level to display.

**Parameters**:
- `level`: Minimum severity level to show

**Behavior**: Messages below this level will be filtered out.

**Example**:
```cpp
// Show all messages including debug
logger->set_log_level(RollingLogger::DEBUG);

// Show only warnings and errors
logger->set_log_level(RollingLogger::WARNING);

// Production mode - only errors
logger->set_log_level(RollingLogger::ERROR);
```

---

### `get_log_level()`
```cpp
LogLevel get_log_level();
```
**Purpose**: Get the current log level filter.

**Returns**: Current `LogLevel`

**Example**:
```cpp
RollingLogger::LogLevel currentLevel = logger->get_log_level();
if (currentLevel == RollingLogger::DEBUG) {
    // Debug mode is enabled
}
```

---

### `set_max_rows()`
```cpp
void set_max_rows(int rows);
```
**Purpose**: Set the maximum number of log rows to retain in memory.

**Parameters**:
- `rows`: Maximum number of log entries (must be > 0)

**Behavior**:
- Limits memory usage by trimming old messages
- Automatically trims existing buffer if it exceeds new limit

**Example**:
```cpp
// Increase buffer for more history
logger->set_max_rows(32);

// Reduce buffer to save memory
logger->set_max_rows(8);
```

---

### `get_max_rows()`
```cpp
int get_max_rows();
```
**Purpose**: Get the current maximum row limit.

**Returns**: Maximum number of log entries

**Example**:
```cpp
int maxRows = logger->get_max_rows();
```

---

### `get_log_rows()`
```cpp
const std::vector<std::pair<LogLevel, std::string>>& get_log_rows() const;
```
**Purpose**: Retrieve all log entries for display or processing.

**Returns**: Const reference to vector of log entries (pairs of LogLevel and message string)

**Usage**: Called by UI components to render logs. The returned vector contains entries in chronological order.

**Example**:
```cpp
const auto& log_rows = logger->get_log_rows();
for (const auto& entry : log_rows) {
    RollingLogger::LogLevel level = entry.first;
    const std::string& message = entry.second;
    // Process or display the log entry
}
```

**Note**: This method is typically called by the UI layer (UTB2026) during rendering cycles.

---

## Usage Patterns

### Basic Setup

```cpp
#include "RollingLogger.h"
#include "ui/utb2026.h"

// Create logger
RollingLogger* logger = new RollingLogger();

// Configure logging behavior
logger->set_log_level(RollingLogger::DEBUG);
logger->set_max_rows(20);

// Create UI and add logger view for display
UTB2026 ui;
ui.init();
ui.add_logger_view(logger, 0, 80, 320, 240, TFT_BLACK, TFT_BLACK);

// Start logging
logger->info("Logger initialized");

// Later, in your main loop:
ui.draw_logger();  // Renders logs to screen
```

---

### Service Integration

All services in the K10 bot receive a logger instance via `setLogger()`:

```cpp
class MyService : public IsServiceInterface {
public:
    bool initializeService() override {
        logger->info("Initializing MyService");
        
        // Initialization code...
        
        service_status_ = STARTED;
        status_timestamp_ = millis();
        #ifdef VERBOSE_DEBUG
        logger->debug(getServiceName() + " " + FPSTR(ServiceInterfaceConsts::msg_initialize_done));
        #endif
        return true;
    }
    
    bool someOperation() {
        if (service_status_ != STARTED) {
            logger->error("Service not initialized");
            return false;
        }
        
        #ifdef VERBOSE_DEBUG
        logger->debug("Performing operation");
        #endif
        
        // Operation code...
        
        logger->info("Operation completed");
        return true;
    }
};
```

**Note**: After `setLogger()` is called, the logger is always available - no need to check for null.

---

### Using PROGMEM Strings

To save RAM, use PROGMEM strings with the `FPSTR()` macro:

```cpp
namespace MyServiceConsts {
    constexpr const char msg_initialized[] PROGMEM = "Service initialized";
    constexpr const char msg_error[] PROGMEM = "Operation failed";
}

// In your code:
logger->info(FPSTR(MyServiceConsts::msg_initialized));
logger->error(FPSTR(MyServiceConsts::msg_error));

// Combining with dynamic data:
logger->debug("Port: " + std::to_string(port) + ", " + FPSTR(MyServiceConsts::msg_status));
```

---

### Development vs Production

```cpp
void setupLogger(RollingLogger* logger, bool isProduction) {
    if (isProduction) {
        // Production: only errors
        logger->set_log_level(RollingLogger::ERROR);
        logger->set_max_rows(8); // Minimal memory
    } else {
        // Development: verbose
        logger->set_log_level(RollingLogger::DEBUG);
        logger->set_max_rows(32); // More history
    }
}
```

---

### String Formatting

Build formatted messages using standard C++ string operations:

```cpp
// Using std::string
std::string message = "WiFi RSSI: " + std::to_string(rssi) + " dBm";
logger->info(message);

// Using snprintf (more memory-efficient)
char buffer[80];
snprintf(buffer, sizeof(buffer), "Frame %d: %dx%d, %d bytes", 
         frameNum, width, height, size);
logger->debug(buffer);

// Simple concatenation
logger->info("Service: " + serviceName + " started");
```

---

## Memory Considerations

The logger is designed for memory-constrained ESP32-S3:

### Buffer Management
- Rolling buffer automatically trims old messages
- Maximum memory: `max_rows * (message_length + sizeof(LogLevel) + std::pair overhead)`
- Typical usage: 16 rows × ~50 chars = ~1-2KB
- PROGMEM strings save RAM by storing constants in flash

### Optimization Tips
```cpp
// ✅ Good - bounded buffer
logger->set_max_rows(16);

// ✅ Best - use PROGMEM strings
namespace MyConsts {
    constexpr const char msg_status[] PROGMEM = "Status update";
}
logger->info(FPSTR(MyConsts::msg_status));

// ✅ Good - reuse buffer for formatting
char buffer[64];
for (int i = 0; i < count; i++) {
    snprintf(buffer, sizeof(buffer), "Item %d", i);
    logger->debug(buffer);
}

// ❌ Bad - unbounded string building
std::string msg;
for (int i = 0; i < 1000; i++) {
    msg += std::to_string(i) + " "; // Grows uncontrolled
}
logger->debug(msg);

// ✅ Good - filter verbose logs in production
#ifdef VERBOSE_DEBUG
    logger->debug("Detailed debug info");
#endLogging Cost
- `log()` calls are lightweight - only add to buffer
- No screen rendering overhead during logging
- Rendering happens separately in UI update cycle
- **Important**: Still avoid logging in ISR contexts (thread safety)

### Best Practices
```cpp
// ✅ Good - log important state changes
logger->info("WiFi connected");
logger->info(FPSTR(ServiceConsts::msg_initialized));

// ✅ Good - periodic logging
static unsigned long lastLog = 0;
if (millis() - lastLog > 1000) {
    logger->debug("Frame rate: " + std::to_string(fps));
    lastLog = millis();
}

// ⚠️ Acceptable - logging in loops (no rendering overhead)
// But still use judiciously to avoid buffer churn
for (int i = 0; i < 100; i++) {
    logger->debug("Processing " + std::to_string(i));
}

// ❌ Bad - logging in ISR (thread safety issue)
void IRAM_ATTR onInterrupt() {
    logger->error("Interrupt!"); // DON'T do this!
}

// UI rendering (separate from logging):
ui.draw_logger();  // Call periodically in main loop (e.g., every 100-500ms)or (int i = 0; i < 1000; i++) {
    logger->debug("Processing " + std::to_string(i)); // 1000 screen refreshes!
}

// ❌ Bad - logging in ISR
void IRAM_ATTR onInterrupt() {
    logger->error("Interrupt!"); // DON'T do this!
}
```

---

## Debugging Tips

### Check Log Level
```cpp
if (logger->get_log_level() >= RollingLogger::DEBUG) {
    // Debug mode - can do expensive debug operations
    std::string detailedInfo = generateDetailedDebugInfo();
    logger->debug(detailedInfo);
}
```

### Trace Function Entry/Exit
```cpp
void complexFunction() {
    if (logger) logger->trace("-> complexFunction");
    
    // Function body...
    
    if (logger) logger->trace("<- complexFunction");
}
```

### Track State Transitions
```cpp
void setState(State newState) {
    if (logger) {
        logger->debug("State transition: " + 
                     stateToString(currentState) + " -> " + 
                     stateToString(newState));
    }
    currentState = newState;
}
```

---

## Display Integration

### Architecture
Display rendering is handled by the **UTB2026** UI class, which:
- Retrieves log entries via `get_log_rows()`
- Manages viewports and colors
- Handles automatic color-coding by log level
- Supports multiple logger views simultaneously

### Adding Logger to UI
```cpp
#include "ui/utb2026.h"

UTB2026 ui;
RollingLogger* logger = new RollingLogger();

// Add logger view with viewport and colors
// add_logger_view(logger, x1, y1, x2, y2, text_color, bg_color)
ui.add_logger_view(logger, 0, 80, 320, 240, TFT_BLACK, TFT_BLACK);

// In main loop:
ui.draw_logger();  // Renders all logger views
```

### Automatic Color-Coding

When text color equals background color (e.g., both `TFT_BLACK`), messages are automatically color-coded:

| Level   | Color       | Constant (UTB2026Consts) |
|---------|-------------|---------------------------|
| ERROR   | Yellow      | `color_error`             |
| WARNING | Yellow      | `color_warning`           |
| INFO    | White       | `color_info`              |
| DEBUG   | Light Grey  | `color_debug`             |
| TRACE   | Light Grey  | `color_trace`             |

**Note**: Use different text and background colors to override automatic coloring.

### Layout Planning
```cpp
// Example: Split screen between UI and logs
constexpr int UI_HEIGHT = 80;
constexpr int SCREEN_WIDTH = 320;
constexpr int SCREEN_HEIGHT = 240;

// Logs use bottom portion
ui.add_logger_view(logger, 0, UI_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT);

// Multiple logger views are supported:
ui.add_logger_view(systemLogger, 0, 0, 160, 80);   // Top-left
ui.add_logger_view(debugLogger, 160, 0, 320, 80);  // Top-right
```

### Character Metrics
- Character height: `10 pixels`
- Typical character width: `6 pixels` (font-dependent)
- Max characters per line (320px width): ~53 characters

### Requirements
- UI component depends on `TFT_eSPI` library
- Logger itself has no display dependencies
- See [src/ui/utb2026.h](src/ui/utb2026.h) for UI implementation details

---

## Completed Improvements

✅ **Separation of Concerns** - Log handling and screen rendering are now separate:
- `RollingLogger` - Pure logging functionality
- `UTB2026` UI class - Display rendering
- Allows logging without screen dependency
- Enables alternative outputs (file, network) in future
- Better testability and maintainability

## Current Limitations

- No log persistence (lost on reboot)
- No log rotation or file output
- No filtering by source/category
- Not thread-safe (don't call from ISRs)

## Potential Future Improvements

- Log persistence to flash/SD card
- Network logging (syslog, remote server)
- Log filtering by component/tag
- Thread-safe implementation with mutexes
- Circular buffer optimization
#include "ui/utb2026.h"

// Global instances
RollingLogger* globalLogger = nullptr;
UTB2026 ui;

void setupLogging() {
    // Create logger
    globalLogger = new RollingLogger();
    
    // Configure for development
    globalLogger->set_log_level(RollingLogger::DEBUG);
    globalLogger->set_max_rows(20);
    
    // Initialize UI
    ui.init();
    
    // Add logger view at bottom of screen with auto color-coding
    ui.add_logger_view(globalLogger, 0, 80, 320, 240, TFT_BLACK, TFT_BLACK);
    
    // Log initialization
    globalLogger->info("====================");
    globalLogger->info("K10 Bot Starting");
    globalLogger->info("====================");
}

void setupServices() {
    // Create services and assign logger
    WiFiService* wifiService = new WiFiService();
    wifiService->setLogger(globalLogger);
    
    HTTPService* httpService = new HTTPService();
    httpService->setLogger(globalLogger);
    
    // Initialize services
    if (!wifiService->initializeService()) {
        globalLogger->error("WiFi init failed");
    }
    
    if (!httpService->initializeService()) {
        globalLogger->error("HTTP init failed");
    }
    
    // Start services
    wifiService->startService();
    httpService->startService();
}

void loop() {
    // Your main loop code...
    
    // Update display periodically (not every loop iteration)
    static unsigned long lastUIUpdate = 0;
    if (millis() - lastUIUpdate > 200) {  // Update 5 times per second
        ui.draw_all();  // or ui.draw_logger() for just logs
        lastUIUpdate = millis();
- PROGMEM usage for constants

### Design Patterns
- **Separation of Concerns**: Logger handles storage, UI handles rendering
- **Single Responsibility**: RollingLogger focused on log management only
- **Dependency Injection**: Services receive logger via `setLogger()`
- **Observer Pattern**: UI observes log buffer via `get_log_rows()`

### Real-Time Considerations
- Logging is **not** real-time safe
- **Never** call from ISR contexts
- No mutex protection (services run in same task context)
- Rendering is decoupled from logging (no blocking)
- Use `VERBOSE_DEBUG` guards for non-critical debug messages

### Integration with IsServiceInterface
All services implementing `IsServiceInterface` have access to the logger via the protected `logger` member after calling `setLogger()`.

### Integration with UTB2026 UI
The UI component (`UTB2026`) handles all display aspects:
- Multiple logger views supported
- Configurable viewports and colors
- Automatic color-coding by log level
- [src/ui/utb2026.h](src/ui/utb2026.h) - UI component that renders logs
- [src/services/FlashStringHelper.h](src/services/FlashStringHelper.h) - Helper for PROGMEM string handling

---

## Questions?

The RollingLogger is used throughout all services in the K10 bot project. Key files to reference:
- Service implementations: [src/services/](src/services/)
- UI rendering: [src/ui/utb2026.cpp](src/ui/utb2026.cpp)
- Logger implementation: [src/services/RollingLogger.cpp](src/services/RollingLogger.cpp)

## Architectural Notes

### GNU C Coding Style
The implementation follows GNU C style as specified in project conventions:
- Indentation and bracing style
- Naming conventions (snake_case for methods)
- Comment style

### Real-Time Considerations
- Logging is **not** real-time safe
- **Never** call from ISR contexts
- Keep messages concise to minimize render time
- Consider disabling verbose logging in time-critical sections

### Integration with IsServiceInterface
All services implementing `IsServiceInterface` have access to the logger via the protected `logger` member after calling `setLogger()`.

---

## Related Documentation

- [IsServiceInterface.md](IsServiceInterface.md) - Service interface with logger integration
- [IsOpenAPIInterface.md](IsOpenAPIInterface.md) - OpenAPI services that use logger

---

## Questions?

The RollingLogger is used throughout all services in the K10 bot project. Refer to existing service implementations in [src/services/](src/services/) for practical examples of logging usage.
