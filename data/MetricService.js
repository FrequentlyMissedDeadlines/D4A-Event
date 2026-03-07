
const metric_history = {
  light: [],
  temperature: [],
  humidity: [],
  accel_x: [],
  accel_y: [],
  accel_z: [],
  udp_total: [],
  udp_dropped: [],
  battery: []
};
const max_history_length = 100;
function addToHistory(array, value) {
  if (value !== undefined && value !== null && !isNaN(value)) {
    array.push(value);
    if (array.length > max_history_length) {
      array.shift();
    }
  }
}
function drawGraph(canvasId, data, color = '#2196F3', fixedMin = null, fixedMax = null) {
  const canvas = document.getElementById(canvasId);
  if (!canvas || !data || data.length === 0) return;
  const ctx = canvas.getContext('2d');
  const width = canvas.width;
  const height = canvas.height;
  const padding = 5;
  ctx.clearRect(0, 0, width, height);
  ctx.fillStyle = '#fafafa';
  ctx.fillRect(0, 0, width, height);
  const min_val = fixedMin !== null ? fixedMin : Math.min(...data);
  const max_val = fixedMax !== null ? fixedMax : Math.max(...data);
  const range = max_val - min_val || 1;
  ctx.strokeStyle = '#e0e0e0';
  ctx.lineWidth = 1;
  for (let i = 0; i <= 4; i++) {
    const y = padding + (height - 2 * padding) * i / 4;
    ctx.beginPath();
    ctx.moveTo(padding, y);
    ctx.lineTo(width - padding, y);
    ctx.stroke();
  }
  ctx.strokeStyle = color;
  ctx.lineWidth = 2;
  ctx.beginPath();
  for (let i = 0; i < data.length; i++) {
    const x = padding + (width - 2 * padding) * i / (max_history_length - 1);
    const y = height - padding - ((data[i] - min_val) / range) * (height - 2 * padding);
    if (i === 0) {
      ctx.moveTo(x, y);
    } else {
      ctx.lineTo(x, y);
    }
  }
  ctx.stroke();
  ctx.fillStyle = '#666';
  ctx.font = '10px Arial';
  ctx.fillText(max_val.toFixed(1), padding + 2, padding + 10);
  ctx.fillText(min_val.toFixed(1), padding + 2, height - padding - 2);
}
async function refreshSensors() {
  const container = document.getElementById('sensors-content');
  try {
    const response = await fetch('/api/sensors/v1/');
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}: ${response.statusText}`);
    }
    const data = await response.json();
    if (autoRefreshEnabled) {
      addToHistory(metric_history.light, data.light);
      addToHistory(metric_history.temperature, data.celcius);
      addToHistory(metric_history.humidity, data.hum_rel);
      if (data.accelerometer && Array.isArray(data.accelerometer)) {
        addToHistory(metric_history.accel_x, data.accelerometer[0]);
        addToHistory(metric_history.accel_y, data.accelerometer[1]);
        addToHistory(metric_history.accel_z, data.accelerometer[2]);
      }
    }
    let html = '';
    html += `
      <div class="metric-row">
        <span class="metric-label">💡 Light Level:</span>
        <span class="metric-value">${data.light !== undefined ? data.light.toFixed(1) : 'N/A'}</span>
      </div>
    `;
    if (autoRefreshEnabled && metric_history.light.length > 1) {
      html += `
        <div class="graph-container">
          <div class="graph-title">Light Level History</div>
          <canvas id="graph-light" class="metric-graph" ></canvas>
        </div>
      `;
    }
    html += `
      <div class="metric-row">
        <span class="metric-label">🌡️ Temperature:</span>
        <span class="metric-value">${data.celcius !== undefined ? data.celcius.toFixed(1) + '°C' : 'N/A'}</span>
      </div>
    `;
    if (autoRefreshEnabled && metric_history.temperature.length > 1) {
      html += `
        <div class="graph-container">
          <div class="graph-title">Temperature History</div>
          <canvas id="graph-temp" class="metric-graph" ></canvas>
        </div>
      `;
    }
    html += `
      <div class="metric-row">
        <span class="metric-label">💧 Humidity:</span>
        <span class="metric-value">${data.hum_rel !== undefined ? data.hum_rel.toFixed(1) + '%' : 'N/A'}</span>
      </div>
    `;
    if (autoRefreshEnabled && metric_history.humidity.length > 1) {
      html += `
        <div class="graph-container">
          <div class="graph-title">Humidity History</div>
          <canvas id="graph-humidity" class="metric-graph" ></canvas>
        </div>
      `;
    }
    if (data.accelerometer && Array.isArray(data.accelerometer)) {
      html += `
        <div class="metric-row">
          <span class="metric-label">📐 Accelerometer:</span>
        </div>
        <div class="accelerometer-values">
          <div class="accel-axis">
            <div class="accel-axis-label">X</div>
            <div class="accel-axis-value">${data.accelerometer[0] !== undefined ? data.accelerometer[0].toFixed(2) : 'N/A'}</div>
          </div>
          <div class="accel-axis">
            <div class="accel-axis-label">Y</div>
            <div class="accel-axis-value">${data.accelerometer[1] !== undefined ? data.accelerometer[1].toFixed(2) : 'N/A'}</div>
          </div>
          <div class="accel-axis">
            <div class="accel-axis-label">Z</div>
            <div class="accel-axis-value">${data.accelerometer[2] !== undefined ? data.accelerometer[2].toFixed(2) : 'N/A'}</div>
          </div>
        </div>
      `;
      if (autoRefreshEnabled && metric_history.accel_x.length > 1) {
        html += `
          <div class="graph-container">
            <div class="graph-title">Accelerometer X History</div>
            <canvas id="graph-accel-x" class="metric-graph" ></canvas>
          </div>
          <div class="graph-container">
            <div class="graph-title">Accelerometer Y History</div>
            <canvas id="graph-accel-y" class="metric-graph" w></canvas>
          </div>
          <div class="graph-container">
            <div class="graph-title">Accelerometer Z History</div>
            <canvas id="graph-accel-z" class="metric-graph" ></canvas>
          </div>
        `;
      }
    }
    html += `<div class="timestamp">Updated: ${new Date().toLocaleTimeString()}</div>`;
    container.innerHTML = html;
    if (autoRefreshEnabled) {
      setTimeout(() => {
        if (metric_history.light.length > 1) drawGraph('graph-light', metric_history.light, '#FFC107');
        if (metric_history.temperature.length > 1) drawGraph('graph-temp', metric_history.temperature, '#F44336');
        if (metric_history.humidity.length > 1) drawGraph('graph-humidity', metric_history.humidity, '#2196F3');
        if (metric_history.accel_x.length > 1) drawGraph('graph-accel-x', metric_history.accel_x, '#4CAF50');
        if (metric_history.accel_y.length > 1) drawGraph('graph-accel-y', metric_history.accel_y, '#4CAF50');
        if (metric_history.accel_z.length > 1) drawGraph('graph-accel-z', metric_history.accel_z, '#4CAF50');
      }, 10);
    }
  } catch (error) {
    container.innerHTML = `
      <div class="error-message">
        ❌ Error loading sensor data:<br>
        ${error.message}
      </div>
    `;
  }
}
async function refreshUDP() {
  const container = document.getElementById('udp-content');
  try {
    const response = await fetch('/api/udp/v1/');
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}: ${response.statusText}`);
    }
    const data = await response.json();
    if (autoRefreshEnabled) {
      addToHistory(metric_history.udp_total, data.total);
      addToHistory(metric_history.udp_dropped, data.dropped);
    }
    let html = '';
    html += `
      <div class="metric-row">
        <span class="metric-label">🔌 Port:</span>
        <span class="metric-value metric-highlight">${data.port !== undefined ? data.port : 'N/A'}</span>
      </div>
    `;
    html += `
      <div class="metric-row">
        <span class="metric-label">📨 Total Messages:</span>
        <span class="metric-value metric-highlight">${data.total !== undefined ? data.total : 'N/A'}</span>
      </div>
    `;
    if (autoRefreshEnabled && metric_history.udp_total.length > 1) {
      html += `
        <div class="graph-container">
          <div class="graph-title">Total Messages History</div>
          <canvas id="graph-udp-total" class="metric-graph" ></canvas>
        </div>
      `;
    }
    const droppedClass = data.dropped > 0 ? 'metric-warning' : 'metric-highlight';
    html += `
      <div class="metric-row">
        <span class="metric-label">⚠️ Dropped Packets:</span>
        <span class="metric-value ${droppedClass}">${data.dropped !== undefined ? data.dropped : 'N/A'}</span>
      </div>
    `;
    if (autoRefreshEnabled && metric_history.udp_dropped.length > 1) {
      html += `
        <div class="graph-container">
          <div class="graph-title">Dropped Packets History</div>
          <canvas id="graph-udp-dropped" class="metric-graph" ></canvas>
        </div>
      `;
    }
    html += `
      <div class="metric-row">
        <span class="metric-label">💾 Buffer Usage:</span>
        <span class="metric-value">${data.buffer !== undefined ? data.buffer : 'N/A'}</span>
      </div>
    `;
    if (data.error) {
      html += `
        <div class="metric-row">
          <span class="metric-label">⚠️ Status:</span>
          <span class="metric-value metric-error">${data.error}</span>
        </div>
      `;
    }
    if (data.messages && Array.isArray(data.messages) && data.messages.length > 0) {
      html += `
        <div class="metric-row">
          <span class="metric-label">📋 Recent Messages (${data.messages.length}):</span>
        </div>
        <div class="messages-list">
      `;
      data.messages.forEach((msg, index) => {
        if (msg && msg.trim() !== '') {
          html += `<div class="message-item">${escapeHtml(msg)}</div>`;
        }
      });
      html += '</div>';
    }
    html += `<div class="timestamp">Updated: ${new Date().toLocaleTimeString()}</div>`;
    container.innerHTML = html;
    if (autoRefreshEnabled) {
      setTimeout(() => {
        if (metric_history.udp_total.length > 1) drawGraph('graph-udp-total', metric_history.udp_total, '#03A9F4');
        if (metric_history.udp_dropped.length > 1) drawGraph('graph-udp-dropped', metric_history.udp_dropped, '#FF5722');
      }, 10);
    }
  } catch (error) {
    container.innerHTML = `
      <div class="error-message">
        ❌ Error loading UDP statistics:<br>
        ${error.message}
      </div>
    `;
  }
}
async function refreshBoardInfo() {
  const container = document.getElementById('board-content');
  try {
    const response = await fetch('/api/board/v1/');
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}: ${response.statusText}`);
    }
    const data = await response.json();
    let html = '';
    if (data.board) {
      html += `
        <div class="metric-row">
          <span class="metric-label">🎛️ Board:</span>
          <span class="metric-value metric-highlight">${data.board}</span>
        </div>
      `;
    }
    if (data.version) {
      html += `
        <div class="metric-row">
          <span class="metric-label">📌 Firmware:</span>
          <span class="metric-value">${data.version}</span>
        </div>
      `;
    }
    if (data.chipModel) {
      html += `
        <div class="metric-row">
          <span class="metric-label">🔌 Chip Model:</span>
          <span class="metric-value metric-highlight">${data.chipModel}</span>
        </div>
      `;
    }
    if (data.chipRevision !== undefined) {
      html += `
        <div class="metric-row">
          <span class="metric-label">📝 Chip Revision:</span>
          <span class="metric-value">${data.chipRevision}</span>
        </div>
      `;
    }
    if (data.chipCores !== undefined) {
      html += `
        <div class="metric-row">
          <span class="metric-label">⚙️ CPU Cores:</span>
          <span class="metric-value metric-highlight">${data.chipCores}</span>
        </div>
      `;
    }
    if (data.cpuFreqMHz !== undefined) {
      html += `
        <div class="metric-row">
          <span class="metric-label">⚡ CPU Frequency:</span>
          <span class="metric-value">${data.cpuFreqMHz} MHz</span>
        </div>
      `;
    }
    if (data.uptimeMs !== undefined) {
      const uptimeSeconds = Math.floor(data.uptimeMs / 1000);
      const hours = Math.floor(uptimeSeconds / 3600);
      const minutes = Math.floor((uptimeSeconds % 3600) / 60);
      const seconds = uptimeSeconds % 60;
      html += `
        <div class="metric-row">
          <span class="metric-label">⏱️ Uptime:</span>
          <span class="metric-value">${hours}h ${minutes}m ${seconds}s</span>
        </div>
      `;
    }
    if (data.heapFree !== undefined) {
      const heapClass = data.heapFree < 50000 ? 'metric-warning' : 'metric-highlight';
      const heapKB = (data.heapFree / 1024).toFixed(1);
      html += `
        <div class="metric-row">
          <span class="metric-label">💾 Free Heap:</span>
          <span class="metric-value ${heapClass}">${heapKB} KB</span>
        </div>
      `;
    }
    if (data.heapTotal !== undefined) {
      const heapKB = (data.heapTotal / 1024).toFixed(1);
      html += `
        <div class="metric-row">
          <span class="metric-label">💽 Total Heap:</span>
          <span class="metric-value">${heapKB} KB</span>
        </div>
      `;
    }
    if (data.freeSketchSpace !== undefined) {
      const sketchKB = (data.freeSketchSpace / 1024).toFixed(1);
      html += `
        <div class="metric-row">
          <span class="metric-label">📦 Free Flash:</span>
          <span class="metric-value">${sketchKB} KB</span>
        </div>
      `;
    }
    if (data.sdkVersion) {
      html += `
        <div class="metric-row">
          <span class="metric-label">🛠️ SDK Version:</span>
          <span class="metric-value">${data.sdkVersion}</span>
        </div>
      `;
    }
    html += `<div class="timestamp">Updated: ${new Date().toLocaleTimeString()}</div>`;
    container.innerHTML = html;
  } catch (error) {
    container.innerHTML = `
      <div class="error-message">
        ❌ Error loading board information:<br>
        ${error.message}
      </div>
    `;
  }
}
async function refreshBattery() {
  const container = document.getElementById('battery-content');
  try {
    const response = await fetch('/api/servos/v1/getBattery');
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}: ${response.statusText}`);
    }
    const data = await response.json();
    const level = data.battery;
    if (autoRefreshEnabled) {
      addToHistory(metric_history.battery, level);
    }
    let levelClass = 'metric-highlight';
    if (level !== undefined) {
      if (level <= 10) levelClass = 'metric-error';
      else if (level <= 30) levelClass = 'metric-warning';
    }
    let html = `
      <div class="metric-row">
        <span class="metric-label">🔋 Battery Level:</span>
        <span class="metric-value ${levelClass}">${level !== undefined ? level + '%' : 'N/A'}</span>
      </div>
    `;
    if (autoRefreshEnabled && metric_history.battery.length > 1) {
      html += `
        <div class="graph-container">
          <div class="graph-title">Battery History</div>
          <canvas id="graph-battery" class="metric-graph"></canvas>
        </div>
      `;
    }
    html += `<div class="timestamp">Updated: ${new Date().toLocaleTimeString()}</div>`;
    container.innerHTML = html;
    if (autoRefreshEnabled && metric_history.battery.length > 1) {
      setTimeout(() => drawGraph('graph-battery', metric_history.battery, '#4CAF50', 0, 100), 10);
    }
  } catch (error) {
    container.innerHTML = `
      <div class="error-message">
        ❌ Error loading battery level:<br>
        ${error.message}
      </div>
    `;
  }
}
async function refreshAllMetrics() {
  try {
    await Promise.all([
      refreshBoardInfo(),
      refreshSensors(),
      refreshUDP(),
      refreshBattery()
    ]);
    const titleStatus = document.getElementById('titleStatus');
    if (titleStatus) {
      titleStatus.textContent = '[Active]';
      titleStatus.style.color = '#4CAF50';
    }
  } catch (error) {
    const titleStatus = document.getElementById('titleStatus');
    if (titleStatus) {
      titleStatus.textContent = '[Error]';
      titleStatus.style.color = '#f44336';
    }
  }
}
function escapeHtml(text) {
  const div = document.createElement('div');
  div.textContent = text;
  return div.innerHTML;
}
let autoRefreshEnabled = false;
let autoRefreshInterval = null;
function toggleAutoRefresh() {
  autoRefreshEnabled = !autoRefreshEnabled;
  const btn = document.getElementById('autoRefreshBtn');
  if (autoRefreshEnabled) {
    autoRefreshInterval = setInterval(refreshAllMetrics, 1000);
    btn.textContent = '⏱️ Auto-Refresh: ON';
    btn.style.background = '#4CAF50';
    btn.style.color = 'white';
  } else {
    if (autoRefreshInterval) {
      clearInterval(autoRefreshInterval);
      autoRefreshInterval = null;
    }
    btn.textContent = '⏱️ Auto-Refresh: OFF';
    btn.style.background = '';
    btn.style.color = '';
  }
}
window.addEventListener('DOMContentLoaded', () => {
  refreshAllMetrics();
});
