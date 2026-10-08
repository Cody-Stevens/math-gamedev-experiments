// Does a "coastline" statistic of ordinary fBm terrain change with grid resolution, and does an
// exponent weight (cell size)^d fix it? Terrain is a fixed continuous function (value-noise fBm).
const H = 0.75; // roughness (Hurst exponent) of the terrain
function hash(i, j, o) { let h = Math.imul(i, 374761393) ^ Math.imul(j, 668265263) ^ Math.imul(o, 0x27d4eb2d); h = Math.imul(h ^ (h >>> 13), 1274126177); return ((h ^ (h >>> 16)) >>> 0) / 4294967296; }
function vnoise(x, y, o) { const i = Math.floor(x), j = Math.floor(y), fx = x - i, fy = y - j, sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
  const a = hash(i, j, o), b = hash(i + 1, j, o), c = hash(i, j + 1, o), d = hash(i + 1, j + 1, o);
  return a + (b - a) * sx + (c - a) * sy + (a - b - c + d) * sx * sy - 0.5; }
function height(x, y, octaves) { let s = 0, f = 4, amp = 1; for (let o = 0; o < octaves; o++) { s += amp * vnoise(x * f, y * f, o); f *= 2; amp *= Math.pow(2, -H); } return s; }
function coastTiles(n, octaves) { // land/sea on an n x n grid; count land-sea neighbour pairs (coast edges)
  const land = new Uint8Array(n * n);
  for (let j = 0; j < n; j++) for (let i = 0; i < n; i++) land[j * n + i] = height((i + .5) / n, (j + .5) / n, octaves) > 0.05 ? 1 : 0;
  let e = 0; for (let j = 0; j < n; j++) for (let i = 0; i < n; i++) { const v = land[j * n + i]; if (i + 1 < n && land[j * n + i + 1] !== v) e++; if (j + 1 < n && land[(j + 1) * n + i] !== v) e++; }
  return e;
}
const ns = [64, 128, 256, 512, 1024];
for (const mode of ['detail grows with resolution (octaves = log2 n)', 'detail fixed (6 octaves)']) {
  const counts = ns.map(n => coastTiles(n, mode.startsWith('detail grows') ? Math.log2(n) - 2 : 6));
  const fits = []; for (let k = 1; k < ns.length; k++) fits.push(Math.log2(counts[k] / counts[k - 1]).toFixed(2));
  console.log('\n' + mode); console.log(' n:', ns.join('  '), '\n coast edges:', counts.join('  '), '\n local exponent between sizes:', fits.join('  '));
  for (const d of [1, 2 - H]) console.log(` weighted by (1/n)^${d.toFixed(2)}:`, counts.map((c, k) => (c * Math.pow(1 / ns[k], d)).toFixed(2)).join('  '));
}
