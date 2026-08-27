// cyd-busybar web UI. No modules, no build step.

'use strict';

// ── STATE ─────────────────────────────────────────────────────────────────
const STORAGE_KEY = 'cydbusybar_v1_draw';
const SHOT_MS     = 500;    // ~2 fps, matching the panel's own repaint budget
const STATUS_MS   = 2000;
const FETCH_MS    = 4000;

const STATE = {
  status: null,
  themes: [],
  live: true,
  seq: 0,
  scanTimer: null,
};

const $ = (id) => document.getElementById(id);

// ── FORMATTING ────────────────────────────────────────────────────────────
function fmtUptime(s) {
  if (s == null) return '--';
  const d = Math.floor(s / 86400), h = Math.floor((s % 86400) / 3600);
  const m = Math.floor((s % 3600) / 60);
  if (d) return `${d}d ${h}h`;
  if (h) return `${h}h ${m}m`;
  return `${m}m ${s % 60}s`;
}
function fmtBytes(b) {
  if (b == null) return '--';
  return b >= 1024 ? `${(b / 1024).toFixed(1)} KB` : `${b} B`;
}
function hexToInt(h) { return parseInt(String(h).replace('#', ''), 16) || 0; }

// ── NETWORK ───────────────────────────────────────────────────────────────
async function api(path, opts = {}) {
  const ctl = new AbortController();
  const timer = setTimeout(() => ctl.abort(), FETCH_MS);
  try {
    const res = await fetch(path, { ...opts, signal: ctl.signal });
    const text = await res.text();
    let body = null;
    try { body = text ? JSON.parse(text) : null; } catch (e) { throw new Error('Response was not JSON'); }
    if (!res.ok) throw new Error((body && body.error) || `HTTP ${res.status}`);
    return body;
  } finally {
    clearTimeout(timer);
  }
}

function toast(msg, kind) {
  const t = $('toast');
  t.textContent = msg;
  t.className = 'toast' + (kind ? ' ' + kind : '');
  t.hidden = false;
  clearTimeout(toast._t);
  toast._t = setTimeout(() => { t.hidden = true; }, 3200);
}

// ── PREVIEW ───────────────────────────────────────────────────────────────
// The device renders a BMP straight out of the virtual framebuffer, so the
// preview is the panel's real pixels rather than a re-simulation of them.
function refreshShots() {
  if (!STATE.live) return;
  $('shot').src = `/api/screen?t=${++STATE.seq}`;
}

// ── STATUS ────────────────────────────────────────────────────────────────
function renderStatus(s) {
  STATE.status = s;

  const dot = $('conn-dot'), txt = $('conn-text');
  if (!s) {
    dot.className = 'dot bad';
    txt.textContent = 'unreachable';
  } else if (s.network.state === 'online') {
    dot.className = s.time_synced ? 'dot ok live' : 'dot warn live';
    txt.textContent = s.time_synced ? s.network.ip : `${s.network.ip} · time not synced`;
  } else if (s.network.state === 'portal') {
    dot.className = 'dot warn';
    txt.textContent = 'setup portal';
  } else {
    dot.className = 'dot bad';
    txt.textContent = 'connecting';
  }

  if (s) renderAp(s.network.ap);

  if (s) {
    $('owner').textContent = s.panel.owner ? `· ${s.panel.owner} @${s.panel.priority}` : '';

    const rows = [
      ['Firmware',   `${s.name} ${s.version}`],
      ['API',        s.api],
      ['Uptime',     fmtUptime(s.uptime_s)],
      ['Heap free',  fmtBytes(s.heap_free)],
      ['Heap min',   fmtBytes(s.heap_min)],
      ['Backlight',  `${Math.round(s.display.brightness / 255 * 100)}% (${s.display.mode})`],
      ['Ambient',    String(s.display.ldr_raw)],
      ['Elements',   String(s.elements)],
      ['Active tab', s.app],
      ['RSSI',       `${s.network.rssi} dBm`],
    ];
    $('readout').innerHTML = rows
      .map(([k, v]) => `<div><dt>${k}</dt><dd>${v}</dd></div>`).join('');

    const bl = $('s-bright');
    if (document.activeElement !== bl) {
      bl.value = s.display.mode === 'auto' ? 'auto'
               : String(Math.round(s.display.brightness / 255 * 100));
    }
    renderThemes(s.theme);
  }
}

async function pollStatus() {
  try {
    renderStatus(await api('/api/status'));
  } catch (e) {
    renderStatus(null);
  }
}

// ── WI-FI ─────────────────────────────────────────────────────────────────
function fmtMins(ms) {
  if (!ms) return '';
  const m = Math.ceil(ms / 60000);
  return `closes in ${m} min${m === 1 ? '' : 's'}`;
}

function renderAp(ap) {
  if (!ap) return;
  const bar = document.querySelector('.ap-bar');
  const btn = $('btn-ap');
  bar.classList.toggle('on', !!ap.active);

  if (ap.active) {
    $('ap-state').textContent = 'Setup AP is on';
    const bits = [`join "${ap.ssid}", then open http://${ap.ip}`];
    if (ap.clients) bits.push(`${ap.clients} client${ap.clients === 1 ? '' : 's'}`);
    if (ap.fallback) bits.push('no credentials stored, so it cannot be turned off');
    else if (ap.ms_left) bits.push(fmtMins(ap.ms_left));
    $('ap-detail').textContent = bits.join(' · ');
    btn.textContent = 'Stop setup AP';
    btn.disabled = !!ap.fallback;
  } else {
    $('ap-state').textContent = 'Setup AP is off';
    $('ap-detail').textContent = 'Starts an open access point so Wi-Fi can be changed without a reboot.';
    btn.textContent = 'Start setup AP';
    btn.disabled = false;
  }
}

async function toggleAp() {
  const on = STATE.status && STATE.status.network.ap && STATE.status.network.ap.active;
  try {
    const r = await api('/api/wifi/ap', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ enabled: !on }),
    });
    toast(r.active ? `Setup AP up — join "${r.ssid}"` : 'Setup AP stopped', 'ok');
    pollStatus();
  } catch (e) {
    toast(e.message, 'bad');
  }
}

function renderNets(list, scanning) {
  const host = $('nets');
  if (scanning && !list.length) { host.innerHTML = '<li class="note">Scanning…</li>'; return; }
  if (!list.length) { host.innerHTML = '<li class="note">No networks found.</li>'; return; }
  // Strongest first, and one row per SSID: a mesh reports the same name once
  // per radio and the picker only needs the name.
  const seen = new Set();
  host.innerHTML = list
    .slice()
    .sort((a, b) => b.rssi - a.rssi)
    .filter((n) => !seen.has(n.ssid) && seen.add(n.ssid))
    .map((n) => `<li><button type="button" data-ssid="${n.ssid.replace(/"/g, '&quot;')}">
        <span>${n.ssid.replace(/</g, '&lt;')}</span>
        <span class="meta">${n.secure ? '<span class="lock">locked</span> · ' : ''}${n.rssi} dBm</span>
      </button></li>`)
    .join('');
}

async function pollScan() {
  try {
    const r = await api('/api/wifi/scan');
    renderNets(r.networks || [], r.scanning);
    if (!r.scanning) {
      clearInterval(STATE.scanTimer);
      STATE.scanTimer = null;
      $('btn-scan').disabled = false;
    }
  } catch (e) {
    clearInterval(STATE.scanTimer);
    STATE.scanTimer = null;
    $('btn-scan').disabled = false;
    $('nets').innerHTML = `<li class="note">Scan failed: ${e.message}</li>`;
  }
}

async function startScan() {
  $('btn-scan').disabled = true;
  renderNets([], true);
  try {
    await api('/api/wifi/scan', { method: 'POST' });
  } catch (e) { /* the poll below reports the real outcome */ }
  clearInterval(STATE.scanTimer);
  STATE.scanTimer = setInterval(pollScan, 900);
  pollScan();
}

async function reconnect() {
  try {
    await api('/api/wifi/reconnect', { method: 'POST' });
    toast('Reconnecting', 'ok');
  } catch (e) {
    toast(e.message, 'bad');
  }
}

async function forget() {
  try {
    await api('/api/wifi/forget', { method: 'POST' });
    toast('Network forgotten — the setup AP is up', 'ok');
    pollStatus();
  } catch (e) {
    toast(e.message, 'bad');
  }
}

// ── THEMES ────────────────────────────────────────────────────────────────
function renderThemes(active) {
  const host = $('theme-chips');
  if (!STATE.themes.length) { host.textContent = 'No themes found.'; return; }
  host.innerHTML = STATE.themes.map((t) =>
    `<button type="button" class="chip" data-theme="${t}" aria-pressed="${t === active}">${t.replace(/_/g, ' ')}</button>`
  ).join('');
}

async function loadThemes() {
  try {
    const r = await api('/api/themes');
    STATE.themes = r.available || [];
    renderThemes(r.active);
  } catch (e) {
    $('theme-chips').textContent = 'Could not load themes.';
  }
}

// ── DRAW ──────────────────────────────────────────────────────────────────
function buildRequest() {
  const type = $('d-type').value;
  const el = {
    id:      'webui-1',
    type,
    x:       Number($('d-x').value),
    y:       Number($('d-y').value),
    w:       Number($('d-w').value),
    h:       Number($('d-h').value),
    align:   Number($('d-align').value),
    color:   $('d-color').value,
  };
  const timeout = Number($('d-timeout').value);
  if (timeout > 0) el.timeout_ms = timeout;

  if (type === 'text') {
    el.text   = $('d-text').value;
    el.font   = $('d-font').value;
    el.scale  = Number($('d-scale').value);
    el.scroll = $('d-scroll').checked;
  } else if (type === 'countdown') {
    const v = $('d-target').value;
    if (!v) throw new Error('Pick a countdown target');
    el.target = Math.floor(new Date(v).getTime() / 1000);
    el.font   = $('d-font').value;
    el.scale  = Number($('d-scale').value);
  }

  const req = {
    application_name: $('d-app').value || 'webui',
    priority: Number($('d-prio').value),
    elements: [el],
  };
  const led = $('d-led').value;
  if (hexToInt(led)) req.led_notification_color = led;
  return req;
}

async function doDraw(ev) {
  ev.preventDefault();
  const btn = ev.submitter;
  if (btn) btn.disabled = true;
  try {
    const req = buildRequest();
    localStorage.setItem(STORAGE_KEY, JSON.stringify(req));
    await api('/api/display/draw', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(req),
    });
    toast('Drawn', 'ok');
    refreshShots();
  } catch (e) {
    toast(e.message, 'bad');
  } finally {
    if (btn) btn.disabled = false;
  }
}

async function doClear(app) {
  try {
    const q = app ? `?application_name=${encodeURIComponent(app)}` : '';
    await api('/api/display/draw' + q, { method: 'DELETE' });
    toast(app ? `Cleared ${app}` : 'Cleared all', 'ok');
    refreshShots();
  } catch (e) {
    toast(e.message, 'bad');
  }
}

// ── DEVICE ────────────────────────────────────────────────────────────────
async function saveDevice() {
  try {
    const bl = $('s-bright').value;
    await api('/api/display/brightness', {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(bl === 'auto' ? { auto: true } : { value: Number(bl) }),
    });
    await api('/api/time', {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ clock24: $('s-clock').value === '24', tz: $('s-tz').value }),
    });
    toast('Applied', 'ok');
    pollStatus();
  } catch (e) {
    toast(e.message, 'bad');
  }
}

async function saveWifi() {
  const ssid = $('w-ssid').value.trim();
  if (!ssid) { toast('SSID is required', 'bad'); return; }
  try {
    await api('/api/wifi', {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ ssid, pass: $('w-pass').value }),
    });
    toast('Saved — the device is rebooting', 'ok');
  } catch (e) {
    toast(e.message, 'bad');
  }
}

// ── TYPE-DEPENDENT FIELDS ─────────────────────────────────────────────────
function syncFields() {
  const type = $('d-type').value;
  const show = (id, on) => { $(id).closest('label').hidden = !on; };
  show('d-text',   type === 'text');
  show('d-scroll', type === 'text');
  show('d-font',   type !== 'rectangle');
  show('d-scale',  type !== 'rectangle');
  show('d-target', type === 'countdown');
  show('d-w',      true);
  show('d-h',      type === 'rectangle');
}

// ── INIT ──────────────────────────────────────────────────────────────────
function restore() {
  try {
    const saved = JSON.parse(localStorage.getItem(STORAGE_KEY) || 'null');
    if (!saved || !saved.elements || !saved.elements[0]) return;
    const e = saved.elements[0];
    $('d-app').value     = saved.application_name || 'webui';
    $('d-prio').value    = saved.priority || 50;
    $('d-type').value    = e.type || 'text';
    if (e.text != null)  $('d-text').value = e.text;
    if (e.color)         $('d-color').value = e.color;
    ['x', 'y', 'w', 'h'].forEach((k) => { if (e[k] != null) $('d-' + k).value = e[k]; });
    if (e.align != null) $('d-align').value = String(e.align);
    if (e.font)          $('d-font').value = e.font;
    if (e.scale)         $('d-scale').value = e.scale;
    $('d-scroll').checked = !!e.scroll;
    $('d-timeout').value  = e.timeout_ms || 0;
  } catch (err) { /* a corrupt cache is not worth a visible error */ }
}

async function loadTz() {
  try {
    const t = await api('/api/time');
    $('tzlist').innerHTML = (t.tz_common || []).map((z) => `<option value="${z}">`).join('');
    if (document.activeElement !== $('s-tz')) $('s-tz').value = t.tz || '';
    $('s-clock').value = t.clock24 ? '24' : '12';
  } catch (e) { /* the readout already reports unreachability */ }
}

function init() {
  restore();
  syncFields();

  $('d-type').addEventListener('change', syncFields);
  $('draw-form').addEventListener('submit', doDraw);
  $('btn-clear-app').addEventListener('click', () => doClear($('d-app').value || 'webui'));
  $('btn-clear-all').addEventListener('click', () => doClear(null));
  $('btn-save-device').addEventListener('click', saveDevice);
  $('btn-wifi').addEventListener('click', saveWifi);
  $('btn-ap').addEventListener('click', toggleAp);
  $('btn-reconnect').addEventListener('click', reconnect);
  $('btn-forget').addEventListener('click', forget);
  $('btn-scan').addEventListener('click', startScan);
  $('nets').addEventListener('click', (ev) => {
    const b = ev.target.closest('[data-ssid]');
    if (!b) return;
    $('w-ssid').value = b.dataset.ssid;
    $('w-pass').focus();
  });
  $('btn-sync').addEventListener('click', async () => {
    try { await api('/api/time', { method: 'PUT', headers: { 'Content-Type': 'application/json' }, body: '{}' });
          toast('Re-syncing', 'ok'); } catch (e) { toast(e.message, 'bad'); }
  });
  $('btn-key').addEventListener('click', async (ev) => {
    try { await api(`/api/input?key=${ev.target.dataset.key}`, { method: 'POST' });
          refreshShots(); } catch (e) { toast(e.message, 'bad'); }
  });

  $('theme-chips').addEventListener('click', async (ev) => {
    const b = ev.target.closest('[data-theme]');
    if (!b) return;
    try {
      await api('/api/themes', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ name: b.dataset.theme }),
      });
      renderThemes(b.dataset.theme);
      refreshShots();
    } catch (e) { toast(e.message, 'bad'); }
  });

  $('live').addEventListener('change', (ev) => {
    STATE.live = ev.target.checked;
    if (STATE.live) refreshShots();
  });

  loadThemes();
  loadTz();
  pollStatus();
  refreshShots();
  setInterval(refreshShots, SHOT_MS);
  setInterval(pollStatus, STATUS_MS);
}

document.addEventListener('DOMContentLoaded', init);
