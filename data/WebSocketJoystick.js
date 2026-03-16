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

// UDP Response Status codes (AmakerBot specific)
const UDP_STATUS = {
  SUCCESS: 0x01,
  IGNORED: 0x02,
  DENIED: 0x03,
  ERROR: 0x04
};

// UDP Action codes
const UDP_ACTION = {
  MASTER_REGISTER: 0x41,
  MASTER_UNREGISTER: 0x42,
  HEARTBEAT: 0x43,
  PING: 0x44
};

// ── Page Initialization ───────────────────────────────────────────────────────

document.addEventListener('DOMContentLoaded', () => {
  // Auto-populate bot IP from current location
  const currentHost = window.location.hostname;
  document.getElementById('botIp').value = currentHost;
  
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
  showStatus('UDP demo loaded. Enter master token to register.', false);
  
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
 * Reject the currently pending UDP response, if any.
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

  
  const botIp = document.getElementById('botIp').value.trim() ;
  const port = document.getElementById('botPort').value.trim();
  // Only add port if it's explicitly specified (not default)
  const portStr = port ? `:${port}` : ':80';
  const wsUrl = `ws://${botIp}${portStr}/ws`;
  wsManualClose = false;
  console.log('[UDP] initializeWebSocket '+wsUrl);
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
        processUDPResponse(response);
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
 * Create UDP socket if not already created
 * Now we use the WebSocket bridge instead
 */
function ensureUDPSocket() {
  const botIp = document.getElementById('botIp').value.trim();
  
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

    // Build UDP packet: 0x41 + token bytes
    const packet = new Uint8Array(1 + token.length);
    packet[0] = UDP_ACTION.MASTER_REGISTER;
    for (let i = 0; i < token.length; i++) {
      packet[i + 1] = token.charCodeAt(i);
    }
    
    // Send UDP packet (simulated via HTTP proxy)
    const response = await sendUDPPacket(packet);
    
    // Parse response: should be echo + status byte
    if (response && response.length >= packet.length + 1) {
      const statusByte = response[response.length - 1];
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
  const statusNames = {
    [UDP_STATUS.SUCCESS]: 'SUCCESS',
    [UDP_STATUS.IGNORED]: 'IGNORED',
    [UDP_STATUS.DENIED]: 'DENIED',
    [UDP_STATUS.ERROR]: 'ERROR'
  };
  
  const statusName = statusNames[statusByte] || `UNKNOWN(0x${statusByte.toString(16)})`;
  updateLastResponse(`MASTER_REGISTER: ${statusName}`);
  
  if (statusByte === UDP_STATUS.SUCCESS) {
    isMasterRegistered = true;
    showStatus(`✓ Registered as master with token ${token}`, false);
    startHeartbeat();
    updateUIState();
  } else if (statusByte === UDP_STATUS.IGNORED) {
    showStatus('Registration ignored (master already registered)', true);
  } else if (statusByte === UDP_STATUS.DENIED) {
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
    
    // Build UDP packet: 0x42
    const packet = new Uint8Array([UDP_ACTION.MASTER_UNREGISTER]);
    
    // Send UDP packet
    const response = await sendUDPPacket(packet);
    
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
  const statusNames = {
    [UDP_STATUS.SUCCESS]: 'SUCCESS',
    [UDP_STATUS.DENIED]: 'DENIED',
    [UDP_STATUS.ERROR]: 'ERROR'
  };
  
  const statusName = statusNames[statusByte] || `UNKNOWN(0x${statusByte.toString(16)})`;
  updateLastResponse(`MASTER_UNREGISTER: ${statusName}`);
  
  if (statusByte === UDP_STATUS.SUCCESS) {
    isMasterRegistered = false;
    showStatus('✓ Successfully unregistered as master', false);
    stopHeartbeat();
    closeWebSocket();
    updateUIState();
  } else if (statusByte === UDP_STATUS.DENIED) {
    showStatus('Unregistration denied (not the registered master)', true);
  } else {
    showStatus(`Unregistration failed: ${statusName}`, true);
  }
}

// ── Bot Name Management ───────────────────────────────────────────────────────

/**
 * Set the bot name via UDP
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
    
    // Build UDP packet: "AMAKERBOT:setname:<name>"
    const message = `AMAKERBOT:setname:${botName}`;
    const packet = new TextEncoder().encode(message);
    
    // Send UDP packet
    const response = await sendUDPPacket(packet);
    
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
    
    // Build UDP packet: 0x24 [mask] [type]
    const packet = new Uint8Array([0x24, mask, type]);
    
    // Send UDP packet
    const response = await sendUDPPacket(packet);
    
    // Parse response: 0x24 + resp_code
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
    
    // Build UDP packet: 0x24 [mask] [type=0]
    const packet = new Uint8Array([0x24, mask, 0]);
    
    // Send UDP packet
    const response = await sendUDPPacket(packet);
    
    // Parse response: 0x24 + resp_code
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
  const typeNames = ['Not Connected', 'Rotational', 'Angular 180°', 'Angular 270°'];
  const typeName = typeNames[type] || `Type ${type}`;
  
  // Binary response codes
  const respNames = {
    0x00: 'OK',
    0x01: 'INVALID_PARAMS',
    0x02: 'INVALID_VALUES',
    0x03: 'OPERATION_FAILED',
    0x04: 'NOT_STARTED',
    0x05: 'UNKNOWN_CMD',
    0x06: 'NOT_MASTER'
  };
  
  const respName = respNames[respCode] || `0x${respCode.toString(16)}`;
  updateLastResponse(`ATTACH_SERVO (mask=0x${mask.toString(16)}, type=${type}): ${respName}`);
  
  if (respCode === 0x00) {
    const channels = [];
    for (let i = 0; i < 8; i++) {
      if (mask & (1 << i)) channels.push(i);
    }
    const action = type === 0 ? 'detached' : `attached as ${typeName}`;
    showStatus(`✓ Servo channels ${channels.join(', ')} ${action}`, false);
  } else if (respCode === 0x06) {
    showStatus('Not authorized - not registered as master', true);
  } else {
    showStatus(`Failed: ${respName}`, true);
  }
}
// ── Servo Control ─────────────────────────────────────────────────────────────

/**
 * Set servo angles for angular servos (0x21)
 */
async function setServoAngles() {
  if (!isMasterRegistered) {
    showStatus('Must be registered as master to control servos', true);
    return;
  }
  
  try {
    showStatus('Setting servo angles...', false);
    
    // Build packet: 0x21 + up to 8 int16 values
    const angles = [];
    let maxChannel = -1;
    
    // Check which channels are enabled (0-3 for angles)
    for (let i = 0; i < 4; i++) {
      const enabled = document.getElementById(`angle${i}-enable`).checked;
      if (enabled) {
        maxChannel = i;
      }
    }
    
    if (maxChannel < 0) {
      showStatus('Please enable at least one channel', true);
      return;
    }
    
    // Build angle array up to maxChannel
    for (let i = 0; i <= maxChannel; i++) {
      const enabled = document.getElementById(`angle${i}-enable`).checked;
      const angle = parseInt(document.getElementById(`angle${i}`).value);
      
      let raw;
      if (enabled) {
        // Encode: (angle << 1) | 1
        raw = (angle << 1) | 1;
      } else {
        // Skip this channel: 0x0000
        raw = 0;
      }
      
      // Convert to int16 LE bytes
      angles.push(raw & 0xFF);        // low byte
      angles.push((raw >> 8) & 0xFF); // high byte
    }
    
    const packet = new Uint8Array([0x21, ...angles]);
    
    // Send UDP packet
    const response = await sendUDPPacket(packet);
    
    // Parse response: 0x21 + resp_code
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
  const respNames = {
    0x00: 'OK',
    0x01: 'INVALID_PARAMS',
    0x02: 'INVALID_VALUES',
    0x03: 'OPERATION_FAILED',
    0x06: 'NOT_MASTER'
  };
  
  const respName = respNames[respCode] || `0x${respCode.toString(16)}`;
  updateLastResponse(`SET_SERVO_ANGLE: ${respName}`);
  
  if (respCode === 0x00) {
    showStatus('✓ Servo angles set', false);
  } else if (respCode === 0x03) {
    showStatus('Operation failed (check servo types and angle ranges)', true);
  } else if (respCode === 0x06) {
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
    
    // Build packet: 0x22 [mask] [8 speed bytes]
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
    
    const packet = new Uint8Array([0x22, mask, ...speeds]);
    
    // Send UDP packet
    const response = await sendUDPPacket(packet);
    
    // Parse response: 0x22 + resp_code
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
  const respNames = {
    0x00: 'OK',
    0x01: 'INVALID_PARAMS',
    0x02: 'INVALID_VALUES',
    0x03: 'OPERATION_FAILED',
    0x06: 'NOT_MASTER'
  };
  
  const respName = respNames[respCode] || `0x${respCode.toString(16)}`;
  updateLastResponse(`SET_SERVO_SPEED (mask=0x${mask.toString(16)}): ${respName}`);
  
  if (respCode === 0x00) {
    showStatus('✓ Servo speeds set', false);
  } else if (respCode === 0x03) {
    showStatus('Operation failed (check servo types are rotational)', true);
  } else if (respCode === 0x06) {
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
    
    // Build packet: 0x23 [mask=0xFF]
    const packet = new Uint8Array([0x23, 0xFF]);
    
    // Send UDP packet
    const response = await sendUDPPacket(packet);
    
    // Parse response: 0x23 + resp_code
    if (response && response.length >= 2) {
      const respCode = response[1];
      if (respCode === 0x00) {
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
 * Uses fire-and-forget (UDP-like): no response is awaited.
 * DENIED responses are detected asynchronously in processUDPResponse().
 */
function startHeartbeat() {
  if (heartbeatInterval) {
    return; // Already running
  }

  heartbeatInterval = setInterval(() => {
    if (!wsConnected) {
      return;
    }
    const packet = new Uint8Array([UDP_ACTION.HEARTBEAT]);
    heartbeatSendTime = performance.now();
    sendUDPFireAndForget(packet);
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
  
  const botIp = document.getElementById('botIp').value.trim();
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
    packet[0] = UDP_ACTION.PING;
    packet[1] = id & 0xFF;
    packet[2] = (id >> 8) & 0xFF;
    packet[3] = (id >> 16) & 0xFF;
    packet[4] = (id >> 24) & 0xFF;
    
    // Send UDP packet
    const response = await sendUDPPacket(packet);
    const endTime = performance.now();
    const latency = Math.round(endTime - startTime);
    
    // Verify response (should be 5-byte echo)
    if (response && response.length === 5 && response[0] === UDP_ACTION.PING) {
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

// ── UDP over WebSocket (fire-and-forget + request-response) ──────────────────

/**
 * Send UDP packet to bot via WebSocket bridge
 * 
 * @param {Uint8Array} packet - UDP packet data
 * @returns {Promise<Uint8Array>} Response packet (received via WebSocket)
 */
async function sendUDPPacket(packet) {
  const packetCopy = new Uint8Array(packet);

  udpSendChain = udpSendChain
    .catch(() => undefined)
    .then(() => sendUDPPacketInternal(packetCopy));

  return udpSendChain;
}

let udpSendChain = Promise.resolve();

/**
 * Process a UDP response received over the WebSocket bridge.
 *
 * Fire-and-forget senders (heartbeat, servo commands from the joystick)
 * never set a pendingResponse, so their replies are handled here as
 * asynchronous notifications.  Request-response senders (register,
 * unregister, ping) set pendingResponse with an expectedAction byte;
 * only a matching reply resolves that promise.
 */
function processUDPResponse(response) {
  // ── Async notification handling (fire-and-forget responses) ──────────
  if (response.length >= 2) {
    const action = response[0];
    const statusByte = response[1];

    // Heartbeat response → measure round-trip time
    if (action === UDP_ACTION.HEARTBEAT) {
      if (statusByte === UDP_STATUS.DENIED) {
        console.warn('[UDP] Heartbeat denied - master registration lost');
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
 * Send one UDP packet while guaranteeing a single in-flight request.
 *
 * @param {Uint8Array} packet - UDP packet data.
 * @returns {Promise<Uint8Array>} Response packet.
 */
async function sendUDPPacketInternal(packet) {
  const botIp = document.getElementById('botIp').value.trim();
  const port = parseInt(document.getElementById('botPort').value, 10) || WS_PORT;

  const hexData = Array.from(packet)
    .map(b => b.toString(16).padStart(2, '0'))
    .join('');

  console.log(`[UDP TX] ${hexData} via WebSocket to ${botIp}:${port}`);

  if (!wsConnected || !ws || ws.readyState !== WebSocket.OPEN) {
    await initializeWebSocket();
  }

  if (!wsConnected || !ws || ws.readyState !== WebSocket.OPEN) {
    console.error('[UDP] WebSocket not connected');
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
 * Send a UDP-like fire-and-forget message over the WebSocket bridge.
 * Mimics real UDP semantics: no response is awaited, the message is
 * silently dropped when the socket is not open.
 *
 * Use this for latency-sensitive controls (joystick servo commands,
 * heartbeat) where waiting for a reply would stall the next send.
 *
 * @param {Uint8Array} packet - Binary payload to send.
 */
function sendUDPFireAndForget(packet) {
  if (!wsConnected || !ws || ws.readyState !== WebSocket.OPEN) {
    return; // Silently discard, just like a real UDP send on a down link
  }
  ws.send(packet.buffer.slice(packet.byteOffset, packet.byteOffset + packet.byteLength));
}

// ── UI Updates ────────────────────────────────────────────────────────────────

/**
 * Update UI elements based on current state
 */
function updateUIState() {
  // Connection status
  const botIp = document.getElementById('botIp').value.trim();
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
