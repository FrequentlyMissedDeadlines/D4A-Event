# K10 Bot Simulator

A standalone visual simulator for the K10 Bot UDP protocol.
Use it to test your client code **without** physical hardware.

The simulator listens on the same UDP port as the real K10 Bot, speaks the
same binary protocol, and visualises the live servo state in a dark-themed
Tkinter window.

---

## Screenshot

```
┌──────────────────────────────────────────────────────────────────────┐
│  🤖  Bot Simulator          Servos: [2]   Port: [24642]  [▶ Start]  │
├─────────────────────┬────────────────────────────────────────────────┤
│  Servo Channels     │  Message Log                                   │
│  ┌──────┐ ┌──────┐  │  [12:34:56.789] ← IN   192.168.1.5:50001      │
│  │  CH0 │ │  CH1 │  │    MASTER_REGISTER  [41 73 69 6D 30 31]       │
│  │  ↕°  │ │  ↕°  │  │  [12:34:56.790] → OUT  192.168.1.5:50001      │
│  └──────┘ └──────┘  │    → SUCCESS                                   │
│  ┌──────┐ ┌──────┐  │  [12:34:57.002] ← IN   192.168.1.5:50001      │
│  │  CH2 │ │  CH3 │  │    SET_SERVOS_ANGLE  channels=['0'] angle=90° │
│  │  (●) │ │  (●) │  │                                                │
│  └──────┘ └──────┘  │                               [Clear]          │
├─────────────────────┴────────────────────────────────────────────────┤
│  Status: Listening :24642  │  Master: 192.168.1.5  │  Token: sim01  │
└──────────────────────────────────────────────────────────────────────┘
```

---

## Requirements

Only the Python **standard library** is used — no `pip install` needed.
`tkinter` is bundled with most Python distributions.  If it is missing:

```bash
# Debian / Ubuntu
sudo apt install python3-tk

# Fedora / RHEL
sudo dnf install python3-tkinter

# macOS (Homebrew)
brew install python-tk
```

---

## Running

From the `example-clients/python/` directory:

```bash
python -m bot_simulator
```

Or from the project root:

```bash
python -m example-clients.python.bot_simulator   # adjust to your Python path
```

---

## UI Controls

| Control | Description |
|---------|-------------|
| **Servos** spinbox | Number of servo channels shown as "connected" (0–6). Channels above this count appear as inactive grey placeholders. |
| **Port** field | UDP port to listen on (default `24642` — same as the real bot). |
| **▶ Start / ■ Stop** | Start or stop the UDP listener. |
| **Clear** | Clear the message log. |

---

## Protocol support

### AmakerBotService (`0x4N`)

| Cmd  | Name               | Notes |
|------|--------------------|-------|
| `0x41` | `MASTER_REGISTER`  | Token must be **`sim01`**. |
| `0x42` | `MASTER_UNREGISTER`| Releases master. |
| `0x43` | `HEARTBEAT`        | Accepted silently (not logged — fires every 40 ms). |
| `0x44` | `PING`             | 4-byte echo. |
| `0x45` | `GET_NAME`         | Returns `"Bot-Simulator"`. |
| `0x46` | `SET_NAME`         | Updates the displayed name. |

### MotorServoService (`0x2N`)

| Cmd  | Name               | Visual effect |
|------|--------------------|---------------|
| `0x21` | `SET_MOTORS_SPEED`  | (state stored; no visual yet) |
| `0x22` | `SET_SERVO_TYPE`    | Marks the channel active and sets its type (rotational or continuous). |
| `0x23` | `SET_SERVOS_SPEED`  | Updates the speed arc on continuous-servo widgets. |
| `0x24` | `SET_SERVOS_ANGLE`  | Moves the needle on rotational-servo widgets. |
| `0x28` | `STOP_ALL_MOTORS`   | Zeros all motor speeds. |

---

## Servo widget colour coding

| Visual | Meaning |
|--------|---------|
| **Gray** needle on arc | Rotational servo (SERVO_180 or SERVO_270). Needle sweeps the angular range. |
| **Green** ring with arc | Continuous servo. The arc colour indicates direction (green = forward, red = reverse). |
| **Dark placeholder** | Channel is not connected (above the configured *Servos* count). |

---

## Connecting your client

Configure your Python client (`config.py`) to point at the machine running
the simulator:

```python
DEFAULT_SERVER_IP   = "127.0.0.1"   # localhost if running on the same machine
DEFAULT_SERVER_PORT = 24642
```

Then use token **`sim01`** (hard-coded in the simulator) when registering as
master.
