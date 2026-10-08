// 03-snaky - Maker-Breaker Snaky on a 17x17 board.
//   LEFT  (WITHOUT): Maker = conventional game AI (threat-potential eval + iterative-deepening
//                    alpha-beta, fixed node budget per move).
//   RIGHT (WITH):    Maker = the 21-move certificate of Family 187 (728 cards, first-surviving-child
//                    policy, build/strategy.tex), reconstructed in C++ from verification/certificate.txt.
// Both Makers face the same seeded Breakers: random-local, greedy Erdos-Selfridge blocker, alpha-beta.
#include "demo.h"
#include "game_run.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace demo;
using namespace snaky;

static const char* kDefaultCert =
    "<openai/math>/preprints/Snaky-in-21-Maker-moves-September-25-2026/verification/certificate.txt";
static const char* kManifestCards = "8d9134e82a9e75f5959ca928c20d1625d7b3929f1bb6525d32317eaadbca844a";
static const char* kManifestPost = "99a97ab2fc1be95e3ce2addeef7ce2a70787f199ea328d122158a81c00098bd1";
static const char* kManifestText = "3fa12d36a6d4dbb185e3f2808c8dfde13d85ee309afbca030d6ad17ae9fe3d04";

static void print_verify(const Certificate& C) {
    std::printf("certificate: %zu bytes on disk, %zu bytes LF, sha256(LF) %s %s\n", C.text_bytes_raw, C.text_bytes_lf,
                C.sha_lf.c_str(), C.sha_lf == kManifestText ? "MATCH" : "MISMATCH");
    if (!C.ok) { std::printf("RECONSTRUCTION FAILED: %s\n", C.err.c_str()); return; }
    const Node& f = C.nodes[C.card_node[727]];
    std::printf("cards %zu  combination nodes %zu  inline %lld  references %lld  reply classes %lld\n", C.card_node.size(),
                C.postorder.size(), C.inline_nodes, C.references, C.replies);
    std::printf("card 727: pivot (%d,%d) |A| %d |T| %d h %d ; T in {0..16}^2\n", f.px, f.py, f.A.count(), f.T.count(), f.h);
    std::printf("cards_sha256     %s %s\npostorder_sha256 %s %s\n", C.cards_sha.c_str(),
                C.cards_sha == kManifestCards ? "MATCH" : "MISMATCH", C.postorder_sha.c_str(),
                C.postorder_sha == kManifestPost ? "MATCH" : "MISMATCH");
    std::printf("runtime tables: %.1f KB\n", C.runtime_bytes() / 1024.0);
}

struct Tally {
    long long games = 0, wins = 0, max_moves_win = 0, sum_moves_win = 0, decisions = 0, inv = 0, max_moves_any = 0;
    double ms = 0;
    void add(const Game& g) {
        ++games;
        if (g.status == 1) { ++wins; max_moves_win = std::max<long long>(max_moves_win, g.maker_moves); sum_moves_win += g.maker_moves; }
        max_moves_any = std::max<long long>(max_moves_any, g.maker_moves);
        decisions += g.maker_decisions; ms += g.maker_ms; inv += g.invariant_breaks;
    }
    double winrate() const { return games ? 100.0 * wins / games : 0; }
    double us() const { return decisions ? 1000.0 * ms / decisions : 0; }
};

// ---------------------------------------------------------------- batch mode (no video)
static int run_batch(const Certificate& C, int n, long long ai_budget, long long brk_budget, const std::string& out) {
    std::string js = "{\n";
    for (int side = 0; side < 2; ++side) {
        MakerKind mk = side ? MakerKind::Certificate : MakerKind::Search;
        Tally all, per[BRK_COUNT];
        double t0 = now_ms();
        for (int i = 0; i < n * BRK_COUNT; ++i) {
            Game g;
            g.start(i, mk, &C, ai_budget, brk_budget);
            while (!g.status) g.ply([](auto&& f) { f(); });
            all.add(g); per[g.kind].add(g);
        }
        double wall = now_ms() - t0;
        std::printf("%s Maker: %lld games, wins %lld (%.1f%%), max Maker moves in a win %lld, mean %.2f, %.2f us/decision, invariant breaks %lld (%.1f s)\n",
                    side ? "CERT  " : "SEARCH", all.games, all.wins, all.winrate(), all.max_moves_win,
                    all.wins ? double(all.sum_moves_win) / all.wins : 0.0, all.us(), all.inv, wall / 1000);
        js += fmt("  \"%s\": {\"games\": %lld, \"wins\": %lld, \"win_pct\": %.2f, \"max_maker_moves_win\": %lld, \"mean_maker_moves_win\": %.3f, \"us_per_decision\": %.3f, \"invariant_breaks\": %lld, \"by_breaker\": {",
                  side ? "certificate" : "search", all.games, all.wins, all.winrate(), all.max_moves_win,
                  all.wins ? double(all.sum_moves_win) / all.wins : 0.0, all.us(), all.inv);
        for (int k = 0; k < BRK_COUNT; ++k) {
            std::printf("   vs %-18s %4lld/%4lld wins  max %lld  mean %.2f moves  %.2f us/decision\n", breaker_name(k), per[k].wins,
                        per[k].games, per[k].max_moves_win, per[k].wins ? double(per[k].sum_moves_win) / per[k].wins : 0.0, per[k].us());
            js += fmt("%s\"%s\": {\"games\": %lld, \"wins\": %lld, \"max_maker_moves_win\": %lld, \"mean_maker_moves_win\": %.3f, \"max_maker_moves_any\": %lld}",
                      k ? ", " : "", breaker_name(k), per[k].games, per[k].wins, per[k].max_moves_win,
                      per[k].wins ? double(per[k].sum_moves_win) / per[k].wins : 0.0, per[k].max_moves_any);
        }
        js += "}},\n";
    }
    js += fmt("  \"games_per_breaker\": %d, \"search_node_budget\": %lld, \"breaker_search_node_budget\": %lld\n}\n", n, ai_budget, brk_budget);
    if (!out.empty()) {
        if (FILE* f = std::fopen(out.c_str(), "wb")) { std::fputs(js.c_str(), f); std::fclose(f); }
    }
    return 0;
}

// ---------------------------------------------------------------- certificate stress test (certificate-aware adversaries)
struct Stress { long long games = 0, wins = 0, max_moves = 0, inv = 0, repl = 0; long long hist[32] = {}; };
static Stress stress_cert(const Certificate& C, int n, int mode, uint64_t seed) {
    // mode 0: reply that keeps the TALLEST surviving child (ties random) - tries to drag the game out
    // mode 1: uniform random cell inside the currently placed envelope
    // mode 2: uniform random free cell on the board
    Stress s;
    Rng rng(seed);
    for (int gi = 0; gi < n; ++gi) {
        Board b;
        CertPolicy cp; cp.reset(C);
        int moves = 0;
        bool won = false;
        while (true) {
            int c = cp.decide(b.own);
            b.play(c, 1); ++moves;
            if (b.maker_won()) { won = true; break; }
            if (moves > 30) break;
            int cand[NC], nc = 0, bestH = -1;
            if (mode == 0) {
                for (int q = 0; q < NC; ++q) {
                    if (b.own[q]) continue;
                    int h = cp.surviving_height(q);
                    if (h > bestH) { bestH = h; nc = 0; }
                    if (h == bestH) cand[nc++] = q;
                }
            } else if (mode == 1) {
                Bits t; cp.placed_T(t);
                t.each([&](int q) { if (!b.own[q]) cand[nc++] = q; });
                if (!nc) for (int q = 0; q < NC; ++q) if (!b.own[q]) cand[nc++] = q;
            } else {
                for (int q = 0; q < NC; ++q) if (!b.own[q]) cand[nc++] = q;
            }
            int r = cand[rng.below(nc)];
            b.play(r, 2);
            cp.observe(r);
            if (!cp.invariant(b.own)) ++s.inv;
        }
        ++s.games;
        if (won) ++s.wins;
        s.max_moves = std::max<long long>(s.max_moves, moves);
        s.hist[std::min(moves, 31)]++;
        s.repl += cp.replacements;
        s.inv += cp.invariant_breaks;
    }
    return s;
}

#include "viz.h"

