# K10 Bot — Python UDP Client

A cross-platform Python TUI controller for the K10 Bot robot.  Built with
[Textual](https://textual.textualize.io/), supports keyboard and multiple
gamepad/joystick inputs with live calibration.

---

## Quick Start

```bash
cd example-clients/python
bash setup.sh          # create venv + pip install -e . (editable install)

source venv/bin/activate
python main.py         # launch the TUI  (or: k10-bot)
```

1. Enter the bot's IP address in the **Server** field and click **Connect**.
2. Use the keyboard arrows (or gamepad) to drive the bot.

> **First time with a gamepad?** Press `j` inside the app to run the
> interactive calibration wizard.

---

## Customising Your Bot

**`customize.py` is the only file you need to edit.**

Open it and follow the four labelled sections:

| Section | What you do |
|---------|-------------|
| **§ 1 Hardware** | Declare your motor ports and servo channels by name |
| **§ 2 Actions** | Define named movements (e.g. `"forward"`, `"grip_close"`) as lists of motor/servo commands |
| **§ 3 Keyboard** | Map keyboard keys → action names |
| **§ 4 Joystick** | Map analog sticks (tank drive) and buttons → actions |

### Example — differential-drive robot with an arm

```python
# § 1 — Hardware
BOT_CONFIG.add_motor("left",  motor_id=1)
BOT_CONFIG.add_motor("right", motor_id=2)
BOT_CONFIG.add_servo("arm",   channel_id=0, servo_type=ServoType.CONTINUOUS)

# § 2 — Actions
BOT_CONFIG.add_action("forward",    [MotorCmd("left",  80), MotorCmd("right",  80)])
BOT_CONFIG.add_action("backward",   [MotorCmd("left", -80), MotorCmd("right", -80)])
BOT_CONFIG.add_action("turn_left",  [MotorCmd("left", -60), MotorCmd("right",  60)])
BOT_CONFIG.add_action("turn_right", [MotorCmd("left",  60), MotorCmd("right", -60)])
BOT_CONFIG.add_action("stop",       [MotorCmd("left",   0), MotorCmd("right",   0)])
BOT_CONFIG.add_action("arm_extend", [ServoSpeedCmd("arm",  70)])
BOT_CONFIG.add_action("arm_retract",[ServoSpeedCmd("arm", -70)])
BOT_CONFIG.add_action("arm_stop",   [ServoSpeedCmd("arm",   0)])

# § 3 — Keyboard
KEYBOARD_ACTIONS = {
    "up":    "forward",
    "down":  "backward",
    "left":  "turn_left",
    "right": "turn_right",
    "space": "arm_extend",
    "shift": "arm_retract",
}
KEYBOARD_IDLE_ACTION = "stop"   # auto-stop when all keys released

# § 4 — Joystick (tank drive)
JOYSTICK_LEFT_STICK_MOTOR  = "left"
JOYSTICK_RIGHT_STICK_MOTOR = "right"
JOYSTICK_SPEED_SCALE       = 80
JOYSTICK_BUTTON_ACTIONS    = {0: "arm_extend", 1: "arm_retract"}
```

---

## Project Structure

```
python/
├── customize.py                ★ EDIT THIS — your bot's hardware + bindings
├── config.py                     Network / UI settings (rarely edited)
├── main.py                       Textual TUI entry point (do not edit)
├── requirements.txt
├── setup.sh
├── README.md
└── udp_client/                   Framework — do not edit
    ├── bot_config.py             BotConfig API: hardware declaration + packet builder
    ├── control/
    │   └── controller.py         Control loop: input state → UDP packets
    ├── network/
    │   └── udp_client.py         Async UDP socket with auto-reconnect
    ├── input/
    │   ├── keyboard_handler.py   Global keyboard listener (pynput)
    │   ├── joystick_handler.py   Multi-gamepad handler (pygame)
    │   ├── joystick_calibration.py  Calibration data model + JSON persistence
    │   ├── calibration_wizard.py    Interactive CLI calibration wizard
    │   └── input_manager.py      Unified input manager
    ├── state/
    │   └── app_state.py          Application state model
    └── ui/
        └── app.py                Reusable Textual widgets
```

---

## How It Works

```
customize.py   ←  you define hardware + bindings here
     │
     └──► Controller (every 40 ms)
               │
         ┌─────┴──────────────────────────┐
         │  Keyboard                      │  Joystick
         │  pressed key → action name     │  left/right Y → motor speed packet
         │             → BotConfig        │  button      → action → BotConfig
         │             → UDP packets      │              → UDP packets
         └────────────────────────────────┘
                         │
                    K10 Bot (UDP port 24642)
```

The **Controller** runs every 40 ms on the same timer as the heartbeat.
It reads the current input state, looks up your mappings in `customize.py`,
and calls `BotConfig.execute()` which translates action names into binary
MotorServoService packets and sends them.

### Binary protocol summary

```
SET_MOTORS_SPEED  0x21  [motor_mask:u8][speed:i8  -100…+100]
SET_SERVO_TYPE    0x22  [servo_mask:u8][type:u8  0=180°, 1=270°, 2=continuous]
SET_SERVOS_SPEED  0x23  [servo_mask:u8][speed:i8  -100…+100]
SET_SERVOS_ANGLE  0x24  [servo_mask:u8][angle_hi:u8][angle_lo:u8]
STOP_ALL_MOTORS   0x28
```

See [binary-protocol.md](../../data/www/help/binary-protocol.md) for the full reference.

---

## Controls

### Keyboard shortcuts (TUI)

| Key | Action |
|-----|--------|
| `q` | Quit |
| `c` | Connect to server |
| `d` | Disconnect |
| `j` | Run joystick calibration wizard |

### Default bot control (from `customize.py`)

| Key | Bot action |
|-----|------------|
| ↑ | Forward |
| ↓ | Backward |
| ← | Turn left |
| → | Turn right |
| *(release all)* | Stop |

### Gamepad (tank drive by default)

| Input | Effect |
|-------|--------|
| Left stick Y | Left motor speed |
| Right stick Y | Right motor speed |
| Buttons | Configurable via `JOYSTICK_BUTTON_ACTIONS` in `customize.py` |

---

## Infrastructure Settings (`config.py`)

Edit `config.py` only for network/timing settings:

```python
DEFAULT_SERVER_IP   = "192.168.4.1"  # K10 Bot IP address
DEFAULT_SERVER_PORT = 24642           # K10 Bot UDP port
HEARTBEAT_INTERVAL  = 0.040           # 40 ms — keep-alive cadence
JOYSTICK_DEADZONE   = 0.15            # fallback when no calibration file
```

---

## Joystick Calibration

The built-in wizard corrects for axis drift, asymmetric range, per-axis
deadzone, and inverted axes.  Calibration is keyed by joystick name and
persists across sessions in `joystick_calibration.json`.

**Run from the TUI** — press `j` or click **🎮 Calibrate Joystick**.  
The TUI suspends, the wizard runs, calibration is applied immediately.

**Run standalone** (before first launch):

```bash
python -m udp_client.input.calibration_wizard
```

Wizard steps:

1. **Center** — release all sticks/triggers → records neutral drift
2. **Range** — guided axis-by-axis: push each direction, hold, press ENTER
3. **Deadzone** — confirm or override (default 0.15)
4. **Inversion** — choose axes to invert (Y-axes suggested automatically)

---

## Installation

### Quick (Linux / macOS)

```bash
cd example-clients/python
bash setup.sh
```

### Manual

```bash
python3 -m venv venv
source venv/bin/activate        # Windows: venv\Scripts\activate
pip install -r requirements.txt
```

### Requirements

- Python 3.9+
- Linux, macOS, or Windows (Windows Terminal)

### Dependencies

| Package | Version | Purpose |
|---------|---------|---------|
| `textual` | 8.2.3 | TUI framework |
| `pygame` | 2.5.2 | Joystick input |
| `pynput` | 1.7.6 | Global keyboard listener |

---

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| *"customize.py not found"* warning | The template is already `customize.py` — edit it and restart |
| Bot not responding | Check IP, power, and that UDP 24642 is not firewalled |
| No joystick detected | Plug in the gamepad **before** starting; on Linux add yourself to the `input` group |
| Joystick drifting / wrong direction | Press `j` to run the calibration wizard; delete `joystick_calibration.json` to reset |
| Keyboard ignored on Linux | Run `sudo python main.py` |

---

## Related Documentation

- [Binary protocol reference](../../data/www/help/binary-protocol.md)
- [Communication transports](../../data/www/help/communication.md)
- [Quick start guide](../../data/www/help/quickstart.md)
- [K10 Bot project docs](../../docs/)


## Features

✨ **Cross-Platform**: Works on Linux, macOS, and Windows (Terminal)  
🎮 **Multi-Input**: Keyboard and multiple joystick/gamepad support  
�️ **Joystick Calibration**: Built-in interactive wizard (axis centering, range, deadzone, inversion) — launch from the TUI with `j`  
�📊 **Real-Time Stats**: Connection status, packet count, latency monitoring  
🎨 **Modern TUI**: Built with Textual framework for professional appearance  
🚀 **Async I/O**: Non-blocking network communication  
⚙️ **Configurable**: Easy server IP and port settings  

## Requirements

- Python 3.9+
- pip (Python package manager)

## Installation

### Quick Setup (Linux/macOS)

```bash
cd example-clients/python
bash setup.sh
```

### Manual Setup

```bash
cd example-clients/python

# Create virtual environment
python3 -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate

# Install dependencies
pip install -r requirements.txt
```

## Usage

```bash
# Activate virtual environment (if not already activated)
source venv/bin/activate

# Run the application
python main.py
```

## Controls

### Keyboard Shortcuts
- `q` - Quit application
- `c` - Connect to server
- `d` - Disconnect from server
- `j` - Launch joystick calibration wizard

### Keyboard Control
- **Arrow Keys**: Movement (↑ ↓ ← →)
- **Space**: Action/Confirm button
- **Shift**: Modifier key
- **Enter**: Send command

### Joystick/Gamepad
- **Left Stick**: Primary control
- **Right Stick**: Secondary control/aiming
- **LT/RT**: Analog triggers
- **A/B/X/Y**: Action buttons
- **LB/RB**: Shoulder buttons
- **D-Pad**: Alternative movement

## Configuration

Edit `config.py` to customize:

```python
DEFAULT_SERVER_IP   = "192.168.1.178"  # K10 Bot IP
DEFAULT_SERVER_PORT = 24642            # K10 Bot UDP port
KEYBOARD_ENABLED    = True             # Enable keyboard input
JOYSTICK_ENABLED    = True             # Enable joystick input
JOYSTICK_DEADZONE   = 0.15             # Fallback deadzone when no calibration file
CALIBRATION_FILE    = Path(...)        # Auto-loaded calibration (joystick_calibration.json)
```

> `JOYSTICK_DEADZONE` is only used when no calibration file exists. The wizard writes
> per-joystick deadzone values into `joystick_calibration.json`, which takes precedence.

## Project Structure

```
python/
├── main.py                          # Application entry point (TUI)
├── config.py                        # Configuration constants
├── requirements.txt                 # Python dependencies
├── setup.sh                         # Setup script
├── README.md                        # This file
├── joystick_calibration.json        # Generated by calibration wizard (git-ignored)
└── udp_client/
    ├── network/
    │   └── udp_client.py           # Async UDP client
    ├── input/
    │   ├── keyboard_handler.py     # Keyboard input handler
    │   ├── joystick_handler.py     # Joystick/gamepad input handler
    │   ├── joystick_calibration.py # Calibration data model + JSON persistence
    │   ├── calibration_wizard.py   # Interactive CLI calibration wizard
    │   └── input_manager.py        # Unified input manager
    ├── state/
    │   └── app_state.py            # Central state management
    └── ui/
        └── app.py                  # Textual UI components
```

## Features in Detail

### Network Layer
- **Async UDP Client**: Non-blocking socket communication
- **Auto-Reconnect**: Automatic reconnection with configurable retry policy
- **Statistics Tracking**: Monitor packets sent, latency, connection state

### Input Handling
- **Keyboard**: Global keyboard listener (doesn't require window focus)
- **Joystick**: Multi-device support for gamepads/joysticks
- **Deadzone**: Configurable analog stick deadzone (fallback when no calibration file)

### Joystick Calibration

The built-in calibration wizard corrects for axis drift, asymmetric range, per-axis
deadzone, and inverted axes. Profiles are keyed by joystick name so they survive
USB reconnects and persist across sessions.

**Run from inside `main.py`** — press `j` or click **🎮 Calibrate Joystick** in the
Input tab. The TUI suspends, the wizard runs interactively, and calibration is applied
immediately on completion without restarting.

**Run standalone** (useful before first launch):
```bash
python -m udp_client.input.calibration_wizard
```

Wizard steps:
1. **Center** — release all sticks/triggers → records neutral drift per axis
2. **Range** — guided axis-by-axis: for each axis you are told exactly which direction to push, hold it, press ENTER to sample, then repeat for the opposite extreme — no time pressure
3. **Deadzone** — confirm or override (default 0.15, range 0.00–0.50)
4. **Inversion** — choose which axes to invert (Y-axes suggested automatically)

Result is saved to `joystick_calibration.json` next to `config.py` and loaded
automatically on next start.

### UI Components
- **Status Bar**: Real-time connection indicator
- **Input Devices Panel**: List of connected devices
- **Joystick Visualizer**: Real-time stick and button visualization
- **Statistics Panel**: Connection metrics and performance
- **Help Panel**: Keyboard shortcuts and usage info

## Message Protocol

Commands are sent as JSON over UDP:

```json
{
  "type": "movement",
  "data": {
    "x": 0.5,
    "y": -0.8,
    "speed": 100
  }
}
```

## Troubleshooting

### Connection Issues
1. **Verify K10 Bot is powered on** and connected to the network
2. **Check firewall settings** - UDP port may be blocked
3. **Verify IP address** - Use the correct network IP of the K10 Bot
4. **Test connectivity** with `ping <k10-bot-ip>`

### No Joystick Detected
1. **Connect joystick before starting** the application
2. **Check device permissions**: On Linux, add yourself to `input` group:
   ```bash
   sudo usermod -a -G input $USER
   ```
3. **Test with** `jstest /dev/input/js0` (Linux)

### Joystick Drifting / Wrong Direction
1. Press `j` inside the app (or run `python -m udp_client.input.calibration_wizard`) to create a calibration profile
2. Delete `joystick_calibration.json` to reset to the factory fallback deadzone
3. If axes are swapped, use the inversion step of the wizard

### Keyboard Not Working
- Ensure the terminal has focus
- On Linux, you may need to run with appropriate permissions:
  ```bash
  sudo python main.py
  ```

### Performance Issues
- Reduce `REFRESH_RATE` in `config.py`
- Disable unused input devices in `config.py`

## Dependencies

- **textual** (0.42.1): Modern TUI framework
- **pygame** (2.5.2): Joystick input handling
- **pynput** (1.7.6): Global keyboard listener

## Platform-Specific Notes

### Linux
```bash
# Install dependencies (Debian/Ubuntu)
sudo apt-get install python3-dev libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev libsdl2-gfx-dev

# Run with proper input permissions
sudo python main.py  # For joystick/keyboard input
```

### macOS
```bash
# Install via Homebrew
brew install python3 sdl2

# Run normally
python main.py
```

### Windows
```bash
# Use Windows Terminal for best experience
python main.py
```

## Development

### Running Tests
```bash
python -m pytest test/
```

### Code Style
- Python 3.9+ type hints
- Black code formatting (optional)
- Follow PEP 8 conventions

## License

See [LICENSE](../../LICENSE) in the project root.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Submit a pull request

## Support

For issues or questions:
1. Check the troubleshooting section above
2. Review [K10 Bot documentation](../../../docs/)
3. Open an issue on GitHub

---

**Created for the K10 Bot Project** 🤖
