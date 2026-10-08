// Slime Lab core: shared by index.html (inlined by build.py) and bench.js (Node).
// Grid: N^3 voxels of the periodic unit cube R^3/Z^3, index = (z*N + y)*N + x.
// NEW   = closed-form minimizer of Family 354 (Theorem intro:main): ball / tube / slab / complements,
//         voxelized by ranking voxels by periodic distance and keeping exactly k.
// PRIOR = volume-preserving threshold dynamics (MBO, Ruuth-Wetton): periodic Gaussian, keep top k,
//         coarse-to-fine (N/2 grid first, prolong, polish at N), started from smooth noise.
(function (root) {
  'use strict';
  const PI = Math.PI;
  const T_BALL_TUBE = 4 * PI / 81, T_TUBE_SLAB = 1 / PI;
  const now = (typeof performance !== 'undefined' && performance.now) ? () => performance.now() : () => Date.now();

  // ------------------------------------------------------------ the paper's profile (eq. intro:profile)
  function I_unit(V) {
    const v = Math.min(V, 1 - V);
    if (v <= 0) return 0;
    return Math.min(Math.cbrt(36 * PI) * Math.pow(v, 2 / 3), 2 * Math.sqrt(PI * v), 2);
  }
  function phaseOf(V) { const v = Math.min(V, 1 - V); return v < T_BALL_TUBE ? 'ball' : v < T_TUBE_SLAB ? 'tube' : 'slab'; }
  function theoremShapeName(V) {
    const p = phaseOf(V);
    if (p === 'slab') return 'slab';
    return V > 0.5 ? (p === 'ball' ? 'ball-shaped bubble' : 'tube-shaped tunnel') : p;
  }

  // ------------------------------------------------------------ seeded RNG + smooth noise (same physical field at every N)
  function rng(seed) {
    let a = seed >>> 0;
    return () => { a = (a + 0x6D2B79F5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
  }
  // 16^3 lattice of uniform values, periodic trilinear upsampling, Gaussian of 1/32 cell (as demos\L2-blob-ladder make_noise).
  function makeNoise(N, seed) {
    const G = 16, R = rng(seed * 2654435761 + 12345), lat = new Float32Array(G * G * G);
    for (let i = 0; i < lat.length; i++) lat[i] = R();
    const n = N * N * N, out = new Float32Array(n);
    const i0 = new Int32Array(N), i1 = new Int32Array(N), tt = new Float32Array(N);
    for (let i = 0; i < N; i++) { const g = (i + 0.5) / N * G, f = Math.floor(g); tt[i] = g - f; i0[i] = ((f % G) + G) % G; i1[i] = (i0[i] + 1) % G; }
    const L = (x, y, z) => lat[(z * G + y) * G + x];
    for (let z = 0; z < N; z++) for (let y = 0; y < N; y++) {
      const y0 = i0[y], y1 = i1[y], ty = tt[y], z0 = i0[z], z1 = i1[z], tz = tt[z], row = (z * N + y) * N;
      for (let x = 0; x < N; x++) {
        const x0 = i0[x], x1 = i1[x], tx = tt[x];
        const c00 = L(x0, y0, z0) + (L(x1, y0, z0) - L(x0, y0, z0)) * tx, c10 = L(x0, y1, z0) + (L(x1, y1, z0) - L(x0, y1, z0)) * tx;
        const c01 = L(x0, y0, z1) + (L(x1, y0, z1) - L(x0, y0, z1)) * tx, c11 = L(x0, y1, z1) + (L(x1, y1, z1) - L(x0, y1, z1)) * tx;
        const c0 = c00 + (c10 - c00) * ty, c1 = c01 + (c11 - c01) * ty;
        out[row + x] = c0 + (c1 - c0) * tz;
      }
    }
    const tmp = new Float32Array(n);
    gaussFIR(N, out, tmp, N / 32);
    return out;
  }

  // ------------------------------------------------------------ periodic Gaussian blurs (in place on a; b is scratch)
  // (1) triple box blur with Kovesi radii (the usual fast game-dev Gaussian; O(1) per voxel per pass)
  function boxRadii(sigma, passes) {
    const wIdeal = Math.sqrt(12 * sigma * sigma / passes + 1);
    let wl = Math.floor(wIdeal); if (wl % 2 === 0) wl--;
    const wu = wl + 2, m = Math.round((12 * sigma * sigma - passes * wl * wl - 4 * passes * wl - 3 * passes) / (-4 * wl - 4));
    const r = []; for (let i = 0; i < passes; i++) r.push(((i < m ? wl : wu) - 1) / 2);
    return r;
  }
  function boxX(N, src, dst, r) {
    const inv = 1 / (2 * r + 1), rows = N * N;
    for (let row = 0; row < rows; row++) {
      const o = row * N; let s = 0;
      for (let d = -r; d <= r; d++) s += src[o + ((d % N) + N) % N];
      for (let i = 0; i < N; i++) {
        dst[o + i] = s * inv;
        let ip = i + r + 1; if (ip >= N) ip -= N;
        let im = i - r; if (im < 0) im += N;
        s += src[o + ip] - src[o + im];
      }
    }
  }
  // slide along an axis with stride S over `count`=N rows, each row a contiguous run of L values
  function boxStrided(N, src, dst, r, base, S, L, acc) {
    const inv = 1 / (2 * r + 1);
    acc.fill(0, 0, L);
    for (let d = -r; d <= r; d++) { const o = base + (((d % N) + N) % N) * S; for (let q = 0; q < L; q++) acc[q] += src[o + q]; }
    for (let j = 0; j < N; j++) {
      const o = base + j * S;
      for (let q = 0; q < L; q++) dst[o + q] = acc[q] * inv;
      let jp = j + r + 1; if (jp >= N) jp -= N;
      let jm = j - r; if (jm < 0) jm += N;
      const oa = base + jp * S, or = base + jm * S;
      for (let q = 0; q < L; q++) acc[q] += src[oa + q] - src[or + q];
    }
  }
  function gaussBox(N, a, b, sigma, acc) {
    const radii = boxRadii(sigma, 3), NN = N * N;
    acc = acc || new Float32Array(NN);
    // x: a->b->a->b ; then y and z continue ping-pong. 9 passes total, so result ends in b; copy back.
    let s = a, d = b;
    for (const r of radii) { if (r > 0) { boxX(N, s, d, r); const t = s; s = d; d = t; } }
    for (const r of radii) { if (r > 0) { for (let z = 0; z < N; z++) boxStrided(N, s, d, r, z * NN, N, N, acc); const t = s; s = d; d = t; } }
    for (const r of radii) { if (r > 0) { boxStrided(N, s, d, r, 0, NN, NN, acc); const t = s; s = d; d = t; } }
    if (s !== a) a.set(s);
  }
  // (2) exact truncated Gaussian FIR (radius ceil(3 sigma)), periodic
  const firCache = new Map();
  function firWeights(sigma) {
    const key = sigma.toFixed(6);
    if (firCache.has(key)) return firCache.get(key);
    const R = Math.max(1, Math.ceil(3 * sigma)), w = new Float32Array(2 * R + 1); let s = 0;
    for (let t = -R; t <= R; t++) { w[t + R] = Math.exp(-0.5 * t * t / (sigma * sigma)); s += w[t + R]; }
    for (let t = 0; t < w.length; t++) w[t] /= s;
    const res = { R, w }; firCache.set(key, res); return res;
  }
  function gaussFIR(N, a, b, sigma) {
    const { R, w } = firWeights(sigma), NN = N * N, wrap = new Int32Array(N + 2 * R);
    for (let t = 0; t < N + 2 * R; t++) wrap[t] = (((t - R) % N) + N) % N;
    // x
    for (let row = 0; row < NN; row++) {
      const o = row * N;
      for (let i = 0; i < N; i++) { let s = 0; for (let t = 0; t <= 2 * R; t++) s += w[t] * a[o + wrap[i + t]]; b[o + i] = s; }
    }
    // y: b -> a
    for (let z = 0; z < N; z++) {
      const base = z * NN;
      for (let j = 0; j < N; j++) {
        const o = base + j * N;
        for (let i = 0; i < N; i++) a[o + i] = 0;
        for (let t = 0; t <= 2 * R; t++) { const wt = w[t], oo = base + wrap[j + t] * N; for (let i = 0; i < N; i++) a[o + i] += wt * b[oo + i]; }
      }
    }
    // z: a -> b -> copy to a
    for (let k = 0; k < N; k++) {
      const o = k * NN;
      for (let q = 0; q < NN; q++) b[o + q] = 0;
      for (let t = 0; t <= 2 * R; t++) { const wt = w[t], oo = wrap[k + t] * NN; for (let q = 0; q < NN; q++) b[o + q] += wt * a[oo + q]; }
    }
    a.set(b);
  }

  // ------------------------------------------------------------ exact top-k by radix select on float bits
  // out[i] = 1 for exactly k allowed voxels with the largest u (ties broken by index). mask[i]=1 forbids i.
  // Returns the number of voxels whose value in `out` changed.
  const H = new Int32Array(2048);
  function selectTopK(u, n, k, out, mask, keys) {
    const U = new Uint32Array(u.buffer, u.byteOffset, n);
    H.fill(0);
    for (let i = 0; i < n; i++) {
      const b = U[i];
      const key = (b & 0x80000000) ? (~b >>> 0) : ((b | 0x80000000) >>> 0);
      keys[i] = key;
      if (!mask || !mask[i]) H[key >>> 21]++;
    }
    let need = k, b1 = 2047;
    for (; b1 > 0; b1--) { if (H[b1] >= need) break; need -= H[b1]; }
    H.fill(0);
    for (let i = 0; i < n; i++) { if (mask && mask[i]) continue; const key = keys[i]; if ((key >>> 21) === b1) H[(key >>> 10) & 2047]++; }
    let b2 = 2047;
    for (; b2 > 0; b2--) { if (H[b2] >= need) break; need -= H[b2]; }
    const pre = (b1 << 11) | b2;
    H.fill(0, 0, 1024);
    for (let i = 0; i < n; i++) { if (mask && mask[i]) continue; const key = keys[i]; if ((key >>> 10) === pre) H[key & 1023]++; }
    let b3 = 1023;
    for (; b3 > 0; b3--) { if (H[b3] >= need) break; need -= H[b3]; }
    const T = ((pre << 10) | b3) >>> 0;
    let ties = need, changed = 0;
    for (let i = 0; i < n; i++) {
      let v = 0;
      if (!mask || !mask[i]) { const key = keys[i]; if (key > T) v = 1; else if (key === T && ties > 0) { v = 1; ties--; } }
      if (out[i] !== v) { out[i] = v; changed++; }
    }
    return changed;
  }

  // ------------------------------------------------------------ obstacle: a pillar along y (vertical in the render)
  function pillarMask(N, radius, cx, cz) {
    const m = new Uint8Array(N * N * N);
    for (let z = 0; z < N; z++) for (let x = 0; x < N; x++) {
      let dx = (x + 0.5) / N - cx, dz = (z + 0.5) / N - cz;
      dx -= Math.round(dx); dz -= Math.round(dz);
      if (dx * dx + dz * dz < radius * radius) for (let y = 0; y < N; y++) m[(z * N + y) * N + x] = 1;
    }
    return m;
  }

  // ------------------------------------------------------------ NEW: closed-form minimizer
  // opt: {c:[cx,cy,cz], tubeAxis:0|1|2, slabAxis:0|1|2, mask}
  function ClosedForm(N) {
    const n = N * N * N;
    this.N = N; this.n = n; this.chi = new Uint8Array(n); this.s = new Float32Array(n); this.keys = new Uint32Array(n);
    this.d2 = [new Float32Array(N), new Float32Array(N), new Float32Array(N)];
  }
  ClosedForm.prototype.build = function (k, opt) {
    opt = opt || {};
    const N = this.N, n = this.n, V = k / n, c = opt.c || [0.5, 0.5, 0.5];
    const ph = phaseOf(V), comp = V > 0.5 && ph !== 'slab', sg = comp ? 1 : -1; // a slab of width V is its own complement type
    for (let a = 0; a < 3; a++) for (let i = 0; i < N; i++) { let d = (i + 0.5) / N - c[a]; d -= Math.round(d); this.d2[a][i] = d * d; }
    const [DX, DY, DZ] = this.d2, s = this.s;
    // which axes count: ball = all three; tube = the two axes orthogonal to tubeAxis; slab = slabAxis only
    const ta = opt.tubeAxis == null ? 0 : opt.tubeAxis, sa = opt.slabAxis == null ? 1 : opt.slabAxis;
    const ux = ph === 'ball' || (ph === 'tube' && ta !== 0) || (ph === 'slab' && sa === 0) ? 1 : 0;
    const uy = ph === 'ball' || (ph === 'tube' && ta !== 1) || (ph === 'slab' && sa === 1) ? 1 : 0;
    const uz = ph === 'ball' || (ph === 'tube' && ta !== 2) || (ph === 'slab' && sa === 2) ? 1 : 0;
    let p = 0;
    for (let z = 0; z < N; z++) { const fz = uz * DZ[z]; for (let y = 0; y < N; y++) { const fyz = fz + uy * DY[y]; for (let x = 0; x < N; x++) s[p++] = sg * (ux * DX[x] + fyz); } }
    selectTopK(s, n, k, this.chi, opt.mask || null, this.keys);
    return this.chi;
  };

  // ------------------------------------------------------------ shared area estimator: integral of |grad(G_1 * chi)| (sigma = 1 voxel)
  function AreaEst(N) { const n = N * N * N; this.N = N; this.n = n; this.f = new Float32Array(n); this.t = new Float32Array(n); }
  AreaEst.prototype.raw = function (chi, mask, maskVal) {
    const N = this.N, n = this.n, f = this.f;
    for (let i = 0; i < n; i++) f[i] = (mask && mask[i]) ? maskVal : chi[i];
    gaussFIR(N, f, this.t, 1.0);
    const NN = N * N; let s = 0;
    for (let z = 0; z < N; z++) {
      const zp = ((z + 1) % N) * NN, zm = ((z + N - 1) % N) * NN, zo = z * NN;
      for (let y = 0; y < N; y++) {
        const yp = ((y + 1) % N) * N, ym = ((y + N - 1) % N) * N, yo = y * N;
        for (let x = 0; x < N; x++) {
          const xp = x + 1 === N ? 0 : x + 1, xm = x === 0 ? N - 1 : x - 1;
          const gx = f[zo + yo + xp] - f[zo + yo + xm], gy = f[zo + yp + x] - f[zo + ym + x], gz = f[zp + yo + x] - f[zm + yo + x];
          s += Math.sqrt(gx * gx + gy * gy + gz * gz);
        }
      }
    }
    return s * 0.5 / NN;
  };
  // plain area of the slime boundary; with an obstacle, the area of the slime/air boundary only
  // (slime touching the pillar costs nothing extra, i.e. a 90 degree contact angle):
  // A_rel(E) = (A(E) + A(E u P) - A(P)) / 2
  AreaEst.prototype.area = function (chi, mask) {
    if (!mask) return this.raw(chi, null, 0);
    if (this._maskRef !== mask) { this._maskRef = mask; this._aP = this.raw(new Uint8Array(this.n), mask, 1); }
    return (this.raw(chi, mask, 0) + this.raw(chi, mask, 1) - this._aP) / 2;
  };

  // ------------------------------------------------------------ shape classification (port of demos\07 / L2 classify)
  function wraps(chi, N, val) {
    let w = 0;
    for (let axis = 0; axis < 3; axis++) {
      let any = false;
      for (let a = 0; a < N && !any; a++) for (let b = 0; b < N && !any; b++) {
        let all = true;
        for (let q = 0; q < N && all; q++) {
          const id = axis === 0 ? (a * N + b) * N + q : axis === 1 ? (a * N + q) * N + b : (q * N + a) * N + b;
          all = chi[id] === val;
        }
        any = all;
      }
      w += any ? 1 : 0;
    }
    return w;
  }
  function components(chi, N, val, minSize) {
    const n = N * N * N, lab = new Int32Array(n).fill(-1), stack = new Int32Array(n);
    let comps = 0;
    for (let s0 = 0; s0 < n; s0++) {
      if (chi[s0] !== val || lab[s0] >= 0) continue;
      let sp = 0, size = 0; stack[sp++] = s0; lab[s0] = comps;
      while (sp > 0) {
        const id = stack[--sp]; size++;
        const x = id % N, y = ((id / N) | 0) % N, z = (id / (N * N)) | 0;
        const nb = [(z * N + y) * N + (x + 1) % N, (z * N + y) * N + (x + N - 1) % N, (z * N + (y + 1) % N) * N + x,
          (z * N + (y + N - 1) % N) * N + x, (((z + 1) % N) * N + y) * N + x, (((z + N - 1) % N) * N + y) * N + x];
        for (const q of nb) if (chi[q] === val && lab[q] < 0) { lab[q] = comps; stack[sp++] = q; }
      }
      if (size >= (minSize || 8)) comps++;
    }
    return comps;
  }
  // with a mask, masked voxels are marked 2 so they belong to neither slime nor air
  function classify(chi, N, mask) {
    let c = chi;
    if (mask) { c = new Uint8Array(chi); for (let i = 0; i < c.length; i++) if (mask[i]) c[i] = 2; }
    const w1 = wraps(c, N, 1), w0 = wraps(c, N, 0), c1 = components(c, N, 1), c0 = components(c, N, 0);
    const cnt = (k, one, many) => k === 1 ? one : k + ' ' + many;
    if (w1 === 0 && w0 === 3) return { name: cnt(c1, 'ball', 'blobs'), phase: c1 === 1 ? 'ball' : null, comp: false };
    if (w1 === 1 && w0 === 3) return { name: cnt(c1, 'tube', 'tubes'), phase: c1 === 1 ? 'tube' : null, comp: false };
    if (w1 === 2 && w0 === 2) return { name: cnt(c1, 'slab', 'slabs'), phase: c1 === 1 ? 'slab' : null, comp: false };
    if (w1 === 3 && w0 === 1) return { name: c0 === 1 ? 'tube-shaped tunnel' : c0 + ' tunnels', phase: c0 === 1 ? 'tube' : null, comp: true };
    if (w1 === 3 && w0 === 0) return { name: c0 === 1 ? 'ball-shaped bubble' : c0 + ' bubbles', phase: c0 === 1 ? 'ball' : null, comp: true };
    return { name: 'tangle (wraps ' + w1 + '/' + w0 + ' axes)', phase: null, comp: null };
  }
  function shapeMatches(info, V) { return info.phase === phaseOf(V) && (info.phase === 'slab' || info.comp === (V > 0.5)); }

  // ------------------------------------------------------------ PRIOR: volume-preserving MBO on one grid
  function MBO(N, sigmaVox, mask, blur) {
    const n = N * N * N;
    this.N = N; this.n = n; this.sigma = sigmaVox; this.mask = mask || null; this.blur = blur || 'box';
    this.chi = new Uint8Array(n); this.u = new Float32Array(n); this.t = new Float32Array(n); this.keys = new Uint32Array(n);
    this.acc = new Float32Array(N * N); this.k = 0; this.iters = 0;
  }
  MBO.prototype.smooth = function (u) {
    if (this.blur === 'fir') gaussFIR(this.N, u, this.t, this.sigma); else gaussBox(this.N, u, this.t, this.sigma, this.acc);
  };
  MBO.prototype.initFrom = function (field, k) { this.k = k; this.iters = 0; return selectTopK(field, this.n, k, this.chi, this.mask, this.keys); };
  MBO.prototype.step = function () {
    const n = this.n, u = this.u, chi = this.chi, m = this.mask;
    if (m) for (let i = 0; i < n; i++) u[i] = m[i] ? 0.5 : chi[i]; // obstacle half-filled: 90 degree contact
    else for (let i = 0; i < n; i++) u[i] = chi[i];
    this.smooth(u);
    this.iters++;
    return selectTopK(u, n, this.k, chi, m, this.keys);
  };

  // periodic trilinear prolongation of a coarse indicator into a float field on the fine grid
  function prolong(src, Nc, dst, N) {
    const i0 = new Int32Array(N), i1 = new Int32Array(N), tt = new Float32Array(N);
    for (let i = 0; i < N; i++) { const g = (i + 0.5) / N * Nc - 0.5, f = Math.floor(g); tt[i] = g - f; i0[i] = ((f % Nc) + Nc) % Nc; i1[i] = (i0[i] + 1) % Nc; }
    const L = (x, y, z) => src[(z * Nc + y) * Nc + x];
    for (let z = 0; z < N; z++) for (let y = 0; y < N; y++) {
      const y0 = i0[y], y1 = i1[y], ty = tt[y], z0 = i0[z], z1 = i1[z], tz = tt[z], row = (z * N + y) * N;
      for (let x = 0; x < N; x++) {
        const x0 = i0[x], x1 = i1[x], tx = tt[x];
        const c00 = L(x0, y0, z0) + (L(x1, y0, z0) - L(x0, y0, z0)) * tx, c10 = L(x0, y1, z0) + (L(x1, y1, z0) - L(x0, y1, z0)) * tx;
        const c01 = L(x0, y0, z1) + (L(x1, y0, z1) - L(x0, y0, z1)) * tx, c11 = L(x0, y1, z1) + (L(x1, y1, z1) - L(x0, y1, z1)) * tx;
        const c0 = c00 + (c10 - c00) * ty, c1 = c01 + (c11 - c01) * ty;
        dst[row + x] = c0 + (c1 - c0) * tz;
      }
    }
  }

  // ------------------------------------------------------------ PRIOR BEST solver: coarse-to-fine MBO, stepped one unit at a time
  // opts: {N, V, seed, sigmaPhys, coarse (bool), coarseSigmaPhys, pillar:{r,cx,cz} | null, blur, warmChi (Uint8Array) }
  // Every call to step() does one timed unit of work and returns its ms. No oracle is used to stop:
  // the coarse stage ends when <= 0.05 % of its voxels change (or 40 its); the fine stage reports
  // `settled` when nothing changes, or <= 0.05 % changes for 5 iterations in a row (cap 400).
  function Solver(o) {
    this.o = o; const N = o.N;
    this.N = N; this.n = N * N * N;
    this.k = Math.round(o.V * this.n);
    this.maskF = o.pillar ? pillarMask(N, o.pillar.r, o.pillar.cx, o.pillar.cz) : null;
    this.fine = new MBO(N, (o.sigmaPhys || 1 / 16) * N, this.maskF, o.blur);
    this.useCoarse = o.coarse !== false && !o.warmChi;
    if (this.useCoarse) {
      const Nc = o.Nc || (N >> 1);
      this.Nc = Nc;
      this.maskC = o.pillar ? pillarMask(Nc, o.pillar.r, o.pillar.cx, o.pillar.cz) : null;
      this.coarseM = new MBO(Nc, (o.coarseSigmaPhys || o.sigmaPhys || 1 / 16) * Nc, this.maskC, o.blur);
      this.kc = Math.round(o.V * Nc * Nc * Nc);
    }
    this.stage = 'init'; this.ms = 0; this.iters = 0; this.calm = 0; this.settled = false; this.lastChanged = -1;
    this.noise = o.warmChi ? null : (o.noise || makeNoise(this.useCoarse ? this.Nc : N, o.seed || 1));
  }
  Solver.prototype.setK = function (k) { // pour more slime / drain: continue from the current shape at the fine level
    this.k = k; this.fine.k = k; this.settled = false; this.calm = 0;
    if (this.stage === 'coarse' || this.stage === 'init') { this.kc = Math.round(k / this.n * this.Nc ** 3); if (this.coarseM) this.coarseM.k = this.kc; }
  };
  Solver.prototype.current = function () { return this.stage === 'coarse' ? { chi: this.coarseM.chi, N: this.Nc, mask: this.maskC } : { chi: this.fine.chi, N: this.N, mask: this.maskF }; };
  Solver.prototype.step = function () {
    const t0 = now();
    let ch = 0;
    if (this.stage === 'init') {
      if (this.o.warmChi) { this.fine.chi.set(this.o.warmChi); this.fine.k = this.k; this.fine.iters = 0; this.stage = 'fine'; }
      else if (this.useCoarse) { this.coarseM.initFrom(this.noise, this.kc); this.stage = 'coarse'; }
      else { this.fine.initFrom(this.noise, this.k); this.stage = 'fine'; }
    } else if (this.stage === 'coarse') {
      ch = this.coarseM.step(); this.iters++;
      if (ch <= Math.max(1, 0.0005 * this.kc) || this.coarseM.iters >= 40) {
        prolong(this.coarseM.chi, this.Nc, this.fine.u, this.N);
        this.fine.initFrom(this.fine.u, this.k);
        this.stage = 'fine';
      }
    } else {
      ch = this.fine.step(); this.iters++;
      if (ch === 0) { this.settled = true; }
      else if (ch <= Math.max(1, 0.0005 * this.k)) { if (++this.calm >= 5) this.settled = true; }
      else this.calm = 0;
      if (this.fine.iters >= 400) this.settled = true;
    }
    this.lastChanged = ch;
    const dt = now() - t0; this.ms += dt; return dt;
  };

  // ------------------------------------------------------------ obstacle warm starts (speculation: no theorem covers the pillar)
  // 'centred': the theorem's shape centred on the pillar axis (tube coaxial, slab around the pillar).
  // 'placed' : try the theorem's shape at several offsets from the pillar and two orientations, score each
  //            on the N/2 grid with the same estimator, keep the cheapest, then rebuild it at N.
  function pillarWarmStart(N, V, pillar, mode) {
    const t0 = now(), n = N * N * N, k = Math.round(V * n), mask = pillarMask(N, pillar.r, pillar.cx, pillar.cz);
    const ph = phaseOf(V);
    let best = { c: [pillar.cx, 0.5, pillar.cz], tubeAxis: 1, slabAxis: 0 }, tried = 1;
    if (mode === 'placed') {
      const Nc = N >> 1, nc = Nc * Nc * Nc, kc = Math.round(V * nc), mc = pillarMask(Nc, pillar.r, pillar.cx, pillar.cz);
      const cf = new ClosedForm(Nc), est = new AreaEst(Nc), cands = [];
      for (const d of [0, 0.06, 0.12, 0.18, 0.24, 0.32, 0.5]) {
        const c = [pillar.cx + d, 0.5, pillar.cz];
        if (ph === 'ball') cands.push({ c });
        else if (ph === 'tube') { cands.push({ c, tubeAxis: 1 }); cands.push({ c, tubeAxis: 2 }); }
        else cands.push({ c, slabAxis: 0 });
      }
      if (ph === 'slab') cands.push({ c: [0.5, 0.5, 0.5], slabAxis: 1 });
      let bestA = Infinity;
      for (const o of cands) { const chi = cf.build(kc, Object.assign({ mask: mc }, o)), A = est.area(chi, mc); if (A < bestA) { bestA = A; best = o; } }
      tried = cands.length;
    }
    const cf = new ClosedForm(N), chi = new Uint8Array(cf.build(k, Object.assign({ mask }, best)));
    return { chi, mask, ms: now() - t0, choice: best, tried };
  }

  // ------------------------------------------------------------ helpers
  function median(a) { if (!a.length) return NaN; const b = a.slice().sort((x, y) => x - y), m = b.length >> 1; return b.length % 2 ? b[m] : (b[m - 1] + b[m]) / 2; }
  // light display smoothing for rendering (not part of any timed solver): chi (+ mask at 0.5) -> blurred bytes
  function displayField(chi, N, mask, out, tmpA, tmpB) {
    const n = N * N * N;
    for (let i = 0; i < n; i++) tmpA[i] = (mask && mask[i]) ? 0.5 : chi[i];
    gaussFIR(N, tmpA, tmpB, 0.85);
    for (let i = 0; i < n; i++) { const v = tmpA[i]; out[i] = v <= 0 ? 0 : v >= 1 ? 255 : (v * 255 + 0.5) | 0; }
    return out;
  }

  const Core = { I_unit, phaseOf, theoremShapeName, T_BALL_TUBE, T_TUBE_SLAB, rng, makeNoise, boxRadii, gaussBox, gaussFIR, selectTopK, pillarMask,
    ClosedForm, AreaEst, classify, shapeMatches, MBO, prolong, Solver, pillarWarmStart, median, displayField, now };
  if (typeof module !== 'undefined' && module.exports) module.exports = Core; else root.SlimeCore = Core;
})(typeof self !== 'undefined' ? self : this);
