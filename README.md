# aMaker K10 Bot

A PlatformIO project for the UniHiker K10 board, built on ESP32-S3 hardware with FreeRTOS for tasks executions.

The project provides a modular architecture with multiple services,  including UDP communication, web server, camera streaming, servo control, and sensor management.

Services run in separate RTOS tasks for optimal performance and responsiveness, with hardware abstraction layers separating concerns.

Features include WiFi connectivity, OpenAPI interfaces, and a web-based UI for remote control and monitoring.
Designed for low-memory and low-CPU optimization with Arduino-style APIs and C++17.

## Documentation
You will find the following dedicated documentation in [docs](docs) :
- [Project Overview](docs/readme.md)
- [Service Interface](docs/IsServiceInterface.md)
- [OpenAPI Interface](docs/IsOpenAPIInterface.md)
- [Rolling Logger](docs/RollingLogger.md)

## Coming next
A [todo](TODO.md) list may be present in project.
