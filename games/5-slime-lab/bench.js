// node bench.js [--ns 32,48,64] [--seeds 8] [--quick] [--explore] [--json out/bench.json]
// Headline numbers for Slime Lab, measured in Node with the same core.js the game inlines.
// Single-threaded JS. Solver time = MBO compute only (initial threshold, iterations, prolongation);
// the 2 % stopping test uses the paper's I(V) and is NOT timed (a real solver could not run it).
'use strict';
const C = require('./core.js');
const fs = require('fs');
const arg = (k, d) => { const i = process.argv.indexOf('--' + k); return i > 0 ? process.argv[i + 1] : d; };
const has = k => process.argv.includes('--' + k);
const Ns = arg('ns', '32,48,64').split(',').map(Number);
const SEEDS = +arg('seeds', has('quick') ? 3 : 8);
const Vs = [0.10, 0.25, 0.40];
const SIG = 1 / 16;            // MBO time step: Gaussian sigma = 1/16 of the cell (as demos\L2-blob-ladder)
const COARSE_SIG = 1 / +arg('coarse-sigma', 6); // coarse stage sigma (1/6 cell: the fastest, never-stuck setting found by --explore, see README)
const CAP = 400;
const f1 = x => x.toFixed(1), f2 = x => x.toFixed(2), pct = x => (x >= 0 ? '+' : '') + (100 * x).toFixed(1) + '%';
const fmtMs = x => isFinite(x) ? (x < 10 ? f2(x) : f1(x)) : 'never';

// one cold run to the 2 % target (oracle untimed); continues until the solver settles by its own rule
function runToTarget(N, V, seed, coarse, coarseSig) {
  const s = new C.Solver({ N, V, seed, sigmaPhys: SIG, coarse, coarseSigmaPhys: coarseSig, blur: 'box' });
  const est = new C.AreaEst(N), I = C.I_unit(s.k / s.n);
  let hitMs = Infinity, hitIt = -1, guard = 0;
  while (guard++ < CAP + 60) {
    s.step();
    if (s.stage === 'fine') {
      const A = est.area(s.fine.chi);           // untimed oracle check
      if (A <= 1.02 * I) { hitMs = s.ms; hitIt = s.iters; break; }
      if (s.settled) break;
    }
  }
  const A = est.area(s.fine.chi), info = C.classify(s.fine.chi, N);
  return { hitMs, hitIt, iters: s.iters, ms: s.ms, msPerIt: s.ms / Math.max(1, s.iters), excess: A / I - 1, shape: info.name, ok: C.shapeMatches(info, s.k / s.n) };
}

function closedFormStats(N) {
  const n = N ** 3, cf = new C.ClosedForm(N), est = new C.AreaEst(N), times = [], bias = [];
  for (const V of Vs) {
    const k = Math.round(V * n);
    for (let r = 0; r < 3; r++) cf.build(k); // warm-up
    for (let r = 0; r < 15; r++) { const t = C.now(); cf.build(k); times.push(C.now() - t); }
    bias.push(est.area(cf.chi) / C.I_unit(k / n) - 1);
  }
  return { ms: C.median(times), bias };
}

function sweep(N, coarse, coarseSig) {
  const runs = [];
  for (const V of Vs) for (let s = 0; s < SEEDS; s++) runs.push(Object.assign({ V, seed: 1000 + 37 * s }, runToTarget(N, V, 1000 + 37 * s, coarse, coarseSig)));
  const ms = runs.map(r => r.hitMs), fails = runs.filter(r => !isFinite(r.hitMs));
  return { runs, med: C.median(ms), fails: fails.length, n: runs.length, msPerIt: C.median(runs.map(r => r.msPerIt)), itMed: C.median(runs.filter(r => isFinite(r.hitMs)).map(r => r.hitIt)), stuck: fails.map(r => `V=${r.V} seed ${r.seed}: ${r.shape} ${pct(r.excess)}`) };
}

// ------------------------------------------------------------ obstacle (speculation, measured only)
const PILLAR = { r: 0.12, cx: 0.5, cz: 0.5 };
function runSettle(N, V, seed, mode) { // mode: 'cold' | 'centred' | 'placed'
  const w = mode === 'cold' ? null : C.pillarWarmStart(N, V, PILLAR, mode);
  const s = new C.Solver({ N, V, seed, sigmaPhys: SIG, coarse: true, coarseSigmaPhys: COARSE_SIG, blur: 'box', pillar: PILLAR, warmChi: w && w.chi });
  const est = new C.AreaEst(N);
  const A0 = w ? est.area(w.chi, s.maskF) : NaN;
  let guard = 0; while (!s.settled && guard++ < CAP + 60) s.step();
  return { A0, A: est.area(s.fine.chi, s.maskF), iters: s.iters, ms: s.ms + (w ? w.ms : 0), warmMs: w ? w.ms : 0, shape: C.classify(s.fine.chi, N, s.maskF).name, choice: w && w.choice };
}

function main() {
  const out = { when: new Date().toISOString(), node: process.version, seeds: SEEDS, volumes: Vs, sigma: SIG, coarseSigma: COARSE_SIG, grids: [], pillar: [] };
  if (has('explore')) { // pick the strongest coarse stage sigma for the baseline
    for (const N of Ns) for (const cs of (arg("cs-list", "16,12,10,8,6,5,4")).split(",").map(x => 1 / +x)) {
      const r = sweep(N, true, cs);
      console.log(`N=${N} coarse sigma 1/${Math.round(1 / cs)}: median ms to 2% ${fmtMs(r.med)}  fails ${r.fails}/${r.n}  ${r.stuck.join('; ')}`);
    }
    return;
  }
  console.log(`Slime Lab bench · node ${process.version} · single thread · V = ${Vs.join('/')} x ${SEEDS} seeds · sigma 1/16 cell, coarse stage 1/${Math.round(1 / COARSE_SIG)} cell on an N/2 grid`);
  console.log('');
  console.log('| grid | plain MBO ms to 2% (stuck) | coarse-to-fine MBO ms to 2% (stuck) | MBO ms/iter at N | c2f its to 2% (coarse+fine) | closed form build ms | ratio c2f/closed | estimator bias on exact shape (V=.10/.25/.40) |');
  console.log('|---|---|---|---|---|---|---|---|');
  for (const N of Ns) {
    const plain = sweep(N, false), c2f = sweep(N, true, COARSE_SIG), cf = closedFormStats(N);
    const row = { N, plain: { med: plain.med, fails: plain.fails, n: plain.n, msPerIt: plain.msPerIt, stuck: plain.stuck },
      c2f: { med: c2f.med, fails: c2f.fails, n: c2f.n, msPerIt: c2f.msPerIt, itMed: c2f.itMed, stuck: c2f.stuck, runs: c2f.runs }, closed: cf };
    out.grids.push(row);
    console.log(`| ${N}^3 | ${fmtMs(plain.med)} (${plain.fails}/${plain.n}) | ${fmtMs(c2f.med)} (${c2f.fails}/${c2f.n}) | ${f2(plain.msPerIt)} | ${c2f.itMed} | ${f2(cf.ms)} | x${isFinite(c2f.med) ? Math.round(c2f.med / cf.ms) : 'inf'} | ${cf.bias.map(pct).join(' / ')} |`);
  }
  for (const g of out.grids) if (g.c2f.stuck.length || g.plain.stuck.length) console.log(`  ${g.N}^3 stuck: plain [${g.plain.stuck.join('; ')}] c2f [${g.c2f.stuck.join('; ')}]`);
  console.log('');
  const NP = +arg('pillar-n', 48), PS = Math.min(SEEDS, 8);
  console.log(`Pillar scene (speculation, measured only): N=${NP}^3, pillar radius ${PILLAR.r}, slime touching the pillar costs no surface; ${PS} cold seeds per V.`);
  console.log('All three stop by the same self-check (no proven target exists here): no voxel moves, or <=0.05% move for 5 iterations in a row.');
  console.log('centred = theorem shape centred on the pillar; placed = theorem shape tried at 7 offsets x orientations, scored on the N/2 grid, best kept (timed).');
  console.log('| V | cold c2f MBO: its / ms / final area (median, best) | warm centred: its / ms / start -> final area | warm placed: its / ms (placement ms) / start -> final area | placed vs cold seeds: lower / higher |');
  console.log('|---|---|---|---|---|');
  for (const V of Vs) {
    const cold = [];
    for (let s = 0; s < PS; s++) cold.push(runSettle(NP, V, 2000 + 53 * s, 'cold'));
    const wc = runSettle(NP, V, 0, 'centred'), wp = runSettle(NP, V, 0, 'placed');
    const lower = cold.filter(c => wp.A < c.A * (1 - 1e-3)).length, higher = cold.filter(c => wp.A > c.A * (1 + 1e-3)).length;
    out.pillar.push({ V, cold, warmCentred: wc, warmPlaced: wp, placedLower: lower, placedHigher: higher });
    const shapes = {}; cold.forEach(c => shapes[c.shape] = (shapes[c.shape] || 0) + 1);
    console.log(`| ${V} | ${C.median(cold.map(c => c.iters))} / ${f1(C.median(cold.map(c => c.ms)))} / ${C.median(cold.map(c => c.A)).toFixed(3)}, ${Math.min(...cold.map(c => c.A)).toFixed(3)} (${Object.entries(shapes).map(([k, v]) => v + 'x ' + k).join(', ')}) | ${wc.iters} / ${f1(wc.ms)} / ${wc.A0.toFixed(3)} -> ${wc.A.toFixed(3)} | ${wp.iters} / ${f1(wp.ms)} (${f1(wp.warmMs)}) / ${wp.A0.toFixed(3)} -> ${wp.A.toFixed(3)} (${wp.shape}) | ${lower}/${PS} / ${higher}/${PS} |`);
  }
  const jf = arg('json', 'bench.json');
  fs.writeFileSync(jf, JSON.stringify(out, (k, v) => (typeof v === 'number' && !isFinite(v)) ? null : v, 1));
  console.log('\nwrote ' + jf);
}
main();
