// snaky_game.h - 17x17 Maker-Breaker board for Snaky, a conventional game-AI Maker (baseline) and
// the Breaker opponents shared by both panels.
#pragma once
#include "snaky_cert.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace snaky {

// ---- all allowed copies t + R(S) lying inside the 17x17 board
struct Lines {
    std::vector<std::array<int16_t, 6>> cells;
    std::vector<std::vector<int>> of_cell;  // cell -> line ids
    Lines() {
        of_cell.resize(NC);
        std::vector<std::array<int16_t, 6>> seen;
        for (int s = 0; s < 8; ++s) {
            Aff r = Aff::from_code(s, 0, 0);
            int pts[6][2], mnx = 99, mny = 99;
            for (int k = 0; k < 6; ++k) {
                r.apply(SNAKE[k][0], SNAKE[k][1], pts[k][0], pts[k][1]);
                mnx = std::min(mnx, pts[k][0]); mny = std::min(mny, pts[k][1]);
            }
            for (int tx = -mnx; tx < N; ++tx)
                for (int ty = -mny; ty < N; ++ty) {
                    std::array<int16_t, 6> l;
                    bool in = true;
                    for (int k = 0; k < 6; ++k) {
                        int x = pts[k][0] + tx, y = pts[k][1] + ty;
                        if (!onb(x, y)) { in = false; break; }
                        l[k] = int16_t(cid(x, y));
                    }
                    if (!in) continue;
                    std::array<int16_t, 6> key = l;
                    std::sort(key.begin(), key.end());
                    if (std::find(seen.begin(), seen.end(), key) != seen.end()) continue;
                    seen.push_back(key);
                    cells.push_back(l);
                }
        }
        for (int i = 0; i < int(cells.size()); ++i)
            for (int c : cells[i]) of_cell[c].push_back(i);
    }
    int size() const { return int(cells.size()); }
};
inline const Lines& lines() { static Lines L; return L; }

// ---- board with incremental per-line counts
struct Board {
    uint8_t own[NC] = {};          // 0 free, 1 Maker, 2 Breaker
    std::vector<uint8_t> lm, lb;   // Maker / Breaker stones per line
    int free_cells = NC, nwon = 0, live = 0, maker_stones = 0;
    Board() {
        lm.assign(lines().size(), 0); lb.assign(lines().size(), 0);
        live = lines().size();
    }
    void play(int c, int who) {
        own[c] = uint8_t(who); --free_cells;
        for (int l : lines().of_cell[c]) {
            if (who == 1) { if (++lm[l] == 6) ++nwon; }
            else { if (lb[l]++ == 0) --live; }
        }
        if (who == 1) ++maker_stones;
    }
    void undo(int c) {
        int who = own[c];
        own[c] = 0; ++free_cells;
        for (int l : lines().of_cell[c]) {
            if (who == 1) { if (lm[l]-- == 6) --nwon; }
            else { if (--lb[l] == 0) ++live; }
        }
        if (who == 1) --maker_stones;
    }
    bool maker_won() const { return nwon > 0; }
    int won_line() const { for (int l = 0; l < int(lm.size()); ++l) if (lm[l] == 6) return l; return -1; }
    bool maker_dead() const { return live == 0; }  // no allowed copy is still Breaker-free
};

// ---- splitmix / xorshift RNG
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed = 1) : s(seed * 0x9E3779B97F4A7C15ull + 0x1234567) {}
    uint64_t next() { uint64_t z = (s += 0x9E3779B97F4A7C15ull); z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull; z = (z ^ (z >> 27)) * 0x94D049BB133111EBull; return z ^ (z >> 31); }
    int below(int n) { return int(next() % uint64_t(n)); }
};

// ======================================================================== baseline game AI
// Threat-potential evaluation + iterative-deepening alpha-beta with a fixed node budget per move
// (deterministic, so runs repeat exactly). Value of a Breaker-free copy with m Maker stones: W[m].
struct SearchAI {
    static constexpr int64_t WIN = 1'000'000'000;
    static constexpr int64_t W[7] = {1, 6, 36, 216, 1296, 7776, 0};
    int width = 9;          // candidate moves per node (ordered by potential)
    long long budget = 6000, nodes = 0;
    bool out = false;
    int last_depth = 0;
    std::vector<float> heat;  // last root move scores (for drawing)

    int64_t eval(const Board& b) const {
        const auto& L = lines();
        int64_t v = 0;
        for (int l = 0; l < L.size(); ++l) if (b.lb[l] == 0) v += W[b.lm[l]];
        return v;
    }
    // move ordering scores; for Maker: gain of claiming c, for Breaker: value destroyed by claiming c
    int candidates(const Board& b, bool maker, int* mv, int k, int64_t* scoreOut = nullptr) const {
        const auto& L = lines();
        int64_t sc[NC] = {};
        bool any = false;
        for (int l = 0; l < L.size(); ++l) {
            if (b.lb[l] || b.lm[l] == 0) continue;
            int m = b.lm[l];
            int64_t add = maker ? (W[m + 1 < 6 ? m + 1 : 5] * (m + 1 == 6 ? 1000 : 1) - W[m]) : W[m] * (m == 5 ? 1000 : 1);
            for (int c : L.cells[l]) if (!b.own[c]) { sc[c] += add; any = true; }
        }
        if (!any) {  // no Maker stone yet: centre-first ordering
            for (int c = 0; c < NC; ++c) if (!b.own[c]) sc[c] = 64 - (std::abs(c / N - 8) + std::abs(c % N - 8));
        }
        int n = 0;
        int idx[NC]; int cnt = 0;
        for (int c = 0; c < NC; ++c) if (!b.own[c] && sc[c] > 0) idx[cnt++] = c;
        int kk = std::min(k, cnt);
        std::partial_sort(idx, idx + kk, idx + cnt, [&](int a, int c) { return sc[a] != sc[c] ? sc[a] > sc[c] : a < c; });
        for (int i = 0; i < kk; ++i) mv[n++] = idx[i];
        if (n == 0) for (int c = 0; c < NC && n < k; ++c) if (!b.own[c]) mv[n++] = c;
        if (scoreOut) for (int c = 0; c < NC; ++c) scoreOut[c] = sc[c];
        return n;
    }
    // Maker-perspective minimax value; maker = side to move
    int64_t ab(Board& b, int depth, int64_t alpha, int64_t beta, bool maker, int ply) {
        ++nodes;
        if (b.maker_won()) return WIN - ply;
        if (b.maker_dead() || b.free_cells == 0) return -WIN + ply;
        if (depth == 0 || nodes > budget) { if (nodes > budget) out = true; return eval(b); }
        int mv[16];
        int n = candidates(b, maker, mv, width);
        if (maker) {
            int64_t best = -WIN * 2;
            for (int i = 0; i < n; ++i) {
                b.play(mv[i], 1);
                int64_t v = ab(b, depth - 1, alpha, beta, false, ply + 1);
                b.undo(mv[i]);
                if (v > best) best = v;
                if (best > alpha) alpha = best;
                if (alpha >= beta || out) break;
            }
            return best;
        } else {
            int64_t best = WIN * 2;
            for (int i = 0; i < n; ++i) {
                b.play(mv[i], 2);
                int64_t v = ab(b, depth - 1, alpha, beta, true, ply + 1);
                b.undo(mv[i]);
                if (v < best) best = v;
                if (best < beta) beta = best;
                if (alpha >= beta || out) break;
            }
            return best;
        }
    }
    // choose a move for `maker` side; iterative deepening until the node budget is spent
    int decide(Board& b, bool maker) {
        nodes = 0; out = false;
        int mv[16];
        int n = candidates(b, maker, mv, width);
        if (n == 0) return -1;
        int best = mv[0];
        last_depth = 0;
        for (int depth = 1; depth <= 12; ++depth) {
            int64_t bv = maker ? -WIN * 3 : WIN * 3;
            int bm = mv[0];
            int64_t alpha = -WIN * 3, beta = WIN * 3;
            for (int i = 0; i < n; ++i) {
                b.play(mv[i], maker ? 1 : 2);
                int64_t v = ab(b, depth - 1, alpha, beta, !maker, 1);
                b.undo(mv[i]);
                if (out) break;
                if (maker ? v > bv : v < bv) { bv = v; bm = mv[i]; }
                if (maker) alpha = std::max(alpha, bv); else beta = std::min(beta, bv);
            }
            if (out) break;  // keep the result of the last fully searched depth
            best = bm;
            last_depth = depth;
            if (bv >= WIN - 64 || bv <= -WIN + 64) break;  // proven
            // move the best move to the front for the next iteration
            for (int i = 0; i < n; ++i) if (mv[i] == bm) { std::swap(mv[0], mv[i]); break; }
        }
        return best;
    }
};

// ======================================================================== Breaker opponents
enum BreakerKind { BRK_RANDOM = 0, BRK_GREEDY = 1, BRK_SEARCH = 2, BRK_COUNT = 3 };
inline const char* breaker_name(int k) {
    static const char* n[] = {"random-local", "greedy ES blocker", "alpha-beta search"};
    return n[k];
}

struct Breaker {
    int kind = BRK_RANDOM;
    Rng rng{1};
    SearchAI ai;
    Breaker() { ai.budget = 6000; }

    int moves = 0;
    int decide(Board& b) {
        // seeded variety: the greedy and search Breakers open with a random cell adjacent to Maker's first stone
        if (moves++ == 0 && kind != BRK_RANDOM) return random_local(b, 1);
        if (kind == BRK_SEARCH) return ai.decide(b, false);
        if (kind == BRK_GREEDY) return greedy(b);
        return random_local(b, 2);
    }
    // Uniformly random free cell within Chebyshev distance 2 of a Maker stone.
    int random_local(const Board& b, int rad = 2) {
        int cand[NC], n = 0;
        for (int c = 0; c < NC; ++c) {
            if (b.own[c]) continue;
            int x = c / N, y = c % N;
            bool near = false;
            for (int dx = -rad; dx <= rad && !near; ++dx)
                for (int dy = -rad; dy <= rad && !near; ++dy)
                    if (onb(x + dx, y + dy) && b.own[cid(x + dx, y + dy)] == 1) near = true;
            if (near) cand[n++] = c;
        }
        if (n == 0) for (int c = 0; c < NC; ++c) if (!b.own[c]) cand[n++] = c;
        return n ? cand[rng.below(n)] : -1;
    }
    // Erdos-Selfridge style blocker: take the cell carrying the largest danger sum over live copies,
    // danger(copy) = 4^(Maker stones); a copy one move from completion is blocked first.
    int greedy(const Board& b) {
        const auto& L = lines();
        double sc[NC] = {};
        for (int l = 0; l < L.size(); ++l) {
            if (b.lb[l]) continue;
            double d = double(1u << (2 * b.lm[l]));
            if (b.lm[l] == 5) d = 1e12;
            for (int c : L.cells[l]) if (!b.own[c]) sc[c] += d;
        }
        int best = -1; double bv = -1;
        int ties[NC], nt = 0;
        for (int c = 0; c < NC; ++c) {
            if (b.own[c]) continue;
            if (sc[c] > bv + 1e-9) { bv = sc[c]; nt = 0; ties[nt++] = c; best = c; }
            else if (std::abs(sc[c] - bv) <= 1e-9) ties[nt++] = c;
        }
        if (nt > 1) best = ties[rng.below(nt)];
        return best;
    }
};

}  // namespace snaky
