> **Exploratory test, not peer-reviewed or independently verified.** See the [disclaimer](../../README.md).

# Slime Lab: least-surface slime in a wrap-around cell

**Scenario.** A containment cell whose opposite walls are glued, so it is a flat 3-torus. The lab shows 3×3 copies behind glass. You pour glowing slime (fill slider, POUR/DRAIN), and it settles into the shape with the least surface. The scene is a WebGL2 raymarch of a 48³ voxel texture (32³ and 64³ are also available). Left: **PRIOR BEST**. Right: **NEW**. Both keep exactly round(V·N³) voxels and are scored by the same estimator, ∫|∇(G₁∗χ)| with σ = 1 voxel, ported from `demos\07-periodic-blob`.

**PRIOR BEST: coarse-to-fine volume-preserving threshold dynamics (MBO / Ruuth–Wetton).** Each step applies a periodic Gaussian (σ = 1/16 cell, triple-box, Kovesi radii; faster than FIR in JS) and keeps the top k by exact radix select. It first runs on an N/2 grid with σ = 1/6 cell, then prolongs and polishes at N. It starts from smooth noise (as in `demos\L2-blob-ladder`). It stops on its own rule: no voxel moves, or ≤ 0.05 % move for 5 steps. I picked the coarse σ with `node bench.js --explore` (1/16, 1/8, 1/6, 1/5, 1/4 tried). 1/6 was fastest without getting stuck. Plain single-grid MBO is shown in the bench for reference. It is a fair baseline because it is the standard volume-constrained perimeter solver: grid-only, no shape assumptions, and it handles any topology and obstacles.

**NEW: the closed form of Family 354** ("The Isoperimetric Conjecture for the Cubic Flat Three-Torus", Theorem `intro:main`, eq. `intro:profile`). With v = min(V, 1−V), the optimum is a ball for v < 4π/81, a wrapping tube for v < 1/π, then a slab, with complements above ½. The build ranks voxels by periodic distance and keeps exactly k. *Proved:* the shape type and the optimal area I(V), over all sets. *Measured here:* every millisecond, stuck count and estimator bias. The 2 % stopping test needs I(V), and it is not timed. Without the paper, MBO cannot know it is done.

**Results, `node bench.js --seeds 12`** (Node 22, one thread, V = 0.10/0.25/0.40 × 12 noises = 36 cold starts per cell; `bench.out.txt`, `bench.json`):

| grid | plain MBO ms to 2 % (stuck) | coarse-to-fine MBO ms to 2 % (stuck) | MBO ms/step at N | closed form ms | ratio c2f / closed | estimator bias on the exact shape |
|---|---|---|---|---|---|---|
| 32³ | 44.6 (15/36) | 2.32 (2/36) | 0.85 | 0.18 | ×13 | −1.2 / −0.6 / +0.8 % |
| 48³ | 98.6 (10/36) | 10.5 (0/36) | 2.64 | 0.75 | ×14 | −0.4 / +0.2 / +0.6 % |
| 64³ | 299 (2/36) | 22.5 (0/36) | 7.14 | 1.23 | ×18 | −0.1 / +0.1 / +0.4 % |

**Browser** (headless Chrome, SwiftShader, 48³; screenshots in this folder):
- `?scene=tube` (`shot.png`): the prior is within 2 % at 14.8 ms and settles at 123 ms after 51 steps. The new build takes 0.70 ms.
- In-page stress test, 9 cold starts: prior median 13.5 ms with 0/9 stuck; new 0.73 ms (×18). Repeat runs on this busy machine gave the prior 11.5–15.5 ms and the new side 0.70–0.89 ms.
- `?scene=stuck` (32³, noise 1333, one of the bench's 2/36 failures): the prior freezes as a "tangle" at +5.4 % after 24 steps.
- The prior's final estimate can read below the exact shape's (+0.1 % against +0.2 %). That is estimator noise, not a better shape.

**Pillar (speculation, measured only; `bench.out.txt`, 48³, 8 noises).** A pillar breaks the theorem. Slime touching it costs no surface: A_rel = (A(E) + A(E∪P) − A(P))/2. "Centred" (theorem shape around the pillar) is a trap: MBO cannot move it (1.939 against cold 1.641 at V = 0.25). "Placed" tries 7 offsets × orientations on the N/2 grid, then polishes:

| V | cold c2f: steps / ms / area (median, best) | placed warm start: steps / ms (placement) / area | placed vs cold |
|---|---|---|---|
| 0.10 | 58.5 / 77.1 / 0.938, 0.937 | 33 / 90.9 (5.8) / 0.936 | same area (±0.2 %), slower |
| 0.25 | 53.5 / 70.2 / 1.641, 1.634 | 3 / 28.9 (20.2) / 1.565 | lower in 8/8, faster |
| 0.40 | 28 / 36.6 / 1.876, 1.731 | 5 / 24.0 (10.8) / 1.739 | lower in 6/8, higher in 2/8 |

In the browser (`shot_pillar.png`), cold reached 1.575 in 48.2 ms and placed reached 1.565 in 37.3 + 14.2 ms.

**Verdict.** In the empty cell, the closed form is exact by theorem and 13–18× faster than a well-tuned coarse-to-fine MBO here, about ×11 at 384³ in C++ (`demos\L2-blob-ladder\README.md`). That is a constant factor plus a guarantee, not a new scaling law. With a strong coarse stage, the prior is rarely stuck at 48–64³ (0/72). Plain MBO is stuck often (10/36 at 48³). The prior wins wherever the paper is silent (pillars, non-cubic cells, gravity), and it alone shows the flow. A warm start helped at 25 % and 40 % fill and did not help at 10 %.

**Reproduce** (from this folder): `node bench.js` (about 1 min) · `node bench.js --explore --ns 48,64 --seeds 6 --cs-list 16,8,6,5,4` · `python build.py` (inlines `core.js` into `index.html`) · screenshot:
`"C:/Program Files/Google/Chrome/Application/chrome.exe" --headless=new --hide-scrollbars --window-size=1280,1000 --virtual-time-budget=15000 --use-angle=swiftshader --enable-unsafe-swiftshader --user-data-dir="$TEMP/5-slime-lab-chrome" --screenshot="<abs>/shot.png" "file:///<abs>/index.html?frames=240&scene=tube&stress=1"`. With `?frames=N`, the N frames run synchronously during page load. Headless virtual time freezes `performance.now()` after load, which would make every millisecond read 0. Presets: `ball`, `tube`, `slab`, `bubble`, `stuck`, `pillar`, `pillar-drop`. Other options: `&grid=32|48|64`, `&pour=1`, `&renderer=cpu` (canvas-2D fallback). Chrome rounds `performance.now()` to 0.1 ms, so NEW is timed as a batch of 9 builds.
