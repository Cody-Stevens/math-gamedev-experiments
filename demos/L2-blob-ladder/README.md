# L2-blob-ladder: isoperimetric minimizer in the cubic flat 3-torus (family 354) under a growing grid

**What scales.** The load is the voxel grid N³ of the periodic unit cube: 32³ → 256³ in the video, plus 384³ (57 M voxels, about 1.9 GB peak) in the sweep. Both sides output exactly k = round(V·N³) voxels on 24 threads and are scored by 07's area estimator, ∫|∇(G₁∗χ)|. The comparison is **time to target**: wall time until the estimated area is within 2 % of the exact I(V).
- **WITHOUT:** volume-preserving MBO (Ruuth–Wetton) from a smooth random field that is the same physical field at every N. σ is fixed at 1/16 of the cell, so every grid solves the same continuum problem. Upgrades over 07: a periodic recursive Young–van Vliet Gaussian (O(1) per voxel; FIR is used instead if faster, which never happened) and a parallel histogram top-k. 07's FIR at fixed physical σ would cost N⁴. Only MBO compute is timed. The stopping test A ≤ 1.02·I(V) is untimed, and it is an oracle that exists only because the paper gives I(V).
- **WITH:** the ball, tube or slab of Theorem `intro:main`, voxelized by ranking voxels by squared periodic distance and keeping the top k (same top-k code). Cost: one build.
- **Sweep:** V = 0.10 / 0.25 / 0.40 (ball, tube and near-slab) × 3 seeds = 9 runs per grid. A run that never reaches 2 % counts as +∞ in the median. These medians come from a noisy machine; `out/ladder.json` is the record.

| grid | WITHOUT ms to 2 % (fails/9) | ms per MBO iter | WITH build ms | ratio | coarse-to-fine MBO ms (extra) | excess L / R |
|---|---|---|---|---|---|---|
| 32³ | 18.1 (3/9) | 0.29 | 0.11 | ×161 | n/a | +1.8 % / −0.3 % |
| 48³ | 22.3 (1/9) | 0.48 | 0.25 | ×89 | n/a | +1.9 % / +0.1 % |
| 64³ | 29.0 (1/9) | 0.59 | 0.34 | ×84 | 20.6 | +1.9 % / +0.2 % |
| 96³ | 56.7 (1/9) | 1.16 | 0.46 | ×123 | 21.6 | +1.9 % / +0.3 % |
| 128³ | 112 (1/9) | 2.32 | 0.89 | ×125 | 26.9 | +1.9 % / +0.3 % |
| 192³ | 373 (1/9) | 7.17 | 2.82 | ×132 | 45.5 | +1.9 % / +0.2 % |
| 256³ | 846 (1/9) | 17.7 | 6.64 | ×127 | 85.2 | +2.0 % / +0.3 % |
| 384³ | 2678 (1/9) | 53.2 | 21.2 | ×126 | 234 | +1.9 % / +0.3 % |

**Break points.** WITHOUT's median never fits in 16.7 ms; 32³ comes closest at 18.1 ms. WITH fits up to 256³ (6.6 ms) and crosses at 384³ (21 ms). Against n = N³, WITHOUT scales as n^0.70 and WITH as n^0.68 over all stages, and both as n^0.96 for N ≥ 128; fixed per-call overhead flattens the small grids. The ratio stays near ×85–160 because MBO needs about 47–60 iterations at every N (σ is fixed in physical units), and each iteration costs about 2.6 builds. The coarse-to-fine variant (32³ solve, prolong, polish at N³) cuts the gap to about ×13 at 256³ and ×11 at 384³.

**Proved vs measured.** The paper proves which shape is optimal and the value I(V). Everything else here is measured: every millisecond, the iteration counts and the failures. The paper says nothing about MBO dynamics. One start (V = 0.25, seed 379) gets stuck at +5.6–6.5 % in a "wraps 2/3 axes" local minimum at every grid. At 32³ two V = 0.40 starts also get stuck, because σ is only 2 voxels there. Successful MBO runs stop at about +1.9 %; the closed form sits at about +0.2 %, which is the estimator's bias.

**Caveats.** The video shows one start, V = 0.25 with seed 1379, which converges at every grid; its counters are measured live while ffmpeg encodes. In the check recording, the 256³ stage hit 2 % after 48 iterations: 781 ms against a 5.7 ms build (×138). The first stage, 32³, read 75 ms against the sweep's 18 ms because the encoder and the thread pool were still starting up, so trust `ladder.json` for the numbers. The win is a constant factor plus reliability, not a better exponent. A smarter baseline narrows it, but no MBO run can tell it has reached the optimum without I(V). Ball, tube and slab were conjectured optimal before this paper; the paper adds the certification.

**Re-run (Git Bash, from `demos/`).** Video, about 30 s / 1830 frames and roughly 6 min wall time: `./build.sh L2-blob-ladder run`. Sweep, about 3 min, writes `out/ladder.json`: `./build.sh L2-blob-ladder && ./L2-blob-ladder/demo.exe --sweep`.
