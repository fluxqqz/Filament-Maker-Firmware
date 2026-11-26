// Builds an interactive dashboard preview: the real page from dashboard.h plus a
// simulated device (fetch interceptor + simple thermal model) and a toolbar.
//   node tests/dashboard_preview.cjs ../dashboard-preview.html
const fs = require('node:fs');
const path = require('node:path');

const root = path.join(__dirname, '..');
const page = fs.readFileSync(path.join(root, 'dashboard.h'), 'utf8').match(/R"rawliteral\(([\s\S]*?)\)rawliteral"/)[1];
const firmware = fs.readFileSync(path.join(root, 'filament_maker.ino'), 'utf8');
const constant = name => Number(firmware.match(new RegExp(`const float ${name} = ([\\d.]+);`))[1]);
const limits = Object.fromEntries(['MIN_SETPOINT', 'MAX_SETPOINT', 'MIN_RPM', 'MAX_RPM', 'MAX_SAFE_TEMP', 'STABLE_BAND']
  .map(k => [k, constant(k)]));

const css = `
/* SIMULATOR TOOLBAR (PREVIEW ONLY) */

.sim-toolbar { position: fixed; bottom: 16px; right: 16px; z-index: 9999; display: flex; align-items: center; gap: 8px;
  flex-wrap: wrap; background: #18222C; color: #fff; padding: 8px 12px; border-radius: 10px;
  box-shadow: 0 8px 24px rgba(0,0,0,.22); border: 1px solid rgba(255,255,255,.12); }
.sim-label { font-weight: 800; font-size: 10px; letter-spacing: .8px; color: #90CAF9; text-transform: uppercase; }
.sim-btn { min-height: 32px; background: #2B3A4A; color: #E2E8F0; border: 1px solid rgba(255,255,255,.15);
  border-radius: 6px; padding: 0 10px; font-size: 11px; font-weight: 600; cursor: pointer; }
.sim-btn:hover { background: #3B4E63; color: #fff; }
.sim-btn.active { background: var(--orange); border-color: var(--orange); color: #fff; }
@media (max-width: 600px) { body { padding-bottom: 60px; } .sim-toolbar { left: 8px; right: 8px; bottom: 8px; justify-content: center; } .sim-label { display: none; } }
`;

const toolbar = `
<div class="sim-toolbar" aria-label="Preview simulator">
  <span class="sim-label">&#9881; Simulator</span>
  <button class="sim-btn active" id="simFastBtn" onclick="toggleSimSpeed()">Fast heat: ON</button>
  <button class="sim-btn" id="simFaultBtn" onclick="triggerSimFault()">Inject fault</button>
  <button class="sim-btn" id="simWifiBtn" onclick="toggleSimWifi()">Drop Wi-Fi</button>
</div>
`;

// Mirrors the firmware's web handlers closely enough to exercise the UI.
const sim = `
const LIMITS = ${JSON.stringify(limits)};
const simWifi = { state: "idle", message: "", newIp: "", connected: "", ip: "", saved: "", hostname: "filament-maker-old.local", networks: [] };
const sim = {
  temp: 28.0, setpoint: 200.0, pwm: 0, rpm: 15.0, rpm2: 8.0, step1sp: 200.0, step2sp: 230.0,
  heater: false, stepper: false, stepper2: false, step: 0, stable: false,
  fault: false, faultMsg: "", faultTemp: 0, faultSP: 0,
  ip: "192.168.1.42", hostname: "filament-maker.local", fastMode: true, wifiOffline: false
};

setInterval(() => {
  const dt = sim.fastMode ? 1.25 : 0.25, ambient = 28.0;
  if (sim.heater) {
    const err = sim.setpoint - sim.temp;
    sim.pwm = Math.max(0, Math.min(255, Math.round(err * 8.5 + (sim.setpoint - ambient) * 0.45)));
    sim.temp += ((sim.pwm / 255) * 6.5 - (sim.temp - ambient) * 0.018) * dt + (Math.random() - 0.5) * 0.14;
  } else {
    sim.pwm = 0;
    sim.temp = Math.max(ambient, sim.temp - (sim.temp - ambient) * 0.04 * dt + (Math.random() - 0.5) * 0.06);
  }
  sim.stable = Math.abs(sim.setpoint - sim.temp) <= LIMITS.STABLE_BAND;
}, 250);

function tripFault(msg, temp) {
  Object.assign(sim, { fault: true, faultMsg: msg, faultTemp: temp, faultSP: sim.setpoint,
                       heater: false, pwm: 0, step: 0, stepper2: false });
}
function toggleSimSpeed() {
  sim.fastMode = !sim.fastMode;
  const b = document.getElementById("simFastBtn");
  b.classList.toggle("active", sim.fastMode);
  b.innerText = "Fast heat: " + (sim.fastMode ? "ON" : "OFF");
}
function triggerSimFault() { tripFault("SENSOR DISCONNECTED", -100); }
function toggleSimWifi() {
  sim.wifiOffline = !sim.wifiOffline;
  const b = document.getElementById("simWifiBtn");
  b.classList.toggle("active", sim.wifiOffline);
  b.innerText = sim.wifiOffline ? "Wi-Fi: DROPPED" : "Drop Wi-Fi";
}

window.fetch = function (url, opts = {}) {
  return new Promise((resolve, reject) => {
    if (sim.wifiOffline) { setTimeout(() => reject(new Error("Network timeout")), 150); return; }
    const u = new URL(url, location.href.startsWith("http") ? location.href : "http://192.168.1.42/");
    const on = u.searchParams.get("state") === "on";
    const num = (min, max) => {
      const v = Number(u.searchParams.get("value"));
      return Number.isFinite(v) && v >= min && v <= max ? v : null;
    };
    let ok = true;
    switch (u.pathname) {
      case "/wifi":
        resolve({ ok: true, json: () => Promise.resolve(simWifi) });
        return;
      case "/wifi/scan":
        simWifi.state = "scanning";
        setTimeout(() => Object.assign(simWifi, { state: "ready", networks: [
          { ssid: "Workshop", rssi: -48, secure: true, supported: true },
          { ssid: "Guest <b>open</b>", rssi: -63, secure: false, supported: true },
          { ssid: "Corp-Enterprise", rssi: -71, secure: true, supported: false }] }), 1500);
        break;
      case "/wifi/connect": {
        const p = new URLSearchParams(opts.body);
        const pw = p.get("password") || "";
        ok = !!p.get("ssid") && (pw === "" || (pw.length >= 8 && pw.length <= 64));
        if (!ok) { setTimeout(() => resolve({ ok, status: 400, text: () => Promise.resolve("Network name 1-32 chars; password 8-63 chars or 64 hex (empty for open)") }), 60); return; }
        simWifi.state = "connecting";
        setTimeout(() => Object.assign(simWifi, { state: "success", connected: p.get("ssid"), saved: p.get("ssid"), newIp: "192.168.0.57", ip: "192.168.0.57" }), 1500);
        break;
      }
      case "/status": {
        const { fastMode, wifiOffline, ...status } = sim;
        resolve({ ok: true, json: () => Promise.resolve(status) });
        return;
      }
      case "/step1":
      case "/step2": {
        const n = u.pathname === "/step1" ? 1 : 2;
        if (on && !sim.fault) Object.assign(sim, { step: n, heater: true, setpoint: n === 1 ? sim.step1sp : sim.step2sp });
        else if (!on) { if (sim.step === n) sim.step = 0; sim.heater = false; }
        break;
      }
      case "/step3":
      case "/stepper": sim.stepper = on; break;          // Puller never changes the heating stage.
      case "/stepper2": sim.stepper2 = on && !sim.fault; break;
      case "/heater": ok = !sim.fault; if (ok) sim.heater = on; break;
      case "/stopall":
        Object.assign(sim, { heater: false, stepper: false, stepper2: false, step: 0, pwm: 0 });
        break;
      case "/step1setpoint":
      case "/step2setpoint": {
        const v = num(LIMITS.MIN_SETPOINT, LIMITS.MAX_SETPOINT);
        ok = v !== null;
        if (ok) {
          const n = u.pathname === "/step1setpoint" ? 1 : 2;
          sim["step" + n + "sp"] = v;
          if (sim.step === n) sim.setpoint = v;
        }
        break;
      }
      case "/rpm":
      case "/rpm2": {
        const v = num(LIMITS.MIN_RPM, LIMITS.MAX_RPM);
        ok = v !== null;
        if (ok) sim[u.pathname === "/rpm" ? "rpm" : "rpm2"] = v;
        break;
      }
      case "/resetfault":
        // Firmware only resets with a plausible reading; the simulated sensor recovers here.
        Object.assign(sim, { fault: false, faultMsg: "" });
        break;
    }
    setTimeout(() => resolve({ ok, status: ok ? 200 : 400 }), 60);
  });
};
`;

let html = page
  .replace('<title>MOUNTOYA | UPCYCLE Controller</title>', '<title>MOUNTOYA | UPCYCLE Controller (Interactive Preview)</title>')
  .replace('</style>', css + '</style>')
  .replace('<script>', toolbar + '<script>' + sim + '</script>\n<script>');
if ((html.match(/<script>/g) || []).length !== 2) throw new Error('dashboard.h should contain exactly one <script> block');

const out = process.argv[2] || 'dashboard-preview.html';
html = html.replace('<!DOCTYPE html>', '<!DOCTYPE html>\n<!-- Generated by tests/dashboard_preview.cjs from dashboard.h. Do not edit by hand. -->');
fs.writeFileSync(out, html.trimStart());
console.log('Wrote', out);
