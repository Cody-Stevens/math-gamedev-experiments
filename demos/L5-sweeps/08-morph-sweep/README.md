# 08-morph-sweep: Minkowski vs logarithmic morph vs number of half-space directions (family 091)

**Load:** half-space directions per in-between body, n = 50 → 6400 (×2). The directions are the facet normals of K and L (both signs) plus M rotated Fibonacci pairs, sized so the total is exactly n. **WITHOUT** is Minkowski interpolation, h = (1−λ)h_K + λh_L. **WITH** is the logarithmic Wulff body W[h_K^(1−λ) h_L^λ] of `thm:main` / `eq:main`. Both sides use the same directions and the same clipper, copied unchanged from 08-convex-morph, on **1 thread**. One frame is one full body build: support values plus clipping a box by every plane. The timed frames cycle through 5 keyframe segments × λ ∈ {0.05, 0.25, 0.5, 0.75, 0.95}, and quality is taken over all 25 bodies.

| n | WITHOUT ms | WITH ms | W/O ÷ WITH | min vol/floor, W/O · WITH | max bloat, W/O · WITH | planes that cut, W/O · WITH |
|---|---|---|---|---|---|---|
| 50 | 0.48 | 0.41 | 1.20 | 1.077 · 1.048 | 1.686 · 1.386 | 49 · 44 |
| 100 | 1.54 | 1.04 | 1.48 | 1.077 · 1.049 | 1.665 · 1.385 | 96 · 75 |
| 200 | 4.37 | 2.09 | 2.09 | 1.076 · 1.049 | 1.654 · 1.381 | 178 · 122 |
| 400 | 10.3 | 4.57 | 2.25 | 1.076 · 1.049 | 1.647 · 1.379 | 321 · 207 |
| 800 | 26.4 | 10.4 | 2.55 | 1.076 · 1.048 | 1.631 · 1.372 | 578 · 342 |
| 1600 | 62.1 | 20.4 | 3.04 | 1.076 · 1.048 | 1.629 · 1.368 | 997 · 550 |
| 3200 | 148 | 51.3 | 2.88 | 1.075 · 1.048 | 1.623 · 1.365 | 1709 · 891 |
| 6400 | 284 | 68.3 | 4.16 | 1.075 · 1.048 | 1.619 · 1.365 | 2944 · 1428 |

- **Budget (16.7 ms):** WITHOUT crosses between n = 400 and n = 800. WITH crosses between n = 800 and n = 1600.
- **Fitted exponents (all stages):** WITHOUT 1.31, WITH 1.08. Over the upper half: 1.15 and 0.95.
  - The clipper is worst-case ~n²: every plane scans all current vertices, and every plane that cuts rebuilds the polytope.
  - Measured growth is smaller because the number of planes that cut, and the face count, grow sublinearly.
  - The log body is smaller, so fewer planes cut it. That is the whole reason for its 1.2–4× speed edge, and the edge widens with n.
- **Quality:** both bodies stay above the floor at every n. WITH bloats less: at most 1.39 × floor against 1.69 × for WITHOUT, and its body is always contained in the Minkowski body. Neither number moves much with n, because refining the directions only shrinks both bodies slightly toward their continuum limits.

**Proved vs measured:**
- **Proved by `eq:main`:** the log body's volume is at least |K|^(1−λ)|L|^λ for generic origin-symmetric bodies, for any direction set. A finite set gives a body that contains the continuum Wulff body.
- **Proved, classical:** Minkowski also has a floor, from Brunn–Minkowski.
- **Measured here:** times, ratios, bloat and cut counts.

**Caveats:**
- A dual convex-hull half-space intersection, O(n log n), would lower both curves alike. Neither side uses it.
- The machine was shared during this run, so the timings are noisy. For example, the 3200 → 6400 WITH step looks too flat.
- The original video's minimum ratios (1.032 and 1.020) came from λ closer to 0 and 1 than this grid samples.

**Rerun:** `./build.sh L5-sweeps/08-morph-sweep && L5-sweeps/08-morph-sweep/demo.exe`. It takes about 50 s.
