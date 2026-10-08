// 04-fractal-frontier - a critical fractal frontier whose animation clock survives refinement.
//
// Both panels sample the SAME critical q = 1 square-lattice FK (bond percolation) Dobrushin
// interfaces in the unit square (fk.h, exact construction of Family 223, 01-introduction.tex)
// at three resolutions n = 64, 256, 1024, and animate them as self-drawing "creep fronts".
// The only difference is the clock that converts lattice traversals into seconds:
//   LEFT  (WITHOUT): constant-speed clock, each traversal costs time proportional to its
//                    Euclidean length delta = 1/n (tuned so 256² looks right).
//   RIGHT (WITH):    the paper's natural clock, each traversal costs c * delta^(7/4) with ONE
//                    constant c for all resolutions (eq:counting-measure, thm:main):
//                    c n^{-d} N_n converges in law (jointly with the curve and its occupation
//                    measure) to the SLE_6 Minkowski content, d = 1 + kappa/8 = 7/4 at q = 1.
// HUD: per-resolution duration quantiles, occupation mismatch between resolutions (mean time
// per fixed screen bin), sampling/tracing throughput. Compute timed = sampling + tracing +
// clock accumulation for the batch of curves each frame (identical work in both panels).
#include "demo.h"
#include "fk.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace demo;

constexpr int R = 3;
static const int RES[R] = {64, 256, 1024};
static const int BATCH[R] = {16, 4, 1};       // curves sampled per frame per resolution
constexpr int NB = 8;                          // occupation bins per side
static const Color RES_COL[R] = {rgb(0xffc861), rgb(0xb392f0), rgb(0x6cb6ff)};

// ------------------------------------------------------------------ clocks
// natural: dt = C_NAT * n^{-7/4};  Euclidean: dt = C_EUC / n with C_EUC = 256^{-3/4} * C_NAT,
// i.e. both clocks agree at n = 256 (the "tuned" resolution of the WITHOUT side).
constexpr double C_NAT = 1.0;  // seconds per (traversal * delta^{7/4})
static double dt_of(bool natural, int n) {
    if (natural) return C_NAT * std::pow(double(n), -1.75);
    return C_NAT * std::pow(256.0, -0.75) / double(n);
}

static uint64_t sample_key(int r, long long idx) { return fk::mix64(0xF00Dull * 1000003ull + uint64_t(r) * 0x1000000ull + uint64_t(idx)); }

struct Stats {
    std::vector<double> dur[R];       // total duration (s) of every sampled curve
    std::vector<double> Nn[R];        // N_n
    double occ[R][NB * NB] = {};      // summed time per bin
    long long samples[R] = {}, steps = 0;
    double compute_ms = 0;
};

struct Clocked {
    bool natural;
    Stats st;
    explicit Clocked(bool nat) : natural(nat) {}
    // measured work: sample + trace the frame's batch, accumulate durations and occupation
    void step() {
        for (int r = 0; r < R; ++r) {
            const int n = RES[r];
            const double dt = dt_of(natural, n);
            for (int b = 0; b < BATCH[r]; ++b) {
                fk::Tracer t;
                t.n = n;
                t.key = sample_key(r, st.samples[r]);
                double* occ = st.occ[r];
                long long cnt[NB * NB] = {};
                long long N = t.trace([&](float x, float y) {
                    int bx = std::min(NB - 1, int(x * NB)), by = std::min(NB - 1, int(y * NB));
                    ++cnt[by * NB + bx];
                });
                for (int k = 0; k < NB * NB; ++k) occ[k] += double(cnt[k]) * dt;
                st.dur[r].push_back(double(N) * dt);
                st.Nn[r].push_back(double(N));
                st.steps += N;
                st.samples[r]++;
            }
        }
    }
};

static double quant(std::vector<double> v, double q) {
    if (v.empty()) return 0;
    size_t k = size_t(q * (v.size() - 1));
    std::nth_element(v.begin(), v.begin() + k, v.end());
    return v[k];
}

// mean time per bin = occ / samples; mismatch = sum|A-B| / sum B
static double mismatch(const Stats& s, int ra, int rb) {
    double num = 0, den = 0;
    for (int k = 0; k < NB * NB; ++k) {
        double a = s.occ[ra][k] / std::max<long long>(1, s.samples[ra]);
        double b = s.occ[rb][k] / std::max<long long>(1, s.samples[rb]);
        num += std::fabs(a - b);
        den += b;
    }
    return den > 0 ? num / den : 0;
}

// ------------------------------------------------------------------ animated lanes
static Color time_color(double t) {  // absolute clock seconds -> color, shared by both panels
    static const Color stops[5] = {rgb(0x56d4dd), rgb(0x6cb6ff), rgb(0xb392f0), rgb(0xff7eb6), rgb(0xffc861)};
    float u = float(std::clamp(t / 7.0, 0.0, 1.0)) * 4.f;
    int i = std::min(3, int(u));
    return lerp(stops[i], stops[i + 1], u - i);
}

struct Lane {
    int r = 0, n = 0;
    double dt = 0;           // seconds per traversal
    double t = 0, T = 0;     // lane clock, duration of the current curve
    long long N = 0, drawn = 0, curves_done = 0;
    long long key_idx = -1;
    bool done = false;
    double hold = 0;
    std::vector<float> path; // projected polyline (N + 2 points)
    static constexpr int S = 296;
    std::vector<float> acc;  // S*S*3 additive light
    float wline = 0;

    void init(int r_, bool natural) {
        r = r_; n = RES[r]; dt = dt_of(natural, n);
        acc.assign(S * S * 3, 0.f);
        wline = 1.2f * std::pow(64.f / n, 0.65f);
    }
    void new_curve(long long idx) {
        key_idx = idx;
        fk::Tracer tr; tr.n = n; tr.key = sample_key(r, idx);
        N = tr.trace([](float, float) {}, &path);
        T = double(N) * dt;
        t = 0; drawn = 0; done = false; hold = 0;
    }
    void deposit(float x0, float y0, float x1, float y1, Color c) {
        float px0 = 2 + x0 * (S - 4), py0 = 2 + (1 - y0) * (S - 4);
        float px1 = 2 + x1 * (S - 4), py1 = 2 + (1 - y1) * (S - 4);
        float L = std::hypot(px1 - px0, py1 - py0);
        int m = std::max(1, int(L * 1.6f));
        float w = wline * L / m;
        for (int s = 0; s < m; ++s) {
            float u = (s + 0.5f) / m;
            float x = px0 + (px1 - px0) * u, y = py0 + (py1 - py0) * u;
            int ix = int(x), iy = int(y);
            float fx = x - ix, fy = y - iy;
            if (ix < 0 || iy < 0 || ix + 1 >= S || iy + 1 >= S) continue;
            float wt[4] = {(1 - fx) * (1 - fy) * w, fx * (1 - fy) * w, (1 - fx) * fy * w, fx * fy * w};
            int id[4] = {iy * S + ix, iy * S + ix + 1, (iy + 1) * S + ix, (iy + 1) * S + ix + 1};
            for (int q = 0; q < 4; ++q) {
                float* a = &acc[id[q] * 3];
                a[0] += c.r * wt[q]; a[1] += c.g * wt[q]; a[2] += c.b * wt[q];
            }
        }
    }
    // advance the lane clock by h seconds; returns true if a new curve is needed
    bool advance(double h) {
        if (done) { hold += h; return hold > 0.45; }
        t += h;
        long long target = std::min<long long>(N, (long long)std::floor(t / dt));
        long long pts = (long long)(path.size() / 2);
        for (long long j = drawn; j < target; ++j) {  // segment j: path point j+0 -> j+1 (shifted by terminal)
            long long a = j, b = j + 1;
            if (b >= pts) break;
            deposit(path[2 * a], path[2 * a + 1], path[2 * b], path[2 * b + 1], time_color(double(j) * dt));
        }
        drawn = target;
        if (t >= T) {
            if (N + 1 < pts) deposit(path[2 * N], path[2 * N + 1], path[2 * N + 2], path[2 * N + 3], time_color(T));
            done = true; curves_done++;
        }
        return false;
    }
    void fade(float k) { for (float& v : acc) v *= k; }
    void tip(float& x, float& y) const {
        long long j = std::min<long long>(drawn, (long long)(path.size() / 2) - 1);
        x = path[2 * j]; y = path[2 * j + 1];
    }
};

struct Side {
    Clocked sim;
    Lane lanes[R];
    std::vector<uint8_t> img;
    explicit Side(bool nat) : sim(nat) {
        for (int r = 0; r < R; ++r) lanes[r].init(r, nat);
        img.resize(Lane::S * Lane::S * 3);
    }
};

// ------------------------------------------------------------------ drawing
static const float LANE_Y = 410, LANE_X0 = 18, LANE_GAP = 17;

static void draw_cdf(Panel& p, const Side& s, double ref_med) {
    const float x0 = 520, x1 = 940, y0 = 40, y1 = 350;
    p.fill_rounded_rect(x0 - 50, y0 - 32, x1 - x0 + 66, y1 - y0 + 80, 10, rgb(0x10131b), 0.9f);
    p.text(x0 - 38, y0 - 26, "total duration per curve · empirical CDF", 15, pal::dim, Font::Sans);
    const double lmin = std::log10(0.3), lmax = std::log10(30.0);
    auto X = [&](double d) { return x0 + float((std::log10(std::max(d, 0.3)) - lmin) / (lmax - lmin)) * (x1 - x0); };
    auto Y = [&](double q) { return y1 - float(q) * (y1 - y0); };
    for (double tk : {0.3, 1.0, 3.0, 10.0, 30.0}) {
        p.line(X(tk), y0, X(tk), y1, 1, pal::grid);
        p.text(X(tk), y1 + 6, fmt(tk < 1 ? "%.1f s" : "%.0f s", tk), 13, pal::dim, Font::Mono, Align::Center);
    }
    for (double q : {0.25, 0.5, 0.75}) {
        p.line(x0, Y(q), x1, Y(q), 1, pal::grid, q == 0.5 ? 1.f : 0.5f);
        p.text(x0 - 6, Y(q) - 8, fmt("%.2f", q), 12, pal::dim, Font::Mono, Align::Right);
    }
    // tuned reference
    p.line(X(ref_med), y0, X(ref_med), y1, 1.5f, pal::text, 0.25f);
    for (int r = 0; r < R; ++r) {
        std::vector<double> v = s.sim.st.dur[r];
        if (v.size() < 2) continue;
        std::sort(v.begin(), v.end());
        std::vector<Vec2> pts;
        const int M = 160;
        for (int k = 0; k <= M; ++k) {
            double q = double(k) / M;
            double d = v[size_t(q * (v.size() - 1))];
            pts.push_back({X(d), Y(q)});
        }
        p.blend = Blend::Add;
        p.polyline(pts.data(), int(pts.size()), 5.f, RES_COL[r], 0.18f);
        p.blend = Blend::Normal;
        p.polyline(pts.data(), int(pts.size()), 2.2f, RES_COL[r], 0.95f);
        double q25 = v[size_t(0.25 * (v.size() - 1))], q50 = v[size_t(0.5 * (v.size() - 1))], q75 = v[size_t(0.75 * (v.size() - 1))];
        float yb = y0 + 8 + r * 11.f;
        p.line(X(q25), yb, X(q75), yb, 4, RES_COL[r], 0.8f);
        p.circle(X(q50), yb, 4.5f, RES_COL[r]);
        p.circle(X(q50), Y(0.5), 4.f, RES_COL[r]);
    }
    float ly = y1 + 26;
    float lx = x0 - 30;
    for (int r = 0; r < R; ++r) {
        p.fill_rect(lx, ly + 7, 18, 4, RES_COL[r]);
        lx += 24 + p.text(lx + 24, ly - 2, fmt("%d²  (%lld curves)", RES[r], s.sim.st.samples[r]), 14, pal::text, Font::Sans) + 14;
    }
}

static void draw_lanes(Panel& p, Side& s, double tnow) {
    for (int r = 0; r < R; ++r) {
        Lane& L = s.lanes[r];
        float x = LANE_X0 + r * (Lane::S + LANE_GAP), y = LANE_Y;
        // tone-map accumulation
        for (int k = 0; k < Lane::S * Lane::S * 3; ++k) {
            float v = L.acc[k];
            float m = 1.f - std::exp(-v * 1.6f);
            s.img[k] = uint8_t(std::clamp(int((0.043f + m * 0.957f) * 255.f + 0.5f), 0, 255));
        }
        // background tint = pal::bg where acc == 0 (approx)
        p.fill_rect(x - 2, y - 2, Lane::S + 4, Lane::S + 4, pal::bg);
        p.image(s.img.data(), Lane::S, Lane::S, x, y, Lane::S, Lane::S);
        p.stroke_rect(x - 1, y - 1, Lane::S + 2, Lane::S + 2, 1, RES_COL[r], 0.45f);
        // wired arc W (top + left) and free arc F (bottom + right) markers
        p.line(x, y + Lane::S, x, y, 3, pal::text, 0.35f);
        p.line(x, y, x + Lane::S, y, 3, pal::text, 0.35f);
        if (!L.path.empty()) {
            float tx, ty;
            L.tip(tx, ty);
            float px = x + 2 + tx * (Lane::S - 4), py = y + 2 + (1 - ty) * (Lane::S - 4);
            if (!L.done) {
                p.glow(px, py, 22, time_color(L.t), 0.9f);
                p.circle(px, py, 3.2f, rgb(0xffffff), 0.95f);
            }
        }
        p.circle(x + 2, y + Lane::S - 2, 4, pal::text, 0.7f);
        p.circle(x + Lane::S - 2, y + 2, 4, pal::text, 0.7f);
        // label + progress
        float ly = y + Lane::S + 8;
        p.text(x, ly, fmt("%d × %d", RES[r], RES[r]), 17, RES_COL[r], Font::Bold);
        p.text(x + Lane::S, ly + 2, fmt("curve #%lld", L.curves_done + (L.done ? 0 : 1)), 14, pal::dim, Font::Mono, Align::Right);
        float by = ly + 28, bw = Lane::S;
        double frac = L.T > 0 ? std::min(1.0, L.t / L.T) : 0;
        p.fill_rounded_rect(x, by, bw, 8, 4, pal::grid);
        p.fill_rounded_rect(x, by, std::max(8.f, float(bw * frac)), 8, 4, time_color(L.t), 0.95f);
        p.text(x, by + 12, fmt("t = %.1f s  of  %.1f s", std::min(L.t, L.T), L.T), 14, pal::text, Font::Mono);
        (void)tnow;
    }
}

static void draw_occ(Panel& p, const Side& s, float vmax) {
    const float sz = 150, y = 820;
    p.text(LANE_X0, y - 30, "mean time spent per screen bin (8×8), same color scale both panels", 14, pal::dim, Font::Sans);
    for (int r = 0; r < R; ++r) {
        float x = LANE_X0 + r * (Lane::S + LANE_GAP);
        float f[NB * NB];
        for (int j = 0; j < NB; ++j)
            for (int i = 0; i < NB; ++i)  // field row 0 = top -> y = 1
                f[j * NB + i] = float(s.sim.st.occ[r][(NB - 1 - j) * NB + i] / std::max<long long>(1, s.sim.st.samples[r]));
        p.field(f, NB, NB, x, y - 8, sz * 0.8f, sz * 0.8f, Cmap::Inferno, 0, vmax, false);
        p.stroke_rect(x, y - 8, sz * 0.8f, sz * 0.8f, 1, RES_COL[r], 0.5f);
        double tot = 0;
        for (int k = 0; k < NB * NB; ++k) tot += s.sim.st.occ[r][k];
        tot /= std::max<long long>(1, s.sim.st.samples[r]);
        p.text(x + sz * 0.8f + 12, y + 10, "mean total", 13, pal::dim);
        p.text(x + sz * 0.8f + 12, y + 28, fmt("%.2f s", tot), 18, RES_COL[r], Font::Mono);
    }
}

int main(int argc, char** argv) {
    Config cfg;
    cfg.name = "04-fractal-frontier";
    cfg.title_left = "constant-speed clock · time ∝ length (tuned at 256²)";
    cfg.title_right = "natural clock · each step costs 1 s × δ^(7/4)";
    cfg.caption = "Family 223 — Natural Occupation Measures for Critical Square-Lattice FK Interfaces · "
                  "WITH: ν = c·n^{-d}·Σ δ_z, d = 7/4 at q = 1 (eq:counting-measure, thm:main) · "
                  "7/4 itself is classical; new = proved convergence with one constant";
    cfg.frames = 1560;
    cfg.show_speedup = false;
    Harness h(argc, argv, cfg);

    Side L(false), Rt(true);
    long long next_idx[2][R] = {};
    // first curves: the very first samples
    for (int r = 0; r < R; ++r) { L.lanes[r].new_curve(0); Rt.lanes[r].new_curve(0); }

    h.left().set_compute_label("sample+trace");
    h.right().set_compute_label("sample+trace");
    h.left().sparkline("occupation mismatch", true);
    h.right().sparkline("occupation mismatch", true);
    double t_ms[2] = {0, 0};

    while (h.next_frame()) {
        {
            double a = now_ms();
            h.left().measure([&] { L.sim.step(); });
            double b = now_ms();
            h.right().measure([&] { Rt.sim.step(); });
            double c = now_ms();
            t_ms[0] += b - a; t_ms[1] += c - b;
        }
        const double hstep = 1.0 / h.fps();
        Side* sides[2] = {&L, &Rt};
        for (int sd = 0; sd < 2; ++sd)
            for (int r = 0; r < R; ++r) {
                Lane& ln = sides[sd]->lanes[r];
                if (ln.advance(hstep)) {
                    long long idx = std::max(++next_idx[sd][r], sides[sd]->sim.st.samples[r] - 1);
                    next_idx[sd][r] = idx;
                    ln.new_curve(idx);
                }
                if (ln.done) ln.fade(0.93f);
            }

        // shared occupation color scale: right panel's mean bin maximum (natural clock)
        float vmax = 0;
        for (int r = 0; r < R; ++r)
            for (int k = 0; k < NB * NB; ++k)
                vmax = std::max(vmax, float(Rt.sim.st.occ[r][k] / std::max<long long>(1, Rt.sim.st.samples[r])));
        vmax *= 1.05f;

        Panel* ps[2] = {&h.left(), &h.right()};
        double tune_med = 0;
        {
            std::vector<double> v = Rt.sim.st.dur[1];
            tune_med = quant(v, 0.5);
        }
        for (int sd = 0; sd < 2; ++sd) {
            Panel& p = *ps[sd];
            Side& s = *sides[sd];
            p.clear(pal::bg);
            draw_cdf(p, s, tune_med);
            draw_lanes(p, s, h.time());
            draw_occ(p, s, vmax);

            double med[R], q25[R], q75[R];
            for (int r = 0; r < R; ++r) {
                med[r] = quant(s.sim.st.dur[r], 0.5);
                q25[r] = quant(s.sim.st.dur[r], 0.25);
                q75[r] = quant(s.sim.st.dur[r], 0.75);
            }
            double ratio = med[0] > 0 ? med[2] / med[0] : 0;
            double mm = std::max({mismatch(s.sim.st, 2, 0), mismatch(s.sim.st, 1, 0), mismatch(s.sim.st, 2, 1)}) * 100;
            bool nat = sd == 1;
            p.metric_text("median duration (s)", fmt("%.2f | %.2f | %.2f", med[0], med[1], med[2]), nat ? Tone::Good : Tone::Bad);
            p.metric_text("IQR (s) 64² | 1024²", fmt("%.2f–%.2f | %.2f–%.2f", q25[0], q75[0], q25[2], q75[2]));
            p.metric("median ratio 1024² / 64²", ratio, "%.3fx", nat ? Tone::Good : Tone::Bad);
            p.metric("occupation mismatch", mm, "%.1f %%", nat ? Tone::Good : Tone::Bad);
            double secs = t_ms[sd] / 1000.0;
            p.metric("traversals traced / s", secs > 0 ? double(s.sim.st.steps) / secs : 0, "si");
            if (!nat) {
                // the other naive convention: one fixed tick per lattice step
                double r0 = quant(s.sim.st.Nn[0], 0.5), r2 = quant(s.sim.st.Nn[2], 0.5);
                p.metric_text("fixed-tick clock: ratio 1024²/64²", fmt("×%.0f", r0 > 0 ? r2 / r0 : 0), Tone::Bad);
            } else {
                double r0 = quant(s.sim.st.Nn[0], 0.5), r2 = quant(s.sim.st.Nn[2], 0.5);
                double fit = (r0 > 0 && r2 > 0) ? std::log(r2 / r0) / std::log(16.0) : 0;
                p.metric("fitted exponent (classical 7/4)", fit, "%.3f", Tone::Accent);
            }
        }
    }
    // results
    Side* sides[2] = {&L, &Rt};
    const char* nm[2] = {"left_euclid", "right_natural"};
    for (int sd = 0; sd < 2; ++sd)
        for (int r = 0; r < R; ++r) {
            const Stats& st = sides[sd]->sim.st;
            std::string k = fmt("%s_n%d_", nm[sd], RES[r]);
            h.result(k + "samples", double(st.samples[r]));
            h.result(k + "dur_q25", quant(st.dur[r], 0.25));
            h.result(k + "dur_median", quant(st.dur[r], 0.5));
            h.result(k + "dur_q75", quant(st.dur[r], 0.75));
        }
    for (int r = 0; r < R; ++r) {
        double m = 0;
        for (double v : Rt.sim.st.Nn[r]) m += v * std::pow(RES[r], -1.75);
        h.result(fmt("mean_N_times_n^-7/4_n%d", RES[r]), m / std::max<size_t>(1, Rt.sim.st.Nn[r].size()));
    }
    h.result("left_occupation_mismatch_1024_vs_64", mismatch(L.sim.st, 2, 0));
    h.result("right_occupation_mismatch_1024_vs_64", mismatch(Rt.sim.st, 2, 0));
    h.result("q", 1);
    h.result("note", "q=1 critical bond percolation Dobrushin interface; c(1) unknown, clock constant set to 1 s per traversal*delta^(7/4)");
    return h.finish();
}
