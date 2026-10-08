// game_run.h - one Maker-Breaker game on the 17x17 board, driven ply by ply.
#pragma once
#include "snaky_game.h"
#include "demo.h"

namespace snaky {

enum class MakerKind { Search, Certificate };

struct Game {
    MakerKind mk = MakerKind::Search;
    Board b;
    Breaker brk;
    SearchAI ai;
    CertPolicy cp;
    int id = 0, kind = 0;
    int maker_moves = 0, status = 0;  // 0 running, 1 Maker won, 2 Breaker held (no Breaker-free copy left)
    bool maker_turn = true;
    int last = -1;
    std::vector<int> hist;            // cells in play order (Maker on even plies)
    double maker_ms = 0;              // summed Maker decision time (pivot / search + child selection)
    int maker_decisions = 0;
    int invariant_breaks = 0;
    int finished_frame = -1;

    void start(int game_id, MakerKind m, const Certificate* C, long long ai_budget, long long brk_budget) {
        *this = Game{};
        mk = m; id = game_id; kind = game_id % BRK_COUNT;
        brk.kind = kind;
        brk.rng = Rng(0xC0FFEEull + uint64_t(game_id) * 7919ull);
        brk.ai.budget = brk_budget;
        ai.budget = ai_budget;
        if (C) cp.reset(*C);
    }

    // one ply; `timed` wraps exactly the Maker policy's work (decision + post-reply bookkeeping)
    template <class Timed>
    void ply(Timed&& timed) {
        if (status) return;
        if (maker_turn) {
            int c = -1;
            double t0 = demo::now_ms();
            timed([&] { c = (mk == MakerKind::Certificate) ? cp.decide(b.own) : ai.decide(b, true); });
            maker_ms += demo::now_ms() - t0;
            ++maker_decisions;
            if (c < 0) { status = 2; return; }
            b.play(c, 1);
            ++maker_moves;
            last = c; hist.push_back(c);
            if (b.maker_won()) status = 1;
            else if (b.free_cells == 0) status = 2;
        } else {
            int c = brk.decide(b);
            if (c < 0) { status = 2; return; }
            b.play(c, 2);
            last = c; hist.push_back(c);
            if (mk == MakerKind::Certificate) {
                double t0 = demo::now_ms();
                timed([&] { cp.observe(c); });
                maker_ms += demo::now_ms() - t0;
                if (!cp.invariant(b.own)) ++invariant_breaks;  // untimed check of def:claim
            }
            if (b.maker_dead() || b.free_cells == 0) status = 2;
        }
        maker_turn = !maker_turn;
    }
};

}  // namespace snaky
