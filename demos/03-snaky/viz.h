// viz.h - drawing + the video main() for 03-snaky (included at the end of main.cpp).
#pragma once

static const Color kMaker = rgb(0x6cb6ff);
static const Color kBreaker = rgb(0xffa657);
static const Color kGold = rgb(0xffd166);

constexpr int kMini = 16, kBigEvery = 3, kBigHold = 54, kMiniHold = 26;

struct Side {
    MakerKind mk = MakerKind::Search;
    const Certificate* C = nullptr;
    long long budget = 6000, bbudget = 6000;
    Game big;
    int big_hold = 0;
    Game mini[kMini];
    int hold[kMini] = {};
    int next_id = 0;
    Tally all, per[BRK_COUNT];
    long long last_win_moves = 0;
    std::vector<int> big_place_frame;  // frame at which each ply of the big game appeared (animation)

    void init(MakerKind m, const Certificate* c, long long b, long long bb) {
        mk = m; C = c; budget = b; bbudget = bb;
        big.start(next_id++, mk, C, budget, bbudget);
        for (auto& g : mini) g.start(next_id++, mk, C, budget, bbudget);
    }
    void record(const Game& g) {
        all.add(g); per[g.kind].add(g);
        if (g.status == 1) last_win_moves = g.maker_moves;
    }
    void step(Panel& p, int frame) {
        auto timed = [&](auto&& f) { p.measure(f); };  // only the Maker policy's own work is timed
        if (big.status) {
            if (++big_hold > kBigHold) { big.start(next_id++, mk, C, budget, bbudget); big_hold = 0; big_place_frame.clear(); }
        } else if (frame % kBigEvery == 0 || big.maker_moves > 21) {  // fast-forward past the bound
            big.ply(timed);
            big_place_frame.push_back(frame);
            if (big.status) record(big);
        }
        for (int i = 0; i < kMini; ++i) {
            Game& g = mini[i];
            if (g.status) {
                if (++hold[i] > kMiniHold) { g.start(next_id++, mk, C, budget, bbudget); hold[i] = 0; }
                continue;
            }
            g.ply(timed);
            if (g.status) record(g);
        }
    }
};

static inline Vec2 cell_xy(float x0, float y0, float cs, int c) {  // top-left corner of cell c (y up)
    return {x0 + (c / N) * cs, y0 + (N - 1 - c % N) * cs};
}

static void draw_win_glow(Panel& p, const Game& g, float x0, float y0, float cs, float pulse) {
    int l = g.b.won_line();
    if (l < 0) return;
    const auto& L = lines().cells[l];
    for (int c : L) {
        Vec2 q = cell_xy(x0, y0, cs, c);
        p.glow(q.x + cs / 2, q.y + cs / 2, cs * 1.25f, kGold, 0.45f + 0.35f * pulse);
    }
    for (int a = 0; a < 6; ++a)
        for (int b = a + 1; b < 6; ++b) {
            int ca = L[a], cb = L[b];
            if (std::abs(ca / N - cb / N) + std::abs(ca % N - cb % N) != 1) continue;
            Vec2 qa = cell_xy(x0, y0, cs, ca), qb = cell_xy(x0, y0, cs, cb);
            p.line(qa.x + cs / 2, qa.y + cs / 2, qb.x + cs / 2, qb.y + cs / 2, std::max(2.f, cs * 0.22f), kGold, 0.95f);
        }
}

static void draw_mini(Panel& p, const Game& g, float x0, float y0, float cs, bool cert, int frame) {
    float W = cs * N;
    Color border = g.status == 1 ? kGold : g.status == 2 ? pal::bad : pal::grid;
    p.fill_rounded_rect(x0 - 3, y0 - 3, W + 6, W + 6, 4, rgb(0x10141d), 1);
    if (cert && !g.status) {
        Bits t; g.cp.placed_T(t);
        t.each([&](int c) { Vec2 q = cell_xy(x0, y0, cs, c); p.fill_rect(q.x, q.y, cs, cs, pal::with, 0.10f); });
    }
    for (int c = 0; c < NC; ++c) {
        if (!g.b.own[c]) continue;
        Vec2 q = cell_xy(x0, y0, cs, c);
        p.fill_rect(q.x + 0.5f, q.y + 0.5f, cs - 1, cs - 1, g.b.own[c] == 1 ? kMaker : kBreaker, g.b.own[c] == 1 ? 0.95f : 0.75f);
    }
    if (g.status == 1) draw_win_glow(p, g, x0, y0, cs, 0.5f + 0.5f * std::sin(frame * 0.25f));
    p.stroke_rect(x0 - 3, y0 - 3, W + 6, W + 6, g.status ? 2.f : 1.f, border, 1);
    std::string lab = g.status == 1   ? fmt("WIN in %d", g.maker_moves)
                      : g.status == 2 ? fmt("held · %d", g.maker_moves)
                                      : fmt("move %d", g.maker_moves + (g.maker_turn ? 1 : 0));
    static const char* sh[] = {"rand", "greedy", "α-β"};
    p.text(x0 - 2, y0 + W + 5, sh[g.kind], 12.5f, pal::dim, Font::Sans);
    p.text(x0 + W + 2, y0 + W + 5, lab, 12.5f, g.status == 1 ? kGold : g.status == 2 ? pal::bad : pal::text, Font::Mono, Align::Right);
}

static void draw_big(Panel& p, const Side& s, const Certificate& C, float x0, float y0, float cs, int frame, Color accent) {
    const Game& g = s.big;
    const bool cert = s.mk == MakerKind::Certificate;
    float W = cs * N;
    p.fill_rounded_rect(x0 - 8, y0 - 8, W + 16, W + 16, 10, rgb(0x0f131b), 1);
    for (int i = 0; i <= N; ++i) {
        p.line(x0 + i * cs, y0, x0 + i * cs, y0 + W, 1, pal::grid, 0.9f);
        p.line(x0, y0 + i * cs, x0 + W, y0 + i * cs, 1, pal::grid, 0.9f);
    }
    if (!g.status) {  // what the Maker policy "sees" (drawing only, untimed)
        if (cert) {
            Bits t; g.cp.placed_T(t);
            t.each([&](int c) {
                Vec2 q = cell_xy(x0, y0, cs, c);
                p.fill_rect(q.x + 1, q.y + 1, cs - 2, cs - 2, pal::with, 0.14f);
            });
            if (g.maker_turn) {
                int pv = g.cp.placed_pivot();
                if (pv >= 0) {
                    Vec2 q = cell_xy(x0, y0, cs, pv);
                    float pu = 0.5f + 0.5f * std::sin(frame * 0.3f);
                    p.ring(q.x + cs / 2, q.y + cs / 2, cs * (0.36f + 0.08f * pu), 2.2f, pal::with, 0.95f);
                    p.glow(q.x + cs / 2, q.y + cs / 2, cs * 0.9f, pal::with, 0.35f);
                }
            }
        } else {
            SearchAI tmp;
            int mv[16];
            int64_t sc[NC];
            Board b = g.b;
            tmp.candidates(b, true, mv, 9, sc);
            int64_t mx = 1;
            for (int c = 0; c < NC; ++c) if (!g.b.own[c]) mx = std::max(mx, sc[c]);
            for (int c = 0; c < NC; ++c) {
                if (g.b.own[c] || sc[c] <= 0) continue;
                float t = std::pow(float(sc[c]) / float(mx), 0.45f);
                Vec2 q = cell_xy(x0, y0, cs, c);
                p.fill_rect(q.x + 1, q.y + 1, cs - 2, cs - 2, pal::without, 0.05f + 0.30f * t);
            }
        }
    }
    for (size_t k = 0; k < g.hist.size(); ++k) {  // stones; the newest pops in
        int c = g.hist[k];
        Vec2 q = cell_xy(x0, y0, cs, c);
        float age = k < s.big_place_frame.size() ? float(frame - s.big_place_frame[k]) : 99.f;
        float sc = std::min(1.f, 0.35f + age / 4.f);
        float cx = q.x + cs / 2, cy = q.y + cs / 2;
        if (k % 2 == 0) {
            p.glow(cx, cy, cs * 0.75f, kMaker, 0.18f);
            p.fill_rounded_rect(cx - cs * 0.40f * sc, cy - cs * 0.40f * sc, cs * 0.80f * sc, cs * 0.80f * sc, 4, kMaker, 0.95f);
        } else {
            p.circle(cx, cy, cs * 0.34f * sc, kBreaker, 0.9f);
            p.line(cx - cs * 0.15f * sc, cy - cs * 0.15f * sc, cx + cs * 0.15f * sc, cy + cs * 0.15f * sc, 2, rgb(0x2a1a0a), 0.8f);
            p.line(cx - cs * 0.15f * sc, cy + cs * 0.15f * sc, cx + cs * 0.15f * sc, cy - cs * 0.15f * sc, 2, rgb(0x2a1a0a), 0.8f);
        }
    }
    if (g.last >= 0 && !g.status) {
        Vec2 q = cell_xy(x0, y0, cs, g.last);
        p.ring(q.x + cs / 2, q.y + cs / 2, cs * 0.55f, 1.5f, pal::text, 0.75f);
    }
    if (g.status == 1) draw_win_glow(p, g, x0, y0, cs, 0.5f + 0.5f * std::sin(frame * 0.2f));
    p.stroke_rect(x0 - 8, y0 - 8, W + 16, W + 16, 1.5f, g.status == 1 ? kGold : g.status == 2 ? pal::bad : accent, g.status ? 0.95f : 0.45f);
    if (!g.status && g.maker_moves > 21) {
        TextStyle st; st.size = 15; st.font = Font::Bold; st.color = pal::bad; st.align = Align::Center;
        st.backdrop = true; st.backdrop_alpha = 0.7f; st.pad = 6;
        p.text(x0 + W / 2, y0 + W + 16, fmt("past 21 Maker moves · fast-forward ×3"), st);
    }
    if (g.status) {
        std::string msg = g.status == 1 ? fmt("MAKER COMPLETES SNAKY · %d moves", g.maker_moves) : fmt("BREAKER HOLDS · %d Maker moves", g.maker_moves);
        TextStyle st; st.size = 21; st.font = Font::Bold; st.color = g.status == 1 ? kGold : pal::bad; st.align = Align::Center;
        st.backdrop = true; st.backdrop_alpha = 0.78f; st.pad = 8;
        p.text(x0 + W / 2, y0 + W + 16, msg, st);
    }
    (void)C;
}

// moves-used bar: 21 segments = the certificate's bound; overflow past 21 drawn in red
static void draw_move_bar(Panel& p, float x, float y, float w, int used, bool cert, int height_left) {
    const int segs = 21;
    float sw = w / segs;
    for (int i = 0; i < segs; ++i) {
        Color c = pal::grid;
        float a = 1;
        if (i < used) c = kMaker;
        else if (cert && i < used + height_left) { c = pal::with; a = 0.30f; }
        p.fill_rounded_rect(x + i * sw + 1, y, sw - 2, 14, 2, c, a);
    }
    if (used > segs) {
        float ow = std::min(110.f, (used - segs) * 5.f);
        p.fill_rounded_rect(x + w + 4, y, ow, 14, 2, pal::bad, 0.9f);
        p.text(x + w + 8 + ow, y - 2, fmt("+%d", used - segs), 13, pal::bad, Font::Mono);
    }
}

int main(int argc, char** argv) {
    const std::string cert_path = arg_str(argc, argv, "--cert", kDefaultCert);
    Certificate C;
    C.load(cert_path.c_str());
    if (arg_flag(argc, argv, "--verify")) {
        print_verify(C);
        if (!C.ok) return 1;
        for (int mode = 0; mode < 3; ++mode) {
            double t0 = now_ms();
            Stress s = stress_cert(C, arg_int(argc, argv, "--stress", 20000), mode, 99 + mode);
            std::printf("stress mode %d: %lld games, %lld wins, max Maker moves %lld, invariant breaks %lld, replacements %lld (%.1f s)\n  hist:",
                        mode, s.games, s.wins, s.max_moves, s.inv, s.repl, (now_ms() - t0) / 1000);
            for (int k = 0; k < 32; ++k) if (s.hist[k]) std::printf(" %d:%lld", k, s.hist[k]);
            std::printf("\n");
        }
        return 0;
    }
    if (!C.ok) { std::printf("certificate reconstruction failed: %s\n", C.err.c_str()); return 1; }
    const long long ai_budget = arg_int(argc, argv, "--budget", 6000);
    const long long brk_budget = arg_int(argc, argv, "--brk-budget", 6000);
    if (int nb = arg_int(argc, argv, "--batch", 0)) {
        print_verify(C);
        return run_batch(C, nb, ai_budget, brk_budget, arg_str(argc, argv, "--batch-out", ""));
    }

    // untimed, before the video: certificate-aware adversary that always keeps the tallest child alive
    const int n_stress = arg_int(argc, argv, "--stress", 20000);
    Stress st = stress_cert(C, n_stress, 0, 99);

    Config cfg;
    cfg.name = "03-snaky";
    cfg.title_left = fmt("game-AI Maker: α-β + threat eval, %lld nodes/move", ai_budget);
    cfg.title_right = "certificate Maker: 728 cards, first surviving child";
    cfg.caption = "Family 187 — Snaky in 21 Maker moves · WITH = card-727 policy (strategy.tex: Thm thm:main, Cor. cor:finite-board) "
                  "rebuilt in C++ via eq:combination + eq:symmetries from certificate.txt";
    cfg.frames = 1500;
    cfg.show_speedup = false;  // frames hold different numbers of decisions; the per-decision µs metric is the fair comparison
    Harness h(argc, argv, cfg);

    Side L, R;
    L.init(MakerKind::Search, &C, ai_budget, brk_budget);
    R.init(MakerKind::Certificate, &C, ai_budget, brk_budget);
    h.left().set_compute_label("Maker decisions");
    h.right().set_compute_label("Maker decisions");
    const double mem_left = (lines().size() * (6 * 2 + 6 * 4 + 2)) / 1024.0;  // copy table + cell index + per-copy counts
    const double mem_right = C.runtime_bytes() / 1024.0;

    while (h.next_frame()) {
        const int f = h.frame();
        L.step(h.left(), f);
        R.step(h.right(), f);

        for (int side = 0; side < 2; ++side) {
            Side& S = side ? R : L;
            Panel& p = side ? h.right() : h.left();
            const bool cert = side == 1;
            const Color acc = cert ? pal::with : pal::without;
            p.clear(pal::bg);
            const float cs = 26.0f, bx = 486, by = 24;
            draw_big(p, S, C, bx, by, cs, f, acc);

            float iy = 446;
            const Game& g = S.big;
            p.text(24, iy, fmt("Featured game #%d  ·  Breaker: %s", g.id + 1, breaker_name(g.kind)), 16, pal::text, Font::Bold);
            iy += 25;
            if (cert) {
                int card = -1;
                for (int j = 727; j >= 0; --j) if (C.card_node[j] == g.cp.node) { card = j; break; }
                std::string where = card >= 0 ? fmt("card %d", card) : std::string("inline combination");
                p.text(24, iy, fmt("active: %s  ·  height ≤ %d", where.c_str(), g.status == 1 ? 0 : C.nodes[g.cp.node].h), 14.5f, pal::with, Font::Mono);
            } else {
                p.text(24, iy, fmt("last search depth %d  ·  %lld nodes/move", g.ai.last_depth, ai_budget), 14.5f, pal::without, Font::Mono);
            }
            iy += 24;
            p.text(24, iy, cert ? "Maker moves used  (teal = proven remaining bound)" : "Maker moves used  (21 segments)", 13, pal::dim);
            iy += 20;
            draw_move_bar(p, 24, iy, 300, g.maker_moves, cert, g.status ? 0 : (cert ? C.nodes[g.cp.node].h : 0));
            iy += 28;
            if (cert) {
                p.text(24, iy, "Guarantee: win in ≤ 21 vs EVERY Breaker (proved)", 15, pal::good, Font::Bold);
                p.text(24, iy + 22, fmt("stress: %lld/%lld wins vs tallest-child adversary,", st.wins, st.games), 13, pal::dim);
                p.text(24, iy + 39, fmt("each in exactly %lld moves · %lld invariant breaks", st.max_moves, st.inv), 13, pal::dim);
            } else {
                p.text(24, iy, "Guarantee: none (heuristic search)", 15, pal::warn, Font::Bold);
                p.text(24, iy + 22, "outcome depends on the Breaker and the search budget", 13, pal::dim);
            }
            // mini boards
            const float mcs = 6.0f, mx0 = 32, my0 = 642, dx = 116.5f, dy = 128;
            p.text(24, my0 - 32, fmt("%d parallel games · one ply per frame · same seeded Breakers on both sides", kMini), 13.5f, pal::dim);
            for (int i = 0; i < kMini; ++i) draw_mini(p, S.mini[i], mx0 + (i % 8) * dx, my0 + (i / 8) * dy, mcs, cert, f);
            // per-Breaker tally strip
            float tx = 24, ty = 912;
            for (int k = 0; k < BRK_COUNT; ++k) {
                const Tally& t = S.per[k];
                Color c = t.games == 0 ? pal::dim : t.wins == t.games ? pal::good : pal::bad;
                tx += p.text(tx, ty, fmt("vs %s ", breaker_name(k)), 14, pal::dim);
                tx += p.text(tx, ty, fmt("%lld/%lld", t.wins, t.games), 14, c, Font::Mono) + 18;
            }

            p.metric("Maker win rate %", S.all.winrate(), "%.1f", S.all.wins == S.all.games ? Tone::Good : Tone::Bad);
            p.metric("games finished", double(S.all.games), "%.0f");
            p.metric("max Maker moves in a win", double(S.all.max_moves_win), "%.0f", S.all.max_moves_win <= 21 ? Tone::Good : Tone::Bad);
            p.metric("Maker moves, last win", double(S.last_win_moves), "%.0f");
            p.metric("decision µs / move", S.all.us(), S.all.us() < 10 ? "%.2f" : "%.0f", cert ? Tone::Good : Tone::Warn);
            p.metric("policy data KB", cert ? mem_right : mem_left, "%.0f");
            p.sparkline("Maker moves, last win");
        }
    }
    for (int side = 0; side < 2; ++side) {
        Side& S = side ? R : L;
        Panel& p = side ? h.right() : h.left();
        p.result("games", double(S.all.games));
        p.result("wins", double(S.all.wins));
        p.result("max_maker_moves_win", double(S.all.max_moves_win));
        p.result("us_per_decision", S.all.us());
        p.result("invariant_breaks", double(S.all.inv));
        p.result("policy_data_kb", side ? mem_right : mem_left);
        for (int k = 0; k < BRK_COUNT; ++k) {
            p.result(fmt("wins_vs_%s", breaker_name(k)), double(S.per[k].wins));
            p.result(fmt("games_vs_%s", breaker_name(k)), double(S.per[k].games));
            p.result(fmt("max_moves_win_vs_%s", breaker_name(k)), double(S.per[k].max_moves_win));
        }
    }
    h.result("certificate_bytes_on_disk", double(C.text_bytes_raw));
    h.result("certificate_bytes_lf", double(C.text_bytes_lf));
    h.result("certificate_sha256_lf", C.sha_lf);
    h.result("cards", double(C.card_node.size()));
    h.result("combination_nodes", double(C.postorder.size()));
    h.result("inline_nodes", double(C.inline_nodes));
    h.result("references", double(C.references));
    h.result("reply_classes", double(C.replies));
    h.result("cards_sha256", C.cards_sha);
    h.result("cards_sha256_matches_manifest", C.cards_sha == kManifestCards ? "yes" : "no");
    h.result("postorder_sha256_matches_manifest", C.postorder_sha == kManifestPost ? "yes" : "no");
    h.result("runtime_tables_kb", mem_right);
    h.result("stress_games_tallest_child_adversary", double(st.games));
    h.result("stress_wins", double(st.wins));
    h.result("stress_max_maker_moves", double(st.max_moves));
    h.result("stress_invariant_breaks", double(st.inv));
    h.result("search_node_budget", double(ai_budget));
    h.result("breaker_search_node_budget", double(brk_budget));
    return h.finish();
}
