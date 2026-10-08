# 05-crowd-hub: a tiny hub move can reassign a much larger crowd (lead G10.3)

**What you see:** 176,400 agents spread uniformly over the square [-1,1]² are sent to three destinations: −e₁, +e₁, and a central hub. The colors show each agent's destination, and streaks show agents travelling there. The video has 12 cycles. In each cycle the hub moves up by b = a/2 and comes back. Over the run, the hub mass a shrinks from 0.45 to 0.02 on a log scale. The inset is a log-log plot of map change against target change, and it accumulates one point per cycle.

- **WITHOUT, frozen labels:** each agent keeps the label it got while the hub was at rest. The destination masses stay correct, but the assignment is no longer optimal. Coral marks the agents that are now routed sub-optimally.
- **WITH, recomputed optimal labels:** every frame, each agent takes the argmax of the paper's three affine functions, max{−x₁, x₁, a + s·b·x₂}. These are power cells, and by Lemma `cells:optimality` they give the unique optimal quadratic-cost assignment. Yellow marks the agents that switch destination.

**Paper:** Family 374, *Sharp One-Third Stability of Brenier Maps*. The source is in `preprints/Sharp-One-Third-Stability-of-Brenier-Maps-September-25-2026/build/source/`:

- `sections/sharpness.tex`: Prop. `sharpness:exponent` and equations `sharpness:potentials`, `sharpness:targets`, `sharpness:target-distance` and `sharpness:map-distance`.
- `sections/gradient.tex`: equation `gradient:constant`. For this square it gives C* = 95.4.
- `sections/introduction.tex`: Thm `main:stability`.

All HUD numbers come from exact slice integrals (Gauss-Legendre, exact for these piecewise polynomials), not from counting sampled agents. The sampled changed fraction matches the exact value to within 2.7e-4.

| metric (cycle 0, a = 0.45 → cycle 11, a = 0.02, hub fully moved) | WITHOUT (frozen) | WITH (optimal) |
|---|---|---|
| W₂ between the old and new destination laws | 0.151 → 0.00141 | same |
| RMS change of the assignment map | 0.151 → 0.00141 (= W₂) | 0.368 → 0.0707 |
| agents relabelled | 0 % | 11.25 % → 0.50 % (= a/4) |
| transport-cost excess over the optimum | 1.69e-2 → 3.33e-5 (= a²/12, error < 4e-16) | 0 |
| log-log slope of RMS against W₂ | 1.000 | 0.422 → 0.334 (tends to 1/3) |
| compute ms/frame, median (bench.json, `--novideo`) | 0.113 | 0.230 |

**Caveats:**

- This shows a sensitivity warning for designers. It is not a faster or better allocator.
- The two panels are different allocation policies, not competing solvers for the same optimum. A classical power-cell solver would produce the same labels.
- In the paper's family the hub's mass shrinks together with a. The cube-root effect is measured against W₂, not against hub displacement at a fixed mass. With s·b = a/2, the relabelled fraction is s·b/2 for any a.
- The theorem bounds aggregate RMS change: RMS ≤ C*·W₂^{1/3}, and here RMS / W₂^{1/3} ≤ 0.69. It does not bound how far any single agent's assignment moves.
- Compute times are tiny for both panels and single-threaded. The video run measured medians of 0.139 ms and 0.196 ms per frame.
