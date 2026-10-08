# Notes: what we learned and where to go next

*Written by Claude (Opus 5.5), the agent that ran these experiments, at Cody's request. The same disclaimer as the README applies: this is exploratory, small-scale and unreviewed.*

## The pattern behind all five

The math does not make computers faster. Each result **hands you an answer you would otherwise have to search for, simulate, tune or playtest**:

| result | what it hands you | what you get to skip |
|---|---|---|
| FK interfaces (7/4 law) | a scaling law | re-tuning gameplay per graphics setting, or simulating at full detail |
| Snaky in 21 | a complete playbook | game-tree search |
| Uniform permanence | a rule you can check on the recipe chart | long balance simulations |
| Switch-chain mixing | a proven step count | guessing a burn-in, or retrying until it fits |
| Flat 3-torus | the best shape, in closed form | running a shape optimiser |

That also tells you where *not* to expect a gain. If your current method doesn't search, tune, simulate or guess, there's nothing to skip. Every win is also tied to the exact model the theorem covers. Change the model (add an obstacle, a new fire rule, integer stockpiles) and you are back to measuring.

## The general idea: scaling exponents for resolution-independent procedural generation

Wildfire Isle generalises beyond fire. When you measure a quantity along a rough, fractal-like boundary, its value depends on grid resolution. Examples are coastline length, the number of shore tiles, the length of a fire front or a river path. Medium and Ultra then disagree, and anything tuned per tile (spawn rates, travel times) drifts.

**The fix:** weight each cell's contribution by (cell size)<sup>d</sup> and tune one constant. The FK paper proves d = 7/4 for its fire model. For other boundaries, measure d offline at three resolutions.

`panel/general/coast.js` tests this on ordinary fBm terrain (roughness H = 0.75). Its output, from `node panel/general/coast.js`:

| grid n | 64 | 128 | 256 | 512 | 1024 | drift |
|---|---|---|---|---|---|---|
| coast edges (detail grows with n) | 727 | 1,730 | 4,222 | 10,270 | 24,994 | |
| weighted by (1/n)<sup>1</sup> (the usual "per metre") | 11.4 | 13.5 | 16.5 | 20.1 | 24.4 | **2.1×** |
| weighted by (1/n)<sup>1.25</sup> (d = 2 − H) | 4.02 | 4.02 | 4.12 | 4.22 | 4.31 | **7%** |
| same, but detail fixed at 6 octaves, (1/n)<sup>1</sup> | 12.4 | 13.8 | 16.5 | 17.1 | 17.2 | settles |
| same, fixed detail, (1/n)<sup>1.25</sup> | 4.39 | 4.12 | 4.12 | 3.60 | 3.04 | overcorrects |

**Lesson:** the right exponent depends on whether your generator adds detail as resolution goes up. If it does, the "per metre" rule drifts badly and an exponent near 2 − H fixes most of it. If detail is fixed, plain "per metre" is already fine. Only the fire's 7/4 is proven. Anything else is a fitted number, so measure it.

**Where this could pay off:**
- Gameplay that must not depend on graphics settings: spawn rates along coasts, travel time along rivers and cliffs, how fast fire or infection spreads.
- Servers that simulate at low resolution while clients render high.
- Deterministic balance across platforms.

## Per-technique: what to try next

1. **Fractal timing (Wildfire Isle).**
   - Measure d for the spread rules games actually use (cellular-automaton fire, Eden growth, invasion percolation). The 7/4 is only proven for critical FK interfaces.
   - Package it as an engine component (Unity or Unreal) that takes a measured d.
   - Check that it still *feels* the same, not just that the median time matches.
2. **Proof as playbook (Tavern).**
   - Look for other games with computer-checked winning strategies, such as Maker-Breaker and other positional games. A certificate makes a perfect boss or tutorial AI that costs almost nothing.
   - Compress the table (265 KB today).
   - Remember the honest limit: it only covers the proven side of one game.
3. **Economy lint (Colony Ledger).**
   - Run the check on every save in a data-driven crafting editor, or as a CI check on recipe JSON.
   - Pair it with a short simulated playtest for pacing. The lint can't see how *low* a resource sits (the `starve` scene passes the lint while every town starves).
   - Extending it to integer or random stockpiles needs new math. The theorem covers the continuous model only.
4. **Certified shuffle (Dungeon Daily).**
   - Switch methods adaptively. Estimate the classic method's acceptance rate from the door counts, and use Curveball only when that rate collapses.
   - The proven step count is very conservative. Measuring the real mixing time could cut the cost a lot, but then it's no longer proven.
   - At 80 rooms it takes about 300 ms, so it's a loading-screen job.
5. **Known best shape (Slime Lab).**
   - Use the closed form as a warm start for the solver when obstacles break the theorem. With a pillar it helped at 25–40% fill and not at 10% (see the game README).
   - Good for blobs, liquids and VFX in wrap-around levels. Non-cube boxes are an open problem.

## Leads not turned into games

- `demos/` has C++ before/after demos for four more results:
  - 02 fluid gates: area-preserving mixing maps;
  - 05 crowd hub: a tiny hub move reassigns a large crowd;
  - 08 convex morph: a morph with a guaranteed volume floor;
  - 09 cubic interference: a certified bias in phasor walks.
- Each has a README with its numbers.
- `demos/L5-sweeps` times several of them under load. Most are "guarantee, not speed-up" results.
- `research/verified-leads/` lists every lead found and how it held up when checked.

## What it would take to make this rigorous

- An independent re-implementation on other hardware and engines.
- Stronger baselines:
  - exact switching samplers (McKay–Wormald, Gao–Wormald) for dungeons;
  - modern level-set and phase-field solvers for slime;
  - tuned MCTS for the board game.
- A mathematician checking that each game's model really meets each theorem's conditions. For example: is the fire exactly the critical FK interface? Are the recipes exactly mass-action?
- Playtests for "feel", not just medians.

## How it was made

1. A scan of every paper in openai/math for game and simulation leads (`research/`).
2. Nine C++ split-screen demos and load ladders (`demos/`).
3. A four-model panel on practical uses (`panel/reports/`).
4. Five games, each built by a sub-agent from `games/BRIEF.md`. I then re-ran each game's bench to check its numbers.
5. Clips recorded with a scripted headless Chrome (`games/video/`).

Every number in the captions and posts comes from a game's README or `bench.js` output.
