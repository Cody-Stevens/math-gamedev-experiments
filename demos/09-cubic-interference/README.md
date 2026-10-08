# 09-cubic-interference: cubic phasor walks with a persistent, certified bias

**What you see.** For each prime p ≡ 1 (mod 3), X sweeps from 60 to 250,000 (10,996 primes). The featured panel draws a phasor walk Σ e(·) scaled by 1/√p, with a strip of recent walks below it. Each walk's endpoint feeds a running total M(X) = Σ S_p/(2√p).

- **WITHOUT:** what a designer would assume, namely that the sum behaves like a random arrow. The gallery walks use p independent uniform phases with the same renderer and normalization. The statistical reference is the uniform-circle model S_p/(2√p) = cos θ_p, with mean 0 and variance 1/2, shown as 48 repetitions and a ±1σ/±2σ band. The true M(X) appears as a ghost line.
- **WITH:** the true cubic sums S_p = Σ_x e(x³/p), using exact integer residues of x³ mod p and a direct O(p) sum. They are overlaid with the proved asymptotic M(X) ~ K·X^(5/6)/log X, where K = (2π)^(2/3)/(5Γ(2/3)) = 0.50291. The plot also shows the paper's elementary model term Σ c*·p^(-1/6) (violet), which has the same asymptotic.

**Paper.** Family 023, `preprints/An-unconditional-first-moment-for-cubic-Gauss-sums-September-25-2026/build/paper.tex`, subsection "The rational-prime convention": `eq:pairing` (G(π)+G(π̄) = S_p/√p) and `eq:rational-main`. This is the rational-prime coefficient, half of the all-primary-prime one. The two must not be mixed.

| metric (X = 250,000) | WITHOUT (random-arrow model) | WITH (true sums + theorem) |
|---|---|---|
| predicted M(X) | 0 ± 74.1 (1σ) | 1274.5 |
| observed M(X) | random-walk control −29.0; true M = 1367.3 (+18.4σ) | 1367.3 (ratio 1.073) |
| check against the verifier | — | M(8191) = 98.808 vs 101.823 (verifier: identical) |
| compute ms/frame, median (`bench.json`, `--novideo`) | 11.7 | 11.1 |

|Im S_p| ≤ 4.9e-12, which is pure floating-point error because S_p is real by `eq:pairing`.

**Caveats.** Both sides do the same O(p) phasor work, so there is **no speedup**. The theorem supplies no faster algorithm. Kummer's positive bias and the phases themselves are classical. What is new is the unconditional asymptotic with this normalization, so the right panel's prediction is certified. The error is only o(main term), with no finite-X bound. The observed ratio of about 1.07 at X = 2.5e5 is consistent with that but is not explained by it. The elementary model sum (1424) overshoots by a similar amount. The bias is visible empirically at demo scale: 18σ against the random model. Random arrows reproduce neither the bias nor the cubic sums' endpoint variance.
