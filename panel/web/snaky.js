// Snaky certificate walker: a line-by-line JS port of CertPolicy in demos/03-snaky/snaky_cert.h.
// own[c]: 0 free, 1 Maker, 2 Breaker; cell c = x*17 + y.
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
// all placements of the Snaky shape on the 17x17 board (8 symmetries x translations)
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
if (typeof module !== 'undefined') module.exports = { SN, SNC, loadCert, CertPolicy, snakeLines };
