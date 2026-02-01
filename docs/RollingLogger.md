# RollingLogger Documentation

## Overview

`RollingLogger` is a logging utility designed for the K10 bot project that provides structured logging with multiple severity levels and automatic on-screen rendering to the TFT display. It maintains a rolling buffer of log messages **and displays** them in a configurable viewport on the screen.

**Location**: [src/services/RollingLogger.h](src/services/RollingLogger.h), [src/services/RollingLogger.cpp](src/services/RollingLogger.cpp)

## Purpose

The logger serves two main purposes:
1. **Structured logging** - Provides leveled logging (TRACE, DEBUG, INFO, WARNING, ERROR) for debugging and monitoring
2. **Visual feedback** - Displays log messages on the TFT screen for real-time system monitoring without requiring a serial connection

## Key Features

- ✅ Multiple log levels with filtering
- ✅ Automatic rolling buffer (prevents memory overflow)
- ✅ On-screen rendering to TFT display
- ✅ Configurable viewport and colors
- ✅ Convenience methods for each log level
- ✅ Color-coded messages by severity
- ✅ Memory-bounded operation (automatic trimming)

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
- Log level: `INFO`
- Max rows: `16`
- Viewport: `(0, 0)` with size `320x160` pixels
- Text color: `TFT_BLACK`
- Background color: `TFT_BLACK`

**Usage**:
```cpp
RollingLogger* logger = new RollingLogger();
```

---

## Logging Methods

### `log()`
```cpp
void log(const std::string message, const LogLevel level);
```
**Purpose**: Log a message with a specific severity level.

**Parameters**:
- `message`: The log message to record
- `level`: Severity level (TRACE, DEBUG, INFO, WARNING, ERROR)

**Behavior**:
- Filters messages based on current log level
- Adds message to rolling buffer
- Automatically trims buffer if it exceeds max_rows
- Renders updated logs to screen

**Example**:
```cpp
logger->log("Connection established", RollingLogger::INFO);
logger->log("Invalid parameter value", RollingLogger::ERROR);
```

---

### Convenience Methods

#### `trace()`
```cpp
void trace(const std::string message);
```
**Purpose**: Log a trace-level message (most verbose).

**Usage**: Track detailed execution flow, function entry/exit.

**Example**:
```cpp
logger->trace("Entering initializeService()");
logger->trace("Processing frame buffer");
```

---

#### `debug()`
```cpp
void debug(const std::string message);
```
**Purpose**: Log debug information.

**Usage**: Development and troubleshooting information.

**Example**:
```cpp
logger->debug("Received UDP packet: 128 bytes");
logger->debug("WiFi signal strength: -45 dBm");
```

---

#### `info()`
```cpp
void info(const std::string message);
```
**Purpose**: Log general informational messages.

**Usage**: Normal operational messages, status updates.

**Example**:
```cpp
logger->info("WiFi connected to network");
logger->info("HTTP server started on port 80");
```

---

#### `warning()`
```cpp
void warning(const std::string message);
```
**Purpose**: Log warning messages.

**Usage**: Potentially problematic situations that don't prevent operation.

**Example**:
```cpp
logger->warning("Low memory: 10KB free");
logger->warning("Retrying connection attempt 3/5");
```

---

#### `error()`
```cpp
void error(const std::string message);
```
**Purpose**: Log error messages.

**Usage**: Failures and critical issues.

**Example**:
```cpp
logger->error("Failed to connect to WiFi");
logger->error("Camera initialization failed");
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
- Re-renders logs after trimming

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

### `set_logger_viewport()`
```cpp
void set_logger_viewport(int x, int y, int width, int height);
```
**Purpose**: Configure the screen area where logs are displayed.

**Parameters**:
- `x`: Left edge of viewport (pixels)
- `y`: Top edge of viewport (pixels)
- `width`: Viewport width (pixels)
- `height`: Viewport height (pixels)

**Default**: `(0, 0, 320, 160)` - top-left corner, standard size

**Example**:
```cpp
// Position logs at bottom of screen
logger->set_logger_viewport(0, 80, 320, 160);

// Narrow viewport on right side
logger->set_logger_viewport(240, 0, 80, 240);

// Full screen
logger->set_logger_viewport(0, 0, 320, 240);
```

---

### `set_logger_text_color()`
```cpp
void set_logger_text_color(uint16_t color, uint16_t bg_color);
```
**Purpose**: Set text and background colors for log display.

**Parameters**:
- `color`: Text color (16-bit RGB565 format)
- `bg_color`: Background color (16-bit RGB565 format)

**Behavior**:
- If `color == bg_color`, automatic color-coding by level is used
- Otherwise, specified color is used for all messages

**Color Constants** (from TFT_eSPI):
- `TFT_BLACK` - 0x0000
- `TFT_WHITE` - 0xFFFF
- `TFT_RED` - 0xF800
- `TFT_GREEN` - 0x07E0
- `TFT_BLUE` - 0x001F
- `TFT_YELLOW` - 0xFFE0
- `TFT_LIGHTGREY` - 0xC618

**Example**:
```cpp
// White text on black background
logger->set_logger_text_color(TFT_WHITE, TFT_BLACK);

// Auto color-coding (text color = bg color triggers this mode)
logger->set_logger_text_color(TFT_BLACK, TFT_BLACK);

// Green text on dark background
logger->set_logger_text_color(TFT_GREEN, TFT_DARKGREY);
```

---

## Automatic Color-Coding

When text color equals background color (default mode), messages are automatically color-coded by severity:

| Level   | Color       | TFT Constant     |
|---------|-------------|------------------|
| ERROR   | Red         | `TFT_RED`        |
| WARNING | Yellow      | `TFT_YELLOW`     |
| INFO    | White       | `TFT_WHITE`      |
| DEBUG   | Light Grey  | `TFT_LIGHTGREY`  |
| TRACE   | Light Grey  | `TFT_LIGHTGREY`  |

---

## Usage Patterns

### Basic Setup

```cpp
#include "RollingLogger.h"

// Create logger
RollingLogger* logger = new RollingLogger();

// Configure
logger->set_log_level(RollingLogger::DEBUG);
logger->set_max_rows(16);
logger->set_logger_viewport(0, 0, 320, 160);

// Start logging
logger->info("Logger initialized");
```

---

### Service Integration

All services in the K10 bot receive a logger instance via `setLogger()`:

```cpp
class MyService : public IsServiceInterface {
public:
    bool initializeService() override {
        if (logger) {
            logger->info("Initializing MyService");
        }
        
        // Initialization code...
        
        if (logger) {
            logger->info("MyService initialized successfully");
        }
        return true;
    }
    
    bool someOperation() {
        if (!initialized) {
            if (logger) logger->error("Service not initialized");
            return false;
        }
        
        if (logger) logger->debug("Performing operation");
        
        // Operation code...
        
        if (logger) logger->info("Operation completed");
        return true;
    }
};
```

---

### Conditional Logging

Always check if logger is available before using:

```cpp
if (logger) {
    logger->debug("Debug information");
}

// Or with formatted strings
if (logger) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Value: %d, Status: %s", value, status);
    logger->info(buffer);
}
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
- Maximum memory: `max_rows * (message_length + sizeof(LogLevel))`
- Typical usage: 16 rows × ~50 chars = ~1KB

### Optimization Tips
```cpp
// ✅ Good - bounded buffer
logger->set_max_rows(16);

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
#endif
```

---

## Performance Considerations

### Rendering Cost
- Each `log()` call triggers screen refresh
- Rendering takes ~5-20ms depending on message count
- **Important**: Don't log in tight loops or ISR contexts

### Best Practices
```cpp
// ✅ Good - log important state changes
logger->info("WiFi connected");
logger->info("Camera initialized");

// ✅ Good - periodic logging
static unsigned long lastLog = 0;
if (millis() - lastLog > 1000) {
    logger->debug("Frame rate: " + std::to_string(fps));
    lastLog = millis();
}

// ❌ Bad - logging in tight loop
for (int i = 0; i < 1000; i++) {
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

### Requirements
- Depends on `TFT_eSPI` library for screen rendering
- Requires global `tft` instance: `extern TFT_eSPI tft;`
- Uses viewport system for non-overlapping display regions

### Layout Planning
```cpp
// Example: Split screen between UI and logs
constexpr int UI_HEIGHT = 80;
constexpr int LOG_HEIGHT = 160;

// UI uses top portion
// Logs use bottom portion
logger->set_logger_viewport(0, UI_HEIGHT, 320, LOG_HEIGHT);
```

### Character Metrics
- Character height: `10 pixels` (defined as `CHAR_HEIGHT`)
- Typical character width: `6 pixels` (font-dependent)
- Max characters per line (320px width): ~53 characters

---

## Limitations and Future Improvements

### Current Limitations
- No log persistence (lost on reboot)
- No log rotation or file output
- Screen rendering is synchronous (blocks briefly)
- No filtering by source/category

### Planned Improvements (from TODO)
> @todo: Split the log handling and screen rendering into separate classes.

This would allow:
- Logging without screen dependency
- Async rendering
- Alternative outputs (file, network)
- Better separation of concerns

---

## Example: Complete Logger Setup

```cpp
#include "RollingLogger.h"

// Global logger instance
RollingLogger* globalLogger = nullptr;

void setupLogging() {
    // Create logger
    globalLogger = new RollingLogger();
    
    // Configure for development
    globalLogger->set_log_level(RollingLogger::DEBUG);
    globalLogger->set_max_rows(20);
    
    // Position at bottom of screen
    globalLogger->set_logger_viewport(0, 80, 320, 160);
    
    // Use auto color-coding
    globalLogger->set_logger_text_color(TFT_BLACK, TFT_BLACK);
    
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
```

---

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
