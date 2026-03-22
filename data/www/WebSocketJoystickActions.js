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

const BOT_SERVOS = {
  LEFTWHEEL: 0,      // Left bumper
  RIGHTWHEEL: 1,      // Right bumper
  SERVOA: 1,      // Left trigger
  SERVOB: 2,      // Right trigger

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
 * Public entry point to detect a gamepad (called from HTML button).
 */
function detectGamepad() {
  checkGamepadConnection();
  if (!gamepadConnected) {
    showStatus('No controller detected. Press any button on your controller.', false);
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

<<<<<<< HEAD
=======
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
    setServoAngle(5, +45);
  }, () => {
    setServoAngle(5, 0);
  });
  
  // RT Button (button 7)
  handleButton(gamepad, XBOX_BUTTONS.RT, 'RT', () => {
    setServoAngle(5, -45);
  }, () => {
    setServoAngle(5, 0);
  });
  
  // D-Pad Up (button 12)
  handleButton(gamepad, XBOX_BUTTONS.DPAD_UP, 'UP', () => {
    _setServoSpeeds([1, 2], [100, 100]);
  }, () => {
    _setServoSpeeds([1, 2], [0, 0]);
  });
  
  // D-Pad Down (button 13)
  handleButton(gamepad, XBOX_BUTTONS.DPAD_DOWN, 'DOWN', () => {
    _setServoSpeeds([1, 2], [-100, -100]);
  }, () => {
    _setServoSpeeds([1, 2], [0, 0]);
  });
  
  // D-Pad Left (button 14)
  handleButton(gamepad, XBOX_BUTTONS.DPAD_LEFT, 'LEFT', () => {
    _setServoSpeeds([1, 2], [100, -100]);
  }, () => {
    _setServoSpeeds([1, 2], [0, 0]);
  });
  
  // D-Pad Right (button 15)
  handleButton(gamepad, XBOX_BUTTONS.DPAD_RIGHT, 'RIGHT', () => {
    _setServoSpeeds([1, 2], [-100, 100]);
  }, () => {
    _setServoSpeeds([1, 2], [0, 0]);
  });
}
>>>>>>> cd4ad93 (we fixes)

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


<<<<<<< HEAD
=======
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
      _setServoSpeeds([1, 2], [100, 100]);
      break;
      
    case 'arrowdown':
      updateButtonIndicator('DOWN', true);
      _setServoSpeeds([1, 2], [-100, -100]);
      break;
      
    case 'arrowleft':
      updateButtonIndicator('LEFT', true);
      _setServoSpeeds([1, 2], [100, -100]);
      break;
      
    case 'arrowright':
      updateButtonIndicator('RIGHT', true);
      _setServoSpeeds([1, 2], [-100, 100]);
      break;
      
    // Q → LB
    case 'q':
      updateButtonIndicator('LB', true);
      setServoAngle(4, 45);
      break;
    // A → LT
    case 'a':
      updateButtonIndicator('LT', true);
      setServoAngle(4, -45);
      break;
      
    // W → RB
    case 'w':
      updateButtonIndicator('RB', true);
      setServoAngle(5, 45);
      break;
      
    // S → RT
    case 's':
      updateButtonIndicator('RT', true);
      setServoAngle(5, -45);
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
      _setServoSpeeds([1, 2], [0, 0]);
      break;
      
    case 'arrowdown':
      updateButtonIndicator('DOWN', false);
      _setServoSpeeds([1, 2], [0, 0]);
      break;
      
    case 'arrowleft':
      updateButtonIndicator('LEFT', false);
      _setServoSpeeds([1, 2], [0, 0]);
      break;
      
    case 'arrowright':
      updateButtonIndicator('RIGHT', false);
      _setServoSpeeds([1, 2], [0, 0]);
      break;
      
    // Q → LB release
    case 'q':
      updateButtonIndicator('LB', false);
      setServoAngle(4, 0);
      break;
    // A → LT release
    case 'a':
      updateButtonIndicator('LT', false);
      setServoAngle(4, 0);
      break;
      
    // W → RB release
    case 'w':
      updateButtonIndicator('RB', false);
      setServoAngle(5, 0);
      break;
      
    // S → RT release
    case 's':
      updateButtonIndicator('RT', false);
      setServoAngle(5, 0);
      break;
  }
}
>>>>>>> cd4ad93 (we fixes)

// ── Servo Control Functions ───────────────────────────────────────────────────

/**
 * Set servo angle (for angular servos).
 * Uses fire-and-forget over the WebSocket WS bridge so that rapid
 * joystick inputs are never queued behind a pending response.
 *
 * @param {number} channel - Servo channel (0-5)
 * @param {number} angle - Angle in degrees ( 0 to 270 depending on type)
 */
function setServoAngle(channel, angle) {
  if (typeof isMasterRegistered !== 'undefined' && !isMasterRegistered) {
    return;
  }

  // Build packet: SET_SERVOS_ANGLE [servo_mask:u8] [angle_hi:u8] [angle_lo:u8]  (big-endian i16)
  const mask    = (1 << channel) & 0xFF;
  const angleHi = (angle >> 8) & 0xFF;
  const angleLo = angle & 0xFF;
  const packet  = new Uint8Array([MOTOR_SERVO_ACTION.SET_SERVOS_ANGLE, mask, angleHi, angleLo]);

<<<<<<< HEAD
=======
  const packet = new Uint8Array([0x21, ...angles]);

>>>>>>> cd4ad93 (we fixes)
  // Fire-and-forget (WS-like): no response awaited
  if (typeof sendWSFireAndForget !== 'undefined') {
    sendWSFireAndForget(packet);
  }
}

/**
 * Set servo speeds (for rotational servos).
 * Uses fire-and-forget over the WebSocket WS bridge so that rapid
 * joystick inputs are never queued behind a pending response.
 *
 * @param {number[]} channels - Array of channel numbers (0-5)
 * @param {number[]} speeds - Array of speeds (-100 to +100), one per channel
 */
function _setServoSpeeds(channels, speeds) {
  if (typeof isMasterRegistered !== 'undefined' && !isMasterRegistered) {
    return;
  }

  // Protocol: SET_SERVOS_SPEED [servo_mask:u8] [speed:i8] — one speed for all masked channels.
  // Group channels that share the same speed into one packet; send separate packets otherwise.
  const speedMap = new Map();
  for (let i = 0; i < channels.length; i++) {
    const speed = speeds[i];
    speedMap.set(speed, (speedMap.get(speed) || 0) | (1 << channels[i]));
  }

<<<<<<< HEAD
  if (typeof sendWSFireAndForget !== 'undefined') {
    speedMap.forEach((mask, speed) => {
      const packet = new Uint8Array([MOTOR_SERVO_ACTION.SET_SERVOS_SPEED, mask & 0xFF, speed & 0xFF]);
      sendWSFireAndForget(packet);
    });
=======
  const packet = new Uint8Array([0x22, mask, ...speedBytes]);

  // Fire-and-forget (WS-like): no response awaited
  if (typeof sendWSFireAndForget !== 'undefined') {
    sendWSFireAndForget(packet);
>>>>>>> cd4ad93 (we fixes)
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
