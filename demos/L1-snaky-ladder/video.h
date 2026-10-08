// video.h - drawing + main() for L1-snaky-ladder (included at the end of main.cpp).
#pragma once

static const Color kMaker = rgb(0x6cb6ff);
static const Color kBreaker = rgb(0xffa657);
static const Color kGold = rgb(0xffd166);

// ---------------------------------------------------------------- quality numbers from a previous --sweep
struct QRow { int n; double s, c; int games; };
static std::vector<QRow> load_quality(const std::string& path) {
    std::vector<QRow> v;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return v;
    char line[8192];
    while (std::fgets(line, sizeof line, f)) {
        const char* p = std::strstr(line, "{\"n\": ");
        const char* q = std::strstr(line, "\"quality\": {");
        if (!p || !q) continue;
        QRow r{};
        r.n = std::atoi(p + 6);
        const char* a = std::strstr(q, "\"without\": ");
        const char* b = std::strstr(q, "\"with\": ");
        const char* g = std::strstr(q, "\"games\": ");
        if (!a || !b || !g) continue;
        r.s = std::atof(a + 11); r.c = std::atof(b + 8); r.games = std::atoi(g + 9);
        v.push_back(r);
    }
    std::fclose(f);
    return v;
}

// ---------------------------------------------------------------- featured board (game slot 0)
struct Featured {
    int hold = 0;
    uint8_t own[NC] = {};
    std::vector<int16_t> hist;
    int status = 0, moves = 0, kind = 0, gen = 0;
    int line[6] = {-1, -1, -1, -1, -1, -1};
};

static inline Vec2 cell_xy(float x0, float y0, float cs, int c) { return {x0 + (c / N) * cs, y0 + (N - 1 - c % N) * cs}; }

static void draw_line_glow(Canvas& p, const int* L, float x0, float y0, float cs, float pulse) {
    if (L[0] < 0) return;
    for (int k = 0; k < 6; ++k) {
        Vec2 q = cell_xy(x0, y0, cs, L[k]);
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

static void draw_featured(Panel& p, const Side& S, const Featured& F, float x0, float y0, float cs, int frame) {
    const bool cert = S.cert;
    const Color accent = cert ? pal::with : pal::without;
    const LGame& g = *S.g[0];
    const bool fin = F.hold > 0;
    const uint8_t* own = fin ? F.own : g.b.own;
    const std::vector<int16_t>& hist = fin ? F.hist : g.hist;
    float W = cs * N;
    p.fill_rounded_rect(x0 - 8, y0 - 8, W + 16, W + 16, 10, rgb(0x0f131b), 1);
    for (int i = 0; i <= N; ++i) {
        p.line(x0 + i * cs, y0, x0 + i * cs, y0 + W, 1, pal::grid, 0.9f);
        p.line(x0, y0 + i * cs, x0 + W, y0 + i * cs, 1, pal::grid, 0.9f);
    }
    if (!fin) {  // what the Maker policy sees (drawing only)
        if (cert) {
            Bits t; g.cp.placed_T(t);
            t.each([&](int c) { Vec2 q = cell_xy(x0, y0, cs, c); p.fill_rect(q.x + 1, q.y + 1, cs - 2, cs - 2, pal::with, 0.15f); });
            int pv = g.cp.placed_pivot();
            if (pv >= 0 && !g.b.own[pv]) {
                Vec2 q = cell_xy(x0, y0, cs, pv);
                float pu = 0.5f + 0.5f * std::sin(frame * 0.3f);
                p.ring(q.x + cs / 2, q.y + cs / 2, cs * (0.36f + 0.08f * pu), 2.2f, pal::with, 0.95f);
            }
        } else if (g.sb) {
            int64_t mx = 1;
            for (int c = 0; c < NC; ++c) if (!g.sb->own[c]) mx = std::max<int64_t>(mx, g.sb->sc[c][0]);
            for (int c = 0; c < NC; ++c) {
                if (g.sb->own[c] || g.sb->sc[c][0] <= 0) continue;
                float t = std::pow(float(g.sb->sc[c][0]) / float(mx), 0.45f);
                Vec2 q = cell_xy(x0, y0, cs, c);
                p.fill_rect(q.x + 1, q.y + 1, cs - 2, cs - 2, pal::without, 0.05f + 0.30f * t);
            }
        }
    }
    for (size_t k = 0; k < hist.size(); ++k) {
        int c = hist[k];
        if (!own[c]) continue;
        Vec2 q = cell_xy(x0, y0, cs, c);
        float cx = q.x + cs / 2, cy = q.y + cs / 2;
        if (own[c] == 1) {
            p.glow(cx, cy, cs * 0.75f, kMaker, 0.18f);
            p.fill_rounded_rect(cx - cs * 0.40f, cy - cs * 0.40f, cs * 0.80f, cs * 0.80f, 4, kMaker, 0.95f);
        } else {
            p.circle(cx, cy, cs * 0.34f, kBreaker, 0.9f);
            p.line(cx - cs * 0.15f, cy - cs * 0.15f, cx + cs * 0.15f, cy + cs * 0.15f, 2, rgb(0x2a1a0a), 0.8f);
            p.line(cx - cs * 0.15f, cy + cs * 0.15f, cx + cs * 0.15f, cy - cs * 0.15f, 2, rgb(0x2a1a0a), 0.8f);
        }
    }
    if (!fin && !hist.empty()) {
        Vec2 q = cell_xy(x0, y0, cs, hist.back());
        p.ring(q.x + cs / 2, q.y + cs / 2, cs * 0.55f, 1.5f, pal::text, 0.75f);
    }
    if (fin && F.status == 1) draw_line_glow(p, F.line, x0, y0, cs, 0.5f + 0.5f * std::sin(frame * 0.2f));
    p.stroke_rect(x0 - 8, y0 - 8, W + 16, W + 16, 1.5f, fin ? (F.status == 1 ? kGold : pal::bad) : accent, fin ? 0.95f : 0.45f);
    TextStyle st; st.size = 15; st.font = Font::Bold; st.align = Align::Center; st.backdrop = true; st.backdrop_alpha = 0.75f; st.pad = 5;
    std::string msg;
    if (fin) {
        msg = F.status == 1 ? fmt("game 1 · MAKER COMPLETES SNAKY in %d moves", F.moves) : fmt("game 1 · BREAKER HOLDS after %d Maker moves", F.moves);
        st.color = F.status == 1 ? kGold : pal::bad;
    } else {
        msg = fmt("game 1 (live) · vs %s · Maker move %d", breaker_name(g.kind), g.maker_moves + 1);
        st.color = pal::text; st.font = Font::Sans;
    }
    p.text(x0 + W / 2, y0 + W + 14, msg, st);
}

// ---------------------------------------------------------------- grid of all N games (pixel buffer)
struct GridGeom { int p = 0, cp = 0, cols = 0, rows = 0, ox = 0, oy = 0; bool boards = false; };
static GridGeom grid_geom(int n, int W, int H) {
    GridGeom G;
    int p = 116;
    while (p > 2 && (W / p) * (H / p) < n) --p;
    G.p = p;
    G.cols = std::min(n, W / p);
    G.rows = (n + G.cols - 1) / G.cols;
    G.cols = (n + G.rows - 1) / G.rows;  // balance the rows
    G.boards = p >= 17;
    int gap = p >= 60 ? 4 : p >= 18 ? 1 : 0;
    G.cp = G.boards ? std::max(1, (p - gap) / 17) : 0;
    G.ox = (W - G.cols * p) / 2;
    G.oy = 0;
    return G;
}

struct RGB8 { uint8_t r, g, b; };
static inline RGB8 c8(Color c) { auto f = [](float v) { return uint8_t(std::clamp(v, 0.f, 1.f) * 255.f + 0.5f); }; return {f(c.r), f(c.g), f(c.b)}; }

static void render_grid(const Side& S, std::vector<uint8_t>& buf, int W, int H, const GridGeom& G) {
    buf.assign(size_t(W) * H * 3, 0);
    const RGB8 bg = c8(pal::bg);
    for (size_t i = 0; i < buf.size(); i += 3) { buf[i] = bg.r; buf[i + 1] = bg.g; buf[i + 2] = bg.b; }
    auto put = [&](int x, int y, RGB8 c) {
        if (x < 0 || y < 0 || x >= W || y >= H) return;
        uint8_t* q = &buf[(size_t(y) * W + x) * 3]; q[0] = c.r; q[1] = c.g; q[2] = c.b;
    };
    auto rect = [&](int x, int y, int w, int h, RGB8 c) { for (int j = 0; j < h; ++j) for (int i = 0; i < w; ++i) put(x + i, y + j, c); };
    const Color boardA = rgb(0x151b28), boardB = rgb(0x1d2535);
    const int n = S.size();
    for (int i = 0; i < n; ++i) {
        const LGame& g = *S.g[i];
        int tx = G.ox + (i % G.cols) * G.p, ty = G.oy + (i / G.cols) * G.p;

        Color flashc = g.flash_status == 1 ? kGold : pal::bad;
        if (G.boards) {
            const int cp = G.cp, BW = cp * N;
            const bool showfin = g.flash > 0 && g.snap.size() == size_t(NC);
            const uint8_t* own = showfin ? g.snap.data() : g.b.own;
            Color base = (G.p - BW) == 0 ? (((i % G.cols) + (i / G.cols)) % 2 ? boardB : boardA) : boardA;
            if (showfin && g.flash_status == 2) base = lerp(base, pal::bad, 0.30f);
            if (showfin && g.flash_status == 1 && cp == 1) base = lerp(base, kGold, 0.18f);
            int bx = tx + (G.p - BW) / 2, by = ty;
            if (G.p - BW >= 1 && showfin) rect(bx - 1, by - 1, BW + 2, BW + 2, c8(flashc));
            rect(bx, by, BW, BW, c8(base));
            if (S.cert && cp >= 2 && !showfin) {
                Bits t; g.cp.placed_T(t);
                RGB8 tc = c8(lerp(base, pal::with, 0.13f));
                t.each([&](int c) { Vec2 q = cell_xy(float(bx), float(by), float(cp), c); rect(int(q.x), int(q.y), cp, cp, tc); });
            }
            RGB8 mk = c8(kMaker), br = c8(lerp(base, kBreaker, 0.62f));
            int s = cp >= 4 ? cp - 1 : cp;
            for (int c = 0; c < NC; ++c) {
                if (!own[c]) continue;
                Vec2 q = cell_xy(float(bx), float(by), float(cp), c);
                rect(int(q.x), int(q.y), s, s, own[c] == 1 ? mk : br);
            }
            if (showfin && g.flash_status == 1) {
                RGB8 gc = c8(kGold);
                for (int k = 0; k < 6; ++k) if (g.flash_line[k] >= 0) {
                    Vec2 q = cell_xy(float(bx), float(by), float(cp), g.flash_line[k]);
                    rect(int(q.x), int(q.y), s, s, gc);
                }
            }
        } else {
            int s = std::max(1, G.p - 1);
            Color c = lerp(rgb(0x1a2232), kMaker, std::min(1.f, g.maker_moves / 11.f));
            if (g.flash > 3) c = lerp(c, flashc, 0.35f + 0.65f * (g.flash - 3) / 3.f);  // short flash when a game ends
            rect(tx, ty, s, s, c8(c));
        }
    }
}

static int nice_k(double ms) {
    if (ms <= 250) return 1;
    static const int ks[] = {2, 3, 4, 5, 6, 8, 10, 12, 15, 20, 24, 30, 40, 50, 60, 75, 90, 105, 120, 150, 180, 210, 300, 420, 600};
    int want = int(std::ceil(ms / 70.0));
    for (int k : ks) if (k >= want) return k;
    return 600;
}

// ---------------------------------------------------------------- chart strip (overlay, both panels)
struct StageRec { int n = 0; std::vector<double> ms[2]; int k[2] = {1, 1}; };
static double med(const std::vector<double>& v) { return quant(v, 0.5); }

static void draw_chart(Canvas& c, const std::vector<StageRec>& st, int cur, int nstages, const std::vector<int>& vst,
                       const std::vector<QRow>& qrows, float top) {
    const float H = 1080 - 60 - top;
    c.fill_rect(0, top, 1920, H, rgb(0x080a0f), 0.93f);
    c.fill_rect(0, top, 1920, 1, pal::grid, 1);
    c.fill_rect(1252, top + 8, 1, H - 16, pal::grid, 1);
    // ---------- cost chart
    const float X0 = 120, X1 = 1215, Y0 = top + 36, Y1 = top + H - 42;
    const double lmin = -4, lmax = 5;
    const double nmax = std::log2(double(vst.back()));
    auto X = [&](int n) { return X0 + float(std::log2(double(n)) / nmax) * (X1 - X0); };
    auto Y = [&](double ms) { double l = std::log10(std::max(ms, 1e-6)); return Y1 - float((std::clamp(l, lmin, lmax) - lmin) / (lmax - lmin)) * (Y1 - Y0); };
    c.text(24, top + 7, "Maker compute per frame vs load (log-log)", 15, pal::text, Font::Bold);
    c.circle(380, top + 17, 4.5f, pal::without); c.text(390, top + 8, "WITHOUT: α-β, 6000 nodes/move", 13, pal::without, Font::Sans);
    c.circle(600, top + 17, 4.5f, pal::with); c.text(610, top + 8, "WITH: certificate policy", 13, pal::with, Font::Sans);
    c.text(X1, top + 8, "median of the frames measured in this video · 1 thread per side · Maker policy only", 12.5f, pal::dim, Font::Sans, Align::Right);
    for (int e = -4; e <= 4; e += 2) {
        float y = Y(std::pow(10.0, e));
        c.line(X0, y, X1, y, 1, pal::grid, 0.8f);
        static const char* lab[] = {"0.1 µs", "10 µs", "1 ms", "100 ms", "10 s"};
        c.text(X0 - 8, y - 8, lab[(e + 4) / 2], 12.5f, pal::dim, Font::Mono, Align::Right);
    }
    float yb = Y(kBudget);
    for (float x = X0; x < X1; x += 12) c.line(x, yb, std::min(X1, x + 7), yb, 1.6f, pal::warn, 0.9f);
    c.text(X1 + 4, yb - 9, "16.7 ms", 12.5f, pal::warn, Font::Mono);
    // x ticks + ratio row
    for (int i = 0; i < nstages; ++i) {
        float x = X(vst[i]);
        bool done = i <= cur;
        c.line(x, Y0, x, Y1, 1, pal::grid, i == cur ? 1.f : 0.45f);
        if (i == cur) c.fill_rect(x - 14, Y0, 28, Y1 - Y0, pal::text, 0.05f);
        c.text(x, Y1 + 4, group(vst[i]), 13, i == cur ? pal::text : pal::dim, Font::Mono, Align::Center);
        if (done && !st[i].ms[0].empty() && !st[i].ms[1].empty()) {
            double w = med(st[i].ms[1]);
            c.text(x, Y1 + 21, (ratio_exact(w) ? "" : "≥") + ratio_str(ratio_value(med(st[i].ms[0]), w)), 12.5f, pal::with, Font::Mono, Align::Center);
        }
    }
    c.text(24, Y1 + 4, "N games", 13, pal::dim, Font::Mono);
    c.text(24, Y1 + 21, "W/O ÷ WITH", 12.5f, pal::with, Font::Mono);
    // curves
    for (int s = 0; s < 2; ++s) {
        Color col = s ? pal::with : pal::without;
        std::vector<Vec2> pts;
        for (int i = 0; i <= cur; ++i) if (!st[i].ms[s].empty()) pts.push_back({X(vst[i]), Y(med(st[i].ms[s]))});
        if (pts.size() >= 2) c.polyline(pts.data(), int(pts.size()), 2.4f, col, 0.95f);
        for (size_t j = 0; j < pts.size(); ++j) {
            bool live = j + 1 == pts.size();
            c.circle(pts[j].x, pts[j].y, live ? 5.5f : 4.f, col, 1);
            if (live) c.glow(pts[j].x, pts[j].y, 16, col, 0.5f);
        }
        // first budget crossing
        for (int i = 0; i <= cur; ++i) {
            if (st[i].ms[s].empty() || med(st[i].ms[s]) <= kBudget) continue;
            float x = X(vst[i]);
            c.ring(x, Y(med(st[i].ms[s])), 9, 2, pal::warn, 1);
            TextStyle ts; ts.size = 13; ts.color = col; ts.font = Font::Bold; ts.backdrop = true; ts.backdrop_alpha = 0.8f; ts.pad = 3;
            bool right = x > X0 + 0.6f * (X1 - X0);
            ts.align = right ? Align::Right : Align::Left;
            c.text(right ? x - 14 : x + 14, s ? Y1 - 22 : Y(med(st[i].ms[s])) + 8, fmt("%s over 16.7 ms from N = %s", s ? "WITH" : "WITHOUT", group(vst[i]).c_str()), ts);
            break;
        }
    }
    // ---------- quality chart (from --sweep)
    const float QX0 = 1310, QX1 = 1880, QY0 = top + 52, QY1 = top + H - 42;
    c.text(1268, top + 7, "What 60 fps buys: 16.7 ms/frame split over N games", 15, pal::text, Font::Bold);
    c.text(1268, top + 27, "Maker win % vs α-β Breaker (6000 nodes)", 13, pal::dim, Font::Sans);
    auto QX = [&](int n) { return QX0 + float(std::log2(double(n)) / nmax) * (QX1 - QX0); };
    auto QY = [&](double r) { return QY1 - float(r) * (QY1 - QY0); };
    for (int pc = 0; pc <= 100; pc += 50) {
        c.line(QX0, QY(pc / 100.0), QX1, QY(pc / 100.0), 1, pal::grid, 0.8f);
        c.text(QX0 - 6, QY(pc / 100.0) - 8, fmt("%d%%", pc), 12.5f, pal::dim, Font::Mono, Align::Right);
    }
    for (int i = 0; i < nstages; i += 2) c.text(QX(vst[i]), QY1 + 4, group(vst[i]), 12, pal::dim, Font::Mono, Align::Center);
    if (qrows.empty()) {
        c.text((QX0 + QX1) / 2, (QY0 + QY1) / 2 - 8, "run --sweep first (writes out/ladder.json)", 13, pal::dim, Font::Sans, Align::Center);
    } else {
        int games = qrows[0].games;
        for (int s = 0; s < 2; ++s) {
            Color col = s ? pal::with : pal::without;
            std::vector<Vec2> pts;
            for (auto& q : qrows) if (q.n <= vst[cur] && q.n <= vst.back()) pts.push_back({QX(q.n), QY(s ? q.c : q.s)});
            if (pts.size() >= 2) c.polyline(pts.data(), int(pts.size()), 2.2f, col, 0.95f);
            for (auto& pt : pts) c.circle(pt.x, pt.y, 3.6f, col);
            if (!pts.empty()) {
                double v = 0;
                for (auto& q : qrows) if (q.n <= vst[cur] && q.n <= vst.back()) v = s ? q.c : q.s;
                bool edge = pts.back().x > QX1 - 40;
                c.text(pts.back().x + (edge ? -8 : 8), pts.back().y - (s ? 20 : -2), fmt("%.0f%%", 100 * v), 13, col, Font::Bold,
                       edge ? Align::Right : Align::Left);
            }
        }
        c.text(1268, QY1 + 21, fmt("measured by --sweep (ladder.json) · %d games per N · search budget = 16.7/N ms per move", games), 12, pal::dim, Font::Sans);
    }
}

// ======================================================================== main
int main(int argc, char** argv) {
    const std::string cert_path = arg_str(argc, argv, "--cert", kDefaultCert);
    Certificate C;
    C.load(cert_path.c_str());
    if (!C.ok) { std::printf("certificate reconstruction failed: %s\n", C.err.c_str()); return 1; }
    if (flat().L != MAXL) { std::printf("unexpected copy count %d (MAXL %d)\n", flat().L, MAXL); return 1; }
    if (int G = arg_int(argc, argv, "--engine-check", 0)) return engine_check(C, G);
    if (arg_flag(argc, argv, "--sweep")) return run_sweep(C, argc, argv);

    std::vector<int> vst = {1, 2, 4, 8, 16, 64, 256, 1024, 4096, 16384};
    if (int mx = arg_int(argc, argv, "--video-max-n", 0)) { while (vst.size() > 1 && vst.back() > mx) vst.pop_back(); }
    const int nst = int(vst.size());
    const int SF = arg_int(argc, argv, "--stage-frames", 210);
    const int start_stage = std::clamp(arg_int(argc, argv, "--start-stage", 0), 0, nst - 1);
    const int ut = arg_int(argc, argv, "--ut-threads", 1);

    Config cfg;
    cfg.name = "L1-snaky-ladder";
    cfg.title_left = "α-β game AI (6000 nodes/move, TT + incremental eval) × N games";
    cfg.title_right = "21-move certificate policy (first surviving child) × N games";
    cfg.caption = "Family 187 — Snaky in 21 Maker moves · PROVED (Thm thm:main, Cor. cor:finite-board): win in ≤ 21 Maker moves vs every Breaker · "
                  "MEASURED here: Maker ms/frame vs load, 1 thread per side";
    cfg.frames = SF * (nst - start_stage);
    cfg.budget_ms = kBudget;
    cfg.show_speedup = false;  // the medians mix stages; the chart shows the per-stage ratio
    Harness h(argc, argv, cfg);

    Side Sd[2];
    Sd[0].cert = false; Sd[0].C = &C; Sd[0].eng.budget = 6000;
    Sd[1].cert = true; Sd[1].C = &C;
    Featured Ft[2];
    std::vector<StageRec> st(nst);
    for (int i = 0; i < nst; ++i) st[i].n = vst[i];
    std::vector<QRow> qrows = load_quality(h.out_dir() + "/ladder.json");
    int cur = -1, next_compute[2] = {0, 0};
    bool stage_first[2] = {true, true};
    std::vector<uint8_t> gbuf;
    const int GW = 928, GH = 370;
    const float GX = 16, GY = 386, chart_top = 70 + 782;
    for (int s = 0; s < 2; ++s) { h.panel(s).set_compute_label("Maker"); h.panel(s).sparkline("compute", true); }

    h.on_overlay([&](Canvas& full) { if (cur >= 0) draw_chart(full, st, cur, nst, vst, qrows, chart_top); });

    while (h.next_frame()) {
        const int f = h.frame();
        const int stg = std::min(nst - 1, start_stage + f / SF);
        if (stg != cur) {
            cur = stg;
            for (int s = 0; s < 2; ++s) { Sd[s].resize(vst[cur]); next_compute[s] = f; stage_first[s] = true; }
        }
        for (int s = 0; s < 2; ++s) {
            Panel& p = h.panel(s);
            Side& S = Sd[s];
            if (f >= next_compute[s]) {
                { auto sc = p.measure(); S.timed(); }
                double ms = p.accum_;
                if (stage_first[s]) { p.ema_ms_ = ms; stage_first[s] = false; }  // HUD EMA restarts at each stage
                const long long seq0 = S.f0_seq;
                S.untimed(ut);
                if (S.f0_seq != seq0) {  // game 1 just ended: hold its final position on the featured board
                    Featured& F = Ft[s];
                    F.hold = 40; F.status = S.f0_status; F.moves = S.f0_moves; F.kind = S.f0_kind;
                    std::memcpy(F.own, S.f0_own, sizeof F.own);
                    F.hist = S.f0_hist;
                    for (int k = 0; k < 6; ++k) F.line[k] = S.f0_line[k];
                }
                st[cur].ms[s].push_back(ms);
                int k = nice_k(ms);
                st[cur].k[s] = k;
                next_compute[s] = f + k;
            }
        }
        // ---------------- draw
        for (int s = 0; s < 2; ++s) {
            Panel& p = h.panel(s);
            Side& S = Sd[s];
            const bool cert = s == 1;
            p.clear(pal::bg);
            draw_featured(p, S, Ft[s], 632, 26, 17, f);
            if (Ft[s].hold > 0) --Ft[s].hold;
            const int n = vst[cur];
            GridGeom G = grid_geom(n, GW, GH);
            render_grid(S, gbuf, GW, GH, G);
            p.image(gbuf.data(), GW, GH, GX, GY, GW, GH);
            std::string head = fmt("STAGE %d/%d  ·  N = %s concurrent games  ·  one Maker decision per game per computed frame", cur + 1, nst, group(n).c_str());
            p.text(GX, GY - 25, head, 15, pal::text, Font::Bold);
            std::string sub = G.boards ? "every game drawn (blue = Maker, orange = Breaker; gold = completed Snaky, final position held 6 frames)"
                                       : "every game drawn as one tile (brighter = more Maker moves, gold flash = win, red = Breaker held)";
            if (cert && G.boards && G.cp >= 2) sub += " · teal = live certificate envelope";
            const auto& ms = st[cur].ms[s];
            const double last = ms.empty() ? 0 : ms.back();
            const int k = st[cur].k[s];
            if (k > 1) {
                TextStyle ts; ts.size = 22; ts.font = Font::Bold; ts.color = pal::bad; ts.align = Align::Center; ts.backdrop = true;
                ts.backdrop_alpha = 0.82f; ts.pad = 10;
                std::string when = k >= SF ? std::string("updated once per stage") : fmt("updated every %d%s frame", k, k == 2 ? "nd" : k == 3 ? "rd" : "th");
                p.text(GX + GW / 2, GY + GH / 2 - 30, fmt("over budget: %s/frame — %s", ms_str(last).c_str(), when.c_str()), ts);
                ts.size = 14; ts.font = Font::Sans; ts.color = pal::text; ts.pad = 6;
                p.text(GX + GW / 2, GY + GH / 2 + 12, "games frozen between updates · HUD and chart report the measured time per computed frame", ts);
            }
            p.text(GX, GY + GH + 4, sub, 12.5f, pal::dim, Font::Sans);
            // HUD metrics
            p.metric("load: concurrent games", double(n), "%.0f", Tone::Accent);
            double us = ms.empty() ? 0 : 1000.0 * med(ms) / n;
            p.metric("µs per Maker decision (stage median)", us, us < 1 ? "%.3f" : us < 100 ? "%.2f" : "%.0f", cert ? Tone::Good : Tone::Warn);
            p.metric("frames computed this stage", double(ms.size()), "%.0f", k > 1 ? Tone::Bad : Tone::Neutral);
            if (S.finished) p.metric("Maker win % (finished games)", 100.0 * S.wins / S.finished, "%.1f", S.wins == S.finished ? Tone::Good : Tone::Bad);
            else p.metric_text("Maker win % (finished games)", "--");
            p.metric("games finished", double(S.finished), "%.0f");
        }
    }
    // ---------------- results
    for (int i = 0; i < nst; ++i)
        for (int s = 0; s < 2; ++s)
            if (!st[i].ms[s].empty()) {
                h.result(fmt("stage_n%d_%s_median_ms", vst[i], s ? "with" : "without"), med(st[i].ms[s]));
                h.result(fmt("stage_n%d_%s_frames", vst[i], s ? "with" : "without"), double(st[i].ms[s].size()));
            }
    for (int s = 0; s < 2; ++s) {
        Panel& p = h.panel(s);
        p.result("games_finished", double(Sd[s].finished));
        p.result("wins", double(Sd[s].wins));
        p.result("max_maker_moves_win", double(Sd[s].max_moves_win));
        p.result("decisions", double(Sd[s].decisions));
        p.result("invariant_breaks", double(Sd[s].inv));
    }
    h.result("timing_breakers", "random-local / greedy ES blocker alternating by slot (untimed)");
    h.result("without_node_budget", 6000.0);
    return h.finish();
}
