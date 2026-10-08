# Load-ladder brief: where does the new math pay off as the load grows?

Cody watched the nine before/after videos (`demos\0N-*`, sheet https://tacksheet.com/sheet/iP9Q753RmKk-4BEur8im50G1).
Each ran at a single fixed size, which hides how cost grows. He asked: "is there a way to run them at a
larger scale to see benefits in performance? maybe with graduated stages of element load to see performance gains at certain points?"

You build a **load ladder** for one demo: the same problem at graduated loads (1×, 2×, 4×, …), both methods
measured at every stage, so the reader sees how each side's cost grows, where it stops fitting a 60 fps frame
(16.7 ms) and, where it applies, how *quality at a fixed budget* changes with load.

## Read first
- `demos\HARNESS.md` and `demos\harness\demo.h` (API; honesty rule). Build: `cd <repo>/demos && ./build.sh <dir> [run|preview] [args]` (Git Bash, clang++ 22, C++20, 24 threads, Ryzen 9 9900X).
- `demos\DEMO_BRIEF.md` (the original rules; all still apply, especially HONESTY).
- The original demo you are extending: its `main.cpp`, headers and `README.md`, and `out\clean\results.json`.
  Reuse its code: `#include "../0N-name/some.h"` or copy what you need into your folder. Do not edit the original demo.

## Deliverables, in `demos\<your ladder dir>\`
1. `main.cpp` building with `./build.sh <dir>`. Two modes:
   - **Video mode** (default): a 1920x1080 split-screen video, ~30-40 s at 60 fps, WITHOUT left / WITH right.
     The load steps up every ~3-4 s through 6-10 stages. Show on screen: the current stage and load
     ("4,096 games"), each side's live compute ms/frame (the harness HUD), the 16.7 ms budget line
     (`cfg.budget_ms = 16.7`), and a **cost-vs-load chart that builds up stage by stage** (log-log; one curve
     per side in coral/teal; the 16.7 ms line; a marker/callout where a side first crosses the budget, and the
     ratio WITHOUT/WITH at each stage). Put the chart where it doesn't fight the visuals: a strip drawn with
     `h.on_overlay(...)` across the bottom of both panels, or inside each panel. Keep the scene itself visually
     alive (it should still look like the original demo, scaled up).
   - **Sweep mode** `--sweep` (no video, no drawing): for each stage, warm up, then time each side for enough
     frames to get a stable median (at least 15 frames or 1.5 s, whichever is more; fewer allowed only for
     very slow stages, record the count). Go further up the ladder than the video if it is cheap to do so.
     Write `out\ladder.json` (schema below) and print a table.
2. Over-budget handling. When one side gets very slow (say > 250 ms per frame), the video must not stall for
   minutes: compute that side only every k-th frame and hold its image, and label it on screen
   ("over budget: 1.9 s/frame — updated every 30th frame"). Its HUD and the chart must still report the
   *measured* time per computed frame. Never extrapolate a number without labeling it "extrapolated".
3. `README.md` (15-30 lines): what the ladder scales, stages, the table of medians, the crossover/break points,
   what is proved by the paper vs only measured, caveats.
4. `out\<dir>.mp4`, `out\poster.png` (check it with the Read tool, and a mid-video frame extracted with ffmpeg),
   `out\results.json`, `out\ladder.json`.

## ladder.json schema (exactly these keys; extra keys allowed)
```json
{
  "demo": "L1-snaky-ladder",
  "family": "187",
  "load_name": "concurrent games",       // what N counts
  "budget_ms": 16.7,
  "threads": {"without": 1, "with": 1},
  "stages": [
    {"n": 1,
     "without": {"median_ms": 4.7, "p95_ms": 5.1, "frames": 40},
     "with":    {"median_ms": 0.001, "p95_ms": 0.002, "frames": 40},
     "quality": {"name": "win rate vs search Breaker", "without": 0.41, "with": 1.0, "higher_is_better": true}}
  ],
  "fit": {"without_exponent": 1.0, "with_exponent": 1.0},   // log-log slope of median_ms vs n, measured
  "max_n_within_budget": {"without": 3, "with": 15000},       // largest measured n with median <= budget (say if bracketed)
  "notes": "one paragraph, honest"
}
```
`quality` is optional per stage; include it whenever the ladder also changes quality (e.g. quality at a fixed budget).

## Fairness rules (on top of the original honesty rule)
- Same thread count on both sides at every stage (state it). Same seeds, same scene.
- The baseline must be a reasonable implementation a game dev would actually ship (e.g. α-β with move
  ordering and a transposition table, not a naive minimax), and must not get a worse asymptotic than it
  needs to. If the original demo's baseline was weak at scale, improve it and say so.
- If the WITH side does not win at any load, the ladder must show that. If WITH only wins past some load,
  report the crossover. Do not stop the ladder early to hide an unfavourable stage.
- Separate "proved by the paper" from "measured here" in the README and the video caption.

## Process
- Other ladder agents run in parallel, so your timings are noisy. Iterate with `preview` / short `--frames`
  and a short `--sweep`. Do one full video recording to check it, but **do not chase clean numbers**:
  the orchestrator will re-run every sweep and re-record every video sequentially on a quiet machine.
  So the README must give the exact commands for that (`./build.sh <dir> run` and the `--sweep` invocation)
  and keep the total wall time of one full recording under ~10 minutes and one sweep under ~5 minutes.
- Before each full recording run `devpulse status`; if commit memory > 85%, wait and re-check. Peak memory of
  your program must stay under 4 GB.
- Work only inside your ladder dir. The harness, other demos and the math repo are read-only. No servers.
  Before finishing: `devpulse prune --mine`, then `devpulse prune --mine --yes`, and make sure no demo.exe /
  ffmpeg you started is left running.
- Final message: path of the video, poster, ladder.json; the stage table (n, median ms both sides, ratio,
  quality); the budget crossings; the fitted exponents; a two-sentence honest verdict.
