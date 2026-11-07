#pragma once

#include <Arduino.h>

static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Temple Lights</title>
<style>
  :root { --bg: #0f172a; --card: #1e293b; --text: #e2e8f0; --muted: #94a3b8;
          --accent: #fbbf24; --ok: #22c55e; --err: #ef4444; }
  * { box-sizing: border-box; }
  body { margin: 0; padding: 16px; background: var(--bg); color: var(--text);
         font-family: system-ui, -apple-system, "Segoe UI", Roboto, sans-serif; }
  .wrap { max-width: 460px; margin: 0 auto; }
  header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 16px; }
  h1 { font-size: 1.3em; margin: 0; }
  .badge { font-size: .8em; padding: 4px 10px; border-radius: 999px; background: #334155; color: var(--muted); }
  .badge.online { background: rgba(34,197,94,.15); color: var(--ok); }
  .badge.offline { background: rgba(239,68,68,.15); color: var(--err); }
  .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
  .card { background: var(--card); border-radius: 16px; padding: 16px; text-align: center;
          transition: box-shadow .2s; }
  .card.on { box-shadow: 0 0 0 2px var(--accent), 0 0 24px rgba(251,191,36,.35); }
  .card h3 { margin: 0 0 12px; font-size: 1em; }
  .bulb { width: 56px; height: 56px; margin: 0 auto 8px; display: block; color: #475569;
          transition: color .2s, filter .2s; }
  .card.on .bulb { color: var(--accent); filter: drop-shadow(0 0 8px rgba(251,191,36,.7)); }
  .toggle { position: relative; width: 64px; height: 34px; border-radius: 17px; border: 0; padding: 0;
            background: #475569; cursor: pointer; transition: background .2s; }
  .toggle::after { content: ''; position: absolute; top: 4px; left: 4px; width: 26px; height: 26px;
                   border-radius: 50%; background: #fff; transition: transform .2s; }
  .card.on .toggle { background: var(--accent); }
  .card.on .toggle::after { transform: translateX(30px); }
  .sw { margin-top: 14px; font-size: .85em; color: var(--muted);
        display: flex; align-items: center; justify-content: center; gap: 6px; }
  .dot { width: 10px; height: 10px; border-radius: 50%; background: #475569; }
  .dot.on { background: var(--ok); box-shadow: 0 0 6px var(--ok); }
  .panel { background: var(--card); border-radius: 16px; padding: 16px; margin-top: 16px; }
  .panel h2 { margin: 0 0 10px; font-size: 1em; }
  .row { display: flex; justify-content: space-between; align-items: center; margin: 10px 0; gap: 8px; }
  .row.buttons { justify-content: flex-end; }
  .btn { background: #334155; color: var(--text); border: 0; border-radius: 10px;
         padding: 10px 16px; font-size: .95em; cursor: pointer; }
  .btn.primary { background: var(--accent); color: #111; font-weight: 600; }
  input[type=time], input[type=datetime-local] { background: var(--bg); color: var(--text);
         border: 1px solid #334155; border-radius: 8px; padding: 8px; font-size: 1em; color-scheme: dark; }
  input[type=checkbox] { width: 20px; height: 20px; accent-color: var(--accent); }
  #info { color: var(--ok); min-height: 1.3em; margin-top: 12px; text-align: center; }
  #status { color: var(--err); min-height: 1.3em; text-align: center; }
  footer { text-align: center; margin-top: 8px; font-size: .85em; }
  footer a { color: var(--muted); }
</style>
</head>
<body>
<div class="wrap">
<header>
  <h1>Temple Lights</h1>
  <span class="badge" id="badge">Connecting...</span>
</header>
<div class="grid" id="lights"></div>
<div class="panel">
  <h2>Alarm</h2>
  <div class="row"><span>Lights ON at</span><input type="time" id="alarmOn"></div>
  <div class="row"><span>Lights OFF at</span><input type="time" id="alarmOff"></div>
  <div class="row"><span>Enabled</span><input type="checkbox" id="alarmEn"></div>
  <div class="row buttons">
    <button class="btn" onclick="loadAlarm()">Read</button>
    <button class="btn primary" onclick="saveAlarm()">Save</button>
  </div>
</div>
<div class="panel">
  <h2>RTC time</h2>
  <div class="row"><input type="datetime-local" step="1" id="rtcTime"></div>
  <div class="row buttons">
    <button class="btn" onclick="useNow()">Now</button>
    <button class="btn primary" onclick="setTime()">Set</button>
  </div>
</div>
<div id="info"></div>
<div id="status"></div>
<footer><a href="/settings">Settings</a> &middot; <a href="/update">Firmware update</a></footer>
</div>
<script>
const COUNT = 4;
const BULB = '<svg class="bulb" viewBox="0 0 24 24" fill="currentColor"><path d="M9 21c0 .55.45 1 1 1h4c.55 0 1-.45 1-1v-1H9v1zm3-19C8.14 2 5 5.14 5 9c0 2.38 1.19 4.47 3 5.74V17c0 .55.45 1 1 1h6c.55 0 1-.45 1-1v-2.26c1.81-1.27 3-3.36 3-5.74 0-3.86-3.14-7-7-7z"/></svg>';
const grid = document.getElementById('lights');
const statusEl = document.getElementById('status');
const badge = document.getElementById('badge');
const lightOn = [false, false, false, false];

for (let i = 0; i < COUNT; i++) {
  const card = document.createElement('div');
  card.className = 'card';
  card.id = 'c' + i;
  card.innerHTML = BULB + '<h3>Light ' + (i + 1) + '</h3>' +
    '<button class="toggle" id="t' + i + '" aria-label="Toggle light ' + (i + 1) + '"></button>' +
    '<div class="sw"><span class="dot" id="d' + i + '"></span><span id="s' + i + '">Switch: --</span></div>';
  grid.appendChild(card);
  card.querySelector('.toggle').onclick = () => toggle(i);
}

function setBadge(cls, text) {
  badge.className = 'badge ' + cls;
  badge.textContent = text;
}

function setLight(i, on) {
  lightOn[i] = on;
  document.getElementById('c' + i).classList.toggle('on', on);
}

function render(d) {
  d.lights.forEach((on, i) => setLight(i, on));
  for (let i = 0; i < COUNT; i++) {
    const sw = d.switches ? d.switches[i] : null;
    document.getElementById('d' + i).className = 'dot' + (sw ? ' on' : '');
    document.getElementById('s' + i).textContent = 'Switch: ' + (sw === null ? '--' : sw ? 'ON' : 'OFF');
  }
  if (d.online) setBadge('online', 'Controller online');
  else setBadge('offline', 'Controller offline');
}

async function refresh() {
  try {
    const r = await fetch('/api/status');
    render(await r.json());
    statusEl.textContent = '';
  } catch (e) {
    setBadge('offline', 'No connection');
  }
}

async function toggle(i) {
  const next = lightOn[i] ? 0 : 1;
  setLight(i, !!next);
  try {
    await fetch('/api/light?id=' + (i + 1) + '&state=' + next, { method: 'POST' });
    statusEl.textContent = '';
    setTimeout(refresh, 800);
  } catch (e) {
    setLight(i, !next);
    statusEl.textContent = 'Command failed';
  }
}

const infoEl = document.getElementById('info');

async function api(path, method) {
  const r = await fetch(path, { method: method });
  const j = await r.json();
  if (!r.ok) throw new Error(j.error || 'Request failed');
  return j;
}

function showAlarm(a) {
  document.getElementById('alarmOn').value = a.on;
  document.getElementById('alarmOff').value = a.off;
  document.getElementById('alarmEn').checked = a.enabled;
}

async function loadAlarm() {
  try {
    showAlarm(await api('/api/alarm', 'GET'));
    statusEl.textContent = '';
    infoEl.textContent = 'Alarm read';
  } catch (e) {
    infoEl.textContent = '';
    statusEl.textContent = 'Alarm: ' + e.message;
  }
}

async function saveAlarm() {
  const on = document.getElementById('alarmOn').value;
  const off = document.getElementById('alarmOff').value;
  if (!on || !off) {
    statusEl.textContent = 'Enter both alarm times';
    return;
  }
  const en = document.getElementById('alarmEn').checked ? 1 : 0;
  try {
    showAlarm(await api('/api/alarm?on=' + on + '&off=' + off + '&enabled=' + en, 'POST'));
    statusEl.textContent = '';
    infoEl.textContent = 'Alarm saved';
  } catch (e) {
    infoEl.textContent = '';
    statusEl.textContent = 'Alarm: ' + e.message;
  }
}

function useNow() {
  const d = new Date();
  const p = n => String(n).padStart(2, '0');
  document.getElementById('rtcTime').value =
    d.getFullYear() + '-' + p(d.getMonth() + 1) + '-' + p(d.getDate()) + 'T' +
    p(d.getHours()) + ':' + p(d.getMinutes()) + ':' + p(d.getSeconds());
}

async function setTime() {
  let v = document.getElementById('rtcTime').value;
  if (!v) {
    statusEl.textContent = 'Pick a date and time';
    return;
  }
  if (v.length === 16) v += ':00';
  v = v.replace('T', ' ');
  try {
    const j = await api('/api/time?value=' + encodeURIComponent(v), 'POST');
    statusEl.textContent = '';
    infoEl.textContent = 'RTC set: ' + j.rtc;
  } catch (e) {
    infoEl.textContent = '';
    statusEl.textContent = 'RTC: ' + e.message;
  }
}

refresh();
setInterval(refresh, 3000);
loadAlarm();
</script>
</body>
</html>
)rawliteral";
