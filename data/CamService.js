let currentMode = 'off';
let streamCheckInterval = null;
function setMode(mode) {
  if (streamCheckInterval) {
    clearInterval(streamCheckInterval);
    streamCheckInterval = null;
  }
  document.querySelectorAll('.mode-btn').forEach(btn => btn.classList.remove('active'));
  document.getElementById('btn' + mode.charAt(0).toUpperCase() + mode.slice(1)).classList.add('active');
  currentMode = mode;
  hideError();
  const image       = document.getElementById('cameraImage');
  const placeholder = document.getElementById('placeholder');
  const controls    = document.getElementById('cameraControls');
  if (mode === 'off') {
    image.src = '';
    image.classList.remove('visible');
    placeholder.style.display = 'block';
    placeholder.textContent = 'Camera is off. Select a mode to view the camera feed.';
    controls.style.display = 'none';
  } else if (mode === 'snapshot') {
    placeholder.textContent = 'Loading snapshot...';
    captureSnapshot();
    controls.style.display = 'flex';
  } else if (mode === 'stream') {
    placeholder.style.display = 'none';
    image.classList.add('visible');
    image.src = '/api/webcam/v1/stream?' + new Date().getTime();
    controls.style.display = 'none';
    streamCheckInterval = setInterval(() => {
      if (image.naturalWidth === 0)
        showError('Stream connection lost. Click Stream again to reconnect.');
    }, 5000);
  }
}
async function captureSnapshot() {
  const image = document.getElementById('cameraImage');
  const placeholder = document.getElementById('placeholder');
  try {
    placeholder.textContent = 'Capturing snapshot...';
    const timestamp = new Date().getTime();
    const response = await fetch(`/api/webcam/v1/snapshot?t=${timestamp}`);
    if (response.status === 409) {
      showError('Cannot capture snapshot while streaming is active. Stop the stream first. (code: 409)');
      return;
    }
    if (!response.ok) {
      throw new Error(`Failed to capture snapshot (code: ${response.status}): ${response.statusText}`);
    }
    const blob = await response.blob();
    const imageUrl = URL.createObjectURL(blob);
    image.onload = () => {
      placeholder.style.display = 'none';
      image.classList.add('visible');
      hideError();
    };
    image.onerror = () => {
      showError('Failed to load snapshot image');
      placeholder.style.display = 'block';
      placeholder.textContent = 'Failed to load image';
    };
    image.src = imageUrl;
  } catch (error) {
    console.error('Snapshot error:', error);
    showError('Failed to capture snapshot: ' + error.message);
    placeholder.style.display = 'block';
    placeholder.textContent = 'Failed to capture snapshot';
  }
}
async function refreshStatus() {
  try {
    const response = await fetch('/api/webcam/v1/serviceStatus');
    const data = await response.json();
    const indicator = document.getElementById('statusIndicator');
    const statusText = document.getElementById('statusText');
    const titleStatus = document.getElementById('titleStatus');
    if (data.status === 'started') {
      indicator.classList.add('active');
      indicator.classList.remove('inactive');
      statusText.textContent = 'Camera status: Started';
      titleStatus.textContent = '[Started]';
      titleStatus.style.color = '#4CAF50';
      hideError();
    } else {
      indicator.classList.remove('active');
      indicator.classList.add('inactive');
      statusText.textContent = `Camera status: ${data.status || 'Not ready'}`;
      titleStatus.textContent = `[${data.status || 'Not ready'}]`;
      titleStatus.style.color = '#f44336';
      showError('Camera is not started');
    }
  } catch (error) {
    console.error('Status check failed:', error);
    document.getElementById('statusText').textContent = 'Camera status: Error fetching status';
    showError('Failed to get camera status: ' + error.message + (error.status ? ` (code: ${error.status})` : ''));
  }
}
function downloadImage() {
  const image = document.getElementById('cameraImage');
  if (!image.src || image.src === '') {
    showError('No image to download');
    return;
  }
  const link = document.createElement('a');
  link.href = image.src;
  link.download = `k10-snapshot-${new Date().getTime()}.jpg`;
  link.click();
}
let _statusClearTimer = null;
function setStatus(message, type = 'info') {
  const el = document.getElementById('page-status');
  if (!el) return;
  if (_statusClearTimer) { clearTimeout(_statusClearTimer); _statusClearTimer = null; }
  el.textContent = message;
  el.className = 'status-left status-' + type;
  if (type === 'success') {
    _statusClearTimer = setTimeout(() => { el.textContent = ''; el.className = 'status-left'; }, 3000);
  }
}
function showError(message) { setStatus(message, 'error'); }
function hideError()        { setStatus('', 'info'); }
function showSuccess(message) { setStatus(message, 'success'); }
document.addEventListener('DOMContentLoaded', () => {
  refreshStatus();
});
window.addEventListener('beforeunload', () => {
  if (streamCheckInterval) clearInterval(streamCheckInterval);
});
