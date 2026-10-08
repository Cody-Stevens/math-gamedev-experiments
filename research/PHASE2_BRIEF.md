# Phase 2 brief: mine new mathematics for opportunities in Cody's repositories

## Context
<openai\math> is a release of ~722 new research manuscripts in 372 result families (numbered 001-377), produced by an internal OpenAI model in Sept-Oct 2026. Many are breakthroughs: new algorithms, sharp thresholds, counterexamples, approximation schemes, complexity bounds, probability limits, etc.
- CONTENTS.md: family descriptions + every manuscript's abstract, with links to preprints/<dir>/<file>.pdf (and often .tex source in the same dir).
- overview.tex / overview.pdf: longer family descriptions.
- lean/formalization.yaml: which papers have Lean formal proofs.

Cody's software repositories are profiled in:
- <repo>\COLLECTIVE.md (read this first, fully: purpose + math hooks for ~70 repos)
- <repo>\repos\group-1.md ... group-6.md (full profiles: stack, architecture, hotspot files/functions). Consult them to name concrete files/functions.

## Your job
Read EVERY family in your assigned slice of CONTENTS.md (line range given below). For each family ask: could this result — its theorem, its algorithm, its proof technique, its bound, or its counterexample — plausibly improve efficiency, performance, correctness, or unlock a new approach in any of Cody's repos? Open the paper's .tex/.pdf for anything promising to understand what is actually proved and whether there's an algorithmic/constructive content.

Speculative findings are WANTED. Cody wants imaginative-but-grounded ideas, not fully vetted engineering plans. But each finding must be honest about the gap between the math and the code. Pure-math results with no plausible computational bridge (most of algebraic geometry, set theory, etc.) should simply be skipped — do not force matches. Also consider "negative" value: a result showing a heuristic the repo relies on can't be beaten / is NP-hard / has a sharp threshold is a useful finding too.

## Rules (anti-hallucination)
- Cite the family number and the exact manuscript path (preprints/<dir>/...) that you actually opened or whose abstract you read. Quote one sentence from the abstract or paper verbatim.
- Cite the repo and a concrete file/function taken from the profile reports (or that you verified in ~\Documents\GitHub\<repo>). Never invent file names.
- State what the result actually proves vs. what you are extrapolating.
- READ-ONLY everywhere except your own output file. No builds, installs, servers, commits.

## Output
Write markdown to the output path given below. Format:

```
# Slice N findings (families AAA-BBB)
Families scanned: <count>. Families with findings: <list>.

## F<N>.<k>: <punchy title>
- Family / paper: <###> — <paper title> — `preprints/<dir>/<file>`
- Quote: "<verbatim sentence>"
- Repo(s) + location: <Repo> — `<path>::<function>` (from group-X.md)
- Idea: <2-4 sentences: what you'd do, and what it could unlock>
- Proven vs. speculative: <1-2 sentences>
- Payoff: low/med/high · Confidence: low/med/high · Effort: S/M/L
```
Rank findings within the file best-first. Aim for quality over quantity (typically 3-10 per slice; zero is acceptable if nothing fits). End with a 3-line "Near misses" list of families that almost fit.
Your final message: the output path and a one-line summary of your top finding.
