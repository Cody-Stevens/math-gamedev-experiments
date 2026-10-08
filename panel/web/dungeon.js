// Dungeon template and the two shuffles from demos/06-dungeon-shuffle (20 rooms, 28 doors).
const DG_COLS = 5, DG_ROWS = 4, DG_N = 20;
function dgTemplate() {
  const adj = new Uint32Array(DG_N), id = (r, c) => r * DG_COLS + c, tog = (u, v) => { adj[u] ^= 1 << v; adj[v] ^= 1 << u; };
  for (let r = 0; r < DG_ROWS; r++) for (let c = 0; c + 1 < DG_COLS; c++) tog(id(r, c), id(r, c + 1));
  for (let r = 0; r + 1 < DG_ROWS; r++) for (let c = 0; c < DG_COLS; c++) tog(id(r, c), id(r + 1, c));
  for (const e of [[0,1,0,2],[2,3,2,4],[1,0,2,0],[2,2,3,2],[1,3,2,3],[3,0,3,1]]) tog(id(e[0], e[1]), id(e[2], e[3]));
  tog(id(1, 2), id(3, 4)); tog(id(1, 2), id(0, 0)); tog(id(2, 1), id(0, 4));
  return adj;
}
const popc = x => { x -= (x >>> 1) & 0x55555555; x = (x & 0x33333333) + ((x >>> 2) & 0x33333333); return Math.imul((x + (x >>> 4)) & 0x0F0F0F0F, 0x01010101) >>> 24; };
const ctz = x => 31 - Math.clz32(x & -x);
function dgRng(seed) { let s = seed >>> 0 || 1; return n => { s ^= s << 13; s ^= s >>> 17; s ^= s << 5; return ((s >>> 0) % n); }; }
// WITH: Curveball pair trade (uniform pair; re-deal their private neighbours by a partial Fisher-Yates shuffle)
function dgCurveball(adj, R) {
  const i = R(DG_N); let j = R(DG_N - 1); if (j >= i) j++;
  const bi = 1 << i, bj = 1 << j, A = adj[i] & ~bj, B = adj[j] & ~bi, S = A ^ B;
  if (!S) return;
  const Ai = A & S, q = popc(Ai), N = popc(S);
  if (q === 0 || q === N) return;
  const list = []; for (let s = S; s; s &= s - 1) list.push(ctz(s));
  let nA = 0; for (let k = 0; k < q; k++) { const t = k + R(N - k); const tmp = list[k]; list[k] = list[t]; list[t] = tmp; nA |= 1 << list[k]; }
  const flip = nA ^ Ai; if (!flip) return;
  adj[i] ^= flip; adj[j] ^= flip;
  for (let s = flip; s; s &= s - 1) adj[ctz(s)] ^= bi | bj;
}
// WITHOUT: lazy double-edge swap (pick two doors, re-pair their ends, reject if it would double a door)
function dgSwap(adj, edges, R) {
  const m = edges.length, a = R(m); let b = R(m - 1); if (b >= a) b++;
  let [u, v] = edges[a], [x, y] = edges[b];
  if (R(2)) { const t = x; x = y; y = t; }
  if (u === x || u === y || v === x || v === y) return;
  if ((adj[u] >> x) & 1 || (adj[v] >> y) & 1) return;
  const tog = (p, q) => { adj[p] ^= 1 << q; adj[q] ^= 1 << p; };
  tog(u, v); tog(x, y); tog(u, x); tog(v, y);
  edges[a] = [u, x]; edges[b] = [v, y];
}
function dgEdges(adj) { const e = []; for (let u = 0; u < DG_N; u++) for (let v = u + 1; v < DG_N; v++) if ((adj[u] >> v) & 1) e.push([u, v]); return e; }
function dgKept(adj, T) { let s = 0; for (let i = 0; i < DG_N; i++) s += popc(adj[i] & T[i]); return s / 2; }
const DG_B = DG_N * (DG_N - 1) / 2, DG_TCERT = Math.ceil(DG_B * (1 + DG_B / 2) * Math.log(2)), DG_M = 28;
function dgQuick(T, R) { const a = Uint32Array.from(T), e = dgEdges(a); for (let k = 0; k < DG_M; k++) dgSwap(a, e, R); return a; }
function dgCertified(T, R) { const a = Uint32Array.from(T); for (let k = 0; k < DG_TCERT; k++) dgCurveball(a, R); return a; }
if (typeof module !== 'undefined') module.exports = { DG_N, DG_TCERT, dgTemplate, dgQuick, dgCertified, dgCurveball, dgKept, popc };
