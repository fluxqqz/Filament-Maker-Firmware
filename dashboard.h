#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <Arduino.h>

// Web dashboard HTML / CSS / JS served to clients at "/"
// Design: Mountoya UPCYCLE Controller UI (Figma matching)
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
  --navy-active: #1F4A6B;
  --navy-muted: #40637D;
  --navy-light: #C7D9E5;
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
body {
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
  background: var(--bg-page);
  color: var(--text-main);
  min-height: 100vh;
  display: flex;
}

/* SIDEBAR */
.sidebar {
  width: 250px;
  background: var(--navy-dark);
  color: #fff;
  display: flex;
  flex-direction: column;
  padding: 28px 20px;
  flex-shrink: 0;
  position: sticky;
  top: 0;
  height: 100vh;
}
.brand-title {
  font-size: 24px;
  font-weight: 800;
  letter-spacing: 1px;
  color: #fff;
}
.brand-desc {
  font-size: 11px;
  font-weight: 600;
  letter-spacing: 1.5px;
  color: var(--navy-light);
  margin-top: 2px;
  margin-bottom: 24px;
}
.sidebar-divider {
  height: 1px;
  background: var(--navy-muted);
  margin-bottom: 24px;
  opacity: 0.5;
}
.nav-section-label {
  font-size: 10px;
  font-weight: 700;
  letter-spacing: 1.2px;
  color: var(--navy-light);
  margin-bottom: 12px;
}
.nav-item {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 12px 14px;
  border-radius: 8px;
  color: #D1DEE5;
  text-decoration: none;
  font-size: 14px;
  font-weight: 500;
  margin-bottom: 6px;
  transition: all 0.15s ease;
}
.nav-item:hover {
  background: var(--navy-hover);
  color: #fff;
}
.nav-item.active {
  background: var(--navy-active);
  color: #fff;
  font-weight: 600;
}
.nav-icon {
  width: 18px;
  height: 18px;
  fill: currentColor;
}
.sidebar-footer {
  margin-top: auto;
  padding-top: 16px;
  border-top: 1px solid var(--navy-muted);
}
.connection-label {
  font-size: 10px;
  font-weight: 700;
  letter-spacing: 1px;
  color: var(--navy-light);
  margin-bottom: 8px;
}
.connection-status {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 12px;
  color: #fff;
}
.status-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: var(--green);
  box-shadow: 0 0 6px rgba(41, 125, 71, 0.8);
}

/* MAIN CONTENT */
.main-wrapper {
  flex: 1;
  padding: 32px 40px;
  max-width: 1200px;
}
.page-header {
  display: flex;
  justify-content: space-between;
  align-items: flex-start;
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
.badge-online {
  background: var(--green-bg);
  color: var(--green);
  font-size: 11px;
  font-weight: 700;
  letter-spacing: 0.5px;
  padding: 6px 12px;
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
  padding: 6px 12px;
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
  grid-template-columns: 280px 1px 1fr 340px;
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
  background: #F9FAFB;
  border: 1px solid var(--border-card);
  border-radius: 6px;
  padding: 8px 12px;
  font-size: 14px;
  font-weight: 600;
  color: var(--text-main);
  outline: none;
}
.text-input:focus {
  border-color: var(--navy-dark);
}
.btn-apply {
  background: var(--navy-dark);
  color: #fff;
  border: none;
  border-radius: 6px;
  padding: 8px 14px;
  font-size: 12px;
  font-weight: 600;
  cursor: pointer;
  transition: background 0.15s ease;
}
.btn-apply:hover {
  background: var(--navy-hover);
}
.action-row {
  display: flex;
  gap: 8px;
  margin-top: auto;
}
.btn-start {
  flex: 1;
  background: var(--green);
  color: #fff;
  border: none;
  border-radius: 6px;
  padding: 10px 0;
  font-size: 13px;
  font-weight: 700;
  cursor: pointer;
  transition: opacity 0.15s ease;
}
.btn-stop {
  flex: 1;
  background: #F0F0F0;
  color: var(--text-main);
  border: 1px solid var(--border-card);
  border-radius: 6px;
  padding: 10px 0;
  font-size: 13px;
  font-weight: 700;
  cursor: pointer;
  transition: background 0.15s ease;
}
.btn-stop:hover {
  background: #E5E5E5;
}
.step-card.active .btn-stop {
  background: var(--red);
  color: #fff;
  border-color: var(--red);
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
.btn-toggle-on {
  background: var(--navy-dark);
  color: #fff;
  border: none;
  border-radius: 6px;
  padding: 8px 14px;
  font-size: 12px;
  font-weight: 700;
  cursor: pointer;
}
.btn-toggle-off {
  background: #F0F0F0;
  color: var(--text-main);
  border: 1px solid var(--border-card);
  border-radius: 6px;
  padding: 8px 14px;
  font-size: 12px;
  font-weight: 700;
  cursor: pointer;
}
.rpm-display-box {
  margin-top: 14px;
  padding: 12px;
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
  font-size: 18px;
  font-weight: 800;
  color: var(--navy-dark);
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

/* RESPONSIVE DESIGN */
@media (max-width: 960px) {
  body { flex-direction: column; }
  .sidebar {
    width: 100%;
    height: auto;
    position: relative;
    padding: 16px 20px;
  }
  .sidebar-divider, .nav-section-label, .nav-item { display: none; }
  .sidebar-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
  }
  .brand-desc { margin-bottom: 0; }
  .sidebar-footer {
    display: none;
  }
  .main-wrapper { padding: 20px 16px; }
  .overview-card {
    grid-template-columns: 1fr;
    gap: 20px;
  }
  .card-divider { display: none; }
  .process-grid {
    grid-template-columns: 1fr;
  }
  .lower-grid {
    grid-template-columns: 1fr;
  }
}
</style>
</head>

<body>

<!-- SIDEBAR -->
<aside class="sidebar">
  <div class="sidebar-header">
    <div>
      <div class="brand-title">MOUNTOYA</div>
      <div class="brand-desc">AIR MINERAL &bull; UPCYCLE</div>
    </div>
  </div>

  <div class="sidebar-divider"></div>

  <div class="nav-section-label">CONTROL</div>
  <a href="#overview" class="nav-item active">
    <svg class="nav-icon" viewBox="0 0 24 24"><path d="M3 13h8V3H3v10zm0 8h8v-6H3v6zm10 0h8V11h-8v10zm0-18v6h8V3h-8z"/></svg>
    Overview
  </a>
  <a href="#process" class="nav-item">
    <svg class="nav-icon" viewBox="0 0 24 24"><path d="M19 3H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm-5 14H7v-2h7v2zm3-4H7v-2h10v2zm0-4H7V7h10v2z"/></svg>
    Process steps
  </a>
  <a href="#manual" class="nav-item">
    <svg class="nav-icon" viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-6h2v6zm0-8h-2V7h2v2z"/></svg>
    Manual controls
  </a>

  <div class="sidebar-footer">
    <div class="connection-label">DEVICE CONNECTION</div>
    <div class="connection-status">
      <span class="status-dot"></span>
      <span id="connStatusText">Connected &bull; Local</span>
    </div>
    <div style="font-size:11px;color:var(--navy-light);margin-top:4px;" id="ipDisplay">192.168.1.1</div>
  </div>
</aside>

<!-- MAIN CONTENT -->
<main class="main-wrapper">

  <!-- PAGE HEADER -->
  <div class="page-header" id="overview">
    <div>
      <h1 class="page-title">Controller overview</h1>
      <p class="page-subtitle">Monitor temperature, manage process steps, and control output.</p>
    </div>
    <div id="systemBadge" class="badge-online">
      <span class="status-dot"></span>
      <span id="systemBadgeText">SYSTEM ONLINE</span>
    </div>
  </div>
  <div class="live-indicator">LIVE STATUS &bull; refreshes every 500 ms</div>

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

        <span class="label-sub">SETPOINT (&deg;C)</span>
        <div class="input-row">
          <input type="number" class="text-input" id="step1SPInput">
          <button class="btn-apply" onclick="setStep1SP()">Apply</button>
        </div>

        <div class="action-row">
          <button class="btn-start" onclick="startStep1()">Start</button>
          <button class="btn-stop" onclick="stopStep1()">Stop</button>
        </div>
      </div>

      <!-- Step 2 -->
      <div class="step-card" id="step2Card">
        <div class="step-top">
          <div class="step-num-box">02</div>
          <span class="step-pill" id="step2Pill">OFF</span>
        </div>
        <h3 class="step-title">Heating &bull; Stage 2</h3>
        <p class="step-desc">Continue heating to the second setpoint.</p>

        <span class="label-sub">SETPOINT (&deg;C)</span>
        <div class="input-row">
          <input type="number" class="text-input" id="step2SPInput">
          <button class="btn-apply" onclick="setStep2SP()">Apply</button>
        </div>

        <div class="action-row">
          <button class="btn-start" onclick="startStep2()">Start</button>
          <button class="btn-stop" onclick="stopStep2()">Stop</button>
        </div>
      </div>

      <!-- Step 3 -->
      <div class="step-card" id="step3Card">
        <div class="step-top">
          <div class="step-num-box">03</div>
          <span class="step-pill" id="step3Pill">OFF</span>
        </div>
        <h3 class="step-title">Stepper operation</h3>
        <p class="step-desc">Drive the output wheel at the configured RPM.</p>

        <span class="label-sub">OUTPUT WHEEL SPEED</span>
        <div style="font-size:14px;font-weight:600;margin-bottom:16px;">
          Target: <span id="targetRpmText">15.0</span> RPM
        </div>

        <div class="action-row">
          <button class="btn-start" onclick="startStep3()">Start</button>
          <button class="btn-stop" onclick="stopStep3()">Stop</button>
        </div>
      </div>
    </div>
  </section>

  <!-- LOWER ROW: MANUAL CONTROLS & RPM -->
  <section class="lower-grid" id="manual">
    <div class="panel">
      <h3 class="panel-title">Manual controls</h3>
      <p class="panel-desc">Direct overrides for maintenance and testing.</p>

      <div class="ctrl-row">
        <div class="ctrl-info">
          <span class="ctrl-title">Heater</span>
          <span class="ctrl-helper">Toggle heater output directly</span>
        </div>
        <div class="btn-group">
          <button class="btn-toggle-on" onclick="heaterOn()">Heater ON</button>
          <button class="btn-toggle-off" onclick="heaterOff()">Heater OFF</button>
        </div>
      </div>

      <div class="ctrl-row">
        <div class="ctrl-info">
          <span class="ctrl-title">Stepper</span>
          <span class="ctrl-helper">Toggle motor output directly</span>
        </div>
        <div class="btn-group">
          <button class="btn-toggle-on" onclick="stepperOn()">Stepper ON</button>
          <button class="btn-toggle-off" onclick="stepperOff()">Stepper OFF</button>
        </div>
      </div>
    </div>

    <div class="panel">
      <h3 class="panel-title">Output wheel speed</h3>
      <p class="panel-desc">Set the target rotation speed.</p>

      <span class="label-sub">TARGET RPM</span>
      <div class="input-row">
        <input type="number" step="0.5" class="text-input" id="rpmInput">
        <button class="btn-apply" onclick="setRPM()">Update RPM</button>
      </div>

      <div class="rpm-display-box">
        <span class="rpm-display-label">CURRENT SPEED</span>
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
function heaterOn() { fetch('/heater?state=on'); }
function heaterOff() { fetch('/heater?state=off'); }
function stepperOn() { fetch('/stepper?state=on'); }
function stepperOff() { fetch('/stepper?state=off'); }
function startStep1() { fetch('/step1?state=on'); }
function stopStep1() { fetch('/step1?state=off'); }
function startStep2() { fetch('/step2?state=on'); }
function stopStep2() { fetch('/step2?state=off'); }
function startStep3() { fetch('/step3?state=on'); }
function stopStep3() { fetch('/step3?state=off'); }

function setStep1SP() {
  let v = document.getElementById("step1SPInput").value;
  fetch('/step1setpoint?value=' + encodeURIComponent(v));
}

function setStep2SP() {
  let v = document.getElementById("step2SPInput").value;
  fetch('/step2setpoint?value=' + encodeURIComponent(v));
}

function setRPM() {
  let v = document.getElementById("rpmInput").value;
  fetch('/rpm?value=' + encodeURIComponent(v));
}

function resetFault() {
  fetch('/resetfault');
}

let spInitialized = false;

setInterval(() => {
  fetch('/status')
    .then(r => r.json())
    .then(data => {
      // Temperature
      document.getElementById("tempBig").innerText = data.temp.toFixed(1);
      document.getElementById("setpointBig").innerText = data.setpoint.toFixed(1);

      // PWM Output
      let pct = Math.round((data.pwm / 255) * 100);
      document.getElementById("pwmPct").innerText = pct;
      document.getElementById("pwmRaw").innerText = data.pwm;
      document.getElementById("pwmFill").style.width = pct + '%';

      // RPM
      document.getElementById("rpmVal").innerText = data.rpm.toFixed(1);
      document.getElementById("targetRpmText").innerText = data.rpm.toFixed(1);

      // Initialize inputs once
      if (!spInitialized) {
        document.getElementById("step1SPInput").value = data.step1sp;
        document.getElementById("step2SPInput").value = data.step2sp;
        document.getElementById("rpmInput").value = data.rpm;
        spInitialized = true;
      }

      // Network info
      if (data.ip) {
        document.getElementById("ipDisplay").innerText = data.ip + (data.hostname ? ' (' + data.hostname + ')' : '');
      }

      // Safety Fault Banner & System Badge
      let fb = document.getElementById("faultBanner");
      let sysBadge = document.getElementById("systemBadge");
      let sysBadgeText = document.getElementById("systemBadgeText");

      if (data.fault) {
        fb.style.display = "flex";
        document.getElementById("faultText").innerText = data.faultMsg;
        sysBadge.className = "badge-fault";
        sysBadgeText.innerText = "SAFETY FAULT";
      } else {
        fb.style.display = "none";
        sysBadge.className = "badge-online";
        sysBadgeText.innerText = "SYSTEM ONLINE";
      }

      // Tiles
      let hTile = document.getElementById("heaterTile");
      let hVal = document.getElementById("heaterTileVal");
      if (data.heater) {
        hTile.className = "status-tile";
        hVal.innerText = "ON";
      } else {
        hTile.className = "status-tile off";
        hVal.innerText = "OFF";
      }

      let sTile = document.getElementById("stepperTile");
      let sVal = document.getElementById("stepperTileVal");
      if (data.stepper) {
        sTile.className = "status-tile";
        sVal.innerText = "RUNNING";
      } else {
        sTile.className = "status-tile off";
        sVal.innerText = "OFF";
      }

      // Active Stage Badge
      let stageBadge = document.getElementById("activeStageBadge");
      if (data.step === 1) {
        stageBadge.className = "stage-badge";
        stageBadge.innerText = "STEP 1 ACTIVE";
      } else if (data.step === 2) {
        stageBadge.className = "stage-badge";
        stageBadge.innerText = "STEP 2 ACTIVE";
      } else if (data.step === 3) {
        stageBadge.className = "stage-badge";
        stageBadge.innerText = "STEP 3 ACTIVE";
      } else {
        stageBadge.className = "stage-badge idle";
        stageBadge.innerText = "IDLE";
      }

      // Step Cards
      let s1Card = document.getElementById("step1Card");
      let s1Pill = document.getElementById("step1Pill");
      if (data.step === 1 && data.heater) {
        s1Card.className = "step-card active";
        s1Pill.className = "step-pill active";
        s1Pill.innerText = data.stable ? "STABLE" : "HEATING";
      } else {
        s1Card.className = "step-card";
        s1Pill.className = "step-pill";
        s1Pill.innerText = "OFF";
      }

      let s2Card = document.getElementById("step2Card");
      let s2Pill = document.getElementById("step2Pill");
      if (data.step === 2 && data.heater) {
        s2Card.className = "step-card active";
        s2Pill.className = "step-pill active";
        s2Pill.innerText = data.stable ? "STABLE" : "HEATING";
      } else {
        s2Card.className = "step-card";
        s2Pill.className = "step-pill";
        s2Pill.innerText = "OFF";
      }

      let s3Card = document.getElementById("step3Card");
      let s3Pill = document.getElementById("step3Pill");
      if (data.stepper) {
        s3Card.className = "step-card active";
        s3Pill.className = "step-pill active";
        s3Pill.innerText = "RUNNING";
      } else {
        s3Card.className = "step-card";
        s3Pill.className = "step-pill";
        s3Pill.innerText = "OFF";
      }
    })
    .catch(err => {
      document.getElementById("connStatusText").innerText = "Disconnected";
    });
}, 500);
</script>

</body>
</html>
)rawliteral";

#endif // DASHBOARD_H
