# 03-snaky: Maker–Breaker Snaky, game-AI Maker vs the 21-move certificate

**What you see.** Both panels play Maker on the 17×17 square board, where the Snaky target is `{(0,0),(1,0),(2,0),(3,0),(3,1),(4,1)}` in any rotation or reflection. Each panel shows one featured game on a large board and 16 parallel games on mini boards. Both sides face the same seeded Breakers: random-local, a greedy Erdős–Selfridge blocker, and an α-β search Breaker. A completed Snaky glows gold. The HUD reports win rate, the most Maker moves in any win, measured decision µs per move, and policy data size.
**WITHOUT:** a conventional game AI. It scores threats and runs iterative-deepening α-β with 6000 nodes per move, the same engine and budget as the α-β Breaker. **WITH:** the certificate policy from Family 187, *Snaky in 21 Maker moves* (`preprints/Snaky-in-21-Maker-moves-September-25-2026/`). `snaky_cert.h` parses `verification/certificate.txt` and rebuilds all 728 cards using `eq:bases`, `eq:combination` and `eq:symmetries` (`build/templates.tex`, `build/certificate-proof.tex`). It then plays the policy from the proof of Thm `thm:main` and Cor. `cor:finite-board` (`build/strategy.tex`): claim the placed pivot, then after Breaker's reply move to the first child whose placed envelope avoids that reply (a reference child composes its placement as F∘G). The teal area on the right board is that live envelope.
**Reconstruction check** (`demo.exe --verify`, see `out/verify.txt`): 728 cards, 1,620 combination nodes, 898 inline nodes, 4,089 references and 37,042 reply classes. The final card has pivot (8,8), A=∅, |T|=251, h=21, and T⊂{0..16}². SHA-256 of all reconstructed cards and of the postorder nodes match `data-manifest.json` byte for byte, as does the SHA-256 of the LF-normalized text.

| seeded batch, 100 games per Breaker (`out/batch.json`) | WITHOUT (α-β Maker) | WITH (certificate) |
|---|---|---|
| Maker wins: overall / vs α-β Breaker | 241/300 (80.3%) / **41/100** | **300/300 / 100/100** |
| max Maker moves in a win | 20 (no bound) | 13 (proved ≤ 21) |
| decision time per Maker move | ≈ 4.7 ms | ≈ 0.5–1.2 µs |
| policy data | 62 KB copy tables | 38 KB text → 94 KB runtime tables |

Clean `--novideo` run (`out/bench.json`, Maker compute only, median): LEFT 26.5 ms/frame (≈38 fps), RIGHT 0.0034 ms/frame.

**Caveats.**
- The guarantee is the paper's theorem: a win within 21 Maker moves against *every* legal Breaker, in the weak (Maker–Breaker) game, on the empty infinite board or the 17×17 board. 21 is an upper bound, not an optimal move count, and nothing is claimed for the strong game. The board is the square grid, not a hex grid.
- Sample wins are not the proof. They only show that the port behaves as specified: 0 invariant breaks of `def:claim` in every game. A certificate-aware adversary that always keeps the tallest child alive forced exactly 21 moves in all 20,000 games, so the bound is attained by this policy. The shared Breakers never pushed it past 13. No replacement claim (pivot already owned) ever occurred.
- Baseline results depend on the budget and do not improve steadily with it. A 30-games-per-Breaker sweep gave these win rates: 93% at 1000 nodes, 100% at 2000 (but up to 54 moves), 74% at 4000 and 6000, and 100% at 12000 (up to 62 moves). A stronger search could do better, but it gets no guarantee.
- The live win rate in the video counts only games that have finished, so it is biased toward short games. Use the batch numbers. The certificate's µs per move includes timer overhead. Memory is not a win: the certificate tables are larger than the baseline's.
