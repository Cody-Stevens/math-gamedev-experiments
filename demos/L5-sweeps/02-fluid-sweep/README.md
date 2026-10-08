# 02-fluid-sweep: cost of the incompressible-gate guarantee vs particle count (family 376)

**Load:** dye particles per region (3 regions), 1k → 1M (×4). Each side also advects 2,568 boundary and tracer points, so the top stage moves 3.07 M points. **WITHOUT** is the single-plane closed-form tween: exp-scaling diag(λ^θ, λ^−θ), with endpoint lerp as a third series (`lerp`). **WITH** is the seven-stage shear flow of `thm:shears` (`eq:velocity`, `eq:fields`, `eq:A`, `eq:Z`, `eq:shears`), advected by RK4 with 2 substeps per frame, as in the original demo. Both sides use the same persistent 12-thread pool. One frame advects every point once. Each stage times both periods (the rule table, then its inverse) at 60 frames per period.

Medians below are in ms per frame. They come from a shared, loaded machine with other agents running. A rerun on a quiet machine about halved the larger stages, for example WITH at 1M was 80 ms instead of 171.

| dye/region | WITHOUT exp | lerp | WITH | WITHOUT/WITH |
|---|---|---|---|---|
| 1k | 0.041 | 0.038 | 0.295 | 0.14 |
| 4k | 0.065 | 0.091 | 0.685 | 0.095 |
| 16k | 0.104 | 0.123 | 1.94 | 0.053 |
| 64k | 0.833 | 0.664 | 10.1 | 0.082 |
| 256k | 1.72 | 1.58 | 40.3 | 0.043 |
| 1M | 3.94 | 4.36 | 171 | 0.023 |

- **Budget (16.7 ms):** WITHOUT stays within budget at every stage. WITH crosses between 64k and 256k particles per region, so about 0.2–0.8 M advected points on 12 threads.
- **Fitted exponents (all stages):** WITHOUT 0.72, WITH 0.94. Over the upper half: 0.56 and 1.02. At small loads the times are dominated by pool overhead. WITH is linear once the pool is saturated.
- **Quality, load-independent and measured once on the original 660-frame clock:**

  | method | max same-layer overlap | max area/volume drift |
  |---|---|---|
  | exp-scaling | 89.2 % | 5.6e-13 |
  | lerp | 105.2 % | 33.3 % |
  | WITH | 0 % | 7.3e-12 |

  These numbers come from the region boundaries and tracer tetrahedra, which do not change with the dye count, so `ladder.json` repeats them at every stage with `load_independent: true`.

**Proved vs measured:**
- **Proved by `thm:shears` and `lem:excursion`:** disjoint routing on private heights, an exactly divergence-free field, and excursion ≤ 2h for any λ.
- **Measured here:** cost and the quality numbers above.
- **Not shown:** whether the new math pays off in speed. It never does. WITH costs 7–40× more at every load, because each point needs 8 field evaluations with exp-based masks against 2 multiply-adds for the tween.

**Caveats:**
- The baseline is improved over the original demo: the per-branch tween factors are computed once per frame instead of per point.
- Both sides use a persistent pool instead of spawning threads every frame.
- The timing clock is coarser than the original (60 frames per period instead of 660), but the per-frame work is the same.

**Rerun:** `./build.sh L5-sweeps/02-fluid-sweep && L5-sweeps/02-fluid-sweep/demo.exe`. It takes about 45–105 s and peaks near 300 MB.
