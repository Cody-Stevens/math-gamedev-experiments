# Demo brief: "WITHOUT vs WITH the new math" videos

Cody (developer moving into game dev + simulation) asked: for each of the biggest game/sim-relevant results found in the new math release (<openai\math>, 372 families of new research manuscripts), build a simple, visually interesting C/C++ example, run it without and with the new math applied, and record a video with benchmark data (fps etc.) — a practical visual before/after.

## Inputs
- Harness (USE IT): <repo>\demos\HARNESS.md and demos\harness\demo.h. Sample: demos\00-sample\main.cpp. Gallery of every draw call: demos\harness\selftest\main.cpp. Build: `cd <repo>/demos && ./build.sh <id> [run|preview] [args]` (Git Bash). clang++ 22, C++20, 24 threads.
- The lead (miner write-up): <repo>\games\slice-NN.md, section G<id>.
- The adversarial verifier's notes (READ THESE, they say exactly what to implement, the fair baseline, and the traps): <repo>\verify-games\v-*.md, block "### G<id>".
- The paper's .tex source under <openai\math>\preprints\<dir>\build\ (cite section/equation labels you implement).

## What each demo must be
1. Split screen (harness two-panel mode): LEFT = WITHOUT (the best fair baseline a game dev would otherwise use), RIGHT = WITH (implementation that genuinely depends on the paper's construction/theorem/formula). Same seed, same scene, same budget. If the honest contrast is a sweep rather than two methods (e.g. a parameter crossing a proved threshold), still use two panels with clear titles, or single-panel mode with a clear before/after narrative — your call, but keep "without vs with" legible.
2. Visually interesting and polished: motion, color, glow, clear labels. 20-30 seconds at 60 fps. Use shortcuts freely (procedural art, colormaps, harness primitives, simple software 3D/raymarching with std::thread if useful). Do not spend effort on assets beyond that.
3. Benchmark HUD per panel: harness-measured compute ms/frame → fps (honesty rule: compute only, measured, never invented), plus 2-5 metrics that capture the actual before/after (e.g. volume drift %, extinct resources, win rate, decision µs, TV distance, area excess). A sparkline of the key metric.
4. Footer caption: "Family NNN — <paper title> · <what the WITH side uses (eq/section)>".
5. HONESTY: Many of these results are guarantees, thresholds or constructions — not speedups. Do NOT fake a speedup or rig the baseline. If WITH is slower, show it. If the real benefit is a guarantee/behavior (stays bounded, never collapses, certified win), make THAT the visible metric. Where the verifier says a classical method gives the same thing, either use that classical method as the baseline or state it in the caption. Label empirical plots as empirical.
6. Deterministic (fixed seeds) so it can be re-run.

## Deliverables per demo, in <repo>\demos\<id>\
- main.cpp (+ any small extra files), building with ./build.sh <id>
- out\<id>.mp4 (final video), out\poster.png (representative frame — check it visually with the Read tool, and check a mid-video frame too: extract with ffmpeg), out\results.json (harness), out\bench.json from a separate `--novideo` run (if the harness supports it; otherwise note) for clean timing.
- README.md: 10-20 lines — what you see, what WITHOUT/WITH are, paper refs (family, path, eq/section labels), headline numbers (a small before→after table), honest caveats (what the paper does and does not guarantee).

## Rules
- Work ONLY inside <repo>\demos\<your ids>\. Do not modify demos\harness (if you need a harness feature, implement it locally in your demo). The math repo and everything else is READ-ONLY.
- Machine is under memory pressure and other agents are building demos in parallel. Before each full video recording run `devpulse status` — if commit memory is above 85%, wait a minute and re-check (use short waits). Record full videos one at a time. Iterate with --preview / few frames first.
- No servers. Make sure no demo.exe / ffmpeg process you started is left running (check `devpulse prune --mine`, then `devpulse prune --mine --yes`).
- Final message: for each demo — video path, poster path, a 3-row before→after metric table, compute ms/frame both panels, and a 1-sentence honest verdict on what the new math buys.
