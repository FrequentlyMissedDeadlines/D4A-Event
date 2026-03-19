
const MASTER_BASE = '/api/amakerbot/v1';
function setFeedback(msg, ok) {
  const el = document.getElementById('master-feedback');
  el.textContent = msg;
  el.style.color = ok ? '#388e3c' : '#c62828';
}
function renderMasterStatus(data) {
  const box = document.getElementById('master-status-box');
  if (data && data.registered) {
    box.style.background = '#e8f5e9';
    box.style.color = '#2e7d32';
    box.innerHTML = '✅ <strong>Master registered</strong> — IP: <code>' + data.ip +
                    '</code>&nbsp;&nbsp;Token: <code>' + (data.token || '—') + '</code>';
  } else {
    box.style.background = '#fff3e0';
    box.style.color = '#e65100';
    box.innerHTML = '⚠️ <strong>No master registered</strong>';
  }
}
function refreshMasterStatus() {
  fetch(MASTER_BASE + '/master')
    .then(r => r.json())
    .then(renderMasterStatus)
    .catch(() => {
      const box = document.getElementById('master-status-box');
      box.style.background = '#ffebee';
      box.style.color = '#b71c1c';
      box.textContent = '❌ Could not reach AmakerBot service';
    });
}
function doRegister() {
  const token = document.getElementById('master-token-input').value.trim();
  if (!token) { setFeedback('Please enter a token.', false); return; }
  fetch(MASTER_BASE + '/register?token=' + encodeURIComponent(token), { method: 'POST' })
    .then(r => r.json())
    .then(data => {
      if (data.result === 'ok') {
        setFeedback('Registered successfully (token logged on device).', true);
        refreshMasterStatus();
      } else {
        setFeedback('Error: ' + (data.message || JSON.stringify(data)), false);
      }
    })
    .catch(e => setFeedback('Request failed: ' + e, false));
}
function doUnregister() {
  const token = document.getElementById('master-token-input').value.trim();
  const url = token
    ? MASTER_BASE + '/unregister?token=' + encodeURIComponent(token)
    : MASTER_BASE + '/unregister';
  fetch(url, { method: 'POST' })
    .then(r => r.json())
    .then(data => {
      if (data.result === 'ok') {
        setFeedback('Master unregistered.', true);
        refreshMasterStatus();
      } else {
        setFeedback('Error: ' + (data.message || JSON.stringify(data)), false);
      }
    })
    .catch(e => setFeedback('Request failed: ' + e, false));
}
refreshMasterStatus();
setInterval(refreshMasterStatus, 5000);
const DISPLAY_BASE = '/api/amakerbot/v1/display';
const DISPLAY_MODE_LABELS = { APP_UI: '🖥 App UI', APP_LOG: '📋 App Log', DEBUG_LOG: '🐛 Debug Log', ESP_LOG: '⚙️ ESP Log' };
function renderDisplayStatus(data) {
  const box = document.getElementById('display-status-box');
  const label = DISPLAY_MODE_LABELS[data.mode] || data.mode;
  box.style.background = '#e3f2fd';
  box.style.color = '#0d47a1';
  box.innerHTML = 'Current mode: <strong>' + label + '</strong>';
}
function setDisplayFeedback(msg, ok) {
  const el = document.getElementById('display-feedback');
  el.textContent = msg;
  el.style.color = ok ? '#388e3c' : '#c62828';
}
function refreshDisplayStatus() {
  fetch(DISPLAY_BASE)
    .then(r => r.json())
    .then(renderDisplayStatus)
    .catch(() => {
      const box = document.getElementById('display-status-box');
      box.style.background = '#ffebee';
      box.style.color = '#b71c1c';
      box.textContent = '❌ Could not reach display endpoint';
    });
}
function setDisplayMode(mode) {
  fetch(DISPLAY_BASE + '?mode=' + mode, { method: 'POST' })
    .then(r => r.json())
    .then(data => {
      if (data.result === 'ok' || data.mode) {
        setDisplayFeedback('Mode set to ' + (DISPLAY_MODE_LABELS[data.mode] || data.mode), true);
        renderDisplayStatus(data);
      } else {
        setDisplayFeedback('Error: ' + (data.message || JSON.stringify(data)), false);
      }
    })
    .catch(e => setDisplayFeedback('Request failed: ' + e, false));
}
function cycleDisplayMode() {
  fetch(DISPLAY_BASE + '/next', { method: 'POST' })
    .then(r => r.json())
    .then(data => {
      setDisplayFeedback('Cycled to ' + (DISPLAY_MODE_LABELS[data.mode] || data.mode), true);
      renderDisplayStatus(data);
    })
    .catch(e => setDisplayFeedback('Request failed: ' + e, false));
}
refreshDisplayStatus();
