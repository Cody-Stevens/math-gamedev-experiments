// node bench.js  -- headline numbers for Colony Ledger, using the same core.js as index.html.
'use strict';
const C = require('./core.js');
const now = () => Number(process.hrtime.bigint()) / 1e6;
const pct = x => (x * 100).toFixed(1) + '%';
const med = a => { const s = a.slice().sort((x, y) => x - y); return s[s.length >> 1]; };
const colony = off => C.CL_CATALOG.filter(c => c.g !== 'plague').map(c => C.clRecipe(c, !off.includes(c.id)));
const fmtR = r => C.clName(r.from) + ' -> ' + C.clName(r.to);
const quick = process.argv.includes('--quick');

// ---------------------------------------------------------------- 1. lint verdicts, cost, fix
function lintTime(rs) { // median of 25 batches of 200 lints
  const ts = [];
  for (let b = 0; b < 25; b++) { const t0 = now(); for (let i = 0; i < 200; i++) C.clLint(rs); ts.push((now() - t0) / 200); }
  return med(ts);
}
// pure SIRS on its own (S = Workers): S+I->2I, I->R, R->S, and with 2I->S+I, S->I added
const sirs = [
  { from: { P: 1, I: 1 }, to: { I: 2 } }, { from: { I: 1 }, to: { R: 1 } }, { from: { R: 1 }, to: { P: 1 } },
].map((c, i) => ({ id: 's' + i, g: 'plague', from: C.clVec(c.from), to: C.clVec(c.to), k: 1, name: 'sirs', active: true }));
const sirsFixed = sirs.concat([{ from: { I: 2 }, to: { P: 1, I: 1 } }, { from: { P: 1 }, to: { I: 1 } }]
  .map((c, i) => ({ id: 'f' + i, g: 'plague', from: C.clVec(c.from), to: C.clVec(c.to), k: 1, name: 'fix', active: true })));
const lintCases = [
  ['colony, 9 recipes (scene fixed)', C.clSceneRecipes('fixed')],
  ['colony, crowding+salvage removed (broken)', C.clSceneRecipes('broken')],
  ['pure SIRS  S+I->2I, I->R, R->S', sirs],
  ['pure SIRS + 2I->S+I + S->I', sirsFixed],
  ['colony + SIRS plague (scene plague)', C.clSceneRecipes('plague')],
  ['colony + plague + 2 returns (endemic)', C.clSceneRecipes('endemic')],
];
console.log('\n1. LINT (weak reversibility, recipe by recipe) -- median time per lint call');
console.log('case'.padEnd(44) + 'verdict'.padEnd(26) + 'us/lint   fix adds');
for (const [nm, rs] of lintCases) {
  const L = C.clLint(rs), us = lintTime(rs) * 1000;
  const copy = rs.map(r => ({ ...r })), add = C.clFix(copy);
  console.log(nm.padEnd(44) + (L.pass ? 'PASS' : 'FAIL: ' + L.bad + ' lack a way back').padEnd(26) + us.toFixed(1).padStart(7) + '   ' + (add.map(fmtR).join(' ; ') || '-'));
}

// ---------------------------------------------------------------- 2. Monte-Carlo sweep vs horizon
const HS = quick ? [2, 5, 10, 20, 40] : [1, 2, 5, 10, 20, 40, 80, 160, 320, 640];
const K = 64;
const sweepCases = [
  ['broken (rates x1)', C.clSceneRecipes('broken'), 1],
  ['broken, slow economy (rates x0.2)', C.clSceneRecipes('broken'), 0.2],
  ['melt-down removed (lint FAIL)', colony(['melt']), 1],
  ['fixed (lint PASS)', C.clSceneRecipes('fixed'), 1],
  ['starve (lint PASS, tiny farm rates)', C.clSceneRecipes('starve'), 1],
];
console.log(`\n2. PRIOR BEST balance sweep: K = ${K} random towns (rates x[1/1.5,1.5], starts in [0.3,3]), clamped sim, dt 0.5;`);
console.log('   cell = seeds flagged (a clamp/faucet fired) / ms wall time; a sweep "catches" the chain if >= 1 seed is flagged');
console.log('case'.padEnd(38) + HS.map(h => ('H=' + h).padStart(12)).join(''));
for (const [nm, rs, sc] of sweepCases) {
  const cells = HS.map(H => { const t0 = now(), r = C.clSweep(rs, { K, H, rateScale: sc }); return (r.flagged + '/' + (now() - t0).toFixed(0) + 'ms').padStart(12); });
  console.log(nm.padEnd(38) + cells.join(''));
}
console.log(`   town-years simulated per sweep = K x H (minus early exits); e.g. H=40 -> up to ${K * 40} town-years.`);

// ---------------------------------------------------------------- 3. outcomes per method
const N = 240, FR = 240;
console.log(`\n3. OUTCOMES, ${N} towns, t = ${FR / 2} years (${FR} frames of 0.5), seed 149, same towns both sides`);
console.log('scene'.padEnd(10) + 'lint'.padEnd(6) + '| NEW (no clamps): died out  ran away  either  below 1 unit |  PRIOR (clamps): pinned now  time pinned  below 1 unit');
const stopNotes = [];
const scenes = ['broken', 'fixed', 'plague', 'endemic', 'slowrot', 'starve', 'tiny'];
for (const sc of scenes) {
  const S = C.CL_SCENES[sc], rs = C.clSceneRecipes(sc), net = C.clNet(rs, S.rateScale), L = C.clLint(rs);
  const a = C.clTowns(net, N, 149, { integer: S.integer }), b = C.clTowns(net, N, 149, { integer: S.integer });
  for (let f = 0; f < FR; f++) { C.clFrame(net, a, 0.5, false); C.clFrame(net, b, 0.5, true); }
  const sa = C.clSummary(a), sb = C.clSummary(b);
  const by = sa.deadBy.map((c, i) => c ? C.CL_SP[i] + ' ' + c : '').filter(Boolean).join(',');
  if (a.stopOnly) stopNotes.push(`${sc}: ${a.stopOnly} of ${N} towns`);
  console.log(sc.padEnd(10) + (L.pass ? 'PASS' : 'FAIL').padEnd(6) + '|' + pct(sa.dead).padStart(25) + (by ? ' (' + by + ')' : '').padEnd(0) + pct(sa.runaway).padStart(10) + pct(sa.deadOrRun).padStart(8) + pct(sa.starving).padStart(14) + ' |' + pct(sb.pinnedNow).padStart(29) + pct(sb.timePinned).padStart(13) + pct(sb.starving).padStart(14));
}

console.log('   status decided by the single RK4 step that left [1e-30, 1e12] (the town was still inside [1e-6, 1e6] one step before): ' + (stopNotes.join('; ') || 'none'));

// single-recipe removals: the lint fails for every one, but collapse is measured in only some of them
console.log(`\n3b. Each single colony recipe removed: lint verdict vs measured outcome (NEW, no clamps), ${N} towns, t = 1000`);
for (const c of C.CL_CATALOG.filter(c => c.g !== 'plague')) {
  const rs = colony([c.id]), net = C.clNet(rs), L = C.clLint(rs), a = C.clTowns(net, N, 149);
  for (let f = 0; f < 2000; f++) C.clFrame(net, a, 0.5, false);
  const s = C.clSummary(a);
  let lo = Infinity; for (let q = 0; q < a.y.length; q++) lo = Math.min(lo, a.y[q]);
  console.log(('  remove ' + c.name).padEnd(30) + (L.pass ? 'PASS' : 'FAIL').padEnd(6) + 'died out ' + pct(s.dead).padStart(6) + '  ran away ' + pct(s.runaway).padStart(6) + (a.stopOnly ? ` (${a.stopOnly} by the out-of-range step)` : '') + '  smallest amount in any town 1e' + (lo / Math.LN10).toFixed(1));
}

// ---------------------------------------------------------------- 4. sim cost per frame
console.log('\n4. SIM COST: ms per frame (dt 0.5 years), median over frames 1-240 [mean], 1 thread');
console.log('N'.padStart(6) + '   fixed NEW        fixed PRIOR      broken NEW       broken PRIOR');
for (const n of quick ? [240, 1000] : [60, 240, 1000, 4000]) {
  const row = [];
  for (const sc of ['fixed', 'broken']) for (const clamp of [false, true]) {
    const net = C.clNet(C.clSceneRecipes(sc)), T = C.clTowns(net, n, 149), ts = [];
    for (let f = 0; f < 240; f++) { const t0 = now(); C.clFrame(net, T, 0.5, clamp); ts.push(now() - t0); }
    row.push((med(ts).toFixed(3) + ' [' + (ts.reduce((x, y) => x + y, 0) / ts.length).toFixed(2) + ']').padEnd(17));
  }
  console.log(String(n).padStart(6) + '   ' + row.join(''));
}
console.log('\nNotes: lint and sweep numbers are wall time on this machine; the sweep cost grows with K x H, the lint does not depend on rates or horizon.');
