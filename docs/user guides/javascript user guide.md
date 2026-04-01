#  How to script your bot Controller

## Connectivity

```
                                                               (your computer)
┌───────┐                       (your bot)                     ┌─────────────┐
│servoA ┼────────────┐        ┌──────────────┐                 │ your js code│
└───────┘            │        │        ┌────┐│                 │             │
┌───────┐            │        │   s5   │ .  ││                 │             │
│servoB ┼───────────┐│        │   s4   │ .  ││                 │             │
└───────┘           │└────────│──►s3   │ .  ││                 │            ---------- Gamepad
┌───────┐           └─────────│──►s2   │ .  ││  web socket     │             │
│servoC ┼─────────────────────│──►s1   │ .  ◄┼────[WiFi]───────►             │
└───────┘            ┌────────│──►s0   │ .  ││                 │             │
┌───────┐            │        │        │K10 ││                 │             │
│servoD ┼────────────┘        │ ext.   │board│                 │             │
└───────┘                     │ board  └────┘│                 │             │
                              └──────────────┘                 │             │
                                                               └─────────────┘
```
## Provided hardware
* ![Unihiker K10 board](./k10.png)  A Unihiker K10 board to be plugged in the Extension board, with pre-build C code that handles interaction with servos, dialog over HTTP and UDP.
* ![Green Servo](./svgreen64.png) 2 continuous rotation servo to be connected to the extension board. 
![Grey Servo](./svgrey64.png) 2 angular servo, to be connected to the extension board. 
*![Extension Board](./DFR1216Board.svg)" An extension board connected to the bot, with the servos attached to it, and defined in your code.

## Not provided hardware
![Gamepad](./gamepad.svg) Your game pad connected to your computer. Control can also be coded using keyboard.

## JS Public documentation of interest
* [Gamepad API - MDN](https://developer.mozilla.org/en-US/docs/Web/API/Navigator/getGamepads)
* [WebSocket API - MDN](https://developer.mozilla.org/en-US/docs/Web/API/WebSocket)

---

# 🚀 Quick Start (5 minutes)

**Just want to move a servo right now?** This section gets you there in 6 steps.
## Step 0: Connect to your Wifi access point
At boot time, the board will try to connect to store Wifi access point.
If there's none, or if connection fails, your board will start its own access point wich name is displayed in UI.
In this case, connect to this access point `amaker-xxxxxx`, and open the [board home page at 192.168.4.1](http://192.168.4.1) to define the Wifi SSID and pasword to connect to : 
1. register as master
2. update and save wifi settings.
3. reboot the board.



## Step 1: Open the Script Editor
1. Navigate to **📝 Script** tab in the web interface
2. You'll see a text editor with example code

## Step 2: Register as Master



In your code, use the `getBotControl(ip, port )` function to connect and register in one call:
```javascript
// Connect to bot and register as master - all in one line!
await getBotControl('192.168.4.1', '81', 'mytoken');
_scriptLog('✓ Connected and registered!');
```

**Why?** You must register before controlling servos. This prevents accidental commands.

## Step 3: Attach Your First Servo
In the **Script Editor**, paste this code:

```javascript
// Attach a servo to channel 0 (green continuous rotation servo)
attachServo(0, SERVO_TYPES.ROTATIONAL);
_scriptLog('✓ Servo 0 attached and ready to move!');
```

Click **▶️ Run** and check the output console.

**What happens?** Your servo is now registered and ready to receive commands.

## Step 4: Move the Servo
Replace the code with:

```javascript
// Move servo 0 forward at full speed
setServoSpeeds([[0, 100]]);
_scriptLog('→ Servo 0 moving forward');
```

Click **▶️ Run**. Your servo should move!

**Expected result:**
- Green servo spins in one direction
- Output shows: `→ Servo 0 moving forward`
- Bot Status shows heartbeat is active

## Step 5: Stop the Servo
Replace the code with:

```javascript
// Stop servo 0
setServoSpeeds([[0, 0]]);
_scriptLog('⏸ Servo 0 stopped');
```

Click **▶️ Run**. The servo stops.

---

## 🎉 You Did It!

You've just controlled your bot! You learned:
- ✅ Register as Master (required first)
- ✅ Attach servos with `attachServo()`
- ✅ Move servos with `setServoSpeeds()`
- ✅ Stop servos by setting speed to 0

---

## Next Steps

**Want to add a second servo?**
```javascript
attachServo(0, SERVO_TYPES.ROTATIONAL);   // Left wheel
attachServo(1, SERVO_TYPES.ROTATIONAL);   // Right wheel
setServoSpeeds([[0, 100], [1, -100]]);      // Turn left
_scriptLog('↙ Turning left');
```

**Want to use gamepad/keyboard control?**
→ See the full **Coding Guide** below for step-by-step instructions

**Need help?**
→ Jump to **Troubleshooting** section or check your **Bot Status** panel for errors

---

# 📚 Learning Path (Choose Your Level)

This guide is organized by **complexity level** with time estimates. Pick where you are:

| Level | Time | What You'll Learn | For You If... |
|-------|------|-------------------|---------------|
| **🚀 Quick Start** | 5 min | Move a servo instantly | You want immediate results |
| **❓ Troubleshooting** | 2 min | Fix common problems | Something isn't working |
| **🔰 Basics** | 15 min | Attach & control multiple servos | You're new to coding this bot |
| **🕹️ Input Control** | 20 min | Gamepad & keyboard input | You want real-time control |
| **🤖 Advanced** | 45 min | State machines, optimization | You're building complex behaviors |
| **📖 Reference** | Lookup | Function signatures, constants | You need quick answers |

**Already did Quick Start?** → Jump to **❓ Troubleshooting** if stuck, else start **🔰 Basics**

---

# ❓ TROUBLESHOOTING (Read When Stuck)

**Something isn't working?** Check this section first - answers to the most common problems.

## Quick Diagnosis

**Select the symptom you're seeing:**

1. [🔴 Nothing happens when I run code](#nothing-happens)
2. [🔴 Bot says "Not Registered" or won't connect](#not-registered)
3. [🔴 Servo attached but doesn't move](#servo-not-moving)
4. [🔴 Gamepad not detected](#gamepad-not-detected)
5. [🔴 JavaScript error messages](#javascript-errors)
6. [🔴 Function not found error](#function-not-found)
7. [🔴 Why are there two setServoSpeeds()?](#two-functions)

---

## <a id="nothing-happens"></a>🔴 Nothing Happens When I Run Code

**Symptom**: Click ▶️ Run, but nothing happens. Output console is empty.

**Checklist:**

1. ✅ **Check the Output Console**
   - Does it show ANY messages?
   - If not, check browser console (F12 → Console)
   - Look for red error messages

2. ✅ **Check for Syntax Errors**
   ```javascript
   // Common mistakes:
   setServoSpeeds[0][100];       // ❌ Wrong: missing parentheses
   setServoSpeeds([[0, 100]]    // ❌ Wrong: missing closing )
   const x = { a: 1 b: 2 };    // ❌ Wrong: missing comma
   ```
   **Fix**: Check browser console for red errors, fix typos

3. ✅ **Try a Simple Test**
   ```javascript
   _scriptLog('Test message');
   ```
   If this doesn't print, something is very wrong.

---

## <a id="not-registered"></a>🔴 Bot Says "Not Registered" or Won't Connect

**Symptom**: Status shows "Registered: No" or "Connected: No"

**Solution:**

1. ✅ **Fill in Bot IP** → Default: `192.168.4.1`
2. ✅ **Fill in WebSocket Port** → Default: `81`
3. ✅ **Enter Master Token** → Any 5 letters/numbers (e.g., `abc12`)
4. ✅ **Click "Register as Master"** → Wait for status change

**If still not connecting:**
- Is bot WiFi on?
- Are you on the same WiFi as the bot?
- Is IP address correct?

---

## <a id="servo-not-moving"></a>🔴 Servo Attached but Doesn't Move

**Symptom**: `attachServo()` works but servo won't move

**Checklist:**

1. ✅ **Did you register as Master first?** (REQUIRED)
   ```javascript
   registerMaster("mytoken");  // Must do this first!
   attachServo(0, SERVO_TYPES.ROTATIONAL);
   ```

2. ✅ **Check Bot Status Panel**
   - "Registered:" = **Yes**
   - "Connected:" = **Yes**
   - "Heartbeat:" = **Active**

3. ✅ **Check Servo Channel** (Valid: 0-5)
   ```javascript
   attachServo(0, SERVO_TYPES.ROTATIONAL);  // Channel 0
   ```

4. ✅ **Check Servo Type**
   ```javascript
   // Green servo = ROTATIONAL
   attachServo(0, SERVO_TYPES.ROTATIONAL);   ✅
   
   // Grey servo = ANGULAR_270
   attachServo(2, SERVO_TYPES.ANGULAR_270);  ✅
   ```

5. ✅ **Check Speed Values** (Valid: -100 to +100)
   ```javascript
   setServoSpeeds([[0, 100]]);    // ✅ Valid
   setServoSpeeds([[0, -100]]);   // ✅ Valid
   setServoSpeeds([[0, 150]]);    // ❌ Too high
   ```

---

## <a id="gamepad-not-detected"></a>🔴 Gamepad Not Detected

**Symptom**: Controller shows "Not connected"

**Solution:**

1. ✅ **Press a Button**
   - Gamepad detection waits for button press
   - Push ANY button on controller

2. ✅ **Use Chrome/Firefox**
   - Xbox/PlayStation work best
   - Safari has limited support

3. ✅ **Try Different USB Port**
   - Unplug and replug into different port
   - Press button again

4. ✅ **Check Compatibility**
   - Some generic gamepads need driver updates
   - Try on different computer if possible

---

## <a id="javascript-errors"></a>🔴 JavaScript Error Messages

**Symptom**: Red text in browser console (F12)

**Common Fixes:**

| Error | Problem | Fix |
|-------|---------|-----|
| `Cannot read property 'ROTATIONAL' of undefined` | SERVO_TYPES not loaded | Check both scripts loaded |
| `setServoSpeeds is not defined` | Typo or not loaded | Check spelling & scripts |
| `SyntaxError: Unexpected token` | Typo in code | Check line number for missing `,` `]` `}` `"` |
| `Maximum call stack exceeded` | Infinite loop | Check for while(true) without break |

**How to fix:**
1. Find line number in error
2. Look at that line for typos
3. Check examples in Code Templates

---

## <a id="function-not-found"></a>🔴 Function Not Found Error

**Common Typos:**

| Trying | Should Be | Fix |
|--------|-----------|-----|
| `setServoSpeed()` | `setServoSpeeds()` | Add **s** (plural) |
| `attachServos()` | `attachServo()` | Remove **s** (singular) |
| `SERVO_TYPE` | `SERVO_TYPES` | Add **S** (plural) |
| `ANGULAR_290` | `ANGULAR_270` | Should be **270** |

---

## <a id="two-functions"></a>🔴 Why Two setServoSpeeds() Functions?

**Answer**: Two DIFFERENT MODES exist:

### Mode 1: Fire-and-Forget ⚡ (Real-Time)
```javascript
setServoSpeeds([[0, 100]]);  // Returns immediately
// Use in: Gamepad/keyboard loops (needs speed)
```

### Mode 2: Request-Response 📊 (Feedback)
```javascript
const resp = await requestSetServoSpeeds([[0, 100]]);  // Waits for response
// Use when you need confirmation
```

**Quick Rule:**
- **🕹️ Gamepad/keyboard?** → Use `setServoSpeeds()`
- **🔘 Need confirmation?** → Use `requestSetServoSpeeds(pairs[])`

---

## Still Stuck?

1. 📖 Check **Debugging Guide** section
2. 👀 Use `_scriptLog()` to debug
3. 🔍 Open F12 console for errors
4. 💾 Copy from **Code Templates**
5. 📚 Search **Function Quick Reference**

---

# 🔰 BASICS (15 minutes)  
## Introduction

Your bot runs **two JavaScript files** that work together:

### BotScript.js (Infrastructure Layer)
Handles all low-level communication and heartbeat management:
- **Heartbeat protocol**: Sends `0x43` HEARTBEAT command every 40ms to keep the bot alive
- **Watchdog safety**: Bot automatically stops all servos if heartbeat is missing for >50ms
- **WebSocket management**: Establishes and maintains connection to the bot
- **Ping monitoring**: Tracks connection latency and stability
- **Fire-and-forget messaging**: Ensures rapid control commands aren't delayed by waiting for responses

**You don't need to modify this file** — it's pre-configured and handles everything automatically.

### BotScriptActions.js (Control Layer)
Pure infrastructure for gamepad/keyboard input handling:
- **Gamepad polling**: Reads Xbox/PlayStation/Switch/Generic gamepad input at display refresh rate with 50ms throttling
- **Button state tracking**: Detects button press/release events (rising/falling edges)
- **Analog stick handling**: Reads continuous stick values with deadzone filtering
- **Servo control functions**: Provides `setServoAngle()` and `setServoSpeeds()` for commanding servos
- **Empty override hooks**: Three functions waiting for you to define in your HTML page:
  - `processGamepadInput(gamepad)` — Map gamepad buttons/sticks to actions
  - `onKeyDown(event)` — Map keyboard key presses to actions
  - `onKeyUp(event)` — Map keyboard key releases to actions

**All custom logic goes in your HTML page** — BotScriptActions.js is infrastructure only.

### Your Workflow
1. Define your servo pin assignments and types (Step 0-1)
2. Create action functions that call `setServoAngle()` and `setServoSpeeds()` (Step 2)
3. **In your HTML page**, override `processGamepadInput()`, `onKeyDown()`, and `onKeyUp()` to map inputs to your actions (Step 3)

The heartbeat and WebSocket communication happen automatically in the background. All your custom code lives in the HTML page.

---

---

# 💻 Programmatic Connection with getBotControl()

**Don't want to click buttons on the web UI?** You can connect and register as master programmatically using the `getBotControl()` function. Perfect for scripts, automation, and headless operation.

## One-Line Connection

Instead of clicking the Control Panel buttons, call this function:

```javascript
// Connect to bot, set IP/port, and register as master - all at once!
await getBotControl('192.168.4.1', '81', 'mytoken');

// Now you can immediately control servos without any UI clicks
attachServo(0, SERVO_TYPES.ROTATIONAL);
setServoSpeeds([[0, 100]]);
_scriptLog('✓ Connected and controlling servo!');
```

## What getBotControl() Does

`getBotControl()` is a convenience function that:

1. **Sets global connection parameters** - Stores IP and port so all functions use them
2. **Initializes WebSocket** - Connects to the bot via WebSocket
3. **Registers as Master** - Authenticates with your token
4. **Sets UI fields** - Updates HTML form fields for consistency (optional for headless)

**Step by step:**
```javascript
// Step 1: Set connection parameters programmatically
// (Instead of reading from HTML form)
gBotIp = '192.168.4.1';
gBotPort = '81';

// Step 2: Initialize WebSocket connection
// (Establishes ws://192.168.4.1:81/ws)
await initializeWebSocket();

// Step 3: Register as Master
// (Sends master token for authentication)
await registerMaster('mytoken');

// Result: You're now registered and can control servos!
```

## Function Signature

```javascript
/**
 * Connect to bot and register as master (programmatic approach)
 * @param {string} ip - Bot IP address (e.g., "192.168.4.1")
 * @param {string} port - WebSocket port (e.g., "81")
 * @param {string} masterToken - Master authentication token (e.g., "abc12")
 * @returns {Promise<void>} - Resolves when connected and registered
 * @throws {Error} - If connection or registration fails
 */
async function getBotControl(ip, port, masterToken)
```

## Usage Examples

### Simple Connection
```javascript
// Basic usage - connect to bot and register
await getBotControl('192.168.4.1', '81', 'abc12');
_scriptLog('✓ Ready to control!');
```

### With Error Handling
```javascript
try {
  await getBotControl('192.168.4.1', '81', 'mytoken');
  _scriptLog('✓ Connected successfully');
  
  // Now control servos
  attachServo(0, SERVO_TYPES.ROTATIONAL);
  setServoSpeeds([[0, 100]]);
} catch (error) {
  _scriptLog('❌ Connection failed: ' + error.message);
  // Handle error - maybe retry or use fallback
}
```

### Dynamic IP from Variable
```javascript
const botIp = '192.168.4.1';  // Could come from config, user input, etc.
const botPort = '81';
const token = 'mytoken';

await getBotControl(botIp, botPort, token);
_scriptLog('✓ Connected to ' + botIp);
```

### Multiple Bots (Sequential)
```javascript
// Control bot 1
await getBotControl('192.168.4.1', '81', 'bot1token');
setServoSpeeds([[0, 100]]);
await delay(2000);

// Switch to bot 2
await getBotControl('192.168.1.179', '81', 'bot2token');
setServoSpeeds([[1, 50]]);
```

## When to Use

### ✅ Use getBotControl() When:
- You're writing **scripts** (not interactive web UI)
- You want to **skip manual UI clicks**
- You need **programmatic control** of connection parameters
- You're testing or **automating** bot operations
- You want **headless operation** (no web UI needed)
- You need to **reconnect dynamically** to different bots

### ❌ Use Web UI When:
- You want **interactive manual control** (clicking buttons)
- Non-technical users need to **operate the bot**
- You prefer **visual feedback** on buttons
- You want **persistent connection** through UI session

## Comparison: UI vs Programmatic

| Aspect | Web UI | getBotControl() |
|--------|--------|-----------------|
| **Method** | Click buttons, fill forms | Call function |
| **Connection** | Manual with Control Panel | Automatic, one call |
| **Setup Time** | 3-4 clicks | 1 line of code |
| **Best For** | Interactive use | Scripts & automation |
| **Error Feedback** | Visual status panel | try/catch error handling |
| **Default values** | Pre-filled in form | Your choice |
| **Headless** | Requires browser | Works without UI |

## Global Variables

`getBotControl()` sets these globals that all other functions use:

```javascript
gBotIp = '192.168.4.1'    // Bot IP address
gBotPort = '81'               // WebSocket port
```

All subsequent calls to functions like `setServoSpeeds()`, `attachServo()`, etc. will use these globals automatically. No need to pass IP/port around.

---

# 🔰 BASICS (15 minutes)

Start here after the Quick Start. You'll learn to attach multiple servos and control them.

## Step 0 : your servos

### Why Debug?

When "nothing happens," debugging helps you figure out **why**. Use logging to track what's actually running in your code.

### Using `_scriptLog()`

The `_scriptLog()` function prints messages to the **Output Console** in the Script Editor. Use it to track what your code is doing.

```javascript
_scriptLog('Starting servo setup...');
attachServo(0, SERVO_TYPES.ROTATIONAL);
_scriptLog('Servo 0 attached');

_scriptLog('Moving servo 0...');
setServoSpeeds([[0, 100]]);
_scriptLog('Command sent');
```

**Output:**
```
Starting servo setup...
Servo 0 attached
Moving servo 0...
Command sent
```

### What to Log

**Log these things to debug problems:**

```javascript
// 1. Track function execution
_scriptLog('→ Function moveForward() called');

// 2. Check variable values
const speed = 100;
_scriptLog('Speed value: ' + speed);

// 3. Confirm servo commands
_scriptLog('Setting servo 0 to speed ' + speed);
setServoSpeeds([[0, speed]]);

// 4. Check connection status
_scriptLog('Is master registered: ' + isMasterRegistered);
_scriptLog('Gamepad connected: ' + gamepadConnected);

// 5. Track timing
_scriptLog('Starting movement...');
await delay(1000);
_scriptLog('Movement complete');

// 6. Use emojis for clarity
_scriptLog('✓ Setup complete');
_scriptLog('❌ Error: servo not attached');
_scriptLog('⚠️ Warning: speed out of range');
_scriptLog('🔍 Debug: checking gamepad state');
```

### Common Debugging Patterns

**Check if a function is being called:**
```javascript
function moveForward() {
  _scriptLog('✓ moveForward() called');  // Add this line
  setServoSpeeds([[0, 100], [1, 100]]);
}

// Call the function
moveForward();

// Check output console - should show "✓ moveForward() called"
```

**Check if a condition is true:**
```javascript
if (isMasterRegistered) {
  _scriptLog('✓ Master is registered, proceeding...');
  moveForward();
} else {
  _scriptLog('❌ ERROR: Not registered as master!');
  _scriptLog('   Click "Register as Master" first');
}
```

**Check array values:**
```javascript
const pairs = [[0, 100], [1, -100]];
_scriptLog('Pairs: ' + JSON.stringify(pairs));
setServoSpeeds(pairs);
```

**Track gamepad input:**
```javascript
CUSTOMCONTROL.processGamepadInput = function(gamepad) {
  _scriptLog('Gamepad input detected');
  _scriptLog('Button A pressed: ' + gamepad.buttons[XBOX_BUTTONS.A].pressed);
  _scriptLog('Left stick X: ' + gamepad.axes[STICK_AXES.LEFT_X]);
  
  if (gamepad.buttons[XBOX_BUTTONS.A].pressed) {
    _scriptLog('→ A button triggered moveForward()');
    moveForward();
  }
};
```

### Troubleshooting Checklist

**Unable to connect**
1. ✅ **Ensure you use http, not https**
   - there is no https on the board.
   - some browser option may force https : ensure it's not your case.
2. ✅ **Verify the network name** 
   - Ensure you're connected on same network
3. ✅ **Verify the IP of board** 
   - Ensure you're using the correct IP


**If "nothing happens" when you click Run:**

1. ✅ **Check the Output Console**
   - Do you see ANY messages from `_scriptLog()`?
   - If not, your code may have a syntax error
   - **Fix**: Check for typos, missing commas, unclosed brackets

2. ✅ **Check if Master is Registered**
   ```javascript
   _scriptLog('Master registered: ' + isMasterRegistered);
   ```
   - Should show `true`
   - If `false`, you must click "Register as Master" first

3. ✅ **Check Bot Status Panel**
   - **Connected**: Should be "Yes"
   - **Registered**: Should be "Yes"
   - **Heartbeat**: Should be "Active"
   - If any are "No", connection is broken

4. ✅ **Check if Servo is Attached**
   ```javascript
   _scriptLog('About to attach servo 0');
   attachServo(0, SERVO_TYPES.ROTATIONAL);
   _scriptLog('Servo 0 should now be attached');
   ```

5. ✅ **Check if Servo Command was Sent**
   ```javascript
   _scriptLog('Before: about to move servo');
   setServoSpeeds([[0, 100]]);
   _scriptLog('After: command sent (no response expected - fire-and-forget)');
   ```

6. ✅ **Check Servo Channel Number**
   ```javascript
   _scriptLog('Moving servo on channel 0');
   setServoSpeeds([[0, 100]]);  // Channel 0 should exist
   // Valid channels: 0, 1, 2, 3, 4, 5
   ```

7. ✅ **Check Speed Values**
   ```javascript
   const speed = 100;
   _scriptLog('Speed must be -100 to +100, using: ' + speed);
   if (speed < -100 || speed > 100) {
     _scriptLog('❌ ERROR: Speed out of range!');
   } else {
     setServoSpeeds([[0, speed]]);
   }
   ```

### Browser Console Debugging

For advanced debugging, open the **Browser Developer Tools**:
1. Press **F12** (or Ctrl+Shift+I on Linux)
2. Go to **Console** tab
3. You'll see all JavaScript errors
4. Look for red error messages with line numbers

**Common errors:**
```
ReferenceError: attachServo is not defined
→ Fix: Check if BotScriptActions.js loaded (should auto-load)

TypeError: Cannot read property 'ROTATIONAL' of undefined
→ Fix: Check spelling: SERVO_TYPES (not SERVO_TYPE)

SyntaxError: Unexpected token
→ Fix: Check for missing commas, brackets, or quotes
```

### Error Handling with Try-Catch

For robustness, wrap risky code in try-catch:

```javascript
try {
  _scriptLog('Attempting to attach servo 0...');
  attachServo(0, SERVO_TYPES.ROTATIONAL);
  _scriptLog('✓ Servo attached successfully');
} catch (error) {
  _scriptLog('❌ Error attaching servo: ' + error.message);
  _scriptLog('   Details: ' + error);
}

try {
  _scriptLog('Moving servo 0...');
  setServoSpeeds([[0, 100]]);
  _scriptLog('✓ Movement command sent');
} catch (error) {
  _scriptLog('❌ Error moving servo: ' + error.message);
}
```

### Performance: Logging in Loops

**Don't log every single gamepad input** - it creates too much output:

```javascript
// ❌ BAD - logs 50 times per second
CUSTOMCONTROL.processGamepadInput = function(gamepad) {
  _scriptLog('Gamepad polled');  // Too much spam!
  setServoSpeeds([[0, gamepad.axes[0] * 100], [1, gamepad.axes[1] * 100]]);
};

// ✅ GOOD - only log when button pressed
CUSTOMCONTROL.processGamepadInput = function(gamepad) {
  if (gamepad.buttons[XBOX_BUTTONS.A].pressed) {
    _scriptLog('A button pressed - moving forward');
    moveForward();
  }
};
```

---

# 📖 REFERENCE (Lookup Anytime)

Quick lookup tables and reference material. Use when you need to find something fast.

## 🔧 Function Quick Reference

**Use this table to quickly find what you need.** For detailed info, click the function name or search the guide.

### Connection & Setup

| Function | Call When | Module | Example | Returns |
|----------|-----------|--------|---------|---------|
| `getBotControl(ip, port, token)` | **Easiest way to connect programmatically** | BotScript.js | `await getBotControl('192.168.4.1', '81', 'abc12')` | Promise |
| `registerMaster(token)` | **FIRST** - before anything (alternative to getBotControl) | BotScript.js | `registerMaster("abc12")` | Promise |
| `unregisterMaster()` | Cleanup, when done | BotScript.js | `unregisterMaster()` | Promise |
| `attachServo(channel, type)` | Setup, before moving | BotScriptActions.js | `attachServo(0, SERVO_TYPES.ROTATIONAL)` | void |
| `detectGamepad()` | Optional, when gamepad not detected | BotScriptActions.js | `detectGamepad()` | void |

### Servo Control (Fire-and-Forget) ⚡

| Function | Use Case | Module | Example | Returns |
|----------|----------|--------|---------|---------|
| `setServoSpeeds(pairs[])` | Move rotational servos immediately | BotScriptActions.js | `setServoSpeeds([[0,100],[1,-100]])` | void |
| `setServoAngle(channel, angle)` | Move angular servos immediately | BotScriptActions.js | `setServoAngle(2, 90)` | void |

### Servo Control (Request-Response with Feedback) 📊

| Function | Use Case | Module | Example | Returns |
|----------|----------|--------|---------|---------|
| `requestSetServoSpeeds(pairs[])` | Confirmed speed control | BotScript.js | `await requestSetServoSpeeds([[0,100]])` | Promise<number\|null> |
| `requestSetServoAngles(pairs[])` | Confirmed angle control | BotScript.js | `await requestSetServoAngles([[2,90]])` | Promise<number\|null> |
| `attachServos()` | UI-based servo attachment | BotScript.js | `attachServos()` | Promise |
| `getBattery()` | Query battery level | BotScript.js | `getBattery()` | Promise |

### Gamepad Input (Hook Functions) 🕹️

| Hook | Called When | Module | Example | What to Do |
|------|-------------|--------|---------|-----------|
| `CUSTOMCONTROL.onKeyDown(event)` | Key pressed | BotScriptActions.js | Override to handle key presses | Call your action functions |
| `CUSTOMCONTROL.onKeyUp(event)` | Key released | BotScriptActions.js | Override to handle key releases | Stop or change state |
| `CUSTOMCONTROL.processGamepadInput(gamepad)` | Gamepad button/stick moved | BotScriptActions.js | Override to handle gamepad | Call your action functions |
| `handleGamepaButton(gamepad, btnIdx, name, onPress, onRelease)` | Helper for button detection | BotScriptActions.js | `handleGamepaButton(gamepad, XBOX_BUTTONS.A, 'A', moveForward, stop)` | void |

### Utility Functions

| Function | Purpose | Module | Example | Returns |
|----------|---------|--------|---------|---------|
| `_scriptLog(message)` | Print debug message | BotScript.js | `_scriptLog('Hello!')` | void |
| `delay(milliseconds)` | Sleep for time period | BotScript.js | `await delay(1000)` | Promise |
| `setBotName(name)` | Set bot display name | BotScript.js | `setBotName("MyBot")` | Promise |
| `setScreen(index)` | Jump to screen (0-5) | BotScript.js | `setScreen(0)` | Promise |
| `nextScreen()` | Go to next screen | BotScript.js | `nextScreen()` | Promise |
| `previousScreen()` | Go to previous screen | BotScript.js | `previousScreen()` | Promise |

### Constants You'll Use

| Constant | Purpose | Values | Module |
|----------|---------|--------|--------|
| `SERVO_TYPES` | Servo type identifier |  `ANGULAR_270`, `ROTATIONAL` | BotScriptActions.js |
| `XBOX_BUTTONS` | Xbox button identifier | `A`, `B`, `X`, `Y`, `DPAD_UP`, `DPAD_DOWN`, etc. | BotScriptActions.js |
| `STICK_AXES` | Xbox stick axis | `LEFT_X`, `LEFT_Y`, `RIGHT_X`, `RIGHT_Y` | BotScriptActions.js |
| `PLAYSTATION_BUTTONS` | PlayStation button identifier | `X`, `CIRCLE`, `SQUARE`, `TRIANGLE`, etc. | BotScriptActions.js |
| `NINTENDO_SWITCH_BUTTONS` | Nintendo button identifier | `B`, `A`, `Y`, `X`, etc. | BotScriptActions.js |

### Decision Tree: Which Function to Use?

**I want to move a servo RIGHT NOW:**
→ Use `setServoSpeeds()` or `setServoAngle()` (fire-and-forget)

**I want to move a servo and see confirmation:**
→ Use `requestSetServoSpeeds(pairs[])` or `requestSetServoAngles(pairs[])` (request-response)

**I want gamepad buttons to trigger actions:**
→ Override `CUSTOMCONTROL.processGamepadInput()` and call your servo functions

**I want keyboard keys to trigger actions:**
→ Override `CUSTOMCONTROL.onKeyDown()` and `CUSTOMCONTROL.onKeyUp()`

**I want to debug my code:**
→ Use `_scriptLog()` to print messages to the Output Console

**I want to wait between actions:**
→ Use `await delay(milliseconds)` to pause execution

---

# 🤖 ADVANCED: Complex Behaviors

Learn advanced patterns for building sophisticated behaviors.

## ⏱️ Understanding Asynchronous Behavior

### The Problem

When you call a servo function, **does it wait for the servo to move, or does it return immediately?**

Different functions behave differently:
- **Fire-and-forget**: Returns immediately (doesn't wait)
- **Request-response**: Waits for confirmation (blocks execution)

This can confuse beginners. This section explains when each happens.

---

### Fire-and-Forget Mode ⚡ (Fastest)

**Functions**: `setServoSpeeds()`, `setServoAngle()`, `attachServo()`

**Timeline**: <1 millisecond

```
Your Code                   Bot
═════════════════════════════════════════════════════════════
setServoSpeeds([[0, 100]])
│
├─ Send command packet to bot (WiFi)     ────────────────→
│                                         (~50-200ms over network)
└─ Return IMMEDIATELY                     ✓ Code continues
  (no waiting for response)                (bot processes packet later)
```

**What happens:**
1. Function sends packet to bot
2. **Returns immediately** (doesn't wait)
3. Code keeps running
4. Bot processes command asynchronously

**Execution flow in your code:**
```javascript
_scriptLog('Before call');
setServoSpeeds([[0, 100]]);  // Returns immediately!
_scriptLog('After call');    // Prints RIGHT AWAY

// Output:
// Before call
// After call
// (servo starts moving a moment later, asynchronously)
```

**Use fire-and-forget when:**
- ✅ You're in a gamepad/keyboard loop (needs to be fast)
- ✅ You want rapid successive commands
- ✅ You don't need confirmation
- ✅ You want real-time responsiveness

**Example: Gamepad polling at 50ms intervals**
```javascript
CUSTOMCONTROL.processGamepadInput = function(gamepad) {
  // This runs 20 times per second
  // Fire-and-forget is critical for smooth control!
  setServoSpeeds([[0, gamepad.axes[0] * 100], [1, gamepad.axes[1] * 100]]);
  // Returns immediately - next gamepad poll won't be blocked
};
```

---

### Request-Response Mode 📊 (Slower but Safe)

**Functions**: `requestSetServoSpeeds(pairs[])`, `requestSetServoAngles(pairs[])`, `attachServos()`, `getBattery()`

**Timeline**: 100-500 milliseconds (depends on network)

```
Your Code                   Bot
═════════════════════════════════════════════════════════════
requestSetServoSpeeds([[0, 100]])
│
├─ Send command packet ────────────────→ (WiFi, ~50-200ms)
│
├─ WAIT FOR RESPONSE ←────────────────── (WiFi, ~50-200ms)
│                                        Bot responds: "✓ OK"
├─ Receive response
│
├─ Show feedback: "✓ Speeds set"
│
└─ Return NOW                           ✓ Code continues
  (after confirmation)
```

**What happens:**
1. Function sends packet to bot
2. **Blocks execution** (waits for response)
3. Bot processes and sends response back
4. Code continues only after response received
5. Status is updated on UI

**Execution flow in your code:**
```javascript
_scriptLog('Before call');
const resp = await requestSetServoSpeeds([[0, 100]]);  // Waits for response!
_scriptLog('After call');    // Prints AFTER response arrives

// Output:
// Before call
// (waits ~100-500ms for bot response)
// ✓ Servo speeds set (feedback shown)
// After call
```

**Use request-response when:**
- ✅ You need confirmation the command worked
- ✅ You're responding to a button click (user expects feedback)
- ✅ You're doing sequential setup (attach → move → stop)
- ✅ You need to know if the command failed

**Example: Button click with feedback**
```javascript
async function onApplyButtonClicked() {
  _scriptLog('Setting servo speeds...');
  
  try {
    const resp = await requestSetServoSpeeds([[0, 100]]);  // Waits for response
    _scriptLog('✓ Successfully applied speeds');
    // Show success message to user
  } catch (error) {
    _scriptLog('❌ Failed to apply speeds: ' + error);
    // Show error message to user
  }
}
```

---

### Comparing the Two Modes

| Aspect | Fire-and-Forget ⚡ | Request-Response 📊 |
|--------|-------------------|-------------------|
| **Speed** | <1ms function call | 100-500ms (waits for response) |
| **Blocks execution** | No - returns immediately | Yes - waits for response |
| **Confirms success** | No - just sends | Yes - gets OK from bot |
| **Use case** | Gamepad/keyboard loops | Button clicks, setup |
| **Functions** | setServoSpeeds, setServoAngle | requestSetServoSpeeds, requestSetServoAngles |
| **Best for** | Real-time control | User interaction |

---

### Common Mistakes

**❌ Mistake 1: Expecting fire-and-forget to confirm**
```javascript
setServoSpeeds([[0, 100]]);
_scriptLog('Servo is now at speed 100');  // Wrong!
// The servo might not have started moving yet
// Fire-and-forget doesn't confirm
```

**✅ Correct:**
```javascript
setServoSpeeds([[0, 100]]);
_scriptLog('Speed command sent (will apply asynchronously)');
```

---

**❌ Mistake 2: Using request-response in a fast loop**
```javascript
// DON'T DO THIS - too slow!
CUSTOMCONTROL.processGamepadInput = function(gamepad) {
  await requestSetServoSpeeds([[0, 100]]);  // Waits 100-500ms each time
  // Gamepad polling can't keep up!
};
```

**✅ Correct:**
```javascript
// Use fire-and-forget in loops
CUSTOMCONTROL.processGamepadInput = function(gamepad) {
  setServoSpeeds([[0, gamepad.axes[0] * 100], [1, gamepad.axes[1] * 100]]);
  // Returns immediately, loop stays fast
};
```

---

**❌ Mistake 3: Forgetting `await` on request-response**
```javascript
async function setupServos() {
  requestSetServoSpeeds([[0, 100]]);  // Missing await!
  _scriptLog('Done');  // Prints immediately, before response
}
```

**✅ Correct:**
```javascript
async function setupServos() {
  await requestSetServoSpeeds([[0, 100]]);  // Wait for response
  _scriptLog('Done');  // Prints after response arrives
}
```

---

### When to Use Async/Await

**Use `async/await` when:**
- You're calling request-response functions
- You want to wait for one operation before starting another
- You need to catch errors with try-catch

**Example: Sequential setup**
```javascript
async function setupBot() {
  _scriptLog('Starting setup...');
  
  // These must happen in order
  attachServo(0, SERVO_TYPES.ROTATIONAL);
  attachServo(1, SERVO_TYPES.ROTATIONAL);
  
  await delay(500);  // Wait half a second
  
  _scriptLog('All servos attached, testing movement...');
  
  await requestSetServoSpeeds([[0, 100]]);  // Wait for this before continuing
  
  _scriptLog('✓ Setup complete!');
}
```

**No async/await needed when:**
- You're calling fire-and-forget functions
- You don't care about order (they're independent)
- You're in a gamepad/keyboard loop

**Example: Gamepad input (no async needed)**
```javascript
CUSTOMCONTROL.processGamepadInput = function(gamepad) {
  // No async, no await needed
  // Just send commands fire-and-forget
  setServoSpeeds([[0, leftSpeed], [1, rightSpeed]]);
};
```

---

Performance Characteristics & Best Practices

### Performance Metrics

Here's what to expect from the K10 bot system:

| Component | Metric | Value | Notes |
|-----------|--------|-------|-------|
| **Gamepad Input** | Polling rate | 50ms minimum | Don't poll faster than 50ms (screen refresh rate) |
| **Gamepad Input** | Button detection | <1ms | Immediate button press detection |
| **Heartbeat** | Interval | 40ms | Keeps connection alive (every 40ms) |
| **Servo Command** | Fire-and-forget execution | <1ms | Returns immediately, no wait |
| **Servo Command** | Request-response execution | 100-500ms | Waits for bot confirmation |
| **Network** | WebSocket roundtrip | 50-200ms | WiFi latency varies by distance |
| **Network** | Packet loss | ~0-5% | WiFi can lose occasional packets |
| **Bot Processing** | Max command rate | ~20 commands/second | Don't exceed this (one every 50ms) |
| **Display** | Refresh rate | 60 FPS | UI updates ~every 16ms |

---

### Performance Expectations by Use Case

**Real-time Gamepad Control** 🕹️
- Expected latency: **50-150ms** total
  - Gamepad poll: 0-50ms
  - Network: 50-100ms
  - Bot response: instant
- **Performance**: Feels responsive and smooth
- **Strategy**: Use fire-and-forget (setServoSpeeds)

**Button Click Actions** 🔘
- Expected latency: **200-600ms** total
  - Network: 50-200ms out
  - Bot processing: 10-50ms
  - Network: 50-200ms back
  - UI update: 16ms
- **Performance**: Slight delay is normal and expected
- **Strategy**: Use request-response (requestSetServoSpeeds) for feedback

**Sequential Commands** 📋
- Expected time for 3 operations: **300-1500ms** total
  - Each operation: 100-500ms
  - Chained together with await
- **Performance**: Noticeably slower, but reliable
- **Strategy**: Use request-response for setup sequences

**Rapid Servo Updates** ⚡
- Expected rate: **20 updates/second maximum**
  - At 50ms per poll: 20 commands
  - Any faster: commands get dropped
- **Performance**: Smooth motion, no stuttering
- **Strategy**: Stick to 50ms or slower polling

---

### Best Practices for Good Performance

#### ✅ DO: Use Fire-and-Forget in Gamepad Loops

```javascript
CUSTOMCONTROL.processGamepadInput = function(gamepad) {
  // This runs ~20 times per second
  const leftSpeed = gamepad.axes[STICK_AXES.LEFT_Y] * 100;
  const rightSpeed = gamepad.axes[STICK_AXES.RIGHT_Y] * 100;
  
  // Fire-and-forget - returns immediately
  setServoSpeeds([[0, leftSpeed], [1, rightSpeed]]);
  // Doesn't block, gamepad loop stays responsive
};
```

**Why**: Gamepad polling needs to be fast. Fire-and-forget keeps the loop responsive.

---

#### ✅ DO: Use Request-Response for Setup

```javascript
async function setupBot() {
  _scriptLog('Setting up...');
  
  // Use request-response to confirm each step
  await requestSetServoSpeeds([[0, 100]]);
  _scriptLog('✓ Speeds set');
  
  await delay(500);
  
  // Do next step only after previous one confirms
  _scriptLog('Setup complete!');
}
```

**Why**: Setup happens once, speed doesn't matter. Confirmation ensures success.

---

#### ❌ DON'T: Use Request-Response in Fast Loops

```javascript
// ❌ BAD - will cause lag!
CUSTOMCONTROL.processGamepadInput = function(gamepad) {
  await requestSetServoSpeeds([[0, 100]]);  // Waits 100-500ms each time
  // Gamepad polling can't keep up
  // Control feels sluggish and unresponsive
};
```

**Why**: Each await blocks for 100-500ms. Gamepad can only poll ~2 times per second. Control becomes unusable.

---

#### ❌ DON'T: Send Too Many Commands Per Second

```javascript
// ❌ BAD - exceeds maximum command rate
for (let i = 0; i < 100; i++) {
  setServoSpeeds([[0, 100]]);  // 100 commands instantly
  // Bot can only handle ~20/second
  // Commands get dropped or cause lag
}
```

**Why**: Bot has a command buffer limit. Excessive commands get dropped or cause delays.

---

#### ✅ DO: Throttle Rapid Operations

```javascript
// ✅ GOOD - respects 50ms minimum
async function quickTest() {
  for (let i = 0; i < 5; i++) {
    setServoSpeeds([[0, 100]]);
    await delay(50);  // Wait 50ms between commands
    setServoSpeeds([[0, 0]]);
    await delay(50);
  }
}
```

**Why**: 50ms minimum between commands matches gamepad polling rate and bot processing.

---

#### ✅ DO: Optimize Gamepad Input

```javascript
// ✅ GOOD - efficient gamepad handling
CUSTOMCONTROL.processGamepadInput = function(gamepad) {
  // Only update when there's actual input change
  const leftSpeed = Math.round(gamepad.axes[STICK_AXES.LEFT_Y] * 100);
  
  // Avoid sending identical commands
  if (leftSpeed !== lastLeftSpeed) {
    setServoSpeeds([[0, leftSpeed]]);
    lastLeftSpeed = leftSpeed;
  }
};
```

**Why**: Fewer commands = less network traffic, smoother control, lower latency.

---

#### ❌ DON'T: Log Everything in Fast Loops

```javascript
// ❌ BAD - logging slows things down
CUSTOMCONTROL.processGamepadInput = function(gamepad) {
  _scriptLog('Axis: ' + gamepad.axes[0]);      // Logs 20 times/second!
  _scriptLog('Button pressed: ' + button);     // Spam in console
  setServoSpeeds([[0, speed]]);
};
```

**Why**: Logging to console is slow. In fast loops, it causes noticeable lag.

---

#### ✅ DO: Log Selectively

```javascript
// ✅ GOOD - only log important events
CUSTOMCONTROL.processGamepadInput = function(gamepad) {
  if (gamepad.buttons[XBOX_BUTTONS.A].pressed) {
    _scriptLog('A button pressed - moving forward');  // Only when needed
    moveForward();
  }
};
```

**Why**: Logging only important events keeps console clean and doesn't slow code.

---

### Bottleneck Analysis

**What's the slowest part of the system?**

```
Gamepad Input (50ms) ←── Bottleneck! Limits responsiveness
         ↓
Send Command (~1ms)
         ↓
Network Latency (50-200ms) ←── Second bottleneck
         ↓
Bot Processing (10-50ms)
         ↓
Response (50-200ms) ←── For request-response only
         ↓
Display Update (16ms)
```

**How to work with bottlenecks:**

1. **Network latency (50-200ms)**: Accept this as-is, can't change
   - Use fire-and-forget to hide it
   - Don't expect instant feedback
   
2. **Gamepad polling (50ms)**: Already optimal
   - Don't send more than 20 commands/second
   - 50ms is good for controllers
   
3. **Bot processing**: Very fast (<50ms)
   - Not a bottleneck for normal use

---

### Optimization Tips

**Reduce Network Latency (slightly)**
```javascript
// ❌ BAD - sends every tiny movement
setServoSpeeds([[0, 99.999]]);  // Different every millisecond
setServoSpeeds([[0, 99.998]]);
setServoSpeeds([[0, 99.997]]);  // Each is separate network packet

// ✅ GOOD - round to reduce changes
const speed = Math.round(gamepad.axes[0] * 100);  // Round to integer
setServoSpeeds([[0, speed]]);  // Only send when value actually changes
```

**Reduce Command Overhead**
```javascript
// ❌ Less efficient - multiple calls per frame
setServoSpeeds([[0, speed0]]);
setServoSpeeds([[1, speed1]]);  // Two network packets

// ✅ GOOD - combine into one call
setServoSpeeds([[0, speed0], [1, speed1]]);  // One network packet
```

**Batch Setup Operations**
```javascript
// ❌ BAD - multiple waits
await requestSetServoSpeeds([[0, 100]]);
await delay(100);
await requestSetServoAngles([[2, 90]]);
await delay(100);
// Takes 200ms+ just for delays

// ✅ GOOD - do independent operations together
setServoSpeeds([[0, 100]]);     // Fire-and-forget
setServoAngle(2, 90);           // Fire-and-forget
// Both sent, completes in <1ms
```

---

### Acceptable Lag Guidelines

| Use Case | Acceptable Lag | Better | Excellent |
|----------|----------------|--------|-----------|
| Gamepad control | <300ms | <150ms | <100ms |
| Button feedback | <500ms | <300ms | <200ms |
| Console logging | <50ms delay | Instant | Instant |
| Servo movement | Visible | Smooth | Very smooth |
| Screen update | <100ms | <50ms | Real-time |

**Your system typically achieves:**
- Gamepad control: **100-150ms** ✅ (Excellent)
- Button feedback: **200-400ms** ✅ (Better)
- Servo movement: **Immediate** ✅ (Excellent)
- Screen update: <100ms

**Your system typically achieves:**
- Gamepad control: **100-150ms** ✅ (Excellent)
- Button feedback: **200-400ms** ✅ (Better)
- Servo movement: **Immediate** ✅ (Excellent)

---

---

## 💾 Code Templates (Copy-Paste Ready)

**Don't reinvent the wheel!** Use these ready-to-use templates as starting points for common tasks.

### Template 1: Simple D-Pad Movement

Use this for basic forward/back/left/right control.

```javascript
// Copy this into your Script Editor textarea
// Change LEFT_WHEEL and RIGHT_WHEEL to match your servo channels

const LEFT_WHEEL = 0;
const RIGHT_WHEEL = 1;

function moveForward() {
  setServoSpeeds([[LEFT_WHEEL, 100], [RIGHT_WHEEL, 100]]);
  _scriptLog('→ Moving forward');
}

function moveBackward() {
  setServoSpeeds([[LEFT_WHEEL, -100], [RIGHT_WHEEL, -100]]);
  _scriptLog('← Moving backward');
}

function turnLeft() {
  setServoSpeeds([[LEFT_WHEEL, -100], [RIGHT_WHEEL, 100]]);
  _scriptLog('↙ Turning left');
}

function turnRight() {
  setServoSpeeds([[LEFT_WHEEL, 100], [RIGHT_WHEEL, -100]]);
  _scriptLog('↘ Turning right');
}

function stop() {
  setServoSpeeds([[LEFT_WHEEL, 0], [RIGHT_WHEEL, 0]]);
  _scriptLog('⏸ Stopped');
}

// Attach servos
attachServo(LEFT_WHEEL, SERVO_TYPES.ROTATIONAL);
attachServo(RIGHT_WHEEL, SERVO_TYPES.ROTATIONAL);
_scriptLog('✓ Servos ready for D-Pad control');

// Handle D-Pad input
CUSTOMCONTROL.processGamepadInput = function(gamepad) {
  if (gamepad.buttons[XBOX_BUTTONS.DPAD_UP].pressed) {
    moveForward();
  } else if (gamepad.buttons[XBOX_BUTTONS.DPAD_DOWN].pressed) {
    moveBackward();
  } else if (gamepad.buttons[XBOX_BUTTONS.DPAD_LEFT].pressed) {
    turnLeft();
  } else if (gamepad.buttons[XBOX_BUTTONS.DPAD_RIGHT].pressed) {
    turnRight();
  } else {
    stop();
  }
};
```

---

### Template 2: Analog Stick Control (with Deadzone)

Use this for smooth, analog left stick control with deadzone to prevent drift.

```javascript
// Copy this into your Script Editor textarea
// Adjust deadzone (0.1 to 0.3) if stick drifts

const LEFT_WHEEL = 0;
const RIGHT_WHEEL = 1;
const DEADZONE = 0.15;  // Prevent stick drift (adjust if needed)

function setSpeed(leftSpeed, rightSpeed) {
  setServoSpeeds([[LEFT_WHEEL, leftSpeed], [RIGHT_WHEEL, rightSpeed]]);
}

// Attach servos
attachServo(LEFT_WHEEL, SERVO_TYPES.ROTATIONAL);
attachServo(RIGHT_WHEEL, SERVO_TYPES.ROTATIONAL);
_scriptLog('✓ Analog stick control ready');

// Handle analog stick input
CUSTOMCONTROL.processGamepadInput = function(gamepad) {
  // Read left stick axes
  let stickX = gamepad.axes[STICK_AXES.LEFT_X];  // -1 to +1
  let stickY = gamepad.axes[STICK_AXES.LEFT_Y];  // -1 to +1
  
  // Apply deadzone (ignore small movements from stick drift)
  if (Math.abs(stickX) < DEADZONE) stickX = 0;
  if (Math.abs(stickY) < DEADZONE) stickY = 0;
  
  // Convert to motor speeds (-100 to +100)
  // Differential steering: adjust left/right separately for turning
  const leftSpeed = Math.round((stickY - stickX) * 100);
  const rightSpeed = Math.round((stickY + stickX) * 100);
  
  // Clamp to valid range
  const leftClamped = Math.max(-100, Math.min(100, leftSpeed));
  const rightClamped = Math.max(-100, Math.min(100, rightSpeed));
  
  setSpeed(leftClamped, rightClamped);
};
```

---

### Template 3: Keyboard Control (WASD)

Use this for keyboard-based movement (W=forward, A=left, S=back, D=right).

```javascript
// Copy this into your Script Editor textarea

const LEFT_WHEEL = 0;
const RIGHT_WHEEL = 1;
const keys = {};  // Track which keys are pressed

function updateMovement() {
  const w = keys['w'];
  const a = keys['a'];
  const s = keys['s'];
  const d = keys['d'];
  
  let leftSpeed = 0;
  let rightSpeed = 0;
  
  if (w) {
    leftSpeed += 100;
    rightSpeed += 100;
  }
  if (s) {
    leftSpeed -= 100;
    rightSpeed -= 100;
  }
  if (a) {
    leftSpeed -= 50;
    rightSpeed += 50;
  }
  if (d) {
    leftSpeed += 50;
    rightSpeed -= 50;
  }
  
  setServoSpeeds([[LEFT_WHEEL, leftSpeed], [RIGHT_WHEEL, rightSpeed]]);
}

// Attach servos
attachServo(LEFT_WHEEL, SERVO_TYPES.ROTATIONAL);
attachServo(RIGHT_WHEEL, SERVO_TYPES.ROTATIONAL);
_scriptLog('✓ Keyboard control ready (W=forward, A=left, S=back, D=right)');

// Handle keyboard
CUSTOMCONTROL.onKeyDown = function(event) {
  const key = event.key.toLowerCase();
  keys[key] = true;
  updateMovement();
};

CUSTOMCONTROL.onKeyUp = function(event) {
  const key = event.key.toLowerCase();
  keys[key] = false;
  updateMovement();
};
```

---

### Template 4: Multi-Servo Coordination (Arms + Wheels)

Use this when you have both movement servos (wheels) and action servos (arms).

```javascript
// Copy this into your Script Editor textarea

const LEFT_WHEEL = 0;
const RIGHT_WHEEL = 1;
const LEFT_ARM = 2;
const RIGHT_ARM = 3;

// State tracking
let armState = 'resting';

function moveForward() {
  setServoSpeeds([[LEFT_WHEEL, 100], [RIGHT_WHEEL, 100]]);
}

function stop() {
  setServoSpeeds([[LEFT_WHEEL, 0], [RIGHT_WHEEL, 0]]);
}

function armUp() {
  setServoAngle(LEFT_ARM, 90);
  setServoAngle(RIGHT_ARM, 90);
  armState = 'up';
  _scriptLog('↑ Arms up');
}

function armDown() {
  setServoAngle(LEFT_ARM, -90);
  setServoAngle(RIGHT_ARM, -90);
  armState = 'down';
  _scriptLog('↓ Arms down');
}

function armRest() {
  setServoAngle(LEFT_ARM, 0);
  setServoAngle(RIGHT_ARM, 0);
  armState = 'resting';
  _scriptLog('→ Arms resting');
}

// Attach all servos
attachServo(LEFT_WHEEL, SERVO_TYPES.ROTATIONAL);
attachServo(RIGHT_WHEEL, SERVO_TYPES.ROTATIONAL);
attachServo(LEFT_ARM, SERVO_TYPES.ANGULAR_270);
attachServo(RIGHT_ARM, SERVO_TYPES.ANGULAR_270);
_scriptLog('✓ Ready: Wheels + Arms');

// Gamepad control
CUSTOMCONTROL.processGamepadInput = function(gamepad) {
  // D-Pad: Movement
  if (gamepad.buttons[XBOX_BUTTONS.DPAD_UP].pressed) {
    moveForward();
  } else {
    stop();
  }
  
  // Buttons: Arm control
  if (gamepad.buttons[XBOX_BUTTONS.Y].pressed) {
    armUp();
  } else if (gamepad.buttons[XBOX_BUTTONS.A].pressed) {
    armDown();
  } else if (gamepad.buttons[XBOX_BUTTONS.X].pressed) {
    armRest();
  }
};
```

---

### Template 5: State Machine (Complex Behavior)

Use this when your bot needs different modes (e.g., explore, capture, return).

```javascript
// Copy this into your Script Editor textarea

const LEFT_WHEEL = 0;
const RIGHT_WHEEL = 1;
const ARM = 2;

let currentState = 'idle';
let stateStartTime = 0;

function setState(newState) {
  currentState = newState;
  stateStartTime = performance.now();
  _scriptLog('→ State changed to: ' + newState);
}

async function idle() {
  if (currentState !== 'idle') return;
  stop();
  _scriptLog('Waiting for input...');
}

async function explore() {
  if (currentState !== 'explore') return;
  _scriptLog('Exploring...');
  
  moveForward();
  await delay(2000);  // Move for 2 seconds
  
  if (currentState === 'explore') {
    stop();
    _scriptLog('Exploration complete');
    setState('idle');
  }
}

async function capture() {
  if (currentState !== 'capture') return;
  _scriptLog('Capturing...');
  
  setServoAngle(ARM, 90);  // Arm up
  await delay(500);
  setServoAngle(ARM, 0);   // Arm down
  
  if (currentState === 'capture') {
    _scriptLog('Capture complete');
    setState('idle');
  }
}

async function returnHome() {
  if (currentState !== 'returnHome') return;
  _scriptLog('Returning home...');
  
  // Turn around
  setServoSpeeds([[LEFT_WHEEL, -100], [RIGHT_WHEEL, 100]]);
  await delay(1000);
  
  // Move back
  setServoSpeeds([[LEFT_WHEEL, -100], [RIGHT_WHEEL, -100]]);
  await delay(2000);
  
  stop();
  _scriptLog('Home!');
  setState('idle');
}

// Attach servos
attachServo(LEFT_WHEEL, SERVO_TYPES.ROTATIONAL);
attachServo(RIGHT_WHEEL, SERVO_TYPES.ROTATIONAL);
attachServo(ARM, SERVO_TYPES.ANGULAR_270);
_scriptLog('✓ State machine ready');

function moveForward() {
  setServoSpeeds([[LEFT_WHEEL, 100], [RIGHT_WHEEL, 100]]);
}

function stop() {
  setServoSpeeds([[LEFT_WHEEL, 0], [RIGHT_WHEEL, 0]]);
}

// Gamepad control state transitions
CUSTOMCONTROL.processGamepadInput = function(gamepad) {
  if (gamepad.buttons[XBOX_BUTTONS.A].pressed) {
    if (currentState !== 'explore') {
      setState('explore');
      explore();
    }
  } else if (gamepad.buttons[XBOX_BUTTONS.B].pressed) {
    if (currentState !== 'capture') {
      setState('capture');
      capture();
    }
  } else if (gamepad.buttons[XBOX_BUTTONS.X].pressed) {
    if (currentState !== 'returnHome') {
      setState('returnHome');
      returnHome();
    }
  } else if (gamepad.buttons[XBOX_BUTTONS.Y].pressed) {
    setState('idle');
  }
};
```

---

### Template 6: Button Detection Helper

Use this for clean button press/release detection without boilerplate.

```javascript
// Copy this into your Script Editor textarea

const LEFT_WHEEL = 0;
const RIGHT_WHEEL = 1;

function moveForward() { setServoSpeeds([[LEFT_WHEEL, 100], [RIGHT_WHEEL, 100]]); }
function moveBackward() { setServoSpeeds([[LEFT_WHEEL, -100], [RIGHT_WHEEL, -100]]); }
function stop() { setServoSpeeds([[LEFT_WHEEL, 0], [RIGHT_WHEEL, 0]]); }

// Attach servos
attachServo(LEFT_WHEEL, SERVO_TYPES.ROTATIONAL);
attachServo(RIGHT_WHEEL, SERVO_TYPES.ROTATIONAL);
_scriptLog('✓ Button detection ready');

// Clean button handling
CUSTOMCONTROL.processGamepadInput = function(gamepad) {
  handleGamepaButton(gamepad, XBOX_BUTTONS.DPAD_UP, 'UP', moveForward, stop);
  handleGamepaButton(gamepad, XBOX_BUTTONS.DPAD_DOWN, 'DOWN', moveBackward, stop);
  
  // For held buttons (Y = move forward, only while held)
  if (gamepad.buttons[XBOX_BUTTONS.Y].pressed) {
    moveForward();
  } else {
    stop();
  }
};
```

---

## Step 0 : your servos
There are 6 pins for servos on the extension board, so you can have up to 6 servos connected at the same time. You can use any of these pins for any type of servo (continuous rotation or angular), but make sure to define them correctly in your code.
All the servo API rely on connector number, and for code readability, we suggest to name your servos, so you can easily remember which one is which. 
In the following example, we're using green servos for left and right wheel, one gray is for an arm the other is mounted to a direction wheel.

```javascript
///
// Name your servos
const myServos = {
  LEFT_WHEEL: 0,  // Assuming left wheel servo is connected to pin 0
  RIGHT_WHEEL: 1, // Assuming right wheel servo is connected to pin 1
  ARM: 4,         // Assuming arm servo is connected to pin 2
  DIRECTION : 4,         // Assuming arm servo is connected to pin 2
}
``` 
## Step 1 : declare the servo type
For each servo you need to call the `attachServo(channel, type)` function to register it with the bot.
Reminder : green servos are continuous rotation ones (ROTATIONAL), grey servos are angular 270 ones (ANGULAR_270). 

```javascript
// First, define your servo pin assignments
const myServos = {
  LEFT_WHEEL: 0,      // Continuous rotation servo
  RIGHT_WHEEL: 1,     // Continuous rotation servo
  ARM: 4,             // Angular 270 servo
  DIRECTION: 5        // Angular 270 servo
};

// Then, declare the servo types to the bot
attachServo(myServos.LEFT_WHEEL, SERVO_TYPES.ROTATIONAL);
attachServo(myServos.RIGHT_WHEEL, SERVO_TYPES.ROTATIONAL);
attachServo(myServos.ARM, SERVO_TYPES.ANGULAR_270);
attachServo(myServos.DIRECTION, SERVO_TYPES.ANGULAR_270);
```
## Step 2 : create your actions code
You can use `lambda functions` to define your actions, or you can use regular function definitions. 
The choice is yours. Using lambda functions can make your code more concise and easier to read.

Setting speed in a function becomes very handy when you realize you did not take care about rotation direction of your servo wheels : instead of dismounting and remounting all your bot body, you can swap rotation direction in code.

```javascript

const moveForward = () => {
  // Move both wheels forward at full speed
  setServoSpeeds([[myServos.LEFT_WHEEL, 100], [myServos.RIGHT_WHEEL, 100]]);
}

const moveBackward = () => {
  // Move both wheels backward at full speed
  setServoSpeeds([[myServos.LEFT_WHEEL, -100], [myServos.RIGHT_WHEEL, -100]]);
}

const rotateClockwise = () => {
  // Rotate the bot clockwise (left forward, right backward)
  setServoSpeeds([[myServos.LEFT_WHEEL, 100], [myServos.RIGHT_WHEEL, -100]]);
}

const rotateCounterClockwise = () => {
  // Rotate the bot counter-clockwise (left backward, right forward)
  setServoSpeeds([[myServos.LEFT_WHEEL, -100], [myServos.RIGHT_WHEEL, 100]]);
}

const stop = () => {
  // Stop both wheels
  setServoSpeeds([[myServos.LEFT_WHEEL, 0], [myServos.RIGHT_WHEEL, 0]]);
}

const shoot = () => {
  // Move the arm servo to shoot position (90 degrees)
  setServoAngle(myServos.ARM, 90);
}

const relax = () => {
  // Move the arm servo to relax position (0 degrees)
  setServoAngle(myServos.ARM, 0);
}
```

---

# 🕹️ INPUT CONTROL (20 minutes)

Now that you can move servos, learn to control them with gamepad and keyboard input.

## Step 3 : Handle Events in Your HTML Page

You need to override three empty functions in **your HTML page** (or a separate custom JS file) to define which events trigger which actions.

**BotScriptActions.js provides three empty hook functions** that you must implement:
- `processGamepadInput(gamepad)` — Called by the polling loop when gamepad input is detected
- `onKeyDown(event)` — Called by the browser when a key is pressed
- `onKeyUp(event)` — Called by the browser when a key is released

Use the provided `handleGamepaButton(gamepad, buttonIndex, buttonName, onPress, onRelease)` helper to simplify button handling.

### gamepad events

In the following example, implement `processGamepadInput()` in your HTML page:

```javascript
function processGamepadInput(gamepad) {
  handleGamepaButton(gamepad, XBOX_BUTTONS.DPAD_UP, 'UP', () => {
    moveForward();
  }, () => {
    stop();
  });
  
  handleGamepaButton(gamepad, XBOX_BUTTONS.DPAD_DOWN, 'DOWN', () => {
    moveBackward();
  }, () => {
    stop();
  });

  handleGamepaButton(gamepad, XBOX_BUTTONS.DPAD_LEFT, 'LEFT', () => {
    rotateCounterClockwise();
  }, () => {
    stop();
  });

  handleGamepaButton(gamepad, XBOX_BUTTONS.DPAD_RIGHT, 'RIGHT', () => {
    rotateClockwise();
  }, () => {
    stop();
  });
}
```

### gamepad analog stick events

Implement analog stick control in your `processGamepadInput()` function. Each axis returns a value between **-1.0 and +1.0**:
- **X axis**: -1 = left, +1 = right
- **Y axis**: -1 = up, +1 = down

To avoid stick drift, apply a **deadzone** (ignore values below a threshold).

In this example, the Xbox right stick controls wheel speed in your HTML page:

```javascript
CUSTOMCONTROL.processGamepadInput = ffunction (gamepad) {
  // Deadzone threshold (prevents stick drift)
  const DEADZONE = 0.15;
  
  // Read right stick axes
  const rightStickX = gamepad.axes[STICK_AXES.RIGHT_X];
  const rightStickY = gamepad.axes[STICK_AXES.RIGHT_Y];
  
  // Apply deadzone
  const x = Math.abs(rightStickX) > DEADZONE ? rightStickX : 0;
  const y = Math.abs(rightStickY) > DEADZONE ? rightStickY : 0;
  
  // Convert analog values (-1.0 to +1.0) to motor speeds (-100 to +100)
  // Y-axis: forward/backward movement (invert if needed)
  const forwardSpeed = -Math.round(y * 100);
  
  // X-axis: turning (left/right)
  const turnSpeed = Math.round(x * 100);
  
  // Differential steering: adjust each wheel independently
  const leftSpeed = forwardSpeed + turnSpeed;
  const rightSpeed = forwardSpeed - turnSpeed;
  
  // Clamp speeds to -100 to +100
  const leftSpeedClamped = Math.max(-100, Math.min(100, leftSpeed));
  const rightSpeedClamped = Math.max(-100, Math.min(100, rightSpeed));
  
  // Send to bot
  setServoSpeeds([[myServos.LEFT_WHEEL, leftSpeedClamped], [myServos.RIGHT_WHEEL, rightSpeedClamped]]);
}
```

**Key concepts**:
- **Deadzone**: Prevents unwanted motion from stick drift (typically 0.1–0.2)
- **Normalization**: Convert -1.0 to +1.0 range to -100 to +100 motor speed
- **Differential steering**: Adjust left/right wheel speeds independently for smooth turning
- **Clamping**: Ensure final speeds stay within valid range (-100 to +100)

### keyboard events

Implement keyboard control in **your HTML page** by overriding `onKeyDown()` and `onKeyUp()`:

```javascript
/**
 * Handle keyboard key down (override this in your HTML page)
 */
CUSTOMCONTROL.onKeyDown = function (event) {
  const key = event.key.toLowerCase();
  
  // Prevent default for arrow keys to avoid page scrolling
  if (['arrowup', 'arrowdown', 'arrowleft', 'arrowright'].includes(key)) {
    event.preventDefault();
  }
  
  // Skip if key already pressed (avoid key repeat)
  if (keyStates[key]) return;
  keyStates[key] = true;
  
  // Map keys to actions
  switch (key) {
    case 'arrowup':
      moveForward();
      break;
    case 'arrowdown':
      moveBackward();
      break;
    case 'arrowleft':
      rotateCounterClockwise();
      break;
    case 'arrowright':
      rotateClockwise();
      break;
    case 'q':
      shoot();
      break;
  }
}

/**
 * Handle keyboard key up (override this in your HTML page)
 */
CUSTOMCONTROL.onKeyUp = 
function (event) {
  const key = event.key.toLowerCase();
  keyStates[key] = false;
  
  switch (key) {
    case 'arrowup':
    case 'arrowdown':
    case 'arrowleft':
    case 'arrowright':
      stop();
      break;
    case 'q':
      relax();
      break;
  }
}
```

---

# 🐛 ADVANCED: Debugging & Optimization

Learn how to debug your code when things don't work, and optimize for performance.

## Debugging Guide

(See earlier section)

---

# 📚 Infrastructure Reference

### Constants (ready to use)

```javascript  
// Servo types
const SERVO_TYPES={
  ANGULAR_270: 1,    // grey servo
  ROTATIONAL: 2      // green servo
}

// Xbox controller button indices (standard Gamepad API mapping)
const XBOX_BUTTONS = { /* ... */ };

// Xbox controller axis indices (standard Gamepad API mapping)
const STICK_AXES = { /* ... */ };

// PlayStation controller button indices
const PLAYSTATION_BUTTONS = { /* ... */ };

// Nintendo Switch Pro controller button indices
const NINTENDO_SWITCH_BUTTONS = { /* ... */ };

// Generic USB gamepad button indices
const GENERIC_GAMEPAD_BUTTONS = { /* ... */ };
```

### Global Configuration

```javascript
let pollIntervalMs = 50;  // Gamepad polling interval in milliseconds (minimum 50ms between polls)
```

### Key Functions (ready to use)

- **`detectGamepad()`** - Public entry point to detect a connected gamepad (called from HTML button)
- **`pollGamepad()`** - Main polling loop that reads gamepad state at display refresh rate with configurable throttling
- **`handleGamepaButton(gamepad, buttonIndex, buttonName, onPress, onRelease)`** - Helper to detect button press/release events and call callbacks
- **`setServoAngle(channel, angle)`** - Set angular servo to specific position (fire-and-forget over WebSocket)
- **`setServoSpeeds(channels, speeds)`** - Set continuous rotation servo speeds (-100 to +100) (fire-and-forget over WebSocket)
- **`updateButtonIndicator(buttonName, pressed)`** - Updates visual feedback on the page (◉ when pressed, ◯ when released)

### Empty Override Hooks (implement these in your HTML page)

- **`processGamepadInput(gamepad)`** - Override in your HTML page to map gamepad buttons/sticks to actions
- **`onKeyDown(event)`** - Override in your HTML page to map keyboard key presses to actions
- **`onKeyUp(event)`** - Override in your HTML page to map keyboard key releases to actions

### State Tracking Variables (used internally)

- `gamepadConnected` - Boolean indicating if a gamepad is currently connected
- `gamepadIndex` - Index of the connected gamepad
- `buttonStates` - Object tracking current state of each button (for press/release detection)
- `keyStates` - Object tracking current state of each keyboard key (for press/release detection)
- `lastGamepadPollTime` - Timestamp of last successful gamepad poll (used for throttling)

# Operational example

Here is an operational example **to be added in your HTML page** for a bot setup:
- **S0**: Left track (continuous rotation)
- **S1**: Right track (continuous rotation)
- **S2**: Arm 1 (angular 270) — up: +135°, down: -120°, neutral: 0°
- **S3**: Arm 2 (angular 270) — up: +100°, down: -40°, neutral: 0°

**Controls**:
- **D-Pad**: Full-speed movement (forward, backward, rotate on itself)
- **Right stick**: Fine-grain speed control (forward/backward and turning with variable speed)
- **Left triggers/bumpers**: Arm 1 control (LT=up, LB=down, release=neutral)
- **Right triggers/bumpers**: Arm 2 control (RT=up, RB=down, release=neutral)

**Add this code to your HTML page** (inside a `<script>` tag or external `.js` file):

```javascript
// ── Bot Configuration ────────────────────────────────────────────────────────

// Define servo pin assignments
const botServos = {
  LEFT_TRACK: 0,    // S0 - Left track (continuous rotation)
  RIGHT_TRACK: 1,   // S1 - Right track (continuous rotation)
  ARM_1: 2,         // S2 - Arm 1 (angular 270)
  ARM_2: 3          // S3 - Arm 2 (angular 270)
};

// Declare servo types
attachServo(botServos.LEFT_TRACK, SERVO_TYPES.ROTATIONAL);
attachServo(botServos.RIGHT_TRACK, SERVO_TYPES.ROTATIONAL);
attachServo(botServos.ARM_1, SERVO_TYPES.ANGULAR_270);
attachServo(botServos.ARM_2, SERVO_TYPES.ANGULAR_270);

// Arm positions
const ARM_1_POSITIONS = {
  UP: 135,
  DOWN: -120,
  NEUTRAL: 0
};

const ARM_2_POSITIONS = {
  UP: 100,
  DOWN: -40,
  NEUTRAL: 0
};

// ── Movement Functions ───────────────────────────────────────────────────────

/**
 * Move forward at full speed
 */
const moveForwardFull = () => {
  setServoSpeeds([[botServos.LEFT_TRACK, 100], [botServos.RIGHT_TRACK, 100]]);
};

/**
 * Move backward at full speed
 */
const moveBackwardFull = () => {
  setServoSpeeds([[botServos.LEFT_TRACK, -100], [botServos.RIGHT_TRACK, -100]]);
};

/**
 * Rotate clockwise at full speed (left forward, right backward)
 */
const rotateClockwiseFull = () => {
  setServoSpeeds([[botServos.LEFT_TRACK, 100], [botServos.RIGHT_TRACK, -100]]);
};

/**
 * Rotate counter-clockwise at full speed (left backward, right forward)
 */
const rotateCounterClockwiseFull = () => {
  setServoSpeeds([[botServos.LEFT_TRACK, -100], [botServos.RIGHT_TRACK, 100]]);
};

/**
 * Stop all track movement
 */
const stopTracks = () => {
  setServoSpeeds([[botServos.LEFT_TRACK, 0], [botServos.RIGHT_TRACK, 0]]);
};

// ── Arm 1 Control ───────────────────────────────────────────────────────────

/**
 * Move Arm 1 to up position
 */
const arm1Up = () => {
  setServoAngle(botServos.ARM_1, ARM_1_POSITIONS.UP);
};

/**
 * Move Arm 1 to down position
 */
const arm1Down = () => {
  setServoAngle(botServos.ARM_1, ARM_1_POSITIONS.DOWN);
};

/**
 * Move Arm 1 to neutral position
 */
const arm1Neutral = () => {
  setServoAngle(botServos.ARM_1, ARM_1_POSITIONS.NEUTRAL);
};

// ── Arm 2 Control ───────────────────────────────────────────────────────────

/**
 * Move Arm 2 to up position
 */
const arm2Up = () => {
  setServoAngle(botServos.ARM_2, ARM_2_POSITIONS.UP);
};

/**
 * Move Arm 2 to down position
 */
const arm2Down = () => {
  setServoAngle(botServos.ARM_2, ARM_2_POSITIONS.DOWN);
};

/**
 * Move Arm 2 to neutral position
 */
const arm2Neutral = () => {
  setServoAngle(botServos.ARM_2, ARM_2_POSITIONS.NEUTRAL);
};

// ── Gamepad Input Processing ─────────────────────────────────────────────────

/**
 * Main gamepad input handler
 */
function processGamepadInput(gamepad) {
  // ── D-Pad: Full-speed movement ──
  handleGamepaButton(gamepad, XBOX_BUTTONS.DPAD_UP, 'UP', () => {
    moveForwardFull();
  }, () => {
    stopTracks();
  });

  handleGamepaButton(gamepad, XBOX_BUTTONS.DPAD_DOWN, 'DOWN', () => {
    moveBackwardFull();
  }, () => {
    stopTracks();
  });

  handleGamepaButton(gamepad, XBOX_BUTTONS.DPAD_LEFT, 'LEFT', () => {
    rotateCounterClockwiseFull();
  }, () => {
    stopTracks();
  });

  handleGamepaButton(gamepad, XBOX_BUTTONS.DPAD_RIGHT, 'RIGHT', () => {
    rotateClockwiseFull();
  }, () => {
    stopTracks();
  });

  // ── Right Stick: Fine-grain movement ──
  const DEADZONE = 0.15;
  const rightStickX = gamepad.axes[STICK_AXES.RIGHT_X];
  const rightStickY = gamepad.axes[STICK_AXES.RIGHT_Y];

  const x = Math.abs(rightStickX) > DEADZONE ? rightStickX : 0;
  const y = Math.abs(rightStickY) > DEADZONE ? rightStickY : 0;

  // Only apply stick control if not using D-Pad (avoid conflicts)
  if (x === 0 && y === 0) {
    stopTracks();
  } else {
    // Convert stick values to motor speeds
    const forwardSpeed = -Math.round(y * 100);
    const turnSpeed = Math.round(x * 100);

    // Differential steering
    const leftSpeed = forwardSpeed + turnSpeed;
    const rightSpeed = forwardSpeed - turnSpeed;

    // Clamp to valid range
    const leftSpeedClamped = Math.max(-100, Math.min(100, leftSpeed));
    const rightSpeedClamped = Math.max(-100, Math.min(100, rightSpeed));

    setServoSpeeds([[botServos.LEFT_TRACK, leftSpeedClamped], [botServos.RIGHT_TRACK, rightSpeedClamped]]);
  }

  // ── Left Triggers/Bumpers: Arm 1 ──
  handleGamepaButton(gamepad, XBOX_BUTTONS.LT, 'LT', () => {
    arm1Up();
  }, () => {
    arm1Neutral();
  });

  handleGamepaButton(gamepad, XBOX_BUTTONS.LB, 'LB', () => {
    arm1Down();
  }, () => {
    arm1Neutral();
  });

  // ── Right Triggers/Bumpers: Arm 2 ──
  handleGamepaButton(gamepad, XBOX_BUTTONS.RT, 'RT', () => {
    arm2Up();
  }, () => {
    arm2Neutral();
  });

  handleGamepaButton(gamepad, XBOX_BUTTONS.RB, 'RB', () => {
    arm2Down();
  }, () => {
    arm2Neutral();
  });

  // ── Center Buttons: UI Navigation ──
  handleGamepaButton(gamepad, XBOX_BUTTONS.BACK, 'BACK', () => {
    previousScreen();
  }, undefined);

  handleGamepaButton(gamepad, XBOX_BUTTONS.START, 'START', () => {
    nextScreen();
  }, undefined);
}

// ── Keyboard Fallback ────────────────────────────────────────────────────────

/**
 * Handle keyboard key down
 */
function onKeyDown(event) {
  const key = event.key.toLowerCase();

  if (['arrowup', 'arrowdown', 'arrowleft', 'arrowright'].includes(key)) {
    event.preventDefault();
  }

  if (keyStates[key]) return;
  keyStates[key] = true;

  switch (key) {
    case 'arrowup':
      moveForwardFull();
      break;
    case 'arrowdown':
      moveBackwardFull();
      break;
    case 'arrowleft':
      rotateCounterClockwiseFull();
      break;
    case 'arrowright':
      rotateClockwiseFull();
      break;
    case 'q':
      arm1Up();
      break;
    case 'a':
      arm1Down();
      break;
    case 'w':
      arm2Up();
      break;
    case 's':
      arm2Down();
      break;
  }
}

/**
 * Handle keyboard key up
 */
function onKeyUp(event) {
  const key = event.key.toLowerCase();
  keyStates[key] = false;

  switch (key) {
    case 'arrowup':
    case 'arrowdown':
    case 'arrowleft':
    case 'arrowright':
      stopTracks();
      break;
    case 'q':
    case 'a':
      arm1Neutral();
      break;
    case 'w':
    case 's':
      arm2Neutral();
      break;
  }
}
```

**Control Summary**:
| Input | Action |
|-------|--------|
| **D-Pad ↑** | Forward (full speed) |
| **D-Pad ↓** | Backward (full speed) |
| **D-Pad ←** | Rotate counter-clockwise (full speed) |
| **D-Pad →** | Rotate clockwise (full speed) |
| **Right Stick** | Fine-grain movement (forward/backward & turning by stick position) |
| **LT** | Arm 1 up |
| **LB** | Arm 1 down |
| **RT** | Arm 2 up |
| **RB** | Arm 2 down |
| **Back** | Previous screen |
| **Start** | Next screen |
| **Keyboard**: ↑ ↓ ← → | Same as D-Pad |
| **Keyboard**: Q/A | Arm 1 up/down |
| **Keyboard**: W/S | Arm 2 up/down |total processing power

---

## Where to Go Next?

✅ **You've finished the Main Guide!**

### What You Can Do Now:

**Just the basics?**
- ✅ Attach servos
- ✅ Control with gamepad/keyboard
- ✅ Debug when something breaks

**Ready for more?** Pick a path:

**Path 1: Real-Time Control Expert** 🕹️
- Master analog stick control (deadzone, sensitivity)
- Learn differential steering math
- Optimize for low latency
- Read: *Analog Stick Control* template + *Performance* section

**Path 2: Complex Behaviors** 🤖
- Build state machines
- Sequence multiple actions
- Time-based animations
- Read: *State Machine* template + *Asynchronous Behavior* section

**Path 3: Performance Optimization** ⚡
- Reduce network overhead
- Batch commands efficiently
- Eliminate lag
- Read: *Performance* section + *Best Practices*

**Path 4: Custom Hardware** 🛠️
- Add more servos (up to 6)
- Different servo types
- Advanced sensor integration
- Modify: Code templates, Steps 0-1

### Quick Reference Shortcuts:

- 🔍 **Find a function**: Go to *Function Quick Reference* table
- ⏱️ **Understand timing**: Read *Asynchronous Behavior* section
- 💾 **Copy working code**: Use *Code Templates* section
- 🐛 **Fix something**: Check *Debugging Guide* section
- 📊 **Optimize code**: Read *Performance* section

### Still Stuck?

1. Check **Debugging Guide** → Troubleshooting Checklist
2. Search **Function Quick Reference** for the function name
3. Look at relevant **Code Template** for working example
4. Check **Browser Console** (F12) for JavaScript errors