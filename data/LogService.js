let autoRefreshInterval = null;
const activeFilters = new Set(['TRACE', 'DEBUG', 'INFO', 'WARNING', 'ERROR']);
const collapsedSections = new Set();
function toggleFilter(level) {
  const btn = document.querySelector(`.filter-btn[data-level="${level}"]`);
  if (activeFilters.has(level)) {
    activeFilters.delete(level);
    btn.classList.remove('active');
  } else {
    activeFilters.add(level);
    btn.classList.add('active');
  }
  refreshLogs();
}
function shouldShowEntry(level) {
  return activeFilters.has(level);
}
function toggleAutoRefresh() {
  const checkbox = document.getElementById('autoRefresh');
  if (checkbox.checked) {
    autoRefreshInterval = setInterval(refreshLogs, 500);
  } else {
    if (autoRefreshInterval) {
      clearInterval(autoRefreshInterval);
      autoRefreshInterval = null;
    }
  }
}
function formatTimestamp(ms) {
  if (ms === undefined || ms === null) return '';
  const s = Math.floor(ms / 1000);
  const m = ms % 1000;
  return `+${s}.${String(m).padStart(3, '0')}s`;
}
function createLogEntry(entry) {
  if (!shouldShowEntry(entry.level)) {
    return null;
  }
  const div = document.createElement('div');
  div.className = `log-entry ${entry.level}`;
  const levelSpan = document.createElement('span');
  levelSpan.className = 'log-level';
  levelSpan.textContent = entry.level;
  const messageSpan = document.createElement('span');
  messageSpan.className = 'log-message';
  messageSpan.textContent = entry.message;
  const tsSpan = document.createElement('span');
  tsSpan.className = 'log-timestamp';
  tsSpan.textContent = formatTimestamp(entry.timestamp_ms);
  tsSpan.title = `${entry.timestamp_ms} ms since boot`;
  div.appendChild(levelSpan);
  div.appendChild(tsSpan);
  div.appendChild(messageSpan);
  return div;
}
function renderLogSection(title, logs, id) {
  const section = document.createElement('div');
  section.className = 'log-section';
  const header = document.createElement('div');
  header.className = 'log-header';
  const titleDiv = document.createElement('div');
  titleDiv.className = 'log-title';
  const collapseBtn = document.createElement('button');
  collapseBtn.className = 'collapse-btn';
  collapseBtn.innerHTML = '▼';
  collapseBtn.setAttribute('aria-label', 'Toggle section');
  const titleText = document.createElement('span');
  titleText.textContent = title;
  titleDiv.appendChild(collapseBtn);
  titleDiv.appendChild(titleText);
  const visibleCount = logs.filter(log => shouldShowEntry(log.level)).length;
  const countDiv = document.createElement('div');
  countDiv.className = 'log-count';
  countDiv.textContent = `${visibleCount} / ${logs.length} entries`;
  header.appendChild(titleDiv);
  header.appendChild(countDiv);
  const entries = document.createElement('div');
  entries.className = 'log-entries';
  entries.id = id;
  if (visibleCount === 0) {
    const empty = document.createElement('div');
    empty.className = 'empty-log';
    empty.textContent = logs.length === 0 ? 'No log entries' : 'All entries filtered out';
    entries.appendChild(empty);
  } else {
    logs.forEach(log => {
      const entry = createLogEntry(log);
      if (entry) {
        entries.appendChild(entry);
      }
    });
  }
  section.appendChild(header);
  section.appendChild(entries);
  if (collapsedSections.has(id)) {
    entries.classList.add('collapsed');
    collapseBtn.classList.add('collapsed');
  }
  header.addEventListener('click', function() {
    const isCollapsed = entries.classList.toggle('collapsed');
    collapseBtn.classList.toggle('collapsed');
    if (isCollapsed) {
      collapsedSections.add(id);
    } else {
      collapsedSections.delete(id);
    }
  });
  return section;
}
async function refreshLogs() {
  try {
    const response = await fetch('/api/logs/v1/all');
    if (!response.ok) {
      throw new Error(`HTTP error! status: ${response.status}`);
    }
    const data = await response.json();
    const container = document.getElementById('logContainer');
    container.innerHTML = '';
    if (data.debug) {
      container.appendChild(renderLogSection('Debug Logger', data.debug, 'debugLogs'));
    }
    if (data.app_info) {
      container.appendChild(renderLogSection('App Info Logger', data.app_info, 'appInfoLogs'));
    }
    if (data.esp) {
      container.appendChild(renderLogSection('ESP-IDF Logger', data.esp, 'espLogs'));
    }
    document.querySelectorAll('.log-entries').forEach(el => {
      el.scrollTop = el.scrollHeight;
    });
    const totalLogs = (data.debug?.length || 0) + (data.app_info?.length || 0) + (data.esp?.length || 0);
    setTitleStatus(`[${totalLogs} entries]`, '#4CAF50');
  } catch (error) {
    console.error('Failed to fetch logs:', error);
    const container = document.getElementById('logContainer');
    container.innerHTML = `
      <div class="error-message">
        <strong>Error loading logs:</strong><br>
        ${error.message}
      </div>
    `;
  }
}
async function exportLogs() {
  try {
    const response = await fetch('/api/logs/v1/all');
    if (!response.ok) {
      throw new Error(`HTTP error! status: ${response.status}`);
    }
    const data = await response.json();
    const exportData = {
      timestamp: new Date().toISOString(),
      debug: data.debug || [],
      app_info: data.app_info || [],
      esp: data.esp || []
    };
    const blob = new Blob([JSON.stringify(exportData, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `k10-logs-${Date.now()}.json`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  } catch (error) {
    console.error('Failed to export logs:', error);
    showStatus('❌ Failed to export logs: ' + error.message + (error.status ? ` - HTTP ${error.status}` : ''), true);
  }
}
refreshLogs();
