# aMaker K10 Bot

A PlatformIO project for the UniHiker K10 board, built on ESP32-S3 hardware with FreeRTOS for task execution.

The project provides a modular, service-based architecture including UDP communication, async web server, camera streaming (MJPEG + audio), servo control, motor control, LED control, sensor management, and master-controller registration.

Services run in dedicated FreeRTOS tasks for real-time performance. Hardware abstraction layers separate concerns.

Features include WiFi connectivity, OpenAPI 3.0 interfaces, a token-based master-controller system, and a web-based UI for remote control and monitoring.
Designed for low-memory and low-CPU optimization with Arduino-style APIs and C++17.

## Key Features
- **Async web server** (ESPAsyncWebServer) — non-blocking HTTP on port 80
- **Master controller registration** — token-based authorization system (`AmakerBotService`)
- **Camera service** — JPEG snapshot, MJPEG streaming, and WAV audio streaming
- **LED control** — onboard K10 RGB LEDs (K10Service) and DFR1216 expansion LEDs
- **Servo & motor control** — DFR0548 (8 servos) + DFR1216 expansion (6 servos, 4 motors)
- **Sensors** — light, temperature, humidity, microphone, accelerometer
- **UDP protocol** — binary + text commands on port 24642
- **OpenAPI 3.0** — live spec at `/api/openapi.json`, interactive docs at `/api/docs`
- **Multi-mode TFT display** — Button A cycles through UI / app log / debug log / ESP log

## Documentation
You will find the following dedicated documentation in [docs](docs):
- [Project Overview](docs/readme.md)
- [API Reference](OPENAPI.md)
- [UDP Guide (AI / AI-agents)](docs/AI_UDP_guide.md)
- [Service Interface](docs/contributor%20guides/IsServiceInterface.md)
- [OpenAPI Interface](docs/contributor%20guides/IsOpenAPIInterface.md)
- [Rolling Logger](docs/contributor%20guides/RollingLogger.md)
- [DFR1216 Expansion Board](docs/contributor%20guides/DFR1216Service.md)
- [UDP Messages](docs/user%20guides/UDPMessages.md)
- [WiFi Service](docs/user%20guides/WiFiService.md)

## Coming next
A [todo](TODO.md) list may be present in project.
