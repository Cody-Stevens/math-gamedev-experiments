# 06-dungeon-sweep: cost of a certified-fair dungeon shuffle vs room count (family 131)

**Load:** rooms n = 10, 20, 40, 80, 160, 320. Each template is a grid of rooms with about 19 % of the corridors removed and about 3n/20 hub "secret passages". Door counts are 1–6 (1–7 at n = 320), with about 1.4–1.7 doors per room. n = 20 is the original template exactly. One frame generates one complete layout, timed on 1 thread.

| series | method | steps per layout |
|---|---|---|
| **WITHOUT** | lazy double-edge swap, ad hoc burn-in of 1 swap per door | m |
| **empirical** | Curveball with the same ad hoc budget, no certificate | m trades |
| **WITH** | Curveball with the §7 certified burn-in | T = ⌈B(1+B/2)·ln 2⌉, B = C(n,2) |

The WITH budget follows from `mix:H-poincare`: the spectral gap is at least 1/B, which gives TV ≤ ¼ from every start.

| n | m | certified T | WITHOUT ms | empirical ms | WITH ms (frames) | autocorr W/O · emp · WITH (EMPIRICAL) | kept-doors hist TV W/O · emp · WITH |
|---|---|---|---|---|---|---|---|
| 10 | 12 | 734 | 0.00020 | 0.00033 | 0.0119 (3519) | +0.259 · +0.114 · −0.008 | 0.45 · 0.23 · 0.02 |
| 20 | 28 | 12,644 | 0.00034 | 0.00070 | 0.334 (2172) | +0.174 · +0.067 · −0.004 | 0.65 · 0.30 · 0.02 |
| 40 | 60 | 211,397 | 0.00061 | 0.0017 | 6.28 (233) | +0.152 · +0.061 · −0.000 | 0.87 · 0.50 · 0.02 |
| 80 | 127 | 3.46 M | 0.0016 | 0.0056 | 200 (15) | +0.149 · +0.055 · +0.002 | 1.00 · 0.74 · n/a (735 samples) |
| 160 | 261 | 56.1 M | 0.0033 | 0.014 | 5,193 (1) | +0.138 · +0.044 · −0.001 | 1.00 · 0.92 · n/a (28 samples) |
| 320 | 535 | 903 M | 0.0076 | 0.039 | 82,920 (1) | +0.137 · +0.047 · 0.000 | 1.00 · 1.00 · n/a (1 sample) |

- **Budget (16.7 ms):**
  - WITH crosses between n = 40 (6.3 ms) and n = 80 (200 ms). Log-log interpolation between those two stages puts the crossing at about 49 rooms (interpolated, not measured).
  - WITHOUT and empirical stay within budget at every stage, with large headroom.
- **Fitted exponents:**

  | series | all stages | upper half |
  |---|---|---|
  | WITHOUT | 1.07 | 1.11 |
  | empirical | 1.40 | 1.40 |
  | WITH | 4.58 | 4.35 |

  The step count alone grows as n^4.05. The cost per trade also rises from 0.016 µs to 0.09 µs as n grows, because there are more singletons per trade and more words per adjacency row.
- **Quality proxy (EMPIRICAL, not TV):** "template doors kept" per layout, compared with a long-run reference (a swap chain mixed for at least 200m swaps; a Curveball run cross-checks it).
  - autocorr = (mean kept − ref)/(m − ref). 0 means uniform.
  - The kept-doors histogram TV has a sampling floor of about 0.01–0.025 at 4,000 samples. The reference mean itself is uncertain by about ±0.005 in autocorr, which explains WITH's −0.008 at n = 10.
  - Both ad hoc budgets stay biased toward the template at every n, about +0.14 and +0.05. Their histograms become almost fully distinguishable from uniform as n grows (TV → 1).
  - Exact TV is only computable for tiny graphs. The original demo's n = 6 and n = 8 toys give swap 0.215/0.459 and certified Curveball about 1e-14.

**Proved vs measured:**
- **Proved:** the TV ≤ ¼ certificate for every start and every graphical degree sequence. The O(n⁴) budget is a corollary of `mix:H-poincare`. The headline theorem is the 2n⁸ single-switch bound.
- **Measured:** the cost and the empirical proxy.
- **What the certificate is not:** the actual mixing time. Much shorter runs may already be close to uniform, but nothing certifies them.

**Caveats:**
- n = 160 and n = 320 have only 1 timed WITH layout each. The quality samples beyond the timed ones are generated untimed on 12 threads.
- Samples are uniform over all labeled realizations, including disconnected dungeons.

**Rerun:** `./build.sh L5-sweeps/06-dungeon-sweep && L5-sweeps/06-dungeon-sweep/demo.exe`. It takes about 150 s, of which the single n = 320 certified layout takes about 83 s. `--max-n 160` takes about 60 s.
