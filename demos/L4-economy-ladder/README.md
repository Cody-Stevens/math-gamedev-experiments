# L4-economy-ladder: 01-economy run with more and more towns

**What scales.** N independent towns (economies), each running the 01-economy recipe network from its own seeded start in the class O+W+2T = 4.5. Each town's rates are the 01-economy rates times a seeded factor in [1/1.5, 1.5], and both sides use the same rates. Each stage restarts every town at t = 0 and runs 240 frames of dt = 0.5 (t = 0→120). Both sides use the 01-economy Rosenbrock 2(3) integrator (rtol 1e-6, atol 1e-14, positivity rejection, a town is halted once a resource passes 1e12) on 1 thread. **WITHOUT** has 7 recipes (2P→P and P+T+F→P+O+W removed), so it is not weakly reversible. **WITH** has all 9 recipes and is weakly reversible. The SCC check from `econ.h` decides which is which. The video runs N = 1…4,096 and the sweep runs N = 1…16,384, plus a WITH-only stage at 65,536.

| N | W/O median ms | WITH median ms | ratio | steps/frame W/O / WITH (mean) | dead or runaway towns at t=120, W/O / WITH |
|---|---|---|---|---|---|
| 1 | 0.0059 | 0.0003 | 20× | 66 / 2 | 100% / 0% |
| 16 | 0.328 | 0.0055 | 59× | 1,048 / 46 | 100% / 0% |
| 256 | 4.65 | 0.083 | 56× | 16.6k / 680 | 99.6% / 0% |
| 1,024 | 20.2 | 0.356 | 57× | 66k / 2.7k | 99.4% / 0% |
| 4,096 | 81.2 | 1.42 | 57× | 266k / 11k | 99.7% / 0% |
| 16,384 | 256 | 4.98 | 51× | 1.06M / 44k | 99.6% / 0% |
| 65,536 | not run | 18.8 | – | – / 175k | – / 0% |

Numbers come from one `--sweep` on a busy machine, so absolute times are noisy; `out/ladder.json` has every stage, p95 and mean. **Budget crossings:** WITHOUT goes over 16.7 ms between N = 256 and N = 1,024. That stage is borderline: the two video runs measured 14.4 and 16.9 ms at 1,024. WITH goes over between N = 16,384 and 65,536. **Fitted log-log slopes:** 1.07 for WITHOUT and 1.00 for WITH. Both are linear in N, as expected for independent towns. Only the constant differs, by about 50×. The ratio of mean frames is about 21× and the ratio of solver steps about 24×. The median ratio is higher because WITH's median frame comes after the towns settle, when each town takes one step per frame.

**Hypothesis test.** *Cost grows with N*: true for both sides at the same rate, and WITHOUT pays about 50× per town. *Cost grows with simulated time*: **false**. With N = 256, WITHOUT's extra cost is a spike during the collapse. In t ∈ (10,100] WITHOUT costs 54× WITH per unit sim time. Once its towns are halted at 1e12 it costs less than WITH: 0.55× in (100,1000] and about 0 after that. If towns are never halted, the ratio is 142× in (10,100], 65× in (100,1000] and 0.92× in (1000,1e4]. The collapsed towns sit at extreme values (about 1e16 and 1e-17) that change slowly, and the solver steps through them as cheaply as through WITH's equilibrium.

**Fixed-step explicit midpoint (RK2), the integrator a game would use.** Cost per frame is the same on both sides by construction. N = 256 runs to t = 1000 at h = 0.5 down to h = 1/512. At every h, ≥ 99.6% of WITHOUT towns go negative or non-finite, and at h = 1/512, 99.2% of them break after t = 10. WITH towns break only in the first moments from extreme starts: 2% after t = 10 at h = 0.5, none after t = 10 for h ≤ 0.125, and 0% in total at h = 1/512. With WITHOUT, no step size keeps the towns valid, because the runaway makes the system stiffer and stiffer.

**Proved vs measured.** The paper proves only the WITH guarantee (family 149, `thm:main` / `eq:uniform-bounds`): after a transient that depends on the start, every resource stays in [ε, 1/ε] for some ε that the paper does not compute. Everything else is measured for this network, these seeds and these rates: the collapse of WITHOUT, every cost and ratio, and the fixed-step behaviour. The paper does not say that non-weakly-reversible networks must collapse or must cost more.

**Caveats.**
- The cost gap comes from WITHOUT's collapse being stiff. It is not an algorithmic speedup, and it disappears once the collapse has happened.
- Whole-video medians in `results.json` (and the harness's printed "speedup") mix all stages, so use the per-stage values instead (`stageK_*` keys, `ladder.json`).
- Both sides run on 1 thread. Towns are independent, so threading would divide both sides' cost alike.

**Re-run (Git Bash, from `demos/`).** `./build.sh L4-economy-ladder run` records the video (about 1 min; the video stops at 4,096 towns). `./build.sh L4-economy-ladder run --sweep` runs the sweep and writes `out/ladder.json` (about 2.5 min). Options: `--max-n`, `--with-ext-n`, `--horizon-n`, `--fixed-n`, `--jit`, `--seed`. Code: `main.cpp`, plus `econ.h` (network, SCC check and integrator, copied from `01-economy/main.cpp`).
