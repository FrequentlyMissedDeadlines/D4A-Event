const melodies = [
  { id: 0, name: 'DADADADUM' },
  { id: 1, name: 'ENTERTAINER' },
  { id: 2, name: 'PRELUDE' },
  { id: 3, name: 'ODE' },
  { id: 4, name: 'NYAN' },
  { id: 5, name: 'RINGTONE' },
  { id: 6, name: 'FUNK' },
  { id: 7, name: 'BLUES' },
  { id: 8, name: 'BIRTHDAY' },
  { id: 9, name: 'WEDDING' },
  { id: 10, name: 'FUNERAL' },
  { id: 11, name: 'PUNCHLINE' },
  { id: 12, name: 'BADDY' },
  { id: 13, name: 'CHASE' },
  { id: 14, name: 'BA_DING' },
  { id: 15, name: 'WAWAWAWAA' },
  { id: 16, name: 'JUMP_UP' },
  { id: 17, name: 'JUMP_DOWN' },
  { id: 18, name: 'POWER_UP' },
  { id: 19, name: 'POWER_DOWN' }
];
let selectedOption = 1; 
function renderMelodies() {
  const grid = document.getElementById('melodyGrid');
  grid.innerHTML = melodies.map(melody =>
    `<button class="melody-btn" onclick="playMelody(${melody.id})">${melody.name}</button>`
  ).join('');
}
function selectOption(option, button) {
  document.querySelectorAll('.option-btn').forEach(btn => {
    btn.classList.remove('active');
  });
  button.classList.add('active');
  selectedOption = option;
}
async function playMelody(melodyId) {
  const result = await apiCallParams('/api/music/v1/play', 'POST', {
    melody: melodyId,
    option: selectedOption
  });
  if (result.ok) {
    const melodyName = melodies.find(m => m.id === melodyId)?.name || melodyId;
    const optionName = getOptionName(selectedOption);
    showStatus(`🎵 Playing ${melodyName} (${optionName})`);
  } else {
    showStatus(`❌ Failed to play melody - HTTP ${result.status || 'n/a'}`, true);
  }
}
async function playNote(frequency) {
  const result = await apiCallParams('/api/music/v1/tone', 'POST', {
    freq: frequency,
    beat: 8000
  });
  if (result.ok) {
    showStatus(`🎹 Playing ${frequency}Hz`);
  } else {
    showStatus(`❌ Failed to play note ${frequency}Hz - HTTP ${result.status || 'n/a'}`, true);
  }
}
async function playCustomTone() {
  const freq = document.getElementById('freqInput').value;
  const duration = document.getElementById('beatInput').value;
  const beat = Math.round(duration * 16);
  const result = await apiCallParams('/api/music/v1/tone', 'POST', {
    freq: freq,
    beat: beat
  });
  if (result.ok) {
    showStatus(`🎹 Playing ${freq}Hz for ${duration}ms`);
  } else {
    showStatus(`❌ Failed to play tone ${freq}Hz for ${duration}ms - HTTP ${result.status || 'n/a'}`, true);
  }
}
async function stopAllMusic() {
  const result = await apiPost('/api/music/v1/stop');
  if (result.ok) {
    showStatus('⏹️ Music stopped');
  } else {
    showStatus(`❌ Failed to stop music - HTTP ${result.status || 'n/a'}`, true);
  }
}
function getOptionName(option) {
  switch(option) {
    case 1: return 'Once';
    case 2: return 'Forever';
    case 4: return 'Once (BG)';
    case 8: return 'Forever (BG)';
    default: return 'Unknown';
  }
}
async function checkServiceStatus() {
  try {
    const result = await apiGet('/api/music/v1/serviceStatus');
    if (result.ok && result.data.status) {
      setTitleStatus(`[${result.data.status}]`, result.data.status === 'started' ? '#4CAF50' : '#FFA500');
    }
  } catch (error) {
    setTitleStatus('[Error]', '#f44336');
  }
}
document.addEventListener('DOMContentLoaded', () => {
  renderMelodies();
  checkServiceStatus();
});
