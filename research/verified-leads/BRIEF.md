# Adversarial verification brief: games / sim / visualization leads

GPT agents produced speculative game-dev / simulation / visualization leads from new math papers (<openai\math>\preprints\...). Files: <repo>\games\slice-NN.md, findings headed "## G<N>.<k>". The brief they followed is <repo>\GAMES_BRIEF.md.
A mechanical check (<repo>\verify-games\mech.json) already tested that cited paper paths exist and quotes appear verbatim. Your job is the semantic check: catch hallucination and overreach. Speculation is allowed and expected; misrepresentation is not.

For each assigned finding:
1. PAPER: open the cited .tex (build/ dir) or abstract in CONTENTS.md. Does the paper actually prove what the "Proven" line says (hypotheses, constants, whether there is an explicit algorithm/construction vs existence only, whether constants are galactic)?
2. BRIDGE: is the game/sim/visual use coherent, or a word-association stretch? Is the new result actually NEEDED for the idea, or would a classical/known method already give the same thing (say which)? This matters: the next phase records "without vs with the new math" demos, so the "with" must genuinely depend on this paper's content.
3. DEMO: is the proposed demo implementable in a few hundred lines of C/C++ from what the paper gives, and is the "without" baseline fair? Suggest a better demo or metric if you see one.

Verdict per finding: SOLID (accurate + coherent, the new result genuinely matters), STRETCH (accurate facts but weak bridge, or classical math would do the same), OVERSTATED (some factual claim about the paper is wrong/inflated — say which), HALLUCINATED (core claim false).
Also give: a one-line plain-English pitch (what a game/sim developer would get), demo-worthiness (low/med/high), and what precisely the demo should implement from the paper (formula/algorithm/construction, with section or equation reference).

READ-ONLY. Write results to the output path below as markdown, one block per finding:
### G<N>.<k> — <verdict>
- Pitch: ...
- Paper check: ...
- Bridge / novelty: ...
- Demo: <worthiness> — <what to implement, baseline, metric, paper section ref>
Final message: one line per finding with ID + verdict + demo-worthiness.
