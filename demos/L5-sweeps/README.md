# L5-sweeps: sweep-only load ladders for four "guarantee, not speedup" demos

These ladders have no video. Each one reruns the original demo's problem at increasing load and times both methods at every stage. The output shows how the cost of the guarantee grows with load, where each side leaves the 16.7 ms frame budget, and whether quality changes with load. Each subfolder has a `main.cpp`, a `README.md` and an `out/ladder.json` in the LADDER_BRIEF schema. Each sweep also prints its stage table to stdout.

| dir | original demo | family | load | threads (both sides) |
|---|---|---|---|---|
| `02-fluid-sweep` | 02-fluid-gates | 376 | dye particles per region, 1k → 1M (×4) | 12 (persistent pool) |
| `05-crowd-sweep` | 05-crowd-hub | 374 | agents, 10k → 4M, plus 8.4M and 16M as extensions | 1 |
| `06-dungeon-sweep` | 06-dungeon-shuffle | 131 | rooms, 10 → 320 (×2) | 1 for timing; untimed quality samples use 12 |
| `08-morph-sweep` | 08-convex-morph | 091 | half-space directions, 50 → 6400 (×2) | 1 |

`ladder_common.h` holds the shared code: timing statistics, log-log fits, budget crossings and the JSON writer. The original demos are not modified. The sweeps copy their kernels, and 02 and 08 include `07-periodic-blob/sw3d.h` for vector math and the thread pool.

## Rerun on a quiet machine (sequentially, Git Bash)

```bash
cd <repo>/demos
./build.sh L5-sweeps/02-fluid-sweep  && L5-sweeps/02-fluid-sweep/demo.exe  | tee L5-sweeps/02-fluid-sweep/out/sweep.log
./build.sh L5-sweeps/05-crowd-sweep  && L5-sweeps/05-crowd-sweep/demo.exe  | tee L5-sweeps/05-crowd-sweep/out/sweep.log
./build.sh L5-sweeps/06-dungeon-sweep && L5-sweeps/06-dungeon-sweep/demo.exe | tee L5-sweeps/06-dungeon-sweep/out/sweep.log
./build.sh L5-sweeps/08-morph-sweep  && L5-sweeps/08-morph-sweep/demo.exe  | tee L5-sweeps/08-morph-sweep/out/sweep.log
```

`build.sh` accepts the nested path as is. Each exe writes `out/ladder.json` next to itself (`--out DIR` overrides). The flags are listed in each `main.cpp` header. A shorter iteration run uses `--min-ms 300`. Measured wall time per sweep on a shared machine: see each README. All four stay under about 3 minutes, and peak memory is under 1 GB.

Timing rule: after warm-up, each side runs at least 15 frames and at least 1.5 s of samples. Very slow stages are the exception: when a frame takes more than 250 ms, timing stops after 5 s of samples, and the frame count is recorded. Quality metrics are always computed untimed.

## Results from the first run (shared, loaded machine; rerun for clean numbers)

| ladder | WITH crosses 16.7 ms | WITHOUT crosses 16.7 ms | fitted exponent W/O · WITH | what the new math buys |
|---|---|---|---|---|
| 02 fluid | between 64k and 256k per region | never (≤ 4 ms at 1M) | 0.72 · 0.94 | 0 % overlap and ~1e-11 drift at any load; WITH costs 7–40× more |
| 05 crowd | between 4M and 8.4M agents | between 8.4M and 16M (median frame) | 1.19 · 1.09 | exactly optimal labels; frozen labels mis-route 11.25 % at a = 0.45 |
| 06 dungeon | between n = 40 and 80 (about 49, interpolated) | never (8 µs at n = 320) | 1.07 · 4.58 (empirical Curveball 1.40) | certified TV ≤ ¼ at O(n⁴) trades; ad hoc budgets stay template-biased |
| 08 morph | between 800 and 1600 directions | between 400 and 800 | 1.31 · 1.08 | guaranteed floor plus 1.37–1.39× bloat against 1.62–1.69×; WITH is also 1.2–4× cheaper |
