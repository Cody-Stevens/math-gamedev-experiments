# 07-periodic-blob: minimum-surface blob in a wrap-around cube

**What you see.** A volume fraction V sweeps from 0.03 to 0.97 over 25 s. Each panel raymarches 2×2×2 periodic copies of a 64³ voxel torus, drawn as translucent glass. The yellow box marks one unit cell. Below each render, a plot shows the exact profile I(V) in white and that panel's measured area as dots. Both panels hold exactly the same voxel count every frame.

- **WITHOUT:** volume-preserving threshold dynamics (MBO / Ruuth–Wetton). Each iteration applies a periodic Gaussian (σ = 2.5 voxels) and keeps the k largest values. The solver runs 3 iterations per frame on 24 threads, starts from smooth random noise, re-seeds every 4 s and warm-starts while V moves.
- **WITH:** the certified minimizer from Theorem `intro:main`, built in closed form: a ball, then a tube about a closed geodesic, then a slab, switching at 4π/81 and 1/π, with complements for V > ½. It is voxelized onto the same grid at the same k.
- **Scoring:** both outputs are scored by the same isotropic estimator, ∫|∇(G₁ * χ)|, and compared with the exact I(V) of eq. `intro:profile`.

**Paper:** Family 354, `preprints/The-Isoperimetric-Conjecture-for-the-Cubic-Flat-Three-Torus-September-24-2026/build/paper.tex`, Theorem `intro:main`, eq. `intro:profile`.

| metric (bench run, 1500 frames) | WITHOUT (MBO) | WITH (closed form) |
|---|---|---|
| mean area excess A/I(V) − 1 | +8.7 % | +0.13 % (estimator bias, range −0.52…+0.54 %) |
| frames with the optimal shape type | 39.8 % | 100 % |
| random starts within 2 % of the optimum after 240 iterations (study, 36 runs) | 20 / 36 (0 / 6 at V = 0.40) | n/a (exact) |
| compute, median ms/frame (`out/bench.json`) | 5.77 (3 iterations) | 2.24 |

**Caveats**
- The ball/tube/slab shapes and their thresholds were conjectured before this paper (Hauswirth–Pérez–Romon–Ros, Ros). The paper's contribution is certification: these shapes are the global optimum over all finite-perimeter sets, including higher-genus competitors, and there are no other equality cases. That certification is what turns I(V) into an exact target for a solver, and it is the only reason the left HUD can say "≠ optimum".
- The theorem says nothing about relaxation dynamics, transition paths or solver convergence. The MBO hysteresis in the video (a ball kept past 4π/81, slabs and "other" shapes stuck) is empirical and specific to this grid and step size.
- The ms/frame figures compare iterating with evaluating. They do not measure a speedup of the same algorithm, so the HUD shows no speedup ratio.
- Area estimates carry a grid bias. A grid-refinement check on the closed-form shapes at N = 32…128 is stored in `results.json` under `refine_bias_*`; at N = 64 the bias is within ±0.5 %. The "excess vs WITH" line uses the same estimator on both sides, so this bias cancels there.
- A partial voxel layer is unavoidable at a fixed count k, and both sides pay for it.

Rebuild with `./build.sh 07-periodic-blob run`. The bench numbers come from `demo.exe --novideo --study`.
