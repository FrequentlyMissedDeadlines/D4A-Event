'use strict';

// ── WebSocket Configuration ───────────────────────────────────────────────────

let ws = null;  // WebSocket connection to bridge
let wsConnected = false;
let wsQueue = [];  // Queue for messages sent while connecting
let wsConnectPromise = null;
let wsReconnectTimer = null;
let wsManualClose = false;
let pendingResponse = null;

let heartbeatInterval = null;
let heartbeatSendTime = 0;           // performance.now() of last heartbeat send
let heartbeatRTTHistory = [];        // rolling window of round-trip times (ms)
const MAX_HEARTBEAT_RTT_HISTORY = 10;
let isMasterRegistered = false;
let pingInterval = null;
let pingHistory = [];
let pingSequence = 0;
const MAX_PING_HISTORY = 100;

const WS_PORT = 80;
const WS_TIMEOUT = 500;
const HEARTBEAT_INTERVAL_MS = 40; // Send every 40ms (well under 50ms deadline)

// BotProto response codes — must match BotProto constants in BotMessageHandler.h
const BOT_RESP = {
  OK:               0x00, // BotProto::resp_ok
  INVALID_PARAMS:   0x01, // BotProto::resp_invalid_params
  INVALID_VALUES:   0x02, // BotProto::resp_invalid_values
  OPERATION_FAILED: 0x03, // BotProto::resp_operation_failed
  NOT_STARTED:      0x04, // BotProto::resp_not_started
  UNKNOWN_SERVICE:  0x05, // BotProto::resp_unknown_service
  UNKNOWN_CMD:      0x06, // BotProto::resp_unknown_cmd
  NOT_MASTER:       0x07  // BotProto::resp_not_master
};

// Human-readable names for BOT_RESP codes (used in debug displays)
const BOT_RESP_NAMES = {
  [BOT_RESP.OK]:               'OK',
  [BOT_RESP.INVALID_PARAMS]:   'INVALID_PARAMS',
  [BOT_RESP.INVALID_VALUES]:   'INVALID_VALUES',
  [BOT_RESP.OPERATION_FAILED]: 'OPERATION_FAILED',
  [BOT_RESP.NOT_STARTED]:      'NOT_STARTED',
  [BOT_RESP.UNKNOWN_SERVICE]:  'UNKNOWN_SERVICE',
  [BOT_RESP.UNKNOWN_CMD]:      'UNKNOWN_CMD',
  [BOT_RESP.NOT_MASTER]:       'NOT_MASTER'
};

// WS Action codes — AmakerBotService (service_id 0x04)
const WS_ACTION = {
  MASTER_REGISTER:   0x41, // AmakerBotService CMD_REGISTER
  MASTER_UNREGISTER: 0x42, // AmakerBotService CMD_UNREGISTER
  HEARTBEAT:         0x43, // AmakerBotService CMD_HEARTBEAT
  PING:              0x44  // AmakerBotService CMD_PING
};

// MotorServo action bytes — MotorServoService (service_id 0x02)
const MOTOR_SERVO_ACTION = {
  SET_MOTORS_SPEED:       0x21, // MotorServoConsts::CMD_SET_MOTORS_SPEED
  SET_SERVO_TYPE:         0x22, // MotorServoConsts::CMD_SET_SERVO_TYPE
  SET_SERVOS_SPEED:       0x23, // MotorServoConsts::CMD_SET_SERVOS_SPEED
  SET_SERVOS_ANGLE:       0x24, // MotorServoConsts::CMD_SET_SERVOS_ANGLE
  INCREMENT_SERVOS_ANGLE: 0x25, // MotorServoConsts::CMD_INCREMENT_SERVOS_ANGLE
  GET_MOTORS_SPEED:       0x26, // MotorServoConsts::CMD_GET_MOTORS_SPEED
  GET_SERVOS_ANGLE:       0x27, // MotorServoConsts::CMD_GET_SERVOS_ANGLE
  STOP_ALL_MOTORS:        0x28, // MotorServoConsts::CMD_STOP_ALL_MOTORS
  GET_BATTERY:            0x29, // MotorServoConsts::CMD_GET_BATTERY
  SET_SERVO270_ANGLE:     0x2A  // MotorServoConsts::CMD_SET_SERVO270_ANGLE
};

// DFR1216 action bytes — DFR1216Board (service_id 0x03)
const DFR1216_ACTION = {
  SET_LED_COLOR:    0x31, // DFR1216Consts::udp_action_set_led_color
  TURN_OFF_LED:     0x32, // DFR1216Consts::udp_action_turn_off_led
  TURN_OFF_ALL:     0x33, // DFR1216Consts::udp_action_turn_off_all_leds
  GET_LED_STATUS:   0x34, // DFR1216Consts::udp_action_get_led_status
  GET_BATTERY:      0x35  // DFR1216Consts::udp_action_get_battery
};


// ── Page Initialization ───────────────────────────────────────────────────────

document.addEventListener('DOMContentLoaded', () => {
  // Auto-populate bot IP from current location
  const currentHost = window.location.hostname;
  gBotIp = currentHost;  // Update global variable
  document.getElementById('botIp').value = currentHost;  // Update HTML field for consistency
  
  // Setup angle sliders
  for (let i = 0; i < 4; i++) {
    const slider = document.getElementById(`angle${i}`);
    const valueSpan = document.getElementById(`angle${i}-value`);
    if (slider && valueSpan) {
      slider.addEventListener('input', (e) => {
        valueSpan.textContent = e.target.value + '°';
      });
    }
  }
  
  // Setup speed sliders
  for (let i = 4; i < 8; i++) {
    const slider = document.getElementById(`speed${i}`);
    const valueSpan = document.getElementById(`speed${i}-value`);
    if (slider && valueSpan) {
      slider.addEventListener('input', (e) => {
        valueSpan.textContent = e.target.value;
      });
    }
  }
  
  updateUIState();
  showStatus('WS demo loaded. Enter master token to register.', false);
  
  // Initialize ping graph
  initPingGraph();
});

window.addEventListener('beforeunload', cleanupRealtimeConnections);
window.addEventListener('pagehide', cleanupRealtimeConnections);

// ── WebSocket Bridge Management ───────────────────────────────────────────────

/**
 * Cleanly stop timers and WebSocket activity when leaving the page.
 */
function cleanupRealtimeConnections() {
  stopHeartbeat();
  stopPing();
  closeWebSocket();
}

/**
 * Reject the currently pending WS response, if any.
 *
 * @param {Error} error - Reason for rejection.
 */
function rejectPendingResponse(error) {
  if (!pendingResponse) {
    return;
  }

  clearTimeout(pendingResponse.timeoutId);
  pendingResponse.reject(error);
  pendingResponse = null;
}

/**
 * Schedule a reconnect only when the page still needs a live master session.
 */
function scheduleReconnect() {
  if (wsReconnectTimer || wsManualClose || !isMasterRegistered) {
    return;
  }

  wsReconnectTimer = setTimeout(() => {
    wsReconnectTimer = null;
    initializeWebSocket().catch((error) => {
      console.error('[WS] Reconnect failed:', error);
    });
  }, 3000);
}

/**
 * Initialize WebSocket connection to the bridge at /ws
 */
function initializeWebSocket() {
  if (wsConnected && ws && ws.readyState === WebSocket.OPEN) {
    return Promise.resolve(ws);
  }

  if (wsConnectPromise) {
    return wsConnectPromise;
  }

  if (wsReconnectTimer) {
    clearTimeout(wsReconnectTimer);
    wsReconnectTimer = null;
  }

  
  // Use global variables set by getBotControl() or HTML form
  const botIp = gBotIp || document.getElementById('botIp').value.trim();
  const port = gBotPort || document.getElementById('botPort').value.trim();
  // Only add port if it's explicitly specified (not default)
  const portStr = port ? `:${port}` : ':80';
  const wsUrl = `ws://${botIp}${portStr}/ws`;
  wsManualClose = false;
  console.log('[WS] initializeWebSocket '+wsUrl);
  console.log(`[WS] Connecting to ${wsUrl}`);

  wsConnectPromise = new Promise((resolve, reject) => {
    try {
      const socket = new WebSocket(wsUrl);
      ws = socket;
      ws.binaryType = 'arraybuffer';

      const connectionTimeout = setTimeout(() => {
        if (socket.readyState === WebSocket.CONNECTING) {
          console.error('[WS] Connection timeout - closing socket');
          socket.close();
        }
      }, 5000);

      socket.onopen = () => {
        clearTimeout(connectionTimeout);

        if (ws !== socket) {
          socket.close();
          return;
        }

        wsConnected = true;
        wsConnectPromise = null;
        console.log('[WS] Connected.');
        showStatus('WebSocket connected', false);
        updateUIState();
        resolve(socket);
      };

      socket.onmessage = (event) => {
        if (ws !== socket) {
          return;
        }

        const response = new Uint8Array(event.data);
        console.log(`[WS RX] ${Array.from(response).map(b => b.toString(16).padStart(2, '0')).join('')}`);
        processWSResponse(response);
      };

      socket.onerror = (error) => {
        clearTimeout(connectionTimeout);

        if (ws !== socket) {
          return;
        }

        wsConnected = false;
        console.error('[WS] Error:', error);
        console.error('[WS] Connection failed. URL:', wsUrl);
        console.error('[WS] Bot IP:', botIp);
        console.error('[WS] ReadyState:', socket.readyState);
        showStatus('WebSocket bridge error: Check bot IP and firewall', true);
        updateUIState();
      };

      socket.onclose = () => {
        clearTimeout(connectionTimeout);

        if (ws !== socket) {
          return;
        }

        ws = null;
        wsConnected = false;
        wsConnectPromise = null;
        rejectPendingResponse(new Error('WebSocket bridge disconnected'));
        console.log('[WS] Disconnected');
        showStatus('WebSocket bridge disconnected', true);
        updateUIState();
        scheduleReconnect();
      };
    } catch (error) {
      wsConnectPromise = null;
      console.error('[WS] Failed to create WebSocket:', error);
      showStatus('Failed to connect to WebSocket bridge', true);
      reject(error);
    }
  });

  return wsConnectPromise;
}

/**
 * Create WebSocket bridge if not already created
 * Now we use the WebSocket bridge instead
 */
function ensureWebSocketBridge() {
  const botIp = gBotIp || document.getElementById('botIp').value.trim();
  
  if (!botIp) {
    console.error('Bot IP address is required');
    showStatus('Please enter bot IP address', true);
    return false;
  }
  
  // WebSocket is the bridge - check if connected
  if (!wsConnected) {
    console.error('WebSocket bridge not connected');
    showStatus('WebSocket bridge not connected', true);
    return false;
  }
  
  return true;
}

/**
 * Close WebSocket connection
 */
function closeWebSocket() {
  wsManualClose = true;

  if (wsReconnectTimer) {
    clearTimeout(wsReconnectTimer);
    wsReconnectTimer = null;
  }

  rejectPendingResponse(new Error('WebSocket bridge closed'));

  if (ws) {
    ws.close();
    ws = null;
    wsConnected = false;
    wsQueue = [];
    console.log('[WS] Closed');
  }

  wsConnectPromise = null;
  updateUIState();
}

// ── Master Registration ───────────────────────────────────────────────────────

/**
 * Register this client as master controller
 */
async function registerMaster() {
  const token = document.getElementById('masterToken').value.trim();
  
  if (!token || token.length !== 5) {
    showStatus('Please enter a valid 5-character token', true);
    console.error('Invalid token:', token);
    return;
  }
  
  // Initialize WebSocket if not already connected
  if (!wsConnected) {
    showStatus('Initializing WebSocket bridge...', false);
    console.info('Initializing WebSocket bridge...');
    await initializeWebSocket();
  }
  

  
  try {
    showStatus('Registering as master...', false);

    // Build WS packet: 0x41 + token bytes
    const packet = new Uint8Array(1 + token.length);
    packet[0] = WS_ACTION.MASTER_REGISTER;
    for (let i = 0; i < token.length; i++) {
      packet[i + 1] = token.charCodeAt(i);
    }
    
    // Send WS packet (simulated via HTTP proxy)
    const response = await sendWSPacket(packet);
    
    // Parse response: [action][status] — always 2 bytes
    if (response && response.length >= 2) {
      const statusByte = response[1];
      handleMasterRegistrationResponse(statusByte, token);
    } else {
      showStatus('Invalid response from bot', true);
      updateLastResponse('Invalid response');
    }
    
  } catch (error) {
    console.error('Master registration failed:', error);
    showStatus('Registration failed: ' + error.message, true);
    updateLastResponse('Error: ' + error.message);
  }
}

/**
 * Handle master registration response
 */
function handleMasterRegistrationResponse(statusByte, token) {
  const statusName = BOT_RESP_NAMES[statusByte] || `UNKNOWN(0x${statusByte.toString(16)})`;
  updateLastResponse(`MASTER_REGISTER: ${statusName}`);
  
  if (statusByte === BOT_RESP.OK) {
    isMasterRegistered = true;
    showStatus(`✓ Registered as master with token ${token}`, false);
    startHeartbeat();
    updateUIState();
  } else if (statusByte === BOT_RESP.OPERATION_FAILED) {
    showStatus('Registration ignored (master already registered)', true);
  } else if (statusByte === BOT_RESP.NOT_MASTER) {
    showStatus('Registration denied (invalid token)', true);
  } else {
    showStatus(`Registration failed: ${statusName}`, true);
  }
}

/**
 * Unregister as master controller
 */
async function unregisterMaster() {
  
  try {
    showStatus('Unregistering master...', false);
    
    // Build WS packet: 0x42
    const packet = new Uint8Array([WS_ACTION.MASTER_UNREGISTER]);
    
    // Send WS packet
    const response = await sendWSPacket(packet);
    
    // Parse response: 0x42 + status byte
    if (response && response.length >= 2) {
      const statusByte = response[1];
      handleMasterUnregistrationResponse(statusByte);
    } else {
      showStatus('Invalid response from bot', true);
      updateLastResponse('Invalid response');
    }
    
  } catch (error) {
    console.error('Master unregistration failed:', error);
    showStatus('Unregistration failed: ' + error.message, true);
    updateLastResponse('Error: ' + error.message);
  }
}

/**
 * Handle master unregistration response
 */
function handleMasterUnregistrationResponse(statusByte) {
  const statusName = BOT_RESP_NAMES[statusByte] || `UNKNOWN(0x${statusByte.toString(16)})`;
  updateLastResponse(`MASTER_UNREGISTER: ${statusName}`);
  
  if (statusByte === BOT_RESP.OK) {
    isMasterRegistered = false;
    showStatus('✓ Successfully unregistered as master', false);
    stopHeartbeat();
    closeWebSocket();
    updateUIState();
  } else if (statusByte === BOT_RESP.NOT_MASTER) {
    showStatus('Unregistration denied (not the registered master)', true);
  } else {
    showStatus(`Unregistration failed: ${statusName}`, true);
  }
}

// ── Bot Name Management ───────────────────────────────────────────────────────

/**
 * Set the bot name via WS
 */
async function setBotName() {
  const botName = document.getElementById('botName').value.trim();
  
  if (!botName) {
    showStatus('Please enter a bot name', true);
    return;
  }
  
  if (!isMasterRegistered) {
    showStatus('Must be registered as master to set bot name', true);
    return;
  }
  
  try {
    showStatus('Setting bot name...', false);
    
    // Build WS packet: "AMAKERBOT:setname:<name>"
    const message = `AMAKERBOT:setname:${botName}`;
    const packet = new TextEncoder().encode(message);
    
    // Send WS packet
    const response = await sendWSPacket(packet);
    
    // Parse text response
    if (response && response.length > 0) {
      const responseText = new TextDecoder().decode(response);
      handleBotNameResponse(responseText, botName);
    } else {
      showStatus('No response from bot', true);
      updateLastResponse('No response');
    }
    
  } catch (error) {
    console.error('Set bot name failed:', error);
    showStatus('Failed to set bot name: ' + error.message, true);
    updateLastResponse('Error: ' + error.message);
  }
}

/**
 * Handle bot name response
 */
function handleBotNameResponse(responseText, botName) {
  updateLastResponse(`SET_NAME: ${responseText}`);
  
  try {
    const response = JSON.parse(responseText);
    if (response.result === 'ok') {
      showStatus(`✓ Bot name set to "${botName}"`, false);
    } else {
      showStatus(`Failed: ${response.message || 'unknown error'}`, true);
    }
  } catch (e) {
    showStatus('Invalid response format', true);
  }
}

// ── Board Info ───────────────────────────────────────────────────────────────

/**
 * Request the battery level from the board.
 * Response: [DFR1216_ACTION.GET_BATTERY][resp_ok][level:u8  0-100]
 */
async function getBattery() {
  if (!wsConnected) {
    showStatus('WebSocket not connected', true);
    return;
  }

  try {
    showStatus('Reading battery level...', false);
    const packet   = new Uint8Array([DFR1216_ACTION.GET_BATTERY]);
    const response = await sendWSPacket(packet);

    if (response && response.length >= 3 && response[1] === BOT_RESP.OK) {
      const level = response[2];
      showStatus(`🔋 Battery: ${level}%`, false);
      updateLastResponse(`GET_BATTERY: ${level}%`);
      const elem = document.getElementById('batteryLevel');
      if (elem) elem.textContent = `${level}%`;
    } else {
      const respName = response && response.length >= 2
        ? (BOT_RESP_NAMES[response[1]] || `0x${response[1].toString(16)}`)
        : 'no response';
      showStatus(`Battery read failed: ${respName}`, true);
      updateLastResponse(`GET_BATTERY: ${respName}`);
    }
  } catch (error) {
    console.error('getBattery failed:', error);
    showStatus('Battery read error: ' + error.message, true);
    updateLastResponse('Error: ' + error.message);
  }
}

// ── Servo Attachment Management ───────────────────────────────────────────────

/**
 * Select all servo channels
 */
function selectAllServos() {
  for (let i = 0; i < 8; i++) {
    document.getElementById(`servo${i}`).checked = true;
  }
}

/**
 * Deselect all servo channels
 */
function deselectAllServos() {
  for (let i = 0; i < 8; i++) {
    document.getElementById(`servo${i}`).checked = false;
  }
}

/**
 * Get selected servo channels as bitmask
 * @returns {number} Bitmask where bit N = servo channel N
 */
function getServoMask() {
  let mask = 0;
  for (let i = 0; i < 8; i++) {
    if (document.getElementById(`servo${i}`).checked) {
      mask |= (1 << i);
    }
  }
  return mask;
}

/**
 * Attach servos with selected type
 */
async function attachServos() {
  if (!isMasterRegistered) {
    showStatus('Must be registered as master to attach servos', true);
    return;
  }
  
  const mask = getServoMask();
  if (mask === 0) {
    showStatus('Please select at least one servo channel', true);
    return;
  }
  
  const type = parseInt(document.getElementById('servoType').value);
  
  try {
    showStatus('Attaching servos...', false);
    
    // Build WS packet: SET_SERVO_TYPE [mask] [type]
    const packet = new Uint8Array([MOTOR_SERVO_ACTION.SET_SERVO_TYPE, mask, type]);
    
    // Send WS packet
    const response = await sendWSPacket(packet);
    
    // Parse response: 0x22 + resp_code
    if (response && response.length >= 2) {
      const respCode = response[1];
      handleServoAttachResponse(respCode, mask, type);
    } else {
      showStatus('Invalid response from bot', true);
      updateLastResponse('Invalid response');
    }
    
  } catch (error) {
    console.error('Attach servos failed:', error);
    showStatus('Failed to attach servos: ' + error.message, true);
    updateLastResponse('Error: ' + error.message);
  }
}

/**
 * Detach selected servos (attach with type=0)
 */
async function detachServos() {
  if (!isMasterRegistered) {
    showStatus('Must be registered as master to detach servos', true);
    return;
  }
  
  const mask = getServoMask();
  if (mask === 0) {
    showStatus('Please select at least one servo channel', true);
    return;
  }
  
  try {
    showStatus('Detaching servos...', false);
    
    // Build WS packet: SET_SERVO_TYPE [mask] [type=0]
    const packet = new Uint8Array([MOTOR_SERVO_ACTION.SET_SERVO_TYPE, mask, 0]);
    
    // Send WS packet
    const response = await sendWSPacket(packet);
    
    // Parse response: 0x22 + resp_code
    if (response && response.length >= 2) {
      const respCode = response[1];
      handleServoAttachResponse(respCode, mask, 0);
    } else {
      showStatus('Invalid response from bot', true);
      updateLastResponse('Invalid response');
    }
    
  } catch (error) {
    console.error('Detach servos failed:', error);
    showStatus('Failed to detach servos: ' + error.message, true);
    updateLastResponse('Error: ' + error.message);
  }
}

/**
 * Handle servo attach/detach response
 */
function handleServoAttachResponse(respCode, mask, type) {
  const typeNames = [ '270° Angular', 'Continuous'];
  const typeName = typeNames[type] || `Type ${type}`;
  
  const respName = BOT_RESP_NAMES[respCode] || `0x${respCode.toString(16)}`;
  updateLastResponse(`ATTACH_SERVO (mask=0x${mask.toString(16)}, type=${type}): ${respName}`);
  
  if (respCode === BOT_RESP.OK) {
    const channels = [];
    for (let i = 0; i < 8; i++) {
      if (mask & (1 << i)) channels.push(i);
    }
    const action = type === 0 ? 'detached' : `attached as ${typeName}`;
    showStatus(`✓ Servo channels ${channels.join(', ')} ${action}`, false);
  } else if (respCode === BOT_RESP.NOT_MASTER) {
    showStatus('Not authorized - not registered as master', true);
  } else {
    showStatus(`Failed: ${respName}`, true);
  }
}
// ── Servo Control ─────────────────────────────────────────────────────────────

/**
 * Set servo angles for angular servos (0x24)
 * Sets all selected servos to the same angle value
 */
async function setServoAngles() {
  if (!isMasterRegistered) {
    showStatus('Must be registered as master to control servos', true);
    return;
  }
  
  try {
    showStatus('Setting servo angles...', false);
    
    // Get the angle value from the first slider
    const angle = parseInt(document.getElementById('angle0').value);
    
    // Get selected servo mask
    const mask = getServoMask();
    if (mask === 0) {
      showStatus('Please select at least one servo channel', true);
      return;
    }
    
    // Convert angle to big-endian i16 format
    const angleValue = Math.round(angle);
    const angleHi = (angleValue >> 8) & 0xFF;
    const angleLo = angleValue & 0xFF;
    
    // Build packet: SET_SERVOS_ANGLE [servo_mask] [angle_hi] [angle_lo]
    const packet = new Uint8Array([MOTOR_SERVO_ACTION.SET_SERVOS_ANGLE, mask, angleHi, angleLo]);
    
    // Send WS packet
    const response = await sendWSPacket(packet);
    
    // Parse response: 0x24 + resp_code
    if (response && response.length >= 2) {
      const respCode = response[1];
      handleServoAngleResponse(respCode);
    } else {
      showStatus('Invalid response from bot', true);
      updateLastResponse('Invalid response');
    }
    
  } catch (error) {
    console.error('Set servo angles failed:', error);
    showStatus('Failed to set angles: ' + error.message, true);
    updateLastResponse('Error: ' + error.message);
  }
}

/**
 * Handle servo angle response
 */
function handleServoAngleResponse(respCode) {
  const respName = BOT_RESP_NAMES[respCode] || `0x${respCode.toString(16)}`;
  updateLastResponse(`SET_SERVO_ANGLE: ${respName}`);
  
  if (respCode === BOT_RESP.OK) {
    showStatus('✓ Servo angles set', false);
  } else if (respCode === BOT_RESP.OPERATION_FAILED) {
    showStatus('Operation failed (check servo types and angle ranges)', true);
  } else if (respCode === BOT_RESP.NOT_MASTER) {
    showStatus('Not authorized - not registered as master', true);
  } else {
    showStatus(`Failed: ${respName}`, true);
  }
}

/**
 * Center all angle servos to 0°
 */
function centerAllAngles() {
  for (let i = 0; i < 4; i++) {
    const slider = document.getElementById(`angle${i}`);
    const valueSpan = document.getElementById(`angle${i}-value`);
    if (slider && valueSpan) {
      slider.value = 0;
      valueSpan.textContent = '0°';
    }
    const checkbox = document.getElementById(`angle${i}-enable`);
    if (checkbox) checkbox.checked = true;
  }
  showStatus('All angles centered to 0°', false);
}

/**
 * Set servo speeds for rotational servos (0x22)
 */
async function setServoSpeeds() {
  if (!isMasterRegistered) {
    showStatus('Must be registered as master to control servos', true);
    return;
  }
  
  try {
    showStatus('Setting servo speeds...', false);
    
    // Build packet: 0x23 [servo_mask] [speed]
    let mask = 0;
    const speeds = [];
    
    // Always send all 8 speed bytes (protocol requirement)
    for (let i = 0; i < 8; i++) {
      let speed = 0;
      
      if (i >= 4 && i <= 7) {
        // Speed channels (4-7)
        const enabled = document.getElementById(`speed${i}-enable`).checked;
        if (enabled) {
          mask |= (1 << i);
          speed = parseInt(document.getElementById(`speed${i}`).value);
        }
      }
      
      // Encode: speed + 128
      const encoded = speed + 128;
      speeds.push(encoded);
    }
    
    if (mask === 0) {
      showStatus('Please enable at least one channel', true);
      return;
    }
    
    // Get speed value from first slider (all selected channels get same speed)
    let speed = 0;
    for (let i = 4; i < 8; i++) {
      const slider = document.getElementById(`speed${i}`);
      if (slider) {
        speed = parseInt(slider.value);
        break;
      }
    }
    
    // Build packet: SET_SERVOS_SPEED [servo_mask] [speed]
    // speed is -100 to +100, encoded as-is (i8)
    const packet = new Uint8Array([MOTOR_SERVO_ACTION.SET_SERVOS_SPEED, mask, speed & 0xFF]);
    
    // Send WS packet
    const response = await sendWSPacket(packet);
    
    // Parse response: 0x23 + resp_code
    if (response && response.length >= 2) {
      const respCode = response[1];
      handleServoSpeedResponse(respCode, mask);
    } else {
      showStatus('Invalid response from bot', true);
      updateLastResponse('Invalid response');
    }
    
  } catch (error) {
    console.error('Set servo speeds failed:', error);
    showStatus('Failed to set speeds: ' + error.message, true);
    updateLastResponse('Error: ' + error.message);
  }
}

/**
 * Handle servo speed response
 */
function handleServoSpeedResponse(respCode, mask) {
  const respName = BOT_RESP_NAMES[respCode] || `0x${respCode.toString(16)}`;
  updateLastResponse(`SET_SERVO_SPEED (mask=0x${mask.toString(16)}): ${respName}`);
  
  if (respCode === BOT_RESP.OK) {
    showStatus('✓ Servo speeds set', false);
  } else if (respCode === BOT_RESP.OPERATION_FAILED) {
    showStatus('Operation failed (check servo types are rotational)', true);
  } else if (respCode === BOT_RESP.NOT_MASTER) {
    showStatus('Not authorized - not registered as master', true);
  } else {
    showStatus(`Failed: ${respName}`, true);
  }
}

/**
 * Stop all servos (0x23)
 */
async function stopAllServos() {
  if (!isMasterRegistered) {
    showStatus('Must be registered as master to stop servos', true);
    return;
  }
  
  try {
    showStatus('Stopping all servos...', false);
    
    // Build packet: STOP_ALL_MOTORS (no parameters)
    const packet = new Uint8Array([MOTOR_SERVO_ACTION.STOP_ALL_MOTORS]);
    
    // Send WS packet
    const response = await sendWSPacket(packet);
    
    // Parse response: STOP_ALL_MOTORS + resp_code
    if (response && response.length >= 2) {
      const respCode = response[1];
      if (respCode === BOT_RESP.OK) {
        showStatus('✓ All servos stopped', false);
        updateLastResponse('STOP_SERVOS: OK');
        
        // Reset speed sliders to 0
        for (let i = 4; i < 8; i++) {
          const slider = document.getElementById(`speed${i}`);
          const valueSpan = document.getElementById(`speed${i}-value`);
          if (slider && valueSpan) {
            slider.value = 0;
            valueSpan.textContent = '0';
          }
        }
      } else {
        showStatus(`Stop failed: response code 0x${respCode.toString(16)}`, true);
        updateLastResponse(`STOP_SERVOS: 0x${respCode.toString(16)}`);
      }
    } else {
      showStatus('Invalid response from bot', true);
      updateLastResponse('Invalid response');
    }
    
  } catch (error) {
    console.error('Stop servos failed:', error);
    showStatus('Failed to stop servos: ' + error.message, true);
    updateLastResponse('Error: ' + error.message);
  }
}
// ── Heartbeat Management ──────────────────────────────────────────────────────

/**
 * Start sending heartbeat packets every 40ms.
 * Uses fire-and-forget (WS-like): no response is awaited.
 * DENIED responses are detected asynchronously in processWSResponse().
 */
function startHeartbeat() {
  if (heartbeatInterval) {
    return; // Already running
  }

  heartbeatInterval = setInterval(() => {
    if (!wsConnected) {
      return;
    }
    const packet = new Uint8Array([WS_ACTION.HEARTBEAT]);
    heartbeatSendTime = performance.now();
    sendWSFireAndForget(packet);
  }, HEARTBEAT_INTERVAL_MS);

  updateUIState();
}

/**
 * Stop sending heartbeat packets
 */
function stopHeartbeat() {
  if (heartbeatInterval) {
    clearInterval(heartbeatInterval);
    heartbeatInterval = null;
  }
  updateUIState();
}

// ── Ping and Latency Monitoring ───────────────────────────────────────────────

let pingCanvas = null;
let pingCtx = null;

/**
 * Initialize ping graph canvas
 */
function initPingGraph() {
  pingCanvas = document.getElementById('pingGraph');
  if (pingCanvas) {
    pingCtx = pingCanvas.getContext('2d');
    drawPingGraph();
  }
}

/**
 * Start ping monitoring
 */
function startPing() {
  if (pingInterval) {
    showStatus('Ping already running', true);
    return;
  }
  
  const botIp = gBotIp || document.getElementById('botIp').value.trim();
  if (!botIp) {
    showStatus('Please enter bot IP address', true);
    return;
  }
  
  showStatus('Starting ping monitor...', false);
  pingInterval = setInterval(sendPingPacket, 1000); // Ping every second
  sendPingPacket(); // Send first ping immediately
}

/**
 * Stop ping monitoring
 */
function stopPing() {
  if (pingInterval) {
    clearInterval(pingInterval);
    pingInterval = null;
    showStatus('Ping monitoring stopped', false);
  }
}

/**
 * Send a single ping packet
 */
async function sendPingPacket() {
  try {
    const startTime = performance.now();
    const id = pingSequence++;
    
    // Build PING packet: 0x44 + 4-byte ID (uint32 LE)
    const packet = new Uint8Array(5);
    packet[0] = WS_ACTION.PING;
    packet[1] = id & 0xFF;
    packet[2] = (id >> 8) & 0xFF;
    packet[3] = (id >> 16) & 0xFF;
    packet[4] = (id >> 24) & 0xFF;
    
    // Send WS packet
    const response = await sendWSPacket(packet);
    const endTime = performance.now();
    const latency = Math.round(endTime - startTime);
    
    // Verify response (should be 5-byte echo)
    if (response && response.length === 5 && response[0] === WS_ACTION.PING) {
      recordPingResult(latency);
    } else {
      console.warn('Invalid ping response');
    }
    
  } catch (error) {
    console.error('Ping failed:', error);
  }
}

/**
 * Record ping result and update graph
 */
function recordPingResult(latency) {
  // Add to history
  pingHistory.push(latency);
  if (pingHistory.length > MAX_PING_HISTORY) {
    pingHistory.shift();
  }
  
  // Calculate statistics
  const current = latency;
  const avg = Math.round(pingHistory.reduce((a, b) => a + b, 0) / pingHistory.length);
  const min = Math.min(...pingHistory);
  const max = Math.max(...pingHistory);
  
  // Update stats display
  document.getElementById('pingCurrent').textContent = current;
  document.getElementById('pingAvg').textContent = avg;
  document.getElementById('pingMin').textContent = min;
  document.getElementById('pingMax').textContent = max;
  
  // Update graph
  drawPingGraph();
}

/**
 * Draw the ping graph
 */
function drawPingGraph() {
  if (!pingCtx || !pingCanvas) return;
  
  const width = pingCanvas.width;
  const height = pingCanvas.height;
  const padding = 10;
  const graphWidth = width - padding * 2;
  const graphHeight = height - padding * 2;
  
  // Clear canvas
  pingCtx.fillStyle = '#0a0a0a';
  pingCtx.fillRect(0, 0, width, height);
  
  if (pingHistory.length === 0) {
    // Draw "No Data" message
    pingCtx.fillStyle = '#666';
    pingCtx.font = '14px Arial';
    pingCtx.textAlign = 'center';
    pingCtx.fillText('No ping data yet', width / 2, height / 2);
    return;
  }
  
  // Calculate scale
  const maxLatency = Math.max(...pingHistory, 50); // Minimum scale of 50ms
  const scale = graphHeight / maxLatency;
  
  // Draw grid lines
  pingCtx.strokeStyle = '#222';
  pingCtx.lineWidth = 1;
  for (let i = 0; i <= 4; i++) {
    const y = padding + (graphHeight / 4) * i;
    pingCtx.beginPath();
    pingCtx.moveTo(padding, y);
    pingCtx.lineTo(width - padding, y);
    pingCtx.stroke();
    
    // Draw scale labels
    const value = Math.round(maxLatency * (1 - i / 4));
    pingCtx.fillStyle = '#666';
    pingCtx.font = '10px Arial';
    pingCtx.textAlign = 'right';
    pingCtx.fillText(value + 'ms', padding - 5, y + 3);
  }
  
  // Draw ping line
  pingCtx.strokeStyle = '#4CAF50';
  pingCtx.lineWidth = 2;
  pingCtx.beginPath();
  
  const pointSpacing = graphWidth / (MAX_PING_HISTORY - 1);
  const startIndex = Math.max(0, pingHistory.length - MAX_PING_HISTORY);
  
  for (let i = 0; i < pingHistory.length; i++) {
    const x = padding + pointSpacing * i;
    const y = padding + graphHeight - (pingHistory[i] * scale);
    
    if (i === 0) {
      pingCtx.moveTo(x, y);
    } else {
      pingCtx.lineTo(x, y);
    }
  }
  
  pingCtx.stroke();
  
  // Draw points
  pingCtx.fillStyle = '#4CAF50';
  for (let i = 0; i < pingHistory.length; i++) {
    const x = padding + pointSpacing * i;
    const y = padding + graphHeight - (pingHistory[i] * scale);
    
    pingCtx.beginPath();
    pingCtx.arc(x, y, 2, 0, 2 * Math.PI);
    pingCtx.fill();
  }
}

/**
 * Clear ping graph and history
 */
function clearPingGraph() {
  pingHistory = [];
  pingSequence = 0;
  document.getElementById('pingCurrent').textContent = '—';
  document.getElementById('pingAvg').textContent = '—';
  document.getElementById('pingMin').textContent = '—';
  document.getElementById('pingMax').textContent = '—';
  drawPingGraph();
  showStatus('Ping history cleared', false);
}

// ── WS over WebSocket (fire-and-forget + request-response) ──────────────────

/**
 * Send WS packet to bot via WebSocket bridge
 * 
 * @param {Uint8Array} packet - WS packet data
 * @returns {Promise<Uint8Array>} Response packet (received via WebSocket)
 */
async function sendWSPacket(packet) {
  const packetCopy = new Uint8Array(packet);

  udpSendChain = udpSendChain
    .catch(() => undefined)
    .then(() => sendWSPacketInternal(packetCopy));

  return udpSendChain;
}

let udpSendChain = Promise.resolve();

/**
 * Process a WS response received over the WebSocket bridge.
 *
 * Fire-and-forget senders (heartbeat, servo commands from the joystick)
 * never set a pendingResponse, so their replies are handled here as
 * asynchronous notifications.  Request-response senders (register,
 * unregister, ping) set pendingResponse with an expectedAction byte;
 * only a matching reply resolves that promise.
 */
function processWSResponse(response) {
  // ── Async notification handling (fire-and-forget responses) ──────────
  if (response.length >= 2) {
    const action = response[0];
    const statusByte = response[1];

    // Heartbeat response → measure round-trip time
    if (action === WS_ACTION.HEARTBEAT) {
      if (statusByte === BOT_RESP.NOT_MASTER) {
        console.warn('[WS] Heartbeat denied - master registration lost');
        isMasterRegistered = false;
        stopHeartbeat();
        updateUIState();
        showStatus('Master registration lost - heartbeat denied', true);
      } else if (heartbeatSendTime > 0) {
        const rtt = Math.round(performance.now() - heartbeatSendTime);
        heartbeatRTTHistory.push(rtt);
        if (heartbeatRTTHistory.length > MAX_HEARTBEAT_RTT_HISTORY) {
          heartbeatRTTHistory.shift();
        }
        const avg = Math.round(
          heartbeatRTTHistory.reduce((a, b) => a + b, 0) / heartbeatRTTHistory.length
        );
        const el = document.getElementById('heartbeatResponseTime');
        if (el) {
          el.textContent = `${avg} ms`;
        }
      }
    }
  }

  // ── Request-response matching ───────────────────────────────────────
  if (!pendingResponse) {
    return; // Fire-and-forget reply, already handled above
  }

  // Only resolve when the action byte matches the expected one so that a
  // stray fire-and-forget reply cannot steal a pending registration or ping.
  if (response.length > 0 && response[0] === pendingResponse.expectedAction) {
    clearTimeout(pendingResponse.timeoutId);
    pendingResponse.resolve(response);
    pendingResponse = null;
  }
}

/**
 * Send one WS packet while guaranteeing a single in-flight request.
 *
 * @param {Uint8Array} packet - WS packet data.
 * @returns {Promise<Uint8Array>} Response packet.
 */
async function sendWSPacketInternal(packet) {
  const botIp = gBotIp || document.getElementById('botIp').value.trim();
  const port = gBotPort || parseInt(document.getElementById('botPort').value, 10) || WS_PORT;

  const hexData = Array.from(packet)
    .map(b => b.toString(16).padStart(2, '0'))
    .join('');

  console.log(`[WS TX] ${hexData} via WebSocket to ${botIp}:${port}`);

  if (!wsConnected || !ws || ws.readyState !== WebSocket.OPEN) {
    await initializeWebSocket();
  }

  if (!wsConnected || !ws || ws.readyState !== WebSocket.OPEN) {
    console.error('[WS] WebSocket not connected');
    throw new Error('WebSocket bridge not connected');
  }

  return new Promise((resolve, reject) => {
    const timeoutId = setTimeout(() => {
      pendingResponse = null;
      reject(new Error('WS response timeout (no reply within ' + WS_TIMEOUT + 'ms)'));
    }, WS_TIMEOUT);

    pendingResponse = { resolve, reject, timeoutId, expectedAction: packet[0] };
    ws.send(packet.buffer.slice(packet.byteOffset, packet.byteOffset + packet.byteLength));
  });
}

/**
 * Send a WS-like fire-and-forget message over the WebSocket bridge.
 * Mimics real WS semantics: no response is awaited, the message is
 * silently dropped when the socket is not open.
 *
 * Use this for latency-sensitive controls (joystick servo commands,
 * heartbeat) where waiting for a reply would stall the next send.
 *
 * @param {Uint8Array} packet - Binary payload to send.
 */
function sendWSFireAndForget(packet) {
  if (!wsConnected || !ws || ws.readyState !== WebSocket.OPEN) {
    return; // Silently discard, just like a real WS send on a down link
  }
  ws.send(packet.buffer.slice(packet.byteOffset, packet.byteOffset + packet.byteLength));
}

// ── UI Updates ────────────────────────────────────────────────────────────────

/**
 * Update UI elements based on current state
 */
function updateUIState() {
  // Connection status
  const botIp = gBotIp || document.getElementById('botIp').value.trim();
  const connStatus = document.getElementById('connectionStatus');
  if (wsConnected) {
    connStatus.textContent = 'Connected';
    connStatus.className = 'status-badge status-connected';
  } else if (wsConnectPromise) {
    connStatus.textContent = 'Connecting';
    connStatus.className = 'status-badge status-warning';
  } else if (botIp) {
    connStatus.textContent = 'Configured';
    connStatus.className = 'status-badge status-warning';
  } else {
    connStatus.textContent = 'Not Connected';
    connStatus.className = 'status-badge status-disconnected';
  }
  
  // Master registration status
  const masterStatus = document.getElementById('masterRegistered');
  if (isMasterRegistered) {
    masterStatus.textContent = 'Yes';
    masterStatus.className = 'status-badge status-connected';
    setTitleStatus('[MASTER]', '#4CAF50');
  } else {
    masterStatus.textContent = 'No';
    masterStatus.className = 'status-badge status-disconnected';
    setTitleStatus('', '#999');
  }
  
  // Heartbeat status
  const heartbeatStatus = document.getElementById('heartbeatStatus');
  if (heartbeatInterval) {
    heartbeatStatus.textContent = 'Running';
    heartbeatStatus.className = 'status-badge status-connected';
  } else {
    heartbeatStatus.textContent = 'Stopped';
    heartbeatStatus.className = 'status-badge status-disconnected';
  }
}

/**
 * Update last response display
 */
function updateLastResponse(text) {
  const elem = document.getElementById('lastResponse');
  if (elem) {
    elem.textContent = text;
    elem.style.fontFamily = "'Courier New', monospace";
    elem.style.fontSize = '12px';
  }
}

/**
 * Toggle a panel section while collapsing another (mutually exclusive)
 * @param {string} thisSectionId - id of the section to toggle
 * @param {string} thisBtnId - id of the button for this section
 * @param {string} otherSectionId - id of the other section to collapse
 * @param {string} otherBtnId - id of the button for the other section
 */
function togglePanelExclusive(thisSectionId, thisBtnId, otherSectionId, otherBtnId) {
  const thisBody = document.getElementById(thisSectionId);
  const thisBtn = document.getElementById(thisBtnId);
  const otherBody = document.getElementById(otherSectionId);
  const otherBtn = document.getElementById(otherBtnId);
  
  // If this panel is currently collapsed, expand it and collapse the other
  const isThisCollapsed = thisBody.classList.contains('collapsed');
  
  if (isThisCollapsed) {
    // Expand this panel
    thisBody.classList.remove('collapsed');
    thisBtn.classList.remove('collapsed');
    
    // Collapse the other panel
    if (otherBody && otherBtn) {
      otherBody.classList.add('collapsed');
      otherBtn.classList.add('collapsed');
    }
  } else {
    // Just collapse this panel (don't force expand the other)
    thisBody.classList.add('collapsed');
    thisBtn.classList.add('collapsed');
  }
}
