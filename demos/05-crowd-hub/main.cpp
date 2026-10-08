// 05-crowd-hub - "a tiny hub move can reassign a much larger crowd" (lead G10.3).
//
// Family 374, "Sharp One-Third Stability of Brenier Maps",
//   preprints/Sharp-One-Third-Stability-of-Brenier-Maps-September-25-2026/build/source/
//   sections/sharpness.tex: Lemma cells:optimality, Prop. sharpness:exponent,
//   eqs sharpness:potentials, sharpness:targets, sharpness:target-distance, sharpness:map-distance;
//   sections/gradient.tex eq gradient:constant (explicit C*), introduction.tex Thm main:stability.
//
// A uniform crowd on the square K = [-1,1]^2 is sent to three destinations: -e1, +e1 and a
// central hub. Assignment = gradient of a finite affine maximum (power / Laguerre cells):
//     u_a(x)  = max{-x1, x1, a}             hub at 0        (sharpness:potentials)
//     u~_a(x) = max{-x1, x1, a + s*b*x2}    hub at s*b*e2,  b = a/2, s in [0,1]
// By Lemma cells:optimality the gradient of u~_a is THE optimal (quadratic-cost) assignment to
// its own image law; all three destination masses ((1-a)/2, (1-a)/2, a) stay fixed for every s.
//
//   LEFT  (WITHOUT): frozen labels. Agents keep the label they got when the hub was at 0; the
//                    hub then moves. Same destination masses (feasible), but no longer optimal.
//   RIGHT (WITH):    recompute the optimal labels every frame from the paper's explicit
//                    three-atom family (argmax of the three affine functions).
// Every HUD number is computed by exact slice integration (piecewise polynomial in x2,
// Gauss-Legendre exact), never from the agent sample, so the thin changed strips cannot create
// sampling artefacts. The sampled-agent counts are only reported as a cross-check.
//
// This is a sensitivity WARNING, not a faster allocator: the hub's destination moves by
// W2 = sqrt(a)*s*b, but the optimal assignment changes on a fraction s*b/2 of the crowd,
// i.e. RMS map change ~ W2^(1/3) (sharp), while frozen labels have RMS change = W2 exactly
// and pay a transport-cost excess of (s*b)^2/3 (= a^2/12 at s = 1).
#include "demo.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

using namespace demo;

// ------------------------------------------------------------------ exact integrals
struct Exact {
    double changed = 0;      // rho(A triangle B_s): fraction of crowd whose optimal label changes
    double rms_with = 0;     // ||T_opt(s) - T_opt(0)||_L2(rho)
    double rms_frozen = 0;   // ||T_frozen(s) - T_opt(0)||_L2(rho)
    double w2 = 0;           // W2 between the two destination laws
    double cost_opt = 0, cost_frozen = 0;  // quadratic transport cost of each policy
};

// Quadratic cost on one x2-slice t (x1 uniform on [-1,1], density 1/2): central interval
// [-w, w] -> hub (0, hy), outer pieces -> (+-1, 0).
static double slice_cost(double w, double t, double hy) {
    double c = 2 * w * w * w / 3 + 2 * w * (t - hy) * (t - hy);
    double o = (1 - w);
    c += 2 * (o * o * o / 3 + o * t * t);
    return 0.5 * c;
}

static Exact exact(double a, double s) {
    const double b = a / 2, hy = s * b;
    // 5-point Gauss-Legendre on [-1,0] and [0,1]: integrands are polynomials of degree <= 3
    // on each half (the |t| kink sits at 0), so this is exact.
    static const double gx[5] = {-0.9061798459386640, -0.5384693101056831, 0.0, 0.5384693101056831,
                                 0.9061798459386640};
    static const double gw[5] = {0.2369268850561891, 0.4786286704993665, 0.5688888888888889, 0.4786286704993665,
                                 0.2369268850561891};
    Exact e;
    double ch = 0, r2w = 0, r2f = 0, co = 0, cf = 0;
    for (int half = 0; half < 2; ++half)
        for (int k = 0; k < 5; ++k) {
            double t = half == 0 ? -0.5 + 0.5 * gx[k] : 0.5 + 0.5 * gx[k];
            double wt = 0.5 * gw[k] * 0.5;  // interval Jacobian 1/2, x2 density 1/2
            double w0 = a, w1 = a + hy * t;
            double d = std::fabs(w1 - w0);
            ch += wt * d;                          // P(label changes | slice) = 2*d*(1/2)
            r2w += wt * (d + hy * hy * w1);        // 1_{A tri B} + (s b)^2 1_B  (sharpness:map-distance)
            r2f += wt * (hy * hy * w0);            // frozen: only the hub moved under A
            co += wt * slice_cost(w1, t, hy);
            cf += wt * slice_cost(w0, t, hy);
        }
    e.changed = ch;
    e.rms_with = std::sqrt(r2w);
    e.rms_frozen = std::sqrt(r2f);
    e.w2 = std::sqrt(a) * hy;  // sharpness:target-distance with b -> s*b
    e.cost_opt = co;
    e.cost_frozen = cf;
    return e;
}

// ------------------------------------------------------------------ crowd
struct Crowd {
    int n = 0;
    std::vector<float> x, y, ph;       // home position in K, stream phase
    std::vector<uint8_t> lab;          // 0: -e1, 1: +e1, 2: hub
    std::vector<uint8_t> ref;          // optimal label with hub at rest (s = 0)
    std::vector<float> sx, sy;         // streaming particle positions (subset)
    int stride = 4;                    // every stride-th agent emits a visible stream particle
};

static Crowd make_crowd(int side, unsigned seed) {
    Crowd c;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> U(0.f, 1.f);
    c.n = side * side;
    c.x.resize(c.n); c.y.resize(c.n); c.ph.resize(c.n);
    for (int j = 0; j < side; ++j)
        for (int i = 0; i < side; ++i) {
            int k = j * side + i;  // jittered (stratified) uniform sample of the square
            c.x[k] = -1.f + 2.f * (i + U(rng)) / side;
            c.y[k] = -1.f + 2.f * (j + U(rng)) / side;
            c.ph[k] = U(rng);
        }
    c.lab.assign(c.n, 0);
    c.ref.assign(c.n, 0);
    c.sx.assign(c.n / c.stride + 1, 0);
    c.sy.assign(c.n / c.stride + 1, 0);
    return c;
}

// Laguerre / power-cell label: argmax over the three affine functions x.y_i + c_i
// (first maximizing index on ties, as in Lemma cells:optimality).
static inline uint8_t label_of(float x1, float x2, float a, float hy) {
    float l0 = -x1, l1 = x1, l2 = a + hy * x2;
    uint8_t k = 0;
    float m = l0;
    if (l1 > m) { m = l1; k = 1; }
    if (l2 > m) k = 2;
    return k;
}

static inline void dest(uint8_t lab, float hy, float& tx, float& ty) {
    if (lab == 0) { tx = -1; ty = 0; }
    else if (lab == 1) { tx = 1; ty = 0; }
    else { tx = 0; ty = hy; }
}

// Stream particles: displacement interpolation x + tau (T(x) - x), tau cycling in [0,1).
static void stream(Crowd& c, float hy, float time) {
    int m = 0;
    for (int i = 0; i < c.n; i += c.stride, ++m) {
        float tau = c.ph[i] + time * 0.42f;
        tau -= std::floor(tau);
        float tx, ty;
        dest(c.lab[i], hy, tx, ty);
        c.sx[m] = c.x[i] + tau * (tx - c.x[i]);
        c.sy[m] = c.y[i] + tau * (ty - c.y[i]);
    }
}

// ------------------------------------------------------------------ schedule
struct Sched {
    int cycles = 12, cyc_len = 125;
    double a_hi = 0.45, a_lo = 0.02;
    double a_of(int k) const { return a_hi * std::pow(a_lo / a_hi, double(k) / (cycles - 1)); }
    static double ease(double u) { u = std::clamp(u, 0.0, 1.0); return u * u * (3 - 2 * u); }
    // returns (a, s, cycle, record-flag)
    void at(int frame, double& a, double& s, int& k, bool& rec) const {
        k = std::min(cycles - 1, frame / cyc_len);
        int f = frame - k * cyc_len;
        double a0 = a_of(std::max(0, k - 1)), a1 = a_of(k);
        const double u = double(f) / cyc_len;
        a = std::exp(std::log(a0) + (std::log(a1) - std::log(a0)) * ease(u / 0.18));
        if (u < 0.2) s = 0;
        else if (u < 0.48) s = ease((u - 0.2) / 0.28);
        else if (u < 0.8) s = 1;
        else s = 1 - ease((u - 0.8) / 0.2);
        rec = f == int(0.64 * cyc_len);
    }
};

// ------------------------------------------------------------------ drawing
static const Color C_LEFT = rgb(0x6cb6ff), C_RIGHT = rgb(0xb392f0), C_HUB = rgb(0xffa657);
static Color lab_color(int l) { return l == 0 ? C_LEFT : (l == 1 ? C_RIGHT : C_HUB); }

struct Geo {  // square K on screen
    float X0 = 210, Y0 = 34, S = 268;
    Vec2 operator()(double x, double y) const { return {float(X0 + (x + 1) * S), float(Y0 + (1 - y) * S)}; }
};

struct LogPt { double w2, rms; };

static void draw_loglog(Panel& p, bool right, const std::vector<LogPt>& mine, const std::vector<LogPt>& other,
                        LogPt live, double C_star) {
    const float X = 482, Y = 590, W = 458, H = 344;
    p.fill_rounded_rect(X, Y, W, H, 10, rgb(0x05070b), 0.84f);
    p.fill_rect(X + 10, Y, W - 20, 2, right ? pal::with : pal::without, 0.9f);
    p.text(X + 14, Y + 10, "map change vs target change (log-log, exact)", 15, pal::dim, Font::Mono);
    const float px0 = X + 60, px1 = X + W - 16, py0 = Y + 40, py1 = Y + H - 44;
    const double lx0 = -3.4, lx1 = -0.6, ly0 = -3.4, ly1 = 0.0;
    auto M = [&](double w2, double r) {
        double u = (std::log10(w2) - lx0) / (lx1 - lx0), v = (std::log10(r) - ly0) / (ly1 - ly0);
        return Vec2{float(px0 + u * (px1 - px0)), float(py1 - v * (py1 - py0))};
    };
    for (int e = -3; e <= 0; ++e) {
        Vec2 q = M(std::pow(10.0, e), 1e-3);
        if (q.x >= px0 - 1 && q.x <= px1 + 1) {
            p.line(q.x, py0, q.x, py1, 1, pal::grid, 0.9f);
            p.text(q.x, py1 + 4, fmt("1e%d", e), 13, pal::dim, Font::Mono, Align::Center);
        }
        Vec2 r = M(1e-3, std::pow(10.0, e));
        if (r.y >= py0 - 1 && r.y <= py1 + 1) {
            p.line(px0, r.y, px1, r.y, 1, pal::grid, 0.9f);
            p.text(px0 - 6, r.y - 8, fmt("1e%d", e), 13, pal::dim, Font::Mono, Align::Right);
        }
    }
    p.text((px0 + px1) / 2, py1 + 20, "W2(old targets, new targets)", 14, pal::dim, Font::Sans, Align::Center);
    p.text(X + 14, py1 + 20, "RMS map change ↑", 13, pal::dim, Font::Sans);
    // reference slopes (labels at the left end, where the lines are well separated)
    auto ref_line = [&](double slope, double x_anchor, double y_anchor, Color c, const char* lab, float dy) {
        double xa = std::pow(10.0, lx0), xb = std::pow(10.0, lx1);
        double ya = y_anchor * std::pow(xa / x_anchor, slope), yb = y_anchor * std::pow(xb / x_anchor, slope);
        Vec2 A = M(xa, ya), B = M(xb, yb);
        if (B.y < py0) { double f = (A.y - py0) / (A.y - B.y); B = {float(A.x + f * (B.x - A.x)), py0}; }
        p.line(A.x, A.y, B.x, B.y, 1.2f, c, 0.5f);
    };


    ref_line(0.5, 1e-3, 0.15, pal::dim, "", 0);
    ref_line(1.0 / 3, 1e-3, 0.03, pal::with, "", 0);
    p.fill_rounded_rect(px0 + 4, py0 - 4, 300, 64, 6, rgb(0x05070b), 0.92f);
    auto leg = [&](float dy, Color c, const char* lab) {
        p.line(px0 + 10, py0 + 8 + dy, px0 + 34, py0 + 8 + dy, 2, c, 0.8f);
        p.text(px0 + 40, py0 + dy, lab, 13, c, Font::Mono);
    };
    leg(0, pal::dim, "slope 1/2 (Letrouit's conjecture)");
    leg(18, pal::with, "slope 1/3 (proved sharp)");
    leg(36, pal::without, "slope 1 (frozen: RMS = W2)");


    // analytic curve of the paper family (s = 1): optimal RMS = sqrt(a) sqrt(1+a^2) / 2
    {
        Vec2 prev{};
        for (int i = 0; i <= 120; ++i) {
            double a = 0.0087 * std::pow(0.49 / 0.0087, i / 120.0);
            Exact e = exact(a, 1.0);
            Vec2 q = M(e.w2, e.rms_with);
            if (i) p.line(prev.x, prev.y, q.x, q.y, 1.5f, pal::with, right ? 0.45f : 0.18f);
            prev = q;
        }
        Vec2 a0 = M(1e-3 * 0.4, 1e-3 * 0.4), a1 = M(0.25, 0.25);
        p.line(a0.x, a0.y, a1.x, a1.y, 1.5f, pal::without, right ? 0.18f : 0.45f);
    }
    // other panel's points faint, own points bright
    Color mc = right ? pal::with : pal::without, oc = right ? pal::without : pal::with;
    for (auto& q : other) { Vec2 v = M(q.w2, q.rms); p.circle(v.x, v.y, 3, oc, 0.3f); }
    for (size_t i = 0; i < mine.size(); ++i) {
        Vec2 v = M(mine[i].w2, mine[i].rms);
        if (i) { Vec2 u = M(mine[i - 1].w2, mine[i - 1].rms); p.line(u.x, u.y, v.x, v.y, 2, mc, 0.8f); }
        p.glow(v.x, v.y, 10, mc, 0.5f);
        p.circle(v.x, v.y, 4, mc);
    }
    if (live.w2 > 0) {
        Vec2 v = M(live.w2, live.rms);
        p.ring(v.x, v.y, 7, 1.5f, pal::text, 0.9f);
    }
    p.text(px1 - 4, py1 - 40, fmt("theorem: RMS ≤ C*·W2^(1/3), C* = %.1f", C_star), 13, pal::dim, Font::Mono,
           Align::Right);
    p.text(px1 - 4, py1 - 22,
           right ? fmt("here RMS / W2^(1/3) = %.2f", live.w2 > 0 ? live.rms / std::cbrt(live.w2) : 0.0)
                 : fmt("here RMS / W2^(1/3) = %.3f", live.w2 > 0 ? live.rms / std::cbrt(live.w2) : 0.0),
           13, right ? pal::with : pal::without, Font::Mono, Align::Right);
}

static void draw_scene(Panel& p, const Crowd& c, bool right, double a, double s, double t, const Exact& e) {
    p.clear(pal::bg);
    Geo g;
    const double b = a / 2, hy = s * b;
    Vec2 k0 = g(-1, 1), k1 = g(1, -1);
    p.fill_rect(k0.x, k0.y, k1.x - k0.x, k1.y - k0.y, rgb(0x10141d), 1);
    // home dots of the crowd, colored by current label; mismatched agents highlighted
    const Color hi = right ? rgb(0xfff2a8) : pal::without;
    for (int i = 0; i < c.n; ++i) {
        Vec2 q = g(c.x[i], c.y[i]);
        bool odd = c.lab[i] != (right ? c.ref[i] : label_of(c.x[i], c.y[i], float(a), float(hy)));
        if (odd) p.splat(q.x, q.y, hi, 0.95f);
        else p.splat(q.x, q.y, lab_color(c.lab[i]), 0.42f);
    }
    // the two changed strips A triangle B (exact polygons): above x2 = 0 B grows, below it shrinks
    if (hy > 1e-6) {
        Color sc = right ? rgb(0xffe066) : pal::without;
        for (int sg = -1; sg <= 1; sg += 2) {
            Vec2 t0 = g(sg * a, 0), t1 = g(sg * a, 1), t2 = g(sg * (a + hy), 1);
            Vec2 u1 = g(sg * a, -1), u2 = g(sg * (a - hy), -1);
            p.triangle(t0, t1, t2, sc, 0.22f);
            p.triangle(t0, u1, u2, sc, 0.22f);
        }
    }
    // cell boundaries: the optimal boundary |x1| = a + s b x2 and the frozen one |x1| = a
    for (int sg = -1; sg <= 1; sg += 2) {
        Vec2 f0 = g(sg * a, -1), f1 = g(sg * a, 1);
        Vec2 o0 = g(sg * (a - hy), -1), o1 = g(sg * (a + hy), 1);
        if (right) {
            p.line(f0.x, f0.y, f1.x, f1.y, 1, pal::dim, 0.35f);
            p.line(o0.x, o0.y, o1.x, o1.y, 2.2f, pal::with, 0.95f);
        } else {
            p.line(o0.x, o0.y, o1.x, o1.y, 1, pal::with, 0.35f);
            p.line(f0.x, f0.y, f1.x, f1.y, 2.2f, pal::without, 0.95f);
        }
    }
    p.stroke_rect(k0.x - 1, k0.y - 1, k1.x - k0.x + 2, k1.y - k0.y + 2, 1.5f, pal::grid);
    // streaming agents (displacement interpolation toward their destination), additive
    p.blend = Blend::Add;
    int m = 0;
    for (int i = 0; i < c.n; i += c.stride, ++m) {
        float tau = c.ph[i] + float(t) * 0.42f;
        tau -= std::floor(tau);
        float al = std::sin(3.14159f * tau);
        Vec2 q = g(c.sx[m], c.sy[m]);
        bool odd = c.lab[i] != (right ? c.ref[i] : label_of(c.x[i], c.y[i], float(a), float(hy)));
        if (odd) p.circle(q.x, q.y, 1.6f, hi, 0.9f * al);
        else p.splat(q.x, q.y, lerp(lab_color(c.lab[i]), rgb(0xffffff), 0.25f), 0.33f * al);
    }
    p.blend = Blend::Normal;
    // destinations (atoms), glow radius ~ sqrt(mass)
    Vec2 dl = g(-1, 0), dr = g(1, 0), dh = g(0, hy), d0 = g(0, 0);
    float mo = float((1 - a) / 2), mh = float(a);
    p.glow(dl.x, dl.y, 18 + 70 * std::sqrt(mo), C_LEFT, 0.9f);
    p.glow(dr.x, dr.y, 18 + 70 * std::sqrt(mo), C_RIGHT, 0.9f);
    p.glow(dh.x, dh.y, 14 + 70 * std::sqrt(mh), C_HUB, 1.0f);
    p.ring(d0.x, d0.y, 6, 1.2f, pal::dim, 0.8f);
    p.circle(dl.x, dl.y, 5, rgb(0xffffff));
    p.circle(dr.x, dr.y, 5, rgb(0xffffff));
    p.circle(dh.x, dh.y, 5, rgb(0xffffff));
    if (hy > 1e-4) p.line(d0.x, d0.y, dh.x, dh.y, 1.5f, pal::text, 0.8f);
    // labels
    TextStyle st;
    st.size = 15; st.font = Font::Mono; st.backdrop = true; st.backdrop_alpha = 0.55f; st.pad = 3;
    st.color = C_LEFT;
    p.text(dl.x + 14, dl.y + 22, fmt("−e₁  mass %.3f", mo), st);
    st.color = C_RIGHT; st.align = Align::Right;
    p.text(dr.x - 14, dr.y + 22, fmt("+e₁  mass %.3f", mo), st);
    st.size = 15; st.font = Font::Mono; st.color = C_HUB; st.backdrop = true; st.backdrop_alpha = 0.55f; st.pad = 3;
    st.align = Align::Center;
    p.text(dh.x, dh.y - 44, fmt("hub  mass a = %.3f", a), st);
    // status line above the square
    std::string top = fmt("a = %.4f   b = a/2 = %.4f   hub moved s·b = %.5f   (s = %.2f)", a, b, hy, s);
    p.text(480, 8, top, 16, pal::text, Font::Mono, Align::Center);
    std::string what = right ? fmt("optimal cells recomputed → %.2f%% of crowd switches destination", 100 * e.changed)
                             : fmt("labels frozen → %.2f%% of crowd now routed sub-optimally", 100 * e.changed);
    TextStyle s2;
    s2.size = 17; s2.font = Font::Bold; s2.color = right ? rgb(0xffe066) : pal::without; s2.align = Align::Center;
    s2.backdrop = true; s2.backdrop_alpha = 0.6f;
    p.text(480, 572 - 18, what, s2);
}

int main(int argc, char** argv) {
    Config cfg;
    cfg.name = "05-crowd-hub";
    cfg.title_left = "frozen labels, hub moved";
    cfg.title_right = "recomputed optimal (Brenier) labels";
    cfg.caption = "Family 374 — Sharp One-Third Stability of Brenier Maps · WITH = explicit three-atom family "
                  "(sharpness.tex eq sharpness:potentials, :target-distance, :map-distance), metrics by exact "
                  "slice integration · a stability WARNING, not a faster allocator";
    cfg.frames = 1500;
    Harness h(argc, argv, cfg);

    const int side = arg_int(argc, argv, "--side", 420);
    const unsigned seed = (unsigned)arg_int(argc, argv, "--seed", 374);
    Crowd L = make_crowd(side, seed), R = L;
    Sched sc;
    const int cycles_needed = (h.frames() + sc.cyc_len - 1) / sc.cyc_len;
    if (cycles_needed < sc.cycles) {  // short runs (preview): compress the a-sweep into the frames we have
        sc.cyc_len = std::max(20, h.frames() / sc.cycles);
    }

    // explicit admissible constant, gradient.tex eq gradient:constant, for K = Y = [-1,1]^2:
    // d = 2, |x| <= R = sqrt2 on K, Y in B(0, L = sqrt2), P_K = 2 + 2, |K| = 4, C0 = 162.
    const double d = 2, Rr = std::sqrt(2.0), Ll = std::sqrt(2.0), PK = 4, volK = 4, C0 = 162;
    const double C_star = std::sqrt(12 * d * Rr * Rr * std::pow(1 + std::sqrt(C0), 2) + 28 * Ll * Ll * PK / volK);

    std::vector<LogPt> ptsL, ptsR;
    double max_ratio_third = 0, max_excess_err = 0, max_sample_err = 0;
    double last_slopeL = NAN, last_slopeR = NAN;
    std::vector<double> rec_a;

    h.left().set_compute_label("allocate");
    h.right().set_compute_label("allocate");
    h.left().sparkline("RMS map change");
    h.right().sparkline("RMS map change");
    h.left().set_hud_corner(Corner::BottomLeft);
    h.right().set_hud_corner(Corner::BottomLeft);

    while (h.next_frame()) {
        double a, s;
        int k;
        bool rec;
        sc.at(h.frame(), a, s, k, rec);
        const float fa = float(a), hy = float(s * a / 2), t = float(h.time());

        // ---- WITHOUT: labels are (re)assigned only while the hub is at rest; then frozen.
        h.left().measure([&] {
            if (s == 0)
                for (int i = 0; i < L.n; ++i) L.lab[i] = label_of(L.x[i], L.y[i], fa, 0.f);
            stream(L, hy, t);
        });
        // ---- WITH: optimal power-cell labels for the current hub, every frame.
        h.right().measure([&] {
            for (int i = 0; i < R.n; ++i) R.lab[i] = label_of(R.x[i], R.y[i], fa, hy);
            stream(R, hy, t);
        });

        // ---- bookkeeping (untimed): reference labels at s = 0, exact metrics, sample cross-check
        for (int i = 0; i < R.n; ++i) R.ref[i] = label_of(R.x[i], R.y[i], fa, 0.f);
        Exact e = exact(a, s);
        long long nchg = 0;
        for (int i = 0; i < R.n; ++i) nchg += R.lab[i] != R.ref[i];
        double frac_sample = double(nchg) / R.n;
        max_sample_err = std::max(max_sample_err, std::fabs(frac_sample - e.changed));
        double excess = e.cost_frozen - e.cost_opt, excess_pred = (s * a / 2) * (s * a / 2) / 3;
        max_excess_err = std::max(max_excess_err, std::fabs(excess - excess_pred));
        if (e.w2 > 0) max_ratio_third = std::max(max_ratio_third, e.rms_with / std::cbrt(e.w2));
        if (rec) {
            ptsL.push_back({e.w2, e.rms_frozen});
            ptsR.push_back({e.w2, e.rms_with});
            rec_a.push_back(a);
            size_t n = ptsR.size();
            if (n >= 2) {
                double dx = std::log(ptsR[n - 1].w2) - std::log(ptsR[n - 2].w2);
                last_slopeR = (std::log(ptsR[n - 1].rms) - std::log(ptsR[n - 2].rms)) / dx;
                last_slopeL = (std::log(ptsL[n - 1].rms) - std::log(ptsL[n - 2].rms)) / dx;
            }
            h.result(fmt("cycle%02d_a", int(n - 1)), a);
            h.result(fmt("cycle%02d_W2", int(n - 1)), e.w2);
            h.result(fmt("cycle%02d_rms_optimal", int(n - 1)), e.rms_with);
            h.result(fmt("cycle%02d_rms_frozen", int(n - 1)), e.rms_frozen);
            h.result(fmt("cycle%02d_changed_fraction", int(n - 1)), e.changed);
            h.result(fmt("cycle%02d_cost_excess_frozen", int(n - 1)), excess);
            h.result(fmt("cycle%02d_a2_over_12", int(n - 1)), a * a / 12);
            h.result(fmt("cycle%02d_sampled_changed_fraction", int(n - 1)), frac_sample);
        }

        // ---- draw (untimed)
        draw_scene(h.left(), L, false, a, s, t, e);
        draw_scene(h.right(), R, true, a, s, t, e);
        LogPt liveL{e.w2, e.rms_frozen}, liveR{e.w2, e.rms_with};
        draw_loglog(h.left(), false, ptsL, ptsR, liveL, C_star);
        draw_loglog(h.right(), true, ptsR, ptsL, liveR, C_star);

        // ---- HUD
        auto hud = [&](Panel& p, bool right) {
            p.metric("W2(old, new destination laws)", e.w2, "%.2e");
            p.metric("RMS map change", right ? e.rms_with : e.rms_frozen, "%.2e", right ? Tone::Warn : Tone::Good);
            p.metric("agents relabelled (exact)", 100 * (right ? e.changed : 0.0), "%.2f %%", right ? Tone::Warn : Tone::Good);
            p.metric("cost excess vs optimal", right ? 0.0 : excess, "%.2e", right ? Tone::Good : Tone::Bad);
            double sl = right ? last_slopeR : last_slopeL;
            if (std::isnan(sl)) p.metric_text("log-log slope RMS vs W2", "(after 2 cycles)");
            else p.metric("log-log slope RMS vs W2", sl, "%.3f", right ? Tone::Warn : Tone::Neutral);
        };
        hud(h.left(), false);
        hud(h.right(), true);
    }

    h.result("agents", L.n);
    h.result("C_star_gradient_constant", C_star);
    h.result("max_rms_over_W2_cuberoot_optimal", max_ratio_third);
    h.result("max_abs_cost_excess_minus_(sb)^2/3", max_excess_err);
    h.result("max_abs_sampled_minus_exact_changed_fraction", max_sample_err);
    if (!std::isnan(last_slopeR)) {
        h.result("final_loglog_slope_optimal", last_slopeR);
        h.result("final_loglog_slope_frozen", last_slopeL);
    }
    h.result("note", "metrics are exact slice integrals; sampled agents only cross-check the changed fraction");
    return h.finish();
}
