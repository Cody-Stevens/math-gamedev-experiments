The clean ladder numbers and the undemoed solid leads are in. The report below ranks what is actually worth building.

# From the measured demos to games you could ship

The load-ladder numbers below are the clean sequential run on a Ryzen 9 9900X (the sheet in `demos/sheet-ladder/out/load-ladders.html` and each `out/ladder.json`). Where a demo README was timed on a busy machine, the sheet wins.

## What is actually happening

**Economy.** A crafting network whose every recipe can be undone, possibly through other recipes, keeps every resource alive and bounded for any positive speeds. The measured broken network (two return recipes removed) drives about 99% of towns to extinction or runaway. Gain: a design rule, plus a side-effect speedup only while the broken towns are collapsing.

**Fluid gates.** Three dyed regions can be swapped and reshaped by a volume-preserving motion that never lets them overlap. An area-preserving tween still overlaps by 89% of a region. Gain: a guarantee. It costs more, not less.

**Snaky.** A proof that the builder always completes a snake within 21 of their own moves was compiled into a lookup table. Each move is one table step. Gain: speed and a guaranteed win. Search gets neither once many games share one frame.

**Fractal frontier.** A critical percolation front has a natural clock: each lattice step is charged by its length to the power 7/4. That clock’s duration and where time is spent already match a huge grid when you trace a tiny one. A length clock does not. Gain: permission to simulate coarse. Tracing itself is the same cost at a given grid size.

**Crowd hub.** If each person is sent to the nearest of three destinations in the optimal way, nudging the small central hub reassigns a large share of the crowd. Keeping the old labels misroutes 11.25% when the hub’s mass is 0.45, at every crowd size. Gain: a warning. Recomputing the labels is the ordinary formula, and it is slower.

**Dungeon shuffle.** Rewiring rooms while keeping each room’s door count can be made close to a fair draw, but only after a burn-in that grows like rooms to the 4.4. A few swaps per door stay biased toward the template at every size tested. Gain: a certificate you can afford for small levels, and a warning about cheap shuffles.

**Periodic blob.** In a cube whose opposite faces join, the least-surface blob of a given volume is a ball, a tube, or a slab, with known switch points. You write the shape down. An iterative blur-and-threshold solver is slower and gets stuck about one run in nine. Gain: an exact shape, a speedup, and no stuck runs.

**Convex morph.** Blending two origin-symmetric convex bodies by multiplying their support values stays above a known volume floor and bloats less than the usual average of those values (about 1.37× the floor against about 1.62–1.69×). Gain: a tighter morph with a floor. The usual average already has a floor. The 1.2–4.4× speedup is because the tighter body is clipped by fewer planes.

**Cubic interference.** Adding up certain cubic waves looks irregular step to step, but the running total drifts upward in a way a random-arrow model does not (at the demo cutoff the true total sat about 18 standard deviations above that model, and about 7% above the proved curve). Gain: a certified drift. There is no faster way to compute it.

## Bridge patterns

- **Compile a proof into a table.** A winning strategy that has been fully written down becomes a lookup: one step per opponent reply, no search. Snaky is the case we measured. Nothing else in the 58 leads arrived with a playable certificate.
- **Replace the solver with the shape.** When the optimum is a short list of formulas, you evaluate them and skip the iteration. The periodic blob is this. The cubic-sum curve is the same idea for a prediction, and it does not save any compute.
- **Turn a graph condition into an editor lint.** Weak reversibility is “every recipe cluster can get back to itself.” You test that when the designer saves, not while the frame runs. The same habit covers the faction-clue threshold and the random-logic window below, which were verified and never demoed.
- **Simulate the coarse object if a theorem says the statistic has already settled.** The fractal clock is the measured case: match the fine grid’s timing by weighting steps, instead of refining the mesh. A connectivity slider for random Voronoi maps (lead G6.3) is the same hope, and it was not measured.
- **Treat a sensitivity theorem as a cache warning.** Optimal crowd labels move like the cube root of how far the destination mix moved, so a small hub edit invalidates many cached assignments. Frozen labels were the measured failure.
- **Spend a mixing bound at level-create time, or not at all.** The dungeon burn-in is a worst-case count, not a per-frame algorithm. Past a few dozen rooms it leaves real time.

## Top 5 practical methods

### 1. Lint crafting graphs for “every recipe can be undone”

**Where.** A town, factory, or ecosystem sim whose resources are continuous amounts and whose recipes run at fixed positive speeds. Tens to tens of thousands of independent towns.

Picture the recipes as roads between bundles of goods. If every bundle that appears as an output can eventually be turned back into the inputs that made it, the town cannot die out or blow up, whatever positive speeds you pick and however you stock the warehouses at the start. Delete a return road and that promise disappears. In the five-resource village we measured, the broken towns went to dust or to absurd stockpiles. The test is the one you already know from one-way streets: split the recipe map into clusters and demand that each cluster is a loop. You run it when the designer hits save.

**What the math adds.** Older guarantees cover two resources, a single recipe cluster, or specially balanced rates. This one covers an arbitrary reversible recipe map at any positive speeds, including the demo’s three coupled loops, which sit outside those older cases. The classical alternative is to simulate a long time and watch for a crash. That can miss a slow failure, and a network that fails the test does not always crash: removing some other return recipes left a stable system.

**Gain.** Guarantee. On this network, this integrator, these seeds: the reversible side had 0 dead or runaway towns at every load; the broken side was about 99% dead or runaway by simulated time 120. Median frame cost was ×53–65 higher on the broken side during the collapse (1,024 towns: 12.4 ms vs 0.22 ms). The broken side still fits 60 fps at 1,024 towns and is over at 4,096 (46 ms). The reversible side’s median still fits at 65,536 towns (16.5 ms); the 95th percentile there is 106 ms, so a locked frame would hitch. After the collapse is halted, the broken side becomes cheaper (0.57×, then nearly free). A fixed-step integrator, the kind a game ships, breaks at least 99.6% of the broken towns at every step size tried, and 0% of the reversible towns at the smallest step.

**Engineering.** A few dozen lines: build the directed graph of recipe complexes and test that each strongly connected piece has every reaction reversible inside it. Store recipes as integer bundles plus a positive rate. At runtime use whatever integrator you already trust; the lint does not replace it. The theorem does not hand you the numeric floor, only that some floor exists. Work size: an afternoon, plus a unit test on the demo network.

**Risks.** Integer stacks, random crafting, and rates the player edits are outside the proof. A green lint on a continuous model can still go extinct as a stochastic inventory. The ×50 is not a speedup you can bank on; it is the cost of simulating a blow-up.

**Visual.** A canvas with five named circles and arrows for the nine recipes. A toggle cuts the two return arrows. Six ghost traces show quantity on a log axis. Drag a rate slider. The number under the map is the smallest quantity at the end of a short run, and a stamp reads “loops intact” or “dead end.” The broken side’s circles pop or vanish; the intact side’s stay in a band.

### 2. Draw the ball, the tube, or the slab

**Where.** Any effect that is “a blob of volume V inside a room whose opposite walls connect”: a periodic lava cell, a force-field core, a fog volume in a tiled world. Grids from 32³ up to 256³.

The room is a cube with the opposite faces glued. You choose how full it is. The puddle with the least skin is a ball when it is small, a tube through the cube when it is middling, and a flat slab when it is large, then the same shapes inside-out past half full. The two changeovers are fixed fractions of the cube, so a slider is enough. You voxelize by keeping the cells closest to that shape. The usual method blurs a random field and keeps the fullest cells, over and over. It often stops on a worse shape, and it has no way to know it is finished unless someone already told it the best possible area.

**What the math adds.** Ball, tube, and slab were guessed before this paper. The paper shows they are the best among every possible shape, not just among nice ones, and that nothing else ties them. That is what lets you treat the formula as the answer rather than as a hint for a solver. The classical alternative is the blur-and-threshold iteration, or a hand-authored morph with no area claim.

**Gain.** Exact target, speed, and reliability. Time to get within 2% of the best area is ×96–157 lower for the formula at every grid in the clean run. The formula fits a 60 fps frame up to 256³. The solver’s median never fits, even at 32³. About one solver run in nine never reaches 2%. A smarter solver that starts coarse and refines is still about ×12 slower at 384³. Both sides then grow almost linearly with voxel count, so the win stays a constant factor.

**Engineering.** One function: given volume fraction, pick ball, tube, or slab (or the complement above one half), rank voxels by squared distance in the wrapping cube, keep the count that matches the volume. Ship the mesh or the SDF. No iteration, no seed. If you later write a general solver for rooms that are not an empty wrapping cube, the formula is only a test target for the empty case. Work size: a day, including the wrap-around distance.

**Risks.** Obstacles, open boundaries, and “looks like liquid” motion are a different problem. The theorem says nothing about how a solver should move, only which shape wins. Grid area estimates sit about 0.2–0.5% off the true area even for the exact shape.

**Visual.** One slider, 0 to 1, with two marks where the shape changes. Left: a blob grown from noise for a fixed iteration budget, sometimes the wrong type. Right: the formula. A line chart of area against the known best; the right-hand dots sit on the line. The number is area excess and the shape name.

### 3. Animate a critical front on a tiny grid

**Where.** A fire line, corruption edge, or infection border that should take about the same time, and linger in the same places, whether the player’s machine uses a coarse map or a fine one. Critical percolation on a square grid.

The front is the boundary of a random cluster at the density where it is just barely connected. Walk along it. If you bill each step by its length, a finer grid has more steps and the animation runs longer, so a clock tuned on a poster-sized grid is wrong on a thumbnail. Bill each step by length to the power 7/4, with one constant for every resolution, and the duration and the time spent in each part of the screen settle immediately. A 32×32 front already matches an 8192×8192 reference. You play the thumbnail.

**What the math adds.** The 7/4 power itself is older. What this paper adds is that, on the square lattice, one constant times that pure power is the whole story: no extra slowly drifting factor, and the occupation of the screen converges together with the curve. A length clock retuned by hand at every resolution matches too, but each new resolution needs its own reference run or the same 7/4 guess. Using the measure as an animation clock is our application.

**Gain.** Less compute for the same timing accuracy, measured: 6.5 µs per curve at 32×32 versus 107 ms per curve at 8192×8192, ×16,150. The 32×32 clock is within 5% on median duration and 10% on where time is spent. The length clock meets that target only at the reference grid. Both clocks cost the same at a given grid; the saving is that you are allowed to stay at 32. For a tighter 2% target the weighted clock needed 64×64. No size met 1% once sampling noise was included.

**Engineering.** Sample fair open/closed edges, trace the interface (the demo’s sampler is a few hundred lines and does no heavy iteration), multiply each step’s duration by `length^1.75`. Store one global speed the artists set on a single reference. Do not renormalize each curve to the same length; that erases the result. Work size: a weekend if the tracer is new, an hour if you already trace the front.

**Risks.** This is one lattice at one density. Other maps, other densities, and q = 4 are not covered. There is no proved rate, so the 32×32 agreement is a measurement, not a promise for the next seed budget. If you do not need the front’s timing to match across resolutions, a hand-tuned clock is enough.

**Visual.** Two polylines on a unit square, same seed, colored by time. Buttons for grid 16, 32, 64, 256. The number is median duration over a few hundred curves. The left number climbs as the grid grows; the right number stays near one fixed value. A small 8×8 heat map shows where time was spent, with one shared color scale.

### 4. Ship the Snaky builder as a table, not as a search

**Where.** A Maker–Breaker puzzle on the square grid: the builder claims cells, the blocker claims cells, and the builder is trying to occupy a specific six-cell snake in any rotation or reflection. One board, or thousands of boards resolved inside one frame. The strong game, where both players race to build it, is not this result.

The builder’s strategy was proved in full, then stored as 728 cards, about 39 KB of text. On each turn the builder looks up the card, sees the blocker’s last cell, and plays the first reply the card still allows. That is the whole AI. A normal search scores threats and reads thousands of future positions. Give that search a single 60 fps frame to share among every live game and it weakens as you add games. The table’s move does not look at the clock, so its win rate does not fall.

**What the math adds.** The certificate is a strategy for the empty board that search does not hand you: a win within 21 builder moves against every legal blocker, on the infinite grid or on 17×17. Twenty-one is a cap, not the shortest possible game. The classical alternative is minimax. It can win when you give it enough nodes, and in the original demo its win rate did not even rise steadily as the node cap rose.

**Gain.** Speed and a guarantee, both measured. Search fits 60 fps up to 4 concurrent games and is over at 8. The table fits up to 131,072 games and is over at 262,144. Where both timings sit above the timer floor, the table is ×55k–90k cheaper per decision. Under a shared 16.7 ms budget the search builder’s win rate against a search blocker goes from 100% at one or two games, to 43% at 64, to 0% at 1,024. The table wins 100% at every load (60 games per stage). In the untimed batch it also won 300/300, including 100/100 against the search blocker, where search-as-builder won 41/100.

**Engineering.** The table already exists. Parse the cards once at load into reply bitsets (the demo’s runtime tables are about 94 KB). Each builder turn is one lookup. The blocker can be the player, a random neighbor, or a search opponent; the proof does not care. Replacing an already-owned pivot is specified and, in the demo, never came up. Work size to drop this opponent into a board UI: a day. Work size to produce a certificate for a different animal or a different ruleset: a research project. This scan contains no second certificate.

**Risks.** If the game you want is not this snake, the speedup does not transfer. Memory is not the win; the tables are larger than the search’s copy tables. Sample wins are not a proof; the proof is the paper.

**Visual.** A 17×17 canvas. You click the blocker’s cells; the builder answers on the same frame and paints its live “still winning” region in teal. A counter shows builder moves and stops glowing gold by move 21. A “add games” control runs N tiny boards. The number is milliseconds spent on the builder this frame, and wins out of N. The search column’s millisecond counter climbs through the 16.7 ms line while its win tally falls; the table column stays flat.

### 5. Pay for a fair dungeon only while it is small

**Where.** A level generator that starts from a template of labeled rooms and must keep each room’s door count. The player sees one new wiring per level, so the cost is paid at level creation, not every frame. Sweet spot: about 20–40 rooms.

You want a new corridor layout that is a fair random draw from every layout with the same door counts, not a slight scramble of the template. Swapping a couple of doors a few times looks busy and still keeps too many of the original doors. There is a counted number of pair-trades that forces the layout close to fair no matter the template and no matter which wiring you start from. That count grows like the fourth power of the room count. At 20 rooms it is cheap. Around 50 rooms it no longer fits in one frame. At a few hundred rooms it is most of a minute.

**What the math adds.** The shuffle itself (Curveball) is older, and so is the ordinary double-edge swap. Both become fair if you run them long enough. The new piece is an explicit worst-case length that works for every door-count list, including the irregular ones in a real dungeon, not only regular grids that older theory already covered. The headline paper bound is even more conservative; the length we timed is the tighter corollary used in the demo.

**Gain.** A certificate, paid for in time. Measured on the sweep: the certified shuffle fits a 16.7 ms frame up to 40 rooms (6.3 ms) and is over at 80 (200 ms); the crossing interpolates near 49 rooms. One swap per door stays under 0.01 ms even at 320 rooms and stays biased toward the template at every size (kept-door autocorrelation about +0.14 to +0.26). On the two tiny graphs where exact distance to uniform can be computed, one-swap-per-door sat at 0.22 and 0.46, and the certified run sat at about 10⁻¹⁴. The certified cost grows about like rooms^4.4.

**Engineering.** Implement the pair trade: pick two rooms, reshuffle the private neighbors with a partial fair shuffle, leave shared neighbors alone. Run the certified count, then drop disconnected or geometrically impossible maps after the chain finishes. Filtering inside the chain voids the proof; filtering finished samples stays fair on whatever you keep. For a 20-room level, 0.3 ms is a load hitch. For 80 rooms, 200 ms is a loading bar, not a frame. Work size: two days, plus a test that the kept-door histogram matches a long reference run.

**Risks.** The bound is a length that is certainly enough, not the shortest length that looks fair. A longer ad hoc run may already be close; the theorem will not tell you when. Samples include disconnected maps (about 0.4% on the 20-room template). Players do not feel “total variation 1/4.” They feel template doors that never left. If your levels have more than about 50 rooms and must be instant, this certificate is the wrong tool.

**Visual.** Twenty rooms, pips for door counts. A slider from “one trade per door” to “certified count,” snapping to those two and to a midpoint. Each release rewires and adds a tick to a histogram of “template doors kept,” with a vertical line at the fair average. The number is that histogram’s distance from flat. The short slider piles ticks on the right; the certified slider sits on the line.

## Generalization ideas

**Other recipe maps.** The lint is not special to ore, wood, and food. Any continuous recipe graph you can draw can be tested the same way. Speculation: integer inventories and player-set rates fall outside the theorem, so a green check there is a warning light, not a proof. A discrete “can every good still be produced?” reachability test is a different, classical tool and is still worth having beside it.

**Other compiled opponents.** Speculation, except for Snaky: any Maker–Breaker game whose whole strategy has been written down as cards can be played the same way, one lookup per reply. This set of papers does not contain a second such table. Heuristic search, opening books, and endgame tablebases are the classical versions of the same idea, and they do not carry a 21-move proof.

**Difficulty dials that scale on purpose.** Two verified leads were never demoed, so the gameplay value is speculation and the theorems are not. For a rumor network with three hidden factions, reconstruction of the faction from noisy local clues is possible only when (branching) times (clue strength) squared exceeds 1, and only when each person has more than four contacts does any strength succeed (G7.1). Belief propagation is the classical estimator; the new part is that at and below the line, no estimator works. For random 3-clause logic puzzles, the band of clause counts where the puzzle flips from solvable to impossible has width on the order of the square root of the number of variables, not a fixed density gap (G7.6). The constant is not supplied, so you fit it once on a reference size and then grow the band like a square root. Neither result was timed here.

## Don’t bother

- **Cubic interference, as a system.** The drift is real and certified, and it does not generate levels, speed anything up, or change a rule a player can use. A random walk already looks irregular.
- **Convex morph, as a feature.** The usual average of the two shapes already stays above a volume floor. The tighter blend matters for origin-symmetric convex crystals and for nothing else we measured. The speedup is a side effect of clipping a smaller body.
- **Fluid shears, as a performance win.** They are ×6–45 slower and leave 60 fps after 64,000 particles per region, while a tween is still fine past a million. Build them only if a puzzle rule forbids overlapping regions. For a few thousand particles the absolute cost is still a frame; the overlap guarantee is the product.
- **Crowd relabeling, as a new allocator.** The three-way labels are the standard nearest-cell rule. Recomputing them is ×2.8–4.2 slower and still fits 8.4 million agents on one thread. What you should take is the warning: a small hub edit misroutes 11.25% if you keep the old labels, and that fraction does not shrink when the crowd grows.
- **The dungeon certificate past ~50 rooms at runtime.** At 320 rooms one certified layout took about 83 seconds. A short shuffle will look busy and stay template-biased.
- **Prime-factor shard sizes (G1.1).** The cheap fragment generator is classical stick-breaking. The paper only says that prime predecessors follow that law. Generic loot shards do not need primes.
- **Flat binary probes (G2.4), the polynomial straightening toy (G2.1), the kick-to-wake oscillator (G4.2), median-of-three graph smoothing (G9.1), and the quantum cluster.** Either no usable construction is supplied, a classical oscillator or blur already does the visible thing, or the certificate is too loose to change what you ship (the smoothing bound carries a factor of 3024 and does not promise a better-looking result).
- **Anything sold as a galactic algorithm.** The sheet’s note matches the ladders: no method in this set has a crossover that appears only at unreachable sizes. The wins that exist are either a large constant, a closed form, or a guarantee that costs as much or more.
