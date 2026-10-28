#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <Arduino.h>

// Web dashboard HTML / CSS / JS served to clients at "/"
// Design: Mountoya UPCYCLE Controller UI (Figma matching + UX & Safety Upgrades)
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
  --text-muted: #7A828A;
  --green: #297D47;
  --green-bg: #E0F2E3;
  --red: #C62828;
  --red-bg: #FFEBEE;
  --orange: #E07A14;
  --orange-bg: #FFF5E5;
}

* { box-sizing: border-box; margin: 0; padding: 0; }
html { scroll-behavior: smooth; }
body {
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
  background: var(--bg-page);
  color: var(--text-main);
  min-height: 100vh;
}

/* COMPACT DEVICE HEADER */
.brand-title {
  font-size: 20px;
  font-weight: 800;
  letter-spacing: 1.5px;
  color: var(--navy-dark);
  margin-bottom: 8px;
}
.device-status {
  min-width: 0;
  max-width: 320px;
}
.network-details {
  margin-top: 8px;
  font-size: 12px;
  line-height: 1.5;
  color: var(--navy-muted);
  overflow-wrap: anywhere;
}
.status-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: var(--green);
  box-shadow: 0 0 6px rgba(41, 125, 71, 0.8);
  transition: background 0.2s ease, box-shadow 0.2s ease;
  flex-shrink: 0;
}
.status-dot.pending {
  background: var(--navy-muted);
  box-shadow: none;
}
.status-dot.offline {
  background: var(--red);
  box-shadow: 0 0 6px rgba(198, 40, 40, 0.8);
}

/* MAIN CONTENT */
.main-wrapper {
  width: 100%;
  min-width: 0;
  padding: 32px 40px;
  max-width: 1200px;
  margin: 0 auto;
}
.page-header {
  display: flex;
  justify-content: space-between;
  align-items: flex-start;
  gap: 16px;
  margin-bottom: 6px;
}
.page-title {
  font-size: 30px;
  font-weight: 700;
  color: var(--text-main);
}
.page-subtitle {
  font-size: 15px;
  color: var(--text-muted);
  margin-top: 4px;
}
.header-actions {
  display: flex;
  align-items: center;
  gap: 12px;
  flex-wrap: wrap;
  justify-content: flex-end;
}
.btn-estop {
  background: var(--red);
  color: #fff;
  border: none;
  border-radius: 6px;
  padding: 7px 14px;
  font-size: 11px;
  font-weight: 800;
  letter-spacing: 0.6px;
  cursor: pointer;
  display: inline-flex;
  align-items: center;
  gap: 6px;
  box-shadow: 0 2px 6px rgba(198, 40, 40, 0.25);
  transition: background 0.15s ease, transform 0.1s ease;
}
.btn-estop:hover {
  background: #B71C1C;
}
.btn-estop:active {
  transform: scale(0.97);
}
.badge-online {
  background: var(--green-bg);
  color: var(--green);
  font-size: 11px;
  font-weight: 700;
  letter-spacing: 0.5px;
  padding: 7px 12px;
  border-radius: 6px;
  display: inline-flex;
  align-items: center;
  gap: 6px;
}
.badge-fault {
  background: var(--red-bg);
  color: var(--red);
  font-size: 11px;
  font-weight: 700;
  letter-spacing: 0.5px;
  padding: 7px 12px;
  border-radius: 6px;
  display: inline-flex;
  align-items: center;
  gap: 6px;
}
.live-indicator {
  font-size: 11px;
  font-weight: 600;
  color: var(--text-muted);
  letter-spacing: 0.5px;
  margin-bottom: 24px;
}
.live-indicator p { margin-top: 8px; }

/* FAULT ALERT BANNER */
.fault-banner {
  background: #B71C1C;
  color: #fff;
  border-radius: 10px;
  padding: 16px 20px;
  margin-bottom: 24px;
  display: none;
  align-items: center;
  justify-content: space-between;
  box-shadow: 0 4px 14px rgba(183, 28, 28, 0.3);
}
.fault-title {
  font-size: 15px;
  font-weight: 700;
}
.fault-desc {
  font-size: 12px;
  color: #FFCDD2;
  margin-top: 2px;
}
.btn-fault-reset {
  background: #fff;
  color: #B71C1C;
  font-size: 13px;
  font-weight: 700;
  padding: 8px 16px;
  border-radius: 6px;
  border: none;
  cursor: pointer;
}

/* TEMPERATURE OVERVIEW CARD */
.overview-card {
  background: var(--bg-card);
  border: 1px solid var(--border-card);
  border-radius: 12px;
  padding: 24px 28px;
  margin-bottom: 32px;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.04);
  display: grid;
  grid-template-columns: minmax(0, 1.1fr) 1px minmax(0, 0.8fr) minmax(0, 1.4fr);
  gap: 28px;
  align-items: center;
}
.temp-col {
  display: flex;
  flex-direction: column;
}
.label-sub {
  font-size: 11px;
  font-weight: 700;
  letter-spacing: 0.8px;
  color: var(--text-muted);
  text-transform: uppercase;
  margin-bottom: 6px;
  display: block;
}
.temp-value-row {
  display: flex;
  align-items: baseline;
  gap: 4px;
}
.temp-large {
  font-size: 60px;
  font-weight: 800;
  color: var(--navy-dark);
  line-height: 1;
}
.temp-unit {
  font-size: 26px;
  font-weight: 600;
  color: var(--navy-dark);
}
.setpoint-row {
  margin-top: 12px;
  font-size: 13px;
  color: var(--text-muted);
}
.setpoint-val {
  font-size: 18px;
  font-weight: 700;
  color: var(--text-main);
  margin-left: 4px;
}
.card-divider {
  width: 1px;
  height: 100%;
  background: var(--border-card);
}
.pwm-col {
  display: flex;
  flex-direction: column;
  justify-content: center;
  padding: 0 8px;
}
.pwm-val {
  font-size: 32px;
  font-weight: 700;
  color: var(--text-main);
  margin-bottom: 10px;
}
.progress-track {
  width: 100%;
  height: 10px;
  background: var(--navy-faint);
  border-radius: 5px;
  overflow: hidden;
  margin-bottom: 8px;
}
.progress-fill {
  height: 100%;
  width: 0%;
  background: var(--navy-dark);
  border-radius: 5px;
  transition: width 0.3s ease;
}
.pwm-desc {
  font-size: 12px;
  color: var(--text-muted);
}
.tiles-col {
  display: flex;
  gap: 14px;
}
.status-tile {
  flex: 1;
  background: #F4F7F4;
  border-radius: 10px;
  padding: 16px 14px;
  display: flex;
  flex-direction: column;
  transition: all 0.2s ease;
}
.status-tile.off {
  background: #F5F5F5;
}
.tile-head {
  display: flex;
  align-items: center;
  gap: 8px;
  margin-bottom: 8px;
}
.tile-dot {
  width: 10px;
  height: 10px;
  border-radius: 50%;
  background: var(--green);
}
.status-tile.off .tile-dot {
  background: #9E9E9E;
}
.tile-title {
  font-size: 11px;
  font-weight: 700;
  letter-spacing: 0.8px;
  color: var(--text-muted);
}
.tile-value {
  font-size: 22px;
  font-weight: 800;
  color: var(--green);
  margin-bottom: 4px;
}
.status-tile.off .tile-value {
  color: #757575;
}
.tile-desc {
  font-size: 11px;
  color: var(--text-muted);
}

/* SECTION HEADERS */
.section-header {
  display: flex;
  justify-content: space-between;
  align-items: flex-end;
  margin-bottom: 16px;
}
.section-title {
  font-size: 22px;
  font-weight: 700;
  color: var(--text-main);
}
.section-subtitle {
  font-size: 13px;
  color: var(--text-muted);
  margin-top: 2px;
}
.stage-badge {
  background: var(--green-bg);
  color: var(--green);
  font-size: 11px;
  font-weight: 700;
  padding: 6px 12px;
  border-radius: 6px;
}
.stage-badge.idle {
  background: #EFEFEF;
  color: #757575;
}
.stage-badge.manual {
  background: var(--orange-bg);
  color: var(--orange);
}

/* PROCESS CARDS GRID */
.process-grid {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 20px;
  margin-bottom: 32px;
}
.step-card {
  background: var(--bg-card);
  border: 1px solid var(--border-card);
  border-radius: 12px;
  padding: 22px;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.04);
  display: flex;
  flex-direction: column;
  position: relative;
  transition: all 0.2s ease;
}
.step-card.active {
  border-color: var(--green);
  box-shadow: 0 0 0 2px var(--green-bg);
}
.step-top {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 14px;
}
.step-num-box {
  width: 36px;
  height: 36px;
  background: var(--navy-dark);
  color: #fff;
  border-radius: 8px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 14px;
  font-weight: 700;
}
.step-card:not(.active) .step-num-box {
  background: var(--navy-faint);
  color: var(--navy-dark);
}
.step-pill {
  font-size: 11px;
  font-weight: 700;
  padding: 4px 10px;
  border-radius: 6px;
  background: #F0F0F0;
  color: #757575;
}
.step-pill.active {
  background: var(--green-bg);
  color: var(--green);
}
.step-title {
  font-size: 17px;
  font-weight: 700;
  color: var(--text-main);
  margin-bottom: 4px;
}
.step-desc {
  font-size: 12px;
  color: var(--text-muted);
  line-height: 1.4;
  margin-bottom: 16px;
  min-height: 34px;
}
.input-row {
  display: flex;
  gap: 8px;
  margin-bottom: 12px;
}
.text-input {
  flex: 1;
  min-width: 0;
  background: #F9FAFB;
  border: 1px solid var(--border-card);
  border-radius: 6px;
  padding: 8px 12px;
  font-size: 14px;
  font-weight: 600;
  color: var(--text-main);
  outline: none;
  transition: border-color 0.15s ease;
}
.text-input:focus {
  border-color: var(--navy-dark);
  background: #fff;
}
.btn-apply {
  background: var(--navy-dark);
  color: #fff;
  border: none;
  border-radius: 6px;
  padding: 8px 14px;
  min-width: 72px;
  font-size: 12px;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.15s ease;
}
.btn-apply:hover {
  background: var(--navy-hover);
}
.btn-apply.saved {
  background: var(--green);
  color: #fff;
}
.btn-apply.error {
  background: var(--red);
  color: #fff;
}
.btn-step-toggle {
  width: 100%;
  background: var(--navy-dark);
  color: #fff;
  border: 1px solid transparent;
  border-radius: 6px;
  padding: 11px 0;
  font-size: 13px;
  font-weight: 700;
  cursor: pointer;
  margin-top: auto;
  transition: all 0.15s ease;
}
.btn-step-toggle:hover {
  background: var(--navy-hover);
}
.btn-step-toggle.active {
  background: #FFF0F0;
  color: #C62828;
  border: 1px solid rgba(198, 40, 40, 0.35);
}
.btn-step-toggle.active:hover {
  background: #FFEBEE;
}

/* LOWER GRID */
.lower-grid {
  display: grid;
  grid-template-columns: 1.4fr 1fr;
  gap: 20px;
  margin-bottom: 28px;
}
.panel {
  background: var(--bg-card);
  border: 1px solid var(--border-card);
  border-radius: 12px;
  padding: 24px;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.04);
  display: flex;
  flex-direction: column;
}
.panel-title {
  font-size: 17px;
  font-weight: 700;
  color: var(--text-main);
  margin-bottom: 4px;
}
.panel-desc {
  font-size: 12px;
  color: var(--text-muted);
  margin-bottom: 18px;
}
.ctrl-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 12px 0;
}
.ctrl-row:not(:last-child) {
  border-bottom: 1px solid #F0F0F0;
}
.ctrl-info {
  display: flex;
  flex-direction: column;
}
.ctrl-title {
  font-size: 14px;
  font-weight: 700;
  color: var(--text-main);
}
.ctrl-helper {
  font-size: 12px;
  color: var(--text-muted);
}
.btn-group {
  display: flex;
  gap: 8px;
}
.btn-ctrl {
  background: #F0F0F0;
  color: var(--text-main);
  border: 1px solid var(--border-card);
  border-radius: 6px;
  padding: 8px 14px;
  font-size: 12px;
  font-weight: 700;
  cursor: pointer;
  transition: all 0.15s ease;
}
.btn-ctrl:hover {
  background: #E5E7EB;
}
.btn-ctrl.active-on {
  background: var(--green);
  color: #fff;
  border-color: var(--green);
}
.btn-ctrl.active-off {
  background: var(--navy-dark);
  color: #fff;
  border-color: var(--navy-dark);
}
.rpm-display-box {
  margin-top: auto;
  padding: 14px 16px;
  background: var(--navy-faint);
  border-radius: 8px;
  display: flex;
  justify-content: space-between;
  align-items: center;
}
.rpm-display-label {
  font-size: 12px;
  font-weight: 600;
  color: var(--navy-dark);
}
.rpm-display-val {
  font-size: 20px;
  font-weight: 800;
  color: var(--navy-dark);
}
.telemetry-row {
  display: flex;
  justify-content: space-between;
  font-size: 13px;
  padding: 8px 0;
  border-bottom: 1px solid #F0F0F0;
  margin-bottom: 12px;
}
.telemetry-label {
  color: var(--text-muted);
}
.telemetry-val {
  font-weight: 700;
  color: var(--text-main);
}

/* SAFETY FOOTER NOTICE */
.safety-notice {
  background: var(--orange-bg);
  border: 1px solid rgba(224, 122, 20, 0.2);
  border-radius: 8px;
  padding: 12px 16px;
  display: flex;
  align-items: center;
  gap: 10px;
  font-size: 12px;
  color: var(--text-main);
  font-weight: 500;
}
.safety-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: var(--orange);
  flex-shrink: 0;
}

/* INPUT AND KEYBOARD ACCESSIBILITY */
button, input { min-height: 44px; }
button, input { font-family: inherit; }
button { touch-action: manipulation; }
button:focus-visible, input:focus-visible {
  outline: 3px solid var(--navy-dark);
  outline-offset: 3px;
}
.text-input { font-size: 16px; }
.badge-pending {
  background: var(--navy-faint);
  color: var(--navy-muted);
}
.overview-card > *, .status-tile, .step-card, .panel { min-width: 0; }
.fault-banner, .section-header, .ctrl-row, .telemetry-row, .rpm-display-box { gap: 12px; }
.fault-title { overflow-wrap: anywhere; }
.btn-fault-reset, .stage-badge { flex-shrink: 0; }
.btn-group { flex-shrink: 0; }

/* RESPONSIVE DESIGN */
@media (max-width: 1100px) {
  .overview-card {
    grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);
    gap: 24px;
  }
  .card-divider { display: none; }
  .tiles-col { grid-column: 1 / -1; }
}
@media (max-width: 960px) {
  .main-wrapper { padding: 24px; }
  .page-header { flex-direction: column; gap: 20px; }
  .header-actions { width: 100%; justify-content: flex-start; }
  .device-status { max-width: 100%; }
  .process-grid, .lower-grid { grid-template-columns: 1fr; }
}
@media (max-width: 600px) {
  .main-wrapper { padding: 20px 16px; }
  .page-title { font-size: 26px; }
  .page-subtitle { font-size: 14px; }
  .header-actions { gap: 12px; }
  .device-status { flex: 1 1 160px; }
  .btn-estop { margin-left: auto; }
  .overview-card { grid-template-columns: minmax(0, 1fr); padding: 20px; gap: 20px; }
  .pwm-col { padding: 0; }
  .tiles-col { gap: 10px; }
  .status-tile { padding: 14px 10px; }
  .tile-value { font-size: 20px; }
  .fault-banner, .section-header, .ctrl-row { align-items: flex-start; flex-direction: column; }
  .btn-group { width: 100%; }
  .btn-ctrl { flex: 1; }
  .step-card, .panel { padding: 20px; }
  .telemetry-row, .rpm-display-box { flex-wrap: wrap; }
}
@media (prefers-reduced-motion: reduce) {
  html { scroll-behavior: auto; }
  *, *::before, *::after { transition: none !important; }
}
</style>
</head>

<body>

<!-- MAIN CONTENT -->
<main class="main-wrapper">

  <!-- PAGE HEADER -->
  <div class="page-header" id="overview">
    <div>
      <div class="brand-title">MOUNTOYA</div>
      <h1 class="page-title">Controller overview</h1>
      <p class="page-subtitle">Monitor temperature, manage process steps, and control output.</p>
    </div>
    <div class="header-actions">
      <div class="device-status">
        <div id="systemBadge" class="badge-online badge-pending" role="status">
          <span class="status-dot pending" id="sysBadgeDot" aria-hidden="true"></span>
          <span id="systemBadgeText">Awaiting device status</span>
        </div>
        <p class="network-details" id="ipDisplay">Waiting for device address</p>
      </div>
      <button class="btn-estop" id="estopBtn" onclick="stopAll()" title="Immediately stop heater and stepper motor">
        &#9632; STOP ALL
      </button>
    </div>
  </div>
  <div class="live-indicator">
    <p id="liveStatusText">Readings are unconfirmed until the device responds.</p>
    <p id="stopStatusText" role="status"></p>
  </div>

  <!-- FAULT ALERT BANNER -->
  <div id="faultBanner" class="fault-banner">
    <div>
      <div class="fault-title">SAFETY FAULT: <span id="faultText">FAULT</span></div>
      <div class="fault-desc">Heater shut down. Check thermistor/wiring before resetting.</div>
    </div>
    <button class="btn-fault-reset" onclick="resetFault()">Reset Fault</button>
  </div>

  <!-- TEMPERATURE OVERVIEW CARD -->
  <section class="overview-card">
    <div class="temp-col">
      <span class="label-sub">CURRENT TEMPERATURE</span>
      <div class="temp-value-row">
        <span class="temp-large" id="tempBig">0.0</span>
        <span class="temp-unit">&deg;C</span>
      </div>
      <div class="setpoint-row">
        Target Setpoint: <span class="setpoint-val" id="setpointBig">0.0</span>&deg;C
      </div>
    </div>

    <div class="card-divider"></div>

    <div class="pwm-col">
      <span class="label-sub">HEATER OUTPUT</span>
      <div class="pwm-val"><span id="pwmPct">0</span>%</div>
      <div class="progress-track">
        <div class="progress-fill" id="pwmFill"></div>
      </div>
      <span class="pwm-desc">Power modulation (PWM: <span id="pwmRaw">0</span>)</span>
    </div>

    <div class="tiles-col">
      <div class="status-tile off" id="heaterTile">
        <div class="tile-head">
          <span class="tile-dot"></span>
          <span class="tile-title">HEATER</span>
        </div>
        <span class="tile-value" id="heaterTileVal">OFF</span>
        <span class="tile-desc">Temperature control</span>
      </div>

      <div class="status-tile off" id="stepperTile">
        <div class="tile-head">
          <span class="tile-dot"></span>
          <span class="tile-title">STEPPER</span>
        </div>
        <span class="tile-value" id="stepperTileVal">OFF</span>
        <span class="tile-desc">Output wheel drive</span>
      </div>
    </div>
  </section>

  <!-- PROCESS STEPS SECTION -->
  <section id="process">
    <div class="section-header">
      <div>
        <h2 class="section-title">Process steps</h2>
        <p class="section-subtitle">Configure setpoints and manage each stage independently.</p>
      </div>
      <div class="stage-badge idle" id="activeStageBadge">IDLE</div>
    </div>

    <div class="process-grid">
      <!-- Step 1 -->
      <div class="step-card" id="step1Card">
        <div class="step-top">
          <div class="step-num-box">01</div>
          <span class="step-pill" id="step1Pill">OFF</span>
        </div>
        <h3 class="step-title">Heating &bull; Stage 1</h3>
        <p class="step-desc">Bring material to the first target temperature.</p>

        <label class="label-sub" for="step1SPInput">SETPOINT (0&ndash;260 &deg;C)</label>
        <div class="input-row">
          <input type="number" min="0" max="260" step="1" class="text-input" id="step1SPInput" oninput="markInputEdited(this)">
          <button class="btn-apply" id="step1ApplyBtn" onclick="setStep1SP(this)">Apply</button>
        </div>

        <button class="btn-step-toggle" id="step1ToggleBtn" onclick="toggleStep1()">Start Stage 1</button>
      </div>

      <!-- Step 2 -->
      <div class="step-card" id="step2Card">
        <div class="step-top">
          <div class="step-num-box">02</div>
          <span class="step-pill" id="step2Pill">OFF</span>
        </div>
        <h3 class="step-title">Heating &bull; Stage 2</h3>
        <p class="step-desc">Continue heating to the second setpoint.</p>

        <label class="label-sub" for="step2SPInput">SETPOINT (0&ndash;260 &deg;C)</label>
        <div class="input-row">
          <input type="number" min="0" max="260" step="1" class="text-input" id="step2SPInput" oninput="markInputEdited(this)">
          <button class="btn-apply" id="step2ApplyBtn" onclick="setStep2SP(this)">Apply</button>
        </div>

        <button class="btn-step-toggle" id="step2ToggleBtn" onclick="toggleStep2()">Start Stage 2</button>
      </div>

      <!-- Step 3 -->
      <div class="step-card" id="step3Card">
        <div class="step-top">
          <div class="step-num-box">03</div>
          <span class="step-pill" id="step3Pill">OFF</span>
        </div>
        <h3 class="step-title">Stepper operation</h3>
        <p class="step-desc">Drive the output wheel at the configured RPM.</p>

        <label class="label-sub" for="rpmInput">TARGET SPEED (0.1&ndash;60 RPM)</label>
        <div class="input-row">
          <input type="number" min="0.1" max="60" step="0.1" class="text-input" id="rpmInput" oninput="markInputEdited(this)">
          <button class="btn-apply" id="rpmApplyBtn" onclick="setRPM(this)">Apply</button>
        </div>

        <button class="btn-step-toggle" id="step3ToggleBtn" onclick="toggleStep3()">Start Stepper</button>
      </div>
    </div>
  </section>

  <!-- LOWER ROW: MANUAL CONTROLS & TELEMETRY -->
  <section class="lower-grid" id="manual">
    <div class="panel">
      <h3 class="panel-title">Manual controls</h3>
      <p class="panel-desc">Direct hardware overrides for maintenance and testing.</p>

      <div class="ctrl-row">
        <div class="ctrl-info">
          <span class="ctrl-title">Heater</span>
          <span class="ctrl-helper">Toggle heater output directly</span>
        </div>
        <div class="btn-group">
          <button class="btn-ctrl" id="btnHeaterOn" onclick="heaterOn()">Heater ON</button>
          <button class="btn-ctrl active-off" id="btnHeaterOff" onclick="heaterOff()">Heater OFF</button>
        </div>
      </div>

      <div class="ctrl-row">
        <div class="ctrl-info">
          <span class="ctrl-title">Stepper</span>
          <span class="ctrl-helper">Toggle motor output directly</span>
        </div>
        <div class="btn-group">
          <button class="btn-ctrl" id="btnStepperOn" onclick="stepperOn()">Stepper ON</button>
          <button class="btn-ctrl active-off" id="btnStepperOff" onclick="stepperOff()">Stepper OFF</button>
        </div>
      </div>
    </div>

    <div class="panel">
      <h3 class="panel-title">Output wheel telemetry</h3>
      <p class="panel-desc">Live drive speed and thermal readiness status.</p>

      <div class="telemetry-row">
        <span class="telemetry-label">Configured Target</span>
        <span class="telemetry-val"><span id="targetRpmText">15.0</span> RPM</span>
      </div>
      <div class="telemetry-row">
        <span class="telemetry-label">Thermal Stability</span>
        <span class="telemetry-val" id="thermalStateText">Standby</span>
      </div>

      <div class="rpm-display-box">
        <span class="rpm-display-label">CURRENT WHEEL SPEED</span>
        <span class="rpm-display-val"><span id="rpmVal">0.0</span> RPM</span>
      </div>
    </div>
  </section>

  <!-- SAFETY NOTICE FOOTER -->
  <div class="safety-notice">
    <span class="safety-dot"></span>
    <span>Safety monitoring active &bull; Thermistor and wiring faults will shut down the heater.</span>
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
  btn.innerText = success ? 'Saved \u2713' : 'Error';
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

function heaterOn() { fetch('/heater?state=on'); }
function heaterOff() { fetch('/heater?state=off'); }
function stepperOn() { fetch('/stepper?state=on'); }
function stepperOff() { fetch('/stepper?state=off'); }

function stopAll() {
  const btn = document.getElementById('estopBtn');
  if (btn.disabled) return;
  const status = document.getElementById('stopStatusText');
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 3000);
  btn.disabled = true;
  btn.innerText = 'STOPPING...';
  status.innerText = 'Waiting for the device to acknowledge Stop All...';
  fetch('/stopall', { method: 'POST', signal: controller.signal })
    .then(r => {
      if (!r.ok) throw new Error('Stop failed');
      status.innerText = 'Stop All acknowledged by the device.';
      btn.innerHTML = '&#9632; STOP ALL';
    })
    .catch(() => {
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

function toggleStep1() {
  if (currentStepState === 1 && heaterRunningState) {
    fetch('/step1?state=off');
  } else {
    fetch('/step1?state=on');
  }
}

function toggleStep2() {
  if (currentStepState === 2 && heaterRunningState) {
    fetch('/step2?state=off');
  } else {
    fetch('/step2?state=on');
  }
}

function toggleStep3() {
  if (stepperRunningState) {
    fetch('/step3?state=off');
  } else {
    fetch('/step3?state=on');
  }
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

function resetFault() {
  fetch('/resetfault');
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

      // Temperature
      document.getElementById("tempBig").innerText = (data.temp <= -50) ? "ERR" : data.temp.toFixed(1);
      document.getElementById("setpointBig").innerText = data.setpoint.toFixed(1);

      // PWM Output
      let pct = Math.round((data.pwm / 255) * 100);
      document.getElementById("pwmPct").innerText = pct;
      document.getElementById("pwmRaw").innerText = data.pwm;
      document.getElementById("pwmFill").style.width = pct + '%';

      // RPM & Telemetry
      document.getElementById("rpmVal").innerText = data.stepper ? data.rpm.toFixed(1) : "0.0";
      document.getElementById("targetRpmText").innerText = data.rpm.toFixed(1);
      document.getElementById("thermalStateText").innerText = data.heater
        ? (data.stable ? "Stable at Setpoint" : "Heating to Setpoint")
        : "Standby";

      // Keep unsaved edits even after the input loses focus.
      syncInput(document.getElementById("step1SPInput"), data.step1sp, pollVersion);
      syncInput(document.getElementById("step2SPInput"), data.step2sp, pollVersion);
      syncInput(document.getElementById("rpmInput"), data.rpm, pollVersion);

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
        document.getElementById("faultText").innerText = data.faultMsg;
        sysBadge.className = "badge-fault";
        sysBadgeDot.className = "status-dot offline";
        sysBadgeText.innerText = "SAFETY FAULT";
      } else {
        fb.style.display = "none";
        sysBadge.className = "badge-online";
        sysBadgeDot.className = "status-dot";
        sysBadgeText.innerText = "SYSTEM ONLINE";
      }

      // Status Tiles & Dynamic Manual Buttons
      let hTile = document.getElementById("heaterTile");
      let hVal = document.getElementById("heaterTileVal");
      let btnHOn = document.getElementById("btnHeaterOn");
      let btnHOff = document.getElementById("btnHeaterOff");

      if (data.heater) {
        hTile.className = "status-tile";
        hVal.innerText = "ON";
        btnHOn.className = "btn-ctrl active-on";
        btnHOff.className = "btn-ctrl";
      } else {
        hTile.className = "status-tile off";
        hVal.innerText = "OFF";
        btnHOn.className = "btn-ctrl";
        btnHOff.className = "btn-ctrl active-off";
      }

      let sTile = document.getElementById("stepperTile");
      let sVal = document.getElementById("stepperTileVal");
      let btnSOn = document.getElementById("btnStepperOn");
      let btnSOff = document.getElementById("btnStepperOff");

      if (data.stepper) {
        sTile.className = "status-tile";
        sVal.innerText = "RUNNING";
        btnSOn.className = "btn-ctrl active-on";
        btnSOff.className = "btn-ctrl";
      } else {
        sTile.className = "status-tile off";
        sVal.innerText = "OFF";
        btnSOn.className = "btn-ctrl";
        btnSOff.className = "btn-ctrl active-off";
      }

      // Active Stage Badge (including Manual Override detection)
      let stageBadge = document.getElementById("activeStageBadge");
      if (data.step === 1 && data.heater) {
        stageBadge.className = "stage-badge";
        stageBadge.innerText = "STEP 1 ACTIVE";
      } else if (data.step === 2 && data.heater) {
        stageBadge.className = "stage-badge";
        stageBadge.innerText = "STEP 2 ACTIVE";
      } else if (data.step === 3 && data.stepper) {
        stageBadge.className = "stage-badge";
        stageBadge.innerText = "STEP 3 ACTIVE";
      } else if (data.heater || data.stepper) {
        stageBadge.className = "stage-badge manual";
        stageBadge.innerText = "MANUAL OVERRIDE";
      } else {
        stageBadge.className = "stage-badge idle";
        stageBadge.innerText = "IDLE";
      }

      // Track states for toggle functions
      currentStepState = data.step;
      heaterRunningState = data.heater;
      stepperRunningState = data.stepper;

      // Step Cards
      let s1Card = document.getElementById("step1Card");
      let s1Pill = document.getElementById("step1Pill");
      let s1Btn = document.getElementById("step1ToggleBtn");
      if (data.step === 1 && data.heater) {
        s1Card.className = "step-card active";
        s1Pill.className = "step-pill active";
        s1Pill.innerText = data.stable ? "STABLE" : "HEATING";
        s1Btn.className = "btn-step-toggle active";
        s1Btn.innerText = "Stop Stage 1";
      } else {
        s1Card.className = "step-card";
        s1Pill.className = "step-pill";
        s1Pill.innerText = "OFF";
        s1Btn.className = "btn-step-toggle";
        s1Btn.innerText = "Start Stage 1";
      }

      let s2Card = document.getElementById("step2Card");
      let s2Pill = document.getElementById("step2Pill");
      let s2Btn = document.getElementById("step2ToggleBtn");
      if (data.step === 2 && data.heater) {
        s2Card.className = "step-card active";
        s2Pill.className = "step-pill active";
        s2Pill.innerText = data.stable ? "STABLE" : "HEATING";
        s2Btn.className = "btn-step-toggle active";
        s2Btn.innerText = "Stop Stage 2";
      } else {
        s2Card.className = "step-card";
        s2Pill.className = "step-pill";
        s2Pill.innerText = "OFF";
        s2Btn.className = "btn-step-toggle";
        s2Btn.innerText = "Start Stage 2";
      }

      let s3Card = document.getElementById("step3Card");
      let s3Pill = document.getElementById("step3Pill");
      let s3Btn = document.getElementById("step3ToggleBtn");
      if (data.stepper) {
        s3Card.className = "step-card active";
        s3Pill.className = "step-pill active";
        s3Pill.innerText = "RUNNING";
        s3Btn.className = "btn-step-toggle active";
        s3Btn.innerText = "Stop Stepper";
      } else {
        s3Card.className = "step-card";
        s3Pill.className = "step-pill";
        s3Pill.innerText = "OFF";
        s3Btn.className = "btn-step-toggle";
        s3Btn.innerText = "Start Stepper";
      }
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
