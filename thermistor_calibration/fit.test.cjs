// Checks the beta-fit math embedded in calibration_dashboard.h. Run: node --test thermistor_calibration/fit.test.cjs
const { it } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const raw = fs.readFileSync(path.join(__dirname, 'calibration_dashboard.h'), 'utf8');
const math = raw.match(/\/\/ fit-math-start([\s\S]*?)\/\/ fit-math-end/)[1];
const ctx = {};
vm.runInNewContext(math + '\nthis.fitBeta = fitBeta; this.betaTempC = betaTempC;', ctx);

const rAt = (t, r25, beta) => r25 * Math.exp(beta * (1 / (t + 273.15) - 1 / 298.15));

it('recovers R25 and beta from spread points', () => {
  const pts = [25, 100, 200].map(ref => ({ ref, r: rAt(ref, 95000, 3900) }));
  const f = ctx.fitBeta(pts, 3950, 25);
  assert.ok(f.betaFitted);
  assert.ok(Math.abs(f.beta - 3900) < 0.01, f.beta);
  assert.ok(Math.abs(f.r25 - 95000) < 0.1, f.r25);
  assert.ok(Math.abs(ctx.betaTempC(pts[2].r, f.r25, f.beta, 25) - 200) < 1e-6);
});

it('keeps datasheet beta for one point or narrow spread', () => {
  for (const refs of [[150], [190, 200]]) {
    const f = ctx.fitBeta(refs.map(ref => ({ ref, r: rAt(ref, 95000, 3950) })), 3950, 25);
    assert.equal(f.betaFitted, false);
    assert.equal(f.beta, 3950);
    assert.ok(Math.abs(f.r25 - 95000) < 0.1, f.r25);
  }
});

it('returns null without points', () => assert.equal(ctx.fitBeta([], 3950, 25), null));
