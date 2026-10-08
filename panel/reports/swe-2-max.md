I'll read the brief's evidence files. Starting with the directory layout.Now the verified leads table — the key source for undemoed SOLID leads.Nine demos map to leads G1.3, G3.2, G4.1, G4.4, G5.1, G6.2, G10.1–3. Now reading the verification write-ups for the undemoed SOLID leads.All evidence read. Writing the report.

---

# Panel report: bridging pure-math results to game wins

## 1. What is actually happening

- **01-economy (weakly reversible recipes):** a shape test on the recipe graph — does every reaction chain eventually feed back into what it consumes — decides whether a continuous economy can permanently extinct or run away. *Gain: design rule.* The ~50× compute gap is only the broken version's stiff collapse, not a speedup.
- **02-fluid-gates (solenoidal shear schedule):** move dye regions through each other with exactly zero overlap and zero volume change by briefly lifting each into a private extra dimension. *Gain: guarantee, paid for at 6–45× the classical cost.*
- **03-snaky (certificate policy):** a paper proof of a winning strategy is compiled into a lookup table — the AI reads a card instead of searching. *Gain: speed + guaranteed correctness* (~4.7 ms → ~1 µs per decision).
- **04-fractal-frontier (natural clock):** the correct animation speed for a fractal front is step-length^1.75, not step-length; one constant then gives identical pacing at every grid density. *Gain: design rule / calibration* enabling a huge effective speedup.
- **05-crowd-hub (Brenier-map instability):** a warning — slightly nudging a destination forces a truly-optimal allocator to relabel ~11% of agents. *Gain: warning about what not to build.*
- **06-dungeon-shuffle (certified burn-in):** a fixed trade count after which a degree-preserving layout shuffle is *provably* uniform, for any door-count pattern. *Gain: correctness/fairness*, ~128× the lazy shuffle's cost but sub-millisecond.
- **07-periodic-blob (isoperimetric classification):** the minimum-surface shape on a wrap-around cube is provably always ball, tube, or slab — evaluate a formula instead of iterating a solver. *Gain: exact target + speed + reliability.*
- **08-convex-morph (log Wulff morph):** a different interpolation formula between symmetric convex bodies that cannot deflate below the geometric-mean volume and bloats less than the standard morph. *Gain: guarantee; speed is incidental.*
- **09-cubic-interference (first moment of cubic Gauss sums):** sums that look like random drift have a certified positive bias ~X^{5/6}/log X. *Gain: content/knowledge — no speedup, no knob.*

## 2. Bridge patterns

- **A proved strategy is data.** Compile a winning-strategy certificate offline into a lookup table; runtime becomes one table step per opponent move — O(1), budget-independent, and correct by construction. (Snaky. The certificate — not the strategy — is the paper's contribution: 38 KB of text becomes 94 KB of tables beating a search AI 100–0 while using ~10⁴–10⁵× less compute per move.)
- **A named global optimum replaces a solver.** When the theorem proves *which* shape is optimal, "optimization" becomes "evaluate a formula," and the theorem's profile I(V) becomes a unit-test oracle for any solver you keep. (Periodic blob; same idea in the convex-morph construction.)
- **Theorem hypotheses are lint rules.** Whatever the theorem *assumes*, check on designer content at build time: weak reversibility on recipe graphs (economy), the dλ²>1 boundary on rumor trees (G7.1), the √n window on puzzle size (G7.6), the noise-budget ceiling on utility-AI scores (G3.4). One-sided, cheap, run in the editor, not the frame loop.
- **The theorem's exponent is your normalization constant.** A proved scaling law makes one parameter mean the same thing at every resolution: δ^{7/4} for the frontier clock; mean-pivotal-count normalization for Voronoi connectivity sliders (G6.3); the arcsin(c/2) variance multiplier for constrained surfaces (G6.1).
- **Hardness and sensitivity theorems are "do not build here" signs.** Sharp impossibility/instability results tell you which mechanics can't be stabilized: optimal labels churn under nudges (crowd hub), information dies below a threshold (G7.1), fair card marginals still allow a rigged deck order (G7.4).
- **A certified limiting distribution is a cheap proxy sampler.** Where the paper identifies a classical distribution as the correct target, the classical sampler (e.g. PD(1) stick-breaking for prime-predecessor fragment sizes, G1.1) is validated for that use — worth knowing, though the sampler itself is old.

## 3. Top 5 practical methods

### 1. Certificate-compiled adversary (Snaky, family 187)
*Situation: any fixed-rules adversarial system — board/puzzle AI, a scripted boss, guaranteed-solvable generated challenges — and any place you want thousands of competent NPC decisions inside one frame.*

> Some games have a mathematically guaranteed way to win, like perfect tic-tac-toe. Normally a computer finds its move by searching thousands of futures each turn — slow, and it gets worse under time pressure. Here, researchers proved a complete winning recipe once, on paper, written as 728 small cards of "if they did this, do that." We turned that proof into a lookup table. The AI stops thinking: it reads the right card in about a millionth of a second, never loses, and you can run over a hundred thousand games inside a single video frame.

- **What the math adds:** the certificate itself. Classical α-β/MCTS searches can win but never *know* they will, and degrade with budget. The proof supplies the policy structure and the ≤21-move bound. Our port is verified (728 cards, 37,042 reply classes, SHA-matched).
- **Gain/size:** speed + correctness. Measured: ~4.7 ms → ~0.5–1.2 µs per decision; ratios ×5·10⁴–1.2·10⁵ per decision; one 16.7 ms frame holds 131,072 concurrent games vs 4 for the upgraded search. Under a fixed frame budget the search's win rate fell 100% → 43% → 0% as load grew; the certificate stayed 100%.
- **Bridge:** offline, someone must produce a strategy certificate for *your* game — that's the research-hard step. Runtime is a parser + card interpreter (few hundred lines; ours exists). It replaces your "choose move" call; memory ~94 KB. Even without a theorem, the reusable lesson is: distill any expensive fixed policy to a table at build time.
- **Risks:** certificates exist for almost no shippable games; the bound is worst-case, not optimal play; scoped to Maker–Breaker rules, not the strong game. Without a certificate this reduces to "opening book," which devs already do.
- **Visual:** split canvas, left a grid playing "claim-the-snake-shape" vs a random blocker under a per-frame time budget with a game-count slider — the searcher visibly degrades past ~32 games; right the same load with instant moves, the live teal "envelope" region drawn, readouts of µs/decision and win%. ~150 lines with a hardcoded toy win-line as an honest scaled-down stand-in.

### 2. Closed-form optimum instead of an iterative solver (periodic blob, family 354)
*Situation: voxel/metaball/goo content on wrap-around maps, minimum-surface or packing relaxations — and generally, anywhere you'd iterate toward a shape the theory already names.*

> You want the smoothest blob holding a fixed volume on a world that wraps at the edges. The usual trick is melting: nudge the surface thousands of times and hope it settles. About one run in nine gets stuck in a wrinkled dead-end and never reaches the best shape. The math proves the answer is always one of three boring shapes — a ball, a tube looping the map, or a flat slab — and exactly when each applies. So skip the melting: just draw the right shape. It's knowing the crossword answer instead of guessing letters.

- **What the math adds:** certification that ball/tube/slab are *global* optima over all finite-perimeter sets — the shapes were conjectured earlier; the proof is what lets you say "≠ optimum" about a solver's output and what makes the formula a replacement rather than a guess.
- **Gain/size:** exact target + speed + reliability. Measured time-to-2%-of-optimal: ×96–157 (0.11–21 ms vs 18 ms–2.7 s, grids 32³–384³); the MBO solver failed ≥1 of 9 starts at *every* grid; the closed form never fails (±0.5% residual is estimator bias).
- **Bridge:** three periodic signed-distance functions + two thresholds (4π/81, 1/π) + complement rule; voxelize by top-k on squared periodic distance or evaluate per-pixel in a shader. Runtime = one grid pass. Small work — the demo kernel is a distance ranking.
- **Risks:** narrow setting (flat cubic torus, area minimization). If your game needs the *dynamics* of surface flow, this doesn't replace them. The durable value may be the oracle I(V) for unit-testing solvers more than the shape itself.
- **Visual:** left canvas runs real 2D threshold dynamics (blur + keep top-k, ~15 lines) on "melt" click, visibly stalling wrinkled with an "area excess +6%" readout; right instantly draws ball→stripe→slab with "+0.1%"; fill-fraction slider; shared perimeter estimator. ~120 lines.

### 3. Recipe-graph lint: permanence check for economy/crafting sims (family 149)
*Situation: survival, colony, factory, and MMO crafting trees where a designer wants "no resource can ever go permanently extinct or inflate to infinity."*

> In a game economy, recipes form loops: ore + wood → tools, tools → more ore. The math says that if every loop eventually feeds back into what it consumes — a property checkable from the recipe graph's shape alone, without simulating anything — the economy mathematically cannot kill a resource forever or explode to infinity. It may slump, but it always recovers. If the check fails, collapse is merely possible — but in our tests it happened essentially every time, and the blow-up made the simulation stiffer and slower to compute too. It's a spell-checker for your crafting tree: run it when authoring recipes, not per frame.

- **What the math adds:** a guarantee for *arbitrary* weakly reversible networks at any fixed positive rates — the earlier theory covered only easier cases (2 species, one linkage class, complex-balanced). Classical alternative: playtest-and-clamp with ad hoc min/max caps.
- **Gain/size:** design rule/correctness + incidental compute. Measured: removing two return reactions collapsed ≥99.4% of towns at every scale and cost ~×53–65 per frame while collapsing; under explicit RK2 — the integrator games actually use — ≥99.6% of broken towns went negative/non-finite at *every* step size down to h=1/512, while the good network had 0 failures after early transients at h≤0.125.
- **Bridge:** build the complex graph (nodes = recipe input/output multisets) and run an SCC check that every node can return to its product side — ~50–100 lines in your content validator or editor. Zero runtime cost; the theorem then covers your continuous sim.
- **Risks:** covers continuous mass-action only — integer inventories, stochastic extinction, and player-driven rates void it (demo's own caveat). One-sided: failure ≠ guaranteed collapse. Still nearly free insurance.
- **Visual:** small node editor with preset graphs (the demo's three-loop economy; the same minus two return arrows). "Lint" runs SCC and stamps PASS/FAIL; below, a ~30-line midpoint integrator animates five resource bars — broken graph flatlines/explodes, checked graph settles into a band. ~180 lines.

### 4. Resolution-invariant procedural clock (fractal frontier, family 223)
*Situation: LOD'd procedural animations whose per-step cost grows with grid resolution — creeping corruption, crack/erosion fronts, percolation-style spread — where a coarse preview should pace like the fine render.*

> A fractal edge — a crack, a creeping vine — gets wigglier on a finer grid. Pay one second of animation per step and the fine version drags on far longer than the coarse one. The math says charge step-length^1.75 per step instead of step-length. Then the animation takes the same ~3 seconds whether the grid is 64² or 1024² — and the cheap coarse grid matches the expensive fine one's pacing and even its "where the front lingers" statistics. One exponent makes every zoom level consistent.

- **What the math adds:** the proof that *one constant* times the pure mesh power converges — no slowly-varying correction — jointly with the occupation measure. The 7/4 exponent itself is classical; classical alternative is re-tuning speed per resolution by hand.
- **Gain/size:** speed + design rule. Measured: a 32² grid with the right clock matched the 8192² reference's statistics (~3–5% error) where the length clock needed the full 8192² — ~×16,000 less compute at equal accuracy; duration ratio 1024²/64² = 0.985 vs 7.88 for the naive clock.
- **Bridge:** pay dt ∝ δ^d per traversal in your existing front-sampler; for this model d=7/4 is proved. Zero runtime cost — savings come from running coarse. Work: a weight, and identifying the right exponent for *your* process.
- **Risks:** partially constructed win — a designer who re-fits speed per resolution matches it without the theorem. Applies as proved only to critical square-lattice FK interfaces; other effects need their own exponents (honest speculation for generalization).
- **Visual:** three canvases draw the same-seeded fronts at 32²/128²/512², self-drawing colored by clock time; toggle "time ∝ length" vs "∝ length^1.75" — durations fan out ~8× on the former, agree on the latter; readout = duration per grid. ~150 lines over a simple random interface.

### 5. Certified-uniform layout shuffle (dungeon shuffle, family 131)
*Situation: roguelike/run-based generation where door counts encode design intent (boss room = 1 door, hub = 6) and you want a defensible fairness claim — shared-seed competition, "all layouts equally likely."*

> Swapping random doors to shuffle a dungeon *looks* mixed after a few swaps but isn't: the result still remembers the template it started from — our lazy shuffle kept ~10 of 28 template doors where fair is ~5.6. The math gives a fixed recipe — run exactly T trades — after which the layout is certified uniformly random among all layouts with the same door counts, for any door-count pattern however irregular. It's the difference between stirring soup until it looks mixed and a packet saying "stir 40 times — lab-tested mixed."

- **What the math adds:** the budget. Curveball is a known shuffle; the paper supplies a universal explicit certified count T = ⌈B(1+B/2)·ln 2⌉ for *every* graphical degree sequence. Classical alternative: lazy edge swaps with heuristic burn-in — same stationary law, no guarantee.
- **Gain/size:** correctness/fairness. Measured: exact TV on enumerable toys 0.215–0.459 → ~1e-14; template-door autocorrelation +0.185 → −0.0007; 0.0027 ms → 0.344 ms per layout (~×128, still trivial at load time).
- **Bridge:** implement the Curveball pair-trade + the T formula (~150–250 lines), run at generation/load. Certified cost grows ~n^4.4: inside a frame budget to ~49 rooms, inside a load budget to ~200; beyond that, run uncertified trades and label them heuristic.
- **Risks:** honesty requires noting the cheap chain also mixes fast empirically (TV 1.4e-4 at 10 swaps/door on the toy); the certificate earns its keep for unaudited/user-made templates or an explicit fairness claim. Rejecting bad layouts *inside* the chain voids the guarantee — filter output, not steps; expect ~0.4% disconnected layouts.
- **Visual:** 8-room dungeon, fixed door pips; buttons "lazy 8 swaps" vs "certified T"; accumulating histogram of template-doors-kept against the uniform reference line; TV badge. Real Curveball on n=8, ~150 lines.

*Undemoed SOLID leads worth an afternoon, not top-5:* **G3.4** — a sharp ceiling on an NPC's expected best noisy action score under a fixed score-variance budget; adding actions doesn't buy what spread does. **G7.1** — the exact boundary where distant noisy clues still carry faction information. **G9.1** — median-of-three nonlinear smoothing for graph attributes with a dimension-independent move/roughness certificate. All unmeasured.

## 4. Generalization ideas

- **Certificate compilation beyond Snaky** *(speculation)*: any game where an offline solver emits a verifiable strategy object — other Maker–Breaker targets, solved endgames, fixed puzzle rules — gets the same O(1)-playback, perfect-under-pressure win. The bottleneck is producing the certificate, a per-game research task; the engineering pattern is proven.
- **A general "math lint" pass over authored content** *(partly speculative)*: hypothesis checks as validator rules — recipe-graph SCCs (proven here), rumor-tree clue depth vs dλ²>1 (G7.1, needs trees — real social graphs have loops, so treat as heuristic), puzzle-size difficulty bands scaling as √n (G7.6), utility-noise budgets (G3.4), start-existence guarantees (G5.5). Each is small code; benefit depends on whether your content violates the assumptions.
- **Exponent-calibrated LOD beyond this front** *(speculation)*: the "one power of step-length" trick extends wherever a self-similar process has a known dimension — DLA, invasion percolation, SLE curves have classical exponents; the Voronoi connectivity slider (G6.3) is the same pattern via mean-pivotal normalization. Proven here only for q=1 FK interfaces; elsewhere you must source or fit the exponent.

## 5. Don't bother

- **02-fluid-gates** as a game method: 6–45× slower than the classical volume-preserving tween, which already keeps drift at ~1e-13; provably-zero-overlap routing is a guarantee almost no game needs.
- **09-cubic-interference:** a certified bias over ~11k primes — beautiful, but no knob, no speedup; pure visualization.
- **05-crowd-hub** *as a tool*: it's a warning, not an allocator — optimal labels are fragile; the actionable takeaway is "freeze or smooth assignments," which devs already know.
- **08-convex-morph:** marginal — the 1.2–4× speed edge is incidental and vertex-lerp is standard; worth it only if you already morph via support functions and care about the 20%-below-floor collapse of naive lerps.
- **Galactic/undefined constants:** planar-graph L1 embedding (success probability 2^−3.2×10⁹), cylinder covers needing 16,000,000 cylinders for a ~5×10⁻¹⁰ saving, perfect-matching FPRAS replication L=(10⁴ND)^100, the IAMP faction optimizer's unknown C(ε) (G8.6), and ultraflat Littlewood polynomials with no explicit sequence (G2.4) — nothing shippable to implement.
- **G6.6 tree-limit worlds:** typed rejection sampling has no usable acceptance guarantee past n≈128 — cute theorem, unshippable generator.
