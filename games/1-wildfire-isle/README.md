> **Exploratory test, not peer-reviewed or independently verified.** See the [disclaimer](../../README.md).

# Wildfire Isle: same fire and same coast at every graphics setting

**Scenario.** A top-down island survival map with sea, beaches, Greenwood forest, the villages Ashford and Saltmere, a lighthouse, a day clock and a *Graphics quality* setting. Low, Medium, High and Ultra set the gameplay grid to 32, 64, 256 and 1024 cells across. There are two gameplay systems. (a) A wildfire edge crosses Greenwood from the watchtower to Ashford. Gameplay number: hours until it reaches Ashford, as a median over many sampled fire edges (target 18 h). (b) Fishing spots and beach finds spawn along the coast of the fBm terrain (target 40).

**PRIOR BEST: per-tile constants in metres, tuned at Medium.** Fire speed is in metres per hour, so step time is proportional to cell size. Spawns are per metre of coastline, so each shore tile has weight proportional to cell size. Both are tuned in a Medium playtest. This is the strongest cheap baseline a careful dev ships: it is exact for smooth shapes and far better than raw per-tile constants. With raw per-tile constants Ultra would get 1034 spawns instead of 40 (bench.js, `naive` column). **REFERENCE: always simulate gameplay at Ultra (1024²).** It is consistent by construction and pays the Ultra cost at every setting.

**NEW: coarse grid + exponent weighting.** Each per-cell quantity is weighted by (cell size)^d, with one constant tuned at Medium. Fire: d = 7/4. Family 223 (`thm:main`) proves that c·n^(-7/4)·N_n converges in law for the critical q = 1 square-lattice FK Dobrushin interface. That is exactly the fire edge traced here (`core.js` copies `panel/web/fk.js`, a port of `demos/04-fractal-frontier/fk.h`). The paper proves this for this model only and gives no convergence rate. Measured in C++ (`demos\L3-fractal-ladder\out\ladder.json`): the median at 32×32 is +3.2% from the 8192² reference (CI up to +5.0%), and at 64×64 it is +0.9%. Coast: d is **not proved**. It is fitted offline by `bench.js` at n = 64, 256 and 1024: 1.17 for this island (theory for unbounded fBm: 2 − H = 1.25; the island falloff smooths the largest scales) and 1.05 for fixed-detail (band-limited) terrain.

**Results, `node bench.js`** (Node 22; 8000/4000/1200/400 fronts; terrain detail grows with quality; full output: run it):

| quality (grid) | fire h PRIOR / NEW / REF | NEW 95% CI | spawns PRIOR / NEW / REF | ms per fire + coast build: PRIOR = NEW / REF |
|---|---|---|---|---|
| Low (32) | 10.8 / 18.2 / 18.0 | −1.2..+0.9% | 34 / 39 / 40 | 0.013 + 0.04 / 5.3 + 72 |
| Medium (64) | 18.0 / 18.0 / 18.0 | (tuned here) | 40 / 40 / 40 | 0.041 + 0.2 / 5.3 + 72 |
| High (256) | 49.7 / 17.6 / 18.0 | −3.3..+1.9% | 48 / 38 / 40 | 0.47 + 3.3 / 5.3 + 72 |
| Ultra (1024) | 139.3 / 17.4 / 18.0 | −4.4..+4.2% | 65 / 40 / 40 | 5.3 + 72 / 5.3 + 72 |

Band-limited terrain (6 fixed octaves), spawns PRIOR / NEW: Low 36 / 37, High 45 / 42, Ultra 46 / 40. As a cross-check, median steps/n^1.75 in JS (3.14, 3.10, 3.03, 3.00) match the C++ medians (3.15, 3.08, 3.06, 3.00).

**Browser** (headless Chrome in real time, page HUD, 4000/2000/400/160 fronts): fire h PRIOR / NEW: 10.7 / 18.0, 18.0 / 18.0, 49.6 / 17.5, 138.6 / 17.3. Spawns match bench.js. ms per fire + coast: 0.014 + 0.08, 0.041 + 0.26, 0.45 + 6.0, 5.0 + 98. Render time is a separate HUD figure. With `--virtual-time-budget` Chrome freezes `performance.now()` during tasks, so the screenshot HUD shows the bench.js ms (inlined from `bench.json`) and says so.

**Verdict.** NEW keeps the fire time within about 4% and the spawn count within 5% of the targets on every grid, at the coarse grid's cost. That makes Low about 400× cheaper than REFERENCE in fire ms and 1800× cheaper in coast ms. PRIOR BEST misses by up to +670% (fire) and +63% (spawns) away from Medium. Where the prior wins or ties: at Medium (its tuning level) it equals NEW in result and cost. REFERENCE is exact everywhere, and NEW still drifts by a few percent (the fire drift is part finite-size, part sampling noise). With band-limited terrain the coast exponent falls toward 1 and PRIOR closes most of the gap (≤15% vs ≤7%). Limits: spawn positions differ between grids (only counts match). Rendering still costs per pixel. Fire models other than this percolation front need their own exponent.

**Reproduce** (in this folder): `node bench.js` (writes `bench.json`, about 10 s), then `python build.py` (inlines `core.js` + `bench.json` into `index.html`). Screenshot:
`"C:/Program Files/Google/Chrome/Application/chrome.exe" --headless=new --hide-scrollbars --window-size=1280,1000 --virtual-time-budget=15000 --user-data-dir="$TEMP/1-wildfire-isle-chrome" --screenshot="<repo>/games/1-wildfire-isle/shot.png" "file:///<repo>/games/1-wildfire-isle/index.html?frames=240&scene=compare"`
Presets: `compare` (High), `low`, `medium` (tie), `ultra`, `bandlimited`, `play` (NEW only, Ultra, night). Params: `frames=N`, `q=0..3`, `view=all|prior|new|ref`, `t=hours`.
