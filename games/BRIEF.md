# Brief: five "real looking game" demos, best prior technique vs the new math

## Context
Cody is an indie game developer. We scanned new pure-math preprints, built C++ before/after demos and load ladders,
and a panel of four AI models picked five practical techniques (see `<repo>\panel\web\out\practical-math.html`,
its sources in `panel\web\*.js`, and the panel reports in `panel\.state\*.log` and `panel\.state\astra3-report.md`).
Cody then asked for general-purpose versions, and now: **"build a unique example for each using a real looking game,
and show the difference between the best prior techniques and our new ones."**

You build ONE of the five. Each is a small, playable-looking game scene in the browser where the same scene runs
with the best prior technique and with the new technique, side by side or toggled, with honest live measurements.

## Deliverables (in your own folder `<repo>\games\<your dir>\`)
1. `index.html`: ONE self-contained file (inline CSS and JS, no network, no CDN, no external files at runtime;
   inline any data). It must open by double-click (file://).
   - **Looks like a game**, not a chart: a styled game scene (tiles, terrain, sprites, buildings, characters drawn
     procedurally on canvas or WebGL), a HUD, a title bar with the game name. Think "indie game screenshot". Charts are
     allowed as small HUD panels or below the scene.
   - **Comparison:** label the two sides/modes exactly `PRIOR BEST: <technique name>` and `NEW: <technique name>`.
     If a third mode is the honest reference (e.g. "expensive ground truth"), add it and label it `REFERENCE`.
   - **Live numbers in the HUD:** compute time per frame or per operation for each technique (measured with
     `performance.now()` over batches; never invented), plus the quality metric your scene is about.
   - A short explainer below the scene: a heading, one plain-English paragraph of at most 120 words (no jargon, no
     formulas; Cody is not a mathematician), then a few bullet lines "What's measured here" and "Where the prior
     technique still wins / limits".
   - Colours: prior = orange `#eb6834`, new = blue `#2a78d6`, reference = grey `#7a7873`. Other game art is free.
   - Controls: a few buttons/sliders that matter (e.g. quality level, number of tables, amount of goo, horizon).
   - URL params for automation: `?frames=N` renders N animation frames then stops the requestAnimationFrame loop (so
     headless screenshots terminate); `?scene=<name>` jumps to an interesting preset. Document the presets.
2. `bench.js` (Node, `node bench.js`) that measures the headline numbers outside the browser where the logic allows it
   (reuse the same core code; it's fine to keep the core in a separate `core.js` that both files use, as long as
   `index.html` inlines it via a tiny `build.py` or by copy; `index.html` must stay self-contained). Print a table.
3. `README.md` (20-40 lines): the game scenario; the prior best technique and why it is the strongest FAIR baseline a
   game dev would actually ship; the new technique and which paper result it rests on (what is proved vs what is only
   measured); a results table from `bench.js` and from the browser; an honest verdict including regimes where the
   prior technique wins; exact commands to reproduce and to take the screenshot.
4. `shot.png`: a screenshot of an interesting moment, made with headless Chrome, and LOOK at it with the Read tool
   before finishing (fix layout problems you see):
   `"C:/Program Files/Google/Chrome/Application/chrome.exe" --headless=new --hide-scrollbars --window-size=1280,1000 --virtual-time-budget=15000 --user-data-dir="$TEMP/<your dir>-chrome" --screenshot="<abs path>/shot.png" "file:///<abs path>/index.html?frames=240&scene=<preset>"`
   For WebGL add `--use-angle=swiftshader --enable-unsafe-swiftshader`. Without `?frames=N` the screenshot never
   finishes (the animation loop keeps virtual time busy); if Chrome hangs, kill only the chrome processes whose
   command line contains your user-data-dir and fix the stopping logic.

## Honesty rules (most important)
- The prior technique must be the strongest reasonable thing a game dev would ship, implemented competently. If the
  original demo's baseline was weak, use a better one. Name it precisely.
- Never invent numbers. Every number in the HUD/README is measured by your code, or quoted from the existing C++
  measurements with the source file named (e.g. `demos\L1-snaky-ladder\out\ladder.json`).
- If the prior technique wins in some regime, the demo must show that regime too (a preset) and say so.
- Separate "proved by the paper" from "measured here" and from "speculation".
- Keep the paragraph simple and concrete. No hype.

## Process rules
- Work ONLY inside your folder. Everything else (math repo, demos, other games, panel\web) is read-only; you may read
  and copy code from them (e.g. `panel\web\snaky.js`, `snaky_cert.json`, `econ.js`, `fk.js`, `dungeon.js`,
  `panel\general\coast.js`, `demos\0N-*\*.h`).
- Other agents work in parallel in sibling folders. Don't touch their files.
- No servers, no network. Headless Chrome only via file://, with your own `--user-data-dir` under `%TEMP%`.
- Before finishing: make sure no chrome/node process you started is still running (`devpulse prune --mine`, then
  `devpulse prune --mine --yes`). Do not kill anything you did not start.
- Never read secrets. Don't commit or push anything.
- Final message: path of index.html and shot.png, the results table, the honest verdict in two sentences, and the
  preset names.
