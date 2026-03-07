


const leds = [
  { id: 0, r: 0, g: 0, b: 0 },
  { id: 1, r: 0, g: 0, b: 0 }
];


let toastTimer = null;
function showToast(msg, type = 'ok') {
  const el = document.getElementById('toast');
  el.textContent = msg;
  el.className = 'show ' + type;
  if (toastTimer) clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { el.className = ''; }, 2500);
}




function rgbToHex(r, g, b) {
  return '#' + [r, g, b].map(v => v.toString(16).padStart(2, '0')).join('');
}
function hexToRgb(hex) {
  const r = parseInt(hex.slice(1, 3), 16);
  const g = parseInt(hex.slice(3, 5), 16);
  const b = parseInt(hex.slice(5, 7), 16);
  return { r, g, b };
}

function glowStyle(r, g, b) {
  const isOn = (r + g + b) > 0;
  if (!isOn) return '';
  const alpha = Math.min(1, (r + g + b) / 300);
  return `0 0 18px rgba(${r},${g},${b},${alpha.toFixed(2)}), 0 0 36px rgba(${r},${g},${b},${(alpha * 0.5).toFixed(2)})`;
}

function createLEDCard(led) {
  const isOn = (led.r + led.g + led.b) > 0;
  const hex  = rgbToHex(led.r, led.g, led.b);
  return `
    <div class="led-card" id="led-card-${led.id}">
      <div class="led-card-header">
        <div class="led-title">LED ${led.id}</div>
        <div class="led-status-badge ${isOn ? 'on' : ''}" id="led-badge-${led.id}">${isOn ? 'ON' : 'OFF'}</div>
      </div>
      <div class="led-preview-wrap">
        <div class="led-preview ${isOn ? 'lit' : ''}" id="led-preview-${led.id}"
             style="background:${hex};box-shadow:${glowStyle(led.r, led.g, led.b)};"></div>
      </div>
      <div class="color-picker-wrap">
        <input type="color" id="led-picker-${led.id}" value="${hex}"
               oninput="onColorPick(${led.id}, this.value)">
        <span style="font-size:12px;color:#a6adc8;" id="led-hex-${led.id}">${hex}</span>
      </div>
      <div class="led-btn-group">
        <button class="btn-led-off" onclick="turnOffLED(${led.id})">⬛ Off</button>
      </div>
    </div>
  `;
}

function renderLEDs() {
  document.getElementById('ledGrid').innerHTML = leds.map(createLEDCard).join('');
}

const ledDebounce = {};

function onColorPick(id, hex) {
  const { r, g, b } = hexToRgb(hex);
  leds[id].r = r; leds[id].g = g; leds[id].b = b;
  document.getElementById(`led-hex-${id}`).textContent = hex;
  updateLEDPreview(id, r, g, b);
  clearTimeout(ledDebounce[id]);
  ledDebounce[id] = setTimeout(() => setLEDColor(id), 300);
}

function updateLEDPreview(id, r, g, b) {
  const preview = document.getElementById(`led-preview-${id}`);
  const badge   = document.getElementById(`led-badge-${id}`);
  const hex     = rgbToHex(r, g, b);
  const isOn    = (r + g + b) > 0;
  preview.style.background = hex;
  preview.style.boxShadow  = glowStyle(r, g, b);
  preview.classList.toggle('lit', isOn);
  badge.textContent = isOn ? 'ON' : 'OFF';
  badge.className   = `led-status-badge${isOn ? ' on' : ''}`;
}




async function setLEDColor(id) {
  const { r, g, b } = leds[id];
  const url = `/api/dfr1216/setLEDColor?led=${id}&red=${r}&green=${g}&blue=${b}`;
  const result = await apiPost(url);
  if (result.ok) {
    showToast(`LED ${id} set to RGB(${r},${g},${b})`, 'ok');
  } else {
    showToast(`Failed to set LED ${id} - HTTP ${result.status || 'n/a'}`, 'err');
  }
}

async function turnOffLED(id) {
  const result = await apiPost(`/api/dfr1216/turnOffLED?led=${id}`);
  if (result.ok) {
    leds[id].r = 0; leds[id].g = 0; leds[id].b = 0;
    document.getElementById(`led-picker-${id}`).value    = '#000000';
    document.getElementById(`led-hex-${id}`).textContent = '#000000';
    updateLEDPreview(id, 0, 0, 0);
    showToast(`LED ${id} turned off`, 'ok');
  } else {
    showToast(`Failed to turn off LED ${id} - HTTP ${result.status || 'n/a'}`, 'err');
  }
}

async function turnOffAllLEDs() {
  const results = await Promise.all(leds.map(l =>
    apiPost(`/api/dfr1216/turnOffLED?led=${l.id}`)
  ));
  const allOk = results.every(r => r.ok);
  if (allOk) {
    leds.forEach(l => { l.r = 0; l.g = 0; l.b = 0; });
    renderLEDs();
    showToast('All LEDs turned off', 'ok');
  } else {
    showToast('Some LEDs failed to turn off - HTTP ' + (results.find(r => !r.ok)?.status || 'n/a'), 'err');
  }
}

async function fetchLEDStatus() {
  const result = await apiGet('/api/dfr1216/getLEDStatus');
  if (result.ok && result.data.leds) {
    result.data.leds.forEach(led => {
      leds[led.id].r = led.red;
      leds[led.id].g = led.green;
      leds[led.id].b = led.blue;
    });
    renderLEDs();
    showToast('LED status refreshed', 'ok');
  } else {
    showToast(`Failed to fetch LED status - HTTP ${result.status || 'n/a'}`, 'err');
  }
}

async function fetchServiceStatus() {
  const result = await apiGet('/api/dfr1216/getStatus');
  const badge  = document.getElementById('ledStatusBadge');
  if (result.ok && result.data.status) {
    badge.textContent = `[DFR1216: ${result.data.status}]`;
    badge.style.color = result.data.status === 'started' ? '#4CAF50' : '#FFA500';
  } else {
    badge.textContent = '[DFR1216: unreachable]';
    badge.style.color = '#f44336';
  }
}




const k10leds = [
  { id: 0, r: 0, g: 0, b: 0 },
  { id: 1, r: 0, g: 0, b: 0 },
  { id: 2, r: 0, g: 0, b: 0 }
];

function createK10LEDCard(led) {
  const isOn = (led.r + led.g + led.b) > 0;
  const hex  = rgbToHex(led.r, led.g, led.b);
  return `
    <div class="led-card" id="k10-card-${led.id}">
      <div class="led-card-header">
        <div class="led-title">LED ${led.id}</div>
        <div class="led-status-badge ${isOn ? 'on' : ''}" id="k10-badge-${led.id}">${isOn ? 'ON' : 'OFF'}</div>
      </div>
      <div class="led-preview-wrap">
        <div class="led-preview ${isOn ? 'lit' : ''}" id="k10-preview-${led.id}"
             style="background:${hex};box-shadow:${glowStyle(led.r, led.g, led.b)};"></div>
      </div>
      <div class="color-picker-wrap">
        <input type="color" id="k10-picker-${led.id}" value="${hex}"
               oninput="onK10ColorPick(${led.id}, this.value)">
        <span style="font-size:12px;color:#a6adc8;" id="k10-hex-${led.id}">${hex}</span>
      </div>
      <div class="led-btn-group">
        <button class="btn-led-off" onclick="turnOffK10LED(${led.id})">⬛ Off</button>
      </div>
    </div>
  `;
}

function renderK10LEDs() {
  document.getElementById('k10LedGrid').innerHTML = k10leds.map(createK10LEDCard).join('');
}

const k10LedDebounce = {};

function onK10ColorPick(id, hex) {
  const { r, g, b } = hexToRgb(hex);
  k10leds[id].r = r; k10leds[id].g = g; k10leds[id].b = b;
  document.getElementById(`k10-hex-${id}`).textContent = hex;
  updateK10LEDPreview(id, r, g, b);
  clearTimeout(k10LedDebounce[id]);
  k10LedDebounce[id] = setTimeout(() => setK10LEDColor(id), 300);
}

function updateK10LEDPreview(id, r, g, b) {
  const preview = document.getElementById(`k10-preview-${id}`);
  const badge   = document.getElementById(`k10-badge-${id}`);
  const hex     = rgbToHex(r, g, b);
  const isOn    = (r + g + b) > 0;
  preview.style.background = hex;
  preview.style.boxShadow  = glowStyle(r, g, b);
  preview.classList.toggle('lit', isOn);
  badge.textContent = isOn ? 'ON' : 'OFF';
  badge.className   = `led-status-badge${isOn ? ' on' : ''}`;
}

async function setK10LEDColor(id) {
  const { r, g, b } = k10leds[id];
  const result = await apiPost(`/api/board/v1/leds/set?led=${id}&red=${r}&green=${g}&blue=${b}`);
  if (result.ok) {
    showToast(`K10 LED ${id} set to RGB(${r},${g},${b})`, 'ok');
  } else {
    showToast(`Failed to set K10 LED ${id} - HTTP ${result.status || 'n/a'}`, 'err');
  }
}

async function turnOffK10LED(id) {
  const result = await apiPost(`/api/board/v1/leds/off?led=${id}`);
  if (result.ok) {
    k10leds[id].r = 0; k10leds[id].g = 0; k10leds[id].b = 0;
    document.getElementById(`k10-picker-${id}`).value    = '#000000';
    document.getElementById(`k10-hex-${id}`).textContent = '#000000';
    updateK10LEDPreview(id, 0, 0, 0);
    showToast(`K10 LED ${id} turned off`, 'ok');
  } else {
    showToast(`Failed to turn off K10 LED ${id} - HTTP ${result.status || 'n/a'}`, 'err');
  }
}

async function turnOffAllK10LEDs() {
  const results = await Promise.all(k10leds.map(l =>
    apiPost(`/api/board/v1/leds/off?led=${l.id}`)
  ));
  if (results.every(r => r.ok)) {
    k10leds.forEach(l => { l.r = 0; l.g = 0; l.b = 0; });
    renderK10LEDs();
    showToast('All K10 LEDs turned off', 'ok');
  } else {
    showToast('Some K10 LEDs failed to turn off - HTTP ' + (results.find(r => !r.ok)?.status || 'n/a'), 'err');
  }
}

async function fetchK10LEDStatus() {
  const result = await apiGet('/api/board/v1/leds');
  if (result.ok && result.data.leds) {
    result.data.leds.forEach(entry => {
      const idx = entry.led;
      if (idx >= 0 && idx < 3) {
        k10leds[idx].r = entry.red;
        k10leds[idx].g = entry.green;
        k10leds[idx].b = entry.blue;
      }
    });
    renderK10LEDs();
    showToast('K10 LED status refreshed', 'ok');
  } else {
    showToast(`Failed to fetch K10 LED status - HTTP ${result.status || 'n/a'}`, 'err');
  }
}




document.addEventListener('DOMContentLoaded', () => {
  renderLEDs();
  renderK10LEDs();
  fetchServiceStatus();
  fetchLEDStatus();
  fetchK10LEDStatus();
});
