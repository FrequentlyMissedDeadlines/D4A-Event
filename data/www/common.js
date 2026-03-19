'use strict';

// ── API helpers ───────────────────────────────────────────────────────────────

function getDebugLogState() {
  if (!window.__k10DebugLogState) {
    window.__k10DebugLogState = { nextId: 1 };
  }
  return window.__k10DebugLogState;
}

function nextDebugLogId() {
  const state = getDebugLogState();
  const id = state.nextId;
  state.nextId += 1;
  return id;
}

function getDebugTimestamp() {
  return new Date().toISOString();
}

function logCommonApi(message, details) {
  const logId = nextDebugLogId();
  const timestamp = getDebugTimestamp();
  if (details !== undefined) {
    console.log(`[common.js][${timestamp}][id:${logId}] ${message}`, details);
    return;
  }
  console.log(`[common.js][${timestamp}][id:${logId}] ${message}`);
}

function logCommonApiError(message, details) {
  const logId = nextDebugLogId();
  const timestamp = getDebugTimestamp();
  if (details !== undefined) {
    console.error(`[common.js][${timestamp}][id:${logId}] ${message}`, details);
    return;
  }
  console.error(`[common.js][${timestamp}][id:${logId}] ${message}`);
}

/**
 * General API call with optional JSON body.
 * @param {string} endpoint
 * @param {string} [method='GET']
 * @param {object|null} [body=null]  Sent as JSON body when provided.
 * @returns {Promise<{ok, status, data}|{ok, error}>}
 */
async function apiCall(endpoint, method = 'GET', body = null) {
  const requestId = nextDebugLogId();
  logCommonApi('apiCall() start', { request_id: requestId, endpoint, method, body });
  const options = { method };
  if (body !== null) {
    options.headers = { 'Content-Type': 'application/json' };
    options.body = JSON.stringify(body);
  }
  try {
    logCommonApi('apiCall() fetch request', { request_id: requestId, endpoint, options });
    const response = await fetch(endpoint, options);
    logCommonApi('apiCall() fetch response received', {
      request_id: requestId,
      endpoint,
      method,
      ok: response.ok,
      status: response.status
    });
    const data = await response.json();
    logCommonApi('apiCall() response json parsed', { request_id: requestId, endpoint, method, data });
    return { ok: response.ok, status: response.status, data };
  } catch (error) {
    logCommonApiError('apiCall() failed', { request_id: requestId, endpoint, method, body, error });
    return { ok: false, error: error.message };
  }
}

/**
 * GET request — convenience wrapper around apiCall.
 * @param {string} url
 */
async function apiGet(url) {
  logCommonApi('apiGet() start', { url });
  return apiCall(url, 'GET');
}

/**
 * POST request with no body — convenience wrapper around apiCall.
 * @param {string} url
 */
async function apiPost(url) {
  logCommonApi('apiPost() start', { url });
  return apiCall(url, 'POST');
}

/**
 * API call where parameters are appended as URL query string.
 * Used when the firmware endpoint reads from query params rather than JSON body.
 * @param {string} endpoint
 * @param {string} [method='GET']
 * @param {object|null} [params=null]
 */
async function apiCallParams(endpoint, method = 'GET', params = null) {
  logCommonApi('apiCallParams() start', { endpoint, method, params });
  let url = endpoint;
  if (params) url += '?' + new URLSearchParams(params).toString();
  logCommonApi('apiCallParams() resolved url', { url, method });
  return apiCall(url, method);
}

// ── Page title status badge ───────────────────────────────────────────────────

/**
 * Update the #titleStatus span shown next to the page <h1>.
 * @param {string} text  - displayed text, e.g. '[started]'
 * @param {string} color - CSS color string
 */
function setTitleStatus(text, color) {
  logCommonApi('setTitleStatus() start', { text, color });
  const el = document.getElementById('titleStatus');
  if (el) { el.textContent = text; el.style.color = color; }
}

// ── Status message (bottom statusbar #page-status) ─────────────────────────

/**
 * Briefly show a message in the bottom statusbar's left slot,
 * then clear it after 3 s.
 * @param {string}  message
 * @param {boolean} [isError=false]
 */
function showStatus(message, isError = false) {
  logCommonApi('showStatus() start', { message, isError });
  console.log(`[STATUS] ${message} ${isError ? '(error)' : ''}`);
  const el = document.getElementById('page-status');
  if (!el) return;
  el.textContent = message;
  el.style.color = isError ? '#ff6b6b' : '#4CAF50';
  clearTimeout(showStatus._t);
  showStatus._t = setTimeout(() => { el.textContent = ''; el.style.color = ''; }, 3000);
}

// ── Collapsible sections ──────────────────────────────────────────────────────

/**
 * Toggle a collapsible section between expanded and collapsed.
 * @param {string} sectionId - id of the .collapsible-body element
 * @param {string} btnId     - id of the .collapse-btn element
 */
function toggleSection(sectionId, btnId) {
  logCommonApi('toggleSection() start', { sectionId, btnId });
  const body = document.getElementById(sectionId);
  const btn  = document.getElementById(btnId);
  const collapsed = body.classList.toggle('collapsed');
  btn.classList.toggle('collapsed', collapsed);
}

// ── HTML escaping ─────────────────────────────────────────────────────────────

/**
 * Escape HTML special characters to prevent XSS.
 * @param {string} text
 * @returns {string}
 */
function escapeHtml(text) {
  logCommonApi('escapeHtml() start', { text });
  const div = document.createElement('div');
  div.textContent = text;
  return div.innerHTML;
}
