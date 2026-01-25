# Copilot Instructions for K10 Bot

## Project context
- PlatformIO project targeting UniHiker K10 hardware.
- Real-time constraints; uses FreeRTOS.
- PlatformIO is installed with `source venv/bin/activate`, and named k`pio`.

## Coding conventions
- always use GNU C coding style.
- Use C++17 (see `platformio.ini`), Arduino-style APIs.
- Use logger service for debug output; avoid Serial.print.
- Use `constexpr` for constants; avoid `#define`.
- prefer `PROGMEM` for static data.
- Avoid malloc in ISR paths; reuse buffers when possible.

## Architectural decisions
- UDP server, Web server and UI should run in separate RTOS tasks.
- UDP should run on its own core to ensure responsiveness.
- Services should be modular and loosely coupled.
- Code must be easy to enrich with new services.

## Task-specific guidance
- Tests go under `test/` using Unity.

## Forbidden/avoid
- Don’t introduce new RTOS tasks without consulting power budget.
- No dynamic casts (not supported in current runtime).
- never delete a folder on disk.
- never delete a file without asking confirmation to user first.

## Optimization needs
- Optimize camera frame handling to avoid copies.
- optimize code for low memory device.
- optimize code for low CPU device.

