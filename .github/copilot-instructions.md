# Copilot Instructions for K10 Bot

## Project context
- PlatformIO project targeting UniHiker K10 hardware, ESP32-S3 based.
- Real-time constraints; uses FreeRTOS.
- PlatformIO is installed with `source venv/bin/activate`, and named `pio`.

## Coding conventions
- always use GNU C coding style.
- Use C++17 (see `platformio.ini`), Arduino-style APIs.
- Use logger service for debug output; avoid Serial.print.
- Use `constexpr` for constants; avoid `#define`.
- prefer `PROGMEM` for static data.
- Avoid malloc in ISR paths; reuse buffers when possible.
- for openapi related code, use common constants defined in namespace RoutesConsts.

## Architectural decisions
- UDP server, Web server and UI should run in separate RTOS tasks.
- UDP should run on its own core to ensure responsiveness.
- Services should be modular and loosely coupled.
- Code must be easy to enrich with new services.- Code architecture should separate hardware abstraction, business logic, and presentation layers.

## Task-specific guidance
- Tests go under `test/` using Unity.

## Forbidden/avoid
- Don't introduce new RTOS tasks.
- No dynamic casts ( supported in current runtime).
- Never delete a folder on disk.
- Never delete a file without asking confirmation to user first.

## Optimization needs
- Optimize camera frame handling to avoid copies.
- optimize code for low memory device.
- optimize code for low CPU device.

