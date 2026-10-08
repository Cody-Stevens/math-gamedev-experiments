// 09-cubic-interference - cubic phasor walks with a persistent, certified hidden bias.
//
// For each prime p ≡ 1 (mod 3) up to X (X sweeps 60 -> 250,000 over the video):
//   RIGHT (WITH):   the true cubic sum S_p = Σ_{x mod p} e(x³/p), computed directly with p unit
//                   phasors (exact integer residues x³ mod p), accumulated into
//                   M(X) = Σ_{p≤X, p≡1(3)} S_p/(2√p)   and compared with the PROVED asymptotic
//                   M(X) ~ K X^{5/6}/log X, K = (2π)^{2/3}/(5Γ(2/3))   (Family 023, paper.tex,
//                   subsection "The rational-prime convention", eq:pairing, eq:rational-main).
//   LEFT (WITHOUT): what a designer would assume - the sum behaves like a random arrow:
//                   (a) the gallery walks use p independent uniform phases (same renderer,
//                       same normalization 1/(2√p)); (b) the statistical reference is the
//                   uniform-circle model S_p/(2√p) = cos θ_p, θ_p uniform (mean 0, variance 1/2),
//                   with 48 independent repetitions and its ±1σ/±2σ band. The true M(X) is drawn
//                   as a ghost line for comparison (not computed on that side).
// Both sides do the same O(p) phasor work per prime; there is no speed claim. S_p is real by
// eq:pairing, so |Im S_p| is pure floating-point error.
#include "demo.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

using namespace demo;

constexpr double PI = 3.14159265358979323846;
constexpr int NREP = 48;  // repetitions of the uniform-circle model
static const double K_RAT = std::pow(2 * PI, 2.0 / 3.0) / (5.0 * std::tgamma(2.0 / 3.0));
static const double C_STAR = std::pow(2 * PI, 2.0 / 3.0) / (3.0 * std::tgamma(2.0 / 3.0));

static double prediction(double X) { return X > 2 ? K_RAT * std::pow(X, 5.0 / 6.0) / std::log(X) : 0; }

static uint64_t mix64(uint64_t z) {
    z += 0x9e3779b97f4a7c15ull;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}
static inline double u01(uint64_t& s) { s = mix64(s); return double(s >> 11) * (1.0 / 9007199254740992.0); }

// ---------------------------------------------------------------- the two per-prime computations
// true cubic sum: x³ mod p maintained incrementally ((x+1)³ = x³ + 3x² + 3x + 1)
static void cubic_sum(int p, double& re, double& im) {
    long long c = 0, d = 1, e = 6;  // x³, 3x²+3x+1, 6x+6  (mod p) at x = 0
    const double w = 2 * PI / p;
    double sr = 0, si = 0;
    for (int x = 0; x < p; ++x) {
        double a = w * double(c);
        sr += std::cos(a); si += std::sin(a);
        c += d; if (c >= p) c -= p;
        d += e; if (d >= p) d -= p;
        e += 6; if (e >= p) e -= p;
    }
    re = sr; im = si;
}
// naive model: p independent uniform phases (the "random arrows" a designer would assume)
static void random_sum(int p, uint64_t seed, double& re, double& im) {
    uint64_t s = seed;
    double sr = 0, si = 0;
    for (int x = 0; x < p; ++x) {
        double a = 2 * PI * u01(s);
        sr += std::cos(a); si += std::sin(a);
    }
    re = sr; im = si;
}

// walk paths for display only (untimed)
static void walk_path(int p, bool cubic, uint64_t seed, std::vector<float>& out) {
    out.clear();
    out.reserve(2 * (p + 1));
    double sr = 0, si = 0, inv = 1.0 / std::sqrt(double(p));
    out.push_back(0); out.push_back(0);
    long long c = 0, d = 1, e = 6;
    uint64_t s = seed;
    for (int x = 0; x < p; ++x) {
        double a;
        if (cubic) {
            a = 2 * PI * double(c) / p;
            c += d; if (c >= p) c -= p;
            d += e; if (d >= p) d -= p;
            e += 6; if (e >= p) e -= p;
        } else {
            a = 2 * PI * u01(s);
        }
        sr += std::cos(a); si += std::sin(a);
        out.push_back(float(sr * inv)); out.push_back(float(si * inv));
    }
}

static uint64_t walk_seed(int p) { return mix64(0xC0BEull * 7919ull + uint64_t(p)); }

// ---------------------------------------------------------------- per-side state
struct Series {
    std::vector<double> X, M;  // after each prime
};

struct Side {
    bool cubic;
    std::vector<int>* primes;
    size_t next = 0;
    double M = 0, max_im = 0;
    double rep[NREP] = {};        // uniform-circle repetitions (left only)
    std::vector<float> rep_hist;  // NREP per prime
    Series s;
    std::vector<double> val;      // S_p/(2√p) per prime
    uint64_t rng = 0x5eed;
    long long phasors = 0;

    // measured: process all primes ≤ X
    void step(double X) {
        while (next < primes->size() && (*primes)[next] <= X) {
            int p = (*primes)[next++];
            double re, im;
            if (cubic) cubic_sum(p, re, im);
            else {
                random_sum(p, walk_seed(p), re, im);
                for (int k = 0; k < NREP; ++k) rep[k] += std::cos(2 * PI * u01(rng));
            }
            phasors += p;
            double v = re / (2 * std::sqrt(double(p)));
            M += v;
            if (cubic) max_im = std::max(max_im, std::fabs(im));
            val.push_back(v);
            s.X.push_back(p);
            s.M.push_back(M);
            if (!cubic) rep_hist.insert(rep_hist.end(), rep, rep + NREP);
        }
    }
};

// ---------------------------------------------------------------- featured walk (accumulation)
struct Feature {
    static constexpr int W = 470, H = 380;
    std::vector<float> acc, path;
    std::vector<uint8_t> img;
    int p = 0, drawn = 0, total = 0;
    double value = 0;
    float ox = 0, oy = 0, sc = 1;  // world -> px
    Feature() { acc.assign(W * H * 3, 0); img.resize(W * H * 3); }
    void start(int p_, bool cubic, double v) {
        p = p_; value = v;
        walk_path(p, cubic, walk_seed(p), path);
        total = int(path.size() / 2) - 1;
        drawn = 0;
        float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
        for (size_t i = 0; i < path.size(); i += 2) {
            x0 = std::min(x0, path[i]); x1 = std::max(x1, path[i]);
            y0 = std::min(y0, path[i + 1]); y1 = std::max(y1, path[i + 1]);
        }
        x0 = std::min(x0, 0.f); x1 = std::max(x1, 0.f);
        float m = 26;
        sc = std::min((W - 2 * m) / std::max(1e-3f, x1 - x0), (H - 2 * m - 30) / std::max(1e-3f, y1 - y0));
        ox = W * 0.5f - sc * (x0 + x1) * 0.5f;
        oy = (H + 20) * 0.5f + sc * (y0 + y1) * 0.5f;
    }
    Vec2 px(float x, float y) const { return {ox + sc * x, oy - sc * y}; }
    void deposit(Vec2 a, Vec2 b, Color c, float w) {
        float L = std::hypot(b.x - a.x, b.y - a.y);
        int m = std::max(1, int(L * 1.5f));
        float ww = w * L / m;
        for (int s = 0; s < m; ++s) {
            float u = (s + 0.5f) / m;
            float x = a.x + (b.x - a.x) * u, y = a.y + (b.y - a.y) * u;
            int ix = int(x), iy = int(y);
            if (ix < 0 || iy < 0 || ix + 1 >= W || iy + 1 >= H) continue;
            float fx = x - ix, fy = y - iy;
            float wt[4] = {(1 - fx) * (1 - fy) * ww, fx * (1 - fy) * ww, (1 - fx) * fy * ww, fx * fy * ww};
            int id[4] = {iy * W + ix, iy * W + ix + 1, (iy + 1) * W + ix, (iy + 1) * W + ix + 1};
            for (int q = 0; q < 4; ++q) {
                float* t = &acc[id[q] * 3];
                t[0] += c.r * wt[q]; t[1] += c.g * wt[q]; t[2] += c.b * wt[q];
            }
        }
    }
    void reveal(double frac) {
        int target = std::min(total, int(frac * total + 0.5));
        // brightness per px of length: keep total light roughly constant across p
        float len_px = 0;
        (void)len_px;
        float w = 0.9f * std::clamp(30.f / std::sqrt(float(std::max(1, p))) * 4.f, 0.4f, 1.2f);
        for (int j = drawn; j < target; ++j) {
            Color c = colormap(Cmap::Turbo, 0.1f + 0.85f * float(j) / total);
            deposit(px(path[2 * j], path[2 * j + 1]), px(path[2 * j + 2], path[2 * j + 3]), c, w);
        }
        drawn = target;
    }
    void fade(float k) { for (float& v : acc) v *= k; }
    void blit(Panel& pn, float x, float y) {
        for (int k = 0; k < W * H * 3; ++k) {
            float m = 1.f - std::exp(-acc[k] * 1.4f);
            float base = (k % 3 == 0) ? 0.043f : (k % 3 == 1 ? 0.051f : 0.071f);
            img[k] = uint8_t(std::clamp(int((base + m * (1 - base)) * 255.f + 0.5f), 0, 255));
        }
        pn.image(img.data(), W, H, x, y, W, H);
    }
};

struct Thumb { int p = 0; double v = 0; std::vector<Vec2> pts; };

static void make_thumb(Thumb& t, int p, bool cubic, double v) {
    std::vector<float> path;
    walk_path(p, cubic, walk_seed(p), path);
    t.p = p; t.v = v; t.pts.clear();
    int n = int(path.size() / 2);
    int stride = std::max(1, n / 3000);
    for (int i = 0; i < n; i += stride) t.pts.push_back({path[2 * i], path[2 * i + 1]});
    t.pts.push_back({path[2 * (n - 1)], path[2 * (n - 1) + 1]});
}

static void draw_thumb(Panel& pn, const Thumb& t, float x, float y, float w, float h, Color accent) {
    pn.fill_rounded_rect(x, y, w, h, 8, rgb(0x10131b));
    if (t.pts.empty()) return;
    float x0 = 0, x1 = 0, y0 = 0, y1 = 0;
    for (auto& q : t.pts) { x0 = std::min(x0, q.x); x1 = std::max(x1, q.x); y0 = std::min(y0, q.y); y1 = std::max(y1, q.y); }
    float m = 10, ih = h - 24;
    float sc = std::min((w - 2 * m) / std::max(1e-3f, x1 - x0), (ih - 2 * m) / std::max(1e-3f, y1 - y0));
    float ox = x + w * 0.5f - sc * (x0 + x1) * 0.5f, oy = y + ih * 0.5f + sc * (y0 + y1) * 0.5f;
    std::vector<Vec2> P(t.pts.size());
    for (size_t i = 0; i < P.size(); ++i) P[i] = {ox + sc * t.pts[i].x, oy - sc * t.pts[i].y};
    pn.blend = Blend::Add;
    int segs = int(P.size()) - 1, chunk = std::max(1, segs / 12);
    for (int s = 0; s < segs; s += chunk) {
        int e = std::min(segs, s + chunk);
        Color c = colormap(Cmap::Turbo, 0.1f + 0.85f * float(s) / segs);
        pn.polyline(&P[s], e - s + 1, 1.0f, c, 0.55f);
    }
    pn.blend = Blend::Normal;
    Vec2 o = P.front(), e = P.back();
    pn.line(o.x, o.y, e.x, e.y, 1.5f, pal::text, 0.8f);
    pn.circle(e.x, e.y, 3, accent);
    pn.circle(o.x, o.y, 2.5f, pal::text, 0.8f);
    pn.text(x + 8, y + h - 22, fmt("p=%d", t.p), 13, pal::dim, Font::Mono);
    pn.text(x + w - 8, y + h - 22, fmt("%+.2f", t.v), 13, t.v >= 0 ? pal::good : pal::bad, Font::Mono, Align::Right);
}

// ---------------------------------------------------------------- running-total plot
struct PlotFrame { float x0, y0, x1, y1; double X1, ylo, yhi; };

static Vec2 P(const PlotFrame& f, double X, double y) {
    return {f.x0 + float(X / f.X1) * (f.x1 - f.x0), f.y1 - float((y - f.ylo) / (f.yhi - f.ylo)) * (f.y1 - f.y0)};
}

static void plot_axes(Panel& pn, const PlotFrame& f, const char* title) {
    pn.fill_rounded_rect(f.x0 - 62, f.y0 - 34, f.x1 - f.x0 + 80, f.y1 - f.y0 + 68, 10, rgb(0x10131b), 0.95f);
    pn.text(f.x0 - 50, f.y0 - 28, title, 15, pal::dim);
    // y ticks
    double span = f.yhi - f.ylo, step = std::pow(10.0, std::floor(std::log10(span / 4)));
    if (span / step > 10) step *= 2;
    if (span / step > 10) step *= 2.5;
    for (double v = std::ceil(f.ylo / step) * step; v <= f.yhi; v += step) {
        Vec2 a = P(f, 0, v);
        pn.line(f.x0, a.y, f.x1, a.y, 1, pal::grid, std::fabs(v) < 1e-9 ? 1.f : 0.6f);
        pn.text(f.x0 - 8, a.y - 8, fmt("%.0f", v), 12, pal::dim, Font::Mono, Align::Right);
    }
    double xs = std::pow(10.0, std::floor(std::log10(f.X1 / 3)));
    if (f.X1 / xs > 6) xs *= 2;
    if (f.X1 / xs > 6) xs *= 2.5;
    for (double X = xs; X <= f.X1 * 1.0001; X += xs) {
        Vec2 a = P(f, X, f.ylo);
        pn.line(a.x, f.y0, a.x, f.y1, 1, pal::grid, 0.5f);
        pn.text(a.x, f.y1 + 4, X >= 1000 ? fmt("%gk", X / 1000) : fmt("%g", X), 12, pal::dim, Font::Mono, Align::Center);
    }
    pn.text(f.x1, f.y1 + 4, "X", 12, pal::dim, Font::Mono, Align::Left);
}

static void polyline_series(Panel& pn, const PlotFrame& f, const std::vector<double>& X, const std::vector<double>& Y,
                            float th, Color c, float a, bool glow) {
    if (X.size() < 2) return;
    std::vector<Vec2> pts;
    size_t n = X.size(), stride = std::max<size_t>(1, n / 700);
    for (size_t i = 0; i < n; i += stride) pts.push_back(P(f, X[i], Y[i]));
    pts.push_back(P(f, X[n - 1], Y[n - 1]));
    if (glow) { pn.blend = Blend::Add; pn.polyline(pts.data(), int(pts.size()), th * 3.5f, c, 0.15f); pn.blend = Blend::Normal; }
    pn.polyline(pts.data(), int(pts.size()), th, c, a);
}

static void dashed_curve(Panel& pn, const PlotFrame& f, double Xa, double Xb, double (*fn)(double), Color c, float th) {
    const int M = 160;
    Vec2 prev = P(f, Xa, fn(Xa));
    for (int i = 1; i <= M; ++i) {
        double X = Xa + (Xb - Xa) * i / M;
        Vec2 q = P(f, X, fn(X));
        if (i % 2 == 0) pn.line(prev.x, prev.y, q.x, q.y, th, c, 0.95f);
        prev = q;
    }
}

int main(int argc, char** argv) {
    Config cfg;
    cfg.name = "09-cubic-interference";
    cfg.title_left = "random-arrow model: each S_p a mean-zero random phasor sum";
    cfg.title_right = "true cubic sums e(x³/p) + proved bias law (eq:rational-main)";
    cfg.caption = "Family 023 — An unconditional first moment for cubic Gauss sums · WITH: Σ S_p/(2√p) over p ≤ X, p ≡ 1 mod 3  ~  "
                  "(2π)^(2/3)/(5Γ(2/3)) · X^(5/6)/log X  (eq:pairing, eq:rational-main) · same O(p) sums both sides, no speedup";
    cfg.frames = 1560;
    cfg.show_speedup = false;
    Harness h(argc, argv, cfg);

    const int XMAX = arg_int(argc, argv, "--xmax", 250000);
    // sieve (setup, untimed)
    std::vector<char> comp(XMAX + 1, 0);
    std::vector<int> primes;
    for (int i = 2; i <= XMAX; ++i) {
        if (comp[i]) continue;
        if (i % 3 == 1) primes.push_back(i);
        for (long long j = (long long)i * i; j <= XMAX; j += i) comp[j] = 1;
    }

    Side L, Rt;
    L.cubic = false; Rt.cubic = true;
    L.primes = Rt.primes = &primes;
    // elementary model term of the paper's argument: Σ c_* p^{-1/6} (same asymptotic as eq:rational-main)
    std::vector<double> modelM(primes.size());
    { double m = 0; for (size_t i = 0; i < primes.size(); ++i) { m += C_STAR * std::pow(double(primes[i]), -1.0 / 6.0); modelM[i] = m; } }

    Feature featL, featR;
    Thumb thL[6], thR[6];
    int thumb_n = 0, feat_p = 0;
    const int FEAT_FRAMES = 54, FEAT_DRAW = 40;

    h.left().set_compute_label("sum phasors");
    h.right().set_compute_label("sum phasors");
    h.left().sparkline("true M(X) in model σ");
    h.right().sparkline("ratio M / prediction");

    auto X_of = [&](int fr) {
        double u = double(fr + 1) / h.frames();
        return 60.0 + (XMAX - 60.0) * std::pow(u, 1.6);
    };

    while (h.next_frame()) {
        const int fr = h.frame();
        const double X = X_of(fr);
        h.left().measure([&] { L.step(X); });
        h.right().measure([&] { Rt.step(X); });
        const size_t n = Rt.val.size();

        // ----- featured walk + thumbnails (display only)
        int ph = fr % FEAT_FRAMES;
        if (ph == 0 && n > 0) {
            if (feat_p > 0) {  // push previous featured prime into the thumbnail strip
                for (int k = 5; k > 0; --k) { thL[k] = thL[k - 1]; thR[k] = thR[k - 1]; }
                size_t idx = std::lower_bound(primes.begin(), primes.end(), feat_p) - primes.begin();
                make_thumb(thL[0], feat_p, false, L.val[idx]);
                make_thumb(thR[0], feat_p, true, Rt.val[idx]);
                thumb_n = std::min(6, thumb_n + 1);
            }
            feat_p = primes[n - 1];
            featL.fade(0); featR.fade(0);
            featL.start(feat_p, false, L.val[n - 1]);
            featR.start(feat_p, true, Rt.val[n - 1]);
        }
        double frac = std::min(1.0, double(ph + 1) / FEAT_DRAW);
        featL.reveal(frac); featR.reveal(frac);
        if (ph > FEAT_FRAMES - 8) { featL.fade(0.8f); featR.fade(0.8f); }

        // ----- shared plot frame
        double sigma = std::sqrt(double(n) / 2.0);
        double Mtrue = Rt.M, pred = prediction(X);
        double yhi = std::max({Mtrue, pred, n ? modelM[n - 1] : 0.0, 2.5 * sigma, 10.0}) * 1.12;
        double ylo = -std::max(2.6 * sigma, 0.18 * yhi);
        PlotFrame pf{80, 612, 930, 900, std::max(X, 100.0), ylo, yhi};

        Side* sides[2] = {&L, &Rt};
        Feature* feats[2] = {&featL, &featR};
        Thumb* ths[2] = {thL, thR};
        for (int sd = 0; sd < 2; ++sd) {
            Panel& pn = sd ? h.right() : h.left();
            Side& S = *sides[sd];
            bool cub = sd == 1;
            pn.clear(pal::bg);
            // featured walk
            const float fx = 476, fy = 14;
            feats[sd]->blit(pn, fx, fy);
            pn.stroke_rect(fx, fy, Feature::W, Feature::H, 1, pal::grid);
            Feature& F = *feats[sd];
            if (F.p > 0) {
                pn.text(fx + 12, fy + 8, fmt("p = %d", F.p), 18, pal::text, Font::Bold);
                pn.text(fx + 12, fy + 32, cub ? "Σ e(x³/p), x = 0 … p−1, scaled by 1/√p" : "Σ e(θ_x), θ_x uniform random, scaled by 1/√p",
                        14, pal::dim);
                if (F.drawn >= F.total) {
                    Vec2 o = F.px(0, 0);
                    Vec2 e = F.px(F.path[2 * F.total], F.path[2 * F.total + 1]);
                    pn.line(fx + o.x, fy + o.y, fx + e.x, fy + e.y, 2.5f, pal::text, 0.9f);
                    pn.glow(fx + e.x, fy + e.y, 16, cub ? pal::with : pal::without, 0.9f);
                    pn.circle(fx + e.x, fy + e.y, 4, rgb(0xffffff));
                    TextStyle st; st.size = 16; st.backdrop = true; st.font = Font::Mono; st.align = Align::Right;
                    st.color = F.value >= 0 ? pal::good : pal::bad;
                    pn.text(fx + Feature::W - 12, fy + Feature::H - 34, fmt("S_p/(2√p) = %+.3f", F.value), st);
                } else {
                    Vec2 t = F.px(F.path[2 * F.drawn], F.path[2 * F.drawn + 1]);
                    pn.glow(fx + t.x, fy + t.y, 14, rgb(0xffffff), 0.7f);
                }
            }
            // thumbnails
            for (int k = 0; k < 6; ++k) {
                float tx = 18 + k * 155.f, ty = 412;
                if (k < thumb_n) draw_thumb(pn, ths[sd][k], tx, ty, 145, 150, cub ? pal::with : pal::without);
                else pn.fill_rounded_rect(tx, ty, 145, 150, 8, rgb(0x10131b));
            }
            // plot
            plot_axes(pn, pf, cub ? "running total M(X) = Σ S_p/(2√p)  vs  proved asymptotic"
                                  : "running total under the random-arrow model  (mean 0, band ±1σ, ±2σ)");
            const size_t m = S.s.X.size();
            if (m >= 2) {
                // model band (both panels, faint on the right)
                {
                    const int B = 120;
                    for (int band = 2; band >= 1; --band) {
                        for (int i = 0; i < B; ++i) {
                            double xa = pf.X1 * i / B, xb = pf.X1 * (i + 1) / B;
                            size_t na = std::upper_bound(primes.begin(), primes.begin() + m, int(xa)) - primes.begin();
                            size_t nb = std::upper_bound(primes.begin(), primes.begin() + m, int(xb)) - primes.begin();
                            double sa = band * std::sqrt(na / 2.0), sb = band * std::sqrt(nb / 2.0);
                            Vec2 a0 = P(pf, xa, -sa), a1 = P(pf, xa, sa), b0 = P(pf, xb, -sb), b1 = P(pf, xb, sb);
                            Vec2 q[4] = {a1, b1, b0, a0};
                            pn.polygon(q, 4, cub ? pal::dim : pal::without, cub ? 0.05f : (band == 2 ? 0.10f : 0.13f), false);
                        }
                    }
                }
                if (!cub) {
                    std::vector<double> Y(m);
                    for (int k = 0; k < NREP; ++k) {
                        for (size_t i = 0; i < m; ++i) Y[i] = S.rep_hist[i * NREP + k];
                        polyline_series(pn, pf, S.s.X, Y, 1.0f, pal::dim, 0.22f, false);
                    }
                    polyline_series(pn, pf, S.s.X, S.s.M, 2.2f, pal::without, 0.95f, true);
                    // ghost: the true cubic total (computed on the right)
                    polyline_series(pn, pf, Rt.s.X, Rt.s.M, 1.6f, pal::text, 0.55f, false);
                    Vec2 e = P(pf, Rt.s.X.back(), Rt.s.M.back());
                    pn.text(e.x - 6, e.y - 22, "actual cubic M(X)", 14, pal::text, Font::Sans, Align::Right);
                    Vec2 r = P(pf, S.s.X.back(), S.s.M.back());
                    { TextStyle st; st.size = 14; st.color = pal::without; st.align = Align::Right; st.backdrop = true; st.pad = 3; st.backdrop_alpha = 0.75f;
                      pn.text(r.x - 6, r.y + 10, "random-arrow walks", st); }
                } else {
                    std::vector<double> mm(modelM.begin(), modelM.begin() + m);
                    polyline_series(pn, pf, S.s.X, mm, 1.4f, pal::violet, 0.75f, false);
                    dashed_curve(pn, pf, 60, pf.X1, prediction, pal::warn, 2.4f);
                    polyline_series(pn, pf, S.s.X, S.s.M, 2.4f, pal::with, 1.f, true);
                    Vec2 e = P(pf, pf.X1, prediction(pf.X1));
                    { TextStyle st; st.size = 14; st.color = pal::warn; st.font = Font::Bold; st.align = Align::Right; st.backdrop = true; st.pad = 3; st.backdrop_alpha = 0.75f;
                      pn.text(e.x - 4, e.y + 18, "K·X^(5/6)/log X  (proved)", st); }
                    Vec2 r = P(pf, S.s.X.back(), S.s.M.back());
                    pn.text(r.x - 4, r.y - 34, "M(X)", 15, pal::with, Font::Bold, Align::Right);
                }
                // legend line
                if (cub) pn.text(pf.x0 + 6, pf.y0 + 4, "violet: elementary model Σ c*·p^(−1/6) (same asymptotic) · grey band: random-arrow ±2σ", 12, pal::dim);
                else pn.text(pf.x0 + 6, pf.y0 + 4, "grey: 48 runs of S_p/(2√p) = cos θ_p, θ uniform · coral: random-arrow walk control", 12, pal::dim);
            }
            // HUD
            pn.metric("X (primes p ≡ 1 mod 3 ≤ X)", X, "%.0f");
            if (!cub) {
                pn.metric_text("model prediction for M(X)", fmt("0 ± %.1f (1σ)", sigma), Tone::Warn);
                pn.metric("random-arrow total", L.M, "%+.1f");
                pn.metric("actual cubic M(X) (right)", Mtrue, "%.1f");
                pn.metric("true M(X) in model σ", sigma > 0 ? Mtrue / sigma : 0, "%+.1f σ", Tone::Bad);
                pn.metric("phasors summed", double(L.phasors), "si");
            } else {
                pn.metric("M(X) = Σ S_p/(2√p)", Mtrue, "%.1f", Tone::Accent);
                pn.metric("prediction K·X^(5/6)/log X", pred, "%.1f", Tone::Warn);
                pn.metric("ratio M / prediction", pred > 0 ? Mtrue / pred : 0, "%.3f", Tone::Good);
                pn.metric("max |Im S_p| (float error)", Rt.max_im, "%.1e");
                pn.metric("phasors summed", double(Rt.phasors), "si");
            }
        }
    }
    size_t n = Rt.val.size();
    double Xf = X_of(h.frames() - 1);
    h.result("X_final", Xf);
    h.result("primes_1mod3", double(n));
    h.result("M_true", Rt.M);
    h.result("prediction_KX56_logX", prediction(Xf));
    h.result("ratio_M_over_prediction", Rt.M / prediction(Xf));
    h.result("elementary_model_sum", n ? modelM[n - 1] : 0);
    h.result("random_model_sigma", std::sqrt(n / 2.0));
    h.result("true_M_in_model_sigma", Rt.M / std::sqrt(n / 2.0));
    h.result("random_arrow_walk_total", L.M);
    h.result("max_abs_im", Rt.max_im);
    h.result("K", K_RAT);
    {
        // check value quoted by the verifier: M(8191)
        double m = 0;
        for (size_t i = 0; i < n && primes[i] <= 8191; ++i) m += Rt.val[i];
        h.result("M_8191", m);
        h.result("prediction_8191", prediction(8191));
    }
    return h.finish();
}
