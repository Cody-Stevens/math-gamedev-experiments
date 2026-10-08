// Colony Ledger core: recipe network, weak-reversibility lint + one-click fix, log-space RK4 town
// simulation (with or without clamps), integer stockpile mode, and the Monte-Carlo balance sweep.
// Network, base rates and the lint rule come from panel/web/econ.js (itself from demos/01-economy).
// Used by index.html (inlined by build.py) and by bench.js (require).
'use strict';
const CL_SP = ['Ore', 'Wood', 'Tools', 'Food', 'Workers', 'Sick', 'Immune'];
const CL_SH = ['O', 'W', 'T', 'F', 'P', 'I', 'R'];
function clVec(o) { const v = [0, 0, 0, 0, 0, 0, 0]; for (const s in o) v[CL_SH.indexOf(s)] = o[s]; return v; }

// The nine econ.js recipes (same rates) plus a SIRS-style plague on the workers.
const CL_CATALOG = [
  { id: 'eat', g: 'village', from: { F: 1, P: 1 }, to: { P: 2 }, k: 1.00, name: 'feed & recruit' },
  { id: 'crowd', g: 'village', from: { P: 2 }, to: { P: 1 }, k: 0.35, name: 'crowding' },
  { id: 'farm', g: 'village', from: { P: 1 }, to: { F: 1, P: 1 }, k: 0.60, name: 'farm' },
  { id: 'craft', g: 'forge', from: { O: 1, W: 1 }, to: { T: 1 }, k: 1.20, name: 'forge tools' },
  { id: 'melt', g: 'forge', from: { T: 1 }, to: { O: 2 }, k: 0.25, name: 'melt down' },
  { id: 'trade', g: 'forge', from: { O: 2 }, to: { O: 1, W: 1 }, k: 0.80, name: 'trade ore for wood' },
  { id: 'salvage', g: 'workshop', from: { T: 1, F: 1, P: 1 }, to: { O: 1, W: 1, P: 1 }, k: 0.90, name: 'salvage' },
  { id: 'build', g: 'workshop', from: { O: 1, W: 1, P: 1 }, to: { T: 1, P: 2 }, k: 0.50, name: 'craft & recruit' },
  { id: 'retire', g: 'workshop', from: { T: 1, P: 2 }, to: { T: 1, F: 1, P: 1 }, k: 0.70, name: 'retire to farm' },
  { id: 'contagion', g: 'plague', from: { P: 1, I: 1 }, to: { I: 2 }, k: 0.90, name: 'contagion' },
  { id: 'recover', g: 'plague', from: { I: 1 }, to: { R: 1 }, k: 0.50, name: 'recovery' },
  { id: 'wane', g: 'plague', from: { R: 1 }, to: { P: 1 }, k: 0.15, name: 'immunity fades' },
];
// Names and rates the fix uses when the return recipe it needs is a known one.
const CL_KNOWN_RETURNS = [
  { from: { P: 2 }, to: { P: 1 }, k: 0.35, name: 'crowding', g: 'village' },
  { from: { T: 1, F: 1, P: 1 }, to: { O: 1, W: 1, P: 1 }, k: 0.90, name: 'salvage', g: 'workshop' },
  { from: { I: 2 }, to: { P: 1, I: 1 }, k: 0.30, name: 'nursing (2 sick: 1 gets well)', g: 'plague' },
  { from: { P: 1 }, to: { I: 1 }, k: 0.02, name: 'traders carry plague in', g: 'plague' },
];
const CL_FIX_RATE = 0.4; // rate for a return recipe that is not in the list above (any positive value is covered)

function clRecipe(c, active) {
  return { id: c.id, g: c.g, from: clVec(c.from), to: clVec(c.to), k: c.k, name: c.name, active: active !== false, added: false };
}
function clName(v) { const s = []; v.forEach((c, i) => { if (c) s.push((c > 1 ? c + ' ' : '') + CL_SP[i]); }); return s.join(' + ') || 'nothing'; }
function clKey(v) { return v.join(','); }

// ---------------------------------------------------------------- scenes (recipe sets + run settings)
const CL_SCENES = {
  broken: { label: 'broken chain', off: ['crowd', 'salvage'], plague: false },
  fixed: { label: 'fixed chain', off: [], plague: false },
  plague: { label: 'plague (SIRS, fails lint)', off: [], plague: true },
  endemic: { label: 'plague + 2 return recipes', off: [], plague: true, fix: true },
  slowrot: { label: 'broken, slow economy (rates x0.2)', off: ['crowd', 'salvage'], plague: false, rateScale: 0.2, speed: 2 },
  starve: { label: 'lint passes but towns starve', off: [], plague: false, rateMul: { farm: 0.004, retire: 0.004 } },
  tiny: { label: 'integer stockpiles (small towns)', off: [], plague: false, integer: 4 },
};
function clSceneRecipes(name) {
  const sc = CL_SCENES[name] || CL_SCENES.broken;
  const rs = CL_CATALOG.filter(c => sc.plague || c.g !== 'plague').map(c => clRecipe(c, !sc.off.includes(c.id)));
  if (sc.rateMul) rs.forEach(r => { if (sc.rateMul[r.id]) r.k *= sc.rateMul[r.id]; });
  if (sc.fix) clFix(rs);
  return rs;
}

// ---------------------------------------------------------------- the lint
// A recipe passes iff its output bundle can reach its input bundle through active recipes
// (weak reversibility, checked recipe by recipe). Same rule as econ.js ecLint.
function clLint(recipes) {
  const keys = new Map(), node = v => { const k = clKey(v); if (!keys.has(k)) keys.set(k, keys.size); return keys.get(k); };
  const act = recipes.filter(r => r.active);
  const E = act.map(r => [node(r.from), node(r.to)]);
  const n = keys.size, reach = [];
  for (let i = 0; i < n; i++) { const row = new Uint8Array(n); row[i] = 1; reach.push(row); }
  for (let e = 0; e < E.length; e++) reach[E[e][0]][E[e][1]] = 1;
  for (let k = 0; k < n; k++) for (let i = 0; i < n; i++) if (reach[i][k]) { const ri = reach[i], rk = reach[k]; for (let j = 0; j < n; j++) if (rk[j]) ri[j] = 1; }
  const ok = new Map(); let bad = 0;
  act.forEach((r, e) => { const pass = !!reach[E[e][1]][E[e][0]]; ok.set(r, pass); if (!pass) bad++; });
  return { ok, bad, pass: bad === 0, nComplex: n, reach, node: k => keys.get(k), keys };
}

// One-click fix: while some recipe a->b lacks a way back, find a bundle c reachable from b that sits in a
// terminal group (everything c reaches reaches c back) and a bundle u that reaches a and sits in an initial
// group, then add the recipe c->u. That closes a loop through a->b and merges at least two groups.
function clFix(recipes) {
  const added = [];
  for (let guard = 0; guard < 50; guard++) {
    const L = clLint(recipes);
    if (L.pass) break;
    const r = recipes.find(x => x.active && !L.ok.get(x));
    const a = L.node(clKey(r.from)), b = L.node(clKey(r.to)), R = L.reach, n = L.nComplex;
    const terminal = c => { for (let j = 0; j < n; j++) if (R[c][j] && !R[j][c]) return false; return true; };
    const initial = u => { for (let j = 0; j < n; j++) if (R[j][u] && !R[u][j]) return false; return true; };
    // breadth-first from b so the nearest terminal bundle is used
    const seen = new Uint8Array(n), q = [b]; seen[b] = 1; let c = -1;
    const adj = Array.from({ length: n }, () => []);
    recipes.forEach(x => { if (x.active) adj[L.node(clKey(x.from))].push(L.node(clKey(x.to))); });
    while (q.length) { const v = q.shift(); if (terminal(v)) { c = v; break; } adj[v].forEach(w => { if (!seen[w]) { seen[w] = 1; q.push(w); } }); }
    let u = a; if (!initial(u)) for (let j = 0; j < n; j++) if (R[j][a] && initial(j)) { u = j; break; }
    const vecOf = id => { for (const [k, v] of L.keys) if (v === id) return k.split(',').map(Number); };
    const from = vecOf(c), to = vecOf(u);
    let rec = recipes.find(x => !x.active && clKey(x.from) === clKey(from) && clKey(x.to) === clKey(to));
    if (rec) rec.active = true;
    else {
      const kn = CL_KNOWN_RETURNS.find(x => clKey(clVec(x.from)) === clKey(from) && clKey(clVec(x.to)) === clKey(to));
      rec = { id: 'fix' + recipes.length, g: kn ? kn.g : r.g, from, to, k: kn ? kn.k : CL_FIX_RATE, name: kn ? kn.name : 'return route', active: true };
      recipes.push(rec);
    }
    rec.added = true; added.push(rec);
  }
  return added;
}

// ---------------------------------------------------------------- compiled network
function clNet(recipes, rateScale) {
  const act = recipes.filter(r => r.active);
  let D = 5; act.forEach(r => { for (let i = 5; i < 7; i++) if (r.from[i] || r.to[i]) D = 7; });
  const m = act.length, Y = new Float64Array(m * D), DV = new Float64Array(m * D), K = new Float64Array(m), ORD = new Int32Array(m);
  act.forEach((r, j) => { K[j] = r.k * (rateScale || 1); for (let i = 0; i < D; i++) { Y[j * D + i] = r.from[i]; DV[j * D + i] = r.to[i] - r.from[i]; ORD[j] += r.from[i]; } });
  return { D, m, Y, DV, K, ORD, recipes: act };
}

// ---------------------------------------------------------------- deterministic RNG
function clRng(seed) { let a = seed >>> 0; return () => { a = (a + 0x6D2B79F5) >>> 0; let t = a; t = Math.imul(t ^ t >>> 15, t | 1); t ^= t + Math.imul(t ^ t >>> 7, t | 61); return ((t ^ t >>> 14) >>> 0) / 4294967296; }; }

// ---------------------------------------------------------------- towns
// Every town gets its own rates (base rate x a factor in [1/J, J]) and its own start (log-uniform in
// [S0, S1] per resource). PRIOR and NEW use the same towns.
const CL = {
  FLOOR: 0.01, FAUCET: 0.05, CAP: 20,      // prior-best clamps: floor 1 unit, relief caravan to 5 units, storage cap 2,000 units (1.0 = 100 units)
  DEAD: 1e-6, RUNAWAY: 1e6,                // "died out" / "ran away" (as demos/L4-economy-ladder/econ.h)
  STOP_HI: 1e12, STOP_LO: 1e-30,           // stop integrating a town past these (as econ.js)
  HMAX: 0.05, HMIN: 1e-9, GUARD: 20000,    // econ.js step rule h = min(0.05, 0.05/fastest log-rate); at most GUARD steps per town per frame (as econ.js)
  S0: 0.3, S1: 3, JITTER: 1.5, STARVE: 0.01,
};
function clTowns(net, N, seed, opt) {
  opt = opt || {};
  const D = net.D, m = net.m, J = opt.jitter || CL.JITTER, rng = clRng(seed);
  const T = { N, D, m, y: new Float64Array(N * D), y0: new Float64Array(N * D), k: new Float64Array(N * m), t: 0,
    status: new Uint8Array(N), stopped: new Uint8Array(N), clampedNow: new Uint8Array(N), clampFrames: new Float64Array(N),
    deadSp: new Int8Array(N).fill(-1), frames: 0, steps: 0, integer: opt.integer || 0, cnt: null, rng };
  for (let n = 0; n < N; n++) {
    for (let j = 0; j < m; j++) T.k[n * m + j] = net.K[j] * Math.exp((rng() * 2 - 1) * Math.log(J));
    for (let i = 0; i < D; i++) {
      let v = Math.exp(Math.log(opt.s0 || CL.S0) + rng() * Math.log((opt.s1 || CL.S1) / (opt.s0 || CL.S0)));
      if (i === 5) v *= 0.05; // a few sick at the start
      T.y[n * D + i] = T.y0[n * D + i] = Math.log(v);
    }
  }
  if (T.integer) { // counts: 1.0 = V units; every resource starts with at least 1
    T.cnt = new Float64Array(N * D);
    for (let q = 0; q < N * D; q++) T.cnt[q] = Math.max(1, Math.round(Math.exp(T.y[q]) * T.integer));
  }
  return T;
}

// log-space derivative: d(log x_i)/dt = sum_r k_r x^{y_r} (y'_r - y_r)_i / x_i
function clDeriv(net, y, yo, k, ko, out) {
  const D = net.D, m = net.m, Y = net.Y, DV = net.DV;
  for (let i = 0; i < D; i++) out[i] = 0;
  for (let r = 0; r < m; r++) {
    let e = 0; for (let i = 0; i < D; i++) e += Y[r * D + i] * y[yo + i];
    const rate = k[ko + r] * Math.exp(e);
    for (let i = 0; i < D; i++) out[i] += rate * DV[r * D + i];
  }
  for (let i = 0; i < D; i++) out[i] *= Math.exp(-y[yo + i]);
}

const _w = { k1: new Float64Array(7), k2: new Float64Array(7), k3: new Float64Array(7), k4: new Float64Array(7), tmp: new Float64Array(7), bak: new Float64Array(7) };
// Advance town n from T.t to T.t+dt. clamp=true is PRIOR BEST (clamp to [FLOOR, CAP], faucet refills an empty resource).
function clAdvance(net, T, n, dt, clamp) {
  if (T.stopped[n]) return 0;
  const D = net.D, y = T.y, yo = n * D, ko = n * net.m, { k1, k2, k3, k4, tmp } = _w;
  const LCAP = Math.log(CL.CAP), LFLOOR = Math.log(CL.FLOOR), LFAU = Math.log(CL.FAUCET), LHI = Math.log(CL.STOP_HI), LLO = Math.log(CL.STOP_LO);
  let t = 0, steps = 0, hit = 0;
  const bak = _w.bak;
  while (t < dt - 1e-12 && steps < CL.GUARD) {
    for (let i = 0; i < D; i++) bak[i] = y[yo + i];
    clDeriv(net, y, yo, T.k, ko, k1);
    let mx = 0; for (let i = 0; i < D; i++) { const a = Math.abs(k1[i]); if (a > mx) mx = a; }
    const h = Math.min(dt - t, CL.HMAX, Math.max(CL.HMIN, CL.HMAX / Math.max(mx, 1e-9)));
    for (let i = 0; i < D; i++) tmp[i] = y[yo + i] + 0.5 * h * k1[i]; clDeriv(net, tmp, 0, T.k, ko, k2);
    for (let i = 0; i < D; i++) tmp[i] = y[yo + i] + 0.5 * h * k2[i]; clDeriv(net, tmp, 0, T.k, ko, k3);
    for (let i = 0; i < D; i++) tmp[i] = y[yo + i] + h * k3[i]; clDeriv(net, tmp, 0, T.k, ko, k4);
    for (let i = 0; i < D; i++) y[yo + i] += h / 6 * (k1[i] + 2 * k2[i] + 2 * k3[i] + k4[i]);
    t += h; steps++;
    if (clamp) {
      for (let i = 0; i < D; i++) {
        const v = y[yo + i];
        if (v > LCAP) { y[yo + i] = LCAP; hit = 1; } else if (v < LFLOOR || v !== v) { y[yo + i] = LFAU; hit = 1; }
      }
    } else {
      let stop = false;
      for (let i = 0; i < D; i++) { const v = y[yo + i]; if (!(v < LHI && v > LLO)) stop = true; }
      if (stop) { // keep the last finite state for display; the town is out of range and no longer simulated
        clStatus(T, n);                                     // direction of the step that left the range
        const st0 = T.status[n];
        for (let i = 0; i < D; i++) y[yo + i] = bak[i];     // show the last in-range state
        T.status[n] = 0; clStatus(T, n);
        if (!T.status[n]) { T.status[n] = st0; T.stopOnly = (T.stopOnly || 0) + 1; } else T.status[n] |= st0;
        T.stopped[n] = 1;
        break; }
    }
  }
  T.clampedNow[n] = hit; if (hit) T.clampFrames[n] += 1;
  if (!clamp) clStatus(T, n);
  return steps;
}
// status bits: 1 = a resource died out (< DEAD), 2 = a resource ran away (> RUNAWAY); both sticky.
function clStatus(T, n) {
  const D = T.D, yo = n * D, LD = Math.log(CL.DEAD), LR = Math.log(CL.RUNAWAY);
  for (let i = 0; i < D; i++) {
    const v = T.y[yo + i];
    if (v < LD) { if (!(T.status[n] & 1)) T.deadSp[n] = i; T.status[n] |= 1; }
    else if (v > LR) T.status[n] |= 2;
  }
}

// Integer stockpiles: exact stochastic simulation (Gillespie). 1.0 = V units. Propensity k V^(1-order) prod n(n-1)..
// clamp=true: faucet adds 1 unit to any empty resource, cap at CAP*V. Without clamps a town whose Workers hit 0 is dead.
function clAdvanceInt(net, T, n, dt, clamp) {
  if (T.stopped[n]) return 0;
  const D = net.D, m = net.m, V = T.integer, c = T.cnt, o = n * D, ko = n * m, a = _ap.length >= m ? _ap : (_ap = new Float64Array(m));
  let t = 0, ev = 0, hit = 0; const cap = CL.CAP * V;
  for (; ;) {
    let a0 = 0;
    for (let r = 0; r < m; r++) {
      let p = T.k[ko + r] * Math.pow(V, 1 - net.ORD[r]);
      for (let i = 0; i < D; i++) { const need = net.Y[r * D + i]; for (let q = 0; q < need; q++) p *= c[o + i] - q; }
      a[r] = p > 0 ? p : 0; a0 += a[r];
    }
    if (a0 <= 0) { break; }
    t += -Math.log(1 - T.rng()) / a0;
    if (t > dt) break;
    let u = T.rng() * a0, r = 0; while (r < m - 1 && u >= a[r]) { u -= a[r]; r++; }
    for (let i = 0; i < D; i++) c[o + i] += net.DV[r * D + i];
    ev++;
    if (clamp) for (let i = 0; i < D; i++) { if (c[o + i] <= 0) { c[o + i] = 1; hit = 1; } else if (c[o + i] > cap) { c[o + i] = cap; hit = 1; } }
    if (ev > 20000) break;
  }
  for (let i = 0; i < D; i++) T.y[o + i] = Math.log(Math.max(c[o + i], 1e-9) / V);
  T.clampedNow[n] = hit; if (hit) T.clampFrames[n] += 1;
  if (!clamp && c[o + 4] <= 0 && !T.status[n]) { T.status[n] = 1; T.deadSp[n] = 4; } // cannot happen here: every recipe that removes a worker needs 2
  return ev;
}
let _ap = new Float64Array(16);

function clFrame(net, T, dt, clamp) {
  let s = 0;
  if (T.integer) for (let n = 0; n < T.N; n++) s += clAdvanceInt(net, T, n, dt, clamp);
  else for (let n = 0; n < T.N; n++) s += clAdvance(net, T, n, dt, clamp);
  T.t += dt; T.frames++; T.steps += s;
  return s;
}

// Summary for the HUD / bench. pinnedNow: clamp fired this frame. timePinned: fraction of town-frames with a clamp firing.
function clSummary(T) {
  const N = T.N, D = T.D; let dead = 0, run = 0, bad = 0, pinned = 0, cf = 0, starve = 0; const deadBy = new Array(D).fill(0);
  for (let n = 0; n < N; n++) {
    if (T.status[n] & 1) { dead++; deadBy[T.deadSp[n]]++; } if (T.status[n] & 2) run++; if (T.status[n]) bad++;
    if (T.clampedNow[n]) pinned++;
    cf += T.clampFrames[n];
    let lo = Infinity; for (let i = 0; i < Math.min(D, 5); i++) lo = Math.min(lo, T.y[n * D + i]); // goods only, not Sick/Immune
    if (lo < Math.log(CL.STARVE * 1.0001) || (T.integer && lo < -20)) starve++;
  }
  return { N, dead: dead / N, runaway: run / N, deadOrRun: bad / N, healthy: (N - bad) / N, pinnedNow: pinned / N,
    timePinned: T.frames ? cf / (N * T.frames) : 0, starving: starve / N, deadBy };
}

// ---------------------------------------------------------------- prior best: Monte-Carlo balance sweep
// K random towns (rates and starts as clTowns), each run with the shipped clamped simulation for H years in
// frames of dt; a seed is flagged as soon as any clamp or faucet fires. Returns flagged count and cost.
function clSweep(recipes, opt) {
  const net = clNet(recipes, opt.rateScale), K = opt.K, H = opt.H, dt = opt.dt || 0.5;
  const T = clTowns(net, K, opt.seed || 7, { jitter: opt.jitter, integer: opt.integer });
  let flagged = 0, years = 0, steps = 0; const firstT = [];
  for (let n = 0; n < K; n++) {
    let t = 0;
    for (; t < H - 1e-9; t += dt) {
      steps += T.integer ? clAdvanceInt(net, T, n, dt, true) : clAdvance(net, T, n, dt, true);
      if (T.clampedNow[n]) { flagged++; firstT.push(t + dt); t += dt; break; }
    }
    years += t;
  }
  return { flagged, K, H, years, steps, firstT };
}

if (typeof module !== 'undefined') module.exports = { CL_SP, CL_SH, CL_CATALOG, CL_SCENES, CL, clVec, clRecipe, clName, clSceneRecipes, clLint, clFix, clNet, clTowns, clAdvance, clAdvanceInt, clFrame, clSummary, clSweep, clRng };
