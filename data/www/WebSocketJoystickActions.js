'use strict';

// ── Xbox Controller Configuration ─────────────────────────────────────────────

// Xbox controller button indices (standard mapping)
const XBOX_BUTTONS = {
  LB: 4,      // Left bumper
  RB: 5,      // Right bumper
  LT: 6,      // Left trigger
  RT: 7,      // Right trigger
  DPAD_UP: 12,
  DPAD_DOWN: 13,
  DPAD_LEFT: 14,
  DPAD_RIGHT: 15
};

let gamepadConnected = false;
let gamepadIndex = -1;
let animationFrameId = null;

// Button state tracking (to detect press/release)
const buttonStates = {};

// Keyboard state tracking
const keyStates = {};

// ── Initialization ────────────────────────────────────────────────────────────

document.addEventListener('DOMContentLoaded', () => {
  // Initialize button states
  Object.values(XBOX_BUTTONS).forEach(btnIdx => {
    buttonStates[btnIdx] = false;
  });
  
  // Listen for gamepad connection
  window.addEventListener('gamepadconnected', onGamepadConnected);
  window.addEventListener('gamepaddisconnected', onGamepadDisconnected);
  
  // Listen for keyboard events
  window.addEventListener('keydown', onKeyDown);
  window.addEventListener('keyup', onKeyUp);
  
  // Start polling if gamepad already connected
  checkGamepadConnection();
});

// ── Gamepad Connection Handlers ───────────────────────────────────────────────

/**
 * Check if gamepad is already connected on page load
 */
function checkGamepadConnection() {
  const gamepads = navigator.getGamepads();
  for (let i = 0; i < gamepads.length; i++) {
    if (gamepads[i]) {
      onGamepadConnected({ gamepad: gamepads[i] });
      break;
    }
  }
}

/**
 * Handle gamepad connection
 */
function onGamepadConnected(event) {
  const gamepad = event.gamepad;
  console.log('Gamepad connected:', gamepad.id);
  
  gamepadConnected = true;
  gamepadIndex = gamepad.index;
  
  // Update UI
  updateGamepadStatus(true, gamepad.id);
  
  // Start polling loop
  if (!animationFrameId) {
    pollGamepad();
  }
}

/**
 * Handle gamepad disconnection
 */
function onGamepadDisconnected(event) {
  console.log('Gamepad disconnected:', event.gamepad.id);
  
  gamepadConnected = false;
  gamepadIndex = -1;
  
  // Update UI
  updateGamepadStatus(false, '');
  
  // Stop polling
  if (animationFrameId) {
    cancelAnimationFrame(animationFrameId);
    animationFrameId = null;
  }
  
  // Reset all button indicators
  Object.keys(XBOX_BUTTONS).forEach(btnName => {
    updateButtonIndicator(btnName, false);
  });
}

// ── Gamepad Polling ───────────────────────────────────────────────────────────

/**
 * Poll gamepad state at 60 FPS
 */
function pollGamepad() {
  if (!gamepadConnected) return;
  
  const gamepads = navigator.getGamepads();
  const gamepad = gamepads[gamepadIndex];
  
  if (gamepad) {
    processGamepadInput(gamepad);
  }
  
  // Continue polling
  animationFrameId = requestAnimationFrame(pollGamepad);
}

/**
 * Process gamepad button states
 */
function processGamepadInput(gamepad) {
  // LB Button (button 4)
  handleButton(gamepad, XBOX_BUTTONS.LB, 'LB', () => {
    setServoAngle(4, +45);
  }, () => {
    setServoAngle(4, 0);
  });
  
  // LT Button (button 6)
  handleButton(gamepad, XBOX_BUTTONS.LT, 'LT', () => {
    setServoAngle(4, -45);
  }, () => {
    setServoAngle(4, 0);
  });
  // RB Button (button 5)
  handleButton(gamepad, XBOX_BUTTONS.RB, 'RB', () => {
    setServoAngle(4, +45);
  }, () => {
    setServoAngle(4, 0 );
  });
  
  // RT Button (button 7)
  handleButton(gamepad, XBOX_BUTTONS.RT, 'RT', () => {
    setServoAngle(4, -45);
  }, () => {
    setServoAngle(4, 0);
  });
  
  // D-Pad Up (button 12)
  handleButton(gamepad, XBOX_BUTTONS.DPAD_UP, 'UP', () => {
    setServoSpeeds([1, 2], [100, 100]);
  }, () => {
    setServoSpeeds([1, 2], [0, 0]);
  });
  
  // D-Pad Down (button 13)
  handleButton(gamepad, XBOX_BUTTONS.DPAD_DOWN, 'DOWN', () => {
    setServoSpeeds([1, 2], [-100, -100]);
  }, () => {
    setServoSpeeds([1, 2], [0, 0]);
  });
  
  // D-Pad Left (button 14)
  handleButton(gamepad, XBOX_BUTTONS.DPAD_LEFT, 'LEFT', () => {
    setServoSpeeds([1, 2], [100, -100]);
  }, () => {
    setServoSpeeds([1, 2], [0, 0]);
  });
  
  // D-Pad Right (button 15)
  handleButton(gamepad, XBOX_BUTTONS.DPAD_RIGHT, 'RIGHT', () => {
    setServoSpeeds([1, 2], [-100, 100]);
  }, () => {
    setServoSpeeds([1, 2], [0, 0]);
  });
}

/**
 * Handle button press/release with callbacks
 */
function handleButton(gamepad, buttonIndex, buttonName, onPress, onRelease) {
  const button = gamepad.buttons[buttonIndex];
  const isPressed = button.pressed || button.value > 0.5;
  const wasPressed = buttonStates[buttonIndex];
  
  // Update indicator
  updateButtonIndicator(buttonName, isPressed);
  
  // Detect press (rising edge)
  if (isPressed && !wasPressed) {
    console.log(`Button ${buttonName} pressed`);
    if (onPress) onPress();
  }
  
  // Detect release (falling edge)
  if (!isPressed && wasPressed) {
    console.log(`Button ${buttonName} released`);
    if (onRelease) onRelease();
  }
  
  // Update state
  buttonStates[buttonIndex] = isPressed;
}

// ── Keyboard Input Handlers ───────────────────────────────────────────────────

/**
 * Handle keyboard key down
 */
function onKeyDown(event) {
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
    // Arrow keys → D-Pad
    case 'arrowup':
      updateButtonIndicator('UP', true);
      setServoSpeeds([1, 2], [100, 100]);
      break;
      
    case 'arrowdown':
      updateButtonIndicator('DOWN', true);
      setServoSpeeds([1, 2], [-100, -100]);
      break;
      
    case 'arrowleft':
      updateButtonIndicator('LEFT', true);
      setServoSpeeds([1, 2], [100, -100]);
      break;
      
    case 'arrowright':
      updateButtonIndicator('RIGHT', true);
      setServoSpeeds([1, 2], [-100, 100]);
      break;
      
    // Q → LB
    case 'q':
      updateButtonIndicator('LB', true);
      setServoAngle(4, 45);
      break;
    // Q → LT
    case 'a':
      updateButtonIndicator('LT', true);
      setServoAngle(4, -45);
      break;
      
    // W → RB
    case 'w':
      updateButtonIndicator('RB', true);
      setServoAngle(4, 45);
      break;
      
    // W → RT
    case 's':
      updateButtonIndicator('RT', true);
      setServoAngle(4, -45);
      break;
  }
}

/**
 * Handle keyboard key up
 */
function onKeyUp(event) {
  const key = event.key.toLowerCase();
  keyStates[key] = false;
  
  // Map keys to release actions
  switch (key) {
    // Arrow keys → Stop servos
    case 'arrowup':
      updateButtonIndicator('UP', false);
      setServoSpeeds([1, 2], [0, 0]);
      break;
      
    case 'arrowdown':
      updateButtonIndicator('DOWN', false);
      setServoSpeeds([1, 2], [0, 0]);
      break;
      
    case 'arrowleft':
      updateButtonIndicator('LEFT', false);
      setServoSpeeds([1, 2], [0, 0]);
      break;
      
    case 'arrowright':
      updateButtonIndicator('RIGHT', false);
      setServoSpeeds([1, 2], [0, 0]);
      break;
      
    // Q → LB release
    case 'q':
      updateButtonIndicator('LB', false);
      setServoAngle(4, 45);
      break;
    // Q → LT release
    case 'a':
      updateButtonIndicator('LT', false);
      setServoAngle(4, -45);
      break;
      
    // W → RB release
    case 'w':
      updateButtonIndicator('RB', false);
      setServoAngle(4, 45);
      break;
      
     // W → RT release
    case 's':
      updateButtonIndicator('RT', false);
      setServoAngle(4, -45);
      break;
  }
}

// ── Servo Control Functions ───────────────────────────────────────────────────

/**
 * Set servo angle (for angular servos).
 * Uses fire-and-forget over the WebSocket UDP bridge so that rapid
 * joystick inputs are never queued behind a pending response.
 *
 * @param {number} channel - Servo channel (0-7)
 * @param {number} angle - Angle in degrees (-135 to +135)
 */
function setServoAngle(channel, angle) {
  if (typeof isMasterRegistered !== 'undefined' && !isMasterRegistered) {
    return;
  }

  // Build packet: 0x21 + int16 values up to channel
  const angles = [];
  for (let i = 0; i <= channel; i++) {
    let raw;
    if (i === channel) {
      raw = (angle << 1) | 1;   // Encode: (angle << 1) | 1
    } else {
      raw = 0;                  // Skip other channels
    }
    angles.push(raw & 0xFF);        // low byte
    angles.push((raw >> 8) & 0xFF); // high byte
  }

  const packet = new Uint8Array([0x21, ...angles]);

  // Fire-and-forget (UDP-like): no response awaited
  if (typeof sendUDPFireAndForget !== 'undefined') {
    sendUDPFireAndForget(packet);
  }
}

/**
 * Set servo speeds (for rotational servos).
 * Uses fire-and-forget over the WebSocket UDP bridge so that rapid
 * joystick inputs are never queued behind a pending response.
 *
 * @param {number[]} channels - Array of channel numbers
 * @param {number[]} speeds - Array of speeds (-100 to +100)
 */
function setServoSpeeds(channels, speeds) {
  if (typeof isMasterRegistered !== 'undefined' && !isMasterRegistered) {
    return;
  }

  let mask = 0;
  const speedBytes = new Array(8).fill(128); // 128 = speed 0
  for (let i = 0; i < channels.length; i++) {
    const ch = channels[i];
    const speed = speeds[i];
    mask |= (1 << ch);
    speedBytes[ch] = speed + 128; // Encode: speed + 128
  }

  const packet = new Uint8Array([0x22, mask, ...speedBytes]);

  // Fire-and-forget (UDP-like): no response awaited
  if (typeof sendUDPFireAndForget !== 'undefined') {
    sendUDPFireAndForget(packet);
  }
}

// ── UI Updates ────────────────────────────────────────────────────────────────

/**
 * Update gamepad connection status display
 */
function updateGamepadStatus(connected, name) {
  const statusElem = document.getElementById('gamepadStatus');
  const nameElem = document.getElementById('gamepadName');
  
  if (statusElem) {
    if (connected) {
      statusElem.textContent = 'Connected';
      statusElem.className = 'status-badge status-connected';
    } else {
      statusElem.textContent = 'Not Connected';
      statusElem.className = 'status-badge status-disconnected';
    }
  }
  
  if (nameElem) {
    nameElem.textContent = name || '—';
  }
}

/**
 * Update button indicator (visual feedback)
 */
function updateButtonIndicator(buttonName, pressed) {
  const elem = document.getElementById(`btn-${buttonName}`);
  if (elem) {
    elem.textContent = pressed ? '◉' : '◯';
    elem.style.color = pressed ? '#fff700ff' : '#666';
  }
}
