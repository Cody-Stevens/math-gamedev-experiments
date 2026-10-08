// 02-fluid-gates - incompressible "fluid gates" that swap and reshape dye regions (lead G10.2).
//
// Family 376, "Finite Instructions and Solenoidal Shear Flows",
//   preprints/Finite-Instructions-and-Solenoidal-Shear-Flows-September-27-2026/build/main.tex
//   sec:shears, Lemma lem:excursion (eq:shears), Theorem thm:shears, eqs eq:A, eq:Z, eq:fields,
//   eq:velocity.
//
// Three dye regions on the flat 3-torus T_3^3 (L = 3) are routed by a finite rule table
// F_i(X) = q_i + diag(lambda_i, 1/lambda_i)(X - p_i):
//     branch 1:  slot A -> slot B, lambda = 5/2     (tall -> wide)
//     branch 2:  slot B -> slot A, lambda = 1/2     (sources and targets of 1 and 2 overlap)
//     branch 3:  slot C -> slot C, lambda = 3       (reshaped in place: P_3 and Q_3 overlap)
// Period 1 runs this table, period 2 runs the inverse table (F_i^-1, same construction), so
// every region should come home.
//
//   LEFT  (WITHOUT): what a game would tween in one plane. Period 1: direct endpoint
//          interpolation X(th) = lerp(X, F(X)) (area inflates mid-route; the swapped regions
//          collide). Period 2: the stronger classical baseline, exponential reciprocal scaling
//          diag(lambda^th, lambda^-th) along the straight center path: exactly area preserving,
//          but still one plane, so the swapping branches still pass through each other.
//   RIGHT (WITH): the paper's smooth velocity V(t,X) = sum_j b_j(t) W_j(X) (eq:velocity) with
//          the seven fields of eq:fields: lift each branch to a private height z_i (source masks
//          A_i^-, eq:A), four shears Y(-l), X(1/l - 1), Y(1), X(l - 1) (eq:shears) at that
//          height (height masks Z_i, eq:Z), translate, lower with target masks A_i^+.
//          Dye particles are advected by RK4 on the SAMPLED velocity field; the exact stage maps
//          (each W_j is constant along its own stage's trajectories) serve as a reference.
//
// HUD: area / material-volume drift, branch overlap (same layer), divergence residual,
// sup excursion from the branch center (lem:excursion: <= 2h during the scaling stages),
// error vs exact flow map. Compute = advecting the same region particles + boundaries +
// volume tracers on both sides (ambient dye on the right is visualization only, untimed).
// This is a construction demo, not a Navier-Stokes solver benchmark.
#include "demo.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <thread>
#include <vector>

using namespace demo;

// ------------------------------------------------------------------ smooth profiles (sec:shears)
static inline double Ef(double t) { return t > 0 ? std::exp(-1.0 / t) : 0.0; }
static inline double Sf(double t) {
    if (t <= 0) return 0;
    if (t >= 1) return 1;
    double a = Ef(t), b = Ef(1 - t);
    return a / (a + b);
}
static inline double Sp(double t) {  // S'(t)
    if (t <= 0 || t >= 1) return 0;
    double a = Ef(t), b = Ef(1 - t), da = a / (t * t), db = b / ((1 - t) * (1 - t));
    double s = a + b;
    return (da * b + a * db) / (s * s);
}
// 1-D cutoff: 1 on [lo, hi], 0 outside [lo - r, hi + r], smooth (product of two S profiles)
struct Cut {
    double lo = 0, hi = 0, r = 1;
    bool in(double x) const { return x > lo - r && x < hi + r; }
    double v(double x) const { return Sf((x - (lo - r)) / r) * Sf(((hi + r) - x) / r); }
    double d(double x) const {
        double u1 = (x - (lo - r)) / r, u2 = ((hi + r) - x) / r;
        return (Sp(u1) * Sf(u2) - Sf(u1) * Sp(u2)) / r;
    }
};

constexpr double TL = 3.0;  // torus side
static inline double wrapc(double x) { x = std::fmod(x, TL); return x < 0 ? x + TL : x; }
static inline double wrapd(double d) { return d - TL * std::round(d / TL); }

struct V3 { double x = 0, y = 0, z = 0; };

// ------------------------------------------------------------------ rule table + seven fields
struct Branch {
    double p[2], q[2], hp[2], hq[2];  // centers and half-widths of P_i, Q_i
    double lam, z;                     // reciprocal scale, private height z_i
    Cut sx, sy, tx, ty, gam;           // source collar, target collar, height cutoff
};

struct Table {
    std::vector<Branch> br;
    double z0 = 0.6, h = 0;
    Cut rx, ry;  // rho_x, rho_y: 1 on the coordinate projections of K + [-2h, 2h]^2
    void finish() {
        const double m = 0.03, r = 0.15;  // plateau margin, collar ramp
        h = 0;
        for (auto& b : br) {
            b.sx = {b.p[0] - b.hp[0] - m, b.p[0] + b.hp[0] + m, r};
            b.sy = {b.p[1] - b.hp[1] - m, b.p[1] + b.hp[1] + m, r};
            b.tx = {b.q[0] - b.hq[0] - m, b.q[0] + b.hq[0] + m, r};
            b.ty = {b.q[1] - b.hq[1] - m, b.q[1] + b.hq[1] + m, r};
            b.gam = {b.z - 0.06, b.z + 0.06, 0.16};
            h = std::max({h, b.hp[0], b.hp[1], b.hq[0], b.hq[1]});
        }
        rx = {0.25, 2.75, 0.17};
        ry = {0.70, 2.75, 0.17};
    }
    Table inverse() const {
        Table t = *this;
        for (auto& b : t.br) {
            std::swap(b.p[0], b.q[0]); std::swap(b.p[1], b.q[1]);
            std::swap(b.hp[0], b.hq[0]); std::swap(b.hp[1], b.hq[1]);
            b.lam = 1.0 / b.lam;
        }
        t.finish();
        return t;
    }
    // eq:A  A_i^-(x,y) = d/dx((x - p_i1) chi_i^-),  A_i^+ with q_i and chi_i^+
    static double Amask(const Cut& cx, const Cut& cy, double c1, double x, double y) {
        if (!cx.in(x) || !cy.in(y)) return 0;
        return cy.v(y) * (cx.v(x) + (x - c1) * cx.d(x));
    }
    // eq:Z  Z_i(z) = d/dz((z - z_i) gamma_i)
    static double Zmask(const Branch& b, double z) {
        if (!b.gam.in(z)) return 0;
        return b.gam.v(z) + (z - b.z) * b.gam.d(z);
    }
    // eq:fields  W_1..W_7 (j = 0..6)
    V3 W(int j, const V3& X) const {
        V3 v;
        for (const auto& b : br) {
            switch (j) {
                case 0: v.z += (b.z - z0) * Amask(b.sx, b.sy, b.p[0], X.x, X.y); break;
                case 6: v.z += (z0 - b.z) * Amask(b.tx, b.ty, b.q[0], X.x, X.y); break;
                default: {
                    double Z = Zmask(b, X.z);
                    if (Z == 0) break;
                    if (j == 1) v.y += -b.lam * Z * rx.v(X.x) * (X.x - b.p[0]);
                    else if (j == 2) v.x += (1.0 / b.lam - 1) * Z * ry.v(X.y) * (X.y - b.p[1]);
                    else if (j == 3) v.y += Z * rx.v(X.x) * (X.x - b.p[0]);
                    else if (j == 4) v.x += (b.lam - 1) * Z * ry.v(X.y) * (X.y - b.p[1]);
                    else { v.x += Z * (b.q[0] - b.p[0]); v.y += Z * (b.q[1] - b.p[1]); }
                }
            }
        }
        return v;
    }
};

// seven disjoint pulse intervals inside (0,1); b_j = rescaled S' (unit integral), eq:velocity
struct Pulses {
    double t0[7], len = 0.12;
    Pulses() { for (int j = 0; j < 7; ++j) t0[j] = 0.03 + 0.14 * j; }
    int active(double t) const {
        for (int j = 0; j < 7; ++j) if (t > t0[j] && t < t0[j] + len) return j;
        return -1;
    }
    double b(int j, double t) const { return Sp((t - t0[j]) / len) / len; }
    double B(int j, double t) const { return Sf((t - t0[j]) / len); }
};

static V3 velocity(const Table& T, const Pulses& P, double t, const V3& X) {
    int j = P.active(t);
    if (j < 0) return {};
    double bj = P.b(j, t);
    if (bj == 0) return {};
    V3 w = T.W(j, X);
    return {w.x * bj, w.y * bj, w.z * bj};
}

static void rk4(const Table& T, const Pulses& P, double t, double dt, V3& X) {
    auto add = [](const V3& a, const V3& k, double s) { return V3{a.x + s * k.x, a.y + s * k.y, a.z + s * k.z}; };
    V3 k1 = velocity(T, P, t, X);
    V3 k2 = velocity(T, P, t + dt / 2, add(X, k1, dt / 2));
    V3 k3 = velocity(T, P, t + dt / 2, add(X, k2, dt / 2));
    V3 k4 = velocity(T, P, t + dt, add(X, k3, dt));
    X.x = wrapc(X.x + dt / 6 * (k1.x + 2 * k2.x + 2 * k3.x + k4.x));
    X.y = wrapc(X.y + dt / 6 * (k1.y + 2 * k2.y + 2 * k3.y + k4.y));
    X.z = wrapc(X.z + dt / 6 * (k1.z + 2 * k2.z + 2 * k3.z + k4.z));
}

// exact stage maps: W_j(X) is constant along stage-j trajectories (its only component does not
// depend on the coordinate it moves), so X(t1) = X(t0) + W_j(X(t0)) (B_j(t1) - B_j(t0)).
static void exact_step(const Table& T, const Pulses& P, double ta, double tb, V3& X) {
    for (int j = 0; j < 7; ++j) {
        double dB = P.B(j, tb) - P.B(j, ta);
        if (dB == 0) continue;
        V3 w = T.W(j, X);
        X = {wrapc(X.x + w.x * dB), wrapc(X.y + w.y * dB), wrapc(X.z + w.z * dB)};
    }
}

// ------------------------------------------------------------------ parallel for
static int g_threads = 12;
template <class F> static void pfor(int n, F&& f) {
    int T = std::max(1, std::min(g_threads, n / 512));
    if (T == 1) { f(0, n); return; }
    std::vector<std::thread> th;
    th.reserve(T);
    for (int k = 0; k < T; ++k) th.emplace_back([&, k] { f(int((long long)n * k / T), int((long long)n * (k + 1) / T)); });
    for (auto& t : th) t.join();
}

// ------------------------------------------------------------------ material
// One flat array of points per side: [region dye | boundaries | tetra tracers]; per-point branch id.
struct Material {
    std::vector<V3> X, X0, Xref;     // current, period-start (baseline), exact reference (WITH)
    std::vector<uint8_t> br;         // branch of each point
    int n_dye = 0, n_bnd_per = 0, bnd0 = 0, tet0 = 0, n_tet = 0;
};

static Material make_material(const Table& T, int dye_per, int bnd_per, int tet_per, unsigned seed) {
    Material m;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> U(0, 1);
    const int nb = int(T.br.size());
    for (int i = 0; i < nb; ++i) {  // dye: jittered grid inside P_i
        const auto& b = T.br[i];
        int gx = int(std::sqrt(dye_per * b.hp[0] / b.hp[1])), gy = std::max(1, dye_per / std::max(1, gx));
        for (int jy = 0; jy < gy; ++jy)
            for (int jx = 0; jx < gx; ++jx) {
                double x = b.p[0] - b.hp[0] + 2 * b.hp[0] * (jx + U(rng)) / gx;
                double y = b.p[1] - b.hp[1] + 2 * b.hp[1] * (jy + U(rng)) / gy;
                m.X.push_back({x, y, T.z0});
                m.br.push_back(uint8_t(i));
            }
    }
    m.n_dye = int(m.X.size());
    m.bnd0 = m.n_dye;
    m.n_bnd_per = bnd_per;
    for (int i = 0; i < nb; ++i) {  // boundary of P_i, counter-clockwise
        const auto& b = T.br[i];
        double x0 = b.p[0] - b.hp[0], x1 = b.p[0] + b.hp[0], y0 = b.p[1] - b.hp[1], y1 = b.p[1] + b.hp[1];
        for (int k = 0; k < bnd_per; ++k) {
            double u = 4.0 * k / bnd_per;
            int side = int(u);
            double f = u - side;
            double x, y;
            if (side == 0) { x = x0 + f * (x1 - x0); y = y0; }
            else if (side == 1) { x = x1; y = y0 + f * (y1 - y0); }
            else if (side == 2) { x = x1 - f * (x1 - x0); y = y1; }
            else { x = x0; y = y1 - f * (y1 - y0); }
            m.X.push_back({x, y, T.z0});
            m.br.push_back(uint8_t(i));
        }
    }
    m.tet0 = int(m.X.size());
    const double eps = 0.004;
    for (int i = 0; i < nb; ++i) {  // small tetrahedra (material volumes) inside P_i
        const auto& b = T.br[i];
        for (int k = 0; k < tet_per; ++k) {
            double x = b.p[0] + (2 * U(rng) - 1) * (b.hp[0] - eps), y = b.p[1] + (2 * U(rng) - 1) * (b.hp[1] - eps);
            double z = T.z0 - eps * 0.3;
            V3 a{x, y, z}, bx{x + eps, y, z}, by{x, y + eps, z}, bz{x, y, z + eps};
            for (auto& q : {a, bx, by, bz}) { m.X.push_back(q); m.br.push_back(uint8_t(i)); }
            ++m.n_tet;
        }
    }
    m.X0 = m.X;
    m.Xref = m.X;
    return m;
}

static double tet_vol(const V3& a, const V3& b, const V3& c, const V3& d) {
    double ux = wrapd(b.x - a.x), uy = wrapd(b.y - a.y), uz = wrapd(b.z - a.z);
    double vx = wrapd(c.x - a.x), vy = wrapd(c.y - a.y), vz = wrapd(c.z - a.z);
    double wx = wrapd(d.x - a.x), wy = wrapd(d.y - a.y), wz = wrapd(d.z - a.z);
    return (ux * (vy * wz - vz * wy) - uy * (vx * wz - vz * wx) + uz * (vx * wy - vy * wx)) / 6;
}

static double poly_area(const Material& m, int i) {
    const V3* P = &m.X[m.bnd0 + i * m.n_bnd_per];
    int n = m.n_bnd_per;
    double A = 0;
    for (int k = 0; k < n; ++k) {
        const V3& a = P[k];
        const V3& b = P[(k + 1) % n];
        A += a.x * b.y - b.x * a.y;
    }
    return 0.5 * std::fabs(A);
}

// scanline fill of region i's boundary polygon into a G x G mask over [0,3]^2
constexpr int G = 400;
static void raster(const Material& m, int i, std::vector<uint8_t>& mask) {
    mask.assign(G * G, 0);
    const V3* P = &m.X[m.bnd0 + i * m.n_bnd_per];
    int n = m.n_bnd_per;
    std::vector<double> xs;
    for (int row = 0; row < G; ++row) {
        double yc = (row + 0.5) * TL / G;
        xs.clear();
        for (int k = 0; k < n; ++k) {
            const V3& a = P[k];
            const V3& b = P[(k + 1) % n];
            if ((a.y <= yc) != (b.y <= yc)) xs.push_back(a.x + (yc - a.y) / (b.y - a.y) * (b.x - a.x));
        }
        std::sort(xs.begin(), xs.end());
        for (size_t k = 0; k + 1 < xs.size(); k += 2) {
            int c0 = std::max(0, int(std::ceil(xs[k] / TL * G - 0.5))), c1 = std::min(G - 1, int(std::floor(xs[k + 1] / TL * G - 0.5)));
            for (int c = c0; c <= c1; ++c) mask[row * G + c] = 1;
        }
    }
}

// ------------------------------------------------------------------ schedule
struct Clock {
    int hold0 = 45, per = 660, gap = 45;
    // phase: 0 hold, 1 period 1, 2 hold, 3 period 2, 4 hold. t = paper time within period.
    void at(int f, int& phase, double& t) const {
        if (f < hold0) { phase = 0; t = 0; return; }
        f -= hold0;
        if (f < per) { phase = 1; t = double(f) / per; return; }
        f -= per;
        if (f < gap) { phase = 2; t = 1; return; }
        f -= gap;
        if (f < per) { phase = 3; t = double(f) / per; return; }
        phase = 4; t = 1;
    }
};
static double theta_of(double t) { return Sf((t - 0.03) / 0.96); }   // baseline tween progress
static double dtheta_of(double t) { return Sp((t - 0.03) / 0.96) / 0.96; }

// ------------------------------------------------------------------ view (oblique 3-D)
struct Cam {
    float cx = 480, cy = 712, S = 330;
    double el = 0.5, az = -0.25, z0 = 0.6, zf = 0.6;
    Vec2 operator()(double x, double y, double z) const {
        double X = x - 1.5, Y = y - 1.7;
        double u = X * std::cos(az) - Y * std::sin(az), d = X * std::sin(az) + Y * std::cos(az);
        return {float(cx + S * u), float(cy - S * (d * std::sin(el) + (z - z0) * std::cos(el) * zf))};
    }
};

static const char* lamstr(double l) {
    static const double v[6] = {2.5, 0.5, 3.0, 0.4, 2.0, 1.0 / 3};
    static const char* s[6] = {"5/2", "1/2", "3", "2/5", "2", "1/3"};
    for (int k = 0; k < 6; ++k) if (std::fabs(l - v[k]) < 1e-9) return s[k];
    return "?";
}

static const Color BCOL[3] = {rgb(0x4cc9f0), rgb(0xf72585), rgb(0xffb703)};

static void draw_rect_dashed(Panel& p, const Cam& cam, double cx, double cy, double hx, double hy, double z, Color c,
                             float al) {
    double xs[5] = {cx - hx, cx + hx, cx + hx, cx - hx, cx - hx}, ys[5] = {cy - hy, cy - hy, cy + hy, cy + hy, cy - hy};
    for (int s = 0; s < 4; ++s) {
        int nd = 14;
        for (int k = 0; k < nd; k += 2) {
            double f0 = double(k) / nd, f1 = double(k + 1) / nd;
            Vec2 a = cam(xs[s] + f0 * (xs[s + 1] - xs[s]), ys[s] + f0 * (ys[s + 1] - ys[s]), z);
            Vec2 b = cam(xs[s] + f1 * (xs[s + 1] - xs[s]), ys[s] + f1 * (ys[s + 1] - ys[s]), z);
            p.line(a.x, a.y, b.x, b.y, 1.6f, c, al);
        }
    }
}

static void draw_plane(Panel& p, const Cam& cam, double z, Color c, float fill, float edge) {
    Vec2 q[4] = {cam(0.45, 0.85, z), cam(2.6, 0.85, z), cam(2.6, 2.6, z), cam(0.45, 2.6, z)};
    if (fill > 0) p.polygon(q, 4, c, fill);
    p.polyline(q, 4, 1.2f, c, edge, true);
}

struct SceneInfo {
    bool with = false;
    int phase = 0, stage = -1;
    double t = 0, theta = 0;
    const Table* T = nullptr;
    const std::vector<uint8_t>* overlap = nullptr;  // G*G mask (baseline highlight)
};

static void draw_scene(Panel& p, const Material& m, const std::vector<V3>* ambient, const SceneInfo& si,
                       const Table& T0) {
    p.fade(pal::bg, 0.55f);
    Cam cam;
    const Table& T = *si.T;
    // planes: floor z0 and (WITH) the private heights
    draw_plane(p, cam, T.z0, rgb(0x1a2233), 0.35f, 0.7f);
    if (si.with)
        for (size_t i = 0; i < T.br.size(); ++i) {
            draw_plane(p, cam, T.br[i].z, BCOL[i], 0.02f, 0.16f);
            Vec2 a = cam(2.6, 0.85, T.br[i].z);
            p.text(a.x + 6, a.y - 10, fmt("z%d", int(i) + 1), 14, BCOL[i], Font::Mono);
        }
    {
        Vec2 a = cam(2.6, 0.85, T.z0);
        p.text(a.x + 6, a.y - 10, "z0", 14, pal::dim, Font::Mono);
    }
    // ghost targets on the floor (current period's table) + faint sources
    for (size_t i = 0; i < T.br.size(); ++i) {
        const auto& b = T.br[i];
        draw_rect_dashed(p, cam, b.q[0], b.q[1], b.hq[0], b.hq[1], T.z0, BCOL[i], 0.55f);
    }
    p.blend = Blend::Add;
    // ambient dye (WITH only: it is carried by the same velocity field)
    if (ambient)
        for (const auto& a : *ambient) {
            Vec2 q = cam(a.x, a.y, a.z);
            double dz = std::fabs(a.z - T.z0);
            p.splat(q.x, q.y, dz > 0.02 ? rgb(0x8fa3c9) : rgb(0x56627a), dz > 0.02 ? 0.28f : 0.35f);
        }
    // region dye
    for (int k = 0; k < m.n_dye; ++k) {
        const V3& X = m.X[k];
        Vec2 q = cam(X.x, X.y, X.z);
        p.splat(q.x, q.y, BCOL[m.br[k]], 0.2f);
    }
    for (int k = 0; k < m.n_dye; k += 9) {
        const V3& X = m.X[k];
        Vec2 q = cam(X.x, X.y, X.z);
        p.glow(q.x, q.y, 5, BCOL[m.br[k]], 0.05f);
    }
    p.blend = Blend::Normal;
    // overlap highlight: cells covered by two branches on the same layer (baseline only can have them)
    if (si.overlap) {
        double sx = 0, sy = 0;
        long long n = 0;
        for (int r = 0; r < G; ++r)
            for (int c = 0; c < G; ++c)
                if ((*si.overlap)[r * G + c]) {
                    Vec2 q = cam((c + 0.5) * TL / G, (r + 0.5) * TL / G, T.z0);
                    p.circle(q.x, q.y, 1.5f, rgb(0xff2020), 0.55f);
                    sx += q.x; sy += q.y; ++n;
                }
        if (n > 40) {
            TextStyle st;
            st.size = 18; st.font = Font::Bold; st.color = rgb(0xff5050); st.backdrop = true; st.align = Align::Center;
            p.text(float(sx / n), float(sy / n) - 75, "branches collide", st);
        }
    }
    // boundaries (+ floor shadow and drop line when lifted)
    std::vector<Vec2> poly(m.n_bnd_per), shadow(m.n_bnd_per);
    for (size_t i = 0; i < T.br.size(); ++i) {
        const V3* P = &m.X[m.bnd0 + i * m.n_bnd_per];
        double zc = 0, xc = 0, yc = 0;
        for (int k = 0; k < m.n_bnd_per; ++k) {
            poly[k] = cam(P[k].x, P[k].y, P[k].z);
            shadow[k] = cam(P[k].x, P[k].y, T.z0);
            zc += P[k].z; xc += P[k].x; yc += P[k].y;
        }
        zc /= m.n_bnd_per; xc /= m.n_bnd_per; yc /= m.n_bnd_per;
        if (zc - T.z0 > 0.01) {
            p.polyline(shadow.data(), m.n_bnd_per, 1.2f, BCOL[i], 0.3f, true);
            Vec2 a = cam(xc, yc, T.z0), b = cam(xc, yc, zc);
            p.line(a.x, a.y, b.x, b.y, 1, BCOL[i], 0.35f);
        }
        p.polyline(poly.data(), m.n_bnd_per, 2.2f, lerp(BCOL[i], rgb(0xffffff), 0.35f), 0.95f, true);
        const auto& b0 = T0.br[i];
        Vec2 lab = cam(xc, yc, zc);
        TextStyle st;
        st.size = 14; st.font = Font::Mono; st.color = BCOL[i]; st.backdrop = true; st.backdrop_alpha = 0.55f;
        st.pad = 3; st.align = Align::Center;
        p.text(lab.x, lab.y - 8 - 120 * float(std::max(b0.hp[1], b0.hq[1])),
               fmt("%d: λ=%s", int(i) + 1, lamstr(T.br[i].lam)), st);
    }
}

static void draw_stagebar(Panel& p, const SceneInfo& si) {
    const float X = 30, Y = 880, W = 900, H = 46;
    p.fill_rounded_rect(X - 10, Y - 30, W + 20, H + 40, 8, rgb(0x05070b), 0.8f);
    if (si.with) {
        static const char* nm[7] = {"1 lift", "2 Y(−λ)", "3 X(1/λ−1)", "4 Y(1)", "5 X(λ−1)", "6 translate", "7 lower"};
        Pulses P;
        for (int j = 0; j < 7; ++j) {
            float x0 = X + W * float(P.t0[j]), w = W * float(P.len);
            bool on = (si.phase == 1 || si.phase == 3) && si.stage == j;
            p.fill_rounded_rect(x0, Y, w, H, 5, on ? pal::with : pal::grid, on ? 0.9f : 0.8f);
            p.text(x0 + w / 2, Y + 13, nm[j], 15, on ? rgb(0x05070b) : pal::dim, on ? Font::Bold : Font::Sans, Align::Center);
        }
        if (si.phase == 1 || si.phase == 3) p.fill_rect(X + W * float(si.t) - 1, Y - 4, 2, H + 8, pal::text);
        p.text(X, Y - 26, si.phase <= 2 ? "period 1: table F_i  (lift → 4 shears → translate → lower)"
                                        : "period 2: inverse table F_i⁻¹, same construction",
               15, pal::text, Font::Mono);
    } else {
        p.fill_rounded_rect(X, Y, W, H, 5, pal::grid, 0.8f);
        float th = float(si.theta);
        p.fill_rounded_rect(X, Y, W * th, H, 5, pal::without, 0.55f);
        p.text(X + W / 2, Y + 13, fmt("tween progress θ = %.2f", si.theta), 15, pal::text, Font::Mono, Align::Center);
        p.text(X, Y - 26, si.phase <= 2 ? "period 1: direct endpoint lerp  X + θ(F(X) − X)  in one plane"
                                        : "period 2: area-preserving tween diag(λ^θ, λ^−θ)  in one plane",
               15, pal::text, Font::Mono);
    }
}

int main(int argc, char** argv) {
    Config cfg;
    cfg.name = "02-fluid-gates";
    cfg.title_left = "single-plane tweens (lerp, then exp-scaling)";
    cfg.title_right = "seven-stage solenoidal shear flow";
    cfg.caption = "Family 376 — Finite Instructions and Solenoidal Shear Flows · WITH = thm:shears seven-stage field "
                  "(eq:velocity, eq:fields, eq:A, eq:Z, eq:shears), private heights, RK4 on the sampled field · "
                  "classical exp-scaling (left, period 2) keeps area, not branch separation";
    cfg.frames = 1500;
    Harness h(argc, argv, cfg);
    g_threads = arg_int(argc, argv, "--threads", 12);
    const int dye_per = arg_int(argc, argv, "--dye", 24000);
    const int substeps = arg_int(argc, argv, "--substeps", 2);
    const unsigned seed = (unsigned)arg_int(argc, argv, "--seed", 376);

    // ---- the finite rule table (thm:shears hypotheses: sources separated, targets separated)
    Table F;
    F.z0 = 0.6;
    auto mk = [](double px, double py, double hx, double hy, double qx, double qy, double lam, double z) {
        Branch b{};
        b.p[0] = px; b.p[1] = py; b.hp[0] = hx; b.hp[1] = hy;
        b.q[0] = qx; b.q[1] = qy; b.hq[0] = hx * lam; b.hq[1] = hy / lam;
        b.lam = lam; b.z = z;
        return b;
    };
    const double Ax = 0.85, Ay = 1.30, Bx = 2.15, By = 1.30, Cx = 1.50, Cy = 2.15;
    F.br.push_back(mk(Ax, Ay, 0.10, 0.25, Bx, By, 2.5, 1.75));  // A -> B
    F.br.push_back(mk(Bx, By, 0.20, 0.14, Ax, Ay, 0.5, 2.25));  // B -> A
    F.br.push_back(mk(Cx, Cy, 0.09, 0.27, Cx, Cy, 3.0, 1.25));  // C -> C (reshape in place)
    F.finish();
    Table Finv = F.inverse();
    Pulses P;
    Clock clk;
    if (h.frames() < 1500) {  // short previews: compress the timeline
        double s = h.frames() / 1500.0;
        clk.hold0 = int(45 * s); clk.per = std::max(10, int(660 * s)); clk.gap = int(45 * s);
    }

    Material L = make_material(F, dye_per, 600, 64, seed), R = L;
    const std::vector<V3> home = L.X;
    const int nb = int(F.br.size());
    std::vector<double> area0(nb), vol0(L.n_tet);
    for (int i = 0; i < nb; ++i) area0[i] = poly_area(L, i);
    for (int k = 0; k < L.n_tet; ++k) {
        const V3* q = &L.X[L.tet0 + 4 * k];
        vol0[k] = tet_vol(q[0], q[1], q[2], q[3]);
    }
    // ambient dye sheet at z0 (WITH side only, untimed visualization of the same flow)
    std::vector<V3> amb;
    {
        std::mt19937 rng(seed + 1);
        std::uniform_real_distribution<double> U(0, 1);
        const int ga = arg_int(argc, argv, "--ambient", 230);
        for (int j = 0; j < ga; ++j)
            for (int i = 0; i < ga; ++i) {
                double x = 0.35 + 2.3 * (i + U(rng)) / ga, y = 0.8 + 1.9 * (j + U(rng)) / ga;
                bool inside = false;
                for (auto& b : F.br)
                    if (std::fabs(x - b.p[0]) < b.hp[0] && std::fabs(y - b.p[1]) < b.hp[1]) inside = true;
                if (!inside) amb.push_back({x, y, F.z0});
            }
    }

    const std::vector<V3> amb_static = amb;  // the tween has no flow: the ambient fluid never moves
    double max_drift[2] = {0, 0}, max_overlap[2] = {0, 0}, max_div[2] = {0, 0}, max_exc[2] = {0, 0};
    double max_scal_exc_with = 0, max_err_with = 0, end1_err[2] = {0, 0}, end2_err[2] = {0, 0};
    int last_phase = 0;
    double exc_run[2] = {0, 0};  // running max of the sup excursion within the current period
    double per_drift_L[2] = {0, 0}, per_ovl_L[2] = {0, 0}, per_drift_R[2] = {0, 0}, per_ovl_R[2] = {0, 0};
    std::vector<uint8_t> mk1, mk2, ovl(G * G);
    std::vector<std::vector<uint8_t>> masks(nb);
    const double dtf = 1.0 / clk.per;  // paper time per frame

    h.left().set_compute_label("advect");
    h.right().set_compute_label("advect");
    h.left().sparkline("overlap");
    h.right().sparkline("overlap");

    while (h.next_frame()) {
        int phase;
        double t;
        clk.at(h.frame(), phase, t);
        const Table& T = phase <= 2 ? F : Finv;
        const bool moving = phase == 1 || phase == 3;
        // start of a period: baseline re-anchors its tween on the current positions
        if (phase != last_phase && moving) { L.X0 = L.X; exc_run[0] = exc_run[1] = 0; }
        double t_prev = std::max(0.0, t - dtf);

        // ---- WITHOUT: closed-form single-plane tween of every region point
        const double th = moving ? theta_of(t) : (phase == 0 ? 0.0 : 1.0);
        h.left().measure([&] {
            if (!moving) return;
            const bool expo = phase == 3;
            pfor(int(L.X.size()), [&](int a, int b) {
                for (int k = a; k < b; ++k) {
                    const Branch& B = T.br[L.br[k]];
                    const V3& X0 = L.X0[k];
                    double c0 = (1 - th) * B.p[0] + th * B.q[0], c1 = (1 - th) * B.p[1] + th * B.q[1];
                    double sx = expo ? std::pow(B.lam, th) : (1 - th) + th * B.lam;
                    double sy = expo ? std::pow(B.lam, -th) : (1 - th) + th / B.lam;
                    L.X[k] = {c0 + sx * (X0.x - B.p[0]), c1 + sy * (X0.y - B.p[1]), X0.z};
                }
            });
        });
        // ---- WITH: RK4 on the sampled seven-stage velocity field (eq:velocity)
        h.right().measure([&] {
            if (!moving) return;
            pfor(int(R.X.size()), [&](int a, int b) {
                const double hstep = (t - t_prev) / substeps;
                for (int k = a; k < b; ++k)
                    for (int s = 0; s < substeps; ++s) rk4(T, P, t_prev + s * hstep, hstep, R.X[k]);
            });
        });
        // ---- untimed: exact reference, ambient dye
        if (moving) {
            pfor(int(R.Xref.size()), [&](int a, int b) { for (int k = a; k < b; ++k) exact_step(T, P, t_prev, t, R.Xref[k]); });
            pfor(int(amb.size()), [&](int a, int b) {
                const double hstep = (t - t_prev) / substeps;
                for (int k = a; k < b; ++k)
                    for (int s = 0; s < substeps; ++s) rk4(T, P, t_prev + s * hstep, hstep, amb[k]);
            });
        }
        last_phase = phase;

        // ---- metrics (untimed)
        int stage = moving ? P.active(t) : -1;
        double drift[2] = {0, 0}, overlap[2] = {0, 0}, div[2] = {0, 0}, exc[2] = {0, 0}, err_with = 0;
        double zc[3] = {0, 0, 0};
        for (int side = 0; side < 2; ++side) {
            Material& M = side ? R : L;
            for (int i = 0; i < nb; ++i) drift[side] = std::max(drift[side], std::fabs(poly_area(M, i) / area0[i] - 1));
            for (int k = 0; k < M.n_tet; ++k) {
                const V3* q = &M.X[M.tet0 + 4 * k];
                drift[side] = std::max(drift[side], std::fabs(tet_vol(q[0], q[1], q[2], q[3]) / vol0[k] - 1));
            }
            // overlap: rasterize regions; WITH counts a pair only if they sit on the same layer
            for (int i = 0; i < nb; ++i) {
                double z = 0;
                for (int k = 0; k < M.n_bnd_per; ++k) z += M.X[M.bnd0 + i * M.n_bnd_per + k].z;
                zc[i] = z / M.n_bnd_per;
                raster(M, i, masks[i]);
            }
            if (!side) std::fill(ovl.begin(), ovl.end(), 0);
            double ov = 0, amean = 0;
            for (int i = 0; i < nb; ++i) amean += area0[i] / nb;
            for (int i = 0; i < nb; ++i)
                for (int j = i + 1; j < nb; ++j) {
                    if (std::fabs(zc[i] - zc[j]) > 0.02) continue;
                    long long c = 0;
                    for (int q = 0; q < G * G; ++q)
                        if (masks[i][q] & masks[j][q]) { ++c; if (!side) ovl[q] = 1; }
                    ov += c * (TL / G) * (TL / G);
                }
            overlap[side] = ov / amean;
            // sup excursion from the branch's moving center / h
            for (int k = 0; k < M.n_dye; k += 7) {
                const Branch& B = T.br[M.br[k]];
                double cx, cy;
                if (side == 0) { cx = (1 - th) * B.p[0] + th * B.q[0]; cy = (1 - th) * B.p[1] + th * B.q[1]; }
                else if (stage >= 0 && stage <= 4) { cx = B.p[0]; cy = B.p[1]; }
                else continue;  // WITH: lem:excursion bounds the scaling stages
                double e = std::max(std::fabs(wrapd(M.X[k].x - cx)), std::fabs(wrapd(M.X[k].y - cy))) / T.h;
                exc[side] = std::max(exc[side], e);
            }
        }
        if (moving) {
            // divergence residual. WITH: central differences of the sampled field at dye points.
            double dmax = 0;
            const double e = 1e-5;
            for (int k = 0; k < R.n_dye; k += 97) {
                V3 X = R.X[k];
                auto Vf = [&](double dx, double dy, double dz) { return velocity(T, P, t, {X.x + dx, X.y + dy, X.z + dz}); };
                double d = (Vf(e, 0, 0).x - Vf(-e, 0, 0).x + Vf(0, e, 0).y - Vf(0, -e, 0).y + Vf(0, 0, e).z - Vf(0, 0, -e).z) / (2 * e);
                dmax = std::max(dmax, std::fabs(d));
            }
            div[1] = dmax;
            // WITHOUT: divergence of the tween's implied velocity, tr(D' D^-1) (per unit paper time)
            double dth = dtheta_of(t);
            for (const auto& B : T.br) {
                double d = phase == 3 ? 0.0
                                      : dth * ((B.lam - 1) / ((1 - th) + th * B.lam) + (1 / B.lam - 1) / ((1 - th) + th / B.lam));
                div[0] = std::max(div[0], std::fabs(d));
            }
            for (int k = 0; k < int(R.X.size()); ++k) {
                const V3 &a = R.X[k], &b = R.Xref[k];
                err_with = std::max(err_with, std::max({std::fabs(wrapd(a.x - b.x)), std::fabs(wrapd(a.y - b.y)), std::fabs(wrapd(a.z - b.z))}));
            }
            if (stage >= 0 && stage <= 4) max_scal_exc_with = std::max(max_scal_exc_with, exc[1]);
            exc_run[0] = std::max(exc_run[0], exc[0]);
            exc_run[1] = std::max(exc_run[1], exc[1]);
        }
        // endpoint errors vs the rule table at the end of each period (exact targets)
        if ((phase == 2 || phase == 4) && last_phase == phase) {
            for (int side = 0; side < 2; ++side) {
                Material& M = side ? R : L;
                double e = 0;
                for (int k = 0; k < M.n_dye; ++k) {
                    const Branch& B = F.br[M.br[k]];
                    const V3& X0 = home[k];  // original home position
                    double ex, ey;
                    if (phase == 2) { ex = B.q[0] + B.lam * (X0.x - B.p[0]); ey = B.q[1] + (X0.y - B.p[1]) / B.lam; }
                    else { ex = X0.x; ey = X0.y; }
                    e = std::max({e, std::fabs(wrapd(M.X[k].x - ex)), std::fabs(wrapd(M.X[k].y - ey)), std::fabs(wrapd(M.X[k].z - F.z0))});
                }
                (phase == 2 ? end1_err : end2_err)[side] = e;
            }
        }
        if (phase == 1 || phase == 3) {
            int q = phase == 1 ? 0 : 1;
            per_drift_L[q] = std::max(per_drift_L[q], drift[0]);
            per_ovl_L[q] = std::max(per_ovl_L[q], overlap[0]);
            per_drift_R[q] = std::max(per_drift_R[q], drift[1]);
            per_ovl_R[q] = std::max(per_ovl_R[q], overlap[1]);
        }
        for (int s = 0; s < 2; ++s) {
            max_drift[s] = std::max(max_drift[s], drift[s]);
            max_overlap[s] = std::max(max_overlap[s], overlap[s]);
            max_div[s] = std::max(max_div[s], div[s]);
            max_exc[s] = std::max(max_exc[s], exc[s]);
        }
        max_err_with = std::max(max_err_with, err_with);

        // ---- draw
        SceneInfo sl, sr;
        sl.with = false; sl.phase = phase; sl.t = t; sl.theta = th; sl.T = &T; sl.overlap = &ovl;
        sr.with = true; sr.phase = phase; sr.t = t; sr.stage = stage; sr.T = &T;
        draw_scene(h.left(), L, &amb_static, sl, F);
        draw_scene(h.right(), R, &amb, sr, F);
        draw_stagebar(h.left(), sl);
        draw_stagebar(h.right(), sr);

        // ---- HUD
        auto hud = [&](Panel& p, int s) {
            bool w = s == 1;
            p.metric("area / material-volume drift", 100 * drift[s], "%.3g %%", drift[s] > 1e-6 ? Tone::Bad : Tone::Good);
            p.metric("overlap", 100 * overlap[s], "%.1f %%", overlap[s] > 0 ? Tone::Bad : Tone::Good);
            p.metric("divergence residual max", div[s], "%.2e", div[s] > 1e-9 ? Tone::Bad : Tone::Good);
            p.metric(w ? "max excursion |X−p|∞/h (≤ 2 proved)" : "max excursion |X−c|∞/h", exc_run[s], "%.3f", Tone::Neutral);
            if (w) p.metric("error vs exact stage maps", err_with, "%.1e", Tone::Good);
            else p.metric("endpoint error vs table", 0.0, "%.1e", Tone::Neutral);
        };
        hud(h.left(), 0);
        hud(h.right(), 1);
    }

    h.result("dye_particles_per_region", dye_per);
    h.result("advected_points_per_side", double(L.X.size()));
    h.result("ambient_particles_with_untimed", double(amb.size()));
    h.result("threads", g_threads);
    h.result("rk4_substeps_per_frame", substeps);
    h.result("h_halfwidth_bound", F.h);
    h.result("period1_lerp_max_drift_without", per_drift_L[0]);
    h.result("period1_lerp_max_overlap_frac_without", per_ovl_L[0]);
    h.result("period2_expscale_max_drift_without", per_drift_L[1]);
    h.result("period2_expscale_max_overlap_frac_without", per_ovl_L[1]);
    h.result("period1_max_drift_with", per_drift_R[0]);
    h.result("period2_max_drift_with", per_drift_R[1]);
    h.result("period1_max_overlap_with", per_ovl_R[0]);
    h.result("period2_max_overlap_with", per_ovl_R[1]);
    h.result("max_drift_without", max_drift[0]);
    h.result("max_drift_with", max_drift[1]);
    h.result("max_overlap_frac_without", max_overlap[0]);
    h.result("max_overlap_frac_with", max_overlap[1]);
    h.result("max_divergence_without", max_div[0]);
    h.result("max_divergence_with_fd", max_div[1]);
    h.result("max_excursion_over_h_without", max_exc[0]);
    h.result("max_excursion_over_h_with_scaling_stages", max_scal_exc_with);
    h.result("max_rk4_vs_exact_stage_maps", max_err_with);
    h.result("endpoint_error_period1_without", end1_err[0]);
    h.result("endpoint_error_period1_with", end1_err[1]);
    h.result("roundtrip_error_without", end2_err[0]);
    h.result("roundtrip_error_with", end2_err[1]);
    return h.finish();
}
