// Colony Ledger browser app. Needs core.js (inlined before this by build.py).
'use strict';
const Q = new URLSearchParams(location.search);
const MAXF = +Q.get('frames') || 0;
const RES_COL = ['#a3a3a3', '#8b5a2b', '#5d6470', '#c9a227', '#b05a8a', '#7fa33a', '#49a6a0'];
const OLD = '#eb6834', NEW = '#2a78d6', REF = '#7a7873';
const MW = 628, MH = 340;
const $ = id => document.getElementById(id);

const st = {
  scene: CL_SCENES[Q.get('scene')] ? Q.get('scene') : 'broken', recipes: [], net: null,
  N: +Q.get('towns') || 240, speed: 0.5, K: +Q.get('k') || 32, H: +Q.get('h') || 10, jitter: 1.5,
  prior: null, neu: null, sel: 0, frame: 0, paused: false, msP: [], msN: [], histP: [], histN: [],
  lint: null, lintUs: 0, sweep: null, sweepMs: 0, layout: null, faucets: 0,
};

// ---------------------------------------------------------------- terrain + town layout
function noise2(seed) {
  const r = clRng(seed), G = 64, v = new Float32Array(G * G); for (let i = 0; i < v.length; i++) v[i] = r();
  const at = (x, y) => v[((y % G + G) % G) * G + ((x % G + G) % G)];
  const sm = t => t * t * (3 - 2 * t);
  return (x, y) => { const xi = Math.floor(x), yi = Math.floor(y), fx = sm(x - xi), fy = sm(y - yi);
    return (at(xi, yi) * (1 - fx) + at(xi + 1, yi) * fx) * (1 - fy) + (at(xi, yi + 1) * (1 - fx) + at(xi + 1, yi + 1) * fx) * fy; };
}
const nA = noise2(11), nB = noise2(23), nC = noise2(37);
function height(x, y) { // x,y in map pixels
  const u = x / MW, w = y / MH;
  let h = nA(u * 5, w * 3) * 0.55 + nB(u * 11, w * 6) * 0.3 + nC(u * 23, w * 12) * 0.15;
  h -= 0.34 * Math.exp(-((u - 0.62) ** 2 / 0.006 + (w - 0.55) ** 2 / 0.02)); // a lake
  h -= 0.25 * Math.exp(-((u - 0.12) ** 2) / 0.0015 - 0 * w) * (0.5 + 0.5 * Math.sin(w * 9)); // a river bend
  return h;
}
let TERRAIN = null;
function buildTerrain() {
  const c = document.createElement('canvas'); c.width = MW; c.height = MH; const g = c.getContext('2d'), img = g.createImageData(MW, MH);
  for (let y = 0; y < MH; y++) for (let x = 0; x < MW; x++) {
    const h = height(x, y), f = nC(x / 9, y / 9), o = (y * MW + x) * 4; let col;
    if (h < 0.3) col = [38 + h * 60, 78 + h * 90, 112 + h * 100];
    else if (h < 0.33) col = [196, 182, 132];
    else if (h < 0.62) { const k = (h - 0.33) / 0.29; col = [92 - 25 * k + f * 14, 128 - 18 * k + f * 16, 64 - 14 * k]; }
    else { const k = Math.min(1, (h - 0.62) / 0.15); col = [104 + 40 * k, 104 + 34 * k, 86 + 40 * k]; }
    img.data[o] = col[0]; img.data[o + 1] = col[1]; img.data[o + 2] = col[2]; img.data[o + 3] = 255;
  }
  g.putImageData(img, 0, 0);
  const r = clRng(5); // forests
  for (let i = 0; i < 1800; i++) { const x = r() * MW, y = r() * MH, h = height(x, y);
    if (h > 0.4 && h < 0.6 && nB(x / 40, y / 40) > 0.55) { g.fillStyle = `rgba(30,${60 + r() * 30 | 0},34,0.75)`; g.beginPath(); g.arc(x, y, 1.6 + r() * 1.6, 0, 7); g.fill(); } }
  return c;
}
function buildLayout(N) {
  const pts = [], r = clRng(1234 + N);
  let cell = Math.sqrt(MW * MH / (N * 1.35));
  for (let tries = 0; tries < 6 && pts.length < N; tries++) {
    pts.length = 0;
    for (let y = cell / 2; y < MH - 4; y += cell) for (let x = cell / 2; x < MW - 4; x += cell) {
      const px = x + (r() - 0.5) * cell * 0.5, py = y + (r() - 0.5) * cell * 0.5;
      if (height(px, py) > 0.35 && height(px, py) < 0.7) pts.push([px, py, r()]);
    }
    if (pts.length < N) cell *= 0.9;
  }
  pts.sort((a, b) => a[2] - b[2]); const P = pts.slice(0, N);
  // roads: each town to its two nearest neighbours
  const c = document.createElement('canvas'); c.width = MW; c.height = MH; const g = c.getContext('2d');
  g.drawImage(TERRAIN, 0, 0); g.strokeStyle = 'rgba(150,120,80,0.55)'; g.lineWidth = Math.max(0.6, cell / 22);
  P.forEach((p, i) => { const d = P.map((q, j) => [j === i ? 1e9 : (q[0] - p[0]) ** 2 + (q[1] - p[1]) ** 2, j]).sort((a, b) => a[0] - b[0]);
    for (let k = 0; k < 2; k++) { const q = P[d[k][1]]; g.beginPath(); g.moveTo(p[0], p[1]); g.lineTo(q[0], q[1]); g.stroke(); } });
  const names = P.map((_, i) => townName(i));
  return { P, cell, bg: c, names };
}
function townName(i) {
  const a = ['Ash', 'Brook', 'Cold', 'Dun', 'Elm', 'Fen', 'Gold', 'Hale', 'Iron', 'Kings', 'Lark', 'Mill', 'North', 'Oak', 'Pike', 'Red', 'Stone', 'Thorn', 'West', 'Wyn'];
  const b = ['ford', 'ham', 'wick', 'stead', 'by', 'mere', 'field', 'gate', 'holt', 'ton', 'well', 'bury'];
  const r = clRng(99 + i); return a[(r() * a.length) | 0] + b[(r() * b.length) | 0];
}

// ---------------------------------------------------------------- simulation state
function restart() {
  const sc = CL_SCENES[st.scene];
  st.net = clNet(st.recipes, sc.rateScale);
  const opt = { jitter: st.jitter, integer: sc.integer };
  st.prior = clTowns(st.net, st.N, 149, opt); st.neu = clTowns(st.net, st.N, 149, opt);
  st.histP = []; st.histN = []; st.msP = []; st.msN = [];
  if (st.sel >= st.N) st.sel = 0;
}
function save() { // what happens when the designer saves the recipe book
  const t0 = performance.now(); let L; for (let i = 0; i < 500; i++) L = clLint(st.recipes);
  st.lintUs = (performance.now() - t0) / 500 * 1000; st.lint = L;
  const sc = CL_SCENES[st.scene], t1 = performance.now();
  st.sweep = clSweep(st.recipes, { K: st.K, H: st.H, rateScale: sc.rateScale, jitter: st.jitter, integer: sc.integer, seed: 7 });
  st.sweepMs = performance.now() - t1;
  restart(); renderEditor(); renderChecks();
}
function setScene(name) {
  st.scene = name; st.recipes = clSceneRecipes(name); st.speed = CL_SCENES[name].speed || 0.5;
  $('speed').value = st.speed; $('speedv').textContent = st.speed;
  document.querySelectorAll('#scenes button').forEach(b => b.classList.toggle('on', b.dataset.s === name));
  save();
}

// ---------------------------------------------------------------- recipe editor
const GROUPS = [['village', 'Village'], ['forge', 'Forge'], ['workshop', 'Workshop'], ['plague', 'Plague (workers)']];
function chips(v) { const s = []; v.forEach((c, i) => { if (c) s.push(`<span class="chip" style="--c:${RES_COL[i]}">${c > 1 ? c + ' ' : ''}${CL_SP[i]}</span>`); }); return s.join('<i>+</i>'); }
function fmtRate(k) { return k >= 0.1 ? k.toFixed(2) : k.toPrecision(2); }
function renderEditor() {
  const L = st.lint, groups = GROUPS.filter(([g]) => st.recipes.some(r => r.g === g));
  $('editor').style.gridTemplateColumns = `repeat(${groups.length}, 1fr)`;
  $('editor').innerHTML = groups.map(([g, title]) => {
    const rs = st.recipes.filter(r => r.g === g);
    const ok = rs.every(r => !r.active || L.ok.get(r));
    return `<div class="grp"><div class="gh"><span>${title}</span><span class="${ok ? 'gok' : 'gbad'}">${ok ? 'loop closed' : 'open chain'}</span></div>` +
      rs.map(r => { const i = st.recipes.indexOf(r), pass = r.active && L.ok.get(r);
        const cls = !r.active ? 'off' : pass ? 'ok' : 'bad';
        return `<div class="card ${cls}${r.added ? ' added' : ''}" data-i="${i}" title="click to ${r.active ? 'remove' : 'restore'} this recipe">
          <div class="eq">${chips(r.from)}<b class="arr">&#10142;</b>${chips(r.to)}</div>
          <div class="meta"><span class="nm">${r.name}${r.added ? ' <em>added by fix</em>' : ''}</span><span class="rt">rate ${fmtRate(r.k * (CL_SCENES[st.scene].rateScale || 1))}</span>
          <span class="bd">${!r.active ? 'removed' : pass ? '&#10003; way back' : '&#10007; no way back'}</span></div></div>`; }).join('') + '</div>';
  }).join('');
  document.querySelectorAll('#editor .card').forEach(c => c.onclick = () => { const r = st.recipes[+c.dataset.i]; r.active = !r.active; save(); });
}
function renderChecks() {
  const s = st.sweep, L = st.lint, pass = s.flagged === 0;
  const ft = s.firstT.length ? Math.min(...s.firstT) : null;
  $('chkP').innerHTML = `<div class="ch-h">Save-time balance sweep <span class="dim">(Monte-Carlo playtest)</span></div>
    <div class="ch-r"><span class="verdict ${pass ? 'pass' : 'fail'}">${pass ? 'PASS' : 'FAIL'}</span>
    ${pass ? `no cap or floor hit in <b>${s.K}</b> random towns &times; <b>${s.H}</b> years` : `<b>${s.flagged}/${s.K}</b> random towns hit a cap or floor (first at year ${ft.toFixed(1)})`}</div>
    <div class="ch-r dim">cost <b class="num">${st.sweepMs.toFixed(1)} ms</b> for ${Math.round(s.years).toLocaleString()} simulated town-years &middot; a pass covers only these ${s.K} rate sets and ${s.H} years</div>`;
  const bad = st.recipes.filter(r => r.active && !L.ok.get(r));
  $('chkN').innerHTML = `<div class="ch-h">Save-time lint <span class="dim">(every recipe needs a way back)</span></div>
    <div class="ch-r"><span class="verdict ${L.pass ? 'pass' : 'fail'}">${L.pass ? 'PASS' : 'FAIL'}</span>
    ${L.pass ? 'every recipe can be undone through other recipes: no good dies out or piles up forever, for <b>any</b> fixed positive rates' :
      `<b>${bad.length}</b> recipe${bad.length > 1 ? 's' : ''} lack a way back: ${bad.slice(0, 2).map(r => r.name).join(', ')}${bad.length > 2 ? ', &hellip;' : ''}
       <button id="fix" class="fixb">Fix: add return recipes</button>`}</div>
    <div class="ch-r dim">cost <b class="num">${st.lintUs < 1000 ? st.lintUs.toFixed(1) + ' &micro;s' : (st.lintUs / 1000).toFixed(2) + ' ms'}</b> &middot; ${L.pass ? 'guarantee: continuous amounts, fixed rates, after a start-up period' : 'failing means no guarantee, not certain collapse'}</div>`;
  if ($('fix')) $('fix').onclick = () => { clFix(st.recipes); save(); };
}

// ---------------------------------------------------------------- drawing
const lv = v => Math.max(0, Math.min(1, (Math.log10(v) + 2) / 3)); // 0 at 1 unit, 1 at 1,000 units
function townState(T, n, side) {
  const D = T.D, xs = []; for (let i = 0; i < D; i++) xs.push(Math.exp(T.y[n * D + i]));
  let light, tag;
  if (side === 'P') {
    let cap = false, flo = false; for (let i = 0; i < D; i++) { if (xs[i] >= CL.CAP * 0.999) cap = true; if (xs[i] <= CL.FAUCET * 1.001) flo = true; }
    if (T.clampedNow[n]) { light = '#f2b233'; tag = cap ? 'pinned at storage cap' : 'relief caravan (faucet)'; } else { light = '#5fd36a'; tag = 'running'; }
    if (T.clampedNow[n] && cap) tag = flo ? 'pinned at cap and floor' : tag;
  } else {
    let lo = Infinity; xs.slice(0, 5).forEach(v => { if (!(v >= lo)) lo = v; });
    if (T.status[n] & 1) { light = '#e0453a'; tag = CL_SP[T.deadSp[n]] + (T.deadSp[n] === 5 ? ': plague burned out' : ' died out'); }
    else if (T.status[n] & 2) { light = '#b36bff'; tag = 'ran away (over 1e8 units)'; }
    else if (lo < CL.STARVE) { light = '#f2e033'; tag = T.integer ? 'a stockpile is empty' : 'starving (below 1 unit)'; }
    else { light = '#5fd36a'; tag = 'healthy'; }
  }
  return { xs, light, tag };
}
function drawTown(g, x, y, s, S, T, n, side, t) {
  const xs = S.xs, dead = side === 'N' && (T.status[n] & 1) && T.deadSp[n] !== 5, run = side === 'N' && (T.status[n] & 2);
  const F = lv(xs[3]), P = lv(xs[4]), O = lv(xs[0]), W = lv(xs[1]), To = lv(xs[2]);
  if (s >= 13) {
    // field (food)
    const fw = s * 0.44, fh = s * 0.26, fx = x - s * 0.5, fy = y + s * 0.08;
    g.fillStyle = dead ? '#4a4038' : `rgb(${110 + 110 * F | 0},${88 + 80 * F | 0},${40 + 10 * F | 0})`; g.fillRect(fx, fy, fw, fh);
    g.strokeStyle = 'rgba(0,0,0,0.25)'; g.lineWidth = 0.6; for (let k = 1; k < 4; k++) { g.beginPath(); g.moveTo(fx, fy + fh * k / 4); g.lineTo(fx + fw, fy + fh * k / 4); g.stroke(); }
    // trees (wood)
    const nt = dead ? 0 : 1 + Math.round(W * 2);
    for (let k = 0; k < nt; k++) { g.fillStyle = '#1f4a26'; g.beginPath(); g.arc(x - s * 0.42 + k * s * 0.13, y - s * 0.28, s * 0.08, 0, 7); g.fill(); }
    // mine (ore)
    const mr = s * (0.07 + 0.09 * O); g.fillStyle = '#7d7a72'; g.beginPath(); g.arc(x + s * 0.32, y + s * 0.34, mr, Math.PI, 0); g.fill();
    g.fillStyle = '#111'; g.beginPath(); g.arc(x + s * 0.32, y + s * 0.34, mr * 0.4, Math.PI, 0); g.fill();
    // forge (tools) + smoke
    g.fillStyle = dead ? '#555' : '#3b3f47'; g.fillRect(x + s * 0.18, y - s * 0.12, s * 0.2, s * 0.16); g.fillRect(x + s * 0.31, y - s * 0.24, s * 0.05, s * 0.12);
    if (!dead && To > 0.25) for (let k = 0; k < 2; k++) { const ph = (t * 0.02 + k * 0.5 + n * 0.13) % 1; g.fillStyle = `rgba(210,210,210,${(1 - ph) * 0.5 * To})`; g.beginPath(); g.arc(x + s * 0.34 + ph * s * 0.1, y - s * 0.28 - ph * s * 0.3, s * 0.04 + ph * s * 0.05, 0, 7); g.fill(); }
    // houses (workers)
    const nh = run ? 6 : 1 + Math.round(P * 4), hs = s * 0.15;
    const off = [[-0.08, -0.02], [0.06, 0.04], [-0.22, 0.0], [-0.02, -0.17], [0.1, 0.18], [-0.16, -0.16]];
    for (let k = 0; k < nh; k++) { const hx = x + off[k][0] * s, hy = y + off[k][1] * s;
      g.fillStyle = dead ? '#6b6b6b' : '#e9dcc0'; g.fillRect(hx - hs / 2, hy - hs * 0.35, hs, hs * 0.7);
      if (!dead) { g.fillStyle = '#7a3b2c'; g.beginPath(); g.moveTo(hx - hs * 0.62, hy - hs * 0.32); g.lineTo(hx, hy - hs * 0.85); g.lineTo(hx + hs * 0.62, hy - hs * 0.32); g.fill(); } }
    if (run) { g.strokeStyle = 'rgba(179,107,255,0.8)'; g.lineWidth = 1.5; g.beginPath(); g.arc(x, y, s * 0.55, 0, 7); g.stroke(); }
    if (xs.length > 5 && !((T.status[n] & 1) && T.deadSp[n] === 5)) { // plague haze
      const f = Math.min(1, Math.sqrt(xs[5] / (xs[4] + xs[5] + 1e-12)) * 2.2); if (f > 0.05) {
        const gr = g.createRadialGradient(x, y, 0, x, y, s * 0.6);
        gr.addColorStop(0, `rgba(127,163,58,${0.65 * f})`); gr.addColorStop(1, 'rgba(127,163,58,0)'); g.fillStyle = gr; g.fillRect(x - s * 0.6, y - s * 0.6, s * 1.2, s * 1.2);
        g.fillStyle = '#9fd04a'; g.fillRect(x - s * 0.5, y - s * 0.7, 1.5, s * 0.4); g.fillRect(x - s * 0.5, y - s * 0.7, s * 0.24, s * 0.15); // plague flag
      } }
  } else { g.fillStyle = dead ? '#6b6b6b' : '#e9dcc0'; g.fillRect(x - s * 0.25, y - s * 0.15, s * 0.5, s * 0.35); }
  // light
  const lr = Math.max(2, s * 0.1), lx = x + s * 0.42, ly = y - s * 0.38;
  g.fillStyle = S.light; g.shadowColor = S.light; g.shadowBlur = 6; g.beginPath(); g.arc(lx, ly, lr, 0, 7); g.fill(); g.shadowBlur = 0;
  g.strokeStyle = 'rgba(0,0,0,0.6)'; g.lineWidth = 0.8; g.stroke();
}
function drawMap(cv, T, side) {
  const g = cv.getContext('2d'), Ly = st.layout, s = Math.min(30, Ly.cell * 0.8);
  g.drawImage(Ly.bg, 0, 0);
  const cnt = { green: 0 };
  for (let n = 0; n < st.N; n++) { const p = Ly.P[n], S = townState(T, n, side); drawTown(g, p[0], p[1], s, S, T, n, side, st.frame); }
  const p = Ly.P[st.sel]; g.strokeStyle = '#fff'; g.lineWidth = 2; g.setLineDash([4, 3]); g.beginPath(); g.arc(p[0], p[1], s * 0.75, 0, 7); g.stroke(); g.setLineDash([]);
  // HUD
  const S = clSummary(T), ms = med(side === 'P' ? st.msP : st.msN), hist = side === 'P' ? st.histP : st.histN;
  const rows = side === 'P'
    ? [['pinned at cap/floor now', S.pinnedNow, '#f2b233'], ['share of town-time pinned', S.timePinned, '#f2b233'], ['died out / ran away', 0, '#e0453a']]
    : [['a good died out' + (S.dead ? ' (mostly ' + CL_SP[S.deadBy.indexOf(Math.max(...S.deadBy))] + ')' : ''), S.dead, '#e0453a'], ['ran away (> 1e6)', S.runaway, '#b36bff'], [T.integer ? 'an empty stockpile now' : 'below 1 unit (starving)', S.starving, '#f2e033']];
  g.fillStyle = 'rgba(16,18,22,0.78)'; g.fillRect(8, 8, 236, 104); g.strokeStyle = side === 'P' ? OLD : NEW; g.lineWidth = 1.5; g.strokeRect(8.5, 8.5, 235, 103);
  g.font = '600 12px system-ui, sans-serif'; g.fillStyle = '#f2f0ea';
  g.fillText(`Year ${st.prior.t.toFixed(1)} · ${st.N} towns`, 16, 26);
  g.font = '12px system-ui, sans-serif';
  rows.forEach(([lab, v, c], i) => { g.fillStyle = c; g.fillRect(16, 37 + i * 17, 8, 8); g.fillStyle = '#d9d6cc'; g.fillText(lab, 30, 45 + i * 17); g.fillStyle = '#fff'; g.textAlign = 'right'; g.fillText((v * 100).toFixed(1) + '%', 236, 45 + i * 17); g.textAlign = 'left'; });
  let live = 0; for (let n = 0; n < st.N; n++) if (!T.stopped[n]) live++;
  g.fillStyle = '#d9d6cc'; g.fillText(`sim cost (${live} towns live)`, 16, 100); g.fillStyle = '#fff'; g.textAlign = 'right'; g.fillText(isFinite(ms) ? ms.toFixed(2) + ' ms / frame' : '-', 236, 100); g.textAlign = 'left';
  // trouble sparkline
  const sx = MW - 168, sy = 8; g.fillStyle = 'rgba(16,18,22,0.78)'; g.fillRect(sx, sy, 160, 52);
  g.fillStyle = '#d9d6cc'; g.font = '11px system-ui, sans-serif'; g.fillText(side === 'P' ? 'towns pinned, over time' : 'towns in trouble, over time', sx + 6, sy + 13);
  g.strokeStyle = side === 'P' ? OLD : NEW; g.lineWidth = 1.5; g.beginPath();
  hist.forEach((v, i) => { const X = sx + 6 + i / Math.max(1, hist.length - 1) * 148, Y = sy + 47 - v * 28; i ? g.lineTo(X, Y) : g.moveTo(X, Y); }); g.stroke();
  g.strokeStyle = 'rgba(255,255,255,0.2)'; g.lineWidth = 1; g.beginPath(); g.moveTo(sx + 6, sy + 47.5); g.lineTo(sx + 154, sy + 47.5); g.moveTo(sx + 6, sy + 19.5); g.lineTo(sx + 154, sy + 19.5); g.stroke();
  g.fillStyle = '#8f8e86'; g.fillText('100%', sx + 128, sy + 30);
}
function med(a) { if (!a.length) return NaN; const s = a.slice().sort((x, y) => x - y); return s[s.length >> 1]; }
function fmtUnits(v) { const u = v * 100; if (!isFinite(u)) return u > 0 ? '∞' : '0'; if (u > 1e12) return '> 1e12'; if (u < 0.01) return u.toExponential(0); if (u < 10) return u.toFixed(2); if (u < 1e5) return Math.round(u).toLocaleString(); return u.toExponential(1); }
function drawLedger() {
  const cv = $('ledger'), g = cv.getContext('2d'), W = cv.width, H = cv.height, n = st.sel, D = st.prior.D;
  g.fillStyle = '#20232a'; g.fillRect(0, 0, W, H);
  const SP = townState(st.prior, n, 'P'), SN = townState(st.neu, n, 'N');
  g.fillStyle = '#f2f0ea'; g.font = '600 15px Georgia, serif'; g.fillText(st.layout.names[n], 12, 22);
  g.font = '11px system-ui, sans-serif'; g.fillStyle = '#8f8e86'; g.fillText(`town #${n} · own random rates · click a town on either map`, 12, 38);
  g.fillStyle = OLD; g.fillText('PRIOR: ' + SP.tag, 12, 54); g.fillStyle = '#7fb0ee'; g.fillText('NEW: ' + SN.tag, 200, 54);
  const L = 92, R = W - 70, X = v => L + Math.max(0, Math.min(1, (Math.log10(v) + 4) / 8)) * (R - L); // 1e-4 .. 1e4
  const top = 66, rowH = Math.min(30, (H - top - 22) / D);
  // floor / cap guides
  g.strokeStyle = 'rgba(235,104,52,0.5)'; g.setLineDash([3, 3]);
  [CL.FLOOR, CL.CAP].forEach(v => { g.beginPath(); g.moveTo(X(v) + 0.5, top - 4); g.lineTo(X(v) + 0.5, top + rowH * D); g.stroke(); }); g.setLineDash([]);
  g.fillStyle = '#8f8e86'; g.font = '10px system-ui'; g.textAlign = 'center';
  g.fillText('floor 1', X(CL.FLOOR), top + rowH * D + 12); g.fillText('cap 2,000', X(CL.CAP), top + rowH * D + 12);
  g.textAlign = 'left'; g.fillText('0.01', L, top + rowH * D + 12); g.textAlign = 'right'; g.fillText('1e6 units', R, top + rowH * D + 12); g.textAlign = 'left';
  for (let i = 0; i < D; i++) {
    const y = top + i * rowH; g.fillStyle = RES_COL[i]; g.fillRect(12, y + 4, 10, 10);
    g.fillStyle = '#e8e6df'; g.font = '12px system-ui'; g.fillText(CL_SP[i], 28, y + 13);
    [[SP.xs[i], OLD, 0], [SN.xs[i], NEW, 1]].forEach(([v, c, k]) => { const by = y + 2 + k * (rowH * 0.42);
      g.fillStyle = 'rgba(255,255,255,0.07)'; g.fillRect(L, by, R - L, rowH * 0.36); g.fillStyle = c; g.fillRect(L, by, isFinite(v) ? X(v) - L : R - L, rowH * 0.36);
      g.fillStyle = '#e8e6df'; g.font = '10px ui-monospace, Consolas, monospace'; g.fillText(fmtUnits(v), R + 6, by + rowH * 0.33); });
  }
}
function draw() { drawMap($('mapP'), st.prior, 'P'); drawMap($('mapN'), st.neu, 'N'); drawLedger(); }

// ---------------------------------------------------------------- loop
function step() {
  const dt = st.speed;
  const a = performance.now(); clFrame(st.net, st.prior, dt, true);
  const b = performance.now(); clFrame(st.net, st.neu, dt, false);
  const c = performance.now();
  st.msP.push(b - a); st.msN.push(c - b); if (st.msP.length > 60) { st.msP.shift(); st.msN.shift(); }
  const sp = clSummary(st.prior), sn = clSummary(st.neu);
  st.histP.push(sp.pinnedNow); st.histN.push(sn.deadOrRun); if (st.histP.length > 400) { st.histP.shift(); st.histN.shift(); }
}
function pickTown() { // show a town where the two sides differ, nearest the map centre
  if (st.userPicked) return;
  let best = -1, bd = Infinity;
  for (let n = 0; n < st.N; n++) { const p = st.layout.P[n], d = (p[0] - MW * 0.5) ** 2 + (p[1] - MH * 0.5) ** 2;
    if ((st.neu.status[n] || st.prior.clampedNow[n]) && d < bd) { bd = d; best = n; } }
  if (best >= 0) st.sel = best;
}
function tick() {
  if (!st.paused) step();
  st.frame++;
  if (st.frame === 200) pickTown();
  draw();
  requestAnimationFrame(tick);
}
function runFrames() { // ?frames=N: simulate N frames synchronously (real wall-clock timing), draw once, stop
  for (let f = 0; f < MAXF; f++) { step(); st.frame++; }
  pickTown(); draw();
  $('status').textContent = `stopped after ${MAXF} frames (?frames=${MAXF})`;
}

// ---------------------------------------------------------------- controls
function init() {
  TERRAIN = buildTerrain(); st.layout = buildLayout(st.N);
  $('scenes').innerHTML = Object.entries(CL_SCENES).map(([k, s]) => `<button data-s="${k}" title="${s.label}">${k}</button>`).join('');
  document.querySelectorAll('#scenes button').forEach(b => b.onclick = () => setScene(b.dataset.s));
  const bind = (id, f) => { $(id).oninput = () => { f(+$(id).value); }; };
  $('towns').value = st.N; $('townsv').textContent = st.N;
  bind('towns', v => { st.N = v; $('townsv').textContent = v; st.layout = buildLayout(v); st.sel = 0; restart(); });
  bind('speed', v => { st.speed = v; $('speedv').textContent = v; });
  const HS = [1, 2, 5, 10, 20, 40, 80, 160, 320, 640];
  $('hz').value = Math.max(0, HS.indexOf(st.H)); $('hzv').textContent = st.H;
  $('hz').onchange = () => { st.H = HS[+$('hz').value]; $('hzv').textContent = st.H; save(); }; $('hz').oninput = () => { $('hzv').textContent = HS[+$('hz').value]; };
  $('kk').value = st.K; $('kv').textContent = st.K;
  $('kk').onchange = () => { st.K = +$('kk').value; save(); }; $('kk').oninput = () => { $('kv').textContent = $('kk').value; };
  $('jit').onchange = () => { st.jitter = +$('jit').value; save(); }; $('jit').oninput = () => { $('jitv').textContent = (+$('jit').value).toFixed(1); };
  $('roll').onclick = () => { const r = clRng((Math.random() * 1e9) | 0); st.recipes.forEach(x => x.k *= Math.exp((r() * 2 - 1) * Math.log(4))); save(); };
  $('reset').onclick = () => setScene(st.scene);
  $('pause').onclick = () => { st.paused = !st.paused; $('pause').textContent = st.paused ? 'Resume' : 'Pause'; };
  const pick = e => { const cv = e.currentTarget, r = cv.getBoundingClientRect(), x = (e.clientX - r.left) * MW / r.width, y = (e.clientY - r.top) * MH / r.height;
    let best = 0, bd = Infinity; st.layout.P.forEach((p, i) => { const d = (p[0] - x) ** 2 + (p[1] - y) ** 2; if (d < bd) { bd = d; best = i; } }); st.sel = best; st.userPicked = true; draw(); };
  $('mapP').onclick = pick; $('mapN').onclick = pick;
  setScene(st.scene);
  if (MAXF) runFrames(); else requestAnimationFrame(tick);
}
init();
