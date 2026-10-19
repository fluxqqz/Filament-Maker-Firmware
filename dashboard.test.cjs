// ponytail: minimal dependency-free regression harness using node:test and node:vm fake DOM
const { describe, it } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const raw = fs.readFileSync(path.join(__dirname, 'dashboard.h'), 'utf8');
const html = raw.match(/R"rawliteral\(([\s\S]*?)\)rawliteral"/)[1];
const js = html.match(/<script>([\s\S]*?)<\/script>/)[1];

function makeEl(id, init = {}) {
  const el = { id, _t: init.innerText || '', className: init.className || '', value: '', min: init.min || '', max: init.max || '', style: {}, dataset: {} };
  Object.defineProperty(el, 'innerText', { get: () => el._t, set: v => { el._t = String(v); } });
  Object.defineProperty(el, 'innerHTML', { get: () => el._t, set: v => { el._t = String(v); } });
  el.classList = {
    add: (...c) => { el.className = [...new Set([...el.className.split(' ').filter(Boolean), ...c])].join(' '); },
    remove: (...c) => { el.className = el.className.split(' ').filter(x => !c.includes(x)).join(' '); }
  };
  return el;
}

function createDOM() {
  const els = {};
  for (const [, tag, a, id] of html.matchAll(/<(\w+)([^>]*\bid=["']([^"']+)["'][^>]*)>/g)) {
    const attr = k => (a.match(new RegExp(`\\b${k}=["']([^"']+)["']`)) || [])[1] || '';
    const txt = (html.match(new RegExp(`<${tag}[^>]*\\bid=["']${id}["'][^>]*>([\\s\\S]*?)<\\/${tag}>`)) || [])[1] || '';
    els[id] = makeEl(id, { className: attr('class'), min: attr('min'), max: attr('max'), innerText: txt.replace(/<[^>]+>/g, '').trim() });
  }
  return { els, doc: { activeElement: null, getElementById: id => els[id] } };
}

const STATUS = {
  temp: 215.3, setpoint: 210.0, rpm: 15.0, pwm: 128, heater: true, stepper: true, step: 1,
  step1sp: 100.0, step2sp: 200.0, stable: true, fault: false, faultMsg: '', ip: '192.168.1.100', hostname: 'filament-maker-test.local'
};

function setupEnv(fetchMock) {
  const { els, doc } = createDOM();
  const timeouts = new Map();
  let tId = 1, intervalCb = null, calls = [];
  const fetch = (url, opts = {}) => {
    calls.push({ url, opts });
    return fetchMock ? fetchMock(url, opts, calls.length) : Promise.resolve({ ok: true, json: async () => STATUS, text: async () => 'OK' });
  };
  const ctx = vm.createContext({
    document: doc, fetch,
    setTimeout: (fn, ms) => (timeouts.set(tId, { fn, ms }), tId++),
    clearTimeout: id => timeouts.delete(id),
    setInterval: fn => (intervalCb = fn, 1),
    clearInterval: () => {},
    AbortController, Number, Math, parseFloat, isNaN, encodeURIComponent, Promise, Error, console
  });
  vm.runInContext(js, ctx);
  return { els, doc, ctx, calls, timeouts, poll: () => intervalCb(), flush: () => new Promise(r => setImmediate(r)) };
}

describe('Dashboard Web UI & Polling Regression Suite', () => {
  it('initial markup truthful awaiting state/no sidebar', () => {
    const { els } = createDOM();
    const expected = {
      connStatusText: 'Connecting...', systemBadgeText: 'Awaiting device status',
      liveStatusText: 'Readings are unconfirmed until the device responds.', ipDisplay: 'Waiting for device address',
      tempBig: '0.0', setpointBig: '0.0', pwmPct: '0', heaterTileVal: 'OFF', stepperTileVal: 'OFF', activeStageBadge: 'IDLE'
    };
    for (const [k, v] of Object.entries(expected)) assert.equal(els[k].innerText, v);
    for (const k of ['connDot', 'systemBadge', 'sysBadgeDot']) assert.match(els[k].className, /\bpending\b/);
    assert.doesNotMatch(html, /<aside\b|\bid=["']sidebar["']|\bclass=["'][^"']*\bsidebar\b|<nav\b/i);
  });

  it('all JS literal getElementById references exist unique in HTML', () => {
    const ids = [...new Set([...js.matchAll(/getElementById\s*\(\s*["']([^"']+)["']/g)].map(m => m[1]))];
    assert.ok(ids.length >= 30);
    for (const id of ids) assert.equal([...html.matchAll(new RegExp(`\\bid=["']${id}["']`, 'g'))].length, 1, `ID ${id} must be unique in HTML`);
  });

  it('usable response restores connected/online', async () => {
    const env = setupEnv();
    await env.flush();
    for (const [k, v] of Object.entries({ connStatusText: 'Connected', connDot: 'status-dot', systemBadge: 'badge-online', systemBadgeText: 'SYSTEM ONLINE', liveStatusText: 'Live readings refresh every 500 ms.' })) {
      assert.equal(k.includes('Dot') || k === 'systemBadge' ? env.els[k].className : env.els[k].innerText, v);
    }
    const expected = {
      tempBig: '215.3', setpointBig: '210.0', pwmPct: '50', pwmRaw: '128', rpmVal: '15.0',
      targetRpmText: '15.0', thermalStateText: 'Stable at Setpoint', ipDisplay: '192.168.1.100 (filament-maker-test.local)',
      heaterTileVal: 'ON', stepperTileVal: 'RUNNING', activeStageBadge: 'STEP 1 ACTIVE', step1Pill: 'STABLE', step1ToggleBtn: 'Stop Stage 1'
    };
    for (const [k, v] of Object.entries(expected)) assert.equal(env.els[k].innerText, v);
  });

  it('Stage 1 Stage 2 motor/manual handlers endpoints unchanged', async () => {
    let curStatus = { ...STATUS, step: 1, heater: true, stepper: true };
    const env = setupEnv(url => url === '/status' ? Promise.resolve({ ok: true, json: async () => curStatus }) : Promise.resolve({ ok: true }));
    await env.flush();
    const testCall = (action, expectedUrl) => { env.calls.length = 0; action(); assert.equal(env.calls[0]?.url, expectedUrl); };

    for (const [fn, url] of [
      [() => env.ctx.heaterOn(), '/heater?state=on'], [() => env.ctx.heaterOff(), '/heater?state=off'],
      [() => env.ctx.stepperOn(), '/stepper?state=on'], [() => env.ctx.stepperOff(), '/stepper?state=off'],
      [() => env.ctx.toggleStep1(), '/step1?state=off'], [() => env.ctx.toggleStep2(), '/step2?state=on'],
      [() => env.ctx.toggleStep3(), '/step3?state=off']
    ]) testCall(fn, url);

    curStatus = { ...STATUS, step: 2, heater: true, stepper: false };
    env.poll();
    await env.flush();
    testCall(() => env.ctx.toggleStep1(), '/step1?state=on');
    testCall(() => env.ctx.toggleStep2(), '/step2?state=off');
    testCall(() => env.ctx.toggleStep3(), '/step3?state=on');

    env.els.step1SPInput.value = '165'; testCall(() => env.ctx.setStep1SP(env.els.step1ApplyBtn), '/step1setpoint?value=165');
    env.els.step2SPInput.value = '215'; testCall(() => env.ctx.setStep2SP(env.els.step2ApplyBtn), '/step2setpoint?value=215');
    env.els.rpmInput.value = '25.5'; testCall(() => env.ctx.setRPM(env.els.rpmApplyBtn), '/rpm?value=25.5');
    testCall(() => env.ctx.resetFault(), '/resetfault');
  });

  it('fault display retained on disconnect', async () => {
    let p = 0;
    const env = setupEnv(url => url === '/status' && (++p === 1
      ? Promise.resolve({ ok: true, json: async () => ({ ...STATUS, fault: true, faultMsg: 'OVERTEMP HAZARD' }) })
      : Promise.reject(new Error('fail'))));
    await env.flush();
    assert.equal(env.els.faultBanner.style.display, 'flex');
    assert.equal(env.els.faultText.innerText, 'OVERTEMP HAZARD');
    assert.equal(env.els.systemBadgeText.innerText, 'SAFETY FAULT');

    env.poll();
    await env.flush();
    assert.equal(env.els.connStatusText.innerText, 'Disconnected');
    assert.equal(env.els.systemBadgeText.innerText, 'Status stale');
    assert.equal(env.els.faultBanner.style.display, 'flex');
    assert.equal(env.els.faultText.innerText, 'OVERTEMP HAZARD');
  });

  it('HTTP/malformed JSON/schema failures mark stale/unknown without overwriting last good values', async () => {
    const cases = [
      { name: 'init 500', init: null, resp: () => Promise.resolve({ ok: false, status: 500 }), badge: 'Status unknown' },
      { name: 'post-good 500', init: { ...STATUS, temp: 212.4 }, resp: () => Promise.resolve({ ok: false, status: 500 }), badge: 'Status stale', temp: '212.4' },
      { name: 'malformed json', init: STATUS, resp: () => Promise.resolve({ ok: true, json: () => { throw new SyntaxError(); } }), badge: 'Status stale', temp: '215.3' },
      { name: 'schema missing temp', init: STATUS, resp: () => Promise.resolve({ ok: true, json: async () => ({ setpoint: 200, heater: true }) }), badge: 'Status stale', temp: '215.3' },
      { name: 'schema invalid type', init: STATUS, resp: () => Promise.resolve({ ok: true, json: async () => ({ ...STATUS, temp: 'bad' }) }), badge: 'Status stale', temp: '215.3' },
      { name: 'network drop', init: STATUS, resp: () => Promise.reject(new Error('drop')), badge: 'Status stale', temp: '215.3' },
    ];
    for (const c of cases) {
      let p = 0;
      const env = setupEnv(url => url === '/status' && (++p === 1 && c.init
        ? Promise.resolve({ ok: true, json: async () => c.init })
        : c.resp()));
      await env.flush();
      if (c.init) { env.poll(); await env.flush(); }
      assert.equal(env.els.systemBadgeText.innerText, c.badge, `${c.name} badge failed`);
      if (c.temp) assert.equal(env.els.tempBig.innerText, c.temp, `${c.name} temp overwritten`);
    }
  });

  it('hung status fetch aborted at 3s and subsequent poll recovers', async () => {
    let p = 0, aborted = false;
    const env = setupEnv((url, opts) => url === '/status' && (++p === 1
      ? new Promise((_, rej) => opts.signal?.addEventListener('abort', () => { aborted = true; rej(new Error('aborted')); }))
      : Promise.resolve({ ok: true, json: async () => STATUS })));
    await env.flush();
    const abortTimeout = [...env.timeouts.values()].find(t => t.ms === 3000);
    assert.ok(abortTimeout, 'Must schedule 3000ms abort timeout');
    assert.equal(aborted, false);
    assert.equal(env.els.connStatusText.innerText, 'Connecting...');

    abortTimeout.fn();
    await env.flush();
    assert.equal(aborted, true);
    assert.equal(env.els.connStatusText.innerText, 'Disconnected');
    assert.equal(env.els.systemBadgeText.innerText, 'Status unknown');

    env.poll();
    await env.flush();
    assert.equal(env.els.connStatusText.innerText, 'Connected');
    assert.equal(env.els.systemBadgeText.innerText, 'SYSTEM ONLINE');
    assert.equal(env.els.tempBig.innerText, '215.3');
  });

  it('focused inputs not overwritten', async () => {
    let cur = { ...STATUS, step1sp: 100.0, step2sp: 200.0, rpm: 15.0 };
    const env = setupEnv(url => url === '/status' && Promise.resolve({ ok: true, json: async () => cur }));
    await env.flush();
    assert.equal(env.els.step1SPInput.value, 100);

    env.doc.activeElement = env.els.step1SPInput;
    env.els.step1SPInput.value = 245;
    cur = { ...cur, step1sp: 120.0, step2sp: 210.0, rpm: 18.0 };
    env.poll();
    await env.flush();

    assert.equal(env.els.step1SPInput.value, 245, 'Focused step1SPInput must not be overwritten');
    assert.equal(env.els.step2SPInput.value, 210, 'Unfocused step2SPInput must update');
    assert.equal(env.els.rpmInput.value, 18, 'Unfocused rpmInput must update');

    env.doc.activeElement = null;
    env.poll();
    await env.flush();
    assert.equal(env.els.step1SPInput.value, 120, 'Blurred step1SPInput must update on next poll');
  });

  it('Stop All still all five endpoints', async () => {
    const env = setupEnv();
    await env.flush();
    env.calls.length = 0;
    env.ctx.stopAll();

    assert.equal(env.els.estopBtn.innerText, 'STOPPING...');
    assert.deepEqual(env.calls.map(c => c.url), [
      '/step1?state=off', '/step2?state=off', '/step3?state=off', '/heater?state=off', '/stepper?state=off'
    ]);

    await env.flush();
    const resetTimeout = [...env.timeouts.values()].find(t => t.ms === 800);
    assert.ok(resetTimeout, 'Must schedule 800ms reset timer');
    resetTimeout.fn();
    assert.equal(env.els.estopBtn.innerHTML, '&#9632; STOP ALL');
  });
});
