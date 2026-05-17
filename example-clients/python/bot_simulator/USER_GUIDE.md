# K10 Bot Simulator — User Guide

The Bot Simulator is a standalone desktop application that mimics a real K10 Bot on your local machine.
It listens for UDP packets, speaks the same binary protocol as the firmware, and shows you the live servo and motor state in a visual dashboard — no physical hardware required.

---

## Table of Contents

1. [Requirements](#1-requirements)
2. [Starting the simulator](#2-starting-the-simulator)
3. [Window layout](#3-window-layout)
4. [Toolbar](#4-toolbar)
5. [Servo channel panel](#5-servo-channel-panel)
6. [Motor speed panel](#6-motor-speed-panel)
7. [Message log](#7-message-log)
8. [Status bar](#8-status-bar)
9. [Connecting your client](#9-connecting-your-client)
10. [Protocol reference](#10-protocol-reference)
11. [Tips and workflows](#11-tips-and-workflows)

---

## 1. Requirements

Only the Python **standard library** is required — no `pip install` needed.
`tkinter` ships with most Python distributions.  If it is missing on your system:

```bash
# Debian / Ubuntu
sudo apt install python3-tk

# Fedora / RHEL
sudo dnf install python3-tkinter

# macOS (Homebrew)
brew install python-tk
```

---

## 2. Starting the simulator

From the `example-clients/python/` directory:

```bash
python -m bot_simulator
```

The window opens with the server **stopped**.  Press **▶ Start** to begin listening.

---

## 3. Window layout

```
┌─────────────────────────────────────────────────────────────────────────┐
│  ● 🤖 Bot Simulator    HB: ▓▓▓▓▓▒    Servos: [2]  Port: [24642]  [▶]  │  ← toolbar
├────────────────────────┬────────────────────────────────────────────────┤
│  Servo Channels        │  ← IN  → OUT  ··· SYS       [Clear]  [📄]    │
│  ┌──────┐  ┌──────┐   │ ─────────────────────────────────────────────  │
│  │  CH0 │  │  CH1 │   │  [12:34:56.123] ← IN   192.168.1.5:50001      │
│  │  ↕°  │  │  ↕°  │   │    MASTER_REGISTER  [41 73 69 6D 30 31]       │
│  └──────┘  └──────┘   │  [12:34:56.124] → OUT  192.168.1.5:50001      │
│  ┌──────┐  ┌──────┐   │    → SUCCESS                                   │
│  │  CH2 │  │  CH3 │   │  [12:34:56.900] ← IN   192.168.1.5:50001      │
│  │  ●●  │  │  (●) │   │    SET_SERVOS_ANGLE  channels=['0'] angle=90° │
│  └──────┘  └──────┘   │                                                │
│  ┌──────┐  ┌──────┐   │  ┌──────────────────────────────────────────┐ │
│  │  CH4 │  │  CH5 │   │  │  41  73  69  6D  30  31                  │ │  ← hex inspector
│  │      │  │      │   │  └──────────────────────────────────────────┘ │
│  └──────┘  └──────┘   │   Inject hex: [22 3f 02         ] [ Send ]    │  ← injector
│                        │                                                │
│  DC Motors             │                                                │
│  M1 ━━━━━━━╸+60%      │                                                │
│  M2 ◂━━━━━━  -40%      │                                                │
│  M3           STOP     │                                                │
│  M4           STOP     │                                                │
├────────────────────────┴────────────────────────────────────────────────┤
│  Status: Listening :24642  │  Master: 192.168.1.5  │  Token: sim01     │  ← status bar
└─────────────────────────────────────────────────────────────────────────┘
```

The **divider** between the left panel and the log can be dragged left or right to resize both panes.

---

## 4. Toolbar

| Control | Description |
|---------|-------------|
| **● LED** | Connection state indicator (see colour table below). |
| **HB bar** | Heartbeat watchdog — drains between heartbeats, turns red on timeout. |
| **Servos** | Spinbox (0–6).  Sets how many servo channels appear as "connected". Channels above this count show as inactive grey placeholders. |
| **Port** | UDP port to listen on (default `24642` — same as the real bot). Change *before* starting. |
| **▶ Start / ■ Stop** | Bind or release the UDP socket. |

### LED colour states

| Colour | Meaning |
|--------|---------|
| ⚫ Gray | Server stopped. |
| 🟡 Yellow | Listening — no master registered yet. |
| 🟢 Green | Master connected, heartbeat arriving normally. |
| 🟠 Orange | Master connected but heartbeat timed out (>200 ms since last `0x43`). |

### Heartbeat bar

The thin bar next to **HB:** fills up on each heartbeat packet and drains over ~200 ms.
- 🟢 **Green** while draining normally.
- 🔴 **Red** when the timeout threshold is crossed.

> **Note:** The real K10 Bot firmware cuts motors after **50 ms** without a heartbeat.
> The simulator uses a 200 ms display window so the bar is readable at normal screen refresh rates.

---

## 5. Servo channel panel

Six servo widgets are arranged in a 2×3 grid.  Only channels below the **Servos** count
(set in the toolbar) are shown as connected; the rest are dimmed placeholders.

### Rotational servo (gray)

Displayed when the channel type is `SERVO_180` or `SERVO_270`.

```
   ╭────────────╮
   │   arm      │   ← custom label (double-click to rename)
   │  ╔══╗      │
   │  ║●●║──/   │   ← needle pointing to current angle
   │  ╚══╝      │
   │  180°  45° │   ← type + current angle
   ╰────────────╯
```

- **Arc track** — shows the full angular range (gray background).
- **Progress arc** — filled from zero to the current angle (lighter gray).
- **Needle** — points from the hub centre to the arc edge at the current angle.
- **Labels** — type (`180°` / `270°`), current angle in degrees.

### Continuous servo (green)

Displayed when the channel type is `CONTINUOUS`.

```
   ╭────────────╮
   │   wheel    │
   │   ╔══╗     │
   │   ║  ║     │   ← ring with speed arc
   │   ╚══╝  ↗  │   ← direction arrow
   │  CONT +75% │
   ╰────────────╯
```

- **Ring** — full circle, green border.
- **Speed arc** — 🟢 green = forward, 🔴 red = reverse; length proportional to `|speed|`.
- **Direction arrow** — indicates rotation direction.
- **Labels** — `CONT`, speed as `+N%` / `-N%` / `STOP`.

### Flash on command

When a channel receives a new command (angle or speed change), its border briefly flashes **white** to draw your eye to the updated widget.

### Setting the servo type

**Right-click** (or middle-click on macOS) any active servo widget to open the type selector:

```
┌──────────────────────────────────┐
│ ✓ ⬤  Continuous rotation (green) │  ← currently selected
│    ◐  Angular 180°  (gray)       │
│    ◕  Angular 270°  (gray)       │
└──────────────────────────────────┘
```

| Option | Widget colour | Controlled by |
|--------|---------------|---------------|
| **Continuous rotation** | 🟢 Green ring | Speed (`−100 … +100 %`) |
| **Angular 180°** | ⬜ Gray arc | Angle (`0 … 180°`) |
| **Angular 270°** | ⬜ Gray arc | Angle (`0 … 270°`) |

Selecting a type:
- Marks the channel as **connected** (increments the *Servos* count if needed).
- Resets the value to a sensible default (90° for 180°, 135° for 270°, speed 0 for continuous).
- Logs the change in the message log as a SYS event.
- If a protocol backend is running, also updates the internal protocol state so subsequent
  `SET_SERVOS_ANGLE` / `SET_SERVOS_SPEED` commands will display correctly.

> **Tip:** You can set types before starting the server — useful to pre-configure the
> layout to match your real bot before connecting a client.

### Renaming a channel

**Double-click** any servo widget to open a small dialog and set a custom label
(e.g. `"arm"`, `"camera pan"`, `"gripper"`).  The label persists until you close the app.

---

## 6. Motor speed panel

Four horizontal **VU-meter bars** show the speed of DC motor channels 1–4.

```
M1  ━━━━━━━╸ +60%     ← green bar, right of centre
M2  ◂━━━━━━   -40%     ← red bar, left of centre
M3           STOP
M4           STOP
```

- Centre line = zero speed.
- Bar extends **right** (🟢 green) for positive (forward) speed.
- Bar extends **left** (🔴 red) for negative (reverse) speed.
- `STOP` shown in dim gray when speed is 0.

---

## 7. Message log

### Filter toggles

Three toggle buttons above the log show or hide message categories:

| Button | Category | What it hides/shows |
|--------|----------|---------------------|
| **← IN** | Incoming packets | All `0x4N` / `0x2N` commands from clients |
| **→ OUT** | Outgoing replies | All responses sent back to the master |
| **··· SYS** | System events | Server start/stop, master registration, errors |

Click a button to toggle visibility.  Active buttons are highlighted.

### Hex byte inspector

Click any log line that has a payload to display the raw bytes in the panel below the log:

```
  41  73  69  6D  30  31  ·  22  3F  02
```

Bytes are shown in groups of 8, separated by `·`.  Click a line with no data to clear the inspector.

### Auto-scroll pause

Move the mouse **into** the log area to pause auto-scrolling — useful for reading a specific message when the log is moving quickly.
Move the mouse **out** to resume.

### Save log to file

Click the **📄** icon in the log header to start tee-ing all messages to a timestamped file:

```
bot_sim_20260430_143012.log
```

The absolute path is printed in the log.  Click again to stop.

### Clear

The **Clear** button empties the log and resets the hex inspector.

---

## 8. Status bar

| Field | Description |
|-------|-------------|
| **Status** | Server state (`Stopped` or `Listening on :PORT`). |
| **Master** | IP address of the currently registered master client, or `—` if none. |
| **Token** | The registration token clients must use (`sim01`). |
| **Legend** | ■ rotational (gray) · ■ continuous (green). |

---

## 9. Connecting your client

### Configuration

Edit `example-clients/python/config.py`:

```python
DEFAULT_SERVER_IP   = "127.0.0.1"   # or the machine's LAN IP
DEFAULT_SERVER_PORT = 24642
```

### Token

The simulator uses the fixed token **`sim01`**.  Make sure your client sends this when
registering as master (`0x41`).

In `customize.py` (or wherever you set the token):

```python
TOKEN = "sim01"
```

### Typical session

1. Start the simulator and press **▶ Start**.
2. Run your Python client (`python main.py`).
3. The client sends `0x41 sim01` → simulator replies `SUCCESS` → **Master** field updates.
4. The client sends heartbeats every 40 ms → **HB bar** pulses, LED turns green.
5. Send motor / servo commands → widgets update in real time.
6. Press **■ Stop** or close the window to end the session.

---

## 10. Protocol reference

### AmakerBotService (`0x4N`) — session management

| Command | Bytes | Description |
|---------|-------|-------------|
| `0x41` MASTER_REGISTER | `41 <5-char token>` | Register as master.  Token must be `sim01`. |
| `0x42` MASTER_UNREGISTER | `42` | Release master. |
| `0x43` HEARTBEAT | `43` | Keep-alive.  Must arrive within 50 ms (firmware).  Accepted silently — not logged. |
| `0x44` PING | `44 <4-byte id>` | 4-byte echo.  Reply: `44 <id>`. |
| `0x45` GET_NAME | `45` | Returns `Bot-Simulator` as ASCII. |
| `0x46` SET_NAME | `46 <name>` | Updates the bot name (1–32 chars). |

### MotorServoService (`0x2N`) — hardware control

| Command | Bytes | Description |
|---------|-------|-------------|
| `0x21` SET_MOTORS_SPEED | `21 <mask> <speed:i8>` | Set speed (−100…+100) for motors in bitmask. |
| `0x22` SET_SERVO_TYPE | `22 <mask> <type>` | Activate channels: `00`=180°, `01`=270°, `02`=continuous. |
| `0x23` SET_SERVOS_SPEED | `23 <mask> <speed:i8>` | Set speed for continuous servos in bitmask. |
| `0x24` SET_SERVOS_ANGLE | `24 <mask> <angle_hi> <angle_lo>` | Set angle (big-endian i16) for positional servos. |
| `0x28` STOP_ALL_MOTORS | `28` | Zero all motor speeds. |

**Bitmasks:**
- Motor mask: bit 0 = motor 1, bit 1 = motor 2, bit 2 = motor 3, bit 3 = motor 4.
- Servo mask: bit 0 = channel 0, …, bit 5 = channel 5.

**Example — activate channels 0 and 1 as 180° servos then set channel 0 to 45°:**

```
22 03 00       → SET_SERVO_TYPE  mask=0b00000011  type=SERVO_180
24 01 00 2D    → SET_SERVOS_ANGLE  mask=0b00000001  angle=45  (0x002D big-endian)
```

---

## 11. Tips and workflows

### Testing a specific command without running the full client

Use the **Inject hex** bar at the bottom of the log.  Type raw bytes separated by spaces and click **Send**:

| What to test | Hex to inject |
|---|---|
| Register as master | `41 73 69 6D 30 31` (`sim01`) |
| Activate CH0 as continuous servo | `22 01 02` |
| Set CH0 speed to +80% | `23 01 50` |
| Set CH0 angle to 90° | `24 01 00 5A` |
| Stop all motors | `28` |

The packet is routed through the full protocol handler, so auth checks apply: if no master is registered, `SET_SERVOS_ANGLE` will be silently ignored.

### Watching a specific channel

1. Set **Servos** to include only the channels you care about.
2. The deactivated channels turn into dim placeholders — less visual noise.
3. Custom labels (double-click) help you match widgets to code variable names.

### Debugging a client session

1. Enable **📄** file logging before starting your client.
2. Run the full client session.
3. Open the generated `.log` file in any text editor to review the complete exchange.

### Checking byte-level details of a specific packet

1. Find the line in the log.
2. Click it — the hex bytes appear in the inspector panel below the log.
3. Cross-reference with the protocol table above.
