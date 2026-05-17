# K10 Bot Python Client — User Guide

A cross-platform TUI (Terminal User Interface) controller for the K10 Bot
robot. Built with [Textual](https://textual.textualize.io/), it supports
keyboard and multiple gamepad/joystick inputs with live calibration.

---

## Table of Contents

1. [Installation](#1-installation)
2. [Running the App](#2-running-the-app)
3. [Connecting to the Bot](#3-connecting-to-the-bot)
4. [Customizing Your Bot](#4-customizing-your-bot)
5. [Keyboard Controls](#5-keyboard-controls)
6. [Joystick / Gamepad Controls](#6-joystick--gamepad-controls)
7. [Joystick Calibration](#7-joystick-calibration)
8. [Infrastructure Settings (`config.py`)](#8-infrastructure-settings-configpy)
9. [Running Tests & Linting](#9-running-tests--linting)
10. [TUI Tabs Reference](#10-tui-tabs-reference)
11. [Troubleshooting](#11-troubleshooting)
12. [Project Structure](#12-project-structure)

---

## 1. Installation

### Prerequisites

- **Python 3.11+**
- **pip** (comes with Python)
- **Linux / macOS / Windows** (Windows Terminal recommended on Windows)

On Linux (Debian/Ubuntu), install SDL2 for gamepad support:

```bash
sudo apt-get install python3-dev libsdl2-dev
```

### Quick Setup (Linux / macOS)

```bash
cd example-clients/python
bash setup.sh
```

This creates a virtual environment, installs all dependencies via
`pip install -e .` (editable install from `pyproject.toml`), and you're
ready to go.

### Manual Setup

```bash
cd example-clients/python
python3 -m venv venv
source venv/bin/activate        # Windows: venv\Scripts\activate
pip install -e .
```

### Development Setup (includes pytest and tox)

```bash
pip install -e ".[dev]"
```

---

## 2. Running the App

```bash
source venv/bin/activate
python main.py
```

Or, if you installed with `pip install -e .`:

```bash
k10-bot
```

The TUI launches in your terminal with tabs for Input, Stats, Help, and
Config.

---

## 3. Connecting to the Bot

### First Launch — Connection Wizard

On the first launch (no saved server), a **Connection Wizard** pops up
automatically. It scans your local `/24` subnet for K10 Bots that respond
to a UDP ping. Select a device and click **Connect**.

### Manual Connection

1. Enter the bot's address in the **Server** field: `192.168.4.1:24642`
2. Click **Connect** (or press `c`)

The app remembers your last-used server in `.last_server` and pre-fills
it on subsequent launches.

### Connection Indicators

| Indicator | Meaning |
|-----------|---------|
| ✅ Connected | Bot is responding to pings |
| ⚠️ Warning | Ping timeout — auto-reconnect in progress |
| ❌ Disconnected | Not connected (press `c` to connect) |

---

## 4. Customizing Your Bot

**`customize.py` is the only file you need to edit.**

It has four labelled sections:

### § 1 — Hardware: Declare Motors & Servos

Map friendly names to physical ports on the DFR1216 board:

```python
BOT_CONFIG = BotConfig()

# DC motors (motor_id: 1–4)
BOT_CONFIG.add_motor("left",  motor_id=1)
BOT_CONFIG.add_motor("right", motor_id=2)

# Servos (channel_id: 0–5)
BOT_CONFIG.add_servo("arm", channel_id=0, servo_type=ServoType.CONTINUOUS)
BOT_CONFIG.add_servo("pan", channel_id=3, servo_type=ServoType.SERVO_180)
```

**Servo types:**

| Type | Use case | Controlled via |
|------|----------|----------------|
| `ServoType.SERVO_180` | Standard positional servo | `ServoAngleCmd` (0–180°) |
| `ServoType.SERVO_270` | Wide-angle positional servo | `ServoAngleCmd` (0–270°) |
| `ServoType.CONTINUOUS` | Continuous-rotation servo | `ServoSpeedCmd` (-100…+100) |

### § 2 — Actions: Define Named Movements

Actions are lists of commands that execute together in one 40 ms tick:

```python
BOT_CONFIG.add_action("forward", [
    MotorCmd("left",  80),
    MotorCmd("right", 80),
])
BOT_CONFIG.add_action("stop", [
    MotorCmd("left",  0),
    MotorCmd("right", 0),
])
BOT_CONFIG.add_action("arm_up",   [ServoSpeedCmd("arm", 70)])
BOT_CONFIG.add_action("arm_down", [ServoSpeedCmd("arm", -70)])
BOT_CONFIG.add_action("look_center", [ServoAngleCmd("pan", 90)])
```

**Command types:**

| Command | Parameters | Range |
|---------|------------|-------|
| `MotorCmd(name, speed)` | DC motor speed | -100 … +100 |
| `ServoSpeedCmd(name, speed)` | Continuous servo speed | -100 … +100 |
| `ServoAngleCmd(name, angle)` | Positional servo angle | 0–180 or 0–270 |

### § 3 — Keyboard Bindings

Map keys to actions. Each binding has `on_keydown` (mandatory) and
optional `on_keyup`:

```python
KEYBOARD_ACTIONS = {
    "up":    {"on_keydown": "forward",    "on_keyup": "stop"},
    "down":  {"on_keydown": "backward",   "on_keyup": "stop"},
    "left":  {"on_keydown": "turn_left",  "on_keyup": "stop"},
    "right": {"on_keydown": "turn_right", "on_keyup": "stop"},
    "space": "arm_up",          # shorthand — no on_keyup
}

# Action when no key is pressed (recommended: "stop")
KEYBOARD_IDLE_ACTION = "stop"
```

**Available key names:** `up`, `down`, `left`, `right`, `space`, `enter`,
`shift`, and any single letter/digit (`w`, `a`, `s`, `d`, etc.)

### § 4 — Joystick Bindings

#### Per-axis binding (simplest)

Map a single stick axis directly to a motor/servo:

```python
JOYSTICK_SPEED_SCALE = 80   # max speed at full stick deflection

JOYSTICK_STICKS = {
    "left_y":  "left",       # left stick Y → left motor
    "right_y": "right",      # right stick Y → right motor
}
```

#### Whole-stick handlers (advanced)

Use built-in helpers for common drive modes:

```python
from udp_client.control.stick_helpers import tank_drive, arcade_drive

JOYSTICK_STICKS = {
    # Tank drive: left stick Y → left motor, right stick Y → right motor
    "left": tank_drive("left", "right"),

    # OR arcade drive: left stick Y → fwd/back, left stick X → turning
    # "left": arcade_drive("left", "right"),
}
```

#### Buttons and D-Pad

```python
JOYSTICK_BUTTON_ACTIONS = {
    0: "arm_up",       # A / Cross
    1: "arm_down",     # B / Circle
}

JOYSTICK_DPAD_ACTIONS = {
    "up":    {"on_keydown": "forward",    "on_keyup": "stop"},
    "down":  {"on_keydown": "backward",   "on_keyup": "stop"},
    "left":  {"on_keydown": "turn_left",  "on_keyup": "stop"},
    "right": {"on_keydown": "turn_right", "on_keyup": "stop"},
}
```

### Live Reload

Press `r` in the TUI to **reload `customize.py`** without restarting. A
notification shows which actions were added or removed.

---

## 5. Keyboard Controls

### TUI Shortcuts

| Key | Action |
|-----|--------|
| `q` | Quit the app |
| `c` | Connect to server |
| `d` | Disconnect |
| `j` | Joystick calibration wizard |
| `r` | Reload `customize.py` |
| `t` | Toggle dark/light theme |
| `w` | Open connection wizard |

### Bot Control (default bindings)

| Key | Action |
|-----|--------|
| ↑ | Forward |
| ↓ | Backward |
| ← | Turn left |
| → | Turn right |
| *(release all)* | Stop |

> **Note:** Terminals don't send key-up events. Keys auto-expire after
> 200 ms of inactivity, which triggers the idle action (usually "stop").

---

## 6. Joystick / Gamepad Controls

### Default tank drive

| Input | Effect |
|-------|--------|
| Left stick Y | Left motor speed |
| Right stick Y | Right motor speed |
| D-Pad | Same as keyboard arrows |

### Identifying Button IDs

Use the **🎮 Input** tab in the TUI. Press each button on your gamepad —
the panel shows which button ID lights up. Use those IDs in
`JOYSTICK_BUTTON_ACTIONS`.

---

## 7. Joystick Calibration

The built-in wizard corrects axis drift, asymmetric range, per-axis
deadzone, and inverted axes. Profiles are keyed by joystick name and
persist in `joystick_calibration.json`.

### From the TUI

Press `j` — the TUI suspends, the wizard runs, and calibration is applied
immediately when done.

### Standalone (before first launch)

```bash
python -m udp_client.input.calibration_wizard
```

### Wizard Steps

1. **Center** — release all sticks/triggers → records neutral drift
2. **Range** — guided axis-by-axis: push each direction, hold, press
   Enter
3. **Deadzone** — confirm or override (default 0.15)
4. **Inversion** — choose axes to invert (Y-axes auto-suggested)

### Reset Calibration

Delete `joystick_calibration.json` to fall back to the default deadzone
from `config.py`.

---

## 8. Infrastructure Settings (`config.py`)

You rarely need to edit this file. Key settings:

```python
DEFAULT_SERVER_IP   = "192.168.4.1"   # K10 Bot IP
DEFAULT_SERVER_PORT = 24642           # K10 Bot UDP port
BOT_TOKEN           = "sim01"         # 5-char token for registration

HEARTBEAT_INTERVAL  = 0.040           # 40 ms keep-alive
PING_INTERVAL       = 0.250           # 250 ms RTT measurement
PING_TIMEOUT        = 0.200           # ping echo timeout

JOYSTICK_ENABLED    = True            # set False to disable gamepad
KEYBOARD_ENABLED    = True            # global keyboard listener (pynput)
JOYSTICK_DEADZONE   = 0.15            # fallback when no calibration file
```

---

## 9. Running Tests & Linting

### Tests (pytest)

```bash
source venv/bin/activate
python -m pytest                      # 51 unit tests
python -m pytest -v                   # verbose output
python -m pytest tests/test_bot_config.py   # single file
```

### Lint & Format (ruff)

```bash
ruff check udp_client/ tests/ main.py config.py
ruff format udp_client/ tests/ main.py config.py
```

### All at once (tox)

```bash
pip install tox
tox
```

This runs tests on Python 3.12 and checks lint + formatting.

---

## 10. TUI Tabs Reference

| Tab | Contents |
|-----|----------|
| **🎮 Input** | Connected devices, virtual D-Pad, keyboard state, joystick visualizer |
| **📊 Stats** | Motor/servo status, action history, packet rate & latency |
| **❓ Help** | Keyboard shortcuts and troubleshooting tips |
| **🔧 Config** | Live inspector for `customize.py` bindings (hardware, actions, key/joystick mappings) |

---

## 11. Troubleshooting

| Symptom | Fix |
|---------|-----|
| `customize.py not found` warning | The file exists — edit it with your bot's hardware and restart |
| Bot not responding | Check IP, power, and that UDP port 24642 is not firewalled |
| ⚠️ Joystick disabled: pygame not installed | Run `pip install pygame` in your venv |
| No joystick detected | Plug in the gamepad **before** starting; on Linux: `sudo usermod -aG input $USER` then re-login |
| Joystick drifting / wrong axis | Press `j` to calibrate; delete `joystick_calibration.json` to reset |
| Keys not registering | Make sure the **Server** input field is not focused (click elsewhere) |
| Connection lost frequently | Check Wi-Fi signal; reduce `PING_TIMEOUT` in `config.py` |
| TUI looks broken | Use a modern terminal (Windows Terminal, iTerm2, GNOME Terminal) |

---

## 12. Project Structure

```
python/
├── customize.py                 ★ EDIT THIS — your bot's hardware + bindings
├── config.py                    Network / UI / timing settings
├── main.py                      TUI entry point (K10BotClient App)
├── pyproject.toml               Package metadata, deps, ruff/pyright config
├── tox.ini                      Test & lint runner
├── requirements.txt             Relaxed dependency ranges
├── requirements.lock            Pinned versions for reproducibility
├── setup.sh                     One-command setup script
│
├── tests/
│   ├── test_bot_config.py       BotConfig: registration, API, packet encoding
│   └── test_merge_commands.py   Controller merge logic: sum, clamp, zero-priority
│
└── udp_client/                  Framework (do not edit)
    ├── bot_config.py            BotConfig: hardware declaration + packet builder
    ├── control/
    │   ├── controller.py        Control loop: input state → UDP packets
    │   └── stick_helpers.py     tank_drive(), arcade_drive() helpers
    ├── network/
    │   └── udp_client.py        Async UDP socket with auto-reconnect
    ├── input/
    │   ├── keyboard_handler.py  Global keyboard listener (pynput)
    │   ├── joystick_handler.py  Multi-gamepad handler (pygame)
    │   ├── joystick_calibration.py   Calibration data model + JSON persistence
    │   ├── calibration_wizard.py     Interactive CLI calibration wizard
    │   └── input_manager.py     Unified input manager
    └── ui/
        ├── constants.py         Shared display constants & messages
        └── widgets/             12 reusable TUI widget panels
            ├── connection_wizard.py
            ├── server_panel.py
            ├── action_indicator.py
            ├── bot_status.py
            ├── statistics.py
            ├── joystick_visualizer.py
            ├── keyboard_display.py
            ├── virtual_dpad.py
            ├── action_history.py
            ├── device_list.py
            ├── config_inspector.py
            └── help_panel.py
```

---

## How It All Fits Together

```
customize.py           You define hardware + bindings
     │
     ▼
Controller (40 ms)     Reads input, looks up your mappings
     │
     ├── Keyboard ──► on_keydown action ──► BotConfig.build_packets()
     ├── Stick    ──► axis × scale      ──► MotorCmd / ServoSpeedCmd
     ├── Button   ──► action name       ──► BotConfig.build_packets()
     └── D-Pad    ──► on_keydown/up     ──► BotConfig.build_packets()
     │
     ▼
UDPClient              Sends binary packets to bot (async, non-blocking)
     │
     ▼
K10 Bot (UDP 24642)    Drives motors and servos
```

**Binary protocol summary:**

| Opcode | Name | Payload |
|--------|------|---------|
| `0x21` | SET_MOTORS_SPEED | `[motor_mask:u8][speed:i8]` |
| `0x22` | SET_SERVO_TYPE | `[servo_mask:u8][type:u8]` |
| `0x23` | SET_SERVOS_SPEED | `[servo_mask:u8][speed:i8]` |
| `0x24` | SET_SERVOS_ANGLE | `[servo_mask:u8][angle_hi:u8][angle_lo:u8]` |
| `0x28` | STOP_ALL_MOTORS | *(no payload)* |
| `0x41` | MASTER_REGISTER | `[token:5 bytes]` |
| `0x43` | HEARTBEAT | *(no payload, every 40 ms)* |
| `0x44` | PING | `[nonce:4 bytes]` |
