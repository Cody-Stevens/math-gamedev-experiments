> **Exploratory test, not peer-reviewed or independently verified.** See the [disclaimer](../../README.md).

# Tavern of a Thousand Tables

**Scenario.** It's a cozy tavern seen from above. Every table plays *Snaky*, a Maker-Breaker game on a 17×17 board. Maker wins by claiming the 6-cell shape {(0,0),(1,0),(2,0),(3,0),(3,1),(4,1)} in any rotation, reflection or position. Breaker blocks one cell per turn. The left hall's Makers search; the right hall's Makers read a playbook. Table *i* in both halls faces the same seeded Breaker: random-local, the greedy Erdős–Selfridge blocker, or the α-β Breaker. A slider opens 1 to 4,096 tables. You can also sit at table 1 in either hall as the Breaker.

**PRIOR BEST: α-β search (TT + threat ordering).** This is a JS port of `demos/L1-snaky-ladder/fast_search.h`: iterative-deepening α-β, a Zobrist transposition table, incremental threat evaluation, and width-9 threat ordering. It is the engine a game developer would ship. At 6000 nodes per move it beats every Breaker here. The load model is the C++ ladder's: each table moves once per frame, and all tables share one 16.7 ms frame. Each move gets 16.7/N ms. The browser clock only ticks in roughly 0.1 ms steps, so the engine converts its share into a node allowance using the speed it measures, and also checks the clock as a deadline. "Out of time" means the move stopped before a full 6000-node search. The α-β Breaker is a port of `demos/03-snaky/snaky_game.h` SearchAI at 6000 nodes, the same Breaker the C++ ladder used.

**NEW: proof-as-playbook (21-move certificate).** This is `CertPolicy` from `panel/web/snaky.js` (a port of `demos/03-snaky/snaky_cert.h`), with `snaky_cert.json` inlined. Each Breaker reply costs one table step, and there is no search.
- *Proved* (preprint *Snaky in 21 Maker moves*, Thm `thm:main`, Cor. `cor:finite-board`): this Maker wins within 21 moves against every Breaker on this board.
- *Measured here*: all times, all win rates, and the 0 invariant breaks.
- *Not claimed*: anything about other shapes, other boards, or the Breaker's side.

**Node bench** (`node bench.js`, Node 22, same `core.js`; Maker time only, Breakers untimed; full log in `bench.log`/`bench.json`):

| tables | PRIOR 6000-node ms/frame | PRIOR budgeted ms/frame (short of full search) | PRIOR win vs greedy / vs α-β (60+60 games) | NEW ms/frame | NEW win |
|---|---|---|---|---|---|
| 1 | 6.6 | 16.7 (0%) | 100% / 100% | 0.0007 | 100% |
| 4 | 26.4 | 13.6 (76%) | 100% / 100% | 0.0004 | 100% |
| 16 | 111.7 | 14.9 (82%) | 100% / 55% | 0.0016 | 100% |
| 64 | 462.2 | 16.4 (87%) | 100% / 42% | 0.0061 | 100% |
| 256 | not timed | 16.7 (98%) | 88% / 3% | 0.026 | 100% |
| 1,024 | not timed | 16.8 (95%) | 100% / 0% | 0.13 | 100% |
| 4,096 | not timed | 17.1 (100%) | 97% / 12% | 0.66 | 100% |
| 16,384 | not timed | 21.1 (100%) | - | 3.23 | - |

A full-strength search move costs 5.6 ms, so 3 tables fit in one frame. The playbook costs 0.12 to 0.25 µs per move and timed all 16,384 tables in 3.2 ms (about 85k tables by linear estimate). The playbook's longest win took 13 moves (the proved bound is 21).

**Browser** (Chrome headless, `?frames=N`; live counts of *finished* games, which favour short games):

| preset | PRIOR ms/frame | PRIOR won | NEW ms/frame | NEW won |
|---|---|---|---|---|
| `one` (1 table, greedy) | 16.7 | 13/13, avg 13.8 moves | 0.0001 | 14/14, avg 12.7 |
| `busy` (256, greedy) | 16.7 | 700/747 (94%), avg 26.8, max 88 | 0.040 | 2292/2292, max 13 |
| `crowd` (4,096, greedy) | 20.7 (over budget with no lookahead) | 21269/21273 | 2.70 | 31262/31262 |
| `duel` (16, α-β Breaker) | 16.7 | 37/50 (74%; bench: 55%) | 0.0024 | 300/300 |
| `nobudget` (16, 6000 nodes) | 119.2 | 72/72 | 0.0025 | 60/60 |

The reference panel quotes `demos\L1-snaky-ladder\out\ladder.json` (C++, labelled as C++ measurements). There, search fits the 16.7 ms frame up to 4 tables and the playbook up to 131,072.

**Verdict.** With a few tables the search is affordable and wins too. Against the greedy blocker it keeps winning even with almost no thinking time; it just takes more moves. Once each move gets about 1 ms or less, a starved search loses to the α-β Breaker (55% at 16 tables, 0 to 12% from 256 tables). The playbook keeps winning in at most 13 moves at a few tenths of a µs per move. But the playbook exists only for this one game and only for the Maker's side. Memory is not a win either: it ships 265 KB of data, while the search needs only 1,664 shape positions plus a transposition table. If tables move only every 15 or 60 frames, each search gets 15 to 60 times more time.

**Reproduce** (Git Bash, in this folder): `python build.py` (inlines `core.js`, `app.js`, the certificate and the C++ ladder numbers into `index.html`), `node bench.js` (about 4 min; `--quick` for about 1.5 min). Presets: `?scene=one|busy|crowd|duel|nobudget|play`, plus `&frames=N` to simulate N frames and stop. Screenshot:
`"C:/Program Files/Google/Chrome/Application/chrome.exe" --headless=new --hide-scrollbars --window-size=1280,1000 --virtual-time-budget=15000 --user-data-dir="$TEMP/2-tavern-tables-chrome" --screenshot="<repo>/games/2-tavern-tables/shot.png" "file:///<repo>/games/2-tavern-tables/index.html?frames=240&scene=busy"`
