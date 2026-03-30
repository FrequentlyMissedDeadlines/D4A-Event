'use strict';

// ═════════════════════════════════════════════════════════════════════════════
// ⚠️  ARCHITECTURE: FIRE-AND-FORGET SERVO CONTROL MODE
// ═════════════════════════════════════════════════════════════════════════════
//
// BotScriptActions.js provides GAMEPAD/KEYBOARD input handling and
// FIRE-AND-FORGET servo control functions optimized for real-time response.
// This file is auto-loaded and always available.
//
// 📌 IMPORTANT: Two different servo control modes exist (choose carefully):
//
// MODE 1: REQUEST-RESPONSE (BotScript.js functions ending in "UI")
//   Functions: setServoAnglesUI(), setServoSpeedsUI(), attachServos()
//   Behavior: Waits for response, shows status, gets values from HTML inputs
//   Use case: UI buttons in WebSocketJoystick.html, manual control
//
// MODE 2: FIRE-AND-FORGET (BotScriptActions.js - THIS FILE)
//   Functions: attachServo(channel, type), setServoAngle(channel, angle),
//             setServoSpeeds(pairs[])
//   Behavior: Returns immediately, no wait, optimized for 50ms gamepad polling
//   Use case: Real-time control from gamepad/keyboard, script automation
//
// ✅ AVAILABLE IN YOUR CODE (from this file):
//   - Constants: SERVO_TYPES, XBOX_BUTTONS, PLAYSTATION_BUTTONS, etc.
//   - Gamepad: detectGamepad(), pollGamepad(), handleGamepaButton()
//   - Gamepad SVG overlay: showGamepad(true/false) — live visual feedback
//   - Servo control (fire-and-forget): attachServo(), setServoAngle(), setServoSpeeds()
//   - UI: updateButtonIndicator(), updateGamepadStatus()
//   - Hooks: CUSTOMCONTROL.onKeyDown, CUSTOMCONTROL.onKeyUp, CUSTOMCONTROL.processGamepadInput
//
// ❌ NOT AVAILABLE (use from BotScript.js instead):
//   - registerMaster(), unregisterMaster() — Master registration (CALL FIRST)
//   - startHeartbeat(), stopHeartbeat() — Connection keepalive (auto-started)
//   - setBotName(), getBattery() — Bot info queries
//   - setScreen(), nextScreen(), previousScreen() — UI screen navigation
//   - Request-response servo functions: setServoAnglesUI(), setServoSpeedsUI()
//
// HOW TO USE:
// 1. Override CUSTOMCONTROL hooks in your HTML page:
//    - CUSTOMCONTROL.onKeyDown(event) — keyboard key down
//    - CUSTOMCONTROL.onKeyUp(event) — keyboard key up
//    - CUSTOMCONTROL.processGamepadInput(gamepad) — gamepad input
// 2. Call fire-and-forget servo functions from within those hooks
// 3. Functions return immediately (no waiting for responses)
// 4. Use setServoSpeedsUI() from BotScript.js if you need status feedback
//
// ═════════════════════════════════════════════════════════════════════════════

// ── Xbox Controller Configuration ─────────────────────────────────────────────
const SERVO_TYPES={
  ANGULAR_270: 1, 
  ROTATIONAL: 2
}
// Xbox controller button indices (standard Gamepad API mapping)
const XBOX_BUTTONS = {
  // Face buttons
  A:             0,  // A (bottom)
  B:             1,  // B (right)
  X:             2,  // X (left)
  Y:             3,  // Y (top)
  // Shoulder / trigger buttons
  LB:            4,  // Left bumper
  RB:            5,  // Right bumper
  LT:            6,  // Left trigger  (analog: value 0.0–1.0)
  RT:            7,  // Right trigger (analog: value 0.0–1.0)
  // Center buttons
  BACK:          8,  // Back / View / Select
  START:         9,  // Start / Menu
  // Guide / Home button (may be absent on some browsers)
  GUIDE:        16,  
  // Stick clicks
  LS:           10,  // Left stick click (L3)
  RS:           11,  // Right stick click (R3)
  // D-Pad
  DPAD_UP:      12,
  DPAD_DOWN:    13,
  DPAD_LEFT:    14,
  DPAD_RIGHT:   15

};

// PlayStation controller button indices (standard Gamepad API mapping)
const PLAYSTATION_BUTTONS = {
  // Face buttons
  X:             0,  // X (bottom)
  CIRCLE:        1,  // O/Circle (right)
  SQUARE:        2,  // Square (left)
  TRIANGLE:      3,  // Triangle (top)
  // Shoulder / trigger buttons
  L1:            4,  // L1 bumper
  R1:            5,  // R1 bumper
  L2:            6,  // L2 trigger (analog: value 0.0–1.0)
  R2:            7,  // R2 trigger (analog: value 0.0–1.0)
  // Center buttons
  SHARE:         8,  // Share / Select
  OPTIONS:       9,  // Options / Start
  // Guide / Home button
  GUIDE:        16,  // PS button
  // Stick clicks
  L3:           10,  // Left stick click
  R3:           11,  // Right stick click
  // D-Pad
  DPAD_UP:      12,
  DPAD_DOWN:    13,
  DPAD_LEFT:    14,
  DPAD_RIGHT:   15
};

// Nintendo Switch Pro controller button indices (standard Gamepad API mapping)
const NINTENDO_SWITCH_BUTTONS = {
  // Face buttons (note: Nintendo layout differs visually)
  B:             0,  // B (bottom)
  A:             1,  // A (right)
  Y:             2,  // Y (left)
  X:             3,  // X (top)
  // Shoulder / trigger buttons
  L:             4,  // L bumper
  R:             5,  // R bumper
  ZL:            6,  // ZL trigger (analog: value 0.0–1.0)
  ZR:            7,  // ZR trigger (analog: value 0.0–1.0)
  // Center buttons
  MINUS:         8,  // Minus / Select
  PLUS:          9,  // Plus / Start
  // Guide / Home button
  HOME:         16,  // Home button
  // Stick clicks
  LS:           10,  // Left stick click
  RS:           11,  // Right stick click
  // D-Pad
  DPAD_UP:      12,
  DPAD_DOWN:    13,
  DPAD_LEFT:    14,
  DPAD_RIGHT:   15
};

// Generic USB gamepad button indices (standard Gamepad API mapping)
// Most generic gamepads follow this standard layout
const GENERIC_GAMEPAD_BUTTONS = {
  // Face buttons
  BUTTON_1:      0,  // Bottom/South button
  BUTTON_2:      1,  // Right/East button
  BUTTON_3:      2,  // Left/West button
  BUTTON_4:      3,  // Top/North button
  // Shoulder / trigger buttons
  LB:            4,  // Left bumper
  RB:            5,  // Right bumper
  LT:            6,  // Left trigger (analog: value 0.0–1.0)
  RT:            7,  // Right trigger (analog: value 0.0–1.0)
  // Center buttons
  BACK:          8,  // Back / Select
  START:         9,  // Start / Menu
  // Guide / Home button
  GUIDE:        16,  // Home button (may be absent)
  // Stick clicks
  LS:           10,  // Left stick click
  RS:           11,  // Right stick click
  // D-Pad
  DPAD_UP:      12,
  DPAD_DOWN:    13,
  DPAD_LEFT:    14,
  DPAD_RIGHT:   15
};

// Xbox controller axis indices (standard Gamepad API mapping)
// Values range from -1.0 to +1.0
const STICK_AXES = {
  LEFT_X:   0,  // Left stick horizontal  (-1 = left,  +1 = right)
  LEFT_Y:   1,  // Left stick vertical    (-1 = up,    +1 = down)
  RIGHT_X:  2,  // Right stick horizontal (-1 = left,  +1 = right)
  RIGHT_Y:  3   // Right stick vertical   (-1 = up,    +1 = down)
};


let pollIntervalMs = 50;

let gamepadConnected = false;
let gamepadIndex = -1;
let animationFrameId = null;
let lastGamepadPollTime = 0;

// Button state tracking (to detect press/release)
const buttonStates = {};

// Keyboard state tracking
const keyStates = {};

// ── Gamepad SVG Overlay ────────────────────────────────────────────────────────
//
// showGamepad(true/false) — Show/hide a live SVG overlay of the detected
// gamepad. Buttons light up on press, sticks move with analog input.
// SVGs are loaded from gamepadxbox.svg, gamepadsony.svg, gamepadnintendo.svg.
// Elements are matched by their inkscape:label attribute.
//
// Usage:  showGamepad(true);   // enable overlay
//         showGamepad(false);  // hide overlay
// ──────────────────────────────────────────────────────────────────────────────

let _gpadOverlayEnabled = false;
let _gpadSVGLoaded = false;
let _gpadSVGType = '';          // 'xbox' | 'sony' | 'nintendo'
let _gpadSVGCache = {};         // { label: { el, origFill } }

// SVG file paths per detected gamepad family
const _GPAD_SVG_FILES = {
  xbox:     './gamepadxbox.svg',
  sony:     './gamepadsony.svg',
  nintendo: './gamepadnintendo.svg'
};

// Button index → inkscape:label per gamepad family
const _GPAD_BTN_MAP = {
  xbox: {
    0: 'btnA',  1: 'btnB',  2: 'btnX',  3: 'btnY',
    4: 'trigLeftTop',  5: 'trigRightTop',
    6: 'trigLeftBottom',  7: 'trigRightBottom',
    8: 'btnBack',  9: 'btnStart',  16: 'btnSelect',
    12: 'padUp',  13: 'padDown',  14: 'padLeft',  15: 'padRight'
  },
  sony: {
    0: 'btnCross',  1: 'btnCircle',  2: 'btnSquare',  3: 'btnTriangle',
    4: 'trigLeftTop',  5: 'trigRightTop',
    8: 'btnShare',  9: 'btnOptions',
    12: 'padUp',  13: 'padDown',  14: 'padLeft',  15: 'padRight'
  },
  nintendo: {
    0: 'btnCross',  1: 'btnCircle',  2: 'btnSquare',  3: 'btnTriangle',
    12: 'padUp',  13: 'padDown',  14: 'padLeft',  15: 'padRight'
  }
};

// Stick click buttons also flash the stick element
const _GPAD_STICK_CLICK = { 10: 'stickLeft', 11: 'stickRight' };

// Visual tuning
const _GPAD_COLOR_PRESSED  = '#FFD700'; // gold
const _GPAD_COLOR_STICK    = '#00BFFF'; // sky blue
const _GPAD_STICK_OFFSET   = 10;        // max px translation
const _GPAD_DEADZONE        = 0.12;

/**
 * Show or hide the gamepad SVG overlay.
 * When enabled, auto-detects gamepad type and loads the matching SVG file.
 * Button presses and stick movements update the SVG in real-time.
 *
 * @param {boolean} enable - true to show, false to hide
 */
/**
 * Toggle the gamepad SVG overlay on/off and update the button UI.
 * Called by the 🎮 Show/Hide Gamepad button in BotScript.html.
 */
function _toggleGamepadOverlay() {
  const btn = document.getElementById('btnShowGamepad');
  const wasOn = _gpadOverlayEnabled;
  showGamepad(!wasOn);
  if (btn) {
    btn.textContent = wasOn ? '🎮 Show Gamepad' : '🎮 Hide Gamepad';
    btn.className   = wasOn ? 'btn btn-primary'  : 'btn btn-danger';
  }
}

function showGamepad(enable) {
  _gpadOverlayEnabled = !!enable;

  if (enable) {
    _gpadEnsureContainer();
    const c = document.getElementById('gamepadSVGContainer');
    c.style.display = 'block';

    // If a gamepad is already connected, load SVG immediately
    if (gamepadConnected && gamepadIndex >= 0) {
      const gp = navigator.getGamepads()[gamepadIndex];
      if (gp) {
        _gpadLoadSVG(_gpadDetectType(gp.id));
        return;
      }
    }
    c.innerHTML = '<p style="color:#888;text-align:center;font-size:14px;">' +
                  '🎮 Connect a gamepad…</p>';
    _gpadSVGLoaded = false;
  } else {
    const c = document.getElementById('gamepadSVGContainer');
    if (c) { c.style.display = 'none'; c.innerHTML = ''; }
    _gpadSVGLoaded = false;
    _gpadSVGCache = {};
  }
}

/** Create the overlay container if it doesn't exist. @private */
function _gpadEnsureContainer() {
  if (document.getElementById('gamepadSVGContainer')) return;
  const c = document.createElement('div');
  c.id = 'gamepadSVGContainer';
  c.style.cssText = 'width:100%;max-width:480px;margin:12px auto;' +
    'text-align:center;position:relative;user-select:none;pointer-events:none';
  // Insert after controlPanel
  const anchor = document.getElementById('controlPanel')
              || document.getElementById('scriptPanel');
  if (anchor) anchor.parentNode.insertBefore(c, anchor.nextSibling);
  else document.body.prepend(c);
}

/**
 * Detect gamepad family from the navigator id string.
 * @param {string} gpId
 * @returns {string} 'xbox' | 'sony' | 'nintendo'
 * @private
 */
function _gpadDetectType(gpId) {
  const id = (gpId || '').toLowerCase();
  if (id.includes('dualsense') || id.includes('dualshock') ||
      id.includes('playstation') || id.includes('ps3') ||
      id.includes('ps4') || id.includes('ps5') ||
      id.includes('054c'))                             return 'sony';
  if (id.includes('switch') || id.includes('pro controller') ||
      id.includes('057e'))                             return 'nintendo';
  return 'xbox'; // default / XInput / generic
}

/**
 * Fetch the SVG and inject it into the overlay container.
 * @param {string} type - 'xbox' | 'sony' | 'nintendo'
 * @private
 */
function _gpadLoadSVG(type) {
  const c = document.getElementById('gamepadSVGContainer');
  if (!c) return;
  _gpadSVGLoaded = false;
  _gpadSVGCache = {};
  _gpadSVGType = type;
  c.innerHTML = '<p style="color:#888;text-align:center;font-size:13px;">Loading…</p>';

  const file = _GPAD_SVG_FILES[type] || _GPAD_SVG_FILES.xbox;
  fetch(file)
    .then(r => { if (!r.ok) throw new Error(r.status); return r.text(); })
    .then(svg => {
      if (!_gpadOverlayEnabled) return;
      c.innerHTML = svg;
      const svgEl = c.querySelector('svg');
      if (svgEl) {
        svgEl.style.width = '100%';
        svgEl.style.height = 'auto';
        svgEl.style.maxHeight = '300px';
        svgEl.removeAttribute('width');
        svgEl.removeAttribute('height');
      }
      _gpadCacheElements(c, type);
      _gpadSVGLoaded = true;
      _scriptLog('🎮 Gamepad SVG loaded: ' + file);
    })
    .catch(err => {
      c.innerHTML = '<p style="color:#c66;text-align:center;font-size:13px;">' +
        '⚠️ SVG not found: ' + file + '</p>';
      _scriptLog('⚠️ Failed to load gamepad SVG: ' + err.message);
    });
}

/**
 * Find SVG elements by inkscape:label and cache their references + original fills.
 * @param {HTMLElement} container
 * @param {string} type
 * @private
 */
function _gpadCacheElements(container, type) {
  _gpadSVGCache = {};
  const all = container.querySelectorAll('*');

  // Build a set of labels we care about
  const btnMap = _GPAD_BTN_MAP[type] || {};
  const wantedLabels = new Set();
  Object.values(btnMap).forEach(l => wantedLabels.add(l));
  Object.values(_GPAD_STICK_CLICK).forEach(l => wantedLabels.add(l));
  wantedLabels.add('stickLeft');
  wantedLabels.add('stickRight');

  all.forEach(el => {
    const label = el.getAttribute('inkscape:label');
    if (label && wantedLabels.has(label)) {
      // Read original fill from inline style or attribute
      const cs = el.style.fill || el.getAttribute('fill') || '';
      _gpadSVGCache[label] = { el: el, origFill: cs || '#bbbbbb' };
    }
  });

  const n = Object.keys(_gpadSVGCache).length;
  if (n === 0) {
    _scriptLog('⚠️ SVG loaded but no matching inkscape:label found.');
  } else {
    _scriptLog('🎮 Cached ' + n + ' SVG elements for live feedback');
  }
}

/**
 * Set fill color on a cached SVG element.
 * @param {string} label - inkscape:label
 * @param {string} color - CSS color, or null to restore original
 * @private
 */
function _gpadSetFill(label, color) {
  const c = _gpadSVGCache[label];
  if (!c) return;
  c.el.style.fill = color || c.origFill;
}

/**
 * Update all SVG button + stick visuals from live gamepad state.
 * Called every poll frame when the overlay is enabled.
 * @param {Gamepad} gamepad
 * @private
 */
function _gpadUpdateSVG(gamepad) {
  if (!_gpadSVGLoaded || !_gpadOverlayEnabled) return;

  const btnMap = _GPAD_BTN_MAP[_gpadSVGType] || {};

  // ── Buttons ──
  for (const [idx, label] of Object.entries(btnMap)) {
    const btn = gamepad.buttons[idx];
    if (!btn) continue;
    const pressed = btn.pressed || btn.value > 0.5;
    _gpadSetFill(label, pressed ? _GPAD_COLOR_PRESSED : null);
  }

  // ── Stick clicks (flash the stick element) ──
  for (const [idx, label] of Object.entries(_GPAD_STICK_CLICK)) {
    const btn = gamepad.buttons[idx];
    if (!btn) continue;
    const pressed = btn.pressed || btn.value > 0.5;
    if (pressed) _gpadSetFill(label, _GPAD_COLOR_PRESSED);
    // don't reset here — stick position color takes over below
  }

  // ── Sticks ──
  _gpadUpdateStick('stickLeft',  gamepad.axes[0] || 0, gamepad.axes[1] || 0);
  _gpadUpdateStick('stickRight', gamepad.axes[2] || 0, gamepad.axes[3] || 0);
}

/**
 * Translate and tint a stick element based on axis values.
 * @param {string} label - 'stickLeft' or 'stickRight'
 * @param {number} x - axis X  (-1 … +1)
 * @param {number} y - axis Y  (-1 … +1)
 * @private
 */
function _gpadUpdateStick(label, x, y) {
  const c = _gpadSVGCache[label];
  if (!c) return;

  const mag = Math.sqrt(x * x + y * y);
  const dead = mag < _GPAD_DEADZONE;

  // Translate
  const ox = dead ? 0 : x * _GPAD_STICK_OFFSET;
  const oy = dead ? 0 : y * _GPAD_STICK_OFFSET;
  const orig = c.origTransform || '';

  // Store original transform once
  if (c.origTransform === undefined) {
    c.origTransform = c.el.getAttribute('transform') || '';
  }

  c.el.setAttribute('transform',
    (c.origTransform ? c.origTransform + ' ' : '') +
    'translate(' + ox.toFixed(1) + ',' + oy.toFixed(1) + ')');

  // Tint based on deflection magnitude
  if (dead) {
    c.el.style.fill = c.origFill;
  } else {
    // Interpolate from original grey to sky blue based on magnitude
    const t = Math.min(mag, 1);
    c.el.style.fill = _gpadLerpColor(c.origFill, _GPAD_COLOR_STICK, t);
  }
}

/**
 * Simple hex color lerp.
 * @param {string} a - start color (hex or named)
 * @param {string} b - end color (hex)
 * @param {number} t - 0…1
 * @returns {string} hex color
 * @private
 */
function _gpadLerpColor(a, b, t) {
  const pa = _gpadParseColor(a);
  const pb = _gpadParseColor(b);
  const r = Math.round(pa[0] + (pb[0] - pa[0]) * t);
  const g = Math.round(pa[1] + (pb[1] - pa[1]) * t);
  const bl = Math.round(pa[2] + (pb[2] - pa[2]) * t);
  return '#' + ((1 << 24) | (r << 16) | (g << 8) | bl).toString(16).slice(1);
}

/** Parse hex color to [r,g,b]. Falls back to [187,187,187] (#bbbbbb). @private */
function _gpadParseColor(c) {
  if (!c || c.charAt(0) !== '#') return [187, 187, 187];
  const hex = c.length === 4
    ? c[1]+c[1]+c[2]+c[2]+c[3]+c[3]
    : c.slice(1,7);
  const n = parseInt(hex, 16);
  return [(n >> 16) & 255, (n >> 8) & 255, n & 255];
}

/**
 * Called from onGamepadConnected when overlay is active.
 * @param {Gamepad} gamepad
 * @private
 */
function _gpadOnConnect(gamepad) {
  if (!_gpadOverlayEnabled) return;
  _gpadEnsureContainer();
  _gpadLoadSVG(_gpadDetectType(gamepad.id));
}

/**
 * Called from onGamepadDisconnected when overlay is active.
 * @private
 */
function _gpadOnDisconnect() {
  if (!_gpadOverlayEnabled) return;
  _gpadSVGLoaded = false;
  _gpadSVGCache = {};
  const c = document.getElementById('gamepadSVGContainer');
  if (c) {
    c.innerHTML = '<p style="color:#888;text-align:center;font-size:14px;">' +
                  '🎮 Connect a gamepad…</p>';
  }
}

// ── Initialization ────────────────────────────────────────────────────────────

const CUSTOMCONTROL = {
  _onKeyUpWarned: false,
  onKeyUp: function (event) {
    if (this._onKeyUpWarned) return;
    this._onKeyUpWarned = true;
    _scriptLog('');
    _scriptLog('❌ ERROR: CUSTOMCONTROL.onKeyUp() not defined!');
    _scriptLog('═════════════════════════════════════════════════════════════');
    _scriptLog('WHY: The keyboard key-up event was triggered, but you haven\'t');
    _scriptLog('     defined what should happen when keys are released.');
    _scriptLog('');
    _scriptLog('HOW TO FIX: Add this to your code in the Script Editor textarea:');
    _scriptLog('');
    _scriptLog('CUSTOMCONTROL.onKeyUp = function(event) {');
    _scriptLog('  const key = event.key.toLowerCase();');
    _scriptLog('  _scriptLog("Key released: " + key);');
    _scriptLog('  if (key === "w") stop();  // Example: stop on W key release');
    _scriptLog('};');
    _scriptLog('');
    _scriptLog('LEARN MORE: See "Full Coding Guide" → "Step 3: Handle Events"');
    _scriptLog('═════════════════════════════════════════════════════════════');
    _scriptLog('Key released: ' + event.key);
  },

  _onKeyDownWarned: false,
  onKeyDown: function (event) {
    if (this._onKeyDownWarned) return;
    this._onKeyDownWarned = true;
    _scriptLog('');
    _scriptLog('❌ ERROR: CUSTOMCONTROL.onKeyDown() not defined!');
    _scriptLog('═════════════════════════════════════════════════════════════');
    _scriptLog('WHY: A keyboard key was pressed, but you haven\'t defined');
    _scriptLog('     what should happen when keys are pressed.');
    _scriptLog('');
    _scriptLog('HOW TO FIX: Add this to your code in the Script Editor textarea:');
    _scriptLog('');
    _scriptLog('CUSTOMCONTROL.onKeyDown = function(event) {');
    _scriptLog('  const key = event.key.toLowerCase();');
    _scriptLog('  _scriptLog("Key pressed: " + key);');
    _scriptLog('  if (key === "w") moveForward();  // Example: move on W key');
    _scriptLog('};');
    _scriptLog('');
    _scriptLog('LEARN MORE: See "Full Coding Guide" → "Step 3: Handle Events"');
    _scriptLog('═════════════════════════════════════════════════════════════');
    _scriptLog('Key pressed: ' + event.key);
  },

  _processGamepadInputWarned: false,
  processGamepadInput: function (gamepad) {
    if (this._processGamepadInputWarned) return;
    this._processGamepadInputWarned = true;
    _scriptLog('');
    _scriptLog('❌ ERROR: CUSTOMCONTROL.processGamepadInput() not defined!');
    _scriptLog('═════════════════════════════════════════════════════════════');
    _scriptLog('WHY: A gamepad was detected, but you haven\'t defined');
    _scriptLog('     what buttons/sticks should do.');
    _scriptLog('');
    _scriptLog('HOW TO FIX: Add this to your code in the Script Editor textarea:');
    _scriptLog('');
    _scriptLog('CUSTOMCONTROL.processGamepadInput = function(gamepad) {');
    _scriptLog('  if (gamepad.buttons[XBOX_BUTTONS.DPAD_UP].pressed) {');
    _scriptLog('    moveForward();');
    _scriptLog('  } else if (gamepad.buttons[XBOX_BUTTONS.DPAD_DOWN].pressed) {');
    _scriptLog('    moveBackward();');
    _scriptLog('  } else {');
    _scriptLog('    stop();');
    _scriptLog('  }');
    _scriptLog('};');
    _scriptLog('');
    _scriptLog('LEARN MORE: See "Full Coding Guide" → "Step 3: Handle Events"');
    _scriptLog('═════════════════════════════════════════════════════════════');
    _scriptLog('Gamepad detected: ' + gamepad.id);
    _scriptLog('Define CUSTOMCONTROL.processGamepadInput() to use this gamepad');
  }
};
// ── Keep this  ───────────────────────────────────────────────────

function onKeyDown(event) {   CUSTOMCONTROL.onKeyDown(event);  }
function onKeyUp(event) {  CUSTOMCONTROL.onKeyUp(event);}
function processGamepadInput(gamepad) {  CUSTOMCONTROL.processGamepadInput(gamepad);}


// ── Main Event Listeners ───────────────────────────────────────────────────

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
  _scriptLog('⚠️ Missing your own function in CUSTOMCONTROL.processGamepadInput(gamepad).');
}

/**
 * Handle gamepad connection
 */
function onGamepadConnected(event) {
  const gamepad = event.gamepad;
  _scriptLog('Gamepad connected:', gamepad.id);
  
  gamepadConnected = true;
  gamepadIndex = gamepad.index;
  
  // Update UI
  updateGamepadStatus(true, gamepad.id);
  
  // Update SVG overlay if enabled
  _gpadOnConnect(gamepad);
  
  // Start polling loop
  if (!animationFrameId) {
    pollGamepad();
  }
}

/**
 * Handle gamepad disconnection
 */
function onGamepadDisconnected(event) {
  _scriptLog('Gamepad disconnected:', event.gamepad.id);
  
  gamepadConnected = false;
  gamepadIndex = -1;
  
  // Update UI
  updateGamepadStatus(false, '');
  
  // Update SVG overlay
  _gpadOnDisconnect();
  
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
 * Poll gamepad state with 50ms minimum interval throttling
 */
function pollGamepad() {
  if (!gamepadConnected) return;
  
  const now = performance.now();
  const timeSinceLastPoll = now - lastGamepadPollTime;
  
  // Only process if pollIntervalMs  has elapsed since last poll
  if (timeSinceLastPoll >= pollIntervalMs ) {
    const gamepads = navigator.getGamepads();
    const gamepad = gamepads[gamepadIndex];
    
    if (gamepad) {
      processGamepadInput(gamepad);
      _gpadUpdateSVG(gamepad);
    }
    
    lastGamepadPollTime = now;
  }
  
  // Continue polling
  animationFrameId = requestAnimationFrame(pollGamepad);
}


/**
 * Handle button press/release with callbacks
 */
function handleGamepaButton(gamepad, buttonIndex, buttonName, onPress, onRelease) {
  const button = gamepad.buttons[buttonIndex];
  const isPressed = button.pressed || button.value > 0.5;
  const wasPressed = buttonStates[buttonIndex];
  
  // Update indicator
  updateButtonIndicator(buttonName, isPressed);
  
  // Detect press (rising edge)
  if (isPressed && !wasPressed) {
    _scriptLog(`Button ${buttonName} pressed`);
    if (onPress) onPress();
  }
  
  // Detect release (falling edge)
  if (!isPressed && wasPressed) {
    _scriptLog(`Button ${buttonName} released`);
    if (onRelease) onRelease();
  }
  
  // Update state
  buttonStates[buttonIndex] = isPressed;
}



// ── Servo Control Functions ───────────────────────────────────────────────────

/**
 * Set servo angle (for angular servos).
 * Uses fire-and-forget over the WebSocket WS bridge so that rapid
 * joystick inputs are never queued behind a pending response.
 *
 * @param {number} channel - Servo channel (0-5)
 * @param {number} angle - Angle in degrees (0 to 270 depending on type)
 */
function setServoAngle(channel, angle) {
  setServo270(channel, angle); // For simplicity, use the 270° function which can handle both types
}

/**
 * Set a 270° servo to a precise angle using calibrated PWM endpoints.
 * Provides accurate 1:1 degree mapping (0° → 0, 270° → 270).
 * Uses fire-and-forget over the WebSocket WS bridge.
 *
 * @param {number} channel - Servo channel (0-5)
 * @param {number} angle   - Angle in degrees (0 to 270)
 */
function setServo270(channel, angle) {
  if (typeof isMasterRegistered !== 'undefined' && !isMasterRegistered) {
    return;
  }

  const clamped  = Math.max(0, Math.min(270, Math.round(angle)));
  const mask     = (1 << channel) & 0xFF;
  const angleHi  = (clamped >> 8) & 0xFF;
  const angleLo  = clamped & 0xFF;
  const packet   = new Uint8Array([MOTOR_SERVO_ACTION.SET_SERVO270_ANGLE, mask, angleHi, angleLo]);

  if (typeof sendWSFireAndForget !== 'undefined') {
    sendWSFireAndForget(packet);
  }
}

/**
 * Attach or detach a servo by configuring its type.
 * Uses fire-and-forget over the WebSocket bridge.
 *
 * @param {number} channel - Servo channel (0-5)
 * @param {number} type - Servo type ( 1=ANGULAR_270, 2=ROTATIONAL)
 */
function attachServo(channel, type) {
  
  if (typeof isMasterRegistered !== 'undefined' && !isMasterRegistered) {
    return;
  }

  // Build packet: SET_SERVO_TYPE [servo_mask:u8] [type:u8]
  const mask = (1 << channel) & 0xFF;
  const packet = new Uint8Array([MOTOR_SERVO_ACTION.SET_SERVO_TYPE, mask, type & 0xFF]);

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
 * @param {Array<[number, number]>} pairs - Array of [channel, speed] pairs
 *   channel: servo channel number (0-5)
 *   speed: speed value (-100 to +100)
 * @example
 *   setServoSpeeds([[0, 100]]);           // servo 0 forward full speed
 *   setServoSpeeds([[0, 100], [1, -100]]); // servo 0 fwd, servo 1 rev
 */
function setServoSpeeds(pairs) {
  if (typeof isMasterRegistered !== 'undefined' && !isMasterRegistered) {
    return;
  }

  // Protocol: SET_SERVOS_SPEED [servo_mask:u8] [speed:i8] — one speed for all masked channels.
  // Group channels that share the same speed into one packet; send separate packets otherwise.
  const speedMap = new Map();
  for (const [channel, speed] of pairs) {
    speedMap.set(speed, (speedMap.get(speed) || 0) | (1 << channel));
  }

  if (typeof sendWSFireAndForget !== 'undefined') {
    speedMap.forEach((mask, speed) => {
      const packet = new Uint8Array([MOTOR_SERVO_ACTION.SET_SERVOS_SPEED, mask & 0xFF, speed & 0xFF]);
      sendWSFireAndForget(packet);
    });
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
