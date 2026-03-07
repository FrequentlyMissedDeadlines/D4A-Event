const servos = [
  { channel: 0, type: 0, value: 90 },
  { channel: 1, type: 0, value: 90 },
  { channel: 2, type: 0, value: 90 },
  { channel: 3, type: 0, value: 90 },
  { channel: 4, type: 0, value: 90 },
  { channel: 5, type: 0, value: 90 }
];
const servoTypes = {
  0: { name: 'Not Connected', label: 'Detached' },
  1: { name: 'Rotational', label: 'Continuous Rotation' },
  2: { name: 'Angular 180', label: 'Angular 180°' },
  3: { name: 'Angular 270', label: 'Angular 270°' }
};
function createServoCard(servo) {
  const isAttached = servo.type !== 0;
  const typeInfo = servoTypes[servo.type];
  return `
    <div class="servo-card type-${servo.type}" id="servo-${servo.channel}">
      <div class="servo-header">
        <div class="servo-title">
          <span class="status-indicator ${isAttached ? 'active' : 'inactive'}"></span>
          S${servo.channel}
        </div>
        <div class="servo-status ${isAttached ? 'attached' : ''}">${typeInfo.label}</div>
      </div>
      ${!isAttached ? `
        <div class="control-group">
          <label class="control-label">Attach Servo Type:</label>
          <select id="type-${servo.channel}" class="param-input">
            <option value="1">Continuous Rotation</option>
            <option value="2">Angular 180°</option>
            <option value="3">Angular 270°</option>
          </select>
        </div>
        <div class="button-group">
          <button onclick="attachServo(${servo.channel})" class="btn">🔗 Attach Servo</button>
        </div>
      ` : ''}
      ${isAttached && servo.type === 1 ? `
        <div class="control-group">
          <label class="control-label">Speed (-100 to +100):</label>
          <div class="slider-container">
            <input type="range" id="speed-${servo.channel}" min="-100" max="100" value="${servo.value}"
                   oninput="updateSliderValue(${servo.channel}, this.value, 'speed')">
            <span class="slider-value" id="speed-value-${servo.channel}">${servo.value}</span>
          </div>
        </div>
        <div class="button-group">
          <button onclick="setServoSpeed(${servo.channel})" class="btn btn-secondary">▶ Send</button>
          <button onclick="detachServo(${servo.channel})" class="btn btn-danger">🚽 Detach</button>
        </div>
      ` : ''}
      ${isAttached && (servo.type === 2) ? `
        <div class="control-group">
          <label class="control-label">Angle (-90° to 90°):</label>
          <div class="slider-container">
            <input type="range" id="angle-${servo.channel}" min="-90" max="90"
                   value="${servo.value}" oninput="updateSliderValue(${servo.channel}, this.value, 'angle')">
            <span class="slider-value" id="angle-value-${servo.channel}">${servo.value}°</span>
          </div>
        </div>
        <div class="button-group">
          <button onclick="setServoAngle(${servo.channel})" class="btn btn-secondary">▶ Send</button>
          <button onclick="detachServo(${servo.channel})" class="btn btn-danger">🚽 Detach</button>
        </div>
      `: ''}
      ${isAttached && (servo.type === 3) ? `
        <div class="control-group">
          <label class="control-label">Angle (-135° to 135°):</label>
          <div class="slider-container">
            <input type="range" id="angle-${servo.channel}" min="-135" max="135"
                   value="${servo.value}" oninput="updateSliderValue(${servo.channel}, this.value, 'angle')">
            <span class="slider-value" id="angle-value-${servo.channel}">${servo.value}°</span>
          </div>
        </div>
        <div class="button-group">
          <button onclick="setServoAngle(${servo.channel})" class="btn btn-secondary">▶ Send</button>
          <button onclick="detachServo(${servo.channel})" class="btn btn-danger">🚽 Detach</button>
        </div>
      ` : ''}
    </div>
  `;
}
function renderServos() {
  const grid = document.getElementById('servoGrid');
  grid.innerHTML = servos.map(createServoCard).join('');
}
function updateSliderValue(channel, value, type) {
  const valueDisplay = document.getElementById(`${type}-value-${channel}`);
  if (valueDisplay) {
    valueDisplay.textContent = type === 'angle' ? `${value}°` : value;
  }
}
async function attachServo(channel) {
  const typeSelect = document.getElementById(`type-${channel}`);
  const connection = parseInt(typeSelect.value);
  const result = await apiCall('/api/servos/v1/attachServo', 'POST', { channel, connection });
  if (result.ok) {
    servos[channel].type = connection;
    servos[channel].value = connection === 1 ? 0 : 90;
    renderServos();
  } else {
    showStatus(`❌ Failed to attach servo ${channel} - HTTP ${result.status || 'n/a'}`, true);
  }
}
async function detachServo(channel) {
  const result = await apiCall('/api/servos/v1/attachServo', 'POST', { channel, connection: 0 });
  if (result.ok) {
    servos[channel].type = 0;
    servos[channel].value = 90;
    renderServos();
  } else {
    showStatus(`❌ Failed to detach servo ${channel} - HTTP ${result.status || 'n/a'}`, true);
  }
}
async function setServoAngle(channel) {
  const angleInput = document.getElementById(`angle-${channel}`);
  const angle = parseInt(angleInput.value);
  const result = await apiCall('/api/servos/v1/setServoAngle', 'POST', { channel, angle });
  if (result.ok) {
    servos[channel].value = angle;
  } else {
    showStatus(`❌ Failed to set servo ${channel} angle - HTTP ${result.status || 'n/a'}`, true);
  }
}
async function setServoSpeed(channel) {
  const speedInput = document.getElementById(`speed-${channel}`);
  const speed = parseInt(speedInput.value);
  const result = await apiCall('/api/servos/v1/setServoSpeed', 'POST', { channel, speed });
  if (result.ok) {
    servos[channel].value = speed;
  } else {
    showStatus(`❌ Failed to set servo ${channel} speed - HTTP ${result.status || 'n/a'}`, true);
  }
}
async function stopAllServos() {
  const result = await apiCall('/api/servos/v1/stopAll', 'POST');
  if (result.ok) {
    servos.forEach(servo => {
      if (servo.type === 1) {
        servo.value = 0;
      }
    });
    renderServos();
    showStatus('⏹️ All servos stopped');
  } else {
    showStatus(`❌ Failed to stop servos - HTTP ${result.status || 'n/a'}`, true);
  }
}
async function setAllToMiddle() {
  const result = await apiCall('/api/servos/v1/setAllServoAngle', 'POST', { angle: 90 });
  if (result.ok) {
    servos.forEach(servo => {
      if (servo.type === 2 || servo.type === 3) {
        servo.value = 90;
      }
    });
    renderServos();
  } else {
    showStatus(`❌ Failed to set servos to middle position - HTTP ${result.status || 'n/a'}`, true);
  }
}
async function refreshAllStatus() {
  try {
    const statusResult = await apiCall('/api/servos/v1/serviceStatus', 'GET');
    if (statusResult.ok && statusResult.data.status) {
      setTitleStatus(`[${statusResult.data.status}]`, statusResult.data.status === 'started' ? '#4CAF50' : '#FFA500');
    }
    const result = await apiCall('/api/servos/v1/getAllStatus', 'GET');
    if (result.ok && result.data.servos) {
      result.data.servos.forEach((servoStatus, index) => {
        if (index < servos.length) {
          const statusMap = {
            'Not Connected': 0,
            'Rotational': 1,
            'Angular 180': 2,
            'Angular 270': 3
          };
          servos[index].type = statusMap[servoStatus.status] || 0;
        }
      });
      renderServos();
      console.log('Status refreshed:', result.data);
    }
  } catch (error) {
    console.error('Failed to refresh status:', error);
    setTitleStatus('[Error]', '#f44336');
  }
}
const motors = [
  { motor: 1, speed: 0 },
  { motor: 2, speed: 0 },
  { motor: 3, speed: 0 },
  { motor: 4, speed: 0 }
];
function createMotorCard(m) {
  return `
    <div class="motor-card" id="motor-${m.motor}">
      <div class="motor-header">
        <div class="motor-title">
          <span class="status-indicator ${m.speed !== 0 ? 'active' : 'inactive'}"></span>
          M${m.motor}
        </div>
        <div class="motor-speed-badge" id="motor-badge-${m.motor}">${m.speed}</div>
      </div>
      <div class="control-group">
        <label class="control-label">Speed (-100 to +100):</label>
        <div class="slider-container">
          <input type="range" id="motor-speed-${m.motor}" min="-100" max="100" value="${m.speed}"
                 oninput="updateMotorSlider(${m.motor}, this.value)">
          <span class="slider-value" id="motor-speed-value-${m.motor}">${m.speed}</span>
        </div>
      </div>
      <div class="button-group">
        <button onclick="sendMotorSpeed(${m.motor})" class="btn btn-secondary">▶ Send</button>
        <button onclick="stopMotor(${m.motor})" class="btn btn-danger">⏹ Stop</button>
      </div>
    </div>
  `;
}
function renderMotors() {
  document.getElementById('motorGrid').innerHTML = motors.map(createMotorCard).join('');
}
function updateMotorSlider(motor, value) {
  const idx = motor - 1;
  motors[idx].speed = parseInt(value);
  const badge = document.getElementById(`motor-badge-${motor}`);
  const display = document.getElementById(`motor-speed-value-${motor}`);
  if (badge)   badge.textContent   = value;
  if (display) display.textContent = value;
}
async function sendMotorSpeed(motor) {
  const speed = motors[motor - 1].speed;
  const result = await apiCall('/api/servos/v1/setMotorSpeed', 'POST', { motor, speed });
  if (!result.ok) showStatus(`❌ Failed to set motor ${motor} speed - HTTP ${result.status || 'n/a'}`, true);
}
async function stopMotor(motor) {
  motors[motor - 1].speed = 0;
  const slider  = document.getElementById(`motor-speed-${motor}`);
  const display = document.getElementById(`motor-speed-value-${motor}`);
  const badge   = document.getElementById(`motor-badge-${motor}`);
  if (slider)  slider.value        = 0;
  if (display) display.textContent = 0;
  if (badge)   badge.textContent   = 0;
  const result = await apiCall('/api/servos/v1/setMotorSpeed', 'POST', { motor, speed: 0 });
  if (!result.ok) showStatus(`❌ Failed to stop motor ${motor} - HTTP ${result.status || 'n/a'}`, true);
}
async function stopAllMotors() {
  const result = await apiCall('/api/servos/v1/stopAllMotors', 'POST');
  if (result.ok) {
    motors.forEach(m => {
      m.speed = 0;
      const slider  = document.getElementById(`motor-speed-${m.motor}`);
      const display = document.getElementById(`motor-speed-value-${m.motor}`);
      const badge   = document.getElementById(`motor-badge-${m.motor}`);
      if (slider)  slider.value        = 0;
      if (display) display.textContent = 0;
      if (badge)   badge.textContent   = 0;
    });
  } else {
    showStatus(`❌ Failed to stop all motors - HTTP ${result.status || 'n/a'}`, true);
  }
}
async function setAllMotorsSpeedLive(value) {
  const speed = parseInt(value);
  document.getElementById('all-motors-speed-value').textContent = speed;
  motors.forEach(m => {
    m.speed = speed;
    const slider  = document.getElementById(`motor-speed-${m.motor}`);
    const display = document.getElementById(`motor-speed-value-${m.motor}`);
    const badge   = document.getElementById(`motor-badge-${m.motor}`);
    if (slider)  slider.value        = speed;
    if (display) display.textContent = speed;
    if (badge)   badge.textContent   = speed;
  });
  const result = await apiCall('/api/servos/v1/setAllMotorsSpeed', 'POST', { speed });
  if (!result.ok) showStatus(`❌ Failed to set all motors speed - HTTP ${result.status || 'n/a'}`, true);
}
document.addEventListener('DOMContentLoaded', () => {
  renderServos();
  renderMotors();
  refreshAllStatus();
});
