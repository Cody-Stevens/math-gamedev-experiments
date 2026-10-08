# Panel report: turning the pure-math results into game techniques

## 1. What is actually happening

| Demo | What the math really buys | Kind of gain |
|---|---|---|
| 01 Economy | A check you can run on the recipe chart. If every recipe can be undone by some chain of other recipes, no resource can die out or explode, whatever recipe speeds you choose. In our test, removing two "return" recipes made every run collapse. | Design rule (simulation is cheaper only while the bad version collapses) |
| 02 Fluid gates | A way to move regions of fluid past each other so they never overlap and never compress. It costs 6–45× more than a plain tween. | Guarantee, slower |
| 03 Snaky | A published, proven playbook for one shape-building board game, compiled into 94 KB of tables. The AI looks up each move in about 1 µs and always wins within 21 moves. | Speed + guarantee |
| 04 Fractal frontier | How to scale each step's time as the grid gets finer, so a fractal front takes the same time to cross the map at any resolution. | Design rule |
| 05 Crowd hub | When a small destination moves slightly, the optimal plan can send some people to a far-away exit, and staying optimal can't avoid it. | Warning |
| 06 Dungeon shuffle | A step count proven to make rewired level graphs fair (every room keeps its door count) instead of echoes of the template. The cost grows steeply with room count. | Content variety (fairness guarantee) |
| 07 Periodic blob | The exact least-surface shape at any fill level of a wrap-around cube: a ball, then a tube, then a slab. You build it in one pass instead of running a solver. | Exact target (faster than a solver) |
| 08 Convex morph | A morph between symmetric convex shapes that never drops below a known volume and bloats less than the classical morph. | Guarantee (incidental speedup) |
| 09 Cubic interference | Number theory: sums that look random carry a steady drift. There is no algorithm and no speedup. | Warning, for mathematicians only |

## 2. Bridge patterns

- **A proof of a winning strategy becomes a compiled lookup table.** You compile it once and play each move in constant time, with a guarantee no search gives. Fits: Snaky (G5.1). Classical relatives are chess endgame tablebases and the solved game of Connect Four.
- **A proved optimum lets you build the answer instead of solving for it, and serves as a test oracle for any solver.** Fits: periodic blob (G10.1), the crowd demo's closed-form cells, the crossing target Z(n) (G5.2), and the cycle/clique board size (G5.6).
- **A structural condition becomes a design-time lint** that runs on data files in the editor or CI. Fits: economy weak reversibility (G4.4), the faction-clue threshold (G7.1), and the wrap-around island check (G1.4, which works without the new paper).
- **A proved scaling law becomes resolution-independent tuning.** Fits: fractal clock (G6.2), the Voronoi connectivity slider (G6.3, which rests on an unproven assumption), and the 3-SAT puzzle band (G7.6).
- **A proved mixing bound becomes a certified iteration budget for a randomizer.** Fits: dungeon shuffle (G4.1).
- **A sharp sensitivity result tells you how much stickiness to add.** Fits: crowd hub (G10.3). Also G7.4: a deck where every card looks fairly placed can still be badly ordered, so use a Fisher–Yates shuffle.

## 3. Top 5 practical methods

### 1. "Every recipe needs a way back": a lint for recipe economies (G4.4; demos 01 and L4)
**Where:** colony sims, city builders, 4X games and ecosystem sims. It matters most for background economies of hundreds to tens of thousands of off-screen towns, where nobody would notice a slow collapse.

**Plain version:** Think of each recipe as a one-way road between bundles of goods, like "ore + wood → tools". The rule is that for every road, some route of other roads must lead back to where it started. If that holds, the paper proves no good can ever run out or pile up forever, however fast or slow you set each recipe. The town may swing at first, then it settles inside a safe band. Delete a way back and the promise is gone: in our test, removing two return recipes made every run starve some goods and flood others. Checking the rule is a map-reading job that a tool does instantly.

**What the math adds:** the promise holds for any recipe speeds and any network size. Before this paper it was proven only for special cases: two resources, a single recipe loop, or "balanced" networks. The demo network fits none of them. The classical alternative is hand-tuning plus caps, sinks and sources, which hides instability rather than ruling it out.

**Gain:** a guarantee, measured on one network.
- **Survival:** without the return recipes, 6 of 30 resources went extinct and 6 of 6 runs ran away. With them, there were no extinctions or runaways, and every resource stayed within [0.48, 1.55].
- **Scale:** across 16,384 towns with rates jittered by up to 1.5× either way, 99.4–100% of the failing towns were dead or runaway by t = 120. None of the passing towns were.
- **Cost:** the failing economy cost ×53–65 more per frame (1,024 towns per 60 fps frame against 65,536), but only while it was collapsing.
- **Game-style integration:** with a fixed-step RK2 integrator, no step size kept the failing towns valid. The passing towns had 0% failures at h = 1/512.

**Engineering bridge:** about a day or two in total.
- **Lint:** make one node per distinct bundle ("Food + Workers" and "2 Workers" are different nodes) and one arrow per recipe. Find the strongly connected components, then flag any recipe whose output node cannot reach its input node. That is about 100 lines.
- **Runtime:** each recipe runs at rate k × (product of its input amounts). Step it with fixed-step RK2 in the economy tick and display rounded integers.

**Risks:**
- The proof covers continuous amounts with fixed positive rates. It does not cover integer stockpiles, random small-number effects, rates the player changes, caps, or price markets.
- It gives no number for how wide the safe band is or how long settling takes.
- Passing guarantees survival, but failing does not guarantee collapse.

**Visual:**
- **Left canvas:** demo 01's nine recipes drawn as three triangles of bundle nodes (village, forge, workshop). Clicking an arrow deletes or restores it. A banner shows "OK" or names the recipe that has no way back, e.g. "Food + Workers → 2 Workers".
- **Right canvas:** log-scale lines for the 5 resources from 6 random starts. Use the `econ.h` rates (1.00, 0.35, 0.60, 1.20, 0.25, 0.80, 0.90, 0.50, 0.70) and fixed-step RK2 at h = 1/512 with 256 substeps per frame. A run greys out when any resource leaves [1e-12, 1e12].
- **Controls:** a "re-roll rates" button multiplies every rate by a random factor between 0.5 and 2.
- **Readouts:** min and max resource, and counts of extinct and runaway resources.

### 2. A compiled, proven opponent: the Snaky certificate (G5.1; demos 03 and L1)
**Where:** a turn-based claim-the-squares duel or puzzle game on a 17×17 board. One side tries to complete the six-square "Snaky" shape and the other only blocks. It also fits a minigame inside a larger game, or server bots running thousands of matches.

**Plain version:** A normal game AI thinks before every move. It imagines thousands of possible futures and picks the best-looking one, which takes time, and under pressure it still guesses wrong. Here, mathematicians already did all the thinking. They published a complete playbook ("if the opponent blocks here, play there") that is proven to finish the shape within 21 moves against anyone. We store the playbook once, and each move becomes a page lookup instead of a brainstorm. It's like following a recipe card instead of inventing the dish: about a microsecond per move, and it never loses.

**What the math adds:** a 38 KB proof that is itself the strategy. Search has no guarantee: at 6,000 nodes per move it won only 41 of 100 games against a search-based blocker. Its win rate also didn't improve steadily with more budget: 74% at 4,000 and 6,000 nodes, then 100% at 12,000 nodes but taking up to 62 moves. The classical alternative is brute-force solving, which only works on tiny boards.

**Gain:** measured speed plus the guarantee.
- **Per move:** about 5 ms for search against about 1 µs for the certificate, ×55k–90k per decision.
- **Wins:** 100 of 100 against the search blocker.
- **Games per 60 fps frame:** 4 with search, 131,072 with the certificate.
- **Under a shared frame budget:** search's win rate fell from 100% to 43% at 64 games and to 0% at 1,024. The certificate stayed at 100%.
- **Memory is not a win:** 94 KB of tables against 62 KB for the search AI.

**Engineering bridge:** mostly done already. `snaky_cert.h` rebuilds all 728 cards and matches the published hashes byte for byte. To ship it, bake the tables into a binary at build time. At runtime, after each opponent move, step to the first child card whose region avoids that move and play its pivot cell. That is constant time and fits in the turn handler. What remains is the game around it: days of work.

**Risks:**
- It covers one shape, on a square grid, under rules where the blocker never wins by building a shape of its own. The 21-move bound is not an optimal move count.
- An unbeatable AI isn't fun by itself, so the design has to use it differently:
  - Let the player be the blocker and score moves survived. The best possible blocker forces exactly 21 moves.
  - Use it as a coach for the player as builder.
  - Mix certificate moves with random moves as a difficulty dial (unmeasured).
- For a single turn-based game, 5 ms of search is already affordable. The speed only matters with many concurrent games.

**Visual:**
- **One-time setup:** export the cards from `snaky_cert.h` to JSON (pivot, region, and child links with a symmetry and offset each). The JS walker is about 60 lines.
- **Left panel:** a greedy threat-search builder with a node budget of 16.7 ms divided by N.
- **Right panel:** the certificate walker.
- **Controls:** a slider for N from 1 to 4,096 games, drawn as mini-boards, with the same seeded blockers on both sides.
- **HUD:** ms per frame for each side against a 16.7 ms line, win %, and longest win.
- **Play mode:** a "You block" toggle on a large board. You click cells to block, teal shading shows the strategy's live region, and the score is "moves survived out of 21".

### 3. Fair level-graph shuffle with exact door counts (G4.1; demos 06 and L5)
**Where:** roguelike or metroidvania level graphs where a designer fixes each room's door count. Also NPC relationship webs where each NPC has exactly k contacts, and portal or trade-route networks. It runs at level load, not every frame.

**Plain version:** Give every room a fixed number of door slots. Any way of joining the slots in pairs is a possible level. To make a new level, you start from the designer's level and swap the ends of some corridors. With too few swaps, the new level quietly resembles the old one and players feel the repetition. The paper gives a number of swaps of a particular kind that is proven to be enough: after that many, every possible level is about equally likely. It's like knowing how many riffles fully mix a deck of cards.

**What the math adds:** a proven step count for every possible list of door counts. The shuffle itself (Curveball) is older work; the paper supplies the count. Run T = ⌈B(1+B/2)·ln 2⌉ trades, where B = rooms × (rooms − 1) / 2, and the layout is within ¼ of perfectly fair (total-variation distance) from any start.

There are two classical alternatives:
- **Configuration model:** deal door slots into random pairs and discard results with doubled doors or self-loops. This is exactly fair and probably cheap at 1–6 doors per room. That is my assessment; we did not measure it.
- **Many more ad hoc swaps:** on the 8-room test graph, 10 swaps per door got within 1.4e-4 of fair, but nothing guarantees that in general.

**Gain:** variety and fairness, measured.
- **Bias of the usual shortcut:** one swap per door kept 9.77 template doors per 20-room layout, where a fair shuffle keeps 5.64.
- **Exact distance from fair on two small test graphs:** 0.215 and 0.459 for the shortcut, about 1e-14 with the certified count.
- **Cost per layout:** 0.33 ms at 20 rooms, 6.3 ms at 40, 200 ms at 80 and 5.2 s at 160, growing about as rooms^4.4. Per-frame use stops around 49 rooms. At level load, 80 rooms takes 200 ms.

**Engineering bridge:** about 60 lines and about a day of work.
- Keep a neighbor set per room. Each trade picks two rooms, pools the neighbors they don't share, shuffles that pool, and deals it back so each room keeps its count.
- Check up front that the door counts are possible at all (Erdős–Gallai, a classical test).
- After shuffling, discard layouts that are disconnected or don't fit the map, and generate again. Filtering finished layouts keeps them fair. Rejecting moves inside the shuffle breaks the guarantee.

**Risks:**
- Fairness is over abstract graphs, so corridors ignore geometry. If most layouts fail your spatial checks, regenerating gets expensive.
- The count is a worst-case bound and is probably far more than needed.
- Beyond about 100 rooms, the certified count is too slow (5.2 s already at 160). Use the configuration model there.

**Visual:**
- **Layout:** 12 rooms on a 4×3 grid, with pips showing each room's door count.
- **Controls:** a method dropdown (swap, Curveball, configuration model) and a log-scale slider from 1 step per door up to the certified 1,556 trades.
- **Each frame:** build about 50 layouts and draw the latest, with kept doors in grey and new doors in blue.
- **Readouts:** a histogram of "template doors kept" against a reference from configuration-model samples, plus the mean, the histogram distance and µs per layout.

### 4. A resolution-proof clock for fractal fronts (G6.2; demos 04 and L3)
**Where:** fronts drawn as critical percolation, such as wildfire in brush at the tipping density, creeping corruption, or tracing cracks and tunnels. It matters when quality settings, LOD, or a server/client split run the front on different grid sizes and gameplay timing must match.

**Plain version:** A fractal front, like fire creeping through dry brush, is wiggly at every zoom level. Draw it on a finer grid and it gets longer, because more wiggles appear. If every grid step costs the same time, the fine-grid fire takes far longer to cross the map. The paper proves the exact discount: as cells shrink, each step's time must shrink a bit faster than the cell width, at the 1.75th power. With that rule and one tuning constant, a small 32×32 grid gives the same typical crossing times as a huge 8192×8192 grid, within a few percent.

**What the math adds:** the 1.75 exponent itself is classical (Beffara, 2008). What the paper adds is proof that one constant times this simple power is exactly right, with no hidden correction, and that the time the front spends in each region also comes out right. The classical alternative is re-tuning the clock at each grid size from a reference run.

**Gain:** a design rule that cuts compute. With the paper's clock, a 32² grid gets within 5% on median crossing time and within 10% on time spent per region against the 8192² reference, at 6.5 µs per front. A clock tuned only at 8192² meets that target only at 8192² itself (107 ms per front), ×16,150. **Honest caveat from L3:** that gap follows by construction. A clock re-tuned at every size would match, so the real gain is never having to re-tune.

**Engineering bridge:** one line in the front's tick, `step_time = C * cell_size^1.75`, with C set once. Keep grids at 32 or larger: n = 16 is off by 6.7%. About an hour, if you already have the front.

**Risks:** it applies only to critical-percolation fronts. Away from the tipping density, or for other growth rules, the exponent changes or isn't proven. The match is statistical: each front is a different random shape, and only the typical timing agrees.

**Visual:**
- **Three lanes:** n = 16, 64 and 256. Each samples a front by porting `fk.h`'s tracer (about 60 lines) and replays it with step time (1/n)^p.
- **Controls:** a slider for p from 1.0 to 2.0, plus a "snap to 1.75" button.
- **Readouts:** median crossing time per lane over the last 200 fronts, and the 256/16 ratio. Expect about 8× at p = 1 and about 1× at p = 1.75.

### 5. Sticky crowd assignment: budget for re-planning churn (G10.3; demos 05 and L5)
**Where:** RTS formation slots, worker-to-job assignment in colony sims, crowds choosing exits in evacuation sims, and tower-defense lanes. It applies to anything that re-solves "who goes where" as targets move.

**Plain version:** Split a crowd among three exits so total walking is as short as possible. Now nudge one small exit a little. Some people near it don't just shift slightly: the best plan sends them to a completely different exit across the map. The paper proves this effect can't be avoided by the best plan, and it shrinks much more slowly than the nudge: a target change a thousand times smaller can still leave a tenth of the re-routing. Meanwhile, letting everyone keep their old exit costs only a little extra walking. So don't re-plan every frame: re-plan on a timer, or when the plan has gotten noticeably worse.

**What the math adds:** a proven worst-case law for churn. Total re-routing is at most a constant × (target change)^(1/3), with a constant that does not grow with the number of destinations, and no exactly optimal planner can do better. Hysteresis is already standard practice; the math tells you how much churn to expect and that you can't optimize it away.

**Gain:** a warning and design rule, measured.
- **Switching:** re-planning switched 11.25% of agents at hub size 0.45, falling only to 0.5% at hub size 0.02.
- **Exponent:** the measured slope of crowd change against target change was 0.334, close to the proven 1/3.
- **Cost of not re-planning:** frozen labels kept every exit's total correct, and their extra transport cost fell from 0.0169 to 3.3e-5 over the same range.
- **Compute:** re-planning every frame cost 2.8–4.2× more than frozen labels.

**Engineering bridge:** keep your existing optimal planner and trigger it on a timer or on a cost-gap threshold. Hours of work.

**Risks:**
- The dramatic case needs a small-capacity target that moves. A fixed-size exit nudged slightly re-routes a share proportional to the nudge.
- The hysteresis policy itself is unmeasured.

**Visual:**
- **Scene:** 20k dots in a square and three exits (left, right, and a central hub that oscillates).
- **Controls:** a slider for hub size from 0.02 to 0.45, and a policy toggle: re-plan every frame (demo 05's rule, argmax{−x, x, a + s·b·y}), frozen, or re-plan every k seconds.
- **Display:** color dots by exit and flash switches yellow.
- **Readouts:** % switched per cycle, extra cost against optimal, and a log-log scatter with a slope-⅓ guide line.

## 4. Generalization ideas (all speculation)

1. **Certify and compile your own small games.** For a designer's variant of the claim-and-block game (any shape, any board), run an offline proof search with symmetry folding. If it proves a forced win, emit Snaky-style cards and ship them. Whether a given shape has a small certificate is unknown until you search.
2. **Lint every flow system, not just recipes.** Plague, ecosystem and influence models often use the same rule: rate = k × the product of amounts. A standard SIRS plague (S+I→2I, I→R, R→S) fails the check. Adding 2I→S+I and S→I makes it pass, which by the theorem would keep the plague endemic: no group ever runs out, in the continuous model. That gives a designer a switch between a plague that burns out and one that never goes away.
3. **A solvability lint for deduction games.** G7.1 proves that distant rumors in a tree of informants (d sub-informants each, each retelling staying correct with probability (1+2λ)/3) carry information about a hidden faction exactly when d·λ² > 1. Below that line, even a perfect detective learns nothing from deep rumors, so a mystery generator should stay above it or put clues near the root. The paper gives no numbers for finite depths, so playtest near the line.

## 5. Don't bother list

- **09 Cubic interference:** same cost on both sides and no algorithm. The lesson "arithmetic sums aren't random" is old.
- **02 Fluid gates:** 6–45× slower, to guarantee no overlap or compression that players can't see. Use a tween unless incompressibility is the puzzle rule.
- **07 Periodic blob as a feature:** the ×96–157 speedup is real, but few games need least-surface shapes in a wrap-around cube. At most, use it as a test oracle for a surface-tension solver.
- **08 Convex morph:** the classical Minkowski morph already has a volume floor. The new morph only bloats less (1.37–1.39× the floor against 1.62–1.69×) and covers only origin-symmetric convex shapes. It's a one-liner only if you already store shapes as k-DOP slabs: h = h_K^(1−t)·h_L^t.
- **G3.4 NPC noise ceiling:** it excludes base utilities, which every utility AI has.
- **G9.1 median diffusion:** its constant of 3024 is too loose to tune by.
- **G6.3 Voronoi slider and G7.6 3-SAT band:** the first rests on an unproven assumption, and neither gives numbers, so you calibrate empirically anyway.
- **G5.5 two-hop start:** it forbids two-way corridors, and you can simply compute the answer directly.
- **Quantum and physics leads (G8.x, G7.2, G10.4), existence-only results (G2.4), leads beaten by classical bounds (G3.5) and the galactic results:** no game hook.
