# L1-snaky-ladder: Maker–Breaker Snaky (Family 187) under growing load

**What scales.** Load N is the number of concurrent 17×17 Snaky games per side. Every frame, the Maker in each game makes one decision. The seeded Breaker replies, untimed. A finished game restarts immediately, so every computed frame holds exactly N Maker decisions. **WITHOUT** is 03-snaky's α-β game AI: threat eval, width-9 move ordering, iterative deepening, 6000 nodes per move. I upgraded it with incremental evaluation and ordering, a Zobrist transposition table and partial-iteration root moves (`fast_search.h`). With the same Breakers and the same 6000 nodes, the upgraded engine won 40/40 games against the α-β Breaker where the original won 21/40 (`--engine-check 40`). On identical positions it costs 0.9 µs per node against the original's 1.4 µs. **WITH** is 03-snaky's port of the certificate policy (728 cards, first surviving child). Both sides use 1 thread and the same seeds. The timer covers only the Maker policy: absorbing Breaker's last reply plus choosing a cell. The timing ladder uses the random-local and greedy Breakers, alternating by game slot.

**Stages.** The video runs N = 1, 2, 4, 8, 16, 64, 256, 1,024, 4,096, 16,384, at 3.5 s per stage. The sweep doubles N from 1 to 262,144. It stopped timing WITHOUT after N = 2,048, where one frame took 9.9 s. The table below is one sweep from `out/ladder.json`, run while other jobs were using the machine. The orchestrator will re-run it.

| N | WITHOUT median ms/frame (frames) | WITH median ms/frame | ratio | quality: win % vs α-β Breaker at 16.7 ms/frame split over N, search / certificate |
|---|---|---|---|---|
| 1 | 3.46 (616) | 0.0000 (timer floor) | ≥ 35k | 100 / 100 |
| 4 | 10.7 (151) | 0.0001 | ≥ 54k | 98 / 100 |
| 8 | **21.1** (75) | 0.0003 | ≈ 53k | 97 / 100 |
| 32 | 72.7 (20) | 0.0011 | 66k | 65 / 100 |
| 128 | 385 (15) | 0.0046 | 84k | 27 / 100 |
| 512 | 2,290 (4) | 0.019 | 118k | 20 / 100 |
| 2,048 | 9,850 (1) | 0.119 | 83k | 0 / 100 |
| 16,384 | not timed | 1.58 | – | 0 / 100 |
| 131,072 | not timed | 13.5 | – | (quality runs to 65,536) |
| 262,144 | not timed | **26.4** | – | – |

**Budget crossings and fits.** WITHOUT fits 16.7 ms up to N = 4 and goes over at N = 8. WITH fits up to N = 131,072 and goes over at N = 262,144. That is 2^15 times more games in one 60 fps frame. Where WITH is above the timer floor (N = 16 to 2,048), the WITHOUT/WITH cost ratio is 6.6×10^4 to 1.2×10^5. Below that floor the sweep gives only lower bounds of 2×10^4 to 5×10^4. The fitted log-log slopes are 1.08 for WITHOUT and 1.15 for WITH. The WITH slope uses stages whose medians are at least 10 timer ticks (N ≥ 32). WITH grows faster than linear because its per-decision cost rises from about 0.03 to 0.10 µs once the games no longer fit in cache. Under a fixed 60 fps budget, the search Maker's win rate against the α-β Breaker falls: 100% at N ≤ 2, about 97% to N = 16, 65% at 32, 47% at 64, ≤ 27% from 128 and about 0% from 1,024. The certificate wins 100% (60/60 games per Breaker) at every N because its moves do not depend on the budget. Its timed frame stays within 16.7 ms through N = 131,072.

**Proved vs measured.** The paper proves that Maker wins within 21 Maker moves against every Breaker (Thm `thm:main`, Cor. `cor:finite-board`). It also implies O(1) work per move: one table step per Breaker reply. Every cost, crossing, exponent and win rate above is measured on this machine. None comes from the paper.

**Caveats.**
- WITH medians for N ≤ 8 are at the 0.1 µs resolution of QueryPerformanceCounter, so those ratios are lower bounds.
- Games start synchronized at each stage. Opening moves cost the search about 1.4× its game average (7.0 vs 5.1 ms in a 64-game probe), so slow stages measured over 1 to 4 frames overstate WITHOUT somewhat.
- The quality games run on 6 threads with wall-clock deadlines. The search always does at least one root candidate scan and the timer calls, so it overruns its allotment by about 2 µs per move. That overrun is small until N ≈ 2,048 (10.8 µs used against 8.2 µs allotted). At N = 16,384 it is 2.9 µs against 1.0 µs, and the search plays its top-ordered move. The certificate's per-move µs includes the timer's own overhead.
- Over-budget stages in the video update the WITHOUT side every k-th frame and say so on screen. The video's own medians are noisier than the sweep's: rendering evicts caches and ffmpeg encodes at the same time. In the video, WITH costs 0.7 µs at N = 1 and 1.9 ms at N = 16,384, so its on-screen ratios (×3.9k to ×34k) are lower than the sweep's. In the video, WITHOUT crosses the budget at N = 8, the same as in the sweep. Use `ladder.json`.

**Reproduce** (Git Bash, from `demos/`). Run the sweep first: the video's quality panel reads `out/ladder.json`.
```
./build.sh L1-snaky-ladder && L1-snaky-ladder/demo.exe --sweep > L1-snaky-ladder/out/sweep.log   # ~4 min, writes out/ladder.json
./build.sh L1-snaky-ladder run                                                                 # ~5 min, 2,100 frames -> out/L1-snaky-ladder.mp4
L1-snaky-ladder/demo.exe --engine-check 40                                                     # optional, ~1 min: original vs upgraded engine
```
