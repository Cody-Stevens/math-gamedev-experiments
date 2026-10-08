# 04-fractal-frontier: a fractal frontier whose animation clock survives grid refinement

**What you see.** Critical q = 1 square-lattice FK (bond percolation) Dobrushin interfaces in the unit square draw themselves as glowing "creep fronts", coloured by clock time, on 64², 256² and 1024² grids at the same time. Both panels sample identical curves with the same seeds. Above the lanes is the empirical CDF of each curve's total duration (24,960 / 6,240 / 1,560 curves at 64² / 256² / 1024²). Below them is the mean time spent in each cell of a fixed 8×8 screen grid, on one colour scale shared by both panels.

- **WITHOUT:** a constant-speed clock, where each traversal costs time proportional to its length δ = 1/n. It is tuned so that 256² looks right. The HUD also shows the fixed-tick clock (one tick per lattice step).
- **WITH:** the paper's natural clock. Each traversal costs 1 s × δ^(7/4), with one constant for every resolution.

**Paper.** Family 223, `preprints/Natural-Occupation-Measures-for-Critical-Square-Lattice-FK-Interfaces-October-5-2026/build/sections/01-introduction.tex`: `eq:parameters` (p_q = 1/2 and d = 7/4 at q = 1), `eq:counting-measure`, `eq:content-measure` and `thm:main`, which proves that c(q)·n^(-d)·N_n converges jointly with the curve and its occupation measure, total mass included. `fk.h` implements the exact construction: the wired top/left arc (declared open), a random bottom/right arc, medial turn pairings, and counting of boundary traversals but not terminal half-edges. Edges are lazily hashed fair bits, which is exact at q = 1.

| metric (end of run) | WITHOUT (length clock) | WITH (δ^(7/4) clock) |
|---|---|---|
| median duration 64² / 256² / 1024² | 1.09 / 3.06 / 8.57 s | 3.08 / 3.06 / 3.03 s |
| median ratio 1024² / 64² (fixed-tick clock: ×126) | 7.88× | 0.985× |
| occupation mismatch, mean time per bin, 1024² vs 64² | 683 % | 2.9 % |
| compute ms/frame, median (`bench.json`, `--novideo`) | 5.78 | 5.76 |

The compute is identical on both sides: sampling and tracing run at about 140 M traversals/s, and the clock is only a weight. **This is not a speedup.** The empirical fitted exponent is 1.744. Mean N·n^(-7/4) is 3.149 / 3.116 / 3.082 at 64² / 256² / 1024², so a small drift remains at 64².

**Caveats.** The 7/4 exponent is classical: Beffara's SLE dimension and the Garban–Pete–Schramm and Holden–Li–Sun results for triangular percolation. A designer could guess the δ^(7/4) clock from it. What is new is the proof that, on the square lattice, one constant times the pure mesh power gives convergence in distribution with no slowly varying factor. That holds jointly with the curve and the occupation measure, boundary mass included. The constant c(1) is not evaluated, so the 1 s unit is arbitrary. The theorem gives no finite-size rate and does not cover q = 4. All of the agreement shown is empirical. Using the measure as an animation clock is our application, not a claim of the paper.
