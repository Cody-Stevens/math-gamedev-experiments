// Dungeon Daily core: templates, three layout generators, reference chain, exact TV toy.
// Shared by index.html (inlined by build.py) and bench.js (Node). No DOM, no globals besides DD.
// Graph = { n, W, adj: Uint32Array(n*W) } adjacency bitsets, W 32-bit words per room.
(function (root) {
'use strict';
const popc = x => { x -= (x >>> 1) & 0x55555555; x = (x & 0x33333333) + ((x >>> 2) & 0x33333333); return Math.imul((x + (x >>> 4)) & 0x0F0F0F0F, 0x01010101) >>> 24; };
const ctz = x => 31 - Math.clz32(x & -x);

// sfc32 PRNG (int32 arithmetic); R(n) -> integer in [0, n) by multiply-shift (bias < n / 2^32)
function makeRng(seed) {
  let a, b, c, d;
  const R = m => { const t = (a + b + d) | 0; d = (d + 1) | 0; a = b ^ (b >>> 9); b = (c + (c << 3)) | 0; c = (c << 21) | (c >>> 11); c = (c + t) | 0; return ((t >>> 0) * m / 4294967296) | 0; };
  R.reseed = sd => { a = 0x9E3779B9 ^ sd; b = (0x243F6A88 + Math.imul(sd, 7)) | 0; c = 0xB7E15162 ^ Math.imul(sd, 13); d = 1; for (let k = 0; k < 15; k++) R(2); };
  R.reseed(seed);
  return R;
}

// ------------------------------------------------------------------ graphs
function newGraph(n) { const W = (n + 31) >>> 5; return { n, W, adj: new Uint32Array(n * W) }; }
function cloneGraph(g) { return { n: g.n, W: g.W, adj: Uint32Array.from(g.adj) }; }
function has(g, u, v) { return (g.adj[u * g.W + (v >>> 5)] >>> (v & 31)) & 1; }
function tog(g, u, v) { g.adj[u * g.W + (v >>> 5)] ^= 1 << (v & 31); g.adj[v * g.W + (u >>> 5)] ^= 1 << (u & 31); }
function deg(g, u) { let s = 0; for (let w = 0; w < g.W; w++) s += popc(g.adj[u * g.W + w]); return s; }
function degrees(g) { const d = []; for (let u = 0; u < g.n; u++) d.push(deg(g, u)); return d; }
function edgeList(g) { const e = []; for (let u = 0; u < g.n; u++) for (let v = u + 1; v < g.n; v++) if (has(g, u, v)) e.push([u, v]); return e; }
function overlap(a, b) { let s = 0; for (let k = 0; k < a.adj.length; k++) s += popc(a.adj[k] & b.adj[k]); return s / 2; }
function components(g) {
  const comp = new Int32Array(g.n).fill(-1), st = []; let k = 0;
  for (let s = 0; s < g.n; s++) {
    if (comp[s] >= 0) continue; comp[s] = k; st.push(s);
    while (st.length) {
      const u = st.pop();
      for (let w = 0; w < g.W; w++) for (let x = g.adj[u * g.W + w]; x; x &= x - 1) {
        const v = (w << 5) | ctz(x); if (comp[v] < 0) { comp[v] = k; st.push(v); }
      }
    }
    k++;
  }
  return { count: k, comp };
}
function isConnected(g) { return components(g).count === 1; }

// ------------------------------------------------------------------ templates
// Each template: { name, n, cols, rows, pos[[c,r]], g, roles: {entrance, boss, hubs:[...], treasure:[...]} }
function gridBase(rows, cols) {
  const g = newGraph(rows * cols), id = (r, c) => r * cols + c;
  for (let r = 0; r < rows; r++) for (let c = 0; c + 1 < cols; c++) tog(g, id(r, c), id(r, c + 1));
  for (let r = 0; r + 1 < rows; r++) for (let c = 0; c < cols; c++) tog(g, id(r, c), id(r + 1, c));
  return { g, id };
}
function finishTemplate(name, rows, cols, g, roles) {
  const pos = []; for (let r = 0; r < rows; r++) for (let c = 0; c < cols; c++) pos.push([c, r]);
  const d = degrees(g), n = g.n;
  if (roles.boss === undefined) roles.boss = d.lastIndexOf(Math.min(...d));
  if (roles.entrance === roles.boss) roles.entrance = (roles.boss + 1) % n;
  // treasure rooms: the lowest-door rooms farthest (on the grid) from the entrance
  const busy = new Set([roles.boss, roles.entrance, ...roles.hubs]), [ec, er] = pos[roles.entrance];
  const cand = [];
  for (let u = 0; u < n; u++) if (!busy.has(u)) cand.push([d[u] * 100 - Math.abs(pos[u][0] - ec) - Math.abs(pos[u][1] - er), u]);
  cand.sort((a, b) => a[0] - b[0]);
  roles.treasure = cand.slice(0, Math.max(2, Math.round(n / 12))).map(x => x[1]);
  return { name, n, rows, cols, pos, g, roles, deg: d, m: edgeList(g).length };
}
// "normal": the original 20-room template of demos/06-dungeon-shuffle (28 doors, door counts 1..6)
function templateNormal() {
  const { g, id } = gridBase(4, 5);
  for (const e of [[0,1,0,2],[2,3,2,4],[1,0,2,0],[2,2,3,2],[1,3,2,3],[3,0,3,1]]) tog(g, id(e[0], e[1]), id(e[2], e[3]));
  tog(g, id(1, 2), id(3, 4)); tog(g, id(1, 2), id(0, 0)); tog(g, id(2, 1), id(0, 4));
  return finishTemplate('normal', 4, 5, g, { entrance: id(3, 4), hubs: [id(1, 2)] });
}
// "hubs": a 6x5 town (30 rooms). A town square with `square` doors, plus `halls` guild halls with
// `hallDoors` doors each, wired by long streets to the nearest rooms they are not yet joined to.
function templateHubs(square = 16, halls = 2, hallDoors = 10) {
  const rows = 5, cols = 6, { g, id } = gridBase(rows, cols);
  // thin the grid a little (dead ends and a cut wing), like the normal template
  for (const e of [[0,1,0,2],[1,0,2,0],[3,4,3,5],[4,1,4,2],[2,4,3,4],[0,4,1,4],[1,5,2,5]]) if (has(g, id(e[0], e[1]), id(e[2], e[3]))) tog(g, id(e[0], e[1]), id(e[2], e[3]));
  // one boss room with exactly one door: room (4,0) keeps only its corridor to (3,0)
  const boss = id(4, 0);
  for (let v = 0; v < g.n; v++) if (v !== id(3, 0) && has(g, boss, v)) tog(g, boss, v);
  if (!has(g, boss, id(3, 0))) tog(g, boss, id(3, 0));
  const sq = id(2, 2), hallIds = [id(1, 4), id(3, 1), id(0, 0), id(4, 5)].slice(0, halls);
  const wire = (h, k) => {
    const [hr, hc] = [Math.floor(h / cols), h % cols];
    const cand = [];
    for (let v = 0; v < g.n; v++) if (v !== h && v !== boss) cand.push([Math.abs(Math.floor(v / cols) - hr) + Math.abs(v % cols - hc) + v * 1e-3, v]);
    cand.sort((a, b) => a[0] - b[0]);
    for (const [, v] of cand) { if (deg(g, h) >= k) break; if (!has(g, h, v)) tog(g, h, v); }
  };
  wire(sq, square);
  for (const h of hallIds) wire(h, hallDoors);
  return finishTemplate('hubs', rows, cols, g, { entrance: id(0, 5), boss, hubs: [sq, ...hallIds] });
}
// "big": the 80-room template of demos/L5-sweeps/06-dungeon-sweep (same recipe AND same seed:
// splitmix64 + Lemire, ported with BigInt so the graph is bit-identical to the C++ one)
function splitmix(seed) {
  const M = (1n << 64n) - 1n; let s = (BigInt(seed) * 0x9E3779B97F4A7C15n + 0x1234567n) & M;
  const next = () => { s = (s + 0x9E3779B97F4A7C15n) & M; let z = s; z = ((z ^ (z >> 30n)) * 0xBF58476D1CE4E5B9n) & M; z = ((z ^ (z >> 27n)) * 0x94D049BB133111EBn) & M; return z ^ (z >> 31n); };
  const below = n => {
    const N = BigInt(n); let m = (next() & 0xFFFFFFFFn) * N, l = m & 0xFFFFFFFFn;
    if (l < N) { const t = ((1n << 32n) - N) % N; while (l < t) { m = (next() & 0xFFFFFFFFn) * N; l = m & 0xFFFFFFFFn; } }
    return Number(m >> 32n);
  };
  return { below };
}
function templateBig(n = 80) {
  const dims = { 40: [5, 8], 80: [8, 10] }[n]; const [rows, cols] = dims;
  const { g } = gridBase(rows, cols), r = splitmix(1310 + n);
  const grid = edgeList(g), nrm = Math.round(grid.length * 6 / 31);
  for (let k = 0, done = 0; done < nrm && k < 100000; k++) {
    const e = grid[r.below(grid.length)];
    if (!has(g, e[0], e[1]) || deg(g, e[0]) <= 1 || deg(g, e[1]) <= 1) continue;
    tog(g, e[0], e[1]); done++;
  }
  const nsec = Math.round(3 * n / 20), nhub = Math.max(1, Math.floor(n / 10)), hubs = [];
  while (hubs.length < nhub) { const h = r.below(g.n); if (!hubs.includes(h)) hubs.push(h); }
  for (let k = 0, done = 0; done < nsec && k < 100000; k++) {
    const h = hubs[done % nhub], v = r.below(g.n);
    const dr = Math.abs(Math.floor(h / cols) - Math.floor(v / cols)), dc = Math.abs(h % cols - v % cols);
    if (v === h || dr + dc <= 1 || has(g, h, v)) continue;
    tog(g, h, v); done++;
  }
  const d = degrees(g), maxd = Math.max(...d);
  return finishTemplate('big', rows, cols, g, { entrance: cols - 1, hubs: [d.indexOf(maxd)] });
}

// Presets (?scene=...): normal = original 20-room template; square = 30-room town with ONE 16-door town square;
// hubs = the same town plus two 10-door guild halls; big = the 80-room sweep template.
const SCENES = {
  normal: { title: 'The Crypt: an ordinary dungeon', make: () => templateNormal() },
  square: { title: 'Market Town: one big town square', make: (sq = 16) => templateHubs(sq, 0, 10) },
  hubs: { title: 'Guild City: square + 2 guild halls', make: (sq = 16) => templateHubs(sq, 2, 10) },
  big: { title: 'The Deep Halls: 80 rooms', make: () => templateBig(80) },
};

// ------------------------------------------------------------------ generators
// Certified budget (Family 131, sec. 7, mix:H-poincare): gap >= 1/B  =>  TV <= 1/4 after
// T = ceil(B (1 + B/2) ln 2) Curveball trades, B = n(n-1)/2, from any start, any graphical sequence.
function certifiedT(n) { const B = n * (n - 1) / 2; return Math.ceil(B * (1 + B / 2) * Math.LN2); }

// NEW: Curveball pair trade. Uniform pair {i,j}; the q singleton neighbours of i (those not shared
// with j) are re-dealt as a uniform q-subset of all N singletons (partial Fisher-Yates).
// makeCurveball(n, R) returns run(adj, steps) that applies `steps` trades in place.
function makeCurveball(n, R) {
  const W = (n + 31) >>> 5, S = new Uint32Array(W), Ai = new Uint32Array(W), flip = new Uint32Array(W), list = new Int32Array(n);
  if (W === 1) return (adj, steps) => {           // fast path, n <= 32
    for (let s = 0; s < steps; s++) {
      const i = R(n); let j = R(n - 1); if (j >= i) j++;
      const bi = 1 << i, bj = 1 << j, A = adj[i] & ~bj, B = adj[j] & ~bi, x0 = A ^ B;
      if (!x0) continue;
      const a = A & x0, q = popc(a), N = popc(x0);
      if (q === 0 || q === N) continue;
      let c = 0; for (let x = x0; x; x &= x - 1) list[c++] = ctz(x);
      let nA = 0;
      for (let k = 0; k < q; k++) { const t = k + R(N - k), tmp = list[k]; list[k] = list[t]; list[t] = tmp; nA |= 1 << list[k]; }
      const f = nA ^ a; if (!f) continue;
      adj[i] ^= f; adj[j] ^= f;
      for (let x = f; x; x &= x - 1) adj[ctz(x)] ^= bi | bj;
    }
  };
  return (adj, steps) => {
    for (let s = 0; s < steps; s++) {
      const i = R(n); let j = R(n - 1); if (j >= i) j++;
      const oi = i * W, oj = j * W, wi = i >>> 5, wj = j >>> 5;
      let q = 0, N = 0;
      for (let w = 0; w < W; w++) {
        let A = adj[oi + w], B = adj[oj + w];
        if (w === wj) A &= ~(1 << (j & 31));
        if (w === wi) B &= ~(1 << (i & 31));
        const x = A ^ B; S[w] = x; Ai[w] = A & x; q += popc(A & x); N += popc(x);
      }
      if (q === 0 || q === N) continue;
      let c = 0;
      for (let w = 0; w < W; w++) { flip[w] = 0; for (let x = S[w]; x; x &= x - 1) list[c++] = (w << 5) | ctz(x); }
      for (let k = 0; k < q; k++) { const t = k + R(N - k), tmp = list[k]; list[k] = list[t]; list[t] = tmp; const z = list[k]; flip[z >>> 5] |= 1 << (z & 31); }
      const mi = 1 << (i & 31), mj = 1 << (j & 31);
      for (let w = 0; w < W; w++) {
        const f = flip[w] ^ Ai[w]; if (!f) continue;
        adj[oi + w] ^= f; adj[oj + w] ^= f;
        for (let x = f; x; x &= x - 1) { const o = ((w << 5) | ctz(x)) * W; adj[o + wi] ^= mi; adj[o + wj] ^= mj; }
      }
    }
  };
}

// PRIOR (common): lazy double-edge swap on an edge list; reject if it would make a loop or double door.
function swapStep(g, E, R) {
  const m = E.length >>> 1, a = R(m); let b = R(m - 1); if (b >= a) b++;
  const u = E[2 * a], v = E[2 * a + 1]; let x = E[2 * b], y = E[2 * b + 1];
  if (R(2)) { const t = x; x = y; y = t; }
  if (u === x || u === y || v === x || v === y) return 0;
  if (has(g, u, x) || has(g, v, y)) return 0;
  tog(g, u, v); tog(g, x, y); tog(g, u, x); tog(g, v, y);
  E[2 * a] = u; E[2 * a + 1] = x; E[2 * b] = v; E[2 * b + 1] = y;
  return 1;
}
function flatEdges(g) { const e = edgeList(g), E = new Int32Array(2 * e.length); e.forEach(([u, v], k) => { E[2 * k] = u; E[2 * k + 1] = v; }); return E; }

// PRIOR (exact): configuration model. Deal all door stubs into uniformly random pairs; abort the
// attempt at the first self-loop or double door (same as dealing everything then rejecting).
// Every simple layout arises from exactly prod(d_i!) pairings, so accepted layouts are exactly uniform.
function makeConfig(tpl) {
  const stubs = []; tpl.deg.forEach((d, u) => { for (let k = 0; k < d; k++) stubs.push(u); });
  const S = Int32Array.from(stubs), L = S.length, buf = new Int32Array(L);
  const g = newGraph(tpl.n);
  // returns true and leaves the layout in g on success
  const attempt = R => {
    g.adj.fill(0); buf.set(S);
    for (let left = L; left > 0; left -= 2) {
      // take the last stub, pair it with a uniform other remaining stub
      const u = buf[left - 1], k = R(left - 1), v = buf[k];
      buf[k] = buf[left - 2];
      if (u === v || has(g, u, v)) return false;
      tog(g, u, v);
    }
    return true;
  };
  return { g, attempt };
}

// ------------------------------------------------------------------ resumable generator jobs
// job.work(units, out) does `units` small pieces of work and pushes finished layouts {g, daily} to out.
// The caller times work() with performance.now(); jobs only count. job.pending = seed: the next layout
// that STARTS uses this seed (no work is ever abandoned) and is flagged daily (same seed -> same layout).
function makeJobs(tpl, seed) {
  const n = tpl.n, T = certifiedT(n), E0 = flatEdges(tpl.g), m = tpl.m, jobs = {};
  {
    const R = makeRng(seed * 3 + 1);
    jobs.swap = { key: 'swap', steps: m, unit: 1, layouts: 0, attempts: 0, rejects: 0, pending: null,
      work(units, out) {      // unit = one complete layout (m swaps)
        for (let u = 0; u < units; u++) {
          let daily = false;
          if (this.pending !== null) { R.reseed(this.pending); this.pending = null; daily = true; }
          const g = cloneGraph(tpl.g), E = Int32Array.from(E0);
          for (let k = 0; k < m; k++) swapStep(g, E, R);
          this.layouts++; this.attempts++; out.push({ g, daily });
        }
      }, progress() { return 0; } };
  }
  {
    const R = makeRng(seed * 3 + 2), C = makeConfig(tpl); let fresh = true, dailyNext = false, since = 0;
    jobs.config = { key: 'config', unit: 1, layouts: 0, attempts: 0, rejects: 0, pending: null,
      work(units, out) {      // unit = one attempt
        for (let u = 0; u < units; u++) {
          if (fresh) { if (this.pending !== null) { R.reseed(this.pending); this.pending = null; dailyNext = true; } fresh = false; since = 0; }
          this.attempts++; since++;
          if (C.attempt(R)) { this.layouts++; out.push({ g: cloneGraph(C.g), daily: dailyNext }); dailyNext = false; fresh = true; }
          else this.rejects++;
        }
      }, progress() { return since; } };
  }
  {
    const R = makeRng(seed * 3 + 3), cb = makeCurveball(n, R); let g = null, t = 0, daily = false;
    jobs.curveball = { key: 'curveball', steps: T, unit: 1000, layouts: 0, attempts: 0, rejects: 0, pending: null,
      work(units, out) {      // unit = up to 1000 trades
        for (let u = 0; u < units; u++) {
          if (!g) { daily = false; if (this.pending !== null) { R.reseed(this.pending); this.pending = null; daily = true; } g = cloneGraph(tpl.g); t = 0; }
          const stop = Math.min(T, t + 1000);
          cb(g.adj, stop - t); t = stop;
          if (t >= T) { this.layouts++; this.attempts++; out.push({ g, daily }); g = null; }
        }
      }, progress() { return g ? t / T : 0; } };
  }
  return { jobs, T };
}
// Havel-Hakimi realisation of a degree sequence (start state for the exact toys)
function havelHakimi(d) {
  const r = d.map((x, i) => [x, i]), E = [];
  for (;;) {
    r.sort((p, q) => q[0] - p[0] || p[1] - q[1]);
    if (!r.length || r[0][0] === 0) return E;
    const [k, v] = r.shift();
    for (let t = 0; t < k; t++) { r[t][0]--; E.push([v, r[t][1]]); }
  }
}

// REFERENCE: one long double-edge swap chain, thinned (default 100 swaps per door between samples).
function makeReference(tpl, seed, thin = 100) {
  const R = makeRng(seed * 7 + 5), g = cloneGraph(tpl.g), E = flatEdges(tpl.g), m = tpl.m;
  for (let k = 0; k < 1000 * m; k++) swapStep(g, E, R);
  return { g, sample() { for (let k = 0; k < thin * m; k++) swapStep(g, E, R); return g; } };
}

// ------------------------------------------------------------------ statistics
function makeStats(m) {
  return { m, n: 0, sumKept: 0, hist: new Float64Array(m + 1), nConn: 0, histConn: new Float64Array(m + 1), sumKeptConn: 0,
    add(kept, conn) { this.n++; this.sumKept += kept; this.hist[kept]++; if (conn) { this.nConn++; this.histConn[kept]++; this.sumKeptConn += kept; } },
    mean(connOnly) { return connOnly ? this.sumKeptConn / this.nConn : this.sumKept / this.n; },
    dist(connOnly) { const h = connOnly ? this.histConn : this.hist, c = connOnly ? this.nConn : this.n; return Array.from(h, x => x / Math.max(1, c)); },
    count(connOnly) { return connOnly ? this.nConn : this.n; } };
}
function histTV(p, q) { let t = 0; for (let k = 0; k < p.length; k++) t += Math.abs(p[k] - q[k]); return t / 2; }
// bias toward the template: (mean kept - fair mean) / (doors - fair mean); 0 = fair, 1 = template copy
function bias(mean, ref, m) { return (mean - ref) / (m - ref); }

// ------------------------------------------------------------------ exact TV on enumerable toys
// Port of demos/06-dungeon-shuffle Exact: enumerate every simple graph with degree sequence d,
// build both chains' full transition matrices and propagate the law from `start` exactly.
function exactToy(d, startEdges) {
  const n = d.length, B = n * (n - 1) / 2, pi = [], pj = [];
  for (let i = 0; i < n; i++) for (let j = i + 1; j < n; j++) { pi.push(i); pj.push(j); }
  const states = [], res = d.slice();
  (function en(k, mask) {
    if (k === B) { for (let v = 0; v < n; v++) if (res[v]) return; states.push(mask >>> 0); return; }
    const i = pi[k], j = pj[k];
    if (res[i] && res[j]) { res[i]--; res[j]--; en(k + 1, (mask | (1 << k)) >>> 0); res[i]++; res[j]++; }
    if (!(j === n - 1 && res[i])) en(k + 1, mask);
  })(0, 0);
  const id = new Map(); states.forEach((s, k) => id.set(s, k));
  const toAdj = mask => { const a = new Uint32Array(n); for (let k = 0; k < B; k++) if ((mask >>> k) & 1) { a[pi[k]] |= 1 << pj[k]; a[pj[k]] |= 1 << pi[k]; } return a; };
  const toMask = a => { let s = 0; for (let k = 0; k < B; k++) if ((a[pi[k]] >>> pj[k]) & 1) s |= 1 << k; return s >>> 0; };
  let s0 = 0; for (const [u, v] of startEdges) { const k = pi.findIndex((x, t) => x === Math.min(u, v) && pj[t] === Math.max(u, v)); s0 |= 1 << k; }
  const start = id.get(s0 >>> 0), m = d.reduce((a, b) => a + b, 0) / 2, S = states.length;
  const Kcb = [], Ksw = [];
  for (let s = 0; s < S; s++) {
    const g = toAdj(states[s]), rowC = new Map(), rowS = new Map(), add = (row, t, p) => row.set(t, (row.get(t) || 0) + p);
    for (let a = 0; a < B; a++) {
      const i = pi[a], j = pj[a], bi = 1 << i, bj = 1 << j, A = g[i] & ~bj, Bm = g[j] & ~bi, Sx = A ^ Bm, Ai = A & Sx;
      const q = popc(Ai), N = popc(Sx), list = []; for (let x = Sx; x; x &= x - 1) list.push(ctz(x));
      const subs = [];
      for (let sub = 0; sub < (1 << N); sub++) if (popc(sub) === q) { let na = 0; for (let b = 0; b < N; b++) if ((sub >> b) & 1) na |= 1 << list[b]; subs.push(na); }
      for (const na of subs) {
        const h = Uint32Array.from(g), f = na ^ Ai; h[i] ^= f; h[j] ^= f;
        for (let x = f; x; x &= x - 1) h[ctz(x)] ^= bi | bj;
        add(rowC, id.get(toMask(h)), 1 / B / subs.length);
      }
    }
    const E = []; for (let u = 0; u < n; u++) for (let v = u + 1; v < n; v++) if ((g[u] >>> v) & 1) E.push([u, v]);
    const pp = 1 / (2 * (m * (m - 1) / 2));
    for (let a = 0; a < m; a++) for (let b = a + 1; b < m; b++) for (let o = 0; o < 2; o++) {
      const [u, v] = E[a]; let [x, y] = E[b]; if (o) [x, y] = [y, x];
      if (u === x || u === y || v === x || v === y || ((g[u] >>> x) & 1) || ((g[v] >>> y) & 1)) { add(rowS, s, pp); continue; }
      const h = Uint32Array.from(g); const t2 = (p, q) => { h[p] ^= 1 << q; h[q] ^= 1 << p; };
      t2(u, v); t2(x, y); t2(u, x); t2(v, y); add(rowS, id.get(toMask(h)), pp);
    }
    Kcb.push([...rowC]); Ksw.push([...rowS]);
  }
  const tv = p => { let t = 0; const u = 1 / S; for (const x of p) t += Math.abs(x - u); return t / 2; };
  const curve = (K, steps, from = start) => {
    let p = new Float64Array(S), q = new Float64Array(S); p[from] = 1; const out = [tv(p)];
    for (let t = 0; t < steps; t++) { q.fill(0); for (let s = 0; s < S; s++) if (p[s]) for (const [c, w] of K[s]) q[c] += p[s] * w; [p, q] = [q, p]; out.push(tv(p)); }
    return out;
  };
  return { n, B, m, S, Kcb, Ksw, curve, start, T: certifiedT(n), bound: t => 0.5 * Math.sqrt(S - 1) * Math.pow(1 - 1 / B, t) };
}
// Configuration model exact law on a toy: enumerate EVERY stub pairing, count pairings per simple graph.
// Returns TV of the accepted-layout law to uniform and the acceptance probability (exact).
function exactConfigToy(d) {
  const stubs = []; d.forEach((x, u) => { for (let k = 0; k < x; k++) stubs.push(u); });
  const L = stubs.length, used = new Uint8Array(L), n = d.length, adj = new Uint32Array(n), counts = new Map();
  let total = 0, simple = 0;
  (function rec() {
    let a = 0; while (a < L && used[a]) a++;
    if (a === L) { total++; let key = ''; simple++; for (let u = 0; u < n; u++) key += adj[u] + ','; counts.set(key, (counts.get(key) || 0) + 1); return; }
    used[a] = 1;
    for (let b = a + 1; b < L; b++) if (!used[b]) {
      used[b] = 1; const u = stubs[a], v = stubs[b];
      if (u === v || ((adj[u] >>> v) & 1)) { total += countRest(L - countUsed()); }
      else { adj[u] |= 1 << v; adj[v] |= 1 << u; rec(); adj[u] &= ~(1 << v); adj[v] &= ~(1 << u); }
      used[b] = 0;
    }
    used[a] = 0;
  })();
  function countUsed() { let c = 0; for (let k = 0; k < L; k++) c += used[k]; return c; }
  function countRest(r) { let c = 1; for (let k = r - 1; k > 0; k -= 2) c *= k; return c; } // (r-1)!! pairings of r stubs
  const vals = [...counts.values()], S = vals.length;
  let tv = 0; for (const c of vals) tv += Math.abs(c / simple - 1 / S); tv /= 2;
  const perGraph = new Set(vals);
  return { pairings: total, simple, graphs: S, tv, perGraph: [...perGraph], accept: simple / total };
}

root.DD = { popc, ctz, makeRng, newGraph, cloneGraph, has, tog, deg, degrees, edgeList, overlap, components, isConnected,
  SCENES, templateNormal, templateHubs, templateBig, havelHakimi, certifiedT, makeCurveball, swapStep, flatEdges, makeConfig, makeJobs,
  makeReference, makeStats, histTV, bias, exactToy, exactConfigToy };
if (typeof module !== 'undefined') module.exports = root.DD;
})(typeof globalThis !== 'undefined' ? globalThis : this);
