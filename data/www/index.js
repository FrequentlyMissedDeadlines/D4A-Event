'use strict';

// ── Bot protocol constants (service 0x04, AmakerBot) ─────────────────────────
// Action byte = (service_id << 4) | cmd
const IDX_ACTION = {
  REGISTER:    0x41,   // CMD_REGISTER   (0x04 << 4 | 0x01)
  UNREGISTER:  0x42,   // CMD_UNREGISTER (0x04 << 4 | 0x02)
  PING:        0x44,   // CMD_PING       (0x04 << 4 | 0x04)
  GET_WIFI:    0x47,   // CMD_GET_WIFI   (0x04 << 4 | 0x07)
  SET_WIFI:    0x48,   // CMD_SET_WIFI   (0x04 << 4 | 0x08)
  RESET_WIFI:  0x49    // CMD_RESET_WIFI (0x04 << 4 | 0x09)
};

// Status codes — must match BotProto constants in BotMessageHandler.h
const IDX_STATUS = {
  OK:               0x00, // BotProto::resp_ok
  INVALID_PARAMS:   0x01, // BotProto::resp_invalid_params
  INVALID_VALUES:   0x02, // BotProto::resp_invalid_values
  OPERATION_FAILED: 0x03, // BotProto::resp_operation_failed (another master active)
  NOT_STARTED:      0x04, // BotProto::resp_not_started
  UNKNOWN_SERVICE:  0x05, // BotProto::resp_unknown_service
  UNKNOWN_CMD:      0x06, // BotProto::resp_unknown_cmd
  NOT_MASTER:       0x07, // BotProto::resp_not_master (invalid token / caller not master)
};

const WS_TIMEOUT_MS = 2000;

// ── Module state ─────────────────────────────────────────────────────────────
let _ws              = null;
let _wsConnected     = false;
let _wsConnectPromise = null;
let _pendingResponse = null;
let _registered      = false;

// ── Feedback helpers ──────────────────────────────────────────────────────────

/**
 * Show a feedback message in the #master-feedback element.
 * @param {string}  msg
 * @param {boolean} ok  - true → green, false → red
 */
function setFeedback(msg, ok) {
  const el = document.getElementById('master-feedback');
  if (!el) return;
  el.textContent = msg;
  el.style.color = ok ? '#388e3c' : '#c62828';
}

/**
 * Show a feedback message in the #settings-feedback element.
 * @param {string}  msg
 * @param {boolean} ok  - true → green, false → red
 */
function setSettingsFeedback(msg, ok) {
  const el = document.getElementById('settings-feedback');
  if (!el) return;
  el.textContent = msg;
  el.style.color = ok ? '#388e3c' : '#c62828';
}

/**
 * Update the master-status box to reflect current registration state.
 * @param {'registered'|'unregistered'|'error'|'connecting'} state
 * @param {string} [detail]  Optional extra text (e.g. error message)
 */
function renderMasterStatus(state, detail) {
  const box = document.getElementById('master-status-box');
  if (!box) return;

  switch (state) {
    case 'registered':
      box.style.background = '#e8f5e9';
      box.style.color      = '#2e7d32';
      box.innerHTML        = '✅ <strong>Registered as master</strong>';
      break;
    case 'unregistered':
      box.style.background = '#fff3e0';
      box.style.color      = '#e65100';
      box.innerHTML        = '⚠️ <strong>Not registered</strong>';
      break;
    case 'connecting':
      box.style.background = '#f3f3f3';
      box.style.color      = '#555';
      box.textContent      = '⏳ Connecting…';
      break;
    case 'error':
    default:
      box.style.background = '#ffebee';
      box.style.color      = '#b71c1c';
      box.textContent      = '❌ ' + (detail || 'WebSocket error');
      break;
  }
}

// ── WebSocket management ─────────────────────────────────────────────────────

/**
 * Open (or reuse) the WebSocket connection to /ws on the current host.
 * @returns {Promise<WebSocket>}
 */
function ensureWebSocket() {
  if (_wsConnected && _ws && _ws.readyState === WebSocket.OPEN) {
    return Promise.resolve(_ws);
  }
  if (_wsConnectPromise) {
    return _wsConnectPromise;
  }

  const wsUrl = 'ws://' + window.location.hostname + ':' + (window.location.port || '81') + '/ws';

  _wsConnectPromise = new Promise((resolve, reject) => {
    const socket = new WebSocket(wsUrl);
    _ws = socket;
    socket.binaryType = 'arraybuffer';

    const connectionTimeout = setTimeout(() => {
      if (socket.readyState === WebSocket.CONNECTING) socket.close();
    }, 5000);

    socket.onopen = () => {
      clearTimeout(connectionTimeout);
      _wsConnected      = true;
      _wsConnectPromise = null;
      resolve(socket);
    };

    socket.onmessage = (event) => {
      const data = new Uint8Array(event.data);
      if (_pendingResponse && data.length > 0 && data[0] === _pendingResponse.expectedAction) {
        clearTimeout(_pendingResponse.timeoutId);
        _pendingResponse.resolve(data);
        _pendingResponse = null;
      }
    };

    socket.onerror = () => {
      clearTimeout(connectionTimeout);
      _wsConnected      = false;
      _wsConnectPromise = null;
      if (_pendingResponse) {
        clearTimeout(_pendingResponse.timeoutId);
        _pendingResponse.reject(new Error('WebSocket error'));
        _pendingResponse = null;
      }
      reject(new Error('WebSocket connection failed to ' + wsUrl));
    };

    socket.onclose = () => {
      clearTimeout(connectionTimeout);
      _wsConnected      = false;
      _wsConnectPromise = null;
      if (_pendingResponse) {
        clearTimeout(_pendingResponse.timeoutId);
        _pendingResponse.reject(new Error('WebSocket closed'));
        _pendingResponse = null;
      }
    };
  });

  return _wsConnectPromise;
}

/**
 * Send a binary packet over the WebSocket and wait for the matching reply.
 * @param {Uint8Array} packet
 * @returns {Promise<Uint8Array>}
 */
async function sendPacket(packet) {
  const socket = await ensureWebSocket();
  return new Promise((resolve, reject) => {
    const timeoutId = setTimeout(() => {
      _pendingResponse = null;
      reject(new Error('No reply within ' + WS_TIMEOUT_MS + ' ms'));
    }, WS_TIMEOUT_MS);

    _pendingResponse = { resolve, reject, timeoutId, expectedAction: packet[0] };
    socket.send(packet.buffer.slice(packet.byteOffset, packet.byteOffset + packet.byteLength));
  });
}

// ── Master registration ───────────────────────────────────────────────────────

/**
 * Register this browser as bot master using the given token.
 * Sends WS action 0x41 + token bytes over WebSocket.
 */
async function doRegister() {
  const token = document.getElementById('master-token-input').value.trim();
  if (!token) { setFeedback('Please enter a token.', false); return; }

  renderMasterStatus('connecting');
  setFeedback('Connecting…', true);

  try {
    const encoded = new TextEncoder().encode(token);
    const packet  = new Uint8Array(1 + encoded.length);
    packet[0]     = IDX_ACTION.REGISTER;
    packet.set(encoded, 1);

    const response   = await sendPacket(packet);
    const statusByte = response.length >= 2 ? response[1] : 0xFF;

    if (statusByte === IDX_STATUS.OK) {
      _registered = true;
      renderMasterStatus('registered');
      setFeedback('Registered successfully.', true);
    } else if (statusByte === IDX_STATUS.OPERATION_FAILED) {
      renderMasterStatus('unregistered');
      setFeedback('Ignored — another master is already registered.', false);
    } else if (statusByte === IDX_STATUS.NOT_MASTER) {
      renderMasterStatus('unregistered');
      setFeedback('Denied — invalid token.', false);
    } else {
      renderMasterStatus('error', 'status 0x' + statusByte.toString(16));
      setFeedback('Registration failed (0x' + statusByte.toString(16) + ').', false);
    }
  } catch (e) {
    renderMasterStatus('error', e.message);
    setFeedback('Request failed: ' + e.message, false);
  }
}

/**
 * Unregister this browser as bot master.
 * Sends WS action 0x42 over WebSocket.
 */
async function doUnregister() {
  renderMasterStatus('connecting');
  setFeedback('Unregistering…', true);

  try {
    const packet  = new Uint8Array([IDX_ACTION.UNREGISTER]);
    const response   = await sendPacket(packet);
    const statusByte = response.length >= 2 ? response[1] : 0xFF;

    if (statusByte === IDX_STATUS.OK) {
      _registered = false;
      renderMasterStatus('unregistered');
      setFeedback('Unregistered.', true);
    } else if (statusByte === IDX_STATUS.DENIED) {
      setFeedback('Denied — not the current master.', false);
    } else {
      renderMasterStatus('error', 'status 0x' + statusByte.toString(16));
      setFeedback('Unregistration failed (0x' + statusByte.toString(16) + ').', false);
    }
  } catch (e) {
    renderMasterStatus('error', e.message);
    setFeedback('Request failed: ' + e.message, false);
  }
}

// ── WiFi settings ─────────────────────────────────────────────────────────────

/**
 * Load WiFi credentials from the device via CMD_GET_WIFI (0x47).
 * No master token required — read is always allowed.
 * Response: [action][resp_ok][ssid_len:1B][ssid…][pass_len:1B][pass…]
 */
async function loadSettings() {
  setSettingsFeedback('Loading…', true);
  try {
    const packet   = new Uint8Array([IDX_ACTION.GET_WIFI]);
    const response = await sendPacket(packet);
    const status   = response.length >= 2 ? response[1] : 0xFF;
    if (status !== IDX_STATUS.OK) {
      setSettingsFeedback('Load failed (0x' + status.toString(16) + ').', false);
      return;
    }
    if (response.length < 3) { setSettingsFeedback('Malformed response.', false); return; }
    const ssidLen = response[2];
    if (response.length < 3 + ssidLen + 1) { setSettingsFeedback('Malformed response.', false); return; }
    const ssid    = new TextDecoder().decode(response.slice(3, 3 + ssidLen));
    const passLen = response[3 + ssidLen];
    const pass    = new TextDecoder().decode(response.slice(4 + ssidLen, 4 + ssidLen + passLen));
    document.getElementById('wifi-ssid-input').value     = ssid;
    document.getElementById('wifi-password-input').value = pass;
    setSettingsFeedback('Settings loaded.', true);
  } catch (e) {
    setSettingsFeedback('Load failed: ' + e.message, false);
  }
}

/**
 * Save WiFi credentials to the device via CMD_SET_WIFI (0x48).
 * Master registration required.
 * Payload: [action][ssid_len:1B][ssid…][pass_len:1B][pass…]
 */
async function saveSettings() {
  if (!_registered) { setSettingsFeedback('Register as master first.', false); return; }
  const ssid = document.getElementById('wifi-ssid-input').value.trim();
  const pass = document.getElementById('wifi-password-input').value;
  if (!ssid) { setSettingsFeedback('SSID cannot be empty.', false); return; }

  // Encode to UTF-8 bytes first — length validation must be on byte count,
  // not character count, because the 802.11 SSID limit is 32 *bytes* and
  // the C++ size() check is also byte-based.
  const enc       = new TextEncoder();
  const ssidBytes = enc.encode(ssid);
  const passBytes = enc.encode(pass);
  if (ssidBytes.length > 32) { setSettingsFeedback('SSID too long (max 32 bytes UTF-8).', false); return; }
  if (passBytes.length > 64) { setSettingsFeedback('Password too long (max 64 bytes UTF-8).', false); return; }

  setSettingsFeedback('Saving…', true);
  try {
    const packet    = new Uint8Array(1 + 1 + ssidBytes.length + 1 + passBytes.length);
    let   off       = 0;
    packet[off++]   = IDX_ACTION.SET_WIFI;
    packet[off++]   = ssidBytes.length;
    packet.set(ssidBytes, off); off += ssidBytes.length;
    packet[off++]   = passBytes.length;
    packet.set(passBytes, off);

    const response   = await sendPacket(packet);
    const statusByte = response.length >= 2 ? response[1] : 0xFF;
    if (statusByte === IDX_STATUS.OK) {
      setSettingsFeedback('Saved. Reconnect if SSID changed.', true);
    } else if (statusByte === IDX_STATUS.NOT_MASTER) {
      setSettingsFeedback('Denied — not the current master.', false);
    } else if (statusByte === IDX_STATUS.INVALID_VALUES) {
      setSettingsFeedback('Invalid values (SSID/password out of range).', false);
    } else {
      setSettingsFeedback('Save failed (0x' + statusByte.toString(16) + ').', false);
    }
  } catch (e) {
    setSettingsFeedback('Save failed: ' + e.message, false);
  }
}

/**
 * Reset WiFi settings to factory defaults via CMD_RESET_WIFI (0x49).
 * Master registration required. Reloads inputs after success.
 */
async function resetDefaultSettings() {
  if (!_registered) { setSettingsFeedback('Register as master first.', false); return; }
  if (!confirm('Reset WiFi settings to factory defaults?')) return;

  setSettingsFeedback('Resetting…', true);
  try {
    const packet     = new Uint8Array([IDX_ACTION.RESET_WIFI]);
    const response   = await sendPacket(packet);
    const statusByte = response.length >= 2 ? response[1] : 0xFF;
    if (statusByte === IDX_STATUS.OK) {
      setSettingsFeedback('Reset to defaults.', true);
      await loadSettings(); // refresh inputs from device
    } else if (statusByte === IDX_STATUS.NOT_MASTER) {
      setSettingsFeedback('Denied — not the current master.', false);
    } else {
      setSettingsFeedback('Reset failed (0x' + statusByte.toString(16) + ').', false);
    }
  } catch (e) {
    setSettingsFeedback('Reset failed: ' + e.message, false);
  }
}

// ── Init ──────────────────────────────────────────────────────────────────────

document.addEventListener('DOMContentLoaded', () => {
  renderMasterStatus('unregistered');
});
