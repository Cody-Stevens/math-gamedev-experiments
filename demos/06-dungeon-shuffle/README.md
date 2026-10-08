# 06-dungeon-shuffle: fair random dungeon layouts with an exact door count per room

**What you see.** A designer template dungeon has 20 labeled rooms on a grid and 28 doors. The degree sequence is irregular (door counts 1 to 6, shown as pips). Each level replays one shuffle from the template, and the dungeon rewires live: grey corridors are template doors, blue corridors are new doors, and flashes and red ghosts mark changes. In every frame each side also generates one complete fresh layout from the template, and only that work is timed. The strip under the map is a histogram of "template doors kept" over every generated layout (1,560 per side), drawn against the uniform reference. The bottom-right plots show **exact** total-variation distance to uniform for two cases, computed by enumerating every graph and propagating the full transition law: n=6 with all degrees 2 (70 graphs) and an irregular n=8 sequence d=(5,4,3,3,2,2,2,1) (623 graphs).
**WITHOUT** is the conventional lazy double-edge swap with an ad hoc burn-in of one swap per door (28). **WITH** is the §2 Curveball pair trade: a uniform pair, with the singletons re-dealt by an unbiased partial Fisher–Yates shuffle. It runs for the burn-in certified by §7: `mix:H-poincare` Var f ≤ ⟨f,Hf⟩ implies that K = I − H/B is PSD with gap ≥ 1/B, which gives TV ≤ ½√|Ω|·(1−1/B)^t ≤ ¼ at **T = ⌈B(1+B/2)·ln 2⌉ = 12,644** (B = C(20,2) = 190).

**Paper.** Family 131, `preprints/Polynomial-Mixing-of-the-Switch-Chain-for-Every-Graphical-Degree-Sequence-September-25-2026/build/sections/`: setup.tex (`eq:pair-operators`, the fiber = uniform q-subset), mixing.tex (`mix:H-poincare`, `mix:TV`) and introduction.tex (`thm:main`, 2n⁸). The O(n⁴) trade budget is the verifier's corollary of `mix:H-poincare`. It is not the headline theorem.

| metric | WITHOUT (28 swaps) | WITH (12,644 trades) |
|---|---|---|
| exact TV to uniform, n=6 d=2 / n=8 irregular | 0.215 / 0.459 | 4.5e-16 / 1.8e-14 |
| edge autocorrelation vs template (template doors kept, uniform ref 5.64) | +0.185 (9.77) | −0.0007 (5.63) |
| compute per layout, median (bench.json) | 0.0027 ms (0.10 µs/step) | 0.344 ms (0.034 µs/step) |

For every t, the exact TV of the Curveball chain stays below the certified bound.

**Caveats.**
- The paper certifies **uniformity, not speed**. WITH costs about 128× more compute per layout. Even so, 0.34 ms per layout is cheap for level generation.
- Both chains have the uniform law as their stationary distribution. The double-edge swap also mixes fast here empirically: on the n=8 toy its TV is 1.4e-4 after 10 swaps per door. What it lacks is a guarantee for an arbitrary degree sequence.
- Curveball with the ad hoc budget would also fall short (TV 0.108 / 0.152 on the toys). The paper's contribution is the budget, not the shuffle; Curveball is due to Carstens–Berger–Strona.
- The guarantee is TV ≤ ¼ for *every* start and every graphical sequence. For a smaller ε, use T = ⌈B(ln(1/2ε) + ½ ln|Ω|)⌉.
- Samples are uniform over **all** labeled realizations, including disconnected ones (about 0.4% here). Rejecting disconnected or geometrically invalid moves *inside* the chain breaks the guarantee. Filtering finished samples keeps the result uniform over the accepted set.
- The O(n⁴) budget can get expensive for large n.
- The n=6 regular toy was already covered by older theory. The irregular n=8 sequence and the dungeon show the wider scope.
