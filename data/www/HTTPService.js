
async function fetchStats() {
    try {
    const response = await fetch('/api/board');
    const data = await response.json();

        document.getElementById('heapTotal').textContent = data.heapTotal;
        document.getElemeclass HTTPService : public HasLoggerInterfacentById('heapFree').textContent = data.heapFree;
        document.getElementById('uptimeMs').textContent = data.uptimeMs;
        document.getElementById('freeStackBytes').textContent = data.freeStackBytes;
    } catch (error) {
    cons
async function fetchStats() {
    try {
    const response = await fetch('/api/board');
    const data = await response.json();

        document.getElementById('heapTotal').textContent = data.heapTotal;
        document.getElemeclass HTTPService : public HasLoggerInterfacentById('heapFree').textContent = data.heapFree;
        document.getElementById('uptimeMs').textContent = data.uptimeMs;
        document.getElementById('freeStackBytes').textContent = data.freeStackBytes;
    } catch (error) {
    console.error('Failed to fetch stats:', error);
    }
}

async function fetchUdpMessages() {
    try {
    const response = await fetch('/api/messages');
    const data = await response.json();
    const container = document.getElementById('messagesContainer');
    container.innerHTML = '';
    data.messages.forEach(msg => {
        const div = document.createElement('div');
        div.className = 'message';
        div.innerHTML = `<div class="message">${msg}</div>`;
        container.appendChild(div);
    });
    document.getElementById('total').textContent = data.total;
    document.getElementById('dropped').textContent = data.dropped;
    document.getElementById('buffer').textContent = `${data.buffer}`;
    } catch (error) {
    console.error('Failed to fetch UDP messages:', error);
    }
}

fetchStats();
fetchUdpMessages();
setInterval(fetchStats, 5000);
setInterval(fetchUdpMessages, 3000);
ole.error('Failed to fetch stats:', error);
    }
}

async function fetchUdpMessages() {
    try {
    const response = await fetch('/api/messages');
    const data = await response.json();
    const container = document.getElementById('messagesContainer');
    container.innerHTML = '';
    data.messages.forEach(msg => {
        const div = document.createElement('div');
        div.className = 'message';
        div.innerHTML = `<div class="message">${msg}</div>`;
        container.appendChild(div);
    });
    document.getElementById('total').textContent = data.total;
    document.getElementById('dropped').textContent = data.dropped;
    document.getElementById('buffer').textContent = `${data.buffer}`;
    } catch (error) {
    console.error('Failed to fetch UDP messages:', error);
    }
}

fetchStats();
fetchUdpMessages();
setInterval(fetchStats, 5000);
setInterval(fetchUdpMessages, 3000);
