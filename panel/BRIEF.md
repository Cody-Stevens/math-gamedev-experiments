# Panel brief: from pure-math results to practical game wins

You are one of four independent panelists (different AI models). Work alone; the orchestrator merges the four reports.

## Who is asking and why
Cody is an indie game / simulation developer, not a mathematician. Over the last week we scanned recent pure-math
preprints for game/sim uses, verified 58 leads, and built C++ before/after demos plus "load ladders" that measure cost as
the number of elements grows. Cody watched the results and said, roughly:

> "How could these be used to make a practical performance gain or logic gain in an actual game environment I could
> create? Part of it may be my ignorance of what is actually happening in your examples. Because these are pure
> mathematics in origin, we may have to do some work to figure out how to apply them effectively. Return the most
> practical methods, each with the simplest possible explanation in a single paragraph plus a visual, so I can understand."

Your job: theorize that bridge. Turn the results into concrete, buildable game techniques, rank them by practicality,
and be honest about which ones are not worth a game developer's time.

## Evidence to read (all under <repo> unless noted)
- Measured results, most important first:
  - `demos\sheet-ladder\out\load-ladders.html` (summary of the load ladders) and each ladder's `out\ladder.json` and
    `README.md` in `demos\L1-snaky-ladder`, `L2-blob-ladder`, `L3-fractal-ladder`, `L4-economy-ladder`, `L5-sweeps\*`.
  - The nine demos `demos\01-economy` … `09-cubic-interference`: `README.md` and `out\clean\results.json`.
- Summary of the load ladders (clean sequential run, Ryzen 9 9900X, 24 threads):
  | Demo | WITHOUT fits 60 fps up to | WITH fits up to | Gap |
  |---|---|---|---|
  | Snaky (Maker-Breaker game AI vs a proved 21-move strategy certificate) | 4 concurrent games | 131,072 | x55k-90k per decision; search AI's win rate under a fixed frame budget falls 100% -> 43% at 64 games -> 0% at 1024; certificate wins 100% |
  | Fractal frontier (animation clock for a fractal percolation front) | needs an 8192^2 grid (107 ms/curve) | 32^2 grid matches it (6.5 us/curve) | x16,000 less compute at equal accuracy |
  | Periodic blob (minimum-surface shape in a wrap-around cube) | never | 256^3 | x96-157 time-to-2%; iterative solver gets stuck ~1 run in 9; closed form never fails |
  | Economy (weakly reversible recipe graph keeps every resource alive) | 1,024 towns | 65,536 towns | x53-65 per frame, but only while the broken economies collapse; stiff fixed-step integration breaks the non-reversible graph |
  | Fluid gates (exact volume-preserving piecewise-linear maps) | >1M particles | 64k/region | WITH 6-45x slower; it is a guarantee, not a speedup |
  | Crowd hub (optimal-transport assignment stability) | 16M | 8.4M | WITH 2.8-4.2x slower; frozen labels misroute 11.25% |
  | Dungeon shuffle (certified-uniform random graph with exact door counts) | >320 rooms | ~49 rooms | certified burn-in grows ~n^4.4; cheap shuffles stay visibly biased |
  | Convex morph (volume floor for interpolating convex shapes) | 400 | 1600 | 1.2-4.4x faster, incidental |
- The 58 verified leads: `verify-games\table.json` (id, title, family, verdict SOLID/STRETCH/OVERSTATED, pitch, demo
  worthiness) and the detailed verifications `verify-games\v-A.md` … `v-F.md`; the original lead write-ups are
  `games\slice-01.md` … `slice-10.md`. Several SOLID leads were never demoed; look at them too.
- Papers, if you need to check a claim: `<openai\math>\preprints\<family dir>\` (`build\*.tex`).

## What to deliver
Write your report as your FINAL MESSAGE (markdown, at most ~2,500 words). Do not create or edit any files. Sections:

1. **What is actually happening** (short): for each of the nine demos, one or two plain-English sentences on what the
   math really buys, so a non-mathematician gets it. Name the general *kind* of gain: speed, guarantee/correctness,
   exact target, content variety, design rule, or warning.
2. **Bridge patterns** (3-6 bullets): the general recipes for turning a pure-math theorem into game value (e.g. "a
   proof of a winning strategy is an offline-precomputed lookup table: compile it once, play it in O(1)"; "a closed-form
   optimum replaces an iterative solver"; "a structural condition on a graph is a design-time lint rule"). Each pattern
   should name which results here fit it.
3. **Top 5 practical methods**, ranked by (real benefit to a game Cody could build) x (ease of building). For each:
   - Name, and the game situation where it applies (be concrete: genre, system, scale).
   - The single-paragraph plain-English explanation (at most 120 words, no jargon, no formulas; an analogy is welcome).
   - What the math contributes that a normal developer would not already have, and what classical alternative exists.
   - Gain type and expected size, citing our measurements where they exist; say "unmeasured" otherwise. Never invent numbers.
   - The engineering bridge: what work turns the paper into shippable code (precompute step, data format, runtime cost,
     where it plugs into a typical engine loop), and roughly how big that work is.
   - Risks / why it might not be worth it.
   - A visual that would make it click: describe a tiny interactive browser demo (canvas, under ~200 lines of JS) in
     enough detail to build it: what is drawn on each side, what the user can drag or toggle, what number to show.
     It should run in real time in a browser, so pick a scaled-down but honest version.
4. **Generalization ideas**: up to 3 ways a method could extend beyond the exact paper setting into something
   broader for games (e.g. other board games whose strategies could be certified and compiled; other recipe graphs a
   designer could lint). Mark clearly which parts are speculation.
5. **Don't bother list**: results that look exciting but give a game developer nothing practical, with a one-line reason.

## Rules
- READ-ONLY. Do not modify, create or delete files anywhere. No servers, no network calls beyond what your harness needs.
- Honesty: speculation is welcome but must be labeled. Never present an unmeasured speedup as measured. If a classical
  technique already gives the same thing, say so.
- Do not read or copy secrets (.env files, credentials, auth files).
- Other panelists run at the same time; files may change while you read. Ignore unrelated files.
