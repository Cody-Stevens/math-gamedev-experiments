// Recipe economy from demos/01-economy (same nine mass-action recipes, rates and six starts).
// Species order: O ore, W wood, T tools, F food, P workers.
const EC_SP = ['Ore', 'Wood', 'Tools', 'Food', 'Workers'];
const EC_RX = [ // [from complex, to complex, rate, group]
  [[0,0,0,1,1],[0,0,0,0,2],1.00,0], [[0,0,0,0,2],[0,0,0,0,1],0.35,0], [[0,0,0,0,1],[0,0,0,1,1],0.60,0],
  [[1,1,0,0,0],[0,0,1,0,0],1.20,1], [[0,0,1,0,0],[2,0,0,0,0],0.25,1], [[2,0,0,0,0],[1,1,0,0,0],0.80,1],
  [[0,0,1,1,1],[1,1,0,0,1],0.90,2], [[1,1,0,0,1],[0,0,1,0,2],0.50,2], [[0,0,1,0,2],[0,0,1,1,1],0.70,2]];
const EC_STARTS = [[1,1,1.25,1,1],[4,0.3,0.1,0.01,20],[0.02,3,0.74,8,0.05],[0.5,0.5,1.75,1e-3,2e-3],[1e-3,1e-3,2.249,30,0.3],[2,2.4,0.05,0.2,3]];
function ecName(c) { const S = 'OWTFP'; let s = []; c.forEach((v, i) => { if (v) s.push((v > 1 ? v : '') + S[i]); }); return s.join('+') || '0'; }
// Lint: a recipe is fine iff its output bundle can reach its input bundle through active recipes.
function ecLint(active) {
  const names = [], idx = c => { const k = ecName(c); let i = names.indexOf(k); if (i < 0) { names.push(k); i = names.length - 1; } return i; };
  const E = EC_RX.map(r => [idx(r[0]), idx(r[1])]);
  const n = names.length, reach = names.map((_, i) => { const r = new Array(n).fill(false); r[i] = true; return r; });
  for (let changed = true; changed;) { changed = false;
    E.forEach(([a, b], k) => { if (!active[k]) return; for (let s = 0; s < n; s++) if (reach[s][a] && !reach[s][b]) { reach[s][b] = true; changed = true; } }); }
  return EC_RX.map((r, k) => !active[k] || reach[E[k][1]][E[k][0]]);
}
// Integrate in log space with adaptive RK4. Returns state with x and flags.
function ecDeriv(y, active, k, out) {
  const x = y.map(Math.exp), f = [0,0,0,0,0];
  EC_RX.forEach((r, j) => { if (!active[j]) return; let rate = k[j];
    for (let i = 0; i < 5; i++) if (r[0][i]) rate *= Math.pow(x[i], r[0][i]);
    for (let i = 0; i < 5; i++) f[i] += rate * (r[1][i] - r[0][i]); });
  for (let i = 0; i < 5; i++) out[i] = f[i] / x[i];
  return out;
}
function ecStep(run, active, k, tTarget) {
  const y = run.y, k1 = [0,0,0,0,0], k2 = [0,0,0,0,0], k3 = [0,0,0,0,0], k4 = [0,0,0,0,0], tmp = [0,0,0,0,0];
  let guard = 0;
  while (run.t < tTarget && !run.stopped && guard++ < 20000) {
    ecDeriv(y, active, k, k1);
    let m = 0; for (let i = 0; i < 5; i++) m = Math.max(m, Math.abs(k1[i]));
    const h = Math.min(tTarget - run.t, 0.05, 0.05 / Math.max(m, 1e-9));
    for (let i = 0; i < 5; i++) tmp[i] = y[i] + 0.5 * h * k1[i]; ecDeriv(tmp, active, k, k2);
    for (let i = 0; i < 5; i++) tmp[i] = y[i] + 0.5 * h * k2[i]; ecDeriv(tmp, active, k, k3);
    for (let i = 0; i < 5; i++) tmp[i] = y[i] + h * k3[i]; ecDeriv(tmp, active, k, k4);
    for (let i = 0; i < 5; i++) y[i] += h / 6 * (k1[i] + 2 * k2[i] + 2 * k3[i] + k4[i]);
    run.t += h;
    for (let i = 0; i < 5; i++) if (y[i] > Math.log(1e12) || y[i] < Math.log(1e-30) || !isFinite(y[i])) run.stopped = true;
  }
}
if (typeof module !== 'undefined') module.exports = { EC_SP, EC_RX, EC_STARTS, ecLint, ecStep, ecName };
