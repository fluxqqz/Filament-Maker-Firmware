#ifndef CALIBRATION_DASHBOARD_H
#define CALIBRATION_DASHBOARD_H

#include <Arduino.h>

// Thermistor calibration page served at "/". Points are kept in the browser (localStorage),
// the fit runs in the browser, and the device only reports readings and drives the heater.
const char calibrationPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>MOUNTOYA | Thermistor Calibration</title>
<style>
:root {
  --navy-dark: #0B3658; --navy-muted: #40637D; --navy-faint: #E8F0F5;
  --bg-page: #F7F7F7; --bg-card: #FFFFFF; --border-card: #E5E7EB;
  --text-main: #212121; --text-muted: #6B737B;
  --green: #297D47; --green-bg: #E0F2E3; --red: #C62828; --red-bg: #FFEBEE;
  --orange: #B35F0C; --orange-bg: #FFF5E5;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
  background: var(--bg-page); color: var(--text-main); }
.topbar { position: sticky; top: 0; z-index: 10; background: var(--bg-page); border-bottom: 1px solid var(--border-card); }
.topbar-inner, .main { max-width: 1100px; margin: 0 auto; padding: 12px 24px; }
.topbar-inner { display: flex; align-items: center; gap: 12px; flex-wrap: wrap; }
.brand { font-size: 18px; font-weight: 800; letter-spacing: 1.5px; color: var(--navy-dark); }
.badge { font-size: 11px; font-weight: 700; letter-spacing: .5px; padding: 6px 10px; border-radius: 6px; }
.ok { background: var(--green-bg); color: var(--green); }
.warn { background: var(--orange-bg); color: var(--orange); }
.bad { background: var(--red-bg); color: var(--red); }
.muted-badge { background: var(--navy-faint); color: var(--navy-muted); }
.btn { border: none; border-radius: 8px; padding: 10px 16px; font-size: 14px; font-weight: 700; cursor: pointer; }
.btn:disabled { opacity: .5; cursor: not-allowed; }
.btn-primary { background: var(--navy-dark); color: #fff; }
.btn-light { background: var(--navy-faint); color: var(--navy-dark); }
.btn-danger { background: var(--red); color: #fff; }
.btn-off { margin-left: auto; }
.main { padding-top: 24px; padding-bottom: 32px; }
h1 { font-size: 26px; }
.sub { color: var(--text-muted); margin: 4px 0 20px; font-size: 14px; }
.fault { display: none; background: #B71C1C; color: #fff; border-radius: 10px; padding: 14px 18px; margin-bottom: 20px;
  align-items: center; justify-content: space-between; gap: 12px; }
.fault .btn { background: #fff; color: #B71C1C; }
.grid { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 20px; margin-bottom: 20px; }
.card { background: var(--bg-card); border: 1px solid var(--border-card); border-radius: 12px; padding: 20px; min-width: 0; }
.card h2 { font-size: 13px; letter-spacing: .8px; text-transform: uppercase; color: var(--navy-muted); margin-bottom: 12px; }
.big { font-size: 40px; font-weight: 700; font-variant-numeric: tabular-nums; }
.big small { font-size: 18px; color: var(--text-muted); font-weight: 600; }
.kv { display: flex; justify-content: space-between; font-size: 14px; padding: 5px 0; border-top: 1px solid var(--border-card);
  font-variant-numeric: tabular-nums; }
.kv span:first-child { color: var(--text-muted); }
.note { font-size: 12px; color: var(--text-muted); margin-top: 10px; line-height: 1.45; }
label { display: block; font-size: 13px; color: var(--text-muted); margin: 10px 0 6px; }
input[type=number] { width: 100%; padding: 10px; font-size: 16px; border: 1px solid var(--border-card); border-radius: 8px; }
input[type=range] { width: 100%; }
.row { display: flex; gap: 8px; margin-top: 12px; }
.row .btn { flex: 1; }
table { width: 100%; border-collapse: collapse; font-size: 14px; font-variant-numeric: tabular-nums; }
th, td { text-align: right; padding: 8px 6px; border-bottom: 1px solid var(--border-card); }
th:first-child, td:first-child { text-align: left; }
th { font-size: 12px; color: var(--text-muted); font-weight: 600; }
.del { background: none; border: none; color: var(--red); cursor: pointer; font-size: 13px; }
.empty { color: var(--text-muted); font-size: 14px; padding: 12px 0; }
textarea { width: 100%; font-family: ui-monospace, Consolas, monospace; font-size: 13px; padding: 10px; border-radius: 8px;
  border: 1px solid var(--border-card); background: var(--bg-page); resize: vertical; margin-top: 8px; }
ol { padding-left: 20px; font-size: 14px; line-height: 1.6; }
.wide { margin-bottom: 20px; }
@media (max-width: 860px) { .grid { grid-template-columns: 1fr; } .topbar-inner, .main { padding-left: 16px; padding-right: 16px; } }
</style>
</head>
<body>
<div class="topbar"><div class="topbar-inner">
  <span class="brand">MOUNTOYA</span>
  <span class="badge muted-badge" id="conn">CONNECTING</span>
  <button class="btn btn-danger btn-off" id="offBtn">HEATER OFF</button>
</div></div>

<div class="main">
  <h1>Thermistor Calibration</h1>
  <p class="sub">Hold the hot end at a steady temperature, enter what a reference thermometer reads, capture. Repeat across the working range.</p>

  <div class="fault" id="faultBox">
    <div><b id="faultText">FAULT</b><div style="font-size:12px;opacity:.85">Heater is locked off.</div></div>
    <button class="btn" id="resetBtn">Reset fault</button>
  </div>

  <div class="grid">
    <div class="card">
      <h2>Live reading</h2>
      <div class="big"><span id="temp">--</span> <small>&deg;C</small></div>
      <div class="note" style="margin:2px 0 10px">with the constants in the firmware now</div>
      <div class="kv"><span>Resistance</span><span id="res">--</span></div>
      <div class="kv"><span>ADC (filtered)</span><span id="adc">--</span></div>
      <div class="kv"><span>Stability (30 s)</span><span id="stab" class="badge muted-badge">--</span></div>
    </div>

    <div class="card">
      <h2>Heater</h2>
      <div class="big"><span id="pwmNow">0</span> <small>% power</small></div>
      <label for="pwmSlider">Set power: <b id="pwmSet">0</b> %</label>
      <input type="range" id="pwmSlider" min="0" max="100" step="1" value="0">
      <div class="row">
        <button class="btn btn-primary" id="applyBtn">Apply</button>
        <button class="btn btn-light" id="zeroBtn">Off</button>
      </div>
      <p class="note">Fixed power, no PID: the block settles where heat in equals heat out, which gives the steadiest reading.
        Firmware cuts the heater above <span id="maxSafe">280</span> &deg;C, on a sensor fault, or 10 s after this page stops polling.</p>
    </div>

    <div class="card">
      <h2>Capture point</h2>
      <label for="refInput">Reference temperature (&deg;C)</label>
      <input type="number" id="refInput" step="0.1" min="-20" max="300" placeholder="e.g. 200.0">
      <div class="row"><button class="btn btn-primary" id="captureBtn">Capture</button></div>
      <p class="note" id="captureMsg">Records the average resistance of the last 10 s.</p>
    </div>
  </div>

  <div class="card wide">
    <h2>Points</h2>
    <table>
      <thead><tr><th>Reference</th><th>Resistance</th><th>ADC</th><th>Firmware now</th><th>Error now</th><th>New fit</th><th>Error after</th><th></th></tr></thead>
      <tbody id="pointsBody"></tbody>
    </table>
    <div class="empty" id="pointsEmpty">No points yet.</div>
  </div>

  <div class="grid" style="grid-template-columns: minmax(0,1.4fr) minmax(0,1fr)">
    <div class="card">
      <h2>Result</h2>
      <div class="kv"><span>Method</span><span id="method">--</span></div>
      <div class="kv"><span>NOMINAL_RESISTANCE (R at 25 &deg;C)</span><span id="fitR25">--</span></div>
      <div class="kv"><span>BETA_COEFFICIENT</span><span id="fitBeta">--</span></div>
      <div class="kv"><span>Worst error now &rarr; after</span><span id="fitErr">--</span></div>
      <p class="note" id="fitWarn"></p>
      <label for="snippet">Paste into filament_maker.ino (replace the two lines)</label>
      <textarea id="snippet" rows="3" wrap="off" readonly></textarea>
      <div class="row"><button class="btn btn-light" id="clearBtn">Clear all points</button></div>
    </div>
    <div class="card">
      <h2>How to</h2>
      <ol>
        <li>Put a reference probe (thermocouple) in the heater block next to the thermistor.</li>
        <li>Capture one point cold (room temperature, heater off).</li>
        <li>Set a fixed power, wait for <b>Stable</b>, read the probe, capture.</li>
        <li>Repeat near your stage temperatures, e.g. ~100 &deg;C and ~200 &deg;C.</li>
        <li>Copy the result into filament_maker.ino and flash it back.</li>
      </ol>
      <p class="note">One point (or points less than 30 &deg;C apart) keeps beta 3950 and only fits R25. Two or more spread points fit both.</p>
    </div>
  </div>
</div>

<script>
// fit-math-start
const MIN_SPAN_C = 30;
function betaTempC(r, r25, beta, t25) {
  return 1 / (Math.log(r / r25) / beta + 1 / (t25 + 273.15)) - 273.15;
}
// Beta model: ln(R) = ln(R25) + beta * (1/T - 1/T25). Least squares on (1/T, ln R).
function fitBeta(pts, beta0, t25) {
  if (!pts.length) return null;
  const n = pts.length;
  const xs = pts.map(p => 1 / (p.ref + 273.15) - 1 / (t25 + 273.15));
  const ys = pts.map(p => Math.log(p.r));
  const refs = pts.map(p => p.ref);
  if (n < 2 || Math.max(...refs) - Math.min(...refs) < MIN_SPAN_C) {
    // Too little spread to trust a slope: keep the datasheet beta, fit R25 only.
    const lnR25 = ys.reduce((s, y, i) => s + y - beta0 * xs[i], 0) / n;
    return { r25: Math.exp(lnR25), beta: beta0, betaFitted: false };
  }
  const mx = xs.reduce((a, b) => a + b) / n, my = ys.reduce((a, b) => a + b) / n;
  let sxy = 0, sxx = 0;
  for (let i = 0; i < n; i++) { sxy += (xs[i] - mx) * (ys[i] - my); sxx += (xs[i] - mx) ** 2; }
  const beta = sxy / sxx;
  return { r25: Math.exp(my - beta * mx), beta, betaFitted: true };
}
// fit-math-end

const $ = id => document.getElementById(id);
const STORE = 'thermCalPoints';
let cfg = null;          // constants reported by the firmware
let hist = [];           // recent samples: {ms, r, t, adc}
let points = [];
try { points = JSON.parse(localStorage.getItem(STORE) || '[]').filter(p => isFinite(p.ref) && p.r > 0); } catch (e) { points = []; }

function fmt(v, d) { return v == null || !isFinite(v) ? '--' : v.toFixed(d); }
function signed(v) {
  if (!isFinite(v)) return '--';
  const s = Math.abs(v).toFixed(2);
  return s === '0.00' ? s : (v > 0 ? '+' : '-') + s;
}
function setBadge(el, text, cls) { el.textContent = text; el.className = 'badge ' + cls; }

function stability() {
  const now = Date.now();
  const win = hist.filter(h => now - h.ms <= 30000 && h.t != null);
  if (win.length < 2 || now - win[0].ms < 25000) return { stable: false, text: 'Collecting', cls: 'muted-badge' };
  const ts = win.map(h => h.t), span = Math.max(...ts) - Math.min(...ts);
  return span <= 0.5
    ? { stable: true, text: 'Stable ±' + (span / 2).toFixed(2) + ' °C', cls: 'ok' }
    : { stable: false, text: 'Settling ' + span.toFixed(1) + ' °C', cls: 'warn' };
}

function render(s) {
  $('temp').textContent = fmt(s.t, 2);
  $('res').textContent = s.r == null ? 'out of range' : s.r.toFixed(1) + ' Ω';
  $('adc').textContent = fmt(s.adc, 1);
  $('pwmNow').textContent = Math.round(s.pwm / 2.55);
  $('maxSafe').textContent = s.maxSafe;
  const st = stability();
  setBadge($('stab'), st.text, st.cls);
  $('faultBox').style.display = s.fault ? 'flex' : 'none';
  $('faultText').textContent = s.fault;
}

function renderPoints() {
  localStorage.setItem(STORE, JSON.stringify(points));
  $('pointsEmpty').style.display = points.length ? 'none' : 'block';
  const body = $('pointsBody');
  body.textContent = '';
  if (!cfg) return;
  const fit = fitBeta(points, cfg.beta, cfg.t25);
  let worstNow = 0, worstAfter = 0;
  points.forEach((p, i) => {
    const now = betaTempC(p.r, cfg.r25, cfg.beta, cfg.t25);
    const after = betaTempC(p.r, fit.r25, fit.beta, cfg.t25);
    worstNow = Math.max(worstNow, Math.abs(now - p.ref));
    worstAfter = Math.max(worstAfter, Math.abs(after - p.ref));
    const tr = document.createElement('tr');
    [p.ref.toFixed(1) + ' °C', p.r.toFixed(1) + ' Ω', p.adc.toFixed(1), now.toFixed(2) + ' °C', signed(now - p.ref),
     after.toFixed(2) + ' °C', signed(after - p.ref)].forEach(v => {
      const td = document.createElement('td'); td.textContent = v; tr.appendChild(td);
    });
    const td = document.createElement('td'), b = document.createElement('button');
    b.className = 'del'; b.textContent = 'Delete';
    b.onclick = () => { points.splice(i, 1); renderPoints(); };
    td.appendChild(b); tr.appendChild(td); body.appendChild(tr);
  });

  if (!fit) {
    ['method', 'fitR25', 'fitBeta', 'fitErr'].forEach(id => $(id).textContent = '--');
    $('fitWarn').textContent = ''; $('snippet').value = '';
    return;
  }
  $('method').textContent = fit.betaFitted ? 'R25 + beta, ' + points.length + ' points' : 'R25 only (beta ' + cfg.beta + ' kept)';
  $('fitR25').textContent = fit.r25.toFixed(1) + ' Ω';
  $('fitBeta').textContent = fit.beta.toFixed(1);
  $('fitErr').textContent = worstNow.toFixed(2) + ' → ' + worstAfter.toFixed(2) + ' °C';
  const warn = [];
  if (fit.beta < 2500 || fit.beta > 5500) warn.push('Beta is far from a normal NTC (2500-5500): check the reference readings.');
  if (fit.betaFitted && points.length === 2) warn.push('Two points fit exactly; add a third to see the real error.');
  if (worstAfter > 3) warn.push('Error after fit is over 3 °C: one point may be bad, or the beta model is not enough.');
  $('fitWarn').textContent = warn.join(' ');
  const day = new Date().toISOString().slice(0, 10);
  $('snippet').value =
    'const float NOMINAL_RESISTANCE = ' + fit.r25.toFixed(1) + ';  // calibrated ' + day + ', ' + points.length + ' point(s)\n' +
    'const float BETA_COEFFICIENT = ' + fit.beta.toFixed(1) + ';';
}

async function send(url, opts) {
  const r = await fetch(url, opts);
  if (!r.ok) throw new Error(await r.text());
}

async function poll() {
  try {
    const ctrl = new AbortController(), timer = setTimeout(() => ctrl.abort(), 2500);
    const s = await (await fetch('/status', { signal: ctrl.signal })).json();
    clearTimeout(timer);
    const first = !cfg;
    cfg = { r25: s.r25, beta: s.beta, t25: s.t25 };
    hist.push({ ms: Date.now(), r: s.r, t: s.t, adc: s.adc });
    hist = hist.filter(h => Date.now() - h.ms <= 60000);
    setBadge($('conn'), 'ONLINE', 'ok');
    render(s);
    if (first) renderPoints();
  } catch (e) {
    setBadge($('conn'), 'OFFLINE', 'bad');
  }
  setTimeout(poll, 1000);
}

$('pwmSlider').oninput = () => { $('pwmSet').textContent = $('pwmSlider').value; };
$('applyBtn').onclick = () => send('/heater?pwm=' + Math.round($('pwmSlider').value * 2.55)).catch(e => alert(e.message));
const heaterOff = () => { $('pwmSlider').value = 0; $('pwmSet').textContent = '0'; send('/heater?pwm=0').catch(e => alert(e.message)); };
$('zeroBtn').onclick = heaterOff;
$('offBtn').onclick = heaterOff;
$('resetBtn').onclick = () => send('/reset', { method: 'POST' }).catch(e => alert(e.message));

$('captureBtn').onclick = () => {
  const ref = parseFloat($('refInput').value);
  if (!isFinite(ref) || ref < -20 || ref > 300) { $('captureMsg').textContent = 'Enter a reference temperature between -20 and 300 °C.'; return; }
  const recent = hist.filter(h => Date.now() - h.ms <= 10000 && h.r != null);
  if (recent.length < 5) { $('captureMsg').textContent = 'Need at least 5 s of valid readings first.'; return; }
  if (!stability().stable && !confirm('Reading is not stable yet. Capture anyway?')) return;
  const avg = k => recent.reduce((s, h) => s + h[k], 0) / recent.length;
  points.push({ ref, r: avg('r'), adc: avg('adc'), at: Date.now() });
  points.sort((a, b) => a.ref - b.ref);
  $('captureMsg').textContent = 'Captured ' + ref.toFixed(1) + ' °C (' + recent.length + ' samples).';
  $('refInput').value = '';
  renderPoints();
};
$('clearBtn').onclick = () => { if (points.length && confirm('Delete all ' + points.length + ' points?')) { points = []; renderPoints(); } };
$('snippet').onclick = () => $('snippet').select();

renderPoints();
poll();
</script>
</body>
</html>
)rawliteral";

#endif
