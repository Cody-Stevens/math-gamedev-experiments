// Interactive demos for the practical-math page. Cores: snaky.js, econ.js, fk.js, dungeon.js.
const css = n => getComputedStyle(document.documentElement).getPropertyValue(n).trim();
function fit(cv) {
  const r = window.devicePixelRatio || 1, w = cv.clientWidth, h = cv.clientHeight;
  if (cv.width !== Math.round(w * r) || cv.height !== Math.round(h * r)) { cv.width = Math.round(w * r); cv.height = Math.round(h * r); }
  const g = cv.getContext('2d'); g.setTransform(r, 0, 0, r, 0, 0); g.clearRect(0, 0, w, h);
  return { g, w, h };
}
const tip = document.getElementById('tip');
function showTip(e, html) { tip.innerHTML = html; tip.style.display = 'block'; tip.style.left = Math.min(e.clientX + 14, innerWidth - 290) + 'px'; tip.style.top = (e.clientY + 14) + 'px'; }
function hideTip() { tip.style.display = 'none'; }
const fmtMs = v => v == null ? '—' : v >= 1000 ? (v / 1000).toFixed(2) + ' s' : v >= 1 ? v.toFixed(v >= 100 ? 0 : 1) + ' ms' : v >= 0.001 ? (v * 1000).toFixed(v >= 0.01 ? 0 : 1) + ' µs' : (v * 1e6).toFixed(0) + ' ns';
const fmtN = v => v >= 1e6 ? (v / 1e6) + 'M' : v >= 1024 ? (v / 1024) + 'k' : String(v);
const font = (px, w = 400, mono = false) => `${w} ${px}px ${mono ? 'ui-monospace, Consolas, monospace' : 'system-ui, sans-serif'}`;
const visible = new Set();
const io = new IntersectionObserver(es => es.forEach(e => e.isIntersecting ? visible.add(e.target.id) : visible.delete(e.target.id)), { rootMargin: '200px' });
document.querySelectorAll('section.m[id]').forEach(s => io.observe(s));

// ===================================================================== 1 SNAKY
const CERT = loadCert(SNAKY_CERT);
const LINES = snakeLines();
const LINES_OF = Array.from({ length: SNC }, () => []);
LINES.forEach((l, i) => l.forEach(c => LINES_OF[c].push(i)));
const sk = { own: new Uint8Array(SNC), P: new CertPolicy(CERT), moves: 0, blocks: 0, done: false, winLine: null, env: [], last: -1, auto: null };
function skWinLine(c) { for (const i of LINES_OF[c]) if (LINES[i].every(x => sk.own[x] === 1)) return LINES[i]; return null; }
function skMaker() {
  const c = sk.P.decide(sk.own); sk.own[c] = 1; sk.moves++; sk.last = c;
  sk.winLine = skWinLine(c); if (sk.winLine) { sk.done = true; stopAuto(); }
  sk.env = sk.P.placedT();
}
function skNew() { stopAuto(); sk.own.fill(0); sk.P.reset(); sk.moves = 0; sk.blocks = 0; sk.done = false; sk.winLine = null; skMaker(); drawSk(); }
function skBlock(c) {
  if (sk.done || sk.own[c]) return;
  sk.own[c] = 2; sk.blocks++; sk.P.observe(c); skMaker(); drawSk();
}
function greedyBlock(own) {
  const sc = new Float64Array(SNC);
  for (const l of LINES) { let m = 0, b = 0; for (const c of l) { if (own[c] === 1) m++; else if (own[c] === 2) b++; } if (b) continue;
    const d = m === 5 ? 1e12 : Math.pow(4, m); for (const c of l) if (!own[c]) sc[c] += d; }
  let best = -1, bv = -1; for (let c = 0; c < SNC; c++) if (!own[c] && sc[c] > bv + 1e-9) { bv = sc[c]; best = c; }
  return best;
}
function stopAuto() { if (sk.auto) { clearInterval(sk.auto); sk.auto = null; document.getElementById('sk-auto').classList.remove('on'); } }
document.getElementById('sk-new').onclick = skNew;
document.getElementById('sk-auto').onclick = () => {
  if (sk.auto) return stopAuto();
  if (sk.done) skNew();
  document.getElementById('sk-auto').classList.add('on');
  sk.auto = setInterval(() => { if (sk.done) return stopAuto(); skBlock(greedyBlock(sk.own)); }, 450);
};
const skCv = document.getElementById('sk-board');
function skCell(e) {
  const r = skCv.getBoundingClientRect(), s = r.width / 17;
  const x = Math.floor((e.clientX - r.left) / s), y = 16 - Math.floor((e.clientY - r.top) / s);
  return (x >= 0 && x < 17 && y >= 0 && y < 17) ? x * 17 + y : -1;
}
skCv.addEventListener('click', e => { const c = skCell(e); if (c >= 0) { stopAuto(); skBlock(c); } });
skCv.addEventListener('mousemove', e => { const c = skCell(e); skCv.style.cursor = c >= 0 && !sk.own[c] && !sk.done ? 'pointer' : 'default'; });
function drawSk() {
  const { g, w } = fit(skCv), s = w / 17;
  const env = new Uint8Array(SNC); for (const c of sk.env) env[c] = 1;
  const win = new Uint8Array(SNC); if (sk.winLine) for (const c of sk.winLine) win[c] = 1;
  for (let x = 0; x < 17; x++) for (let y = 0; y < 17; y++) {
    const c = x * 17 + y, px = x * s, py = (16 - y) * s;
    g.fillStyle = env[c] && !sk.done ? css('--new-soft') : css('--surface-2');
    g.fillRect(px + 1, py + 1, s - 2, s - 2);
    if (sk.own[c] === 1) { g.fillStyle = css('--new'); g.beginPath(); g.arc(px + s / 2, py + s / 2, s * 0.36, 0, 7); g.fill(); }
    if (sk.own[c] === 2) { g.strokeStyle = css('--stone'); g.lineWidth = 2.4; const q = s * 0.26;
      g.beginPath(); g.moveTo(px + s / 2 - q, py + s / 2 - q); g.lineTo(px + s / 2 + q, py + s / 2 + q); g.moveTo(px + s / 2 + q, py + s / 2 - q); g.lineTo(px + s / 2 - q, py + s / 2 + q); g.stroke(); }
    if (win[c]) { g.strokeStyle = css('--ink'); g.lineWidth = 3; g.strokeRect(px + 2.5, py + 2.5, s - 5, s - 5); }
    if (c === sk.last && !sk.done) { g.strokeStyle = css('--ink'); g.lineWidth = 1.5; g.beginPath(); g.arc(px + s / 2, py + s / 2, s * 0.44, 0, 7); g.stroke(); }
  }
  const h = sk.P.height();
  document.getElementById('sk-read').innerHTML = sk.done
    ? `<b>Playbook won</b> with ${sk.moves} moves (the proof's limit is 21). You blocked ${sk.blocks} times.<br>The best possible blocker forces exactly 21. Try "New game".`
    : `Playbook moves: <b>${sk.moves}</b> · your blocks: <b>${sk.blocks}</b><br>Proof's promise: it finishes in at most <b>${h - 1}</b> more moves. Blue tint = region you can never break into.`;
}
// load-ladder data (demos/L1-snaky-ladder/out/ladder.json, clean run)
const SKL = {
  n: [1,2,4,8,16,32,64,128,256,512,1024,2048,4096,8192,16384,32768,65536,131072,262144],
  search: [3.6249,5.4537,13.3976,25.7311,48.003,91.2099,187.253,350.531,1054.69,2455.39,5048.98],
  cert: [1e-4,1e-4,2e-4,3e-4,7e-4,0.0014,0.0021,0.0064,0.0123,0.0274,0.0592,0.17475,0.3906,0.83295,1.7447,3.1278,6.7751,14.1301,28.7391],
  winS: [1,1,.8833,.85,.8,.6,.4333,.4167,.15,.0333,0,0,.0167,0,0,0,0],
  winC: [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1]
};
const skN = document.getElementById('sk-n'), skCh = document.getElementById('sk-chart');
let skHover = null;
function drawSkChart() {
  const { g, w, h } = fit(skCh), i = +skN.value;
  const L = 52, R = 14, T1 = 10, H1 = 190, T2 = 236, H2 = 70, X = k => L + k / 18 * (w - L - R);
  const ly0 = -4.3, ly1 = 4; const Y1 = v => T1 + H1 - (Math.log10(v) - ly0) / (ly1 - ly0) * H1;
  const Y2 = v => T2 + H2 - v * H2;
  g.font = font(11, 400, true); g.textAlign = 'right'; g.fillStyle = css('--ink-3'); g.strokeStyle = css('--grid'); g.lineWidth = 1;
  for (const [v, lab] of [[1e-4,'0.1µs'],[1e-3,'1µs'],[1e-2,'10µs'],[0.1,'100µs'],[1,'1ms'],[10,'10ms'],[100,'100ms'],[1000,'1s']]) {
    g.beginPath(); g.moveTo(L, Y1(v)); g.lineTo(w - R, Y1(v)); g.stroke(); g.fillText(lab, L - 5, Y1(v) + 4); }
  for (const v of [0, .5, 1]) { g.beginPath(); g.moveTo(L, Y2(v)); g.lineTo(w - R, Y2(v)); g.stroke(); g.fillText((v * 100) + '%', L - 5, Y2(v) + 4); }
  g.textAlign = 'center';
  for (let k = 0; k <= 18; k += 3) g.fillText(fmtN(SKL.n[k]), X(k), T2 + H2 + 16);
  g.textAlign = 'left'; g.fillStyle = css('--ink-2'); g.font = font(12, 600);
  g.fillText('compute per frame (log)', L + 4, T1 + 12); g.fillText('wins vs a search blocker, frame shared by all games', L + 4, T2 - 8);
  // budget line
  g.setLineDash([4, 4]); g.strokeStyle = css('--ink'); g.beginPath(); g.moveTo(L, Y1(16.7)); g.lineTo(w - R, Y1(16.7)); g.stroke(); g.setLineDash([]);
  g.font = font(11); g.fillStyle = css('--ink'); g.textAlign = 'right'; g.fillText('16.7 ms = 60 fps', w - R - 2, Y1(16.7) - 5);
  // cursor
  g.strokeStyle = css('--ink-3'); g.beginPath(); g.moveTo(X(i), T1); g.lineTo(X(i), T2 + H2); g.stroke();
  const line = (arr, Y, col) => { g.strokeStyle = col; g.lineWidth = 2; g.beginPath(); arr.forEach((v, k) => { if (v == null) return; k && arr[k - 1] != null ? g.lineTo(X(k), Y(v)) : g.moveTo(X(k), Y(v)); }); g.stroke();
    arr.forEach((v, k) => { if (v == null) return; g.fillStyle = col; g.beginPath(); g.arc(X(k), Y(v), k === i ? 5 : 3, 0, 7); g.fill(); if (k === i) { g.strokeStyle = css('--surface'); g.lineWidth = 2; g.stroke(); g.lineWidth = 2; g.strokeStyle = col; } }); };
  line(SKL.search, Y1, css('--old')); line(SKL.cert, Y1, css('--new'));
  line(SKL.winS, Y2, css('--old')); line(SKL.winC, Y2, css('--new'));
  const n = SKL.n[i], s = SKL.search[i], c = SKL.cert[i];
  document.getElementById('sk-nv').textContent = n.toLocaleString();
  skCh._read = `<b>${n.toLocaleString()}</b> games: search ${s != null ? fmtMs(s) : 'not timed (over 5 s per frame from 1,024)'} · playbook ${fmtMs(c)}` +
    (SKL.winS[i] != null ? `<br>wins with the frame shared: search ${Math.round(SKL.winS[i] * 100)}% · playbook ${Math.round(SKL.winC[i] * 100)}%` : '<br>(win rates measured up to 65,536 games)');
  document.getElementById('sk-bench').innerHTML = skCh._read + (skBenchText ? '<br>' + skBenchText : '');
}
let skBenchText = '';
skN.oninput = drawSkChart;
skCh.addEventListener('mousemove', e => { const r = skCh.getBoundingClientRect(), k = Math.round((e.clientX - r.left - 52) / (r.width - 66) * 18);
  if (k >= 0 && k <= 18) { const s = SKL.search[k], c = SKL.cert[k];
    showTip(e, `${SKL.n[k].toLocaleString()} games<br>search: ${fmtMs(s)}${s ? ' · ' + (s > 16.7 ? 'over' : 'within') + ' budget' : ''}<br>playbook: ${fmtMs(c)}${k < 2 ? ' (timer floor)' : ''}` +
      (SKL.winS[k] != null ? `<br>wins: ${Math.round(SKL.winS[k] * 100)}% vs ${Math.round(SKL.winC[k] * 100)}%` : '')); } else hideTip(); });
skCh.addEventListener('mouseleave', hideTip);
skCh.addEventListener('click', e => { const r = skCh.getBoundingClientRect(), k = Math.round((e.clientX - r.left - 52) / (r.width - 66) * 18); if (k >= 0 && k <= 18) { skN.value = k; drawSkChart(); } });
function skBench() { // 1,000 games vs a random-local blocker; time only the playbook's work by replaying recorded replies
  let s = 7; const rnd = n => { s = (Math.imul(s, 1103515245) + 12345) >>> 0; return (s >>> 8) % n; };
  const games = [], own = new Uint8Array(SNC); let wins = 0, maxMoves = 0;
  for (let gi = 0; gi < 1000; gi++) {
    own.fill(0); const P = new CertPolicy(CERT), replies = []; let moves = 0;
    for (;;) { const c = P.decide(own); own[c] = 1; moves++;
      let won = false; for (const li of LINES_OF[c]) if (LINES[li].every(x => own[x] === 1)) { won = true; break; }
      if (won) { wins++; break; } if (moves > 40) break;
      const cand = []; for (let q = 0; q < SNC; q++) { if (own[q]) continue; const x = (q / 17) | 0, y = q % 17; let near = false;
        for (let dx = -2; dx <= 2 && !near; dx++) for (let dy = -2; dy <= 2 && !near; dy++) { const X = x + dx, Y = y + dy; if (X >= 0 && Y >= 0 && X < 17 && Y < 17 && own[X * 17 + Y] === 1) near = true; }
        if (near) cand.push(q); }
      const b = cand[rnd(cand.length)]; own[b] = 2; replies.push(b); P.observe(b); }
    maxMoves = Math.max(maxMoves, moves); games.push(replies);
  }
  let dec = 0; const t0 = performance.now();
  for (let rep = 0; rep < 5; rep++) for (const replies of games) { own.fill(0); const P = new CertPolicy(CERT);
    for (const b of replies) { own[P.decide(own)] = 1; own[b] = 2; P.observe(b); dec++; } own[P.decide(own)] = 1; dec++; }
  const us = (performance.now() - t0) * 1000 / dec;
  skBenchText = `In your browser: ${wins}/1000 wins vs a random blocker, at most ${maxMoves} moves, <b>${us.toFixed(2)} µs</b> per playbook move.`;
  drawSkChart();
}

// ===================================================================== 2 ECONOMY
const ec = { active: EC_RX.map(() => true), k: EC_RX.map(r => r[2]), runs: [], hist: [], t: 0 };
const EC_POS = { // complex name -> [x, y] in unit layout; one triangle per recipe group
  'F+P': [.17, .1], '2P': [.29, .78], 'P': [.05, .78], 'O+W': [.5, .1], 'T': [.62, .78], '2O': [.38, .78],
  'T+F+P': [.83, .1], 'O+W+P': [.95, .78], 'T+2P': [.71, .78] };
const EC_GROUP = ['village', 'forge', 'workshop'];
function ecReset() {
  ec.runs = EC_STARTS.map(s => ({ y: s.map(Math.log), t: 0, stopped: false }));
  ec.hist = EC_STARTS.map(s => [[0, s.map(v => Math.log10(v))]]); ec.t = 0;
}
function ecArrowGeom(w, h, k) {
  const r = EC_RX[k], a = EC_POS[ecName(r[0])], b = EC_POS[ecName(r[1])];
  const P = p => [16 + p[0] * (w - 32), 22 + p[1] * (h - 96)];
  const A = P(a), B = P(b), mx = (A[0] + B[0]) / 2, my = (A[1] + B[1]) / 2, dx = B[0] - A[0], dy = B[1] - A[1], L = Math.hypot(dx, dy);
  const nx = -dy / L, ny = dx / L, bend = 16, cx = mx + nx * bend, cy = my + ny * bend;
  return { A, B, cx, cy, mid: [(A[0] + 2 * cx + B[0]) / 4, (A[1] + 2 * cy + B[1]) / 4], L };
}
const ecG = document.getElementById('ec-graph');
function drawEcGraph() {
  const { g, w, h } = fit(ecG), ok = ecLint(ec.active);
  g.font = font(12, 600); g.textAlign = 'center'; g.fillStyle = css('--ink-3');
  EC_GROUP.forEach((nm, i) => g.fillText(nm, 16 + [.17, .5, .83][i] * (w - 32), h - 34));
  EC_RX.forEach((r, k) => {
    const { A, B, cx, cy, mid } = ecArrowGeom(w, h, k), on = ec.active[k], bad = on && !ok[k];
    g.strokeStyle = !on ? css('--line') : bad ? css('--old') : css('--ink-2'); g.lineWidth = bad ? 2.6 : 1.8; g.setLineDash(on ? [] : [4, 4]);
    // shorten ends to leave room for labels
    const t0 = 0.2, t1 = 0.8, q = t => [(1 - t) * (1 - t) * A[0] + 2 * (1 - t) * t * cx + t * t * B[0], (1 - t) * (1 - t) * A[1] + 2 * (1 - t) * t * cy + t * t * B[1]];
    g.beginPath(); for (let t = t0; t <= t1 + 1e-9; t += 0.02) { const p = q(t); t === t0 ? g.moveTo(p[0], p[1]) : g.lineTo(p[0], p[1]); } g.stroke(); g.setLineDash([]);
    const p1 = q(t1), p0 = q(t1 - 0.04), ang = Math.atan2(p1[1] - p0[1], p1[0] - p0[0]);
    if (on) { g.fillStyle = g.strokeStyle; g.beginPath(); g.moveTo(p1[0], p1[1]); g.lineTo(p1[0] - 9 * Math.cos(ang - .45), p1[1] - 9 * Math.sin(ang - .45)); g.lineTo(p1[0] - 9 * Math.cos(ang + .45), p1[1] - 9 * Math.sin(ang + .45)); g.fill(); }
    else { g.fillStyle = css('--ink-3'); g.font = font(13, 700); g.fillText('×', mid[0], mid[1] + 4); }
  });
  g.font = font(13, 700, true);
  for (const [nm, p] of Object.entries(EC_POS)) { const x = 16 + p[0] * (w - 32), y = 22 + p[1] * (h - 96);
    const tw = g.measureText(nm).width + 12; g.fillStyle = css('--surface-2'); g.strokeStyle = css('--line'); g.lineWidth = 1;
    g.beginPath(); g.roundRect(x - tw / 2, y - 11, tw, 22, 6); g.fill(); g.stroke(); g.fillStyle = css('--ink'); g.fillText(nm, x, y + 4.5); }
  g.font = font(11); g.fillStyle = css('--ink-3'); g.textAlign = 'left';
  g.fillText('O ore · W wood · T tools · F food · P workers · "2P" = two workers', 8, h - 10);
  const badNames = EC_RX.map((r, k) => ec.active[k] && !ok[k] ? `${ecName(r[0])} → ${ecName(r[1])}` : null).filter(Boolean);
  const removed = ec.active.filter(v => !v).length;
  document.getElementById('ec-lint').innerHTML = badNames.length
    ? `<b>✗ Lint fails.</b> No way back for: ${badNames.join(', ')}.<br>No survival guarantee: goods may die out or pile up.`
    : `<b>✓ Lint passes</b>${removed ? ` (${removed} recipe${removed > 1 ? 's' : ''} removed, every remaining loop still closes)` : ''}.<br>Proven: no good dies out or runs away, for any positive rates.`;
}
ecG.addEventListener('click', e => {
  const r = ecG.getBoundingClientRect(), x = e.clientX - r.left, y = e.clientY - r.top;
  let best = -1, bd = 22; EC_RX.forEach((_, k) => { const m = ecArrowGeom(r.width, r.height, k).mid, d = Math.hypot(m[0] - x, m[1] - y); if (d < bd) { bd = d; best = k; } });
  if (best >= 0) { ec.active[best] = !ec.active[best]; ecReset(); drawEcGraph(); }
});
ecG.addEventListener('mousemove', e => {
  const r = ecG.getBoundingClientRect(), x = e.clientX - r.left, y = e.clientY - r.top;
  let best = -1, bd = 22; EC_RX.forEach((_, k) => { const m = ecArrowGeom(r.width, r.height, k).mid, d = Math.hypot(m[0] - x, m[1] - y); if (d < bd) { bd = d; best = k; } });
  ecG.style.cursor = best >= 0 ? 'pointer' : 'default';
  if (best >= 0) { const rx = EC_RX[best]; showTip(e, `${ecName(rx[0])} → ${ecName(rx[1])}<br>rate ${ec.k[best].toFixed(2)} · ${ec.active[best] ? 'click to delete' : 'click to restore'}`); } else hideTip();
});
ecG.addEventListener('mouseleave', hideTip);
document.getElementById('ec-full').onclick = () => { ec.active = EC_RX.map(() => true); ecReset(); drawEcGraph(); };
document.getElementById('ec-broken').onclick = () => { ec.active = EC_RX.map((_, i) => i !== 1 && i !== 6); ecReset(); drawEcGraph(); };
document.getElementById('ec-roll').onclick = () => { ec.k = EC_RX.map(r => r[2] * Math.exp((Math.random() * 2 - 1) * Math.log(2))); ecReset(); drawEcGraph(); };
const EC_TEND = 120, ecC = document.getElementById('ec-chart');
function stepEc() {
  if (ec.t >= EC_TEND) return;
  ec.t = Math.min(EC_TEND, ec.t + 0.5);
  ec.runs.forEach((r, i) => { ecStep(r, ec.active, ec.k, ec.t); ec.hist[i].push([r.t, r.y.map(v => v / Math.LN10)]); });
}
function drawEcChart() {
  const { g, w, h } = fit(ecC), L = 44, R = 10, T = 10, B = 26, X = t => L + t / EC_TEND * (w - L - R), Y = v => T + (12 - Math.max(-12, Math.min(12, v))) / 24 * (h - T - B);
  g.font = font(11, 400, true); g.fillStyle = css('--ink-3'); g.strokeStyle = css('--grid'); g.lineWidth = 1; g.textAlign = 'right';
  for (let v = -12; v <= 12; v += 6) { g.beginPath(); g.moveTo(L, Y(v)); g.lineTo(w - R, Y(v)); g.stroke(); g.fillText(v === 0 ? '1' : '1e' + v, L - 5, Y(v) + 4); }
  g.textAlign = 'center'; for (let t = 0; t <= EC_TEND; t += 30) g.fillText('t=' + t, X(t), h - 8);
  g.fillStyle = css('--old-soft'); g.fillRect(L, Y(12), w - L - R, Y(6) - Y(12)); g.fillRect(L, Y(-6), w - L - R, Y(-12) - Y(-6));
  g.fillStyle = css('--ink-3'); g.textAlign = 'left'; g.font = font(11); g.fillText('runaway zone (> 1e6)', L + 6, Y(12) + 14); g.fillText('dead zone (< 1e-6)', L + 6, Y(-12) - 6);
  let dead = 0, run = 0, lo = Infinity, hi = 0;
  ec.hist.forEach(H => { for (let sp = 0; sp < 5; sp++) {
    const last = H[H.length - 1][1][sp]; const bad = last < -6 || last > 6;
    if (ec.t >= EC_TEND) { if (last < -6) dead++; if (last > 6) run++; lo = Math.min(lo, last); hi = Math.max(hi, last); }
    g.strokeStyle = bad ? css('--old') : css('--new'); g.globalAlpha = bad ? 0.9 : 0.55; g.lineWidth = 1.5; g.beginPath();
    H.forEach(([t, ys], k) => k ? g.lineTo(X(t), Y(ys[sp])) : g.moveTo(X(t), Y(ys[sp]))); g.stroke(); } });
  g.globalAlpha = 1;
  document.getElementById('ec-read').innerHTML = ec.t < EC_TEND ? `simulating… t = ${ec.t.toFixed(0)} / ${EC_TEND} (30 lines: 6 towns × 5 goods)`
    : `After t = ${EC_TEND}: <b>${dead}</b> of 30 goods died out, <b>${run}</b> ran away.<br>Range of final amounts: ${Math.pow(10, lo).toExponential(1)} to ${Math.pow(10, hi).toExponential(1)}.` +
      (ec.runs.some(r => r.stopped) ? '<br>A town\'s lines end where one of its goods left the chart (the simulation stops there).' : '');
}

// ===================================================================== 3 FRACTAL CLOCK
const FK_NS = [16, 64, 256], FK_SAMPLES = 300;
const fkd = { Ns: FK_NS.map(() => []), seeds: FK_NS.map(() => []), ready: false, p: 1, lanes: [], t0: 0, traceMs: FK_NS.map(() => 0) };
const med = a => { const s = [...a].sort((x, y) => x - y); return s[s.length >> 1]; };
function fkPrecompute(li = 0, s = 0) { // chunked so the page stays responsive
  const t0 = performance.now();
  while (performance.now() - t0 < 12 && li < FK_NS.length) {
    const n = FK_NS[li], seed = n * 100003 + s, a = performance.now(), r = fkTrace(n, seed, false);
    fkd.traceMs[li] += performance.now() - a;
    if (r.N > 0) { fkd.Ns[li].push(r.N); fkd.seeds[li].push(seed); }
    if (++s >= FK_SAMPLES) { fkd.traceMs[li] /= FK_SAMPLES; li++; s = 0; }
  }
  if (li < FK_NS.length) setTimeout(() => fkPrecompute(li, s), 0); else { fkd.ready = true; fkd.med = fkd.Ns.map(med); fkStart(); drawFkBars(); }
}
const fkScale = () => 3 / (fkd.med[1] * Math.pow(1 / 64, fkd.p)); // one constant: 64x64 median = 3 s
function fkStart() {
  fkd.lanes = FK_NS.map((n, li) => { // a typical front: random pick from the middle 40% of this grid's distribution
    const order = fkd.Ns[li].map((N, k) => [N, k]).sort((a, b) => a[0] - b[0]), lo = Math.floor(order.length * .3), pick = order[lo + Math.floor(Math.random() * order.length * .4)][1];
    const r = fkTrace(n, fkd.seeds[li][pick], true); return { n, N: r.N, path: r.path };
  });
  fkd.t0 = performance.now();
}
const fkP = document.getElementById('fk-p');
function setP(v) { fkd.p = v; fkP.value = Math.round(v * 100); document.getElementById('fk-pv').textContent = v.toFixed(2); if (fkd.ready) { fkStart(); drawFkBars(); } }
fkP.oninput = () => setP(fkP.value / 100);
document.getElementById('fk-p1').onclick = () => setP(1);
document.getElementById('fk-p175').onclick = () => setP(1.75);
const fkL = document.getElementById('fk-lanes'), fkB = document.getElementById('fk-bars');
function drawFkLanes() {
  const { g, w, h } = fit(fkL);
  if (!fkd.ready) { g.fillStyle = css('--ink-3'); g.font = font(14); g.fillText('sampling 900 fronts…', 12, 24); return; }
  const gap = 14, side = Math.min((w - 2 * gap) / 3, h - 44), el = (performance.now() - fkd.t0) / 1000, C = fkScale();
  let allDone = true, maxDur = 0;
  fkd.lanes.forEach((ln, li) => {
    const x0 = li * (side + gap) + (w - 3 * side - 2 * gap) / 2, y0 = 30, dur = C * ln.N * Math.pow(1 / ln.n, fkd.p);
    maxDur = Math.max(maxDur, dur);
    const frac = Math.min(1, el / dur); if (frac < 1) allDone = false;
    g.fillStyle = css('--surface-2'); g.fillRect(x0, y0, side, side);
    g.font = font(13, 600); g.fillStyle = css('--ink'); g.textAlign = 'left'; g.fillText(`${ln.n}×${ln.n} grid`, x0, 18);
    g.font = font(12, 400, true); g.textAlign = 'right'; g.fillStyle = frac >= 1 ? css('--ink') : css('--ink-2');
    g.fillText(frac >= 1 ? `crossed in ${dur.toFixed(1)} s` : `${el.toFixed(1)} s`, x0 + side, 18);
    const pts = ln.path, nPts = pts.length / 2, upto = Math.max(1, Math.floor(frac * (nPts - 1)));
    g.strokeStyle = css('--new'); g.lineWidth = ln.n >= 256 ? 0.8 : ln.n >= 64 ? 1.2 : 2; g.lineJoin = 'round'; g.beginPath();
    for (let k = 0; k <= upto; k++) { const px = x0 + pts[2 * k] * side, py = y0 + (1 - pts[2 * k + 1]) * side; k ? g.lineTo(px, py) : g.moveTo(px, py); }
    g.stroke();
    if (frac < 1) { g.fillStyle = css('--old'); g.beginPath(); g.arc(x0 + pts[2 * upto] * side, y0 + (1 - pts[2 * upto + 1]) * side, 4.5, 0, 7); g.fill(); }
    g.fillStyle = css('--ink-3'); g.font = font(11); g.textAlign = 'left'; g.fillText(`${ln.N.toLocaleString()} steps`, x0, y0 + side + 14);
  });
  if (allDone && el > maxDur + 1.2) fkStart();
}
function drawFkBars() {
  const { g, w, h } = fit(fkB); if (!fkd.ready) return;
  const C = fkScale(), durs = fkd.med.map((N, li) => C * N * Math.pow(1 / FK_NS[li], fkd.p)), L = 92, R = 70, top = 14, bh = 30, mx = Math.max(12, ...durs) * 1.05;
  g.font = font(11, 400, true); g.fillStyle = css('--ink-3'); g.strokeStyle = css('--grid'); g.textAlign = 'center';
  for (let s = 0; s <= mx; s += mx > 20 ? 5 : 2) { const x = L + s / mx * (w - L - R); g.beginPath(); g.moveTo(x, top - 4); g.lineTo(x, h - 22); g.stroke(); g.fillText(s + ' s', x, h - 8); }
  durs.forEach((d, li) => { const y = top + li * (bh + 12);
    g.fillStyle = css('--new'); g.beginPath(); g.roundRect(L, y, Math.max(2, d / mx * (w - L - R)), bh, [0, 4, 4, 0]); g.fill();
    g.fillStyle = css('--ink'); g.textAlign = 'right'; g.font = font(13, 600); g.fillText(`${FK_NS[li]}×${FK_NS[li]}`, L - 8, y + bh / 2 + 5);
    g.textAlign = 'left'; g.font = font(12, 400, true); g.fillText(d.toFixed(2) + ' s', L + d / mx * (w - L - R) + 6, y + bh / 2 + 4); });
  fkB._durs = durs;
  const ratio = durs[2] / durs[0];
  document.getElementById('fk-read').innerHTML = `p = ${fkd.p.toFixed(2)}: the 256×256 front takes <b>${ratio.toFixed(2)}×</b> as long as the 16×16 one` +
    (Math.abs(fkd.p - 1.75) < 0.03 ? ', so the grids agree.' : fkd.p < 1.75 ? ', so finer grids run slow.' : ', so finer grids run fast.') +
    `<br>Cost to trace one front here: 16×16 ${fmtMs(fkd.traceMs[0])} · 64×64 ${fmtMs(fkd.traceMs[1])} · 256×256 ${fmtMs(fkd.traceMs[2])}`;
}
fkB.addEventListener('mousemove', e => { if (!fkB._durs) return; const r = fkB.getBoundingClientRect(), li = Math.floor((e.clientY - r.top - 14 + 6) / 42);
  if (li >= 0 && li < 3) showTip(e, `${FK_NS[li]}×${FK_NS[li]} grid<br>median ${fkd.med[li].toLocaleString()} steps<br>median crossing ${fkB._durs[li].toFixed(2)} s`); else hideTip(); });
fkB.addEventListener('mouseleave', hideTip);

// ===================================================================== 4 DUNGEON
const DT = dgTemplate();
const dg = { show: 'q', latest: { q: DT, c: DT }, hist: { q: new Float64Array(29), c: new Float64Array(29) }, cnt: { q: 0, c: 0 }, ms: { q: 0, c: 0 }, msn: { q: 0, c: 0 }, ref: null, refMean: 0, R: dgRng(20261007) };
function dgRef() { // long Curveball run: the fair (uniform) distribution of "template doors kept"
  const a = Uint32Array.from(DT), R = dgRng(77), h = new Float64Array(29); let n = 0, s = 0;
  for (let k = 0; k < 100000; k++) dgCurveball(a, R);
  for (let i = 0; i < 30000; i++) { for (let k = 0; k < 40; k++) dgCurveball(a, R); const kept = dgKept(a, DT); h[kept]++; s += kept; n++; }
  dg.ref = h.map(v => v / n); dg.refMean = s / n;
}
function dgTick() {
  let t0 = performance.now();
  for (let i = 0; i < 300; i++) { const a = dgQuick(DT, dg.R); dg.hist.q[dgKept(a, DT)]++; dg.cnt.q++; dg.latest.q = a; }
  dg.ms.q += performance.now() - t0; dg.msn.q += 300;
  t0 = performance.now(); let k = 0;
  while (performance.now() - t0 < 5 || k === 0) { const a = dgCertified(DT, dg.R); dg.hist.c[dgKept(a, DT)]++; dg.cnt.c++; dg.latest.c = a; k++; }
  dg.ms.c += performance.now() - t0; dg.msn.c += k;
}
const dgMap = document.getElementById('dg-map'), dgH = document.getElementById('dg-hist');
document.getElementById('dg-q').onclick = () => { dg.show = 'q'; document.getElementById('dg-q').classList.add('on'); document.getElementById('dg-c').classList.remove('on'); };
document.getElementById('dg-c').onclick = () => { dg.show = 'c'; document.getElementById('dg-c').classList.add('on'); document.getElementById('dg-q').classList.remove('on'); };
function drawDgMap() {
  const { g, w, h } = fit(dgMap), a = dg.latest[dg.show], P = i => [40 + (i % 5) / 4 * (w - 80), 30 + Math.floor(i / 5) / 3 * (h - 70)];
  for (let u = 0; u < DG_N; u++) for (let v = u + 1; v < DG_N; v++) if ((a[u] >> v) & 1) {
    const kept = (DT[u] >> v) & 1, A = P(u), B = P(v), adj = Math.abs((u % 5) - (v % 5)) + Math.abs(((u / 5) | 0) - ((v / 5) | 0)) === 1;
    g.strokeStyle = kept ? css('--ink-3') : css('--new'); g.lineWidth = kept ? 2.5 : 2; g.beginPath(); g.moveTo(A[0], A[1]);
    if (adj) g.lineTo(B[0], B[1]); else { const mx = (A[0] + B[0]) / 2, my = (A[1] + B[1]) / 2, dx = B[0] - A[0], dy = B[1] - A[1], L = Math.hypot(dx, dy); g.quadraticCurveTo(mx - dy / L * 18, my + dx / L * 18, B[0], B[1]); }
    g.stroke(); }
  for (let i = 0; i < DG_N; i++) { const [x, y] = P(i), d = popc(DT[i]);
    g.fillStyle = css('--surface-2'); g.strokeStyle = css('--ink-2'); g.lineWidth = 1.5; g.beginPath(); g.roundRect(x - 15, y - 12, 30, 24, 5); g.fill(); g.stroke();
    g.fillStyle = css('--ink'); for (let k = 0; k < d; k++) { g.beginPath(); g.arc(x - (d - 1) * 2.6 + k * 5.2, y, 1.9, 0, 7); g.fill(); } }
  const kept = dgKept(a, DT);
  g.font = font(12, 400, true); g.fillStyle = css('--ink-2'); g.textAlign = 'left';
  g.fillText(`${dg.show === 'q' ? 'quick shuffle' : 'certified shuffle'} · kept ${kept} of 28 template doors · dots = door count`, 8, h - 8);
}
function drawDgHist() {
  const { g, w, h } = fit(dgH), L = 40, R = 10, T = 10, B = 26, xmax = 18;
  if (!dg.ref) return;
  const pq = Array.from(dg.hist.q, v => v / Math.max(1, dg.cnt.q)), pc = Array.from(dg.hist.c, v => v / Math.max(1, dg.cnt.c));
  const ymax = Math.max(0.3, ...pq, ...pc, ...dg.ref) * 1.08, X = k => L + (k + .5) / (xmax + 1) * (w - L - R), Y = v => T + (1 - v / ymax) * (h - T - B);
  g.font = font(11, 400, true); g.fillStyle = css('--ink-3'); g.strokeStyle = css('--grid'); g.textAlign = 'right';
  for (let v = 0; v <= ymax; v += 0.1) { g.beginPath(); g.moveTo(L, Y(v)); g.lineTo(w - R, Y(v)); g.stroke(); g.fillText(Math.round(v * 100) + '%', L - 5, Y(v) + 4); }
  g.textAlign = 'center'; for (let k = 0; k <= xmax; k += 2) g.fillText(k, X(k), h - 8);
  const bw = (w - L - R) / (xmax + 1) * 0.34;
  for (let k = 0; k <= xmax; k++) {
    g.fillStyle = css('--old'); g.beginPath(); g.roundRect(X(k) - bw - 1, Y(pq[k]), bw, Y(0) - Y(pq[k]), [3, 3, 0, 0]); g.fill();
    g.fillStyle = css('--new'); g.beginPath(); g.roundRect(X(k) + 1, Y(pc[k]), bw, Y(0) - Y(pc[k]), [3, 3, 0, 0]); g.fill(); }
  g.strokeStyle = css('--ink-3'); g.lineWidth = 2; g.setLineDash([5, 4]); g.beginPath();
  for (let k = 0; k <= xmax; k++) k ? g.lineTo(X(k), Y(dg.ref[k])) : g.moveTo(X(k), Y(dg.ref[k])); g.stroke(); g.setLineDash([]);
  const mean = p => p.reduce((s, v, k) => s + v * k, 0), tv = p => 0.5 * p.reduce((s, v, k) => s + Math.abs(v - dg.ref[k]), 0);
  dgH._p = { pq, pc };
  document.getElementById('dg-read').innerHTML =
    `quick: <b>${mean(pq).toFixed(2)}</b> doors kept on average · ${fmtMs(dg.ms.q / Math.max(1, dg.msn.q))} per layout · ${dg.cnt.q.toLocaleString()} layouts<br>` +
    `certified: <b>${mean(pc).toFixed(2)}</b> · ${fmtMs(dg.ms.c / Math.max(1, dg.msn.c))} per layout · ${dg.cnt.c.toLocaleString()} layouts · perfectly fair: <b>${dg.refMean.toFixed(2)}</b><br>` +
    `distance from fair: quick ${tv(pq).toFixed(3)} · certified ${tv(pc).toFixed(3)} (0 = identical; small samples read a little high)`;
}
dgH.addEventListener('mousemove', e => { if (!dgH._p) return; const r = dgH.getBoundingClientRect(), k = Math.floor((e.clientX - r.left - 40) / (r.width - 50) * 19);
  if (k >= 0 && k <= 18) showTip(e, `${k} template doors kept<br>quick ${(dgH._p.pq[k] * 100).toFixed(1)}% · certified ${(dgH._p.pc[k] * 100).toFixed(1)}%<br>fair ${(dg.ref[k] * 100).toFixed(1)}%`); else hideTip(); });
dgH.addEventListener('mouseleave', hideTip);

// ===================================================================== 5 BLOB (2D stand-in)
const BN = 128, BNN = BN * BN;
const bl = { V: 0.22, u: new Float32Array(BNN), it: 0, tmp: new Float32Array(BNN), tmp2: new Float32Array(BNN), formula: new Uint8Array(BNN), sorted: new Float32Array(BNN) };
function boxBlur(src, dst, r, horiz) { // periodic box blur
  const k = 1 / (2 * r + 1);
  for (let a = 0; a < BN; a++) { let s = 0;
    for (let d = -r; d <= r; d++) { const b = (d + BN) % BN; s += horiz ? src[a * BN + b] : src[b * BN + a]; }
    for (let b = 0; b < BN; b++) { horiz ? dst[a * BN + b] = s * k : dst[b * BN + a] = s * k;
      const add = (b + r + 1) % BN, rem = (b - r + BN) % BN; s += horiz ? src[a * BN + add] - src[a * BN + rem] : src[add * BN + a] - src[rem * BN + a]; } }
}
function gauss(u, r) { for (let p = 0; p < 3; p++) { boxBlur(u, bl.tmp2, r, true); boxBlur(bl.tmp2, u, r, false); } }
function keepTop(field, out) { // indicator of the k largest cells, k = V * BNN
  const k = Math.round(bl.V * BNN); bl.sorted.set(field); bl.sorted.sort(); const thr = bl.sorted[BNN - k];
  let c = 0; for (let i = 0; i < BNN; i++) { const on = field[i] > thr || (field[i] === thr && c < k); out[i] = on ? 1 : 0; if (on) c++; }
}
function blSeed() { for (let i = 0; i < BNN; i++) bl.tmp[i] = Math.random(); gauss(bl.tmp, 4); keepTop(bl.tmp, bl.u); bl.it = 0; }
function blFormula() { // disk, band, or their complements; exact cell count by ranking a signed distance
  const V = bl.V, f = bl.tmp, c = BN / 2;
  for (let y = 0; y < BN; y++) for (let x = 0; x < BN; x++) {
    const dx = Math.min(Math.abs(x + .5 - c), BN - Math.abs(x + .5 - c)), dy = Math.min(Math.abs(y + .5 - c), BN - Math.abs(y + .5 - c));
    const disk = -Math.hypot(dx, dy), band = -dy + 1e-3 * Math.random();
    f[y * BN + x] = V < 1 / Math.PI ? disk : V <= 1 - 1 / Math.PI ? band : -disk; }
  const u = new Float32Array(BNN); keepTop(f, u); bl.formula = u;
}
function blStep() { gauss(bl.u, 3); const out = new Float32Array(BNN); keepTop(bl.u, out); bl.u = out; bl.it++; }
function perimeter(ind) { // marching-squares length of the 0.5 contour of a lightly smoothed indicator, unit-square units
  const s = Float32Array.from(ind); boxBlur(s, bl.tmp2, 1, true); boxBlur(bl.tmp2, s, 1, false);
  let len = 0; const at = (x, y) => s[((y + BN) % BN) * BN + ((x + BN) % BN)];
  for (let y = 0; y < BN; y++) for (let x = 0; x < BN; x++) {
    const a = at(x, y) - .5, b = at(x + 1, y) - .5, c = at(x + 1, y + 1) - .5, d = at(x, y + 1) - .5, pts = [];
    const e = (p, q, px, py, qx, qy) => { if ((p > 0) !== (q > 0)) { const t = p / (p - q); pts.push([px + t * (qx - px), py + t * (qy - py)]); } };
    e(a, b, 0, 0, 1, 0); e(b, c, 1, 0, 1, 1); e(c, d, 1, 1, 0, 1); e(d, a, 0, 1, 0, 0);
    for (let k = 0; k + 1 < pts.length; k += 2) len += Math.hypot(pts[k][0] - pts[k + 1][0], pts[k][1] - pts[k + 1][1]); }
  return len / BN;
}
const bestPerim2D = V => { const m = Math.min(V, 1 - V); return Math.min(2 * Math.sqrt(Math.PI * m), 2); };
const blV = document.getElementById('bl-v');
blV.oninput = () => { bl.V = blV.value / 100; document.getElementById('bl-vv').textContent = Math.round(bl.V * 100) + '%'; blFormula(); bl.it = 0; bl.pF = perimeter(bl.formula); drawBlChart(); };
document.getElementById('bl-seed').onclick = () => blSeed();
const blS = document.getElementById('bl-sim'), blC = document.getElementById('bl-chart');
let blImg = null;
function drawBl() {
  const { g, w, h } = fit(blS), side = Math.min((w - 16) / 2, h - 26);
  if (!blImg) blImg = new ImageData(BN, BN);
  const off = drawBl.off || (drawBl.off = document.createElement('canvas')); off.width = BN; off.height = BN;
  const col = css('--new'), r = parseInt(col.slice(1, 3), 16), gg = parseInt(col.slice(3, 5), 16), b = parseInt(col.slice(5, 7), 16);
  const bg = css('--surface-2'), br = parseInt(bg.slice(1, 3), 16), bgg = parseInt(bg.slice(3, 5), 16), bb = parseInt(bg.slice(5, 7), 16);
  [[bl.u, 0, 'melting solver'], [bl.formula, side + 16, 'drawn answer']].forEach(([ind, x0, label]) => {
    for (let i = 0; i < BNN; i++) { const on = ind[i] > .5, o = i * 4; blImg.data[o] = on ? r : br; blImg.data[o + 1] = on ? gg : bgg; blImg.data[o + 2] = on ? b : bb; blImg.data[o + 3] = 255; }
    off.getContext('2d').putImageData(blImg, 0, 0); g.imageSmoothingEnabled = false; g.drawImage(off, x0, 22, side, side);
    g.font = font(13, 600); g.fillStyle = css('--ink'); g.textAlign = 'left'; g.fillText(label, x0, 15); });
  g.font = font(12, 400, true); g.fillStyle = css('--ink-2'); g.textAlign = 'right'; g.fillText(`iteration ${bl.it}`, side, 15);
  if (bl.it % 3 === 0) bl.pS = perimeter(bl.u);
  const best = bestPerim2D(bl.V), shape = bl.V < 1 / Math.PI ? 'disk' : bl.V <= 1 - 1 / Math.PI ? 'band' : 'disk-shaped hole';
  document.getElementById('bl-read').innerHTML = `best possible edge length ${best.toFixed(3)} (${shape})<br>solver: <b>${bl.pS.toFixed(3)}</b> (${((bl.pS / best - 1) * 100).toFixed(1)}% over) · drawn answer: <b>${bl.pF.toFixed(3)}</b> (${((bl.pF / best - 1) * 100).toFixed(1)}%; the leftover is pixel measuring error)`;
}
function drawBlChart() {
  const { g, w, h } = fit(blC), L = 40, R = 12, T = 12, B = 30, X = v => L + v * (w - L - R), ymax = 3, Y = a => T + (1 - a / ymax) * (h - T - B);
  const ball = V => Math.cbrt(36 * Math.PI) * Math.pow(Math.min(V, 1 - V), 2 / 3), tube = V => 2 * Math.sqrt(Math.PI * Math.min(V, 1 - V)), slab = () => 2;
  g.font = font(11, 400, true); g.fillStyle = css('--ink-3'); g.strokeStyle = css('--grid'); g.lineWidth = 1; g.textAlign = 'right';
  for (let a = 0; a <= 3; a++) { g.beginPath(); g.moveTo(L, Y(a)); g.lineTo(w - R, Y(a)); g.stroke(); g.fillText(a, L - 6, Y(a) + 4); }
  g.textAlign = 'center'; for (const v of [0, .25, .5, .75, 1]) g.fillText(Math.round(v * 100) + '%', X(v), h - 12);
  const curve = (f, col, lw, dash) => { g.strokeStyle = col; g.lineWidth = lw; g.setLineDash(dash || []); g.beginPath(); for (let k = 0; k <= 400; k++) { const v = .002 + k / 400 * .996, y = Math.min(f(v), ymax); k ? g.lineTo(X(v), Y(y)) : g.moveTo(X(v), Y(y)); } g.stroke(); g.setLineDash([]); };
  curve(ball, css('--ink-3'), 1.2, [3, 3]); curve(tube, css('--ink-3'), 1.2, [3, 3]); curve(slab, css('--ink-3'), 1.2, [3, 3]);
  curve(v => Math.min(ball(v), tube(v), slab(v)), css('--new'), 3);
  for (const t of [4 * Math.PI / 81, 1 / Math.PI, 1 - 1 / Math.PI, 1 - 4 * Math.PI / 81]) { g.strokeStyle = css('--line'); g.beginPath(); g.moveTo(X(t), T); g.lineTo(X(t), h - B); g.stroke(); }
  g.font = font(12, 600); g.fillStyle = css('--ink'); g.textAlign = 'center';
  [['ball', .075], ['tube', .235], ['slab', .5], ['tube hole', .765], ['ball hole', .925]].forEach(([s, v]) => g.fillText(s, X(v), T + 14));
  g.font = font(11); g.fillStyle = css('--ink-3'); g.textAlign = 'left'; g.fillText('dashed: each candidate shape · blue: the proven best', L + 6, h - B - 8);
  const v = bl.V, a = Math.min(ball(v), tube(v), slab(v));
  g.fillStyle = css('--new'); g.beginPath(); g.arc(X(v), Y(a), 6, 0, 7); g.fill(); g.strokeStyle = css('--surface'); g.lineWidth = 2; g.stroke();
  const which = a === ball(v) ? 'ball' : a === tube(v) ? 'tube' : 'slab';
  document.getElementById('bl-read2').innerHTML = `At ${Math.round(v * 100)}% full, the 3D answer is a <b>${which}${v > .5 ? '-shaped hole' : ''}</b> with surface ${a.toFixed(3)} (cube side = 1).<br>Switch points, proven: 4π/81 ≈ 15.5% and 1/π ≈ 31.8% (mirrored past 50%).`;
}

// ===================================================================== main loop
function frame() {
  if (visible.has('econ')) { stepEc(); drawEcChart(); }
  if (visible.has('frac')) drawFkLanes();
  if (visible.has('dung') && dg.ref) { dgTick(); drawDgMap(); if ((frame.k = (frame.k || 0) + 1) % 6 === 0) drawDgHist(); }
  if (visible.has('blob')) { if (bl.it < 400) blStep(); drawBl(); }
  requestAnimationFrame(frame);
}
function redrawStatic() { drawSk(); drawSkChart(); drawEcGraph(); drawEcChart(); drawFkBars(); drawBlChart(); if (dg.ref) drawDgHist(); }
window.addEventListener('resize', redrawStatic);
matchMedia('(prefers-color-scheme: dark)').addEventListener('change', () => { redrawStatic(); });
skNew(); drawSkChart(); ecReset(); drawEcGraph(); drawEcChart();
blFormula(); blSeed(); bl.pF = perimeter(bl.formula); bl.pS = perimeter(bl.u); blV.oninput(); drawBl(); setP(1);
setTimeout(() => { skBench(); dgRef(); drawDgHist(); fkPrecompute(); }, 50);
requestAnimationFrame(frame);
