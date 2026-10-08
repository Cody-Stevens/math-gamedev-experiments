# Games / simulation / visualization brief: mine new mathematics for game-dev and sim leads

## Context
<openai\math> is a release of ~722 new research manuscripts in 372 result families (numbered 001-377), produced by an internal OpenAI model in Sept-Oct 2026.
- CONTENTS.md: family descriptions + every manuscript's abstract, with links to preprints/<dir>/<file>.pdf (often .tex source in the same dir).
- overview.tex: longer family descriptions.

Cody (a developer) wants to move into GAME DEVELOPMENT and SIMULATION. He wants to apply this new pure mathematics to:
1. Gameplay mechanics (economies, AI opponents, procedural generation, puzzles, shuffles/loot/randomness, matchmaking, pathing, physics toys, emergent systems).
2. Novel visualizations (things that look striking and were not easy/possible to depict before: new tilings, fractal/random geometry, chaotic dynamics, phase transitions, random surfaces, interfaces, packings).
3. Calculations / performance (an algorithm or bound that runs faster, scales better, or is provably correct/stable at game-loop scale — NOT galactic algorithms with astronomical constants; flag those as "galactic" if you mention them at all).
4. Simulation design rules (a theorem that tells you when a sim stays stable / blows up / mixes / percolates / synchronizes — which a designer can use as a knob).

The output will feed a follow-up phase where the best leads become small C/C++ visual demos run "with vs. without the new math" and recorded on video with benchmark data (fps, step time, error, stability metric). So favour leads that have CONCRETE, CONSTRUCTIVE content (an explicit algorithm, construction, formula, threshold, rate, or object you can draw/simulate in a few hundred lines of C), and say what the "without" baseline would be.

## Your job
Read EVERY family in your assigned slice of CONTENTS.md (line range given below). For each family ask: does the theorem, algorithm, construction, threshold, rate, or counterexample give a game/sim/visualization something it couldn't easily do before, or a faster/more correct way to do it? Open the paper's .tex (preferred) or .pdf for anything promising, to confirm what is actually proved and whether there is an explicit construction you could implement.

Speculative findings are WANTED, but honest: separate what the paper proves from what you extrapolate. Skip pure-math families with no plausible bridge — do not force matches.

## Rules (anti-hallucination)
- Cite the family number and the exact manuscript path (preprints/<dir>/<file>) that you actually opened or whose abstract you read. Quote one sentence verbatim from the abstract or paper.
- State what the result actually proves vs. what you extrapolate. If constants are astronomical or the result is existential-only, say so.
- READ-ONLY everywhere except your own output file. No builds, installs, servers, commits.

## Output
Write markdown to the output path given below. Format:

```
# Slice N games/sim findings (families AAA-BBB)
Families scanned: <count>. Families with findings: <list>.

## G<N>.<k>: <punchy title>
- Family / paper: <###> — <paper title> — `preprints/<dir>/<file>`
- Quote: "<verbatim sentence>"
- Kind: gameplay | visualization | performance | sim-design-rule (one or more)
- Idea: <2-4 sentences: the mechanic / visual / calculation, and what it unlocks>
- Demo sketch: <2-3 sentences: a small C/C++ demo, what is shown WITH the new math, what the WITHOUT baseline is, and the measurable metric (fps, ms/step, error, drift, mixing distance, ...)>
- Proven vs. speculative: <1-2 sentences>
- Wow: low/med/high · Confidence: low/med/high · Effort: S/M/L
```
Rank best-first. Typically 3-8 per slice; zero is acceptable. End with a 3-line "Near misses" list.
Your final message: the output path and a one-line summary of your top finding.
