const { DG_N, DG_TCERT, dgTemplate, dgQuick, dgCertified, dgCurveball, dgKept, popc } = require('../dungeon.js');
const T = dgTemplate(), R = (() => { let s = 99; return n => { s ^= s << 13; s ^= s >>> 17; s ^= s << 5; return (s >>> 0) % n; }; })();
const deg = Array.from(T, popc); console.log('doors', deg.reduce((a, b) => a + b) / 2, 'degrees', deg.join(','), 'Tcert', DG_TCERT);
let q = 0, t0 = Date.now(); for (let i = 0; i < 20000; i++) q += dgKept(dgQuick(T, R), T); const tq = (Date.now() - t0) / 20000;
let c = 0; t0 = Date.now(); for (let i = 0; i < 300; i++) { const a = dgCertified(T, R); c += dgKept(a, T); if (Array.from(a, popc).join() !== deg.join()) throw 'degree changed'; } const tc = (Date.now() - t0) / 300;
const a = Uint32Array.from(T); for (let k = 0; k < 200000; k++) dgCurveball(a, R); let ref = 0; for (let i = 0; i < 40000; i++) { for (let k = 0; k < 50; k++) dgCurveball(a, R); ref += dgKept(a, T); }
console.log('mean kept quick', (q / 20000).toFixed(2), 'certified', (c / 300).toFixed(2), 'fair ref', (ref / 40000).toFixed(2), 'ms/layout quick', tq.toFixed(4), 'cert', tc.toFixed(3));
