const STATE = {
  tab: 'draw',
  token: localStorage.getItem('cydbusybar_v1_token') || '',
};

function headers() {
  const h = { 'Content-Type': 'application/json' };
  if (STATE.token) h['X-API-Token'] = STATE.token;
  return h;
}

async function api(path, opts) {
  const r = await fetch(path, { ...opts, headers: { ...headers(), ...(opts && opts.headers) } });
  const ct = r.headers.get('content-type') || '';
  if (ct.includes('application/json')) {
    const j = await r.json();
    if (!r.ok) throw new Error(j.error || r.statusText);
    return j;
  }
  if (!r.ok) throw new Error(r.statusText);
  return r;
}

function hex8(hex) {
  const h = hex.replace('#', '');
  return '#' + (h.length === 6 ? h + 'FF' : h);
}

function paintBmp(canvas, blob) {
  const url = URL.createObjectURL(blob);
  const img = new Image();
  img.onload = () => {
    const ctx = canvas.getContext('2d');
    ctx.imageSmoothingEnabled = false;
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.drawImage(img, 0, 0, canvas.width, canvas.height);
    URL.revokeObjectURL(url);
  };
  img.src = url;
}

async function pollScreens() {
  try {
    const f = await fetch('/api/screen?display=0', { headers: headers() });
    if (f.ok) paintBmp(document.getElementById('front'), await f.blob());
    const b = await fetch('/api/screen?display=1', { headers: headers() });
    if (b.ok) paintBmp(document.getElementById('back'), await b.blob());
    document.getElementById('preview-status').textContent = 'Live';
  } catch (e) {
    document.getElementById('preview-status').textContent = 'Preview failed: ' + e.message;
  }
}

function switchTab(id) {
  STATE.tab = id;
  document.querySelectorAll('.tab').forEach((t) => t.classList.toggle('is-on', t.dataset.tab === id));
  document.querySelectorAll('.panel').forEach((p) => {
    p.classList.toggle('is-hidden', p.id !== 'panel-' + id);
  });
}

document.querySelectorAll('.tab').forEach((t) => {
  t.addEventListener('click', () => switchTab(t.dataset.tab));
});

document.getElementById('draw-form').addEventListener('submit', async (ev) => {
  ev.preventDefault();
  const fd = new FormData(ev.target);
  const type = fd.get('type');
  const el = {
    id: 'web1',
    type,
    x: Number(fd.get('x')),
    y: Number(fd.get('y')),
    display: fd.get('display'),
    align: fd.get('align'),
    timeout: Number(fd.get('timeout')) || 0,
  };
  if (type === 'text') {
    el.text = fd.get('text');
    el.font = fd.get('font');
    el.color = hex8(fd.get('color'));
    if (Number(fd.get('width')) > 0) {
      el.width = Number(fd.get('width'));
      el.scroll_rate = Number(fd.get('scroll_rate'));
      el.scroll_start_delay = 400;
      el.scroll_repeat_delay = 1200;
    }
  } else if (type === 'countdown') {
    el.timestamp = String(fd.get('timestamp') || Math.floor(Date.now() / 1000) + 300);
    el.direction = fd.get('direction');
    el.show_hours = 'when_non_zero';
    el.color = hex8(fd.get('color'));
  } else if (type === 'rectangle') {
    el.width = 40;
    el.height = 12;
    el.fill = 'solid';
    el.fill_colors = [hex8(fd.get('color'))];
    el.border_width = 1;
    el.border_color = '#FFFFFFFF';
  } else if (type === 'xpmbitmap') {
    el.data = '! XPM2\n8 8 2 1\n. c #000000\nx c #F4620E\n........\n..xxxx..\n.xxxxxx.\nxxxxxxxx\nxxxxxxxx\n.xxxxxx.\n..xxxx..\n........';
  }
  const body = {
    application_name: fd.get('application_name'),
    priority: Number(fd.get('priority')),
    led_notification_color: hex8(fd.get('led')),
    elements: [el],
  };
  const msg = document.getElementById('draw-msg');
  try {
    await api('/api/display/draw', { method: 'POST', body: JSON.stringify(body) });
    msg.textContent = 'Drawn.';
  } catch (e) {
    msg.textContent = e.message;
  }
});

document.getElementById('btn-clear').addEventListener('click', async () => {
  const app = document.querySelector('[name=application_name]').value;
  try {
    await api('/api/display/draw?application_name=' + encodeURIComponent(app), {
      method: 'DELETE',
      body: JSON.stringify({ application_name: app }),
    });
    document.getElementById('draw-msg').textContent = 'Cleared.';
  } catch (e) {
    document.getElementById('draw-msg').textContent = e.message;
  }
});

async function loadThemes() {
  const data = await api('/api/themes');
  const grid = document.getElementById('theme-grid');
  grid.innerHTML = '';
  data.themes.forEach((t) => {
    const b = document.createElement('button');
    b.type = 'button';
    b.className = 'theme-card' + (t.id === data.current ? ' is-on' : '');
    b.innerHTML = '<strong>' + t.label + '</strong><div class="hint">' + t.id + '</div>';
    b.addEventListener('click', async () => {
      await api('/api/theme?name=' + encodeURIComponent(t.id), { method: 'POST' });
      loadThemes();
    });
    grid.appendChild(b);
  });
}

async function loadDevice() {
  const [cfg, tz, st] = await Promise.all([
    api('/api/config'),
    api('/api/time/tzlist'),
    api('/api/status'),
  ]);
  document.getElementById('br-mode').value = cfg.brightness_mode;
  document.getElementById('br-val').value = cfg.brightness;
  document.getElementById('hour12').value = String(cfg.hour12);
  const sel = document.getElementById('tz');
  sel.innerHTML = '';
  tz.list.forEach((z) => {
    const o = document.createElement('option');
    o.value = z.name;
    o.textContent = z.name + ' ' + z.offset;
    if (z.name === cfg.tz_name) o.selected = true;
    sel.appendChild(o);
  });
  document.getElementById('status-dump').textContent = JSON.stringify(st, null, 2);
  document.getElementById('conn-line').textContent =
    (cfg.connected ? cfg.ip : 'offline') + '  ·  ' + (cfg.wifi_ssid || 'no wifi');
}

document.getElementById('device-form').addEventListener('submit', async (ev) => {
  ev.preventDefault();
  const fd = new FormData(ev.target);
  const mode = fd.get('brightness_mode');
  const val = mode === 'auto' ? 'auto' : String(fd.get('brightness'));
  await api('/api/display/brightness?value=' + encodeURIComponent(val), { method: 'POST' });
  const body = {
    hour12: fd.get('hour12') === 'true',
    tz_name: fd.get('tz_name'),
  };
  if (fd.get('api_token')) {
    body.api_token = fd.get('api_token');
    STATE.token = fd.get('api_token');
    localStorage.setItem('cydbusybar_v1_token', STATE.token);
  }
  await api('/api/config', { method: 'PUT', body: JSON.stringify(body) });
  loadDevice();
});

pollScreens();
setInterval(pollScreens, 500);
loadThemes().catch(() => {});
loadDevice().catch((e) => {
  document.getElementById('conn-line').textContent = e.message;
});
