# Practical game uses of the results

**Snaky is the clearest first build.** Recipe validation and fractal-effect timing have broader uses, but narrower guarantees than their headline speedups suggest.

I prioritized the current ladder JSON and clean results over older README timings. The measurements cover CPU computation, excluding rendering and other engine work. “Fits 60 fps” generally means the **median** fits: the stable economy at 65,536 towns measures 16.54 ms median but 106 ms at the 95th percentile. [Evidence](<repo>/demos/L4-economy-ladder/out/ladder.json)

## 1. What is actually happening

| Demo | What the math actually buys |
|---|---|
| **Economy — design rule / guarantee** | Every recipe has a route back through the recipe network. Under the specified continuous production rules, positive resources eventually remain above zero and below finite limits; the theorem does not calculate useful inventory limits or guarantee numerical stability. |
| **Fluid gates — correctness** | Separate regions take different temporary height lanes, allowing them to exchange places and change shape without squeezing or intersecting. This prescribed motion costs 6–45× more than the simple animation baseline; it is not faster fluid simulation. |
| **Snaky — speed / correctness** | Someone has already solved the opponent’s decisions and supplied a checkable strategy. Runtime code follows that strategy instead of searching, with a guaranteed win within 21 Maker moves under the exact rules. |
| **Fractal frontier — design rule / conditional speed** | Finer curves contain more wiggles, so their animation steps need shorter durations. The clock preserves particular timing statistics across resolutions, allowing cheaper coarse curves when those statistics are sufficient. |
| **Crowd hub — warning** | Small changes to destinations can cause substantial changes in the cheapest assignment of people to them. Recomputing assignments costs more; the theorem does not prevent destination switching or improve pathfinding. |
| **Dungeon shuffle — content variety / correctness** | Room connections change while every room keeps its door count. The theorem supplies a conservative shuffle budget giving **bounded distance from uniform**, not exact uniformity, connectivity, or good level design. |
| **Periodic blob — exact target / speed** | In a wrap-around cube, the minimum-surface object is a ball, wrapping tube, slab, or complement. Building that answer directly beats searching for it, but does not reproduce the physical motion toward equilibrium. |
| **Convex morph — guarantee** | A particular morph keeps a symmetric convex object above a volume floor while swelling less than the classical comparison. Volume alone says nothing about doorway width or player clearance. |
| **Cubic interference — warning / statistical prediction** | Apparently irregular arithmetic waves accumulate a systematic bias. The paper predicts that bias asymptotically; it supplies neither faster evaluation nor better general-purpose randomness. |

The [load-ladder summary](<repo>/demos/sheet-ladder/out/load-ladders.html) supports these distinctions. The largest ratios describe specific substitutions, not general engine acceleration.

## 2. Bridge patterns

- **Compile a proof into an asset.** Snaky’s certificate becomes small runtime tables. Spend effort validating the asset once; make cheap decisions during play.
- **Replace equilibrium search with a known answer.** The periodic blob provides both a direct generator and a reference target for testing solvers.
- **Turn assumptions into editor checks.** Economy validation checks recipe return paths; the undemoed two-hop result checks whether a starting location offers sufficient fresh destinations.
- **Preserve a statistic while reducing detail.** The fractal clock makes coarse simulation useful for selected timing measurements. Visual fidelity needs its own acceptance test.
- **Pay explicitly for a property.** Dungeon mixing buys a distribution guarantee; convex morphing buys a volume floor; fluid gates buy separation and volume preservation. Crowd and cubic examples warn against assumptions that cheap substitutes violate.

## 3. Top five practical methods

Ranked by useful game behavior and implementation effort. Effort estimates assume reuse of the supplied code and cover a first integrated version. Each browser visual below can target roughly 200 lines of JavaScript, using prepared data where specified.

### 1. A certified puzzle opponent

**Situation:** A 17×17 tavern minigame in which the player blocks an opponent and scores points for delaying its completed Snaky shape.

**Plain explanation:** The opponent carries a book of answers. After your move, it finds the next applicable page and plays the move written there. The book covers every legal reply, so the opponent never needs to think harder when the game becomes difficult. You could make surviving until its twenty-first move the challenge, turning an unbeatable opponent into a score-attack puzzle.

**Contribution and gain:** The supplied winning certificate is the valuable new object. Classical search or an independently built tablebase is the alternative. The ladder measured roughly **55,000–90,000× lower decision cost**; at 64 concurrent games under a shared frame budget, search won 43% against the tested search defender while the certificate won 100%. This is specific to this opponent and benchmark. [Results](<repo>/demos/L1-snaky-ladder/out/ladder.json)

**Engineering bridge:** Package the reconstructed cards as indexed binary tables containing moves, child references, transformations, and reply masks. Keep the current card and placement in each game state; advance them after the defender moves. Runtime work is bounded for this fixed certificate. Reuse the reconstruction and invariant checks. Estimated effort: **2–4 days**, with approximately 94 KB of existing runtime tables.

**Risk:** The guarantee applies to the empty-board Maker–Breaker game. Obstacles, altered targets, or different turn rules require new analysis. An unbeatable opponent also needs an enjoyable scoring premise.

**Browser visual:** Draw two boards with a shared move scrubber. Replay recorded search and certificate games against the same defender policy; show searched nodes versus certificate transitions. Add a right-board mode where the user clicks legal blocking moves and the compiled certificate responds.

### 2. A recipe validator for continuous simulations

**Situation:** A chemical garden or colony simulation with hundreds of settlements, fractional resource quantities, and fixed production rules.

**Plain explanation:** Treat recipes as roads between bundles of ingredients. Before accepting a recipe set, check that every road has a route back. For the paper’s production model, that structure prevents resources from eventually disappearing or growing forever. It gives a designer a useful “this system has a survival guarantee” badge before running thousands of simulation trials.

**Contribution and gain:** Graph reachability is classical; the paper connects it to eventual survival and boundedness for a broader class of coupled reaction systems. Classical alternatives include deliberately restricted balanced networks and empirical testing. The measured **53–65× median cost gap** comes from avoiding this example’s expensive collapse. It is not an expected speedup for an already stable economy. [Results](<repo>/demos/L4-economy-ladder/out/ladder.json)

**Engineering bridge:** Store each ingredient bundle as an integer vector and each recipe as a directed edge with a positive rate. Run a strongly connected component check during authoring or loading, on **bundles, not individual resource names**. The check is linear in graph size and adds no per-frame work. Keep the numerical integrator separately validated. Estimated effort: **1–3 days** for the validator and diagnostics.

**Risk:** Failure means “this guarantee is unavailable,” not “the economy is broken.” Passing does not guarantee enjoyable balance, tolerable transient quantities, integer-inventory safety, or safety under player-controlled rate changes.

**Browser visual:** Show the complete and broken recipe graphs beside resource bars. Toggle the two removed return recipes and choose among the six existing starting states. Replay their recorded trajectories; display the structural verdict and observed minimum/maximum separately.

### 3. Resolution-independent timing for a creeping visual effect

**Situation:** A strategy-game corruption border or magical crack drawn using the demonstrated random frontier model, with different detail levels for near and distant effects.

**Plain explanation:** A detailed crack takes more tiny turns than a coarse one. Giving every segment the same travel speed makes the detailed version finish later. Adjust the time assigned to each segment according to the grid’s detail, and the overall pacing stays statistically consistent. Distant effects can then use cheaper curves without changing their typical duration.

**Contribution and gain:** The scaling exponent was already known. The paper establishes the square-lattice clock’s limiting behavior with one common constant. Classical resolution-based retiming can obtain the same practical benefit.

The ladder’s 32² grid had approximately **3.2% median-duration error and 4.7% occupation error** against its 8192² reference, costing about **6.6 µs versus 107 ms**. The roughly 16,000× comparison is against a length clock calibrated only at the reference resolution; it does not measure equal geometric detail. [Results](<repo>/demos/L3-fractal-ladder/out/ladder.json)

**Engineering bridge:** Generate a seeded polyline with the existing tracer, store cumulative traversal times, and reveal it through the engine’s animation clock. Apply the mesh-size scaling once per resolution. Generation cost follows traversal count; playback needs a cursor through the timed vertices. Estimated effort: **1–3 days**.

**Risk:** This is not a general fire, erosion, or fracture law. The theorem supplies no finite-grid error bound, and a coarse curve can still look unacceptable.

**Browser visual:** Load sampled curves at 32², 64², and 128². Draw identical curves on both sides at each resolution, using different clocks. Let the user change resolution and toggle a classical retuned clock. Show duration histograms and accumulated time in an 8×8 screen grid.

### 4. Choose a start with fresh nearby opportunities

**Situation:** A generated exploration map of roughly 30–100 locations connected by one-way portals, with no reciprocal connections and at least one exit from every location.

**Plain explanation:** A starting room can look generous because it immediately offers many destinations, yet reveal little new after that. Count its immediate destinations, then count the genuinely new destinations available after two moves. Choose a start where the second group contains at least as much reward as the first. Count each destination once, even if several routes reach it.

**Contribution and gain:** The undemoed **G5.5** result guarantees that some qualifying vertex exists in every oriented graph, including a version with nonnegative destination rewards. Ordinary graph scans find it; many sparse cases already had older guarantees. **Gain: a design rule; performance and player benefit are unmeasured.** [Verification](<repo>/verify-games/v-B.md)

**Engineering bridge:** Store adjacency lists and reward weights. At level generation, build deduplicated one-hop and exact-two-hop sets for each candidate and select a passing start. Cache the selected location in the level asset. A simple cubic scan is adequate to prototype at this scale; bitsets can reduce the work. Estimated effort: **half a day to two days**.

**Risk:** A sink passes vacuously, hence the proposed no-sink map restriction. This does not guarantee multiplayer fairness, good long-term routes, or an acceptable start after imposing additional designer constraints.

**Browser visual:** Draw the same editable directed map twice. Left chooses maximum immediate reward; right chooses a qualifying start. Drag nodes, adjust reward weights, and edit valid arrows. Highlight immediate destinations orange and new two-hop destinations blue; show both reward totals.

### 5. Compact morphs for convex crystal assets

**Situation:** A puzzle game with a handful of symmetric crystals changing shape, or an editor that bakes such transformations.

**Plain explanation:** Describe a crystal using planes that fence it in, then move those fences to create the intermediate shapes. The paper’s rule produces a tighter transition than ordinary support-based blending while keeping a minimum amount of volume. This is useful when a crystal should change shape without becoming either a thin sliver or an oversized lump.

**Contribution and gain:** Classical Minkowski blending already guarantees a volume floor. The new guarantee supports the tighter logarithmic construction for generic centrally symmetric 3D shapes. The original demo’s maximum volume-to-floor ratio fell from **1.647 to 1.382**. Ladder speedups of **1.2–4.4×** were incidental to the clipper processing fewer effective cuts. [Demo results](<repo>/demos/08-convex-morph/out/clean/results.json)

**Engineering bridge:** Validate convexity and central symmetry; store endpoint plane directions and support distances. Construct intermediate meshes by half-space intersection. Prefer editor baking, or rebuild occasional active objects when their morph parameter changes. Cached meshes cost ordinary rendering work; arbitrary live rebuilding retains the clipping cost. Estimated effort: **3–7 days** using the existing geometry code.

**Risk:** Volume does not guarantee usable passage widths. Nonconvex rooms, asymmetric creatures, collision response, and arbitrary interpolation between baked meshes fall outside the guarantee.

**Browser visual:** Bake paired 3D meshes and volumes at a shared set of morph positions. Draw rotating wireframes side by side. A slider selects those positions; dragging rotates the view. Plot volume against its floor and optionally display the recorded naive vertex-morph curve.

## 4. Generalization ideas

- **Other certified puzzle modules — speculation:** Search offline for winning strategies, validate every opponent reply, and ship the resulting tables. Snaky demonstrates the delivery pattern; it does not establish affordable certificates for other games.
- **Procedural recipe authoring — within scope under unchanged assumptions:** Generate coupled recipe cycles and run the validator automatically. Measure practical quantity ranges separately. Extending the guarantee to arbitrary player interventions remains speculative.
- **Other effects across detail levels — speculation:** Measure which statistics matter for branching lightning, tendrils, or cracks, then calibrate their timing across resolutions. Do not automatically reuse the frontier’s exponent or claim preservation of full geometry.

## 5. Don’t bother list

- **Periodic blob as a general liquid replacement:** Its excellent **96–157×** result solves periodic equilibrium geometry; obstacles, forces, and transition motion are different problems. It deserves promotion if that exact equilibrium is your game mechanic. A coarse-to-fine baseline already narrows the gap to roughly 12× at 384³. [Results](<repo>/demos/L2-blob-ladder/out/ladder.json)
- **Certified dungeon shuffling during large live updates:** Current measurements are about 7 ms at 40 rooms, 159 ms at 80, and 64 seconds at 320. Small offline generation remains viable; uniform graph sampling does not ensure connected or enjoyable levels.
- **Fluid gates for ordinary particle performance:** Use them only when their specific separated, volume-preserving routing behavior matters.
- **Crowd stability as a faster navigation algorithm:** It is a useful sensitivity warning, not a new fast allocator or anti-flicker mechanism.
- **Arithmetic noise and prime-factor debris as default content tools:** Seeded randomness and classical stick-breaking already provide practical alternatives; the arithmetic theorem matters only if its specific distribution is the target.
- **Median-of-three diffusion as an automatic biome upgrade:** Its dimension-independent bound is interesting, but the factor 3024 is loose and no visual, speed, or conservation advantage has been demonstrated.
- **Gaussian NPC-score bounds as gameplay balance:** A ceiling on an expected noisy score does not establish action fairness or policy quality.
- **Ultraflat binary probes:** No practical theorem-derived sign generator or usable parameter cutoff has been established.
- **Quantum, singularity, and infinite-world results without a matching game premise:** Their mathematical novelty supplies little ordinary engine value; pursue them as the subject of a game, not as presumed optimizations.