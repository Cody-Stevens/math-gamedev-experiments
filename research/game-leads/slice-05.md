# Slice 5 games/sim findings (families 153-191)
Families scanned: 38. Families with findings: 153, 165, 173, 187, 188, 189.

All family descriptions and manuscript abstracts in CONTENTS.md lines 3666-4433 were read. Family 163 is absent from that range. Paper checks confirm the statements and implementation ingredients below; they are not independent audits of the complete proofs.

## G5.1: A guaranteed Snaky opponent from a 39 KB certificate

- Family / paper: 187 — Snaky in 21 Maker moves — `preprints/Snaky-in-21-Maker-moves-September-25-2026/build/paper.tex`; also opened `build/introduction.tex`, `build/templates.tex`, and `build/strategy.tex` in that directory, plus `verification/certificate.txt` and `verification/supporting/route21/primary.py`.
- Quote: "We prove that Maker can achieve the Snaky hexomino within 21 actual Maker moves against arbitrary legal Breaker play on the initially empty infinite square board."
- Kind: gameplay | performance
- Idea: Build a shape-completion game with a Maker opponent that has a stated worst-case win bound against any legal human Breaker. The target is the six cells `{(0,0),(1,0),(2,0),(3,0),(3,1),(4,1)}`, allowing translations, rotations, and reflections; Maker moves first and both players claim one free cell per turn. The paper supplies 728 strategy cards, including six bases, with an empty starting requirement, a 251-cell envelope, and height 21 inside a 17×17 board. A small interpreter follows pivots and chooses the first child envelope avoiding the latest Breaker move, replacing an already-owned pivot with a fresh claim.
- Demo sketch: WITH the math, load the supplied roughly 39 KB certificate, reconstruct its sets and transformations, and play its policy against a human or seeded adversarial Breaker. WITHOUT, use a fixed-budget shallow search that scores unblocked target copies, with the same board and rules. Report win rate, maximum Maker claims before victory, and milliseconds per decision; the certificate removes the need to search the full game tree during play.
- Proven vs. speculative: The paper gives a legal policy winning within 21 actual Maker moves on the empty finite board; this is an upper bound, not a claim of optimality, and applies to Maker–Breaker rather than a game where both players seek Snaky. The supplied reconstruction checker passed after CRLF was normalized to LF in memory only, confirming 728 cards, 1,620 combination nodes, and the stated endpoint; a C/C++ policy interpreter still needs implementation tests before claiming the guarantee in a shipped game.
- Wow: high · Confidence: high · Effort: M

## G5.2: A network puzzle with a certified crossing target

- Family / paper: 165 — The crossing number of complete graphs — `preprints/The-crossing-number-of-complete-graphs-September-23-2026/build/main.tex`.
- Quote: "Its two-page case attains the classical upper bound."
- Kind: gameplay | visualization | performance
- Idea: Turn complete-graph routing into a puzzle with an exact attainable minimum crossing score: `Z(n) = floor(n/2) floor((n-1)/2) floor((n-2)/2) floor((n-3)/2) / 4`. Place vertices `0,...,n-1` along a line and draw edge `ij` above it when `(i+j) mod n < floor(n/2)`, below otherwise. The drawing rule is cheap, and the formula supplies a constant-time stopping target for layout search. Use curved arcs and unconstrained routing, since straight-line layouts and obstacles impose different problems.
- Demo sketch: WITH the math, render the endpoint-sum assignment using semicircles; WITHOUT, assign the same edges randomly to the two pages with identical positions and rendering. Count crossings through alternating endpoints among edges on the same page, perturbing any coincident crossings slightly, and report excess above `Z(n)`, generation time, and drawing FPS. A read-only arithmetic check matched this construction's count to the formula for `n=3,...,32`.
- Proven vs. speculative: The manuscript proves the formula for ordinary drawings with unrestricted continuous edge arcs and supplies the matching construction. The drawing itself is classical, so the new contribution is the claimed global optimality certificate, not a faster newly invented renderer; arbitrary graphs, fixed obstacles, and straight-line crossing minima are outside its guarantee.
- Wow: med · Confidence: med · Effort: S

## G5.3: Overlapping random branches with singular limiting mass

- Family / paper: 153 — Arithmetic classification and non-Pisot singularity for Bernoulli convolutions — `preprints/Arithmetic-classification-and-non-Pisot-singularity-for-Bernoulli-convolutions-October-3-2026/build/main.tex`; also opened `build/salem.tex` in that directory.
- Quote: "The criterion is expressed through one-sided approximation by explicitly defined finite sets of algebraic units; it is an infinite approximation condition, not a finite membership algorithm."
- Kind: visualization | gameplay
- Idea: Make a randomness microscope using `X = (1-λ) Σ U_j λ^j`, with independent fair bits `U_j`. Two explicit parameters come from `y=(1+sqrt(13))/2` or `y=1+sqrt(2)`, then `β=(y+sqrt(y*y-4))/2` and `λ=1/β`. Both have `λ>1/2`, so their branches overlap and the limiting support is the full interval, yet the paper proves that their limiting probability measures are singular. These give specific arithmetic choices for procedural noise or loot-distribution experiments beyond the familiar separated Cantor construction.
- Demo sketch: WITH the math, sample those two parameters and display histograms at successively finer resolutions, cumulative mass, and the fraction of bins holding 90% of samples. WITHOUT, run the same normalized sampler at `λ=1/2`, whose limit is uniform on `[0,1]`, with equal sample budgets and bin sizes. Report sampling milliseconds, concentration across resolutions, and uncertainty; truncation after `N` bits has deterministic tail error at most `λ^N`, which must stay well below bin width.
- Proven vs. speculative: The paper proves singularity at reciprocals of every quartic Salem number in `(1,2)`, including these explicit examples; affine normalization preserves singularity. Visible finite-resolution contrast and useful gameplay behavior remain speculative: no practical visibility scale is supplied, floating-point parameters are approximations, and neither histograms nor the infinite classification furnish a finite singularity test.
- Wow: med · Confidence: med · Effort: S

## G5.4: Predict the debris left by random triangle destruction

- Family / paper: 188 — The sharp terminal leave in random triangle removal — `preprints/The-Sharp-Terminal-Leave-in-Random-Triangle-Removal-September-25-2026/build/paper.tex`; also opened `build/introduction.tex` and `build/queries.tex` in that directory.
- Quote: "In particular, the same limit holds in probability and for the normalized expectation."
- Kind: visualization | sim-design-rule
- Idea: Start with all pairwise links among `n` particles, repeatedly remove the three edges of a uniformly chosen surviving triangle, and stop when none remains. The paper predicts terminal debris `F ≈ n^(3/2)/(2 sqrt(2))`, giving a parameter-free asymptotic target for a triangle-free network generator or a visual destruction toy. The accepted triangles also partition the removed links into groups of three, leaving a measurable uncovered-pair budget. This is new analysis of an existing process, rather than a new removal algorithm.
- Demo sketch: Shuffle all initial vertex triples once and scan them, accepting only triples whose three edges remain; the paper shows that accepted steps have exactly the uniform sequential-removal law. WITH the math, overlay the stated terminal prediction for several modest `n`; WITHOUT, predict `C n^(3/2)` with `C` fitted only at the smallest training size, using the same simulation engine and seeds. Report held-out prediction error, the sample mean and spread of `F/n^(3/2)`, and milliseconds per accepted removal.
- Proven vs. speculative: The paper proves mean-square convergence of `F/n^(3/2)` to `1/(2 sqrt(2))` specifically from the complete graph. It supplies no practical finite-`n` error bar, sharp constant for other starting graphs, or speed improvement; the theoretical predictor may lose to a fitted baseline at small sizes.
- Wow: med · Confidence: high · Effort: S

## G5.5: Choose a start with at least as many fresh two-hop options

- Family / paper: 173 — A proof of Seymour's second-neighborhood conjecture — `preprints/A-proof-of-Seymours-second-neighborhood-conjecture-September-23-2026/build/source/00-introduction.tex`; also opened `04-weighted-consequences.tex` in that directory. The abstract was read at `preprints/A-proof-of-Seymours-second-neighborhood-conjecture-September-23-2026/paper.pdf` in CONTENTS.md.
- Quote: "We prove that every nonempty finite oriented graph has a vertex with at least as many vertices at directed distance two as at directed distance one."
- Kind: gameplay | sim-design-rule
- Idea: Model one-way travel, upgrades, or faction influence as an oriented graph: no loops or opposite edge pairs. The paper guarantees a start whose distinct destinations at distance exactly two number at least its direct destinations, excluding the start and all direct neighbors and counting duplicate routes only once. Its nonnegative vertex-weighted corollary gives the same comparison for destination rewards. This supplies a testable rule for selecting a start with enough future options, without restricting the graph to a tournament or a special geometric map.
- Demo sketch: Generate a directed Hamiltonian cycle, then add chords while forbidding opposite arcs, so every vertex has positive outdegree and a sink cannot satisfy the test vacuously. WITH the math, scan direct and exact-two-hop sets and choose a qualifying start, optionally using positive destination rewards; WITHOUT, choose the start with the greatest immediate reward. Highlight both sets and measure the second-hop-minus-first-hop reward margin, distinct new options, and selection time; adjacency bitsets or a simple cubic scan suffice.
- Proven vs. speculative: The manuscript proves the unweighted existence claim and derives the reward-weighted version through established equivalences; checking a candidate's two-hop sets is an ordinary finite graph calculation, not a new faster pathfinding algorithm. The game interpretation is speculative: the theorem guarantees one suitable vertex, not fair starts everywhere, a winning policy, or higher eventual reward.
- Wow: med · Confidence: med · Effort: S

## G5.6: An exact board-size dial for cycle-or-clique puzzles

- Family / paper: 189 — Cycle--clique Ramsey numbers — `preprints/Cycle-clique-Ramsey-numbers-September-25-2026/build/main.tex`; also opened `build/sections/01-introduction.tex` in that directory.
- Quote: "The cycle length in the theorem is exact."
- Kind: gameplay | sim-design-rule
- Idea: Let a player color the edges of a complete graph red or blue while trying to avoid both a red cycle of exactly `m` vertices and a blue `n`-clique. The paper gives the exact unavoidable board size `(m-1)(n-1)+1` for `m>=n>=3`, except `(3,3)`, whose threshold is six. One vertex below it, use `n-1` disjoint red cliques of size `m-1` with all intergroup edges blue: this explicitly avoids both targets. A formula can therefore distinguish a solvable avoidance board from an impossible completed board, without enumerating all colorings to determine that threshold.
- Demo sketch: WITH the math, render the explicit below-threshold coloring and add one vertex, showing the exact threshold; WITHOUT, choose the board size from a fixed budget of random-coloring trials and report its threshold error against the formula. For a compact interactive run use `m=5,n=4` (12 versus 13 vertices) and search for motifs by bounded-size backtracking, reporting witness-search milliseconds and coloring attempts; this small case was already known before the paper. A 64-versus-65-vertex `m=n=9` view can illustrate the wider parameter range, but arbitrary witness search there belongs offline rather than in an unbounded frame update.
- Proven vs. speculative: The manuscript proves the threshold over all completed colorings in the stated range; the extremal coloring is classical, while the new contribution completes the range beyond prior results. It gives no practical worst-case algorithm for finding the forced motif, and the Ramsey statement alone is not a turn-by-turn strategy for an alternating edge-coloring game.
- Wow: med · Confidence: med · Effort: M

## Near misses

- 155 — `preprints/A-translational-tile-with-no-fully-periodic-tiling-in-dimension-three-September-23-2026/build/sections/07-three-dimensions.tex`: "No practical bound on the size of the finite construction is required." The 3D aperiodic tile requires finite searches without a usable asset-size bound; drawing its digit-pattern witness would not reproduce the actual tile.
- 178 — `preprints/Deterministic-nonbipartite-Ramanujan-graphs-in-every-fixed-degree-September-23-2026/build/sections/setup.tex`: "Constants may depend on $d$ and on the fixed parameters and moment orders chosen below, but never on $n$." The full deterministic construction is galactic for this purpose: its setup calls for over three million auxiliary parameters, and no practical polynomial exponent or starting-size threshold is supplied; random generation plus a spectral check would be a different algorithm.
- 191 — `preprints/A-power-improvement-in-the-Heilbronn-triangle-lower-bound-September-25-2026/build/sections/08-alteration.tex`, with `03-norm-digits.tex` also opened: "This final step is an elementary alteration argument." The new improvement is galactic: `d=41`, `k=binom(binom(163,41),3)^2+1 ≈ 1.4×10^231`, with `3k` digits per sampled column; modest finite-field parabola/cap pictures are explicitly classical ingredients, not the new planar triangle-area bound.
