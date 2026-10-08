// Critical (q = 1) square-lattice FK Dobrushin interface: JS port of demos/04-fractal-frontier/fk.h.
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
if (typeof module !== 'undefined') module.exports = { fkTrace };
