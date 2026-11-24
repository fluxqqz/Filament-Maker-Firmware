#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <Arduino.h>

// Web dashboard HTML / CSS / JS served to clients at "/"
// Layout follows the material path: Cutter (stepper 2) -> Hot end (stages 1/2) -> Puller (stepper 1).
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>MOUNTOYA | UPCYCLE Controller</title>
<style>
:root {
  --navy-dark: #0B3658;
  --navy-hover: #164770;
  --navy-muted: #40637D;
  --navy-faint: #E8F0F5;
  --bg-page: #F7F7F7;
  --bg-card: #FFFFFF;
  --border-card: #E5E7EB;
  --text-main: #212121;
  --text-muted: #6B737B;
  --green: #297D47;
  --green-bg: #E0F2E3;
  --red: #C62828;
  --red-bg: #FFEBEE;
  --orange: #B35F0C;
  --orange-bg: #FFF5E5;
}

* { box-sizing: border-box; margin: 0; padding: 0; }
body {
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
  background: var(--bg-page);
  color: var(--text-main);
  min-height: 100vh;
}

/* STICKY TOP BAR: Stop All is always reachable */
.topbar {
  position: sticky;
  top: 0;
  z-index: 10;
  background: var(--bg-page);
  border-bottom: 1px solid var(--border-card);
}
.topbar-inner {
  max-width: 1200px;
  margin: 0 auto;
  padding: 12px 40px;
  display: flex;
  align-items: center;
  gap: 8px 16px;
  flex-wrap: wrap;
}
.brand-title {
  font-size: 18px;
  font-weight: 800;
  letter-spacing: 1.5px;
  color: var(--navy-dark);
}
.device-status {
  display: flex;
  align-items: center;
  gap: 4px 12px;
  flex-wrap: wrap;
  min-width: 0;
}
.network-details {
  font-size: 12px;
  color: var(--navy-muted);
  overflow-wrap: anywhere;
}
.live-indicator {
  flex-basis: 100%;
  font-size: 12px;
  color: var(--text-muted);
}
.stop-status:not(:empty) { margin-left: 8px; font-weight: 700; color: var(--text-main); }
.stop-status.warn { color: var(--red); }
.status-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: var(--green);
  flex-shrink: 0;
}
.status-dot.pending { background: var(--navy-muted); }
.status-dot.offline { background: var(--red); }
.badge-online, .badge-fault {
  font-size: 11px;
  font-weight: 700;
  letter-spacing: 0.5px;
  padding: 6px 10px;
  border-radius: 6px;
  display: inline-flex;
  align-items: center;
  gap: 6px;
  background: var(--green-bg);
  color: var(--green);
}
.badge-fault { background: var(--red-bg); color: var(--red); }
.badge-pending { background: var(--navy-faint); color: var(--navy-muted); }
.btn-estop {
  margin-left: auto;
  background: var(--red);
  color: #fff;
  border: none;
  border-radius: 8px;
  padding: 0 22px;
  font-size: 14px;
  font-weight: 800;
  letter-spacing: 0.6px;
  cursor: pointer;
  box-shadow: 0 2px 6px rgba(198, 40, 40, 0.3);
}
.btn-estop:hover { background: #B71C1C; }
.btn-estop:active { transform: scale(0.97); }

/* MAIN CONTENT */
.main-wrapper {
  max-width: 1200px;
  margin: 0 auto;
  padding: 24px 40px 32px;
}

/* PAGE HEADER (scrolls away; Stop All stays in the top bar) */
.page-header { margin-bottom: 20px; }
.page-title { font-size: 30px; font-weight: 700; }
.page-subtitle { font-size: 15px; color: var(--text-muted); margin-top: 4px; }

/* FAULT ALERT BANNER */
.fault-banner {
  background: #B71C1C;
  color: #fff;
  border-radius: 10px;
  padding: 16px 20px;
  margin-bottom: 20px;
  display: none;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
}
.fault-title { font-size: 15px; font-weight: 700; overflow-wrap: anywhere; }
.fault-desc { font-size: 12px; color: #FFCDD2; margin-top: 2px; }
.btn-fault-reset {
  background: #fff;
  color: #B71C1C;
  font-size: 13px;
  font-weight: 700;
  padding: 0 16px;
  border-radius: 6px;
  border: none;
  cursor: pointer;
  flex-shrink: 0;
}

/* STATIONS: one card per machine station, in material order */
.stations {
  display: grid;
  grid-template-columns: minmax(0, 1fr) minmax(0, 1.8fr) minmax(0, 1fr); /* Hot end holds two stages side by side */
  gap: 20px;
  align-items: stretch;
  margin-bottom: 20px;
}
.station {
  background: var(--bg-card);
  border: 1px solid var(--border-card);
  border-radius: 12px;
  padding: 22px;
  min-width: 0;
  display: flex;
  flex-direction: column;
}
/* Equal-height cards: keep each card's setting and start/stop at the bottom. */
.station-controls { margin-top: auto; }
.station.active {
  border-color: var(--green);
  box-shadow: 0 0 0 2px var(--green-bg);
}
.station-head {
  display: flex;
  align-items: center;
  gap: 10px;
  margin-bottom: 4px;
}
.step-num-box {
  width: 30px;
  height: 30px;
  background: var(--navy-faint);
  color: var(--navy-dark);
  border-radius: 8px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 13px;
  font-weight: 700;
  flex-shrink: 0;
}
.station.active .step-num-box { background: var(--navy-dark); color: #fff; }
.station-title { font-size: 18px; font-weight: 700; flex: 1; }
.station-desc {
  font-size: 12px;
  color: var(--text-muted);
  line-height: 1.4;
  margin-bottom: 16px;
}
.step-pill {
  font-size: 11px;
  font-weight: 700;
  padding: 4px 10px;
  border-radius: 6px;
  background: #F0F0F0;
  color: #616161;
  flex-shrink: 0;
}
.step-pill.active { background: var(--green-bg); color: var(--green); }

.label-sub {
  font-size: 11px;
  font-weight: 700;
  letter-spacing: 0.8px;
  color: var(--text-muted);
  text-transform: uppercase;
  margin-bottom: 6px;
  display: block;
}
.readout { display: flex; align-items: baseline; gap: 4px; margin-bottom: 6px; }
.readout-val { font-size: 40px; font-weight: 800; color: var(--navy-dark); line-height: 1; }
.readout-unit { font-size: 16px; font-weight: 600; color: var(--navy-dark); }

/* OVERVIEW CARD */
.overview-card {
  background: var(--bg-card);
  border: 1px solid var(--border-card);
  border-radius: 12px;
  padding: 24px 28px;
  margin-bottom: 20px;
  display: grid;
  grid-template-columns: minmax(0, 1.1fr) 1px minmax(0, 0.8fr) minmax(0, 1.6fr);
  gap: 28px;
  align-items: center;
}
.overview-card > * { min-width: 0; }
.overview-card .readout { margin-bottom: 0; }
.temp-large { font-size: 60px; }
.temp-unit { font-size: 26px; }
.setpoint-row { margin-top: 12px; font-size: 13px; color: var(--text-muted); }
.setpoint-val { font-size: 18px; font-weight: 700; color: var(--text-main); margin-left: 4px; }
.card-divider { width: 1px; height: 100%; background: var(--border-card); }
.pwm-val { font-size: 32px; font-weight: 700; margin-bottom: 10px; }
.tiles-col { display: flex; gap: 12px; }
.status-tile {
  flex: 1;
  min-width: 0;
  background: #F4F7F4;
  border-radius: 10px;
  padding: 14px 12px;
  display: flex;
  flex-direction: column;
}
.status-tile.off { background: #F5F5F5; }
.tile-head { display: flex; align-items: center; gap: 8px; margin-bottom: 8px; }
.tile-dot { width: 10px; height: 10px; border-radius: 50%; background: var(--green); flex-shrink: 0; }
.status-tile.off .tile-dot { background: #9E9E9E; }
.tile-title { font-size: 11px; font-weight: 700; letter-spacing: 0.8px; color: var(--text-muted); }
.tile-value { font-size: 20px; font-weight: 800; color: var(--green); margin-bottom: 4px; }
.status-tile.off .tile-value { color: #757575; }
.tile-desc { font-size: 11px; color: var(--text-muted); }

.progress-track {
  height: 8px;
  background: var(--navy-faint);
  border-radius: 4px;
  overflow: hidden;
  margin-bottom: 6px;
}
.progress-fill { height: 100%; width: 0%; background: var(--navy-dark); transition: width 0.3s ease; }
.pwm-desc { font-size: 12px; color: var(--text-muted); }

/* One fixed-height status line per station keeps the three cards the same height. */
.status-line, .hint {
  height: 20px;
  margin: 0 0 14px;
  font-size: 13px;
  line-height: 20px;
  color: var(--text-muted);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}
.hint.ok { color: var(--green); font-weight: 600; }
.hint.warn { color: var(--orange); font-weight: 700; }

/* Heating stages inside the hot end card */
.stages { margin-top: auto; display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 12px; }
.stage-row { min-width: 0; border: 1px solid var(--border-card); border-radius: 10px; padding: 14px; }
.stage-row.active { border-color: var(--green); box-shadow: 0 0 0 2px var(--green-bg); }
.stage-head { display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px; }
.stage-name { font-size: 15px; font-weight: 700; }
.stage-row.active .stage-name { color: var(--green); }

.input-row { display: flex; gap: 8px; margin-bottom: 10px; }
.text-input {
  flex: 1;
  min-width: 0;
  background: #F9FAFB;
  border: 1px solid var(--border-card);
  border-radius: 6px;
  padding: 0 12px;
  font-size: 16px;
  font-weight: 600;
  color: var(--text-main);
  outline: none;
}
.text-input:focus { border-color: var(--navy-dark); background: #fff; }
.btn-apply {
  background: var(--navy-dark);
  color: #fff;
  border: none;
  border-radius: 6px;
  padding: 0 14px;
  min-width: 76px;
  font-size: 13px;
  font-weight: 600;
  cursor: pointer;
}
.btn-apply:hover { background: var(--navy-hover); }
.btn-apply.saved { background: var(--green); }
.btn-apply.error { background: var(--red); }
.btn-step-toggle {
  width: 100%;
  background: var(--navy-dark);
  color: #fff;
  border: 1px solid transparent;
  border-radius: 6px;
  font-size: 14px;
  font-weight: 700;
  cursor: pointer;
}
.btn-step-toggle:hover { background: var(--navy-hover); }
.btn-step-toggle.active {
  background: #FFF0F0;
  color: var(--red);
  border-color: rgba(198, 40, 40, 0.35);
}
.btn-step-toggle.active:hover { background: var(--red-bg); }

/* SAFETY FOOTER NOTICE */
.safety-notice {
  background: var(--orange-bg);
  border: 1px solid rgba(224, 122, 20, 0.2);
  border-radius: 8px;
  padding: 12px 16px;
  font-size: 12px;
  font-weight: 500;
}

/* INPUT AND KEYBOARD ACCESSIBILITY */
button, input { min-height: 44px; font-family: inherit; }
button { touch-action: manipulation; transition: background 0.15s ease; }
button:focus-visible, input:focus-visible { outline: 3px solid var(--navy-dark); outline-offset: 3px; }

/* RESPONSIVE: stack stations in material order */
@media (max-width: 1100px) {
  .overview-card { grid-template-columns: minmax(0, 1fr) minmax(0, 1fr); gap: 24px; }
  .card-divider { display: none; }
  .tiles-col { grid-column: 1 / -1; }
  .stage-row { padding: 10px; }
  .stage-row .btn-apply { min-width: 60px; padding: 0 10px; }
}
@media (max-width: 960px) {
  .topbar-inner { padding: 10px 16px; }
  .main-wrapper { padding: 16px 16px 24px; }
  .stations { grid-template-columns: minmax(0, 1fr); gap: 16px; }
}
@media (max-width: 600px) {
  .network-details { display: none; }
  .topbar-inner { gap: 6px 8px; }
  .brand-title { font-size: 14px; letter-spacing: 1px; }
  .badge-online, .badge-fault { padding: 6px 8px; }
  .btn-estop { padding: 0 12px; letter-spacing: 0.3px; }
  .station { padding: 18px; }
  .stages { grid-template-columns: minmax(0, 1fr); }
  .page-title { font-size: 24px; }
  .page-subtitle { font-size: 14px; }
  .overview-card { grid-template-columns: minmax(0, 1fr); padding: 20px; gap: 20px; }
  .tiles-col { gap: 8px; }
  .status-tile { padding: 12px 10px; }
  .tile-value { font-size: 17px; }
  .temp-large { font-size: 48px; }
  .fault-banner { flex-direction: column; align-items: flex-start; }
}
@media (prefers-reduced-motion: reduce) {
  *, *::before, *::after { transition: none !important; }
}
</style>
</head>

<body>

<header class="topbar">
  <div class="topbar-inner">
    <div class="brand-title">MOUNTOYA</div>
    <div class="device-status">
      <div id="systemBadge" class="badge-online badge-pending" role="status">
        <span class="status-dot pending" id="sysBadgeDot" aria-hidden="true"></span>
        <span id="systemBadgeText">Awaiting device status</span>
      </div>
      <p class="network-details" id="ipDisplay">Waiting for device address</p>
    </div>
    <button class="btn-estop" id="estopBtn" onclick="stopAll()" title="Immediately stop heater, cutter and puller">&#9632; STOP ALL</button>
    <p class="live-indicator"><span id="liveStatusText">Readings are unconfirmed until the device responds.</span><span class="stop-status" id="stopStatusText" role="status"></span></p>
  </div>
</header>

<main class="main-wrapper">

  <div class="page-header">
    <h1 class="page-title">Controller overview</h1>
    <p class="page-subtitle">Monitor temperature, manage process steps, and control output.</p>
  </div>

  <!-- FAULT ALERT BANNER -->
  <div id="faultBanner" class="fault-banner">
    <div>
      <div class="fault-title">SAFETY FAULT: <span id="faultText">FAULT</span></div>
      <div class="fault-desc" id="faultCause">Heater shut down. Check thermistor/wiring before resetting.</div>
    </div>
    <button class="btn-fault-reset" onclick="resetFault()">Reset Fault</button>
  </div>

  <!-- OVERVIEW CARD -->
  <section class="overview-card" aria-label="Machine overview">
    <div class="temp-col">
      <span class="label-sub">Current temperature</span>
      <div class="readout">
        <span class="readout-val temp-large" id="tempBig">0.0</span><span class="readout-unit temp-unit">&deg;C</span>
      </div>
      <div class="setpoint-row">Target Setpoint: <span class="setpoint-val" id="setpointBig">0.0</span>&deg;C</div>
    </div>

    <div class="card-divider"></div>

    <div class="pwm-col">
      <span class="label-sub">Heater output</span>
      <div class="pwm-val"><span id="pwmPct">0</span>%</div>
      <div class="progress-track"><div class="progress-fill" id="pwmFill"></div></div>
      <span class="pwm-desc">Power modulation (PWM: <span id="pwmRaw">0</span>)</span>
    </div>

    <div class="tiles-col">
      <div class="status-tile off" id="cutterTile">
        <div class="tile-head"><span class="tile-dot"></span><span class="tile-title">CUTTER</span></div>
        <span class="tile-value" id="cutterTileVal">OFF</span>
        <span class="tile-desc">Bottle strip cutter</span>
      </div>
      <div class="status-tile off" id="heaterTile">
        <div class="tile-head"><span class="tile-dot"></span><span class="tile-title">HEATER</span></div>
        <span class="tile-value" id="heaterTileVal">OFF</span>
        <span class="tile-desc">Temperature control</span>
      </div>
      <div class="status-tile off" id="pullerTile">
        <div class="tile-head"><span class="tile-dot"></span><span class="tile-title">PULLER</span></div>
        <span class="tile-value" id="pullerTileVal">OFF</span>
        <span class="tile-desc">Output wheel drive</span>
      </div>
    </div>
  </section>

  <div class="stations">

    <!-- 1 CUTTER (stepper 2) -->
    <section class="station" id="cutterCard" aria-labelledby="cutterTitle">
      <div class="station-head">
        <span class="step-num-box">1</span>
        <h2 class="station-title" id="cutterTitle">Cutter</h2>
        <span class="step-pill" id="cutterPill">OFF</span>
      </div>
      <p class="station-desc">Cuts the bottle into strip.</p>

      <span class="label-sub">Motor speed</span>
      <div class="readout"><span class="readout-val" id="cutterRpmVal">0.0</span><span class="readout-unit">RPM</span></div>
      <p class="status-line" id="cutterStatus"></p>

      <div class="station-controls">
        <label class="label-sub" for="rpm2Input">Target (0.1&ndash;60 RPM)</label>
        <div class="input-row">
          <input type="number" min="0.1" max="60" step="0.1" class="text-input" id="rpm2Input" oninput="markInputEdited(this)">
          <button class="btn-apply" id="rpm2ApplyBtn" onclick="setRPM2(this)">Apply</button>
        </div>
        <button class="btn-step-toggle" id="cutterToggleBtn" onclick="toggleCutter()">Start Cutter</button>
      </div>
    </section>

    <!-- 2 HOT END (heater, stages 1 and 2) -->
    <section class="station" id="hotendCard" aria-labelledby="hotendTitle">
      <div class="station-head">
        <span class="step-num-box">2</span>
        <h2 class="station-title" id="hotendTitle">Hot end</h2>
        <span class="step-pill" id="hotendPill">OFF</span>
      </div>
      <p class="station-desc">Heats the strip through two heating stages.</p>
      <p class="status-line" id="hotendStatus">Not heating</p>

      <div class="stages">
      <div class="stage-row" id="step1Card">
        <div class="stage-head">
          <span class="stage-name">Stage 1</span>
          <span class="step-pill" id="step1Pill">OFF</span>
        </div>
        <label class="label-sub" for="step1SPInput">Setpoint (0&ndash;260 &deg;C)</label>
        <div class="input-row">
          <input type="number" min="0" max="260" step="1" class="text-input" id="step1SPInput" oninput="markInputEdited(this)">
          <button class="btn-apply" id="step1ApplyBtn" onclick="setStep1SP(this)">Apply</button>
        </div>
        <button class="btn-step-toggle" id="step1ToggleBtn" onclick="toggleStep1()">Start Stage 1</button>
      </div>

      <div class="stage-row" id="step2Card">
        <div class="stage-head">
          <span class="stage-name">Stage 2</span>
          <span class="step-pill" id="step2Pill">OFF</span>
        </div>
        <label class="label-sub" for="step2SPInput">Setpoint (0&ndash;260 &deg;C)</label>
        <div class="input-row">
          <input type="number" min="0" max="260" step="1" class="text-input" id="step2SPInput" oninput="markInputEdited(this)">
          <button class="btn-apply" id="step2ApplyBtn" onclick="setStep2SP(this)">Apply</button>
        </div>
        <button class="btn-step-toggle" id="step2ToggleBtn" onclick="toggleStep2()">Start Stage 2</button>
      </div>
      </div>
    </section>

    <!-- 3 PULLER (stepper 1, output wheel) -->
    <section class="station" id="pullerCard" aria-labelledby="pullerTitle">
      <div class="station-head">
        <span class="step-num-box">3</span>
        <h2 class="station-title" id="pullerTitle">Puller</h2>
        <span class="step-pill" id="pullerPill">OFF</span>
      </div>
      <p class="station-desc">Pulls filament through the hot end.</p>

      <span class="label-sub">Wheel speed</span>
      <div class="readout"><span class="readout-val" id="rpmVal">0.0</span><span class="readout-unit">RPM</span></div>
      <p class="hint" id="pullerHint" role="status"></p>

      <div class="station-controls">
        <label class="label-sub" for="rpmInput">Target (0.1&ndash;60 RPM)</label>
        <div class="input-row">
          <input type="number" min="0.1" max="60" step="0.1" class="text-input" id="rpmInput" oninput="markInputEdited(this)">
          <button class="btn-apply" id="rpmApplyBtn" onclick="setRPM(this)">Apply</button>
        </div>
        <button class="btn-step-toggle" id="step3ToggleBtn" onclick="toggleStep3()">Start Puller</button>
      </div>
    </section>

  </div>

  <div class="safety-notice">
    Safety monitoring active &bull; Thermistor and wiring faults shut down the heater and cutter. Speeds are commanded values, not measured.
  </div>

</main>

<script>
// Helper for button micro-feedback
function flashButton(btn, success = true) {
  if (!btn) return;
  const origText = btn.dataset.origText || btn.innerText;
  btn.dataset.origText = origText;
  btn.classList.remove('saved', 'error');
  btn.classList.add(success ? 'saved' : 'error');
  btn.innerText = success ? 'Saved ✓' : 'Error';
  setTimeout(() => {
    btn.classList.remove('saved', 'error');
    btn.innerText = origText;
  }, 1500);
}

// Clamp numeric input to its HTML min/max attributes
function clampInput(inputEl) {
  if (String(inputEl.value).trim() === '') return null;
  let val = Number(inputEl.value);
  const min = parseFloat(inputEl.min);
  const max = parseFloat(inputEl.max);
  if (!Number.isFinite(val)) return null;
  if (!isNaN(min) && val < min) val = min;
  if (!isNaN(max) && val > max) val = max;
  inputEl.value = val;
  return val;
}

function stopAll() {
  const btn = document.getElementById('estopBtn');
  if (btn.disabled) return;
  const status = document.getElementById('stopStatusText');
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 3000);
  btn.disabled = true;
  btn.innerText = 'STOPPING...';
  status.className = 'stop-status';
  status.innerText = 'Waiting for the device to acknowledge Stop All...';
  fetch('/stopall', { method: 'POST', signal: controller.signal })
    .then(r => {
      if (!r.ok) throw new Error('Stop failed');
      status.innerText = 'Stop All acknowledged by the device.';
      btn.innerHTML = '&#9632; STOP ALL';
    })
    .catch(() => {
      status.className = 'stop-status warn';
      status.innerText = 'STOP UNCONFIRMED. Retry or use the physical power cutoff.';
      btn.innerText = 'RETRY STOP ALL';
    })
    .finally(() => {
      clearTimeout(timeout);
      btn.disabled = false;
    });
}

let currentStepState = 0;
let heaterRunningState = false;
let stepperRunningState = false;
let cutterRunningState = false;

function toggleStep1() {
  fetch('/step1?state=' + (currentStepState === 1 && heaterRunningState ? 'off' : 'on'));
}
function toggleStep2() {
  fetch('/step2?state=' + (currentStepState === 2 && heaterRunningState ? 'off' : 'on'));
}
function toggleStep3() {
  fetch('/step3?state=' + (stepperRunningState ? 'off' : 'on'));
}
function toggleCutter() {
  fetch('/stepper2?state=' + (cutterRunningState ? 'off' : 'on'));
}

function markInputEdited(input) {
  input.dataset.dirty = 'true';
  input.dataset.editVersion = String(Number(input.dataset.editVersion || 0) + 1);
}

function syncInput(input, value, pollVersion) {
  if (!input.dataset.dirty && document.activeElement !== input &&
      pollVersion > Number(input.dataset.syncAfter || 0)) {
    input.value = value;
  }
}

function applyInput(inputId, endpoint, btn) {
  if (btn.disabled) return;
  const input = document.getElementById(inputId);
  const v = clampInput(input);
  if (v === null) { flashButton(btn, false); return; }
  input.dataset.dirty = 'true';
  const editVersion = input.dataset.editVersion;
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 3000);
  btn.disabled = true;
  fetch(endpoint + '?value=' + encodeURIComponent(v), { signal: controller.signal })
    .then(r => {
      if (!r.ok) throw new Error('Save failed');
      if (input.dataset.editVersion === editVersion) {
        delete input.dataset.dirty;
        // Ignore status requests started before the save was acknowledged.
        input.dataset.syncAfter = String(statusPollVersion);
      }
      flashButton(btn, true);
    })
    .catch(() => flashButton(btn, false))
    .finally(() => {
      clearTimeout(timeout);
      btn.disabled = false;
    });
}

function setStep1SP(btn) { applyInput('step1SPInput', '/step1setpoint', btn); }
function setStep2SP(btn) { applyInput('step2SPInput', '/step2setpoint', btn); }
function setRPM(btn) { applyInput('rpmInput', '/rpm', btn); }
function setRPM2(btn) { applyInput('rpm2Input', '/rpm2', btn); }

// Mirrors tripThermalFaultLocked() reasons in filament_maker.ino.
const FAULT_CAUSES = {
  "SENSOR DISCONNECTED": "Thermistor read below 0°C or invalid: open circuit, loose connector or broken wire. Heater is off.",
  "OVERTEMP HAZARD": "Temperature exceeded 280°C absolute limit: shorted thermistor, stuck MOSFET/driver or runaway. Heater is off.",
  "OVERTEMP RUNAWAY": "Stayed 15°C+ above setpoint for 10 s with heater on: driver stuck on or thermistor out of the block. Heater is off.",
  "HEATING FAILED": "Rose under 2°C in 40 s at high power: heater cartridge loose/dead, blown fuse/no 12V, or thermistor out of the block.",
  "INVALID PID OUTPUT": "PID produced a non-numeric output: check PID tuning values (Kp/Ki/Kd). Heater is off."
};

function resetFault() {
  fetch('/resetfault');
}

function setTile(tile, val, on, onText) {
  tile.className = on ? "status-tile" : "status-tile off";
  val.innerText = on ? onText : "OFF";
}

// One running/stopped look for a card or stage row, its pill and its start/stop button.
function setRun(box, base, pill, onText, btn, on, startText, stopText) {
  box.className = base + (on ? ' active' : '');
  pill.className = 'step-pill' + (on ? ' active' : '');
  pill.innerText = on ? onText : 'OFF';
  btn.className = 'btn-step-toggle' + (on ? ' active' : '');
  btn.innerText = on ? stopText : startText;
}

let isPolling = false;
let hasDeviceStatus = false;
let statusPollVersion = 0;

function pollStatus() {
  if (isPolling) return;
  isPolling = true;
  const pollVersion = ++statusPollVersion;
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 3000);

  fetch('/status', { signal: controller.signal, cache: 'no-store' })
    .then(r => {
      if (!r.ok) throw new Error('Status request failed');
      return r.json();
    })
    .then(data => {
      const numericFields = ['temp', 'setpoint', 'pwm', 'rpm', 'step', 'step1sp', 'step2sp'];
      const booleanFields = ['heater', 'stepper', 'stable', 'fault'];
      if (!data || !numericFields.every(key => Number.isFinite(data[key])) ||
          !booleanFields.every(key => typeof data[key] === 'boolean') ||
          typeof data.faultMsg !== 'string' ||
          (data.ip !== undefined && typeof data.ip !== 'string') ||
          (data.hostname !== undefined && typeof data.hostname !== 'string')) {
        throw new Error('Invalid device status');
      }

      // Only a usable response confirms the last-known device state.
      hasDeviceStatus = true;
      document.getElementById("liveStatusText").innerText = "Live readings refresh every 500 ms.";

      // Hot end
      document.getElementById("tempBig").innerText = (data.temp <= -50) ? "ERR" : data.temp.toFixed(1);
      document.getElementById("setpointBig").innerText = data.setpoint.toFixed(1);
      let pct = Math.round((data.pwm / 255) * 100);
      document.getElementById("pwmPct").innerText = pct;
      document.getElementById("pwmRaw").innerText = data.pwm;
      document.getElementById("pwmFill").style.width = pct + '%';
      let hotReady = data.heater && data.stable;
      let hotPill = document.getElementById("hotendPill");
      document.getElementById("hotendCard").className = data.heater ? "station active" : "station";
      hotPill.className = data.heater ? "step-pill active" : "step-pill";
      hotPill.innerText = data.heater ? (data.stable ? "STABLE" : "HEATING") : "OFF";
      let stageName = data.step === 1 || data.step === 2 ? "Stage " + data.step : "Heater on";
      document.getElementById("hotendStatus").innerText = !data.heater ? "Not heating"
        : data.stable ? stageName + " at " + data.setpoint.toFixed(1) + " °C"
        : stageName + ": " + data.temp.toFixed(1) + " °C, target " + data.setpoint.toFixed(1) + " °C";
      setTile(document.getElementById("heaterTile"), document.getElementById("heaterTileVal"), data.heater, "ON");
      setTile(document.getElementById("pullerTile"), document.getElementById("pullerTileVal"), data.stepper, "RUNNING");

      let stageText = data.stable ? "STABLE" : "HEATING";
      setRun(document.getElementById("step1Card"), "stage-row", document.getElementById("step1Pill"), stageText,
             document.getElementById("step1ToggleBtn"), data.step === 1 && data.heater, "Start Stage 1", "Stop Stage 1");
      setRun(document.getElementById("step2Card"), "stage-row", document.getElementById("step2Pill"), stageText,
             document.getElementById("step2ToggleBtn"), data.step === 2 && data.heater, "Start Stage 2", "Stop Stage 2");

      // Puller
      document.getElementById("rpmVal").innerText = data.stepper ? data.rpm.toFixed(1) : "0.0";
      setRun(document.getElementById("pullerCard"), "station", document.getElementById("pullerPill"), "RUNNING",
             document.getElementById("step3ToggleBtn"), data.stepper, "Start Puller", "Stop Puller");
      let hint = document.getElementById("pullerHint");
      hint.title = "";
      if (hotReady) {
        hint.className = "hint ok";
        hint.innerText = "Hot end at temperature";
      } else if (data.stepper) {
        hint.className = "hint warn";
        hint.innerText = "Hot end not at temperature";
        hint.title = "Pulling below temperature can snap the strip.";
      } else {
        hint.className = "hint";
        hint.innerText = "Start once hot end is at temperature";
      }

      // Cutter (older firmware omits these fields)
      if (typeof data.stepper2 === 'boolean') {
        cutterRunningState = data.stepper2;
        document.getElementById("cutterRpmVal").innerText =
          data.stepper2 && Number.isFinite(data.rpm2) ? data.rpm2.toFixed(1) : "0.0";
        setRun(document.getElementById("cutterCard"), "station", document.getElementById("cutterPill"), "RUNNING",
               document.getElementById("cutterToggleBtn"), data.stepper2, "Start Cutter", "Stop Cutter");
        setTile(document.getElementById("cutterTile"), document.getElementById("cutterTileVal"), data.stepper2, "RUNNING");
        if (Number.isFinite(data.rpm2)) document.getElementById("cutterStatus").innerText = "Set to " + data.rpm2.toFixed(1) + " RPM";
      }

      // Keep unsaved edits even after the input loses focus.
      syncInput(document.getElementById("step1SPInput"), data.step1sp, pollVersion);
      syncInput(document.getElementById("step2SPInput"), data.step2sp, pollVersion);
      syncInput(document.getElementById("rpmInput"), data.rpm, pollVersion);
      if (typeof data.rpm2 === 'number') syncInput(document.getElementById("rpm2Input"), data.rpm2, pollVersion);

      // Network info
      if (data.ip) {
        document.getElementById("ipDisplay").innerText = data.ip + (data.hostname ? ' (' + data.hostname + ')' : '');
      }

      // Safety Fault Banner & System Badge
      let fb = document.getElementById("faultBanner");
      let sysBadge = document.getElementById("systemBadge");
      let sysBadgeText = document.getElementById("systemBadgeText");
      let sysBadgeDot = document.getElementById("sysBadgeDot");

      if (data.fault) {
        fb.style.display = "flex";
        // Optional fields: older firmware omits them. -100 is readTempC()'s no-sensor sentinel.
        let faultNum = !Number.isFinite(data.faultTemp) ? "" :
          data.faultTemp <= -100 ? " (no sensor reading)" :
          " at " + data.faultTemp.toFixed(1) + "°C" + (Number.isFinite(data.faultSP) && data.faultSP > 0 ? " (setpoint " + data.faultSP.toFixed(1) + "°C)" : "");
        document.getElementById("faultText").innerText = data.faultMsg + faultNum;
        document.getElementById("faultCause").innerText = FAULT_CAUSES[data.faultMsg] || "Heater shut down. Check thermistor/wiring before resetting.";
        sysBadge.className = "badge-fault";
        sysBadgeDot.className = "status-dot offline";
        sysBadgeText.innerText = "SAFETY FAULT";
      } else {
        fb.style.display = "none";
        sysBadge.className = "badge-online";
        sysBadgeDot.className = "status-dot";
        sysBadgeText.innerText = "SYSTEM ONLINE";
      }

      // Track states for toggle functions
      currentStepState = data.step;
      heaterRunningState = data.heater;
      stepperRunningState = data.stepper;
    })
    .catch(() => {
      document.getElementById("systemBadge").className = "badge-fault";
      document.getElementById("sysBadgeDot").className = "status-dot offline";
      document.getElementById("systemBadgeText").innerText = hasDeviceStatus ? "Status stale" : "Status unknown";
      document.getElementById("liveStatusText").innerText = hasDeviceStatus
        ? "Connection lost. Readings show the last known state, not live status."
        : "Device unavailable. Readings and controls do not confirm machine state.";
    })
    .finally(() => {
      clearTimeout(timeout);
      isPolling = false;
    });
}
pollStatus();
setInterval(pollStatus, 500);
</script>

</body>
</html>
)rawliteral";

#endif // DASHBOARD_H
