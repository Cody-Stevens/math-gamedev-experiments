// core.js - game logic for "Tavern of a Thousand Tables" (shared by index.html and bench.js).
// Maker-Breaker Snaky on a 17x17 board. Cell c = x*17 + y. own[c]: 0 free, 1 Maker, 2 Breaker.
//   NEW   : CertPolicy, the 21-move certificate walker (verbatim copy of panel/web/snaky.js).
//   PRIOR : FastSearch, a port of demos/L1-snaky-ladder/fast_search.h (iterative-deepening alpha-beta,
//           Zobrist transposition table, incremental threat evaluation + move ordering, width 9),
//           run under a per-move node cap and a wall-clock deadline.
//   Breakers: random-local and greedy Erdos-Selfridge blocker (demos/03-snaky/snaky_game.h), and an
//           alpha-beta Breaker (FastSearch, 6000 nodes per move, untimed).
'use strict';

// ===================================================================== certificate walker (snaky.js)
const SN = 17, SNC = 289;
const SNAKE = [[0,0],[1,0],[2,0],[3,0],[3,1],[4,1]];
const onb = (x, y) => x >= 0 && y >= 0 && x < SN && y < SN;
function affApply(F, x, y) { return [F[0]*x + F[1]*y + F[4], F[2]*x + F[3]*y + F[5]]; }
function affInv(F, X, Y) { const u = X - F[4], v = Y - F[5]; return [F[0]*u + F[2]*v, F[1]*u + F[3]*v]; }
function affThen(F, g) { // F o g (apply g first); g = [a,b,c,d,tx,ty]
  return [F[0]*g[0] + F[1]*g[2], F[0]*g[1] + F[1]*g[3], F[2]*g[0] + F[3]*g[2], F[2]*g[1] + F[3]*g[3],
          F[0]*g[4] + F[1]*g[5] + F[4], F[2]*g[4] + F[3]*g[5] + F[5]];
}
function loadCert(J) {
  const nodes = J.nodes.map(([px, py, base, h, T, kids]) => {
    const t = new Uint8Array(SNC); for (const c of T) t[c] = 1;
    return { px, py, base: !!base, h, T: t, Tlist: T, kids: kids.map(k => ({ ref: !!k[0], idx: k[1], g: k.slice(2) })) };
  });
  return { nodes, card: J.card_node, t727: J.t727 };
}
class CertPolicy {
  constructor(C) { this.C = C; this.reset(); }
  reset() { this.node = this.C.card[727]; this.F = [1,0,0,1,0,0]; this.breaks = 0; }
  height() { return this.C.nodes[this.node].h; }
  decide(own) {
    const n = this.C.nodes[this.node];
    if (n.base) {
      for (const [sx, sy] of SNAKE) { const [x, y] = affApply(this.F, sx, sy); const c = x*SN + y; if (own[c] !== 1) return c; }
      return this.firstFree(own);
    }
    const [x, y] = affApply(this.F, n.px, n.py); const c = x*SN + y;
    return own[c] === 0 ? c : this.firstFree(own);
  }
  firstFree(own) { for (const c of this.C.t727) if (own[c] === 0) return c; for (let c = 0; c < SNC; c++) if (own[c] === 0) return c; return -1; }
  observe(b) { // Breaker replied at b: move to the first child whose placed envelope avoids b
    const n = this.C.nodes[this.node];
    if (n.base) return;
    const [lx, ly] = affInv(this.F, (b / SN) | 0, b % SN);
    for (const k of n.kids) {
      if (k.ref) {
        const src = this.C.nodes[this.C.card[k.idx]];
        const [cx, cy] = affInv(k.g, lx, ly);
        if (onb(cx, cy) && src.T[cx*SN + cy]) continue;
        this.F = affThen(this.F, k.g); this.node = this.C.card[k.idx]; return;
      } else {
        if (onb(lx, ly) && this.C.nodes[k.idx].T[lx*SN + ly]) continue;
        this.node = k.idx; return;
      }
    }
    this.breaks++;
  }
  placedT() { // cells of the live envelope on the board
    const n = this.C.nodes[this.node], out = [];
    for (const i of n.Tlist) { const [x, y] = affApply(this.F, (i / SN) | 0, i % SN); if (onb(x, y)) out.push(x*SN + y); }
    return out;
  }
  invariant(own) { // envelope must contain no Breaker cell (Definition def:claim)
    const n = this.C.nodes[this.node];
    for (const i of n.Tlist) { const [x, y] = affApply(this.F, (i / SN) | 0, i % SN); if (!onb(x, y) || own[x*SN + y] === 2) return false; }
    return true;
  }
}
function snakeLines() {
  const seen = new Set(), lines = [];
  for (let s = 0; s < 8; s++) {
    const pts = SNAKE.map(([x, y]) => { let a = (s & 2) ? -x : x, b = (s & 4) ? -y : y; return (s & 1) ? [b, a] : [a, b]; });
    for (let dx = -20; dx <= 20; dx++) for (let dy = -20; dy <= 20; dy++) {
      const cells = pts.map(([x, y]) => [x + dx, y + dy]);
      if (!cells.every(([x, y]) => onb(x, y))) continue;
      const ids = cells.map(([x, y]) => x*SN + y).sort((a, b) => a - b), key = ids.join(',');
      if (!seen.has(key)) { seen.add(key); lines.push(ids); }
    }
  }
  return lines;
}

// ===================================================================== flat copy tables
const LINES = snakeLines();               // 1,664 allowed copies of the shape inside the board
const NL = LINES.length;
const LCELLS = new Int16Array(NL * 6);
LINES.forEach((l, i) => l.forEach((c, k) => { LCELLS[i * 6 + k] = c; }));
const LSTART = new Int32Array(SNC + 1);
const LIDX = (() => {
  const per = Array.from({ length: SNC }, () => []);
  LINES.forEach((l, i) => l.forEach(c => per[c].push(i)));
  const out = []; for (let c = 0; c < SNC; c++) { LSTART[c] = out.length; out.push(...per[c]); }
  LSTART[SNC] = out.length; return Int16Array.from(out);
})();

// per-copy contributions indexed by m*7+b (Maker stones m, Breaker stones b), as in fast_search.h
const SW = [1, 6, 36, 216, 1296, 7776, 0];
const KM = new Int32Array(49), KB = new Int32Array(49), KE = new Int32Array(49), KA = new Int32Array(49);
const KD = new Float64Array(49);          // greedy Erdos-Selfridge danger: 4^m, 1e12 for a copy one stone short
for (let m = 0; m < 7; m++) for (let b = 0; b < 7; b++) {
  const i = m * 7 + b, act = b === 0 && m >= 1 && m <= 5;
  KM[i] = act ? SW[m + 1 < 6 ? m + 1 : 5] * (m + 1 === 6 ? 1000 : 1) - SW[m] : 0;
  KB[i] = act ? SW[m] * (m === 5 ? 1000 : 1) : 0;
  KE[i] = b === 0 ? SW[m] : 0;
  KA[i] = act ? 1 : 0;
  KD[i] = b === 0 ? (m === 5 ? 1e12 : Math.pow(4, m)) : 0;
}

// ===================================================================== RNG + Zobrist
function rng32(seed) { // mulberry32
  let s = seed >>> 0;
  return () => { s = (s + 0x6D2B79F5) >>> 0; let t = s; t = Math.imul(t ^ (t >>> 15), t | 1); t ^= t + Math.imul(t ^ (t >>> 7), t | 61); return ((t ^ (t >>> 14)) >>> 0); };
}
const ZLO = new Int32Array(3 * SNC), ZHI = new Int32Array(3 * SNC);
let ZSLO = 0, ZSHI = 0;
{ const r = rng32(0x5A0B); for (let i = 0; i < 3 * SNC; i++) { ZLO[i] = r() | 0; ZHI[i] = r() | 0; } ZSLO = r() | 0; ZSHI = r() | 0; }

// ===================================================================== board with incremental counts
class Board {
  constructor() {
    this.own = new Uint8Array(SNC); this.lm = new Uint8Array(NL); this.lb = new Uint8Array(NL);
    this.sc = new Int32Array(SNC * 2);     // ordering scores [cell*2 + 0 Maker / 1 Breaker]
    this.dz = new Float64Array(SNC);       // greedy danger per cell (referee level only)
    this.near = new Uint8Array(SNC);       // Maker stones within Chebyshev distance 2 (referee level only)
    this.clear();
  }
  clear() {
    this.own.fill(0); this.lm.fill(0); this.lb.fill(0); this.sc.fill(0); this.near.fill(0);
    for (let c = 0; c < SNC; c++) this.dz[c] = LSTART[c + 1] - LSTART[c];
    this.eval = NL; this.nwon = 0; this.live = NL; this.free = SNC; this.active = 0; this.hlo = 0; this.hhi = 0; this.makerStones = 0;
  }
  _apply(c, who, sgn) {
    const dm = who === 1 ? sgn : 0, db = who === 2 ? sgn : 0, lm = this.lm, lb = this.lb, sc = this.sc;
    for (let i = LSTART[c], e = LSTART[c + 1]; i < e; i++) {
      const l = LIDX[i], m = lm[l], b = lb[l], m2 = m + dm, b2 = b + db, o = m * 7 + b, n = m2 * 7 + b2;
      const dM = KM[n] - KM[o], dB = KB[n] - KB[o];
      this.eval += KE[n] - KE[o]; this.active += KA[n] - KA[o];
      this.nwon += (m2 === 6 ? 1 : 0) - (m === 6 ? 1 : 0);
      this.live += (b2 === 0 ? 1 : 0) - (b === 0 ? 1 : 0);
      lm[l] = m2; lb[l] = b2;
      if (dM | dB) { const q = l * 6; for (let k = 0; k < 6; k++) { const j = LCELLS[q + k] * 2; sc[j] += dM; sc[j + 1] += dB; } }
    }
  }
  play(c, who) { this.own[c] = who; this.free--; this.hlo ^= ZLO[who * SNC + c]; this.hhi ^= ZHI[who * SNC + c]; this._apply(c, who, 1); }
  undo(c) { const who = this.own[c]; this.own[c] = 0; this.free++; this.hlo ^= ZLO[who * SNC + c]; this.hhi ^= ZHI[who * SNC + c]; this._apply(c, who, -1); }
  // referee move: also keeps the Breakers' danger and proximity tables
  refPlay(c, who) {
    const dm = who === 1 ? 1 : 0, db = who === 2 ? 1 : 0, dz = this.dz;
    for (let i = LSTART[c], e = LSTART[c + 1]; i < e; i++) {
      const l = LIDX[i], m = this.lm[l], b = this.lb[l], d = KD[(m + dm) * 7 + b + db] - KD[m * 7 + b];
      if (d !== 0) { const q = l * 6; for (let k = 0; k < 6; k++) dz[LCELLS[q + k]] += d; }
    }
    this.play(c, who);
    if (who === 1) {
      this.makerStones++;
      const x = (c / SN) | 0, y = c % SN;
      for (let dx = -2; dx <= 2; dx++) for (let dy = -2; dy <= 2; dy++) if (onb(x + dx, y + dy)) this.near[(x + dx) * SN + y + dy]++;
    }
  }
  won() { return this.nwon > 0; }
  dead() { return this.live === 0 || this.free === 0; }
  wonLine() { for (let l = 0; l < NL; l++) if (this.lm[l] === 6) return LINES[l]; return null; }
}

// ===================================================================== PRIOR BEST: alpha-beta search
const WIN = 1e9, TTBITS = 16, TTMASK = (1 << TTBITS) - 1, EXACT = 1, LOWER = 2, UPPER = 3;
const perfNow = (typeof performance !== 'undefined' && performance.now) ? () => performance.now() : () => Date.now();
class FastSearch {
  constructor() {
    this.width = 9; this.maxDepth = 40;
    this.nodeCap = 6000;     // node allowance per decision (Infinity = none, < 1 = no search: top-ordered move)
    this.deadline = 0;       // absolute clock deadline in ms (0 = none)
    this.now = perfNow;
    this.nodes = 0; this.out = false; this.lastDepth = 0; this.cut = false; this.gen = 0;
    const T = 1 << TTBITS;
    this.tklo = new Int32Array(T); this.tkhi = new Int32Array(T); this.tval = new Float64Array(T);
    this.tgen = new Uint32Array(T); this.tdep = new Int8Array(T); this.tflag = new Uint8Array(T); this.tmove = new Int16Array(T);
    this.mvb = Array.from({ length: 64 }, () => new Int16Array(16));
    this.bsb = Array.from({ length: 64 }, () => new Float64Array(16));
  }
  candidates(b, maker, ply, k) {
    const mv = this.mvb[ply], bs = this.bsb[ply], side = maker ? 0 : 1, own = b.own, sc = b.sc;
    let n = 0;
    const consider = (c, s) => {
      if (n === k && (s < bs[n - 1] || (s === bs[n - 1] && c > mv[n - 1]))) return;
      let i = n < k ? n++ : k - 1;
      while (i > 0 && (bs[i - 1] < s || (bs[i - 1] === s && mv[i - 1] > c))) { bs[i] = bs[i - 1]; mv[i] = mv[i - 1]; i--; }
      bs[i] = s; mv[i] = c;
    };
    if (b.active > 0) { for (let c = 0; c < SNC; c++) if (own[c] === 0 && sc[c * 2 + side] > 0) consider(c, sc[c * 2 + side]); }
    else { for (let c = 0; c < SNC; c++) if (own[c] === 0) consider(c, 64 - (Math.abs(((c / SN) | 0) - 8) + Math.abs(c % SN - 8))); }
    if (n === 0) for (let c = 0; c < SNC && n < k; c++) if (own[c] === 0) mv[n++] = c;
    return n;
  }
  timeout() {
    if (this.nodes > this.nodeCap) return true;
    if (this.deadline > 0 && (this.nodes & 7) === 0 && this.now() > this.deadline) return true;
    return false;
  }
  ab(b, depth, alpha, beta, maker, ply) {
    this.nodes++;
    if (b.nwon > 0) return WIN - ply;
    if (b.live === 0 || b.free === 0) return -WIN + ply;
    if (depth === 0) return b.eval;
    if (this.timeout()) { this.out = true; return b.eval; }
    const klo = b.hlo ^ (maker ? ZSLO : 0), khi = b.hhi ^ (maker ? ZSHI : 0), s = klo & TTMASK;
    let ttm = -1;
    if (this.tklo[s] === klo && this.tkhi[s] === khi && this.tgen[s] === this.gen) {
      ttm = this.tmove[s];
      if (this.tdep[s] >= depth) {
        let v = this.tval[s]; v = v > WIN - 1000 ? v - ply : v < -WIN + 1000 ? v + ply : v;
        const f = this.tflag[s];
        if (f === EXACT || (f === LOWER && v >= beta) || (f === UPPER && v <= alpha)) return v;
      }
    }
    const n = this.candidates(b, maker, ply, this.width), mv = this.mvb[ply];
    if (ttm >= 0) for (let i = 1; i < n; i++) if (mv[i] === ttm) { for (let j = i; j > 0; j--) mv[j] = mv[j - 1]; mv[0] = ttm; break; }
    const a0 = alpha, b0 = beta;
    let best = maker ? -WIN * 2 : WIN * 2, bm = mv[0];
    for (let i = 0; i < n; i++) {
      const c = mv[i];
      b.play(c, maker ? 1 : 2);
      const v = this.ab(b, depth - 1, alpha, beta, !maker, ply + 1);
      b.undo(c);
      if (this.out) return (best === WIN * 2 || best === -WIN * 2) ? v : best;
      if (maker ? v > best : v < best) { best = v; bm = c; }
      if (maker) { if (best > alpha) alpha = best; } else { if (best < beta) beta = best; }
      if (alpha >= beta) break;
    }
    this.tklo[s] = klo; this.tkhi[s] = khi; this.tgen[s] = this.gen; this.tdep[s] = depth; this.tmove[s] = bm;
    this.tval[s] = best > WIN - 1000 ? best + ply : best < -WIN + 1000 ? best - ply : best;
    this.tflag[s] = best <= a0 ? UPPER : best >= b0 ? LOWER : EXACT;
    return best;
  }
  decide(b, maker) {
    this.nodes = 0; this.out = false; this.cut = false; this.gen = (this.gen + 1) >>> 0;
    const n = this.candidates(b, maker, 0, this.width), mv = this.mvb[0];
    if (n === 0) return -1;
    let best = mv[0];
    this.lastDepth = 0;
    if (this.nodeCap < 1) { this.cut = true; return best; }
    for (let depth = 1; depth <= this.maxDepth; depth++) {
      let bv = maker ? -WIN * 3 : WIN * 3, bm = mv[0], done = 0, alpha = -WIN * 3, beta = WIN * 3;
      for (let i = 0; i < n; i++) {
        if (this.deadline > 0 && this.now() > this.deadline) { this.out = true; break; }
        const c = mv[i];
        b.play(c, maker ? 1 : 2);
        const v = this.ab(b, depth - 1, alpha, beta, !maker, 1);
        b.undo(c);
        if (this.out) break;
        done++;
        if (maker ? v > bv : v < bv) { bv = v; bm = c; }
        if (maker) { if (bv > alpha) alpha = bv; } else { if (bv < beta) beta = bv; }
      }
      if (this.out) { if (done > 0) best = bm; break; }
      best = bm; this.lastDepth = depth;
      if (bv >= WIN - 64 || bv <= -WIN + 64) break;   // proven
      for (let i = 0; i < n; i++) if (mv[i] === bm) { for (let j = i; j > 0; j--) mv[j] = mv[j - 1]; mv[0] = bm; break; }
    }
    this.cut = this.out;
    return best;
  }
}

// ===================================================================== 03-snaky SearchAI (used as the alpha-beta Breaker)
// Same decisions as demos/03-snaky/snaky_game.h SearchAI (no transposition table, depth <= 12, 6000 nodes,
// keeps only fully searched depths, swap-to-front), but reading the incrementally kept eval/ordering scores,
// which equal the full rescans of the C++ original.
class SearchAI {
  constructor(budget) { this.width = 9; this.budget = budget || 6000; this.nodes = 0; this.out = false; this.lastDepth = 0;
    this.fs = new FastSearch(); }   // borrow the candidate generator and buffers
  ab(b, depth, alpha, beta, maker, ply) {
    this.nodes++;
    if (b.nwon > 0) return WIN - ply;
    if (b.live === 0 || b.free === 0) return -WIN + ply;
    if (depth === 0 || this.nodes > this.budget) { if (this.nodes > this.budget) this.out = true; return b.eval; }
    const n = this.fs.candidates(b, maker, ply, this.width), mv = this.fs.mvb[ply];
    let best = maker ? -WIN * 2 : WIN * 2;
    for (let i = 0; i < n; i++) {
      const c = mv[i];
      b.play(c, maker ? 1 : 2);
      const v = this.ab(b, depth - 1, alpha, beta, !maker, ply + 1);
      b.undo(c);
      if (maker) { if (v > best) best = v; if (best > alpha) alpha = best; }
      else { if (v < best) best = v; if (best < beta) beta = best; }
      if (alpha >= beta || this.out) break;
    }
    return best;
  }
  decide(b, maker) {
    this.nodes = 0; this.out = false;
    const n = this.fs.candidates(b, maker, 0, this.width), mv = this.fs.mvb[0];
    if (n === 0) return -1;
    let best = mv[0]; this.lastDepth = 0;
    for (let depth = 1; depth <= 12; depth++) {
      let bv = maker ? -WIN * 3 : WIN * 3, bm = mv[0], alpha = -WIN * 3, beta = WIN * 3;
      for (let i = 0; i < n; i++) {
        b.play(mv[i], maker ? 1 : 2);
        const v = this.ab(b, depth - 1, alpha, beta, !maker, 1);
        b.undo(mv[i]);
        if (this.out) break;
        if (maker ? v > bv : v < bv) { bv = v; bm = mv[i]; }
        if (maker) alpha = Math.max(alpha, bv); else beta = Math.min(beta, bv);
      }
      if (this.out) break;
      best = bm; this.lastDepth = depth;
      if (bv >= WIN - 64 || bv <= -WIN + 64) break;
      for (let i = 0; i < n; i++) if (mv[i] === bm) { const t = mv[0]; mv[0] = mv[i]; mv[i] = t; break; }
    }
    return best;
  }
}

// ===================================================================== Breaker opponents
const BRK_RANDOM = 0, BRK_GREEDY = 1, BRK_SEARCH = 2;
const BRK_NAMES = ['random-local', 'greedy ES blocker', 'alpha-beta Breaker'];
let breakerEngine = null;   // one shared 03-snaky SearchAI for all alpha-beta Breakers (6000 nodes, untimed)
class BreakerAgent {
  constructor(kind, seed) { this.kind = kind; this.rnd = rng32(seed); this.moves = 0; }
  below(n) { return this.rnd() % n; }
  decide(b) {
    if (this.moves++ === 0 && this.kind !== BRK_RANDOM) return this.randomNear(b, 1);  // seeded variety (as 03-snaky)
    if (this.kind === BRK_SEARCH) {
      if (!breakerEngine) breakerEngine = new SearchAI(6000);
      return breakerEngine.decide(b, false);
    }
    if (this.kind === BRK_GREEDY) return this.greedy(b);
    return this.randomNear(b, 2);
  }
  randomNear(b, rad) { // uniformly random free cell within Chebyshev distance rad of a Maker stone
    const own = b.own, cand = [];
    for (let c = 0; c < SNC; c++) {
      if (own[c]) continue;
      let near;
      if (rad === 2) near = b.near[c] > 0;
      else { near = false; const x = (c / SN) | 0, y = c % SN;
        for (let dx = -rad; dx <= rad && !near; dx++) for (let dy = -rad; dy <= rad && !near; dy++) if (onb(x + dx, y + dy) && own[(x + dx) * SN + y + dy] === 1) near = true; }
      if (near) cand.push(c);
    }
    if (!cand.length) for (let c = 0; c < SNC; c++) if (!own[c]) cand.push(c);
    return cand.length ? cand[this.below(cand.length)] : -1;
  }
  greedy(b) { // Erdos-Selfridge blocker: largest danger sum over live copies, danger = 4^(Maker stones)
    const own = b.own, dz = b.dz; let bv = -1, nt = 0; const ties = this.ties || (this.ties = new Int16Array(SNC));
    for (let c = 0; c < SNC; c++) {
      if (own[c]) continue;
      const v = dz[c];
      if (v > bv) { bv = v; nt = 0; ties[nt++] = c; } else if (v === bv) ties[nt++] = c;
    }
    return nt ? ties[nt > 1 ? this.below(nt) : 0] : -1;
  }
}

// ===================================================================== one table = one game, restarted forever
const gameSeed = (slot, gen) => (Math.imul(slot + 1, 1000003) + Math.imul(gen + 1, 7919) + 0xC0FFEE) >>> 0;
class Table {
  constructor(id, side, cert, kind) {
    this.id = id; this.side = side; this.board = new Board(); this.kind = kind; this.gen = 0;
    this.cert = side === 'new' ? new CertPolicy(cert) : null;
    this.last = 0; this.nodes = 0;    // result of the previous finished game (display), nodes of the last search
    this.start();
  }
  start() {
    this.board.clear(); this.brk = new BreakerAgent(this.kind, gameSeed(this.id, this.gen)); this.gen++;
    if (this.cert) this.cert.reset();
    this.makerMoves = 0; this.done = false; this.result = 0; this.winLine = null; this.lastM = -1; this.lastB = -1;
    this.pending = -1;     // Breaker reply not yet absorbed by the certificate walker
    this.hold = 0; this.replies = [];
    this.depth = 0; this.cut = false; this.allotUs = 0; this.usedUs = 0;
  }
  // Maker decision (the timed part): absorb Breaker's last reply, then choose a cell
  makerDecideNew() {
    if (this.pending >= 0) { this.cert.observe(this.pending); this.pending = -1; }
    return this.cert.decide(this.board.own);
  }
  // untimed referee: apply Maker's cell, check for a win, then Breaker replies and the game may end
  applyMaker(c) {
    const b = this.board;
    b.refPlay(c, 1); this.makerMoves++; this.lastM = c;
    if (b.won()) { this.done = true; this.result = 1; this.winLine = b.wonLine(); return; }
    if (b.free === 0) { this.done = true; this.result = -1; return; }
  }
  breakerReply(r) {
    const b = this.board;
    if (r === undefined) r = this.brk.decide(b);
    b.refPlay(r, 2); this.lastB = r; this.replies.push(r);
    if (this.cert) this.pending = r;
    if (b.dead()) { this.done = true; this.result = -1; }
  }
}

// ===================================================================== a hall = N tables played by one technique
// Load model (as the C++ ladder): one tick = one 60 fps frame. Each table makes one Maker move every k ticks,
// so a tick holds about N/k Maker decisions, and all of them share one frame's AI budget (16.7 ms).
// PRIOR gets A = min(16.7, 16.7*k/N) ms per move: a node allowance tuned from measured speed so the moves fit,
// plus a clock deadline (the browser clock ticks in ~0.1 ms steps, so the node allowance does the fine work).
const FRAME_MS = 16.7, FULL_NODES = 6000;   // full strength = 03-snaky's 6000 nodes per move
function newStats() {
  return { ticks: 0, aiMs: 0, moves: 0, tickMs: [], games: 0, wins: 0, sumWinMoves: 0, maxWinMoves: 0,
           cutMoves: 0, shortMoves: 0, depthSum: 0, nodes: 0, breaks: 0, invFails: 0, overTicks: 0 };
}
class Hall {
  constructor(side, cert) {
    this.side = side; this.cert = cert; this.tables = []; this.n = 0; this.kind = BRK_GREEDY; this.k = 1;
    this.mode = 'budget';             // PRIOR only: 'budget' (share the frame) or 'fixed' (6000 nodes per move, ignores budget)
    this.engine = side === 'prior' ? new FastSearch() : null;
    this.cap = 6000; this.usPerNode = 1; this.holdTicks = 0; this.featured = 0; this.now = perfNow;
    this.st = newStats(); this.player = null;
    this.allotN = 0;                  // bench: give each move the share it would get with allotN tables open
    this.onGame = null;
  }
  configure(n, kind, k, mode) {
    const rebuild = n !== this.n || kind !== this.kind;
    this.k = k; this.mode = mode || this.mode;
    if (rebuild) {
      this.n = n; this.kind = kind; this.tables = [];
      for (let i = 0; i < n; i++) this.tables.push(new Table(i, this.side, this.cert, kind));
    }
    this.st = newStats();
    this.cap = Math.max(0.5, this.allotMs() * 1000 / this.usPerNode);
  }
  allotMs() { return Math.min(FRAME_MS, FRAME_MS * this.k / Math.max(1, this.allotN || this.n)); }
  record(t) {
    const s = this.st; s.games++;
    if (t.result > 0) { s.wins++; s.sumWinMoves += t.makerMoves; if (t.makerMoves > s.maxWinMoves) s.maxWinMoves = t.makerMoves; }
    if (t.cert) s.breaks += t.cert.breaks;
    if (this.onGame) this.onGame(t);
  }
  // PRIOR: one search decision for table t with allotment A (ms) inside a frame that started at frameStart
  searchMove(t, A, frameStart) {
    const e = this.engine, t0 = this.now();
    if (this.mode === 'fixed') { e.nodeCap = 6000; e.deadline = 0; }
    else { e.nodeCap = Math.floor(this.cap); e.deadline = Math.min(t0 + A, frameStart + FRAME_MS); }
    const c = e.decide(t.board, true);
    t.depth = e.lastDepth; t.cut = e.cut; t.nodes = e.nodes; t.allotUs = this.mode === 'fixed' ? 0 : A * 1000;
    this.st.depthSum += e.lastDepth; this.st.nodes += e.nodes; if (e.cut) this.st.cutMoves++;
    t.short = e.cut && e.nodes < FULL_NODES; if (t.short) this.st.shortMoves++;   // stopped before a full-strength search
    t.capHit = e.cut && e.nodes >= Math.floor(e.nodeCap);   // stopped by the node allowance (incl. "no time at all")
    return c;
  }
  // one frame of Maker work: every table due this tick decides (timed), then its cell is applied (untimed).
  // The Breaker replies are queued; breakerPhase() plays them (untimed) and may spread them over frames.
  makerPhase(tickNo) {
    const s = this.st, k = this.k, due = [];
    for (const t of this.tables) {
      if (t === this.player) continue;
      if (t.done) { if (t.hold > 0) { t.hold--; continue; } t.start(); }
      if (t.id % k === tickNo % k) due.push(t);
    }
    const m = due.length, cells = this.cells || (this.cells = new Int16Array(65536));
    this.queue = []; this.qi = 0;
    let ms = 0;
    if (m) {
      const t0 = this.now(), nodes0 = s.nodes;
      if (this.side === 'new') {
        for (let i = 0; i < m; i++) cells[i] = due[i].makerDecideNew();
      } else {
        const A = this.allotMs();
        for (let i = 0; i < m; i++) cells[i] = this.searchMove(due[i], A, t0);
      }
      ms = this.now() - t0;
      if (this.side === 'prior' && this.mode === 'budget') {   // tune the node allowance toward A per move
        const avg = ms / m, A = this.allotMs();
        let capHit = false; for (let i = 0; i < m; i++) if (due[i].capHit) { capHit = true; break; }
        if (avg > A || capHit) this.cap = Math.min(2e6, Math.max(0.5, this.cap * Math.sqrt(Math.min(2, Math.max(0.5, A / Math.max(avg, 1e-4))))));
        const dn = s.nodes - nodes0;
        if (dn > 500 && ms > 0.5) this.usPerNode = 0.8 * this.usPerNode + 0.2 * (ms * 1000 / dn);
      }
      for (let i = 0; i < m; i++) {
        const t = due[i];
        t.applyMaker(cells[i]);
        if (t.done) this.finish(t); else this.queue.push(t);
      }
      s.ticks++; s.aiMs += ms; s.moves += m;
      s.tickMs.push(ms); if (s.tickMs.length > 120) s.tickMs.shift();
      if (ms > FRAME_MS) s.overTicks++;
    }
    this.lastTick = { ms, moves: m };
    return this.lastTick;
  }
  finish(t) { this.record(t); t.last = t.result; t.hold = t.id < this.featured ? this.holdTicks : 0; }
  // play queued Breaker replies; returns true when the queue is empty (maxMs bounds the work per call)
  breakerPhase(maxMs) {
    const q = this.queue; if (!q) return true;
    const t0 = maxMs < Infinity ? this.now() : 0;
    while (this.qi < q.length) {
      const t = q[this.qi++];
      if (!t.done) { t.breakerReply(); if (t.done) this.finish(t); }
      if (maxMs < Infinity && this.now() - t0 > maxMs) break;
    }
    return this.qi >= q.length;
  }
  tick(tickNo) { const r = this.makerPhase(tickNo); this.breakerPhase(Infinity); return r; }
  // the human player sits at table t as the Breaker
  playerMove(t, c) {
    if (t.done || t.board.own[c]) return false;
    t.breakerReply(c);
    if (!t.done) this.playerMaker(t);
    return true;
  }
  playerMaker(t) {
    const t0 = this.now();
    const c = this.side === 'new' ? t.makerDecideNew() : this.searchMove(t, this.allotMs(), t0);
    t.usedUs = (this.now() - t0) * 1000;
    t.applyMaker(c);
  }
}
// full-strength cost of the PRIOR search: 6000-node decisions in real games vs the greedy blocker
function calibrateSearch(games) {
  const e = new FastSearch(); e.nodeCap = 6000; e.deadline = 0;
  let ms = 0, nodes = 0, dec = 0;
  for (let g = 0; g < (games || 2); g++) {
    const t = new Table(900 + g, 'prior', null, BRK_GREEDY);
    while (!t.done) {
      const t0 = perfNow(), c = e.decide(t.board, true); ms += perfNow() - t0; nodes += e.nodes; dec++;
      t.applyMaker(c); if (!t.done) t.breakerReply();
    }
  }
  return { usPerNode: ms * 1000 / nodes, msPerMove: ms / dec, decisions: dec };
}
const median = a => { if (!a.length) return 0; const b = [...a].sort((x, y) => x - y), h = b.length >> 1; return b.length % 2 ? b[h] : (b[h - 1] + b[h]) / 2; };

if (typeof module !== 'undefined') module.exports = {
  SN, SNC, NL, LINES, loadCert, CertPolicy, snakeLines, Board, FastSearch, SearchAI, BreakerAgent, Table, Hall,
  BRK_RANDOM, BRK_GREEDY, BRK_SEARCH, BRK_NAMES, gameSeed, perfNow, FRAME_MS, median, calibrateSearch,
};
