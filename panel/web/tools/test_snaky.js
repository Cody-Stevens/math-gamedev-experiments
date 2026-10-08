// Self-check of the JS port: certificate vs random-local and greedy blockers, seeded.
const { SN, SNC, loadCert, CertPolicy, snakeLines } = require('../snaky.js');
const C = loadCert(require('../snaky_cert.json'));
const L = snakeLines();
let s = 12345; const rnd = n => { s = (s * 1103515245 + 12345) >>> 0; return (s >>> 8) % n; };
function randomLocal(own) {
  const cand = [];
  for (let c = 0; c < SNC; c++) { if (own[c]) continue; const x = (c / SN) | 0, y = c % SN; let near = false;
    for (let dx = -2; dx <= 2 && !near; dx++) for (let dy = -2; dy <= 2 && !near; dy++) { const X = x + dx, Y = y + dy; if (X >= 0 && Y >= 0 && X < SN && Y < SN && own[X*SN + Y] === 1) near = true; }
    if (near) cand.push(c); }
  return cand[rnd(cand.length)];
}
function greedy(own) {
  const sc = new Float64Array(SNC);
  for (const l of L) { let m = 0, b = 0; for (const c of l) { if (own[c] === 1) m++; else if (own[c] === 2) b++; } if (b) continue;
    const d = m === 5 ? 1e12 : Math.pow(4, m); for (const c of l) if (!own[c]) sc[c] += d; }
  let best = -1, bv = -1; for (let c = 0; c < SNC; c++) if (!own[c] && sc[c] > bv) { bv = sc[c]; best = c; }
  return best;
}
const won = own => L.some(l => l.every(c => own[c] === 1));
let games = 0, wins = 0, maxMoves = 0, breaks = 0, inv = 0, t = 0, dec = 0;
for (const kind of ['random', 'greedy']) for (let g = 0; g < 500; g++) {
  const own = new Uint8Array(SNC), P = new CertPolicy(C); let moves = 0, w = false;
  while (moves < 60) {
    const t0 = process.hrtime.bigint(); const c = P.decide(own); t += Number(process.hrtime.bigint() - t0); dec++;
    own[c] = 1; moves++; if (won(own)) { w = true; break; }
    const b = kind === 'random' ? randomLocal(own) : greedy(own); own[b] = 2;
    const t1 = process.hrtime.bigint(); P.observe(b); t += Number(process.hrtime.bigint() - t1);
    if (!P.invariant(own)) inv++;
  }
  games++; if (w) wins++; maxMoves = Math.max(maxMoves, moves); breaks += P.breaks;
}
console.log({ games, wins, maxMoves, invariant_breaks: breaks, invariant_fail_checks: inv, us_per_decision: (t / dec / 1000).toFixed(2) });
