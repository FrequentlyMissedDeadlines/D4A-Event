'use strict';

// ── API helpers ───────────────────────────────────────────────────────────────

/**
 * General API call with optional JSON body.
 * @param {string} endpoint
 * @param {string} [method='GET']
 * @param {object|null} [body=null]  Sent as JSON body when provided.
 * @returns {Promise<{ok, status, data}|{ok, error}>}
 */
async function apiCall(endpoint, method = 'GET', body = null) {
  const options = { method };
  if (body !== null) {
    options.headers = { 'Content-Type': 'application/json' };
    options.body = JSON.stringify(body);
  }
  try {
    const response = await fetch(endpoint, options);
    return { ok: response.ok, status: response.status, data: await response.json() };
  } catch (error) {
    console.error('API call failed:', error);
    return { ok: false, error: error.message };
  }
}

/**
 * GET request — convenience wrapper around apiCall.
 * @param {string} url
 */
async function apiGet(url) {
  return apiCall(url, 'GET');
}

/**
 * POST request with no body — convenience wrapper around apiCall.
 * @param {string} url
 */
async function apiPost(url) {
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
  let url = endpoint;
  if (params) url += '?' + new URLSearchParams(params).toString();
  return apiCall(url, method);
}

// ── Page title status badge ───────────────────────────────────────────────────

/**
 * Update the #titleStatus span shown next to the page <h1>.
 * @param {string} text  - displayed text, e.g. '[started]'
 * @param {string} color - CSS color string
 */
function setTitleStatus(text, color) {
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
  const div = document.createElement('div');
  div.textContent = text;
  return div.innerHTML;
}
