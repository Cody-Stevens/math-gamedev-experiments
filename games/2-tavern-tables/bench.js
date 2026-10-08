// bench.js - headline numbers for "Tavern of a Thousand Tables" outside the browser (same core.js).
//   node bench.js            full run (~3 min)
//   node bench.js --quick    fewer games / shorter timings
// Writes bench.json next to this file.
'use strict';
const fs = require('fs'), path = require('path');
const C = require('./core.js');
const cert = C.loadCert(require('./snaky_cert.json'));
const quick = process.argv.includes('--quick');
const G = quick ? 20 : 60;                 // games per (stage, Breaker) in the quality runs, as the C++ ladder
const NS = [1, 4, 16, 64, 256, 1024, 4096];
const fmt = (v, d = 2) => v === null || v === undefined ? '-' : (typeof v === 'number' ? v.toFixed(d) : String(v));
const pad = (s, w) => String(s).padStart(w);

// ---------------------------------------------------------------- 1. full-strength search cost
const cal = C.calibrateSearch(quick ? 2 : 6);
console.log(`PRIOR full strength (6000 nodes/move, vs greedy): ${cal.msPerMove.toFixed(2)} ms per move, ${cal.usPerNode.toFixed(2)} us per node (${cal.decisions} decisions)`);

// ---------------------------------------------------------------- 2. timing ladder: every table moves every frame (k = 1)
function timeHall(side, n, mode, maxMs, maxTicks) {
  const h = new C.Hall(side, cert); h.usPerNode = cal.usPerNode; h.configure(n, C.BRK_GREEDY, 1, mode);
  const t0 = C.perfNow(); let tick = 0;
  h.tick(tick++);                                       // warm-up
  h.st = Object.assign(h.st, { tickMs: [], aiMs: 0, moves: 0, ticks: 0, cutMoves: 0, shortMoves: 0, depthSum: 0, nodes: 0 });
  const all = [];
  while (h.st.ticks < 3 || (C.perfNow() - t0 < maxMs && h.st.ticks < maxTicks)) { const r = h.tick(tick++); if (r.moves) all.push(r.ms); }
  return { medianMs: C.median(all), usPerMove: h.st.aiMs * 1000 / h.st.moves, ticks: h.st.ticks,
           cutPct: 100 * h.st.shortMoves / h.st.moves, depth: h.st.depthSum / h.st.moves };
}
const timing = [];
console.log('\nTIMING: Maker AI ms per frame, every table moves every frame (k=1), greedy Breakers (untimed)');
console.log(`${pad('tables', 7)} ${pad('PRIOR 6000n ms', 15)} ${pad('PRIOR budget ms', 16)} ${pad('short%', 6)} ${pad('depth', 6)} ${pad('NEW ms', 9)} ${pad('NEW us/move', 12)}`);
for (const n of [...NS, 16384]) {
  const fixed = n <= 64 ? timeHall('prior', n, 'fixed', quick ? 800 : 2500, 400) : null;
  const bud = timeHall('prior', n, 'budget', quick ? 800 : 2500, 600);
  const nw = timeHall('new', n, 'budget', quick ? 400 : 1500, 4000);
  timing.push({ n, prior_fixed_ms: fixed && fixed.medianMs, prior_budget_ms: bud.medianMs, prior_budget_cut_pct: bud.cutPct,
                prior_budget_depth: bud.depth, new_ms: nw.medianMs, new_us_per_move: nw.usPerMove });
  console.log(`${pad(n, 7)} ${pad(fixed ? fmt(fixed.medianMs, 1) : 'not timed', 15)} ${pad(fmt(bud.medianMs, 1), 16)} ${pad(fmt(bud.cutPct, 0), 6)} ${pad(fmt(bud.depth, 1), 6)} ${pad(fmt(nw.medianMs, 4), 9)} ${pad(fmt(nw.usPerMove, 3), 12)}`);
}
const big = timing[timing.length - 1];
console.log(`tables whose Maker move fits one 16.7 ms frame: PRIOR full strength ${Math.floor(16.7 / cal.msPerMove)}; NEW all ${big.n} timed (${big.new_ms.toFixed(2)} ms/frame), about ${Math.floor(big.n * 16.7 / big.new_ms).toLocaleString()} by linear estimate`);
console.log('short% = moves stopped before a full-strength (6000-node) search; depth = mean completed lookahead');

// ---------------------------------------------------------------- 3. quality: win rate when the frame is shared by n tables
// PRIOR: each move gets 16.7/n ms (node allowance tuned to measured speed + clock deadline); played on
// min(n, 8) real tables. NEW: the certificate does not depend on the budget. Same seeded Breakers on both.
function quality(side, n, kind) {
  const h = new C.Hall(side, cert); h.usPerNode = cal.usPerNode; h.allotN = n;
  const m = side === 'new' ? Math.min(G, 8) : Math.min(n, 8), quota = Math.ceil(G / m);
  h.configure(m, kind, 1, 'budget');
  const per = new Array(m).fill(0), res = [];
  h.onGame = t => { if (per[t.id] < quota && res.length < G) { per[t.id]++; res.push({ win: t.result > 0, moves: t.makerMoves }); } };
  let tick = 0; while (res.length < G) h.tick(tick++);
  const wins = res.filter(r => r.win);
  return { games: res.length, wins: wins.length, winPct: 100 * wins.length / res.length,
           avgWinMoves: wins.length ? wins.reduce((a, r) => a + r.moves, 0) / wins.length : null,
           maxWinMoves: wins.length ? Math.max(...wins.map(r => r.moves)) : null,
           usPerMove: h.st.aiMs * 1000 / h.st.moves, cutPct: 100 * h.st.shortMoves / Math.max(1, h.st.moves), breaks: h.st.breaks };
}
const qual = [];
const qNew = {};
for (const kind of [C.BRK_GREEDY, C.BRK_SEARCH, C.BRK_RANDOM]) qNew[kind] = quality('new', 1, kind);
console.log(`\nQUALITY: Maker win rate, ${G} games per cell, PRIOR move allowance = 16.7 ms / tables`);
console.log(`NEW (budget-independent): vs greedy ${qNew[1].wins}/${qNew[1].games}, vs alpha-beta ${qNew[2].wins}/${qNew[2].games}, vs random ${qNew[0].wins}/${qNew[0].games}; max moves in a win ${Math.max(qNew[1].maxWinMoves, qNew[2].maxWinMoves, qNew[0].maxWinMoves)}; invariant breaks ${qNew[1].breaks + qNew[2].breaks + qNew[0].breaks}`);
console.log(`${pad('tables', 7)} ${pad('allot us', 9)} ${pad('PRIOR vs greedy', 16)} ${pad('avg/max win mv', 15)} ${pad('PRIOR vs a-b', 13)} ${pad('avg/max win mv', 15)} ${pad('short%', 6)} ${pad('NEW', 6)}`);
for (const n of NS) {
  const g = quality('prior', n, C.BRK_GREEDY), a = quality('prior', n, C.BRK_SEARCH);
  qual.push({ n, allot_us: 16700 / n, prior_vs_greedy: g, prior_vs_ab: a });
  console.log(`${pad(n, 7)} ${pad(fmt(16700 / n, 1), 9)} ${pad(`${g.wins}/${g.games} (${g.winPct.toFixed(0)}%)`, 16)} ${pad(`${fmt(g.avgWinMoves, 1)}/${fmt(g.maxWinMoves, 0)}`, 15)} ${pad(`${a.wins}/${a.games} (${a.winPct.toFixed(0)}%)`, 13)} ${pad(`${fmt(a.avgWinMoves, 1)}/${fmt(a.maxWinMoves, 0)}`, 15)} ${pad(fmt(a.cutPct, 0), 6)} ${pad('100%', 6)}`);
}
const out = { generated: new Date().toISOString(), node: process.version, quick, games_per_cell: G, calibration: cal,
  timing, quality_new: qNew, quality_prior: qual,
  notes: 'All numbers measured by bench.js with core.js on this machine. Breaker moves, referee and restarts are untimed. PRIOR = FastSearch port of demos/L1-snaky-ladder/fast_search.h; alpha-beta Breaker = port of demos/03-snaky SearchAI at 6000 nodes.' };
fs.writeFileSync(path.join(__dirname, 'bench.json'), JSON.stringify(out, null, 1));
console.log('\nwrote bench.json');
