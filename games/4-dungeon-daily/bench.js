// Dungeon Daily bench: node bench.js [--ms 2000] [--scenes normal,square,hubs,big] [--json out.json]
// Same core.js as index.html. Times each generator over whole layouts (work only; stats outside the timer).
'use strict';
const DD = require('./core.js');
const arg = (k, d) => { const i = process.argv.indexOf('--' + k); return i > 0 ? process.argv[i + 1] : d; };
const MS = +arg('ms', 2000), SC = arg('scenes', 'normal,square,hubs,hubs:18,big').split(','), JSON_OUT = arg('json', '');
const NAMES = { swap: 'PRIOR BEST: lazy double-edge swap', config: 'PRIOR BEST: configuration model', curveball: 'NEW: certified Curveball' };

function runMethod(tpl, job, minMs, minLayouts) {
  const out = [], kept = [], conn = []; let time = 0, units = job.key === 'config' ? 4096 : job.key === 'swap' ? 256 : 4;
  const t0 = performance.now();
  while ((time < minMs || job.layouts < minLayouts) && performance.now() - t0 < 120000) {
    const a = performance.now(); job.work(units, out); time += performance.now() - a;
    for (const L of out) { kept.push(DD.overlap(L.g, tpl.g)); conn.push(DD.isConnected(L.g)); }
    out.length = 0;
  }
  return { time, layouts: job.layouts, attempts: job.attempts, rejects: job.rejects, kept, conn };
}
function hist(arr, m, mask) { const h = new Float64Array(m + 1); let c = 0; arr.forEach((k, i) => { if (!mask || mask[i]) { h[k]++; c++; } }); return Array.from(h, x => x / Math.max(1, c)); }
const mean = a => a.reduce((s, x) => s + x, 0) / a.length;
// expected histogram TV between two FAIR samples of sizes n1, n2 (bootstrap from the reference histogram)
function noiseTV(p, n1, n2, R = DD.makeRng(77)) {
  n1 = Math.min(n1, 40000); const cdf = []; let s = 0; for (const x of p) cdf.push(s += x);
  const draw = n => { const h = new Float64Array(p.length); for (let k = 0; k < n; k++) { const u = R(1e9) / 1e9; let i = 0; while (i < cdf.length - 1 && cdf[i] < u) i++; h[i]++; } return Array.from(h, x => x / n); };
  let t = 0; for (let r = 0; r < 8; r++) t += DD.histTV(draw(n1), draw(n2)); return t / 8;
}
const fmt = (x, d = 3) => x === undefined || x === null || Number.isNaN(x) ? 'n/a' : (Math.abs(x) >= 100 ? x.toFixed(0) : Math.abs(x) >= 1 ? x.toFixed(2) : x.toPrecision(d));

const results = { machine: require('os').cpus()[0].model, node: process.version, scenes: {} };
console.log(`Dungeon Daily bench  (${results.machine}, node ${process.version}, >= ${MS} ms per method)\n`);
for (const key of SC) {
  const [sk, sq] = key.split(':'), tpl = DD.SCENES[sk].make(sq ? +sq : undefined), m = tpl.m, T = DD.certifiedT(tpl.n);
  const lam = tpl.deg.reduce((s, d) => s + d * (d - 1), 0) / tpl.deg.reduce((s, d) => s + d, 0);
  // REFERENCE: long-run swap chain, 100 swaps per door between samples
  const ref = DD.makeReference(tpl, 99), rk = [], rc = [];
  for (let k = 0; k < 4000; k++) { const g = ref.sample(); rk.push(DD.overlap(g, tpl.g)); rc.push(DD.isConnected(g)); }
  const refMean = mean(rk), refH = hist(rk, m), refHc = hist(rk, m, rc);
  const half = DD.histTV(hist(rk.slice(0, 2000), m), hist(rk.slice(2000), m));
  console.log(`== ${key}: ${DD.SCENES[sk].title}${sq ? ` [square=${sq}]` : ''}  n=${tpl.n} doors=${m} door counts ${Math.min(...tpl.deg)}..${Math.max(...tpl.deg)}  certified T=${T.toLocaleString()}  (sum d(d-1)/sum d = ${lam.toFixed(2)})`);
  console.log(`   REFERENCE long-run swap chain: mean doors kept ${refMean.toFixed(3)}, connected ${(100 * mean(rc)).toFixed(1)}%, split-half hist TV (noise floor) ${half.toFixed(3)}`);
  const { jobs } = DD.makeJobs(tpl, 7);
  const rows = [];
  for (const mk of ['swap', 'config', 'curveball']) {
    const r = runMethod(tpl, jobs[mk], MS, mk === 'swap' ? 1000 : 30);
    const mk_ = mean(r.kept), cfrac = mean(r.conn), b = DD.bias(mk_, refMean, m);
    const row = { method: NAMES[mk], ms_per_layout: r.time / r.layouts, layouts: r.layouts,
      attempts_per_layout: r.attempts / r.layouts, reject_rate: r.rejects / r.attempts, connected: cfrac,
      ms_per_connected_layout: r.time / (r.layouts * cfrac), mean_kept: mk_, bias: b,
      hist_tv: r.layouts >= 30 ? DD.histTV(hist(r.kept, m), refH) : null, hist_tv_connected: r.layouts >= 30 ? DD.histTV(hist(r.kept, m, r.conn), refHc) : null,
      hist_tv_noise: noiseTV(refH, r.layouts, rk.length) };
    rows.push(row);
  }
  console.log('   method                               ms/layout   layouts  attempts/layout  rejected  connected  ms/connected  kept avg  bias   histTV  (noise)  histTV(conn)');
  for (const r of rows) console.log('   ' + r.method.padEnd(36) + fmt(r.ms_per_layout, 3).padStart(10) + String(r.layouts).padStart(10) + fmt(r.attempts_per_layout).padStart(17) +
    ((100 * r.reject_rate).toFixed(3) + '%').padStart(10) + ((100 * r.connected).toFixed(1) + '%').padStart(11) + fmt(r.ms_per_connected_layout).padStart(14) +
    r.mean_kept.toFixed(2).padStart(10) + r.bias.toFixed(3).padStart(7) + fmt(r.hist_tv).padStart(9) + ('(' + r.hist_tv_noise.toFixed(3) + ')').padStart(9) + fmt(r.hist_tv_connected).padStart(13));
  const [sw, cf, cb] = rows;
  const win = cf.ms_per_layout < cb.ms_per_layout ? `PRIOR (configuration model) is ${(cb.ms_per_layout / cf.ms_per_layout).toFixed(1)}x faster than NEW and exactly fair`
    : `NEW (certified Curveball) is ${(cf.ms_per_layout / cb.ms_per_layout).toFixed(1)}x faster than the exact prior`;
  console.log(`   -> ${win}\n`);
  results.scenes[key] = { n: tpl.n, doors: m, deg_max: Math.max(...tpl.deg), certified_T: T, ref_mean_kept: refMean, ref_connected: mean(rc), ref_split_half_tv: half, rows };
}

// Exact checks on enumerable toys
console.log('== exact TV to uniform on enumerable toys (full law propagated / every stub pairing enumerated)');
const toys = [{ name: 'A n=6 d=(2,...,2)', d: [2, 2, 2, 2, 2, 2], start: [[0, 1], [1, 2], [0, 2], [3, 4], [4, 5], [3, 5]] },
  { name: 'B n=8 d=(5,4,3,3,2,2,2,1)', d: [5, 4, 3, 3, 2, 2, 2, 1] }];
results.toys = {};
for (const t of toys) {
  const X = DD.exactToy(t.d, t.start || DD.havelHakimi(t.d)), cb = X.curve(X.Kcb, X.T), sw = X.curve(X.Ksw, X.m);
  console.log(`   ${t.name}: ${X.S} layouts; swap @ ${X.m} swaps (1/door) TV ${sw[X.m].toPrecision(3)};  Curveball @ ${X.m} trades TV ${cb[X.m].toPrecision(3)};  certified Curveball @ ${X.T} trades TV ${cb[X.T].toPrecision(3)} (bound ${X.bound(X.T).toPrecision(3)})`);
  results.toys[t.name] = { layouts: X.S, swap_tv_at_m: sw[X.m], cb_tv_at_m: cb[X.m], cb_tv_at_T: cb[X.T], T: X.T };
}
for (const d of [[2, 2, 2, 2, 2, 2], [4, 3, 2, 2, 2, 2, 1], [6, 5, 2, 2, 2, 1, 1, 1]]) {
  const C = DD.exactConfigToy(d);
  console.log(`   configuration model d=(${d}): ${C.pairings.toLocaleString()} pairings, ${C.graphs} layouts, each hit by exactly ${C.perGraph.join('/')} pairings -> TV ${C.tv}; acceptance ${(100 * C.accept).toFixed(3)}%`);
  results.toys['config ' + d.join(',')] = C;
}
if (JSON_OUT) require('fs').writeFileSync(JSON_OUT, JSON.stringify(results, null, 1));
