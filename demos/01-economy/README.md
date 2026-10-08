# 01-economy: a weakly reversible recipe graph keeps every resource alive and bounded

**What you see.** Five resources (Ore, Wood, Tools, Food, Workers) are drawn as glowing orbs whose radius tracks log quantity. Particles flow along the recipe arcs at a rate set by log flux. The economy has nine mass-action recipes in three coupled linkage classes, and each class is a directed 3-cycle: village `P+F→2P→P→P+F`, forge `O+W→T→2O→O+W`, workshop `P+T+F→P+O+W→2P+T→P+T+F`. Six very different starts are integrated side by side, all in one stoichiometric class (O+W+2T = 4.5). The bottom plot shows all 30 trajectories on a log scale.
**WITHOUT** has the same recipes and rates with two return reactions removed (`2P→P` and `P+T+F→P+O+W`). **WITH** has all nine recipes. Both sides use the same Rosenbrock 2(3) adaptive integrator (rtol 1e-6, atol 1e-14) with positivity rejection and step halving. The code computes the "weakly reversible" verdict from the strongly connected components of the complex graph.

**Paper.** Family 149, `preprints/Uniform-Permanence-in-Weakly-Reversible-Mass-Action-Systems-October-5-2026/build/sections/`. The ODE is §1 `eq:mass-action`. The guarantee is `thm:main` / `eq:uniform-bounds`, proved through §3 `prop:plateau`, `prop:increase`, `lem:trapping` and §4 `eq:scale-sequence`.

**Why the theorem is needed for this network.** All of these properties are computed on screen and written to results.json:
- 5 species, so the two-species result (CNP13) does not apply.
- 3 coupled linkage classes, so the single-linkage results (BH20, GMS14) do not apply.
- dim S = 4, so the dim S = 2 result (Pantea12) does not apply.
- The deficiency is 2 and the complex-balance residual at equilibrium is 0.36, so the network is not complex-balanced.
- The witness w = (0,0,0,−1,−1) shows the network is not strongly endotactic.
- The class is unbounded, because Food and Workers are free directions of S. Craciun 2026 covers only bounded trajectories.

| metric (bench.json, `--novideo`) | WITHOUT | WITH |
|---|---|---|
| min / max resource at end | 1.6e-12 / 1.0e12 (run halted) | 0.48 / 1.54 |
| resources extinct (<1e-6) / runs that ran away | 6 of 30 / 6 of 6 | 0 / 0 |
| conservation residual \|Δ(O+W+2T)\| | 7.5e-15 | 2.0e-15 |
| compute, 6 starts per frame (median) | 0.017 ms | 0.0029 ms |

WITH's empirical envelope for t ≥ 40 across all six starts is [0.475, 1.547].

**Caveats.**
- The theorem proves that *some* ε_P exists with ε_P ≤ x_i ≤ 1/ε_P after a transient that depends on the start. It does not give the value of ε_P or the entry time, and neither does this demo. The band in the plot is empirical, measured from a finite trace.
- The collapse on the left is one demonstrated case. It is not a theorem that every network that fails weak reversibility fails this way: removing some single return reactions leaves the system stable.
- The guarantee is for continuous mass-action dynamics with fixed positive rates. It says nothing about integer inventories, stochastic extinction, rates the player changes, or a careless fixed timestep.
- WITHOUT costs more compute only because its blow-up is stiff (100k vs 11k solver steps). This is not a speedup, so the speedup line is hidden.
- `analysis.py` is the offline SciPy cross-check. Its final states agree with the C++ run.
