# 02-fluid-gates: incompressible gates that swap and reshape dye regions (lead G10.2)

**What you see:** three dye regions, with 24,000 particles each plus traced boundaries, follow a finite rule table F_i(X) = q_i + diag(λ_i, 1/λ_i)(X − p_i) on the 3-torus T₃³:

- Branch 1 goes from slot A to slot B with λ = 5/2.
- Branch 2 goes from slot B to slot A with λ = 1/2.
- Branch 3 is reshaped in place at slot C with λ = 3.

Sources overlap targets, which `thm:shears` allows. Period 1 runs the table and period 2 runs the inverse table, so every region should come home. Dashed rectangles mark the targets.

- **WITHOUT:** the regions are tweened in one plane. Period 1 uses a direct endpoint lerp. Period 2 uses the stronger classical baseline the verifier asked for, exponential reciprocal scaling diag(λ^θ, λ^−θ). This keeps area exactly, but the swapped branches still pass through each other. Red marks the overlap.
- **WITH:** the paper's velocity V = Σ b_j(t) W_j(X) (`eq:velocity`) with the seven fields of `eq:fields`, the masks `eq:A` and `eq:Z`, and the shear order of `eq:shears`. Each branch lifts to its own private height z_i, makes four shears, translates, and lowers. Particles are advected by RK4 on the sampled field. The exact stage maps serve as the reference.

**Paper:** Family 376, *Finite Instructions and Solenoidal Shear Flows*, `preprints/Finite-Instructions-and-Solenoidal-Shear-Flows-September-27-2026/build/main.tex`: `sec:shears`, `lem:excursion` and `thm:shears`.

| metric (full run; results.json / bench.json) | WITHOUT | WITH |
|---|---|---|
| max area / material-volume drift | 33.3 % with lerp; 5e-13 with exp-scaling | 5.2e-12 |
| max branch overlap, same layer, as % of the mean region area | 105 % with lerp; 89 % with exp-scaling | 0 % |
| divergence residual | up to 1.33 (lerp), 0 (exp) | 0 (finite differences of the sampled field) |
| max sup excursion / h | 1.00 | 1.90 during scaling (proved bound: 2) |
| endpoint / round-trip error | 4e-16 / 7e-16 (exact by construction) | 5e-14 / 9e-14 (RK4 vs exact maps: ≤ 2.1e-9 mid-flight) |
| compute ms/frame, median, 12 threads | 0.85 | 2.97 |

**Caveats:**

- The WITH panel is about 3.5× slower with 12 threads. With 1 thread it is 17.6 ms against 0.32 ms (`bench_1thread.json`). The baseline is a closed-form tween.
- The benefit is a guarantee about the motion, not speed. The regions stay disjoint, keep their volume, follow an exactly divergence-free field, and stay within the 2h excursion bound for any λ.
- The timing medians come from a loaded shared machine, so the p95 values are noisy. Both panels time the same 74,333 points. The ambient dye on the right is drawn only for visualization and is not timed.
- The ambient fluid in the collars really is stirred by the masks' compensating lobes. The theorem acts exactly only on neighborhoods of the rectangles.
- This is a construction demo with sampled prescribed velocity. It is not a Navier–Stokes solver benchmark. The unbounded-computation part of the paper needs exact real coordinates.
