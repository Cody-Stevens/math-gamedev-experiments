// Wildfire Isle core: shared by index.html (inlined by build.py) and bench.js (Node).
// (a) fire front = critical (q = 1) square-lattice FK Dobrushin interface (Family 223);
//     tracer copied from panel/web/fk.js, a port of demos/04-fractal-frontier/fk.h.
// (b) island terrain = value-noise fBm (H = 0.75) as in panel/general/coast.js, plus an island falloff.

// ------------------------------------------------------------------ FK tracer (verbatim from panel/web/fk.js)
function fkBit(key, id) {
  let h = Math.imul(id ^ key, 0x9E3779B1) ^ Math.imul(key >>> 16 | 1, 0x85EBCA77);
  h ^= h >>> 16; h = Math.imul(h, 0x85EBCA6B); h ^= h >>> 13; h = Math.imul(h, 0xC2B2AE35); h ^= h >>> 16;
  return (h >>> 31) === 1;
}
function fkTrace(n, seed, wantPath) {
  const key = Math.imul(seed + 1, 0x632BE5AB) ^ 0x5bd1e995;
  const W = n + 4;
  const openH = (i, j) => (i >= 0 && i + 1 <= n && j >= 0 && j <= n) ? (j === n ? true : fkBit(key, ((j + 2) * W + (i + 2)) * 2)) : (i < 0 || j > n);
  const openV = (i, j) => (i >= 0 && i <= n && j >= 0 && j + 1 <= n) ? (i === 0 ? true : fkBit(key, ((j + 2) * W + (i + 2)) * 2 + 1)) : (i < 0 || j + 1 > n);
  const sideA = [0, 0, 1, 2], sideB = [3, 1, 2, 3], cdx = [0, 1, 1, 0], cdy = [0, 0, 1, 1];
  const inv = 1 / n, cl = v => v < 0 ? 0 : v > 1 ? 1 : v;
  let sx = 0, sy = -1, k = 3, sin = 3, N = 0;
  const path = wantPath ? [0, 0] : null, cap = 64 * (n + 2) * (n + 2);
  for (;;) {
    ++N;
    const sout = sideA[k] === sin ? sideB[k] : sideA[k];
    const ux = sx + cdx[k], uy = sy + cdy[k];
    let vx, vy, nsx = sx, nsy = sy, open, ex, ey;
    switch (sout) {
      case 0: open = openH(sx, sy); ex = sx + .5; ey = sy; vx = ux === sx ? sx + 1 : sx; vy = sy; nsy = sy - 1; break;
      case 2: open = openH(sx, sy + 1); ex = sx + .5; ey = sy + 1; vx = ux === sx ? sx + 1 : sx; vy = sy + 1; nsy = sy + 1; break;
      case 3: open = openV(sx, sy); ex = sx; ey = sy + .5; vx = sx; vy = uy === sy ? sy + 1 : sy; nsx = sx - 1; break;
      default: open = openV(sx + 1, sy); ex = sx + 1; ey = sy + .5; vx = sx + 1; vy = uy === sy ? sy + 1 : sy; nsx = sx + 1; break;
    }
    if (path) path.push(cl(ex * inv), cl(ey * inv));
    let wx, wy;
    if (open) { wx = vx; wy = vy; sin = sout; } else { sx = nsx; sy = nsy; wx = ux; wy = uy; sin = sout ^ 2; }
    const dx = wx - sx, dy = wy - sy;
    k = dy === 0 ? (dx === 0 ? 0 : 1) : (dx === 1 ? 2 : 3);
    if (sx === n && sy === n) { if (path) path.push(1, 1); return { N: k === 0 ? N : -1, path }; }
    if ((sx === -1 && sy === -1) || N > cap) return { N: -1, path };
  }
}

// ------------------------------------------------------------------ game constants
const QUALITY = [ // graphics quality -> gameplay simulation grid (cells across the map / across the fire region)
  { name: 'Low', n: 32 }, { name: 'Medium', n: 64 }, { name: 'High', n: 256 }, { name: 'Ultra', n: 1024 }];
const TUNED = 1;            // PRIOR and NEW are both play-tested and tuned at Medium
const ULTRA = 3;            // REFERENCE always simulates gameplay at Ultra
const FIRE_TARGET_H = 18;   // design target: median hours for the fire line to reach Ashford
const SPAWN_TARGET = 40;    // design target: coastal spawns (fishing spots + beach finds) on the island
const FIRE_P = { prior: 1, new: 1.75 };  // step time = C * (cell size)^p ; 1 = "fire speed in metres per hour"
// Coastline exponent d, measured OFFLINE by bench.js (least-squares slope of log coast-edge count vs log n
// over n = 64, 256, 1024) for each terrain mode. Not proved by any paper; re-measure if the terrain changes.
const D_COAST = { grow: 1.17, fixed: 1.05 };
const FIRE_SQ = { x0: 0.29, y0: 0.27, s: 0.40 }; // fire region (Greenwood) in map units, y down; Ashford at its NE corner
const SAMPLES = { 32: 4000, 64: 2000, 256: 400, 1024: 160 }; // fronts sampled per grid in the browser

// ------------------------------------------------------------------ terrain (value-noise fBm, from panel/general/coast.js)
const HURST = 0.75;
function hash(i, j, o) { let h = Math.imul(i, 374761393) ^ Math.imul(j, 668265263) ^ Math.imul(o, 0x27d4eb2d); h = Math.imul(h ^ (h >>> 13), 1274126177); return ((h ^ (h >>> 16)) >>> 0) / 4294967296; }
function vnoise(x, y, o) {
  const i = Math.floor(x), j = Math.floor(y), fx = x - i, fy = y - j, sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
  const a = hash(i, j, o), b = hash(i + 1, j, o), c = hash(i, j + 1, o), d = hash(i + 1, j + 1, o);
  return a + (b - a) * sx + (c - a) * sy + (a - b - c + d) * sx * sy - 0.5;
}
const AMP = []; for (let o = 0, a = 1; o < 16; o++, a *= Math.pow(2, -HURST)) AMP.push(a);
function fbm(x, y, octaves) { let s = 0, f = 4; for (let o = 0; o < octaves; o++) { s += AMP[o] * vnoise(x * f + 17.3, y * f + 5.1, o + 11); f *= 2; } return s; }
const smooth01 = t => t <= 0 ? 0 : t >= 1 ? 1 : t * t * (3 - 2 * t);
function heightAt(x, y, octaves) { // > 0 is land
  const dx = x - 0.5, dy = y - 0.5, r = Math.sqrt(dx * dx + dy * dy);
  const base = 0.30 - 4.0 * Math.max(0, r - 0.29) - 7 * Math.max(0, r - 0.40);
  const h = base + 1.2 * fbm(x, y, octaves);
  return h + greenwoodMask(x, y) * Math.max(0, 0.15 - h); // no lakes in Greenwood or Ashford: raise only low ground there
}
function greenwoodMask(x, y) { // 1 on the fire region (plus Ashford's corner), fading to 0 over a 0.05 margin
  const m = 0.05, q = FIRE_SQ;
  return smooth01((x - q.x0 + m) / m) * smooth01((q.x0 + q.s + 0.06 + m - x) / m) * smooth01((y - q.y0 + 0.06 + m) / m) * smooth01((q.y0 + q.s + m - y) / m);
}
const octavesFor = (n, detail) => detail === 'fixed' ? 6 : Math.round(Math.log2(n)) - 2; // detail grows with quality

// ------------------------------------------------------------------ coastline + spawns
function coastEdges(n, octaves) { // land/sea on an n x n grid of cell centres; every land-sea neighbour pair is a coast edge
  const land = new Uint8Array(n * n);
  for (let j = 0; j < n; j++) for (let i = 0; i < n; i++) land[j * n + i] = heightAt((i + .5) / n, (j + .5) / n, octaves) > 0 ? 1 : 0;
  const L = [], S = [];
  for (let j = 0; j < n; j++) for (let i = 0; i < n; i++) {
    const a = j * n + i, v = land[a];
    if (i + 1 < n && land[a + 1] !== v) { L.push(v ? a : a + 1); S.push(v ? a + 1 : a); }
    if (j + 1 < n && land[a + n] !== v) { L.push(v ? a : a + n); S.push(v ? a + n : a); }
  }
  const E = L.length, ang = new Float32Array(E), order = new Uint32Array(E);
  for (let e = 0; e < E; e++) { const c = L[e], x = (c % n + .5) / n - .5, y = ((c / n | 0) + .5) / n - .5; ang[e] = Math.atan2(y, x); order[e] = e; }
  order.sort((a, b) => ang[a] - ang[b]); // walk the coast roughly in order, so systematic sampling spreads spawns evenly
  return { n, E, land: Int32Array.from(L), sea: Int32Array.from(S), order };
}
// Systematic sampling along the coast: every edge carries the same weight w; a spawn is placed each time the running sum crosses
// an integer. Count = floor(E * w + 0.5). Odd spawns are fishing spots (sea side), even ones beach finds (land side).
function spawnCoast(c, w) {
  const out = []; let acc = 0.5;
  for (let k = 0; k < c.E; k++) {
    acc += w;
    while (acc >= 1) {
      acc -= 1; const e = c.order[k], fish = out.length % 2 === 0, cell = fish ? c.sea[e] : c.land[e];
      out.push({ x: (cell % c.n + .5) / c.n, y: ((cell / c.n | 0) + .5) / c.n, fish });
    }
  }
  return out;
}
const spawnCount = (E, w) => Math.floor(E * w + 0.5);

// ------------------------------------------------------------------ fire statistics
function median(a) { const s = Float64Array.from(a).sort(); const m = s.length >> 1; return s.length % 2 ? s[m] : (s[m - 1] + s[m]) / 2; }
function bootMedianCI(a, reps = 300, seed = 7) { // 95% bootstrap CI of the median, relative to the median
  let st = seed >>> 0; const rnd = () => ((st = Math.imul(st ^ (st >>> 15), 0x2c1b3c6d) + 0x9E3779B9 >>> 0) / 4294967296);
  const m0 = median(a), ms = [], b = new Float64Array(a.length);
  for (let r = 0; r < reps; r++) { for (let k = 0; k < a.length; k++) b[k] = a[(rnd() * a.length) | 0]; ms.push(median(b)); }
  ms.sort((x, y) => x - y); return [ms[Math.floor(reps * .025)] / m0 - 1, ms[Math.floor(reps * .975)] / m0 - 1];
}
const frontSeed = (n, s) => n * 100003 + s;
// Constants: PRIOR and NEW are tuned at Medium (median front there = FIRE_TARGET_H); REFERENCE is tuned at Ultra.
function fireConstants(medN) { // medN[level] = median lattice steps per front at QUALITY[level].n
  const nT = QUALITY[TUNED].n, nU = QUALITY[ULTRA].n;
  return {
    prior: FIRE_TARGET_H / (medN[TUNED] * Math.pow(1 / nT, FIRE_P.prior)),
    new: FIRE_TARGET_H / (medN[TUNED] * Math.pow(1 / nT, FIRE_P.new)),
    ref: FIRE_TARGET_H / (medN[ULTRA] * Math.pow(1 / nU, FIRE_P.new)), // any exponent: REFERENCE only ever runs at Ultra
  };
}
const stepHours = (C, mode, n) => C[mode] * Math.pow(1 / n, mode === 'prior' ? FIRE_P.prior : FIRE_P.new);
// Spawn weights per coast edge; E[level] = coast edges at QUALITY[level].n for the shipped terrain mode.
function spawnConstants(E, d) {
  const nT = QUALITY[TUNED].n;
  return { prior: SPAWN_TARGET / (E[TUNED] * (1 / nT)), new: SPAWN_TARGET / (E[TUNED] * Math.pow(1 / nT, d)), ref: SPAWN_TARGET / E[ULTRA] };
}
const spawnWeight = (K, mode, n, d) => mode === 'ref' ? K.ref : mode === 'prior' ? K.prior / n : K.new * Math.pow(1 / n, d);

if (typeof module !== 'undefined') module.exports = { fkTrace, greenwoodMask, QUALITY, TUNED, ULTRA, FIRE_TARGET_H, SPAWN_TARGET, FIRE_P, D_COAST, FIRE_SQ, SAMPLES,
  heightAt, octavesFor, coastEdges, spawnCoast, spawnCount, median, bootMedianCI, frontSeed, fireConstants, stepHours, spawnConstants, spawnWeight };
