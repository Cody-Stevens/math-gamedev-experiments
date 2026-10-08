// node bench.js [--quick]   Headline numbers for Wildfire Isle, using the same core.js as index.html.
const C = require('./core.js');
const quick = process.argv.includes('--quick');
const M = quick ? { 32: 2000, 64: 1000, 256: 300, 1024: 100 } : { 32: 8000, 64: 4000, 256: 1200, 1024: 400 };
const now = () => Number(process.hrtime.bigint()) / 1e6;
const f = (v, d = 1) => v.toFixed(d), pct = v => (v >= 0 ? '+' : '') + (100 * v).toFixed(1) + '%';

// ---- (b) coastline: offline exponent fit at 3 sizes, then counts at the shipped levels
function fitD(detail, sizes) {
  const xs = sizes.map(Math.log), ys = sizes.map(n => Math.log(C.coastEdges(n, C.octavesFor(n, detail)).E));
  const mx = xs.reduce((a, b) => a + b) / xs.length, my = ys.reduce((a, b) => a + b) / ys.length;
  let sxy = 0, sxx = 0; xs.forEach((x, k) => { sxy += (x - mx) * (ys[k] - my); sxx += (x - mx) ** 2; }); return sxy / sxx;
}
console.log('Coastline exponent, least-squares fit of log(coast edges) vs log(n) over n = 64, 256, 1024 (offline):');
for (const detail of ['grow', 'fixed']) console.log(`  terrain detail ${detail.padEnd(5)}: d = ${f(fitD(detail, [64, 256, 1024]), 3)}   (core.js uses ${C.D_COAST[detail]})`);
const coast = {};
for (const detail of ['grow', 'fixed']) {
  coast[detail] = C.QUALITY.map(q => {
    const reps = q.n >= 1024 ? 3 : q.n >= 256 ? 5 : 20; const ts = []; let c;
    for (let r = 0; r < reps; r++) { const t = now(); c = C.coastEdges(q.n, C.octavesFor(q.n, detail)); C.spawnCoast(c, 0.01); ts.push(now() - t); }
    return { E: c.E, ms: C.median(ts) };
  });
}
// ---- (a) fire fronts
const fire = C.QUALITY.map(q => {
  const Ns = []; const t = now();
  for (let s = 0; s < M[q.n]; s++) { const r = C.fkTrace(q.n, C.frontSeed(q.n, s), false); if (r.N > 0) Ns.push(r.N); }
  return { n: q.n, Ns, ms: (now() - t) / M[q.n], med: C.median(Ns), ci: C.bootMedianCI(Ns) };
});
const K = C.fireConstants(fire.map(x => x.med));
const hours = (mode, L) => mode === 'ref' ? fire[C.ULTRA].med * C.stepHours(K, 'ref', 1024) : fire[L].med * C.stepHours(K, mode, C.QUALITY[L].n);

for (const detail of ['grow', 'fixed']) {
  const d = C.D_COAST[detail], E = coast[detail].map(x => x.E), S = C.spawnConstants(E, d);
  const cnt = (mode, L) => mode === 'ref' ? C.spawnCount(E[C.ULTRA], S.ref) : C.spawnCount(E[L], C.spawnWeight(S, mode, C.QUALITY[L].n, d));
  console.log(`\nTerrain detail: ${detail === 'grow' ? 'grows with quality (octaves = log2 n - 2)' : 'fixed at 6 octaves (band-limited)'}; spawn exponent d = ${d}; design targets ${C.FIRE_TARGET_H} h, ${C.SPAWN_TARGET} spawns`);
  console.log('quality  grid  | fire arrival median, hours    | coastal spawns        | compute ms (fire front + coast build)');
  console.log('               | PRIOR    NEW     REF    NEW95%CI      | PRIOR NEW REF  naive | PRIOR=NEW            REFERENCE');
  C.QUALITY.forEach((q, L) => {
    const naive = Math.round(C.SPAWN_TARGET * E[L] / E[C.TUNED]); // per-tile constant with no metre scaling at all
    const ci = fire[L].ci.map(pct).join('..');
    console.log(`${q.name.padEnd(7)} ${String(q.n).padStart(5)} | ${f(hours('prior', L)).padStart(6)} ${f(hours('new', L)).padStart(6)} ${f(hours('ref', L)).padStart(6)}  ${ci.padEnd(13)} | ${String(cnt('prior', L)).padStart(5)} ${String(cnt('new', L)).padStart(3)} ${String(cnt('ref', L)).padStart(3)} ${String(naive).padStart(6)} | ${f(fire[L].ms, 3).padStart(7)} + ${f(coast[detail][L].ms, 1).padStart(6)}     ${f(fire[C.ULTRA].ms, 3).padStart(7)} + ${f(coast[detail][C.ULTRA].ms, 1).padStart(6)}`);
  });
}
console.log('\nFronts sampled per grid:', C.QUALITY.map(q => `${q.n}: ${M[q.n]}`).join(', '), '| median lattice steps:', fire.map(x => `${x.n}: ${x.med}`).join(', '));
console.log('median steps / n^1.75:', fire.map(x => `${x.n}: ${f(x.med / Math.pow(x.n, 1.75), 3)}`).join(', '), '(C++ reference at 8192: median duration 3.053, demos/L3-fractal-ladder/out/ladder.json)');
console.log('coast edges (grow):', coast.grow.map(x => x.E).join(', '), '| (fixed):', coast.fixed.map(x => x.E).join(', '));

// headline numbers for the page: index.html shows these only if the browser's timer is frozen (headless virtual time)
require('fs').writeFileSync(__dirname + '/bench.json', JSON.stringify({ node: process.version, quick,
  fireMs: Object.fromEntries(fire.map(x => [x.n, +x.ms.toFixed(4)])),
  coastMs: { grow: coast.grow.map(x => +x.ms.toFixed(3)), fixed: coast.fixed.map(x => +x.ms.toFixed(3)) },
  fronts: Object.fromEntries(fire.map(x => [x.n, x.Ns.length])), medianSteps: Object.fromEntries(fire.map(x => [x.n, x.med])) }, null, 1));
console.log('wrote bench.json');
