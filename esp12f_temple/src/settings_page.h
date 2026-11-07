#pragma once

#include <Arduino.h>

static const char SETTINGS_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Temple Lights - Settings</title>
<style>
  :root { --bg: #0f172a; --card: #1e293b; --text: #e2e8f0; --muted: #94a3b8;
          --accent: #fbbf24; --ok: #22c55e; --err: #ef4444; }
  * { box-sizing: border-box; }
  body { margin: 0; padding: 16px; background: var(--bg); color: var(--text);
         font-family: system-ui, -apple-system, "Segoe UI", Roboto, sans-serif; }
  .wrap { max-width: 460px; margin: 0 auto; }
  h1 { font-size: 1.3em; margin: 0 0 16px; }
  .panel { background: var(--card); border-radius: 16px; padding: 16px; margin-top: 16px; }
  .panel h2 { margin: 0 0 10px; font-size: 1em; }
  label { display: block; margin: 12px 0 4px; font-size: .85em; color: var(--muted); }
  input { width: 100%; background: var(--bg); color: var(--text); border: 1px solid #334155;
          border-radius: 8px; padding: 10px; font-size: 1em; }
  .btn { background: var(--accent); color: #111; border: 0; border-radius: 10px; padding: 12px 20px;
         font-size: 1em; font-weight: 600; cursor: pointer; margin-top: 16px; width: 100%; }
  #info { color: var(--ok); min-height: 1.3em; margin-top: 12px; text-align: center; }
  #status { color: var(--err); min-height: 1.3em; text-align: center; }
  .hint { font-size: .8em; color: var(--muted); margin-top: 12px; }
  footer { text-align: center; margin-top: 12px; font-size: .85em; }
  footer a { color: var(--muted); }
</style>
</head>
<body>
<div class="wrap">
<h1>Settings</h1>
<form id="form" autocomplete="off">
  <div class="panel">
    <h2>Wi-Fi network</h2>
    <label for="wifiSsid">SSID</label>
    <input id="wifiSsid" maxlength="32" required>
    <label for="wifiPass">Password (blank = keep current)</label>
    <input id="wifiPass" type="password" maxlength="63" placeholder="unchanged">
  </div>
  <div class="panel">
    <h2>Hotspot (used when Wi-Fi fails)</h2>
    <label for="apSsid">SSID</label>
    <input id="apSsid" maxlength="32" required>
    <label for="apPass">Password, 8+ characters (blank = keep current)</label>
    <input id="apPass" type="password" maxlength="63" placeholder="unchanged">
  </div>
  <div class="panel">
    <h2>Admin login (settings and firmware update)</h2>
    <label for="otaUser">Username</label>
    <input id="otaUser" maxlength="31" required>
    <label for="otaPass">Password, 8+ characters (blank = keep current)</label>
    <input id="otaPass" type="password" maxlength="31" placeholder="unchanged">
  </div>
  <button class="btn" type="submit">Save and restart</button>
  <div class="hint">The device restarts after saving. If you changed the Wi-Fi network, reconnect to the new address.</div>
</form>
<div id="info"></div>
<div id="status"></div>
<footer><a href="/">Back to lights</a></footer>
</div>
<script>
const ids = ['wifiSsid', 'wifiPass', 'apSsid', 'apPass', 'otaUser', 'otaPass'];
const infoEl = document.getElementById('info');
const statusEl = document.getElementById('status');

async function load() {
  try {
    const r = await fetch('/api/settings');
    const s = await r.json();
    document.getElementById('wifiSsid').value = s.wifiSsid;
    document.getElementById('apSsid').value = s.apSsid;
    document.getElementById('otaUser').value = s.otaUser;
  } catch (e) {
    statusEl.textContent = 'Could not load settings';
  }
}

document.getElementById('form').onsubmit = async ev => {
  ev.preventDefault();
  const body = new URLSearchParams();
  ids.forEach(id => body.append(id, document.getElementById(id).value));
  infoEl.textContent = '';
  statusEl.textContent = '';
  try {
    const r = await fetch('/api/settings', { method: 'POST', body: body });
    const j = await r.json();
    if (!r.ok) throw new Error(j.error || 'Save failed');
    infoEl.textContent = 'Saved. The device is restarting...';
  } catch (e) {
    statusEl.textContent = e.message;
  }
};

load();
</script>
</body>
</html>
)rawliteral";
