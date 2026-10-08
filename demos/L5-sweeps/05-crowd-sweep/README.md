# 05-crowd-sweep: frozen vs recomputed optimal labels vs crowd size (family 374)

**Load:** agents on [-1,1]², 10k → 4M as requested. Two extension stages, 8.41M and 16M, were added because they were cheap and they locate the budget crossing. Each stage replays the original hub cycle at a = 0.45 (125 frames). **WITHOUT** uses frozen labels: agents are relabelled only while the hub is at rest, then streamed toward stale destinations. **WITH** recomputes the optimal power-cell labels every frame, argmax{−x₁, x₁, a + s·b·x₂} (Lemma `cells:optimality`, `sharpness.tex`). Both sides time exactly what the original demo timed: labelling plus streaming every 4th agent. Both run on **1 thread**.

The machine was shared and loaded during this run. A quieter rerun measured roughly half of these times, for example WITH at 4M was 4.4 ms.

| agents | WITHOUT median (p95) ms | WITH ms | W/O ÷ WITH | mis-routed, sampled (exact 0.1125) | cost excess, sampled (exact 0.016875) |
|---|---|---|---|---|---|
| 10k | 0.0041 (0.015) | 0.0147 | 0.28 | 0.1140 | 0.01702 |
| 40k | 0.0153 (0.059) | 0.0569 | 0.27 | 0.1127 | 0.01686 |
| 160k | 0.067 (0.26) | 0.231 | 0.29 | 0.1126 | 0.01687 |
| 640k | 0.53 (1.68) | 1.08 | 0.49 | 0.1125 | 0.01688 |
| 2.56M | 2.61 (7.40) | 5.36 | 0.49 | 0.1125 | 0.01688 |
| 4M | 4.87 (11.8) | 8.75 | 0.56 | 0.1125 | 0.01688 |
| 8.41M (ext) | 9.31 (23.4) | 20.1 | 0.46 | 0.1125 | 0.01688 |
| 16M (ext) | 21.1 (47.5) | 42.4 | 0.50 | 0.1125 | 0.01688 |

WITH has 0 mis-routed agents and 0 cost excess at every stage.

- **Budget (16.7 ms):**
  - WITH crosses between 4M and 8.4M agents.
  - WITHOUT's median frame crosses between 8.4M and 16M. Its relabel frames (p95) already cross between 4M and 8.4M.
- **Fitted exponents:** WITHOUT 1.19, WITH 1.09. Both are linear scans. Cache effects push the larger stages slightly above 1.
- **Quality:** load-independent in exact terms. At the full hub move, frozen labels route a/4 = 11.25 % of agents sub-optimally, with transport-cost excess a²/12. The sampled values converge to the exact ones as n grows.

**Proved vs measured:**
- **Proved:** the power-cell labels are the unique optimal assignment (Lemma `cells:optimality`). The optimal map's RMS change scales like W₂^(1/3), which is sharp (Prop. `sharpness:exponent`).
- **Measured here:** cost per frame and the sampled mis-routed fraction.

**Caveats:**
- WITH is a policy, not a faster solver. Any classical power-cell labeller gives the same labels.
- WITHOUT's median is a frozen frame, which does no labelling. Its relabel frames cost about as much as a WITH frame.
- Neither side is multithreaded.

**Rerun:** `./build.sh L5-sweeps/05-crowd-sweep && L5-sweeps/05-crowd-sweep/demo.exe`. It takes about 45 s and peaks near 600 MB at 16M agents. `--max-side 2000` stops at the requested 4M.
