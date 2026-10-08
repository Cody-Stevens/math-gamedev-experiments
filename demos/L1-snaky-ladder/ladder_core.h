// ladder_core.h - N concurrent Maker-Breaker Snaky games per side; one Maker decision per game per frame.
#pragma once
#include "fast_search.h"
#include "../03-snaky/game_run.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace snaky {

// ---------------------------------------------------------------- tiny parallel-for (untimed work only)
template <class F>
inline void pfor(int n, int threads, F&& f) {
    int T = std::min(threads, n / 32);
    if (T <= 1) { for (int i = 0; i < n; ++i) f(i); return; }
    std::vector<std::thread> ts;
    ts.reserve(T);
    for (int t = 0; t < T; ++t) {
        int a = int(int64_t(n) * t / T), b = int(int64_t(n) * (t + 1) / T);
        ts.emplace_back([&f, a, b] { for (int i = a; i < b; ++i) f(i); });
    }
    for (auto& th : ts) th.join();
}

inline uint64_t game_seed(int slot, int gen) { return 0xC0FFEEull + (uint64_t(slot) * 1000003ull + uint64_t(gen)) * 7919ull; }

inline void reset_board(Board& b) {
    std::memset(b.own, 0, sizeof b.own);
    std::fill(b.lm.begin(), b.lm.end(), 0);
    std::fill(b.lb.begin(), b.lb.end(), 0);
    b.free_cells = NC; b.nwon = 0; b.live = int(b.lm.size()); b.maker_stones = 0;
}

struct LGame {
    Board b;
    Breaker brk;
    CertPolicy cp;
    std::unique_ptr<SBoard> sb;     // WITHOUT engine's incremental board (search side only)
    int slot = 0, gen = 0, kind = 0;
    int maker_moves = 0;
    int next = -1, pending = -1;    // Maker's chosen cell this frame; Breaker reply not yet seen by the Maker policy
    // results of a game that just ended (tallied sequentially after the parallel phase)
    int8_t res_status = 0; int res_moves = 0;
    int inv_breaks = 0;
    // display only
    int flash = 0; int8_t flash_status = 0; int16_t flash_line[6] = {};
    std::vector<uint8_t> snap;      // final position of the game that just ended (video only)
    std::vector<int16_t> hist;
};

struct Side {
    bool cert = false;
    const Certificate* C = nullptr;
    FastSearch eng;
    std::vector<int> kinds{BRK_RANDOM, BRK_GREEDY};
    std::vector<std::unique_ptr<LGame>> g;
    bool keep_hist = true;
    // running totals
    long long finished = 0, wins = 0, held = 0, max_moves_win = 0, sum_moves_win = 0, decisions = 0, inv = 0;
    // final position of the last finished game in slot 0 (display only)
    uint8_t f0_own[NC] = {};
    std::vector<int16_t> f0_hist;
    int f0_status = 0, f0_moves = 0, f0_kind = 0, f0_line[6] = {-1, -1, -1, -1, -1, -1};
    long long f0_seq = 0;

    void start(LGame& x, int slot, int gen) {
        x.slot = slot; x.gen = gen; x.kind = kinds[slot % kinds.size()];
        reset_board(x.b);
        x.brk = Breaker{};
        x.brk.kind = x.kind;
        x.brk.rng = Rng(game_seed(slot, gen));
        x.brk.ai.budget = 6000;
        if (cert) x.cp.reset(*C);
        else { if (!x.sb) x.sb = std::make_unique<SBoard>(); x.sb->clear(); }
        x.maker_moves = 0; x.next = -1; x.pending = -1;
        x.hist.clear();
    }
    void resize(int n) {
        int old = int(g.size());
        g.resize(n);
        for (int i = old; i < n; ++i) { g[i] = std::make_unique<LGame>(); start(*g[i], i, 0); }
    }
    void clear() { g.clear(); finished = wins = held = max_moves_win = sum_moves_win = decisions = inv = 0; }
    int size() const { return int(g.size()); }

    // ---- TIMED: the Maker policy's work for one frame = absorb Breaker's last reply + choose a cell, for every game
    void timed() {
        const int n = size();
        if (cert) {
            for (int i = 0; i < n; ++i) {
                LGame& x = *g[i];
                if (x.pending >= 0) x.cp.observe(x.pending);
                x.next = x.cp.decide(x.b.own);
            }
        } else {
            for (int i = 0; i < n; ++i) {
                LGame& x = *g[i];
                SBoard& s = *x.sb;
                if (x.pending >= 0) s.play(x.pending, 2);
                x.next = eng.decide(s, true);
                if (x.next >= 0) s.play(x.next, 1);
            }
        }
        decisions += n;
    }
    // ---- UNTIMED: referee + Breaker replies + restarts (parallel)
    void finish(LGame& x, int status) {
        x.res_status = int8_t(status); x.res_moves = x.maker_moves;
        x.flash = 6; x.flash_status = int8_t(status);
        if (keep_hist) x.snap.assign(x.b.own, x.b.own + NC);
        if (status == 1) { int l = x.b.won_line(); for (int k = 0; k < 6; ++k) x.flash_line[k] = l >= 0 ? lines().cells[l][k] : -1; }
        if (x.slot == 0) {
            std::memcpy(f0_own, x.b.own, sizeof f0_own);
            f0_hist = x.hist; f0_status = status; f0_moves = x.maker_moves; f0_kind = x.kind;
            for (int k = 0; k < 6; ++k) f0_line[k] = status == 1 ? x.flash_line[k] : -1;
            ++f0_seq;
        }
        start(x, x.slot, x.gen + 1);
    }
    void untimed_one(LGame& x) {
        if (x.flash > 0) --x.flash;
        if (cert && x.pending >= 0 && !x.cp.invariant(x.b.own)) ++x.inv_breaks;  // def:claim check, untimed
        x.pending = -1;
        int c = x.next; x.next = -1;
        if (c < 0 || x.b.own[c]) { finish(x, 2); return; }
        x.b.play(c, 1); ++x.maker_moves;
        if (keep_hist) x.hist.push_back(int16_t(c));
        if (x.b.maker_won()) { finish(x, 1); return; }
        if (x.b.free_cells == 0) { finish(x, 2); return; }
        int r = x.brk.decide(x.b);
        if (r < 0) { finish(x, 2); return; }
        x.b.play(r, 2);
        if (keep_hist) x.hist.push_back(int16_t(r));
        x.pending = r;
        if (x.b.maker_dead() || x.b.free_cells == 0) { finish(x, 2); return; }
    }
    void untimed(int threads) {
        pfor(size(), threads, [&](int i) { untimed_one(*g[i]); });
        for (auto& p : g) {
            LGame& x = *p;
            if (x.inv_breaks) { inv += x.inv_breaks; x.inv_breaks = 0; }
            if (!x.res_status) continue;
            ++finished;
            if (x.res_status == 1) { ++wins; max_moves_win = std::max<long long>(max_moves_win, x.res_moves); sum_moves_win += x.res_moves; }
            else ++held;
            x.res_status = 0;
        }
    }
};

// ---------------------------------------------------------------- single games (engine check + quality ladder)
enum class MK { Orig, Fast, Cert };
struct OneResult { int status = 0, moves = 0, decisions = 0; double maker_ms = 0; long long nodes = 0; };

// time_ms > 0: hard per-move wall-clock budget for the Fast engine; node_budget used otherwise.
inline OneResult play_one(MK mk, int kind, uint64_t seed, const Certificate& C, long long node_budget, double time_ms,
                          FastSearch* eng, long long brk_budget = 6000) {
    OneResult R;
    Board b;
    Breaker brk; brk.kind = kind; brk.rng = Rng(seed); brk.ai.budget = brk_budget;
    CertPolicy cp; cp.reset(C);
    SearchAI orig; orig.budget = node_budget;
    std::unique_ptr<SBoard> sb;
    if (mk == MK::Fast) { sb = std::make_unique<SBoard>(); sb->clear(); }
    int reply = -1;
    while (true) {
        int c = -1;
        double t0 = demo::now_ms();
        if (mk == MK::Cert) {
            if (reply >= 0) cp.observe(reply);
            c = cp.decide(b.own);
        } else if (mk == MK::Orig) {
            c = orig.decide(b, true);
            R.nodes += orig.nodes;
        } else {
            if (reply >= 0) sb->play(reply, 2);
            if (time_ms > 0) { eng->budget = 0; eng->deadline = t0 + time_ms; }
            else { eng->budget = node_budget; eng->deadline = 0; }
            c = eng->decide(*sb, true);
            if (c >= 0) sb->play(c, 1);
            R.nodes += eng->nodes;
        }
        R.maker_ms += demo::now_ms() - t0;
        ++R.decisions;
        if (c < 0 || b.own[c]) { R.status = 2; break; }
        b.play(c, 1); ++R.moves;
        if (b.maker_won()) { R.status = 1; break; }
        if (b.free_cells == 0) { R.status = 2; break; }
        reply = brk.decide(b);
        if (reply < 0) { R.status = 2; break; }
        b.play(reply, 2);
        if (b.maker_dead() || b.free_cells == 0) { R.status = 2; break; }
    }
    return R;
}

}  // namespace snaky
