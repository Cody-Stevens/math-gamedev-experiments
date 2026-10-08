> **Exploratory test, not peer-reviewed or independently verified.** See the [disclaimer](../../README.md).

# Dungeon Daily: fair daily dungeon layouts, prior best vs certified Curveball

**Scenario.** A roguelike makes a new dungeon every day ("Daily #N", same seed means the same layout for every player) from a designer template. Every room keeps its door count: the boss room has 1 door, a town square has many. Only the room-to-room wiring changes. "Fair" means every simple wiring with those door counts is equally likely. Each panel draws the day's layout as stone rooms and tiled corridors. Pale corridor lines are template corridors that survived; coloured ones are new. A hero walks from START to BOSS, and rooms the entrance can't reach are shown as SEALED.

**Priors.**
- `PRIOR BEST: lazy double-edge swap`, 1 swap per door. This is what most games ship: cheap, but it has no guarantee and stays biased toward the template.
- `PRIOR BEST: configuration model + rejection`. Deal all door stubs into random pairs and abort at the first loop or double door. This is the strongest simple exact method: each simple layout is produced by exactly prod(d_i!) pairings, so accepted layouts are **exactly** uniform. Rarely shipped exact switching samplers (McKay-Wormald, Gao-Wormald) are not implemented here.

**New.** `NEW: certified Curveball`: Curveball pair trades run for T = ceil(B(1+B/2) ln 2) trades, B = n(n-1)/2. *Proved* (Family 131, sec. 7 `mix:H-poincare`, corollary of the checked proof, not its headline 2n^8 theorem): TV to uniform <= 1/4 from any start, for every graphical degree sequence. *Measured*: cost and the fairness proxies below. The certificate guarantees fairness, not speed.

**Results.** `node bench.js --ms 3000` on a Ryzen 9 9900X with node 22 (`bench.txt`, `bench.json`). Fairness is the "template doors kept" histogram TV against a long-run swap chain reference; the expected noise is in brackets. Browser numbers come from Chrome 154 headless (`?prewarm=8000&debug=1`, run with `--disable-renderer-backgrounding --disable-features=UseEcoQoSForBackgroundProcess`). V8 in Chrome runs these loops about 2-3x slower than node.

| preset (rooms, doors, max doors/room) | swap ms / hist TV | config model ms / deals per layout / hist TV | certified Curveball ms / hist TV | browser: config vs Curveball ms | winner |
|---|---|---|---|---|---|
| `normal` (20, 28, 6) | 0.0016 / 0.67 | 0.0019 / 13.8 / 0.026 (0.018) | 0.56 / 0.034 (0.020) | 0.0051 vs 1.30 | **prior**, 298x |
| `square` (30, 53, one 16-door square) | 0.0031 / 0.89 | 0.37 / 2,893 / 0.023 (0.027) | 3.20 / 0.054 (0.044) | 1.02 vs 7.05 | **prior**, 8.7x |
| `hubs` (30, 64, square 16 + halls 10, 10) | 0.0029 / 0.93 | 20.0 / 165,041 / 0.093 (0.13) | 3.54 / 0.053 (0.053) | 55.9 vs 7.37 | **new**, 5.6x |
| `hubs&square=18` (30, 66, 18) | 0.0034 / 0.94 | 102 / 814,967 / 0.30 (0.27; 30 layouts) | 3.18 / 0.059 (0.047) | 448 vs 7.98 | **new**, 32x |
| `big` (80, 127, 6) | 0.0062 / 0.99 | 0.0082 / 21.5 / 0.028 (0.023) | 304 / 30 samples only | 0.025 vs 677 | **prior**, 37,000x |

The C++ sweep (`demos\L5-sweeps\06-dungeon-sweep\out\ladder.json`) measures certified Curveball at 0.33 ms (20 rooms), 6.3 ms (40) and 200 ms (80).

Exact TV (every layout enumerated, law propagated exactly; shown live in the page):
- 8-room toy (623 layouts): swap at 1 swap per door is 0.459 off uniform, and certified Curveball (292 trades) is 1.8e-14 off.
- Configuration model: exactly 0. All 10,395 and 2,027,025 pairings of two toys were enumerated, and every layout is hit 64 and 2,304 times respectively.
- A hub toy d=(6,5,2,2,2,1,1,1) accepts only 0.317% of its 654,729,075 pairings.

**Verdict.** On ordinary dungeons, and even with one 16-door square in 30 rooms, the configuration model is both exactly fair and faster than certified Curveball by 8.7x to 37,000x, so ship it there. Once several big hubs make almost every deal clash (`hubs`, and more so `square=18`), its cost explodes, while Curveball's stays about 3-4 ms, so the certified count wins.
- The swap shortcut is the cheapest everywhere and measurably unfair everywhere: it keeps 1.5-3.7x the fair number of template corridors.
- Certified Curveball becomes a loading bar at 80 rooms.
- Filtering out disconnected finished layouts (`?connected=1`) keeps the configuration model exactly uniform, and Curveball within its bound, over connected layouts. Rejecting moves inside a chain would break that.

**Reproduce.** Run `python build.py` (inlines `core.js` into `src.html` to make `index.html`), then `node bench.js` (about 2 min). Screenshot: `"C:/Program Files/Google/Chrome/Application/chrome.exe" --headless=new --hide-scrollbars --window-size=1280,1000 --virtual-time-budget=15000 --user-data-dir="$TEMP/4-dungeon-daily-chrome" --screenshot="<abs>/shot.png" "file:///<abs>/index.html?frames=600&scene=hubs"`.
- `?frames=N` steps N frames synchronously at load, draws once and stops. Headless virtual time hardly runs requestAnimationFrame and freezes `performance.now()` inside timer tasks, but not during the load script. If the clock is frozen, the HUD says so instead of showing 0.
- Other parameters: `?scene=normal|square|hubs|big`, `&square=6..20`, `?prewarm=MS`, `?day=N`, `?connected=1`, `?budget=ms per method per frame` (default 4).
