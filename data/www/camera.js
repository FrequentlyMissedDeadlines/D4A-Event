let currentMode = 'off';
let streamCheckInterval = null;
let currentBlobUrl = null;
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
    if (currentBlobUrl) { URL.revokeObjectURL(currentBlobUrl); currentBlobUrl = null; }
    image.src = '';
    image.classList.remove('visible');
    placeholder.style.display = 'block';
    placeholder.textContent = 'Camera is off. Select a mode to view the camera feed.';
    controls.style.display = 'none';
  } else if (mode === 'snapshot') {
    image.classList.remove('visible');
    placeholder.style.display = 'block';
    placeholder.textContent = 'Loading snapshot...';
    captureSnapshot();
    controls.style.display = 'flex';
  } else if (mode === 'stream') {
    placeholder.style.display = 'none';
    image.classList.add('visible');
    let streamLoaded = false;
    image.onload = () => { streamLoaded = true; };
    image.onerror = () => {
      if (currentMode !== 'stream') return;
      showError('Stream lost. Reconnecting...');
      setTimeout(() => {
        if (currentMode === 'stream') {
          image.src = '/cam/stream?' + new Date().getTime();
        }
      }, 2000);
    };
    image.src = '/cam/stream?' + new Date().getTime();
    controls.style.display = 'none';
    // Only flag a stall after the stream has successfully delivered at least
    // one frame AND been idle for 10 s (avoids false positives on slow startup)
    streamCheckInterval = setInterval(() => {
      if (!streamLoaded) return; // not yet loaded — don't flag as lost
      if (image.naturalWidth === 0 && currentMode === 'stream') {
        showError('Stream connection lost. Reconnecting...');
        image.src = '/cam/stream?' + new Date().getTime();
        streamLoaded = false;
      }
    }, 10000);
  }
}
async function captureSnapshot() {
  const image = document.getElementById('cameraImage');
  const placeholder = document.getElementById('placeholder');
  try {
    placeholder.textContent = 'Capturing snapshot...';
    const timestamp = new Date().getTime();
    const response = await fetch(`/cam/snapshot?t=${timestamp}`);
    if (response.status === 503) {
      showError('Cannot capture snapshot while streaming is active. Stop the stream first. (code: 503)');
      return;
    }
    if (!response.ok) {
      throw new Error(`Failed to capture snapshot (code: ${response.status}): ${response.statusText}`);
    }
    const blob = await response.blob();
    if (currentBlobUrl) { URL.revokeObjectURL(currentBlobUrl); currentBlobUrl = null; }
    const imageUrl = URL.createObjectURL(blob);
    currentBlobUrl = imageUrl;
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
  const indicator = document.getElementById('statusIndicator');
  const statusText = document.getElementById('statusText');
  const titleStatus = document.getElementById('titleStatus');
  try {
    // No dedicated status endpoint exists on the C++ backend.
    // Use a HEAD request against /cam/snapshot to verify the route is registered.
    const response = await fetch('/cam/snapshot', { method: 'HEAD' });
    // 503 means the route exists but the camera is busy/not initialised;
    // any non-404 response means the server is running and the route is registered.
    const alive = response.status !== 404;
    if (alive) {
      indicator.classList.add('active');
      indicator.classList.remove('inactive');
      statusText.textContent = 'Camera status: Started';
      titleStatus.textContent = '[Started]';
      titleStatus.style.color = '#4CAF50';
      hideError();
    } else {
      indicator.classList.remove('active');
      indicator.classList.add('inactive');
      statusText.textContent = 'Camera status: Not ready (routes not registered)';
      titleStatus.textContent = '[Not ready]';
      titleStatus.style.color = '#f44336';
      showError('Camera routes not registered on the bot');
    }
  } catch (error) {
    console.error('Status check failed:', error);
    statusText.textContent = 'Camera status: Error fetching status';
    showError('Failed to get camera status: ' + error.message);
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
