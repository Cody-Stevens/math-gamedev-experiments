# New math → game-dev tricks: five exploratory experiments

> [!WARNING]
> **Exploratory only. Not peer-reviewed, not independently verified.**
> This is a quick experiment to see whether some brand-new math results could give practical gains in games. It is not research and not a benchmark.
> - The math comes from preprints in [openai/math](https://github.com/openai/math). They have not been through peer review either.
> - Every test is small and ran on one machine (Ryzen 9 9900X, Node 22, Chrome, one thread).
> - I chose and tuned the baselines ("prior best") myself. A stronger baseline may exist.
> - Most of the code and text was written by AI agents. It was checked by running it, not by expert review.
>
> Read every number as "this is what happened in this test", not "this is what you will get". Corrections and better baselines are very welcome (open an issue).

In October 2026 [openai/math](https://github.com/openai/math) published a large batch of new proofs. I wanted to know whether any of them give a game developer a real edge. With a team of AI agents, I did four things:
1. Scanned the papers for results that touch games or simulation.
2. Had a panel of four models argue about which ones are practical.
3. Picked five and turned each one into a game technique.
4. Built a small browser game for each one. Every game runs the new technique side by side with the best method I could find that people already use.

Orange is today's best method. Blue is built on the new proof.

## Results at a glance

| # | Game | Gain in this test | The trick | Where the old way still wins | Paper |
|---|---|---|---|---|---|
| 1 | [Wildfire Isle](games/1-wildfire-isle) | **~400× less compute** for the same fire timing: 18.2 h on a 32×32 grid vs 18.0 h on 1024×1024. Usual scaling gives 10.8 h. | Time each grid step by (tile size)<sup>1.75</sup> instead of by tile size. | If you can afford the full-detail grid, it is exact. The 1.75 is proven only for this fire model. | [FK interfaces](https://github.com/openai/math/tree/main/preprints/Natural-Occupation-Measures-for-Critical-Square-Lattice-FK-Interfaces-October-5-2026) |
| 2 | [Tavern of a Thousand Tables](games/2-tavern-tables) | **1,000×+ cheaper AI moves** (0.0002 ms vs 0.26 ms budget). With 64 games per frame it wins 100% vs 42% against a strong opponent. | The proof *is* a winning playbook that wins within 21 moves. Store it (265 KB) and look moves up. | It only works for this one game and one side. With a few games per frame, normal search wins too. | [Snaky in 21](https://github.com/openai/math/tree/main/preprints/Snaky-in-21-Maker-moves-September-25-2026) |
| 3 | [Colony Ledger](games/3-colony-ledger) | **1,000×+ faster balance check** (1.5–4.3 µs vs 5–550 ms of simulated playtest). It caught a recipe list that a 10-year playtest passed. | If every recipe can be undone by some chain of other recipes, no resource dies out or explodes, at any recipe speeds. | It says nothing about *how low* a resource can sit (starvation), integer stockpiles, or pacing and fun. | [Uniform permanence](https://github.com/openai/math/tree/main/preprints/Uniform-Permanence-in-Weakly-Reversible-Mass-Action-Systems-October-5-2026) |
| 4 | [Dungeon Daily](games/4-dungeon-daily) | **5.6–32× faster** fair shuffles on maps with several big hub rooms (3–4 ms vs 20–102 ms). | Run a "Curveball" shuffle for the proven number of steps, T = ⌈B(1+B/2)·ln 2⌉ with B = n(n−1)/2. | On normal maps the classic fair method is 9× to 37,000× faster. | [Switch chain mixing](https://github.com/openai/math/tree/main/preprints/Polynomial-Mixing-of-the-Switch-Chain-for-Every-Graphical-Degree-Sequence-September-25-2026) |
| 5 | [Slime Lab](games/5-slime-lab) | **13–18× faster** least-surface shapes, and exact (0.2–1.2 ms vs 2–22 ms). | In a wrap-around box the best shape is known. Up to half full it is a ball below 4π/81 ≈ 15.5%, a tube below 1/π ≈ 31.8%, and a slab otherwise. Above half full it is the inverse. | Obstacles, gravity or non-cube rooms still need the solver. | [Flat 3-torus](https://github.com/openai/math/tree/main/preprints/The-Isoperimetric-Conjecture-for-the-Cubic-Flat-Three-Torus-September-24-2026) |

Each game folder's README has:
- the full results table;
- the exact baseline;
- what is proved, what is only measured, and what is not claimed;
- the commands to reproduce it.

## Try it

- **Play online:** https://cody-stevens.github.io/math-gamedev-experiments/games/
- **Play locally:** open [`games/index.html`](games/index.html) in a browser. There's no server or build step, and every game is a single HTML file.
- **Watch:** [`videos/`](videos) holds the five 25–31 s side-by-side clips.
- **Re-measure:** in any game folder, run `node bench.js` (Node 22).

## What's in here

| folder | contents |
|---|---|
| [`games/`](games) | The five games and a launcher. Each has `core.js` (the techniques), `bench.js` (measurements), `README.md` (results) and `index.html` (playable). `games/BRIEF.md` is the brief the game-building agents worked from. |
| [`games/video/`](games/video) | Scripted recorder (headless Chrome over the DevTools protocol) and ffmpeg composer that made the clips. |
| [`videos/`](videos), [`posts/`](posts) | The clips and the text of the X posts. |
| [`demos/`](demos) | The earlier C++ "WITHOUT vs WITH" split-screen demos (01–09) for nine results, plus the load ladders (L1–L5) that time each one under growing load. See `demos/HARNESS.md`. Videos are not included (too large); the JSON results are. |
| [`panel/`](panel) | The panel brief, the four panel reports (Grok 4.7, Claude Opus 5.5, GPT-6 Astra, SWE-2), the interactive explainer page (`panel/web/out/practical-math.html`), the JS ports of each technique with tests, and `panel/general/coast.js` (the scaling-exponent experiment in NOTES). |
| [`research/`](research) | The paper-by-paper scan for game and simulation leads, and the follow-up check of each lead. |
| [`NOTES.md`](NOTES.md) | Commentary, the general idea behind all five, and where to go next. |

## Reproduce

You need Node 22, Python 3, ffmpeg and Chrome. To rebuild one game, run this in its folder:
```bash
node bench.js
python build.py
```
`build.py` inlines the code into `index.html`. To re-record one clip:
```bash
cd games/video
node record.js 1
python compose.py 1
```
The C++ demos use g++ and `demos/build.sh` (see `demos/HARNESS.md`).

## Credits and license

- **Math:** all theorems are from [openai/math](https://github.com/openai/math) (Apache-2.0). All credit for the results goes to its authors. Any mistake in applying them is mine.
- **Code and text:** by Cody Stevens with AI agents. Claude Opus 5.5 (Claude Code) orchestrated, built and measured. Grok 4.7, GPT-6 Astra, Claude Opus 5.5 and SWE-2 formed the review panel.
- **License:** Apache-2.0 (see `LICENSE`). `demos/harness/vendor/` contains the public-domain stb headers.
