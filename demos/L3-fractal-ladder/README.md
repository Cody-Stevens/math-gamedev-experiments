# L3-fractal-ladder: can a game run the fractal frontier on a coarse grid?

**What scales.** Grid size n. Each frame, each side samples and traces one critical q = 1 FK Dobrushin interface on an n×n grid (`../04-fractal-frontier/fk.h`, 1 thread per side, same seed on both sides). The video ladder is n = 16 … 8192 (10 stages, 3.5 s each). The sweep adds n = 8 and a timing-only n = 16384. The curve is identical on both sides, so the cost is identical too. The sides differ only in the clock. **WITHOUT** is a length clock (dt ∝ δ), tuned so it agrees with WITH at the 8192² reference, as in demo 04. **WITH** is the natural clock (dt = 1 s × δ^(7/4)), with one constant for every n. Quality is the error against an independent 6,000-curve 8192² reference, for the median curve duration and for the 8×8 occupation measure (mean time per bin, L1). Each stage uses 3,000–16,000 curves, and the 95% CIs come from 400 bootstrap resamples of both the stage and the reference.

| n | ms/curve WITHOUT / WITH | lattice steps | median err WITHOUT / WITH (95% CI) | occupation L1 WITHOUT / WITH |
|---|---|---|---|---|
| 8 | 0.0008 / 0.0009 | 135 | −99.4% / +14.5% [13.3, 15.9] | 99.4% / 17.9% |
| 16 | 0.0026 / 0.0027 | 424 | −99.0% / +6.7% [4.6, 7.9] | 99.0% / 9.2% |
| 32 | 0.0087 / 0.0088 | 1.38k | −98.4% / +3.2% [2.0, 5.0] | 98.4% / 4.7% |
| 64 | 0.031 / 0.031 | 4.6k | −97.4% / +0.9% [−0.5, 2.5] | 97.4% / 2.8% |
| 256 | 0.41 / 0.40 | 51k | −92.6% / +0.2% | 92.6% / 1.3% |
| 1024 | 4.23 / 4.29 | 570k | −79.3% / −1.6% [−3.1, 0.0] | 79.3% / 1.6% |
| 2048 | 16.3 / 16.4 | 1.92M | −65.4% / −2.0% | 65.2% / 1.6% |
| 4096 | 55.3 / 53.5 | 6.5M | −40.3% / +0.4% | 40.8% / 1.2% |
| 8192 | 154 / 148 | 21.8M | −0.1% / −0.1% | 1.1% / 1.1% (sampling-noise floor ≈ 1.6%) |
| 16384 | 487 / 491 | (timing only) | | |

**Budget and cost.** Both sides stay under the 16.7 ms budget up to n = 2048 and go over it at n = 4096 (2048 sits right at the line: 16.3 ms in the sweep, 11.5 ms in the video run). The fitted cost exponent is 1.77 over 8…16384 and 1.71 over n ≥ 256. Mean lattice steps grow as n^1.748 for n ≥ 128 (n^1.736 over 8…8192), against an expected 7/4.

**Cost to target** (|median err| ≤ 5% and occupation L1 ≤ 10%, from `ladder.json` → `cost_to_target`). WITH first meets the target at **n = 32** (0.0087 ms per curve), and the bootstrap upper bound also meets it, at 4.96%, which is marginal. n = 16 fails at +6.7%. WITHOUT meets the target only at **n = 8192**, the reference grid itself (154 ms per curve, 9× over budget). The ratio is about 17,500× in ms and 15,800× in lattice steps. For tighter targets (`target_sweep`), WITH needs n = 64 for ≤2%/4%, and the bootstrap upper bound needs n = 128. No n meets ≤1% on the bootstrap upper bound, because the 1024 and 2048 stages sit about 2% low.

**Proved vs measured.** The paper proves (Family 223, `thm:main`, `eq:counting-measure`) that c·n^(−7/4)·N_n converges in law jointly with the curve and its occupation measure. It gives no rate and does not evaluate c(1). Everything else here is measured: the finite-size drift (mean N/n^1.75 is 3.54 / 3.31 / 3.21 / 3.15 at n = 8 / 16 / 32 / 64, then flat at 3.07–3.11 against a reference of 3.116 ± 0.013), every error, every cost and both exponents.

**Caveats.**
- The WITHOUT result follows by construction. A length clock tuned at n_ref drifts as (n/n_ref)^(3/4), so the cost ratio is set by the choice of n_ref and grows like (n_ref/n)^(7/4).
- A length clock re-tuned at each n would match WITH. That re-tuning needs either a reference run per n or the classical 7/4 exponent (Beffara). The paper's new contribution is the proof that one constant with no slowly varying correction is the right choice.
- The normalized occupation shape does not depend on the clock and is the same on both sides.
- Errors include sampling noise.
- Timings below ~0.01 ms are dominated by the timer, which explains the 0.8–1.0× ratios at n ≤ 16.
- The timings were taken on a shared machine.

**Re-run** (from `demos/`, Git Bash):
- Video, about 4–5 min including about 100 s of untimed precompute: `./build.sh L3-fractal-ladder run`
- Sweep, about 3.5 min: `./build.sh L3-fractal-ladder && L3-fractal-ladder/demo.exe --sweep` → `out/ladder.json` and a printed table.

Outputs: `out/L3-fractal-ladder.mp4`, `out/poster.png`, `out/mid_frame.png`, `out/results.json`, `out/ladder.json`, `out/sweep.log`.
