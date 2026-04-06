# aMaker Bot — Code Architecture

The aMaker Bot runs on a **UniHiker K10** board (ESP32-S3, dual-core, FreeRTOS, Arduino framework).

**Core 0** runs the real-time transport task: UDP server (port 24642) and WebSocket server (port 81) at maximum FreeRTOS priority.  
**Core 1** runs the web server (HTTP port 80) and the TFT display update loop at normal priority.

All three transports feed binary frames into **AmakerBotService**, which owns the master-registration and heartbeat logic, then dispatches commands to the registered service handlers via **BotMessageHandler**.

Service handlers are independent C++ classes that each own one area of hardware:

| Service | Responsibility |
|---|---|
| `AmakerBotService` | Master auth, heartbeat watchdog, WiFi config, reboot |
| `MotorServoService` | 4 DC motors + 6 servo channels on the DFR1216 board |
| `DFR1216Board` | Low-level I²C driver for the expansion board |
| `WifiService` | WiFi connection, NVS credential storage |
| `AmakerBotUIService` | TFT screen manager + remote screen control |

The web UI (static files in `/data/www/`) communicates exclusively over the **WebSocket** at port 81 using the same binary protocol as UDP.

---

## Related guides

| Topic | Audience | File |
|---|---|---|
| Binary protocol — frame format, all commands | Developers | [binary-protocol.md](binary-protocol.md) |
| Communication transports (UDP / WS / HTTP) | Developers | [communication.md](communication.md) |
| Quick start: register master & move servos | Users | [quickstart.md](quickstart.md) |
| BotScript web UI | Users | [web-ui.md](web-ui.md) |
| IsServiceInterface — writing a new service | Contributors | [../../../docs/contributor guides/IsServiceInterface.md](../../../docs/contributor%20guides/IsServiceInterface.md) |
| RollingLogger | Contributors | [../../../docs/contributor guides/RollingLogger.md](../../../docs/contributor%20guides/RollingLogger.md) |
