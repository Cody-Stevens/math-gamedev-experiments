const { EC_RX, EC_STARTS, ecLint, ecStep } = require('../econ.js');
for (const [label, active] of [['full', EC_RX.map(() => true)], ['broken', EC_RX.map((_, i) => i !== 1 && i !== 6)]]) {
  const lint = ecLint(active), k = EC_RX.map(r => r[2]);
  let dead = 0, run_ = 0, lo = 1e99, hi = 0;
  for (const s of EC_STARTS) { const r = { y: s.map(Math.log), t: 0, stopped: false }; ecStep(r, active, k, 120);
    r.y.forEach(v => { const x = Math.exp(v); if (x < 1e-6) dead++; if (x > 1e6) run_++; lo = Math.min(lo, x); hi = Math.max(hi, x); }); }
  console.log(label, 'lint bad:', lint.map((v, i) => v ? null : i).filter(v => v !== null), 'dead', dead, 'runaway', run_, 'range', lo.toExponential(2), hi.toExponential(2));
}
