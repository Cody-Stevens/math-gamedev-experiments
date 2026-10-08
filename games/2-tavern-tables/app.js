// app.js - scene, HUD and controls for "Tavern of a Thousand Tables". Logic lives in core.js.
'use strict';
const CERT = loadCert(SNAKY_CERT);
const CERT_KB = Math.round(JSON.stringify(SNAKY_CERT).length / 1024);
const W = 1280, H = 640, HW = 634, HX = [0, 646];
const COL = { prior: '#eb6834', new: '#2a78d6', ref: '#7a7873', gold: '#f2c14e', red: '#c8402f' };
const qs = new URLSearchParams(location.search);
const MAX_FRAMES = +qs.get('frames') || 0;
const SCENE = qs.get('scene') || '';

// ------------------------------------------------------------------ state
const cv = document.getElementById('scene'), g = cv.getContext('2d');
const halls = [new Hall('prior', CERT), new Hall('new', CERT)];
const cfg = { n: 16, kind: BRK_GREEDY, k: 1, mode: 'budget', every: 10, play: false };
let frameNo = 0, tickNo = 0, calib = null, certUs = null, certLog = [], playerTally = [{ g: 0, w: 0 }, { g: 0, w: 0 }];
const tonight = new Map();
const ui = id => document.getElementById(id);

function presets() {
  const P = {
    one: { n: 1 }, busy: { n: 256 }, crowd: { n: 4096, every: 4 }, duel: { n: 16, kind: BRK_SEARCH },
    nobudget: { n: 16, mode: 'fixed' }, play: { n: 256, play: true },
  };
  Object.assign(cfg, P[SCENE] || {});
}

function clampCfg() {
  let w = '';
  if (cfg.kind === BRK_SEARCH && cfg.n > 256) { cfg.n = 256; w = 'The α-β Breakers are slow to simulate: capped at 256 tables. '; }
  if (cfg.mode === 'fixed' && cfg.n > 64) { cfg.n = 64; w += 'No-budget search: capped at 64 tables (one frame would take seconds). '; }
  ui('warn').textContent = w;
}

function applyCfg() {
  clampCfg();
  for (const h of halls) {
    if (calib) h.usPerNode = calib.usPerNode;
    h.configure(cfg.n, cfg.kind, cfg.k, cfg.mode);
    h.player = null;
    h.featured = cfg.play ? 3 : 4;
    h.holdTicks = Math.round(1.4 * 60 / cfg.every);
  }
  if (cfg.play) for (const h of halls) { const t = h.tables[0]; h.player = t; t.start(); h.playerMaker(t); }
  tickNo = 0;
  ui('nslider').value = Math.round(Math.log2(cfg.n)); ui('ntab').textContent = cfg.n.toLocaleString();
  ui('brk').value = cfg.kind; ui('pace').value = cfg.k; ui('pmode').value = cfg.mode; ui('speed').value = cfg.every;
  ui('play').classList.toggle('on', cfg.play); ui('play').textContent = cfg.play ? 'Leave your seat' : 'Sit down as Breaker';
}

// ------------------------------------------------------------------ measurements
// playbook microseconds per move: replay tonight's finished games (their Breaker replies) in a batch of >= 4 ms
function measureCert() {
  const games = certLog.length ? certLog : [[]];
  const P = new CertPolicy(CERT), own = new Uint8Array(SNC);
  let dec = 0; const t0 = performance.now(); let t1 = t0;
  let reps = 0;
  while (t1 - t0 < 4 && reps++ < 2000) {   // the rep cap also ends the loop if the clock is frozen (headless virtual time)
    for (const rep of games) {
      own.fill(0); P.reset();
      for (let i = 0; i <= rep.length; i++) { const c = P.decide(own); dec++; if (c < 0 || i === rep.length) break; own[c] = 1; own[rep[i]] = 2; P.observe(rep[i]); }
    }
    t1 = performance.now();
  }
  return (t1 - t0) * 1000 / dec;
}
halls[1].onGame = t => { certLog.push(t.replies.slice()); if (certLog.length > 24) certLog.shift(); };
halls[0].onGame = null;

const cfgKey = () => `${cfg.n}|${cfg.kind}|${cfg.k}|${cfg.mode}`;
function snapTonight() {
  const [p, q] = halls; if (!p.st.games && !q.st.games) return;
  tonight.set(cfgKey(), { n: cfg.n, kind: cfg.kind, k: cfg.k, mode: cfg.mode,
    p: { g: p.st.games, w: p.st.wins, mv: p.st.wins ? p.st.sumWinMoves / p.st.wins : 0, mx: p.st.maxWinMoves, ms: median(p.st.tickMs), cut: p.st.moves ? p.st.shortMoves / p.st.moves : 0 },
    q: { g: q.st.games, w: q.st.wins, mv: q.st.wins ? q.st.sumWinMoves / q.st.wins : 0, mx: q.st.maxWinMoves, ms: newFrameMs() } });
}
function newFrameMs() { // NEW hall: measured tick time if the clock can see it, else moves per tick x batch-timed us per move
  const h = halls[1], m = median(h.st.tickMs);
  if (m >= 0.5 || certUs === null) return m;
  return Math.ceil(cfg.n / cfg.k) * certUs / 1000;
}
function renderTonight() {
  const rows = [...tonight.values()].sort((a, b) => a.n - b.n || a.kind - b.kind || a.k - b.k);
  if (!rows.length) return;
  const pc = (w, g) => g ? `${(100 * w / g).toFixed(0)}%` : '-';
  let h = `<table class="ref"><tr><th>tables</th><th>Breaker</th><th>moves every</th><th class="p">PRIOR ms/frame</th><th class="p">won</th><th class="p">avg/max moves</th><th class="n">NEW ms/frame</th><th class="n">won</th><th class="n">avg/max moves</th></tr>`;
  for (const r of rows) {
    const hl = r.n === cfg.n && r.kind === cfg.kind && r.k === cfg.k && r.mode === cfg.mode ? ' class="hl"' : '';
    h += `<tr${hl}><td>${r.n.toLocaleString()}</td><td>${BRK_NAMES[r.kind].split(' ')[0]}</td><td>${r.k} fr${r.mode === 'fixed' ? ', no budget' : ''}</td>` +
      `<td>${r.p.ms.toFixed(1)}</td><td>${r.p.w}/${r.p.g} ${pc(r.p.w, r.p.g)}</td><td>${r.p.mv.toFixed(1)}/${r.p.mx}</td>` +
      `<td>${r.q.ms < 0.1 ? r.q.ms.toFixed(4) : r.q.ms.toFixed(2)}</td><td>${r.q.w}/${r.q.g} ${pc(r.q.w, r.q.g)}</td><td>${r.q.mv.toFixed(1)}/${r.q.mx}</td></tr>`;
  }
  ui('tonight').innerHTML = h + '</table><div class="src">Browser clock steps are about 0.1 ms; tiny NEW times are moves &times; batch-timed &micro;s per move.</div>';
}
function renderLadder() {
  const rows = LADDER.stages;
  let h = `<table class="ref"><tr><th>tables</th><th class="p">search ms/frame<br>(6000 nodes)</th><th class="n">playbook<br>ms/frame</th><th class="p">search win %<br>vs α-β / greedy</th><th class="n">playbook<br>win %</th></tr>`;
  for (const s of rows) {
    const hl = s.n === cfg.n ? ' class="hl"' : '';
    const pq = s.qa === null ? '-' : `${Math.round(100 * s.qa)} / ${Math.round(100 * s.qg)}`;
    h += `<tr${hl}><td>${s.n.toLocaleString()}</td><td>${s.wo === null ? 'not timed' : s.wo.toFixed(1)}</td><td>${s.wi < 0.01 ? s.wi.toFixed(4) : s.wi.toFixed(3)}</td><td>${pq}</td><td>${s.cq === null ? '-' : Math.round(100 * s.cq)}</td></tr>`;
  }
  ui('ladder').innerHTML = h + `</table><div class="src">C++ budget crossing: search fits 16.7 ms up to ${LADDER.fit.without} tables, playbook up to ${LADDER.fit.with.toLocaleString()}. Small playbook times sit at the 0.1 &micro;s timer floor.</div>`;
}

// ------------------------------------------------------------------ art helpers
function rnd(seed) { let s = seed >>> 0; return () => { s = (s + 0x6D2B79F5) >>> 0; let t = s; t = Math.imul(t ^ (t >>> 15), t | 1); t ^= t + Math.imul(t ^ (t >>> 7), t | 61); return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }
function rr(x, y, w, h, r) { g.beginPath(); g.moveTo(x + r, y); g.arcTo(x + w, y, x + w, y + h, r); g.arcTo(x + w, y + h, x, y + h, r); g.arcTo(x, y + h, x, y, r); g.arcTo(x, y, x + w, y, r); g.closePath(); }
const SKIN = ['#f1c9a5', '#e0a77e', '#c68863', '#9a6a46', '#6e4a32', '#f5d7bd', '#b5d39a'];
const ROBE = ['#5b3f8c', '#2f6b4f', '#8c2f3a', '#3f5f8c', '#7a6a2f', '#4d4d4d', '#8c5a2f', '#2f7a7a', '#6b2f6b', '#a0522d'];
const HAIR = ['#2b1a10', '#5a3a1f', '#c9a25a', '#8c3a1f', '#d8d0c0', '#1a1a1a', '#7a5230'];

let bgCanvas = null;
function buildBackground() {
  bgCanvas = document.createElement('canvas'); bgCanvas.width = W; bgCanvas.height = H;
  const b = bgCanvas.getContext('2d'), R = rnd(7);
  b.fillStyle = '#1b110a'; b.fillRect(0, 0, W, H);
  for (let hi = 0; hi < 2; hi++) {
    const x0 = HX[hi];
    b.save(); b.beginPath(); b.rect(x0, 0, HW, H); b.clip();
    // floor planks
    const ph = 22;
    for (let y = 64, row = 0; y < H; y += ph, row++) {
      let x = x0 - ((row * 53) % 140);
      while (x < x0 + HW) {
        const len = 90 + R() * 110, sh = 0.85 + R() * 0.25;
        b.fillStyle = `rgb(${Math.round(112 * sh)},${Math.round(76 * sh)},${Math.round(44 * sh)})`;
        b.fillRect(x, y, len - 2, ph - 2);
        b.strokeStyle = 'rgba(40,24,12,.25)'; b.lineWidth = 1;
        for (let k = 0; k < 3; k++) { const yy = y + 4 + R() * (ph - 8); b.beginPath(); b.moveTo(x + 4, yy); b.bezierCurveTo(x + len * .3, yy + R() * 3 - 1.5, x + len * .6, yy + R() * 3 - 1.5, x + len - 6, yy); b.stroke(); }
        b.fillStyle = 'rgba(30,18,8,.6)'; b.fillRect(x + len - 2, y, 2, ph - 2);
        b.fillStyle = '#2a1a0d'; b.beginPath(); b.arc(x + 5, y + ph / 2 - 1, 1.2, 0, 7); b.arc(x + len - 7, y + ph / 2 - 1, 1.2, 0, 7); b.fill();
        x += len;
      }
      b.fillStyle = '#2b1a0c'; b.fillRect(x0, y + ph - 2, HW, 2);
    }
    // back wall: stone
    b.fillStyle = '#3c342d'; b.fillRect(x0, 0, HW, 64);
    for (let y = 0, row = 0; y < 64; y += 13, row++) for (let x = x0 - (row % 2) * 14; x < x0 + HW; x += 28) {
      const v = 50 + R() * 22; b.fillStyle = `rgb(${v + 8},${v},${v - 6})`; rrB(b, x + 1, y + 1, 26, 11, 3); b.fill();
    }
    b.fillStyle = '#24170d'; b.fillRect(x0, 58, HW, 8);       // skirting beam
    b.fillStyle = '#4a2f19'; b.fillRect(x0, 58, HW, 2);
    if (hi === 0) { // hearth
      const hx = x0 + 34; b.fillStyle = '#5a5047'; rrB(b, hx - 6, 4, 92, 62, 6); b.fill();
      b.fillStyle = '#120a06'; b.beginPath(); b.moveTo(hx + 6, 64); b.lineTo(hx + 6, 26); b.quadraticCurveTo(hx + 40, 2, hx + 74, 26); b.lineTo(hx + 74, 64); b.fill();
      b.fillStyle = '#3b2a1a'; b.fillRect(hx + 18, 54, 44, 6);
    } else { // bar with kegs and bottles
      const bx = x0 + 18;
      b.fillStyle = '#5a3a1f'; b.fillRect(bx, 10, 120, 6); b.fillRect(bx, 30, 120, 6);
      const bc = ['#2f6b4f', '#8c2f3a', '#c9a25a', '#3f5f8c', '#6b2f6b'];
      for (let i = 0; i < 9; i++) { b.fillStyle = bc[i % 5]; b.fillRect(bx + 6 + i * 13, 1, 6, 9); b.fillRect(bx + 8 + i * 13, -2, 2, 4); }
      for (let i = 0; i < 4; i++) { b.fillStyle = bc[(i + 2) % 5]; b.fillRect(bx + 10 + i * 26, 20, 7, 10); }
      for (let i = 0; i < 3; i++) { const kx = bx + 16 + i * 36; b.fillStyle = '#7a4f28'; b.beginPath(); b.ellipse(kx, 50, 15, 10, 0, 0, 7); b.fill(); b.strokeStyle = '#2b1a0c'; b.lineWidth = 2; b.beginPath(); b.ellipse(kx, 50, 15, 10, 0, 0, 7); b.stroke(); b.beginPath(); b.moveTo(kx - 8, 41); b.lineTo(kx - 8, 59); b.moveTo(kx + 8, 41); b.lineTo(kx + 8, 59); b.stroke(); }
    }
    // a rug for the back rooms
    b.fillStyle = '#4b1f1a'; rrB(b, x0 + 6, 402, HW - 12, 118, 8); b.fill();
    b.strokeStyle = '#c9a25a'; b.lineWidth = 2; rrB(b, x0 + 11, 407, HW - 22, 108, 6); b.stroke();
    b.strokeStyle = 'rgba(201,162,90,.35)'; b.setLineDash([3, 4]); rrB(b, x0 + 16, 412, HW - 32, 98, 5); b.stroke(); b.setLineDash([]);
    // vignette
    const vg = b.createRadialGradient(x0 + HW / 2, 260, 120, x0 + HW / 2, 300, 520);
    vg.addColorStop(0, 'rgba(0,0,0,0)'); vg.addColorStop(1, 'rgba(0,0,0,.45)'); b.fillStyle = vg; b.fillRect(x0, 0, HW, H);
    b.restore();
  }
  // stone pillar between halls
  b.fillStyle = '#2e2620'; b.fillRect(HW, 0, HX[1] - HW, H);
  for (let y = 0; y < H; y += 16) { b.fillStyle = (y / 16) % 2 ? '#463c33' : '#3d342c'; b.fillRect(HW + 1, y + 1, HX[1] - HW - 2, 14); }
}
function rrB(b, x, y, w, h, r) { b.beginPath(); b.moveTo(x + r, y); b.arcTo(x + w, y, x + w, y + h, r); b.arcTo(x + w, y + h, x, y + h, r); b.arcTo(x, y + h, x, y, r); b.arcTo(x, y, x + w, y, r); b.closePath(); }

// ------------------------------------------------------------------ patrons (top-down)
function drawPatron(x, y, ang, seed, sc, you) {
  const R = rnd(seed * 7919 + 13);
  const skin = SKIN[(R() * SKIN.length) | 0], robe = you ? '#c9a25a' : ROBE[(R() * ROBE.length) | 0], hair = HAIR[(R() * HAIR.length) | 0];
  const hat = (R() * 6) | 0;
  g.save(); g.translate(x, y); g.rotate(ang); g.scale(sc, sc);
  g.fillStyle = 'rgba(0,0,0,.28)'; g.beginPath(); g.ellipse(2, 3, 17, 14, 0, 0, 7); g.fill();   // shadow
  g.fillStyle = '#3b2412'; rr(-15, -13, 12, 26, 3); g.fill();                                     // chair back
  g.fillStyle = robe; g.beginPath(); g.ellipse(0, 0, 10, 15, 0, 0, 7); g.fill();                   // shoulders
  g.strokeStyle = 'rgba(0,0,0,.35)'; g.lineWidth = 1; g.stroke();
  g.fillStyle = robe; g.beginPath(); g.ellipse(9, -10, 7, 3.6, 0.35, 0, 7); g.ellipse(9, 10, 7, 3.6, -0.35, 0, 7); g.fill(); // arms
  g.fillStyle = skin; g.beginPath(); g.arc(15, -11, 2.8, 0, 7); g.arc(15, 11, 2.8, 0, 7); g.fill();   // hands
  g.fillStyle = skin; g.beginPath(); g.arc(1, 0, 7.2, 0, 7); g.fill();                                // head
  if (hat === 0) { g.fillStyle = robe; g.beginPath(); g.arc(-1, 0, 7.6, Math.PI * 0.55, Math.PI * 1.45); g.fill(); g.beginPath(); g.arc(-2, 0, 6.5, 0, 7); g.fill(); } // hood
  else if (hat === 1) { g.fillStyle = '#2d2350'; g.beginPath(); g.arc(1, 0, 11, 0, 7); g.fill(); g.fillStyle = '#3d3170'; g.beginPath(); g.arc(0, 0, 6, 0, 7); g.fill(); g.fillStyle = '#f6d48a'; g.beginPath(); g.arc(-1, 0, 1.6, 0, 7); g.fill(); } // wizard hat
  else if (hat === 2) { g.fillStyle = '#8a8f96'; g.beginPath(); g.arc(0, 0, 7.4, 0, 7); g.fill(); g.fillStyle = '#ece3cf'; g.beginPath(); g.moveTo(-1, -6); g.lineTo(-3, -14); g.lineTo(3, -7); g.moveTo(-1, 6); g.lineTo(-3, 14); g.lineTo(3, 7); g.fill(); } // horned helm
  else if (hat === 3) { g.fillStyle = hair; g.beginPath(); g.arc(0, 0, 7.4, Math.PI * 0.5, Math.PI * 1.5); g.fill(); g.beginPath(); g.ellipse(-8, 0, 4, 2.4, 0, 0, 7); g.fill(); } // ponytail
  else if (hat === 4) { g.fillStyle = '#6b2f2f'; g.beginPath(); g.arc(0, 0, 7.6, Math.PI * 0.4, Math.PI * 1.6); g.fill(); g.fillStyle = '#d8b04a'; g.fillRect(-2, -1, 3, 2); } // bandana
  else { g.fillStyle = hair; g.beginPath(); g.arc(-1, 0, 7.4, Math.PI * 0.62, Math.PI * 1.38); g.fill(); g.fillStyle = skin; g.beginPath(); g.moveTo(1, -6); g.lineTo(-2, -11); g.lineTo(3, -7); g.moveTo(1, 6); g.lineTo(-2, 11); g.lineTo(3, 7); g.fill(); } // elf
  g.restore();
}
function bubble(x, y, w, lines, tone, tailX, tailY, meter) {
  g.save();
  g.fillStyle = tone === 'win' ? '#fff3c8' : tone === 'bad' ? '#ffe2d6' : '#fbf4e2';
  g.strokeStyle = tone === 'win' ? COL.gold : tone === 'bad' ? '#b5482b' : '#6b4a2e'; g.lineWidth = 1.4;
  const h = 6 + lines.length * 12 + (meter !== undefined ? 7 : 0);
  rr(x, y, w, h, 6); g.fill(); g.stroke();
  g.beginPath(); g.moveTo(Math.max(x + 8, Math.min(x + w - 18, tailX - 5)), y + h - 0.5); g.lineTo(tailX, tailY); g.lineTo(Math.max(x + 18, Math.min(x + w - 8, tailX + 5)), y + h - 0.5); g.fill();
  g.beginPath(); g.moveTo(Math.max(x + 8, Math.min(x + w - 18, tailX - 5)), y + h); g.lineTo(tailX, tailY); g.lineTo(Math.max(x + 18, Math.min(x + w - 8, tailX + 5)), y + h); g.stroke();
  g.font = '10.5px Georgia, serif'; g.textBaseline = 'top';
  lines.forEach((l, i) => { g.fillStyle = l.c || '#2b1d12'; g.font = (l.b ? 'bold ' : '') + '10.5px Georgia, serif'; g.fillText(l.t, x + 6, y + 4 + i * 12); });
  if (meter !== undefined) {   // think meter: nodes searched as a share of one full-strength (6000-node) move
    const my = y + h - 8, mw = w - 12;
    g.fillStyle = '#d9c597'; g.fillRect(x + 6, my, mw, 4);
    g.fillStyle = meter < 0.1 ? '#c8402f' : COL.prior; g.fillRect(x + 6, my, Math.max(1, mw * meter), 4);
  }
  g.restore();
  return h;
}

// ------------------------------------------------------------------ boards and tables
function drawBoard(t, bx, by, s, side, opts) {
  const own = t.board.own, win = new Uint8Array(SNC);
  if (t.winLine) for (const c of t.winLine) win[c] = 1;
  g.fillStyle = '#e9d7ad'; g.fillRect(bx - 2, by - 2, 17 * s + 4, 17 * s + 4);
  if (opts && opts.env && t.cert && !t.done) { g.fillStyle = 'rgba(42,120,214,.16)'; for (const c of t.cert.placedT()) g.fillRect(((c / SN) | 0) * s + bx, (16 - c % SN) * s + by, s, s); }
  g.strokeStyle = 'rgba(120,90,50,.45)'; g.lineWidth = s >= 10 ? 1 : 0.6; g.beginPath();
  for (let i = 0; i <= 17; i++) { g.moveTo(bx + i * s, by); g.lineTo(bx + i * s, by + 17 * s); g.moveTo(bx, by + i * s); g.lineTo(bx + 17 * s, by + i * s); }
  g.stroke();
  const mk = COL[side];
  for (let c = 0; c < SNC; c++) {
    if (!own[c] && !win[c]) continue;
    const px = bx + ((c / SN) | 0) * s, py = by + (16 - c % SN) * s;
    if (win[c]) { g.fillStyle = COL.gold; g.fillRect(px, py, s, s); }
    if (own[c] === 1) { g.fillStyle = mk; g.beginPath(); g.arc(px + s / 2, py + s / 2, s * 0.4, 0, 7); g.fill(); }
    else if (own[c] === 2) { g.fillStyle = '#2d2722'; g.beginPath(); g.arc(px + s / 2, py + s / 2, s * 0.4, 0, 7); g.fill(); g.fillStyle = 'rgba(255,255,255,.25)'; g.beginPath(); g.arc(px + s * 0.42, py + s * 0.4, s * 0.13, 0, 7); g.fill(); }
  }
  for (const [c, colr] of [[t.lastM, '#fff'], [t.lastB, '#f6d48a']]) if (c >= 0 && !t.done) {
    const px = bx + ((c / SN) | 0) * s, py = by + (16 - c % SN) * s;
    g.strokeStyle = colr; g.lineWidth = s >= 10 ? 2 : 1; g.strokeRect(px + 0.5, py + 0.5, s - 1, s - 1);
  }
  if (t.winLine && s >= 6) { g.save(); g.shadowColor = COL.gold; g.shadowBlur = 10; g.strokeStyle = '#fff2b0'; g.lineWidth = 1.5;
    for (const c of t.winLine) g.strokeRect(bx + ((c / SN) | 0) * s + 0.5, by + (16 - c % SN) * s + 0.5, s - 1, s - 1); g.restore(); }
}
function woodTable(x, y, w, h) {
  g.fillStyle = 'rgba(0,0,0,.35)'; rr(x + 4, y + 6, w, h, 12); g.fill();
  g.fillStyle = '#5a3a1f'; rr(x, y, w, h, 12); g.fill();
  const gr = g.createLinearGradient(x, y, x + w, y + h); gr.addColorStop(0, '#9a6a3c'); gr.addColorStop(1, '#7a4f2a');
  g.fillStyle = gr; rr(x + 4, y + 4, w - 8, h - 8, 9); g.fill();
  g.strokeStyle = 'rgba(60,35,15,.35)'; g.lineWidth = 1;
  for (let i = 1; i < 5; i++) { g.beginPath(); g.moveTo(x + 8, y + i * h / 5); g.lineTo(x + w - 8, y + i * h / 5 + 2); g.stroke(); }
}
function mug(x, y) { g.fillStyle = '#c9cdd2'; g.beginPath(); g.arc(x, y, 5, 0, 7); g.fill(); g.fillStyle = '#e8b54a'; g.beginPath(); g.arc(x, y, 3.6, 0, 7); g.fill(); g.fillStyle = '#fff6dc'; g.beginPath(); g.arc(x - 1, y - 1, 1.6, 0, 7); g.fill(); }
function candle(x, y, t) { g.fillStyle = '#efe4c8'; g.beginPath(); g.arc(x, y, 3.4, 0, 7); g.fill(); const f = 2 + Math.sin(t * 9 + x) * 0.6; g.fillStyle = '#ffcf5a'; g.beginPath(); g.arc(x, y, f, 0, 7); g.fill(); }

function makerBubble(h, t) {
  if (t.done) return t.result > 0 ? { tone: 'win', lines: [{ t: 'Snaky! I win!', b: 1, c: '#7a5a10' }, { t: `in ${t.makerMoves} moves` }] }
                                  : { tone: 'bad', lines: [{ t: 'Blocked! I lose.', b: 1, c: '#9a2f1f' }, { t: `after ${t.makerMoves} moves` }] };
  if (h.side === 'new') {
    return { tone: '', lines: [{ t: 'playbook page ' + t.cert.node.toLocaleString(), b: 1, c: '#1d4f8f' }, { t: `move ${t.makerMoves + 1} · win by 21` }] };
  }
  const dots = '.'.repeat(1 + ((frameNo >> 3) % 3));
  if (h.mode === 'fixed') return { tone: '', lines: [{ t: 'thinking' + dots, b: 1 }, { t: `depth ${t.depth}, 6000 nodes` }] };
  const al = t.allotUs >= 1000 ? (t.allotUs / 1000).toFixed(1) + ' ms' : Math.round(t.allotUs) + ' µs';
  if (!t.short) return { tone: '', lines: [{ t: 'thinking' + dots + ' ' + al, b: 1 }, { t: `looked ${t.depth} moves ahead` }] };
  if (t.depth === 0) return { tone: 'bad', lines: [{ t: 'out of time! ' + al, b: 1, c: '#a8381c' }, { t: 'no lookahead: gut move' }] };
  return { tone: 'bad', lines: [{ t: 'out of time! ' + al, b: 1, c: '#a8381c' }, { t: `only ${t.depth} move${t.depth > 1 ? 's' : ''} ahead` }] };
}
function drawSmallTable(h, t, x, y, w, hh, time) {
  const s = Math.max(6, Math.min(13, Math.floor(Math.min((hh - 36) / 17, (w - 130) / 17))));
  const pad = s >= 10 ? 12 : 9, bw = 17 * s, tw = bw + 2 * pad, th = tw, sc = s >= 10 ? 1.6 : 1.3;
  const tx = x + (w - tw) / 2 + 12, ty = y + hh - th - 10, my = ty + th / 2;
  drawPatron(tx - 12 * sc, my, 0, t.id * 2 + 1, sc, false);                 // Maker on the left
  drawPatron(tx + tw + 12 * sc, my, Math.PI, t.id * 2 + 2, sc, false);      // Breaker on the right
  woodTable(tx, ty, tw, th);
  drawBoard(t, tx + pad, ty + pad, s, h.side);
  mug(tx + tw - 6, ty + th - 4); candle(tx + 5, ty + 5, time);
  const b = makerBubble(h, t), bwid = 136;
  const meter = h.side === 'prior' && !t.done && h.mode === 'budget' ? Math.min(1, (t.nodes || 0) / 6000) : undefined;
  const bh = 6 + b.lines.length * 12 + (meter !== undefined ? 7 : 0);
  const bx = Math.max(x + 2, tx - 12 * sc - bwid + 40), by = Math.max(y - 6, my - 14 * sc - bh - 16);
  bubble(bx, by, bwid, b.lines, b.tone, tx - 12 * sc + 4, my - 9 * sc, meter);
  g.fillStyle = '#e8d7b0'; g.font = '10px Georgia, serif'; g.textBaseline = 'alphabetic';
  g.fillText(`table ${t.id + 1}`, tx + tw - 34, ty + th + 9);
}
function drawBigTable(h, t, x, y, w, hh, time) {
  const s = 12, bw = 17 * s, pad = 11, tw = bw + 2 * pad, th = tw;
  const tx = x + (w - tw) / 2 + 4, ty = y + 34;
  drawPatron(tx + tw / 2, ty - 8, Math.PI / 2, t.id * 2 + 1, 1.3, false);       // Maker across the table
  drawPatron(tx + tw / 2, ty + th + 10, -Math.PI / 2, 99, 1.3, true);           // you
  woodTable(tx, ty, tw, th);
  drawBoard(t, tx + pad, ty + pad, s, h.side, { env: showEnv });
  h.bigBoard = { x: tx + pad, y: ty + pad, s };
  const b = makerBubble(h, t);
  const meter = h.side === 'prior' && !t.done && h.mode === 'budget' ? Math.min(1, (t.nodes || 0) / 6000) : undefined;
  bubble(tx + tw / 2 - 26 - 140, y - 8, 140, b.lines, b.tone, tx + tw / 2 - 12, ty - 14, meter);
  const pt = playerTally[h.side === 'prior' ? 0 : 1];
  g.textBaseline = 'alphabetic'; g.textAlign = 'center';
  g.fillStyle = '#f6d48a'; g.font = 'bold 12px Georgia, serif';
  g.fillText('Table 1: YOU are the Breaker. Click a square.', x + w / 2, ty + th + 34);
  g.font = '11px Georgia, serif'; g.fillStyle = '#e8d7b0';
  g.fillText(h.side === 'new' ? `your games ${pt.g} · Maker won ${pt.w} · tint = playbook's safe zone` : `your games ${pt.g} · Maker won ${pt.w} · it gets ${fmtUs(h.allotMs() * 1000)} a move`, x + w / 2, ty + th + 48);
  g.textAlign = 'left';
}
function drawBackRoom(h, x0, time) {
  const N = h.tables.length, x = x0 + 22, y = 428, w = HW - 44, hh = 86;
  g.fillStyle = '#f0deb4'; g.font = '11px Georgia, serif'; g.textBaseline = 'alphabetic';
  g.fillText(`All ${N.toLocaleString()} table${N > 1 ? 's' : ''} tonight`, x, 423);
  const lg = [[COL.gold, 'won'], [COL.red, 'blocked'], ['#9a7a52', 'no result yet']];
  let lx = x + 160; for (const [c, l] of lg) { g.fillStyle = c; g.fillRect(lx, 415, 8, 8); g.fillStyle = '#f0deb4'; g.fillText(l, lx + 11, 423); lx += 22 + g.measureText(l).width; }
  if (h.side === 'prior' && h.mode === 'budget') { g.fillStyle = '#ffb27a'; g.beginPath(); g.arc(lx + 4, 419, 2.5, 0, 7); g.fill(); g.fillStyle = '#f0deb4'; g.fillText('ran out of time', lx + 10, 423); }
  let s = Math.floor(Math.sqrt(w * hh / N)); s = Math.max(2, Math.min(26, s));
  let cols = Math.floor(w / s); while (Math.ceil(N / cols) * s > hh && s > 2) { s--; cols = Math.floor(w / s); }
  const gap = s >= 3 ? 1 : 0, rows = Math.ceil(N / cols), oy = y + (hh - rows * s) / 2;
  for (let i = 0; i < N; i++) {
    const t = h.tables[i], px = x + (i % cols) * s, py = oy + ((i / cols) | 0) * s;
    g.fillStyle = t.last > 0 ? COL.gold : t.last < 0 ? COL.red : '#9a7a52';
    if (s >= 12) { rr(px + 1, py + 1, s - 2, s - 2, 3); g.fill(); g.fillStyle = 'rgba(40,24,10,.55)'; g.fillRect(px + s * .3, py + s * .3, s * .4, s * .4); }
    else g.fillRect(px, py, s - gap, s - gap);
    if (h.side === 'prior' && t.short && h.mode === 'budget' && s >= 4) { g.fillStyle = '#ffb27a'; const d = Math.max(1, s * 0.3); g.fillRect(px + (s - d) / 2 - gap / 2, py + (s - d) / 2 - gap / 2, d, d); }
    if (t === h.player) { g.strokeStyle = '#fff'; g.lineWidth = 1; g.strokeRect(px + 0.5, py + 0.5, s - 1, s - 1); }
  }
}
function fmtMs(ms) { return ms >= 100 ? ms.toFixed(0) : ms >= 1 ? ms.toFixed(1) : ms >= 0.01 ? ms.toFixed(3) : ms.toFixed(4); }
function fmtUs(us) { return us >= 1000 ? (us / 1000).toFixed(2) + ' ms' : us >= 10 ? us.toFixed(0) + ' µs' : us.toFixed(2) + ' µs'; }
function drawHUD(h, x0) {
  const x = x0 + 8, y = 526, w = HW - 16, hh = 108, st = h.st, prior = h.side === 'prior';
  g.fillStyle = 'rgba(0,0,0,.4)'; rr(x + 3, y + 4, w, hh, 7); g.fill();
  g.fillStyle = '#f1e2bf'; rr(x, y, w, hh, 7); g.fill();
  g.strokeStyle = prior ? COL.prior : COL.new; g.lineWidth = 3; rr(x, y, w, hh, 7); g.stroke();
  const ms = prior ? median(st.tickMs) : newFrameMs();
  // budget bar
  const bx = x + 12, by = y + 10, bw = 250, bh = 14, cap = 16.7 * 2;
  g.fillStyle = '#d9c597'; g.fillRect(bx, by, bw, bh);
  g.fillStyle = prior ? COL.prior : COL.new; g.fillRect(bx, by, bw * Math.min(1, ms / cap), bh);
  if (ms > cap) { g.fillStyle = '#c8402f'; g.fillRect(bx + bw - 6, by, 6, bh); }
  g.strokeStyle = '#2b1d12'; g.lineWidth = 2; g.beginPath(); g.moveTo(bx + bw / 2, by - 3); g.lineTo(bx + bw / 2, by + bh + 3); g.stroke();
  g.fillStyle = '#2b1d12'; g.font = '9px Georgia, serif'; g.textBaseline = 'alphabetic'; g.fillText('16.7 ms frame', bx + bw / 2 + 3, by + bh + 9);
  g.font = 'bold 14px Consolas, monospace'; g.fillText(`${fmtMs(ms)} ms`, bx + bw + 10, by + 12);
  g.font = '12px Georgia, serif'; g.fillText('Maker AI per frame (median)', bx + bw + 92, by + 12);
  const L = [];
  const pct = st.games ? (100 * st.wins / st.games).toFixed(0) + '%' : '-';
  const mv = st.wins ? (st.sumWinMoves / st.wins).toFixed(1) : '-';
  if (prior) {
    const per = st.moves ? st.aiMs * 1000 / st.moves : 0;
    if (h.mode === 'fixed') L.push(`each move: full 6000-node search, ${fmtUs(per)} · no budget, so ${Math.ceil(cfg.n / cfg.k)} moves = ${fmtMs(ms)} ms per frame`);
    else L.push(`each move gets ${fmtUs(h.allotMs() * 1000)} · used ${fmtUs(per)} · looked ${st.moves ? (st.depthSum / st.moves).toFixed(1) : '-'} moves ahead · out of time ${st.moves ? (100 * st.shortMoves / st.moves).toFixed(0) : '-'}%`);
    L.push(calib ? `full strength (6000 nodes) = ${calib.msPerMove.toFixed(1)} ms per move → ${Math.max(0, Math.floor(16.7 / calib.msPerMove))} table${Math.floor(16.7 / calib.msPerMove) === 1 ? '' : 's'} fit one frame` : 'timing full strength...');
  } else {
    L.push(`each move: ${certUs === null ? '...' : fmtUs(certUs)} (one table lookup per Breaker reply, timed in batches), no search`);
    const tm = median(st.tickMs), mpt = Math.ceil(cfg.n / cfg.k);
    const fit = tm >= 0.5 ? Math.floor(mpt * 16.7 / tm) : certUs ? Math.floor(16700 / certUs) : 0;
    L.push(`→ about ${fit.toLocaleString()} tables' moves would fit one frame (linear estimate${tm >= 0.5 ? ' from this frame time' : ''}) · data ${CERT_KB} KB`);
  }
  L.push(`won ${st.wins}/${st.games} (${pct}) vs ${BRK_NAMES[h.kind]} · moves to win avg ${mv}, max ${st.maxWinMoves || '-'}${prior ? '' : ' (proved ≤ 21)'}`);
  let long = 0; for (const t of h.tables) if (!t.done && t.makerMoves > 21) long++;
  L.push((prior ? '' : `proof invariant broken: ${st.breaks} times · `) + `games running past move 21 right now: ${long}`);
  g.font = '12px Georgia, serif'; g.fillStyle = '#2b1d12';
  L.forEach((l, i) => { if (i === 2) g.font = 'bold 12px Georgia, serif'; else g.font = '12px Georgia, serif'; g.fillText(l, x + 12, y + 46 + i * 16); });
}
function drawBanner(h, x0) {
  const prior = h.side === 'prior';
  const text = prior ? 'PRIOR BEST: α-β search (TT + threat ordering)' : 'NEW: proof-as-playbook (21-move certificate)';
  g.font = 'bold 15px Georgia, serif';
  const tw = g.measureText(text).width + 30, x = x0 + HW - tw - 14, y = 10;
  g.strokeStyle = '#1a1008'; g.lineWidth = 2; g.beginPath(); g.moveTo(x + 14, 0); g.lineTo(x + 14, y); g.moveTo(x + tw - 14, 0); g.lineTo(x + tw - 14, y); g.stroke();
  g.fillStyle = 'rgba(0,0,0,.45)'; rr(x + 2, y + 3, tw, 30, 5); g.fill();
  g.fillStyle = '#2a1a0d'; rr(x, y, tw, 30, 5); g.fill();
  g.strokeStyle = prior ? COL.prior : COL.new; g.lineWidth = 2.5; rr(x, y, tw, 30, 5); g.stroke();
  g.fillStyle = prior ? COL.prior : '#6aa8f0'; g.textBaseline = 'middle'; g.fillText(text, x + 15, y + 16);
  g.textBaseline = 'alphabetic';
}
function drawLights(time) {
  g.save(); g.globalCompositeOperation = 'lighter';
  const spots = [[HX[0] + 74, 58, 120, 0.5], [HX[0] + 320, 240, 260, 0.13], [HX[1] + 320, 240, 260, 0.13], [HX[1] + 560, 50, 120, 0.12]];
  spots.forEach(([x, y, r, a], i) => {
    const f = a * (0.85 + 0.15 * Math.sin(time * (i === 0 ? 11 : 3) + i * 2) * Math.sin(time * 7.3 + i));
    const gr = g.createRadialGradient(x, y, 0, x, y, r); gr.addColorStop(0, `rgba(255,170,70,${f})`); gr.addColorStop(1, 'rgba(255,140,40,0)');
    g.fillStyle = gr; g.fillRect(x - r, y - r, 2 * r, 2 * r);
  });
  g.restore();
  // hearth fire
  const hx = HX[0] + 34 + 40;
  for (let i = 0; i < 6; i++) {
    const fx = hx - 16 + i * 6.5, fh = 12 + 8 * Math.abs(Math.sin(time * 6 + i * 1.7));
    g.fillStyle = i % 2 ? '#ffb43a' : '#ff7a22'; g.beginPath(); g.moveTo(fx - 4, 56); g.quadraticCurveTo(fx, 56 - fh * 1.3, fx + 4, 56); g.fill();
  }
  g.fillStyle = '#ffe28a'; g.beginPath(); g.ellipse(hx, 53, 12, 3, 0, 0, 7); g.fill();
}
let showEnv = false;
function draw() {
  const time = frameNo / 60;
  g.drawImage(bgCanvas, 0, 0);
  drawLights(time);
  halls.forEach((h, hi) => {
    const x0 = HX[hi];
    g.save(); g.beginPath(); g.rect(x0, 0, HW, H); g.clip();
    const fy = 70, fh = 330;
    if (cfg.play) {
      drawBigTable(h, h.tables[0], x0, fy, 336, fh, time);
      for (let j = 1; j <= 2 && j < h.tables.length; j++) drawSmallTable(h, h.tables[j], x0 + 336, fy + (j - 1) * 165, 296, 163, time);
    } else {
      const F = Math.min(4, h.tables.length);
      const cols = F === 1 ? 1 : 2, rows = F <= 2 ? 1 : 2, cw = HW / cols, ch = fh / rows;
      for (let j = 0; j < F; j++) {
        const cx = x0 + (j % cols) * cw, cy = fy + ((j / cols) | 0) * ch;
        drawSmallTable(h, h.tables[j], cx, cy, cw, ch, time);
      }
    }
    drawBackRoom(h, x0, time);
    drawHUD(h, x0);
    drawBanner(h, x0);
    g.restore();
  });
}

// ------------------------------------------------------------------ main loop
function step() {   // one rendered frame of simulation
  frameNo++;
  const ready = halls.every(h => !h.queue || h.qi >= h.queue.length);
  if (ready && frameNo % cfg.every === 0) {
    for (const h of halls) h.makerPhase(tickNo);
    tickNo++;
  }
  for (const h of halls) h.breakerPhase(cfg.kind === BRK_SEARCH ? 12 : 40);
  if (frameNo % 90 === 1) { certUs = measureCert(); snapTonight(); renderTonight(); renderLadder(); }
}
function loop() {
  step(); draw();
  requestAnimationFrame(loop);
}
// ?frames=N: simulate N frames synchronously at load (real clock), draw the last one and stop. Headless Chrome's
// virtual time does not drive requestAnimationFrame reliably, and this way the screenshot always terminates.
function runFrames() {
  for (let i = 0; i < MAX_FRAMES; i++) step();
  draw(); snapTonight(); renderTonight(); renderLadder();
  ui('warn').textContent = `stopped after ${MAX_FRAMES} frames (?frames=${MAX_FRAMES})`;
}

// ------------------------------------------------------------------ controls
function onCfg() { snapTonight(); applyCfg(); renderTonight(); renderLadder(); }
ui('nslider').oninput = e => { cfg.n = 1 << +e.target.value; onCfg(); };
ui('brk').onchange = e => { cfg.kind = +e.target.value; onCfg(); };
ui('pace').onchange = e => { cfg.k = +e.target.value; onCfg(); };
ui('pmode').onchange = e => { cfg.mode = e.target.value; onCfg(); };
ui('speed').onchange = e => { cfg.every = +e.target.value; for (const h of halls) h.holdTicks = Math.round(1.4 * 60 / cfg.every); };
ui('play').onclick = () => { cfg.play = !cfg.play; showEnv = cfg.play; onCfg(); };
ui('reset').onclick = () => { tonight.clear(); applyCfg(); renderTonight(); };
cv.addEventListener('click', e => {
  if (!cfg.play) return;
  const r = cv.getBoundingClientRect(), mx = (e.clientX - r.left) * W / r.width, my = (e.clientY - r.top) * H / r.height;
  halls.forEach((h, hi) => {
    const b = h.bigBoard; if (!b) return;
    const x = Math.floor((mx - b.x) / b.s), y = 16 - Math.floor((my - b.y) / b.s);
    if (x < 0 || x > 16 || y < 0 || y > 16) return;
    const t = h.tables[0];
    if (t.done) { t.start(); h.playerMaker(t); return; }
    if (h.playerMove(t, x * SN + y) && t.done) { playerTally[hi].g++; if (t.result > 0) playerTally[hi].w++; }
  });
});
cv.addEventListener('mousemove', e => {
  if (!cfg.play) { cv.style.cursor = 'default'; return; }
  const r = cv.getBoundingClientRect(), mx = (e.clientX - r.left) * W / r.width, my = (e.clientY - r.top) * H / r.height;
  cv.style.cursor = halls.some(h => h.bigBoard && mx >= h.bigBoard.x && mx < h.bigBoard.x + 17 * h.bigBoard.s && my >= h.bigBoard.y && my < h.bigBoard.y + 17 * h.bigBoard.s) ? 'pointer' : 'default';
});

// ------------------------------------------------------------------ start
presets();
showEnv = cfg.play;
buildBackground();
calib = calibrateSearch(2);
{ const t = new Table(0, 'new', CERT, BRK_GREEDY); while (!t.done) { t.applyMaker(t.makerDecideNew()); if (!t.done) t.breakerReply(); } certLog.push(t.replies.slice()); }
for (let i = 0; i < 4; i++) certUs = measureCert();   // JIT warm-up, keep the last
applyCfg();
renderLadder();
if (MAX_FRAMES) runFrames(); else requestAnimationFrame(loop);
