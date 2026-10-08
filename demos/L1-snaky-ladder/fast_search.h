// fast_search.h - the WITHOUT engine of the load ladder.
//
// Same evaluation (threat potential W[m] over Breaker-free copies), same move ordering (per-cell
// threat gain, width 9) and same iterative-deepening alpha-beta with a node budget as 03-snaky's
// SearchAI, upgraded to what a game dev would ship:
//   * incremental evaluation and incremental per-cell ordering scores: play/undo touch only the
//     ~34 copies through the cell (03-snaky rescans all 1,664 copies at every node and leaf);
//   * a Zobrist transposition table (bounds + best move, mate scores ply-corrected), probed for
//     cutoffs and for move ordering; generation-stamped per decision so results do not depend on
//     which game was searched before (deterministic under a node budget);
//   * an aborted iteration still uses root moves that were fully searched before the abort;
//   * the per-game search board (SBoard) is kept incrementally between moves, so a decision does
//     not rebuild it; applying Breaker's reply to it is part of the timed Maker work;
//   * optional hard wall-clock deadline (fixed-budget quality ladder).
#pragma once
#include "demo.h"
#include "../03-snaky/snaky_game.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace snaky {

constexpr int MAXL = 1664;  // allowed Snaky copies inside the 17x17 board (checked at startup)
constexpr int64_t SW[7] = {1, 6, 36, 216, 1296, 7776, 0};

struct Zob {
    uint64_t k[3][NC];
    uint64_t side;
    Zob() {
        Rng r(0x5A0B);
        for (auto& a : k) for (auto& v : a) v = r.next();
        side = r.next();
    }
};
inline const Zob& zob() { static Zob z; return z; }

// flattened copy table (cells per copy, copies per cell) for cache-friendly incremental updates
struct FlatLines {
    int L = 0;
    int16_t cells[MAXL][6];
    int start[NC + 1];
    std::vector<int16_t> idx;
    FlatLines() {
        const Lines& ln = lines();
        L = ln.size();
        for (int l = 0; l < L && l < MAXL; ++l) for (int k = 0; k < 6; ++k) cells[l][k] = ln.cells[l][k];
        start[0] = 0;
        for (int c = 0; c < NC; ++c) {
            for (int l : ln.of_cell[c]) idx.push_back(int16_t(l));
            start[c + 1] = int(idx.size());
        }
    }
};
inline const FlatLines& flat() { static FlatLines F; return F; }

// per-copy contribution tables indexed by (Maker stones m, Breaker stones b) of the copy
struct Contrib {
    int32_t M[7][7], B[7][7], E[7][7], A[7][7];
    constexpr Contrib() : M{}, B{}, E{}, A{} {
        for (int m = 0; m < 7; ++m)
            for (int b = 0; b < 7; ++b) {
                bool act = b == 0 && m >= 1 && m <= 5;
                M[m][b] = act ? int32_t(SW[m + 1 < 6 ? m + 1 : 5] * (m + 1 == 6 ? 1000 : 1) - SW[m]) : 0;  // Maker ordering gain
                B[m][b] = act ? int32_t(SW[m] * (m == 5 ? 1000 : 1)) : 0;                                  // Breaker ordering gain
                E[m][b] = b == 0 ? int32_t(SW[m]) : 0;                                                     // evaluation
                A[m][b] = act;
            }
    }
};
inline constexpr Contrib kC{};

struct SBoard {
    uint8_t own[NC];
    uint8_t lm[MAXL], lb[MAXL];
    int32_t sc[NC][2];         // move-ordering scores [cell][0 = Maker, 1 = Breaker], maintained incrementally
    int64_t eval = 0;          // sum of W[lm] over Breaker-free copies
    int nwon = 0, live = 0, free_cells = NC, active = 0;
    uint64_t hash = 0;

    void clear() {
        const int L = flat().L;
        std::memset(own, 0, sizeof own);
        std::memset(lm, 0, sizeof lm); std::memset(lb, 0, sizeof lb);
        std::memset(sc, 0, sizeof sc);
        eval = L; nwon = 0; live = L; free_cells = NC; active = 0; hash = 0;
    }
    template <int SGN>
    void apply(int c, int who) {
        const FlatLines& F = flat();
        const int dm = who == 1 ? SGN : 0, db = who == 2 ? SGN : 0;
        for (int i = F.start[c]; i < F.start[c + 1]; ++i) {
            int l = F.idx[i];
            int m = lm[l], b = lb[l], m2 = m + dm, b2 = b + db;
            int32_t dM = kC.M[m2][b2] - kC.M[m][b], dB = kC.B[m2][b2] - kC.B[m][b];
            eval += kC.E[m2][b2] - kC.E[m][b];
            active += kC.A[m2][b2] - kC.A[m][b];
            nwon += (m2 == 6) - (m == 6);
            live += (b2 == 0) - (b == 0);
            lm[l] = uint8_t(m2); lb[l] = uint8_t(b2);
            if (dM | dB) {
                const int16_t* cl = F.cells[l];
                for (int k = 0; k < 6; ++k) { int32_t* s = sc[cl[k]]; s[0] += dM; s[1] += dB; }
            }
        }
    }
    void play(int c, int who) { own[c] = uint8_t(who); --free_cells; hash ^= zob().k[who][c]; apply<1>(c, who); }
    void undo(int c) { int who = own[c]; own[c] = 0; ++free_cells; hash ^= zob().k[who][c]; apply<-1>(c, who); }
    bool won() const { return nwon > 0; }
    bool dead() const { return live == 0; }
};

struct FastSearch {
    static constexpr int64_t WIN = 1'000'000'000;
    static constexpr int TTBITS = 16;
    struct TTE { uint64_t key = 0; int64_t val = 0; uint32_t gen = 0; int8_t depth = 0; uint8_t flag = 0; int16_t move = -1; };
    enum { EXACT = 1, LOWER = 2, UPPER = 3 };

    int width = 9, max_depth = 40;
    long long budget = 6000;   // node budget per decision (0 = unlimited)
    double deadline = 0;       // absolute now_ms() deadline (0 = none)
    long long nodes = 0, total_nodes = 0, decisions = 0;
    bool out = false;
    int last_depth = 0;
    uint32_t gen = 0;
    std::vector<TTE> tt;
    FastSearch() : tt(size_t(1) << TTBITS) {}

    int candidates(const SBoard& b, bool maker, int* mv, int k) const {
        const int side = maker ? 0 : 1;
        int64_t bs[16];
        int n = 0;
        auto consider = [&](int c, int64_t s) {
            if (n == k && (s < bs[n - 1] || (s == bs[n - 1] && c > mv[n - 1]))) return;
            int i = n < k ? n++ : k - 1;
            while (i > 0 && (bs[i - 1] < s || (bs[i - 1] == s && mv[i - 1] > c))) { bs[i] = bs[i - 1]; mv[i] = mv[i - 1]; --i; }
            bs[i] = s; mv[i] = c;
        };
        if (b.active > 0) {
            for (int c = 0; c < NC; ++c) if (!b.own[c] && b.sc[c][side] > 0) consider(c, b.sc[c][side]);
        } else {  // no live Maker stone: centre-first ordering (as in 03-snaky)
            for (int c = 0; c < NC; ++c) if (!b.own[c]) consider(c, 64 - (std::abs(c / N - 8) + std::abs(c % N - 8)));
        }
        if (n == 0) for (int c = 0; c < NC && n < k; ++c) if (!b.own[c]) mv[n++] = c;
        return n;
    }
    static int64_t to_tt(int64_t v, int ply) { return v > WIN - 1000 ? v + ply : v < -WIN + 1000 ? v - ply : v; }
    static int64_t from_tt(int64_t v, int ply) { return v > WIN - 1000 ? v - ply : v < -WIN + 1000 ? v + ply : v; }

    bool timeout() {
        if (budget && nodes > budget) return true;
        if (deadline > 0 && demo::now_ms() > deadline) return true;  // interior nodes only (~1 us each), so checking every time is cheap
        return false;
    }
    int64_t ab(SBoard& b, int depth, int64_t alpha, int64_t beta, bool maker, int ply) {
        ++nodes;
        if (b.won()) return WIN - ply;
        if (b.dead() || b.free_cells == 0) return -WIN + ply;
        if (depth == 0) return b.eval;
        if (timeout()) { out = true; return b.eval; }
        const uint64_t key = b.hash ^ (maker ? zob().side : 0);
        TTE& e = tt[key & ((1u << TTBITS) - 1)];
        int ttm = -1;
        if (e.key == key && e.gen == gen) {
            ttm = e.move;
            if (e.depth >= depth) {
                int64_t v = from_tt(e.val, ply);
                if (e.flag == EXACT || (e.flag == LOWER && v >= beta) || (e.flag == UPPER && v <= alpha)) return v;
            }
        }
        int mv[16];
        int n = candidates(b, maker, mv, width);
        if (ttm >= 0)
            for (int i = 1; i < n; ++i) if (mv[i] == ttm) { std::rotate(mv, mv + i, mv + i + 1); break; }
        const int64_t a0 = alpha, b0 = beta;
        int64_t best = maker ? -WIN * 2 : WIN * 2;
        int bm = mv[0];
        for (int i = 0; i < n; ++i) {
            b.play(mv[i], maker ? 1 : 2);
            int64_t v = ab(b, depth - 1, alpha, beta, !maker, ply + 1);
            b.undo(mv[i]);
            if (out) return best == WIN * 2 || best == -WIN * 2 ? v : best;
            if (maker ? v > best : v < best) { best = v; bm = mv[i]; }
            if (maker) alpha = std::max(alpha, best); else beta = std::min(beta, best);
            if (alpha >= beta) break;
        }
        e.key = key; e.gen = gen; e.depth = int8_t(depth); e.move = int16_t(bm);
        e.val = to_tt(best, ply);
        e.flag = best <= a0 ? UPPER : best >= b0 ? LOWER : EXACT;
        return best;
    }
    int decide(SBoard& b, bool maker) {
        nodes = 0; out = false; ++gen; ++decisions;
        int mv[16];
        int n = candidates(b, maker, mv, width);
        if (n == 0) return -1;
        int best = mv[0];
        last_depth = 0;
        for (int depth = 1; depth <= max_depth; ++depth) {
            int64_t bv = maker ? -WIN * 3 : WIN * 3;
            int bm = mv[0], done = 0;
            int64_t alpha = -WIN * 3, beta = WIN * 3;
            for (int i = 0; i < n; ++i) {
                if (deadline > 0 && demo::now_ms() > deadline) { out = true; break; }  // hard budget: may return the top-ordered move
                b.play(mv[i], maker ? 1 : 2);
                int64_t v = ab(b, depth - 1, alpha, beta, !maker, 1);
                b.undo(mv[i]);
                if (out) break;
                ++done;
                if (maker ? v > bv : v < bv) { bv = v; bm = mv[i]; }
                if (maker) alpha = std::max(alpha, bv); else beta = std::min(beta, bv);
            }
            if (out) { if (done > 0) best = bm; break; }  // fully searched root moves of the aborted depth still count
            best = bm;
            last_depth = depth;
            if (bv >= WIN - 64 || bv <= -WIN + 64) break;  // proven
            for (int i = 0; i < n; ++i) if (mv[i] == bm) { std::rotate(mv, mv + i, mv + i + 1); break; }
        }
        total_nodes += nodes;
        return best;
    }
};

}  // namespace snaky
