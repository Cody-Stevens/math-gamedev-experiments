const { fkTrace } = require('../fk.js');
const med = a => { const s = [...a].sort((x, y) => x - y); return s[s.length >> 1]; };
const res = {};
for (const n of [16, 32, 64, 256]) { const Ns = []; let bad = 0; const t0 = Date.now();
  for (let s = 0; s < 400; s++) { const r = fkTrace(n, s * 7 + n, false); if (r.N < 0) bad++; else Ns.push(r.N); }
  res[n] = med(Ns); console.log(n, 'median N', res[n], 'bad', bad, 'ms/curve', ((Date.now() - t0) / 400).toFixed(3)); }
console.log('N ratio 256/16', (res[256] / res[16]).toFixed(1), 'expect ~16^1.75 =', Math.pow(16, 1.75).toFixed(1));
for (const p of [1, 1.75]) console.log('p', p, 'duration ratio 256/16 =', ((res[256] * Math.pow(1/256, p)) / (res[16] * Math.pow(1/16, p))).toFixed(2));
