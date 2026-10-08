// 08-convex-morph - morphing origin-symmetric convex "rooms"/crystals in 3D.
//
// Family 091, "The logarithmic Brunn-Minkowski conjecture",
// preprints/The-logarithmic-Brunn-Minkowski-conjecture-September-23-2026/build/introduction.tex,
// Theorem thm:main, eq. eq:main:   | W[ h_K^(1-l) h_L^l ] |  >=  |K|^(1-l) |L|^l
// for origin-symmetric convex bodies K, L in any dimension (here: generic ones in 3D, no
// common symmetry besides x -> -x).
//
// Both panels build the in-between body the same way: intersect the half-spaces
// u.x <= h(u) over the same direction set U (all facet normals of both endpoints, +-, plus
// M rotated Fibonacci pairs), clipping a box with the same exact-arithmetic-style clipper.
//   LEFT  (WITHOUT): classical Minkowski interpolation, h = (1-l) h_K + l h_L (the arithmetic
//                    mean of support functions). It ALREADY has a volume floor (Brunn-Minkowski)
//                    but bloats: it contains the logarithmic body.
//   RIGHT (WITH):    logarithmic interpolation, h = h_K^(1-l) h_L^l (Wulff body of the geometric
//                    mean). eq:main guarantees volume >= |K|^(1-l)|L|^l. A finite direction set
//                    gives a body CONTAINING the continuum Wulff body, so the floor survives.
// The plot also shows (untimed, gray) the naive game-dev morph: radial / shared-mesh vertex
// lerp on a sphere-parameterized mesh, which has no floor and dips below it.
//
// Build + run:  ./build.sh 08-convex-morph run      (or: preview)
#include "demo.h"
#include "../07-periodic-blob/sw3d.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <vector>

using namespace demo;
using sw3d::V3;
using sw3d::dot;
using sw3d::cross;

static constexpr double PI = 3.14159265358979323846;

// ---------------------------------------------------------------- convex polyhedron + clipper
struct Poly {
    std::vector<std::vector<V3>> faces;  // CCW seen from outside
    std::vector<V3> normals;             // outward unit normals
};

static Poly make_box(double R) {
    Poly P;
    V3 c[8];
    for (int i = 0; i < 8; ++i) c[i] = V3{(i & 1) ? R : -R, (i & 2) ? R : -R, (i & 4) ? R : -R};
    // faces CCW from outside
    int f[6][4] = {{1, 3, 7, 5}, {0, 4, 6, 2}, {2, 6, 7, 3}, {0, 1, 5, 4}, {4, 5, 7, 6}, {0, 2, 3, 1}};
    V3 nrm[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (int k = 0; k < 6; ++k) {
        P.faces.push_back({c[f[k][0]], c[f[k][1]], c[f[k][2]], c[f[k][3]]});
        P.normals.push_back(nrm[k]);
    }
    return P;
}

static void push_unique(std::vector<V3>& v, V3 p, double tol) {
    if (!v.empty() && sw3d::len(v.back() - p) <= tol) return;
    v.push_back(p);
}

// Keep the part with n.x <= d. Returns true if anything was cut.
static constexpr double CLIP_TOL = 1e-12;
static bool clip(Poly& P, V3 n, double d) {
    double mx = -1e300;
    for (auto& f : P.faces)
        for (auto& v : f) mx = std::max(mx, dot(n, v) - d);
    if (mx <= CLIP_TOL) return false;
    Poly out;
    std::vector<V3> cap;
    for (size_t fi = 0; fi < P.faces.size(); ++fi) {
        const auto& f = P.faces[fi];
        std::vector<V3> o;
        const size_t m = f.size();
        for (size_t i = 0; i < m; ++i) {
            V3 a = f[i], b = f[(i + 1) % m];
            double da = dot(n, a) - d, db = dot(n, b) - d;
            bool ia = da <= 0, ib = db <= 0;
            if (ia) push_unique(o, a, CLIP_TOL);
            if (ia != ib) {
                V3 p = a + (b - a) * (da / (da - db));
                push_unique(o, p, CLIP_TOL);
                cap.push_back(p);
            }
        }
        while (o.size() > 1 && sw3d::len(o.front() - o.back()) <= CLIP_TOL) o.pop_back();
        if (o.size() >= 3) { out.faces.push_back(std::move(o)); out.normals.push_back(P.normals[fi]); }
    }
    if (cap.size() >= 3) {
        V3 c{0, 0, 0};
        for (auto& p : cap) c += p;
        c = c / (double)cap.size();
        V3 e1 = sw3d::norm(std::fabs(n.x) < 0.9 ? cross(n, V3{1, 0, 0}) : cross(n, V3{0, 1, 0}));
        V3 e2 = cross(n, e1);
        std::sort(cap.begin(), cap.end(), [&](const V3& a, const V3& b) {
            return std::atan2(dot(a - c, e2), dot(a - c, e1)) < std::atan2(dot(b - c, e2), dot(b - c, e1));
        });
        std::vector<V3> capu;
        for (auto& p : cap) push_unique(capu, p, 1e-10);
        while (capu.size() > 1 && sw3d::len(capu.front() - capu.back()) <= 1e-10) capu.pop_back();
        if (capu.size() >= 3) { out.faces.push_back(std::move(capu)); out.normals.push_back(n); }
    }
    P = std::move(out);
    return true;
}

static double volume(const Poly& P) {
    double v = 0;
    for (auto& f : P.faces)
        for (size_t i = 1; i + 1 < f.size(); ++i) v += dot(f[0], cross(f[i], f[i + 1]));
    return v / 6.0;
}
static double support(const Poly& P, V3 u) {
    double h = -1e300;
    for (auto& f : P.faces)
        for (auto& v : f) h = std::max(h, dot(u, v));
    return h;
}

// ---------------------------------------------------------------- generic symmetric endpoints
struct Body {  // K = intersection of |n_i . x| <= a_i
    std::vector<V3> n;
    std::vector<double> a;
    Poly P;
    double vol = 0;
    std::string name;
    void build() {
        P = make_box(50);
        for (size_t i = 0; i < n.size(); ++i) { clip(P, n[i], a[i]); clip(P, -n[i], a[i]); }
        vol = volume(P);
    }
    void normalize(double target) {
        double s = std::cbrt(target / vol);
        for (auto& x : a) x *= s;
        build();
    }
    double radial(V3 d) const {  // radial function along unit d
        double m = 0;
        for (size_t i = 0; i < n.size(); ++i) m = std::max(m, std::fabs(dot(n[i], d)) / a[i]);
        return 1.0 / m;
    }
};

static V3 rand_dir(std::mt19937& rng) {
    std::normal_distribution<double> G(0, 1);
    return sw3d::norm(V3{G(rng), G(rng), G(rng)});
}
static V3 perp_dir(std::mt19937& rng, V3 axis, double tilt) {
    V3 r = rand_dir(rng);
    V3 p = sw3d::norm(r - axis * dot(r, axis));
    return sw3d::norm(p + axis * tilt);
}

static Body make_body(int type, unsigned seed, double target_vol) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> U(0, 1);
    Body b;
    if (type == 0) {  // tablet: one thin direction
        V3 ax = rand_dir(rng);
        b.n.push_back(ax); b.a.push_back(0.28);
        for (int i = 0; i < 7; ++i) { b.n.push_back(perp_dir(rng, ax, 0.5 * (U(rng) - 0.5))); b.a.push_back(0.85 + 0.45 * U(rng)); }
        b.name = "tablet";
    } else if (type == 1) {  // needle: one long direction
        V3 ax = rand_dir(rng);
        for (int i = 0; i < 6; ++i) { b.n.push_back(perp_dir(rng, ax, 0.25 * (U(rng) - 0.5))); b.a.push_back(0.28 + 0.12 * U(rng)); }
        for (int i = 0; i < 2; ++i) { b.n.push_back(sw3d::norm(ax + rand_dir(rng) * 0.35)); b.a.push_back(1.4 + 0.3 * U(rng)); }
        b.name = "needle";
    } else {  // generic crystal
        for (int i = 0; i < 9; ++i) { b.n.push_back(rand_dir(rng)); b.a.push_back(0.55 + 0.6 * U(rng)); }
        b.name = "crystal";
    }
    b.build();
    b.normalize(target_vol);
    return b;
}

// ---------------------------------------------------------------- the morph (timed part)
struct Morph {
    std::vector<V3> dirs;      // full symmetric direction set (u and -u both present)
    std::vector<double> h;     // per-direction bound used last
    Poly P;
    double vol = 0;
    int planes_cut = 0;
    // geometric = true: h = hK^(1-l) hL^l (eq:main); false: h = (1-l) hK + l hL (Minkowski)
    void build(const Body& K, const Body& L, double lam, bool geometric) {
        const size_t m = dirs.size();
        h.resize(m);
        double hmax = 0;
        for (size_t j = 0; j < m; ++j) {  // support values recomputed from the original endpoints
            double hk = support(K.P, dirs[j]), hl = support(L.P, dirs[j]);
            h[j] = geometric ? std::pow(hk, 1 - lam) * std::pow(hl, lam) : (1 - lam) * hk + lam * hl;
            hmax = std::max(hmax, h[j]);
        }
        P = make_box(hmax * 1.01 + 1e-9);
        planes_cut = 0;
        for (size_t j = 0; j < m; ++j) planes_cut += clip(P, dirs[j], h[j]);
        vol = volume(P);
    }
};

static std::vector<V3> direction_set(const Body& K, const Body& L, int M, unsigned seed) {
    std::vector<V3> d;
    for (auto& n : K.n) { d.push_back(n); d.push_back(-n); }
    for (auto& n : L.n) { d.push_back(n); d.push_back(-n); }
    // M Fibonacci directions on a hemisphere, under a random rotation, plus antipodes
    std::mt19937 rng(seed);
    V3 a = rand_dir(rng), b0 = rand_dir(rng);
    V3 b = sw3d::norm(b0 - a * dot(b0, a)), c = cross(a, b);
    const double ga = PI * (3 - std::sqrt(5.0));
    for (int i = 0; i < M; ++i) {
        double z = (i + 0.5) / M, r = std::sqrt(1 - z * z), ph = i * ga;
        V3 u = a * (r * std::cos(ph)) + b * (r * std::sin(ph)) + c * z;
        d.push_back(u); d.push_back(-u);
    }
    return d;
}

// naive game-dev morph: shared sphere-parameterized mesh, radial vertex lerp (untimed, plot only)
struct Sphere {
    std::vector<V3> v;
    std::vector<int> t;
    Sphere(int sub) {
        const double p = (1 + std::sqrt(5.0)) / 2;
        v = {{-1, p, 0}, {1, p, 0}, {-1, -p, 0}, {1, -p, 0}, {0, -1, p}, {0, 1, p}, {0, -1, -p}, {0, 1, -p}, {p, 0, -1}, {p, 0, 1}, {-p, 0, -1}, {-p, 0, 1}};
        for (auto& x : v) x = sw3d::norm(x);
        t = {0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 7, 1, 8,
             3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1};
        for (int s = 0; s < sub; ++s) {
            std::vector<int> nt;
            std::vector<std::pair<long long, int>> cache;
            auto mid = [&](int a, int b) {
                long long key = (long long)std::min(a, b) * 1000000 + std::max(a, b);
                for (auto& kv : cache) if (kv.first == key) return kv.second;
                v.push_back(sw3d::norm(v[a] + v[b]));
                cache.push_back({key, (int)v.size() - 1});
                return (int)v.size() - 1;
            };
            for (size_t i = 0; i < t.size(); i += 3) {
                int a = t[i], b = t[i + 1], c = t[i + 2];
                int ab = mid(a, b), bc = mid(b, c), ca = mid(c, a);
                int q[12] = {a, ab, ca, b, bc, ab, c, ca, bc, ab, bc, ca};
                nt.insert(nt.end(), q, q + 12);
            }
            t = nt;
        }
    }
};
static double radial_lerp_volume(const Sphere& S, const Body& K, const Body& L, double lam) {
    std::vector<V3> p(S.v.size());
    for (size_t i = 0; i < S.v.size(); ++i) p[i] = S.v[i] * ((1 - lam) * K.radial(S.v[i]) + lam * L.radial(S.v[i]));
    double vol = 0;
    for (size_t i = 0; i < S.t.size(); i += 3) vol += dot(p[S.t[i]], cross(p[S.t[i + 1]], p[S.t[i + 2]]));
    return std::fabs(vol) / 6.0;
}

// ---------------------------------------------------------------- drawing
static void hull2d(std::vector<Vec2>& pts) {  // monotone chain, in place
    std::sort(pts.begin(), pts.end(), [](Vec2 a, Vec2 b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
    if (pts.size() < 3) return;
    std::vector<Vec2> H(2 * pts.size());
    auto cr = [](Vec2 o, Vec2 a, Vec2 b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); };
    size_t k = 0;
    for (size_t i = 0; i < pts.size(); ++i) { while (k >= 2 && cr(H[k - 2], H[k - 1], pts[i]) <= 0) --k; H[k++] = pts[i]; }
    for (size_t i = pts.size() - 1, t = k + 1; i > 0; --i) { while (k >= t && cr(H[k - 2], H[k - 1], pts[i - 1]) <= 0) --k; H[k++] = pts[i - 1]; }
    H.resize(k - 1);
    pts = H;
}

struct View3 {
    sw3d::Camera cam;
    V3 off{0, 0, 0};
    double s = 1;
    Vec2 P(V3 p) const { double x, y; cam.project(off + p * s, x, y); return {(float)x, (float)y}; }
};

static void draw_body(Panel& pn, const Poly& P, const View3& v, Color base, float alpha_mul = 1.f, bool edges = true) {
    const V3 Ld = sw3d::norm(V3{-0.4, 0.85, 0.35});
    std::vector<Vec2> pts;
    for (int pass = 0; pass < 2; ++pass) {  // 0 = back faces (inside of the glass), 1 = front
        for (size_t fi = 0; fi < P.faces.size(); ++fi) {
            const auto& f = P.faces[fi];
            V3 nrm = P.normals[fi];
            V3 vp = v.off + f[0] * v.s;
            bool front = dot(nrm, v.cam.eye - vp) > 0;
            if (front != (pass == 1)) continue;
            pts.clear();
            for (auto& q : f) pts.push_back(v.P(q));
            if (pass == 0) {
                float diff = (float)std::max(0.0, dot(-nrm, Ld));
                pn.polygon(pts.data(), (int)pts.size(), scale(base, 0.18f + 0.25f * diff), 0.55f * alpha_mul);
                if (edges) pn.polyline(pts.data(), (int)pts.size(), 1.0f, base, 0.12f * alpha_mul, true);
            } else {
                float diff = (float)std::max(0.0, dot(nrm, Ld));
                V3 vdir = sw3d::norm(v.cam.eye - vp);
                V3 H = sw3d::norm(Ld + vdir);
                float spec = (float)std::pow(std::max(0.0, dot(nrm, H)), 40.0);
                float fres = (float)std::pow(1 - std::max(0.0, dot(nrm, vdir)), 3.0);
                Color c = scale(base, 0.22f + 0.7f * diff);
                c = lerp(c, rgb(0xffffff), std::min(1.f, 0.75f * spec + 0.25f * fres));
                pn.polygon(pts.data(), (int)pts.size(), c, (0.55f + 0.3f * fres) * alpha_mul);
                if (edges) pn.polyline(pts.data(), (int)pts.size(), 1.1f, lerp(base, rgb(0xffffff), 0.5f), 0.35f * alpha_mul, true);
            }
        }
    }
}
static void draw_outline(Panel& pn, const Poly& P, const View3& v, Color c, float alpha, float thick) {
    std::vector<Vec2> pts;
    for (auto& f : P.faces)
        for (auto& q : f) pts.push_back(v.P(q));
    hull2d(pts);
    pn.polyline(pts.data(), (int)pts.size(), thick, c, alpha, true);
}

struct Curves {  // per-segment reference curves on a lambda grid (untimed; for the plot)
    std::vector<double> lam, floor_, logv, mink, radial;
};

static void draw_plot(Panel& p, const Curves& C, const std::vector<Vec2>& live_log, const std::vector<Vec2>& live_min,
                      double lam_now, double v_now, bool is_with, double radial_min_ratio) {
    const float x0 = 80, x1 = 930, y0 = 712, y1 = 915;
    p.fill_rounded_rect(x0 - 60, y0 - 26, x1 - x0 + 80, y1 - y0 + 62, 10, rgb(0x0f131b), 0.92f);
    double lo = 1e9, hi = -1e9;
    for (size_t i = 0; i < C.lam.size(); ++i) {
        lo = std::min({lo, C.floor_[i], C.radial[i], C.logv[i]});
        hi = std::max({hi, C.mink[i], C.logv[i]});
    }
    double pad = 0.08 * (hi - lo);
    lo -= pad; hi += pad;
    auto X = [&](double l) { return float(x0 + (x1 - x0) * l); };
    auto Y = [&](double v) { return float(y1 - (y1 - y0) * (v - lo) / (hi - lo)); };
    for (int k = 0; k <= 4; ++k) {
        double v = lo + (hi - lo) * k / 4;
        p.line(x0, Y(v), x1, Y(v), 1, pal::grid);
        p.text(x0 - 8, Y(v) - 8, fmt("%.2f", v), 12, pal::dim, Font::Mono, Align::Right);
    }
    std::vector<Vec2> fl, lg, mk, rd;
    std::vector<Vec2> below;  // shaded "forbidden for WITH" region under the floor
    for (size_t i = 0; i < C.lam.size(); ++i) {
        fl.push_back({X(C.lam[i]), Y(C.floor_[i])});
        lg.push_back({X(C.lam[i]), Y(C.logv[i])});
        mk.push_back({X(C.lam[i]), Y(C.mink[i])});
        rd.push_back({X(C.lam[i]), Y(C.radial[i])});
    }
    for (size_t i = 0; i + 1 < fl.size(); ++i) {
        Vec2 q[4] = {fl[i], fl[i + 1], {fl[i + 1].x, y1}, {fl[i].x, y1}};
        p.polygon(q, 4, pal::bad, 0.07f, false);
    }
    // reference curves (full segment, faint), then the live traces
    p.polyline(rd.data(), (int)rd.size(), 1.6f, pal::dim, 0.7f);
    p.polyline(mk.data(), (int)mk.size(), 1.4f, pal::without, 0.30f);
    p.polyline(lg.data(), (int)lg.size(), 1.4f, pal::with, 0.30f);
    for (size_t i = 0; i + 1 < fl.size(); i += 2) p.line(fl[i].x, fl[i].y, fl[i + 1].x, fl[i + 1].y, 2.2f, pal::text, 0.95f);
    if (live_min.size() > 1) p.polyline(live_min.data(), (int)live_min.size(), is_with ? 2.0f : 3.0f, pal::without, 1.0f);
    if (live_log.size() > 1) p.polyline(live_log.data(), (int)live_log.size(), is_with ? 3.0f : 2.0f, pal::with, 1.0f);
    Color acc = is_with ? pal::with : pal::without;
    p.glow(X(lam_now), Y(v_now), 15, acc, 0.9f);
    p.circle(X(lam_now), Y(v_now), 4.5f, lerp(acc, rgb(0xffffff), 0.4f));
    // legend
    float lx = x0, ly = y1 + 8;
    auto key = [&](Color c, const std::string& s, bool dashed) {
        if (dashed) { p.line(lx, ly + 9, lx + 7, ly + 9, 2.2f, c); p.line(lx + 11, ly + 9, lx + 18, ly + 9, 2.2f, c); }
        else p.line(lx, ly + 9, lx + 18, ly + 9, 2.6f, c);
        lx += 24 + p.text(lx + 24, ly, s, 13, pal::dim, Font::Sans) + 16;
    };
    key(pal::text, "floor |K|^(1−λ)|L|^λ", true);
    key(pal::without, "Minkowski", false);
    key(pal::with, "logarithmic (eq:main)", false);
    key(pal::dim, fmt("naive radial vertex lerp (no guarantee, min %.2f× floor)", radial_min_ratio), false);
    p.text(x0, y0 - 22, "volume vs λ (this segment)", 13, pal::dim, Font::Sans);
    p.text(x1, y0 - 22, "λ →", 13, pal::dim, Font::Sans, Align::Right);
}

// ---------------------------------------------------------------- refinement study (results.json)
static void refinement_study(Harness& h, const Body& K, const Body& L, bool print) {
    for (int M : {8, 16, 32, 64, 128, 256, 512}) {
        Morph mg, ma;
        mg.dirs = ma.dirs = direction_set(K, L, M, 77);
        mg.build(K, L, 0.5, true);
        ma.build(K, L, 0.5, false);
        double fl = std::sqrt(K.vol * L.vol);
        h.result(fmt("refine_M%d_log_over_floor", M), mg.vol / fl);
        h.result(fmt("refine_M%d_mink_over_floor", M), ma.vol / fl);
        if (print) std::printf("[refine] M=%4d planes=%4zu  log/floor %.5f   Minkowski/floor %.5f\n", M, mg.dirs.size(), mg.vol / fl, ma.vol / fl);
    }
}

// ---------------------------------------------------------------- main
int main(int argc, char** argv) {
    Config cfg;
    cfg.name = "08-convex-morph";
    cfg.title_left = "Minkowski morph (1−λ)h_K + λh_L";
    cfg.title_right = "log morph W[h_K^(1−λ) h_L^λ]";
    cfg.caption = "Family 091 — The logarithmic Brunn–Minkowski conjecture · WITH uses Thm thm:main / eq:main "
                  "(Wulff body of the geometric-mean support, volume ≥ |K|^(1−λ)|L|^λ) · baseline also has a floor "
                  "(Brunn–Minkowski) but contains the WITH body";
    cfg.frames = 1500;
    Harness h(argc, argv, cfg);

    const int M = arg_int(argc, argv, "--dirs", 160);
    const int seg_frames = arg_int(argc, argv, "--seg", 300);
    const unsigned seed = (unsigned)arg_int(argc, argv, "--seed", 91);
    const double lfix = arg_double(argc, argv, "--lfix", -1);
    const int still_every = arg_int(argc, argv, "--still-every", 0);

    // keyframes: generic origin-symmetric bodies, different volumes
    std::vector<Body> keys = {make_body(0, seed + 1, 1.2), make_body(1, seed + 2, 0.8), make_body(2, seed + 3, 1.5),
                              make_body(0, seed + 4, 0.9), make_body(2, seed + 5, 1.1)};
    const int nkeys = (int)keys.size();
    if (!h.preview()) refinement_study(h, keys[0], keys[1], !h.quiet());

    double rmax_all = 0;
    for (auto& b : keys)
        for (auto& f : b.P.faces)
            for (auto& v : f) rmax_all = std::max(rmax_all, sw3d::len(v));
    Sphere sph(4);
    Morph mL, mR;  // L = Minkowski (WITHOUT), R = logarithmic (WITH)
    Curves curves;
    int cur_seg = -1;
    double minL = 1e9, maxL = 0, minR = 1e9, maxR = 0, min_radial = 1e9, max_LoverR = 0;
    double seg_radial_min = 1;
    std::vector<Vec2> liveLog, liveMin;

    h.left().set_compute_label("support + clip");
    h.right().set_compute_label("support + clip");
    h.left().sparkline("volume / floor");
    h.right().sparkline("volume / floor");

    while (h.next_frame()) {
        const int f = h.frame();
        const int seg = (f / seg_frames) % nkeys;
        const Body& K = keys[seg];
        const Body& L = keys[(seg + 1) % nkeys];
        double u = double(f % seg_frames) / (seg_frames - 1);
        u = std::clamp((u - 0.05) / 0.9, 0.0, 1.0);
        double lam = lfix >= 0 ? lfix : u * u * (3 - 2 * u);

        if (seg != cur_seg) {  // new segment: direction set + reference curves (untimed)
            cur_seg = seg;
            mL.dirs = mR.dirs = direction_set(K, L, M, seed * 31 + seg);
            curves = Curves{};
            Morph a, b;
            a.dirs = b.dirs = mL.dirs;
            seg_radial_min = 1e9;
            for (int i = 0; i <= 40; ++i) {
                double l = i / 40.0;
                a.build(K, L, l, true); b.build(K, L, l, false);
                double fl = std::pow(K.vol, 1 - l) * std::pow(L.vol, l);
                double rv = radial_lerp_volume(sph, K, L, l);
                curves.lam.push_back(l); curves.floor_.push_back(fl); curves.logv.push_back(a.vol);
                curves.mink.push_back(b.vol); curves.radial.push_back(rv);
                seg_radial_min = std::min(seg_radial_min, rv / fl);
            }
            min_radial = std::min(min_radial, seg_radial_min);
            liveLog.clear(); liveMin.clear();
        }

        h.left().measure([&] { mL.build(K, L, lam, false); });
        h.right().measure([&] { mR.build(K, L, lam, true); });

        const double floorv = std::pow(K.vol, 1 - lam) * std::pow(L.vol, lam);
        const double rL = mL.vol / floorv, rR = mR.vol / floorv;
        if (lam > 0.02 && lam < 0.98) {  // interior of the morph (at the endpoints both ratios are exactly 1)
            minL = std::min(minL, rL); minR = std::min(minR, rR);
        }
        maxL = std::max(maxL, rL); maxR = std::max(maxR, rR);
        const double TOL = 1e-9;  // floating-point clipping tolerance, reported separately
        max_LoverR = std::max(max_LoverR, mL.vol / mR.vol);

        // ---- draw (untimed)
        const double t = f / h.fps();
        View3 view;
        // fixed scale for the whole video: the largest keyframe radius maps to ~255 px
        const double focal = 380 / std::tan(17 * PI / 180);
        view.cam.orbit(V3{0, 0, 0}, 0.4 + 0.28 * t, 0.32 + 0.12 * std::sin(0.35 * t), focal * rmax_all / 255, 34 * PI / 180, 760, 590, 430);
        View3 thumb = view;
        thumb.s = 0.2;
        {
            // plot coordinates for live traces (same mapping as draw_plot)
            double lo = 1e9, hi = -1e9;
            for (size_t i = 0; i < curves.lam.size(); ++i) {
                lo = std::min({lo, curves.floor_[i], curves.radial[i], curves.logv[i]});
                hi = std::max({hi, curves.mink[i], curves.logv[i]});
            }
            double pad = 0.08 * (hi - lo); lo -= pad; hi += pad;
            auto X = [&](double l) { return float(80 + 850 * l); };
            auto Y = [&](double v) { return float(915 - 203 * (v - lo) / (hi - lo)); };
            liveLog.push_back({X(lam), Y(mR.vol)});
            liveMin.push_back({X(lam), Y(mL.vol)});
        }
        for (int side = 0; side < 2; ++side) {
            Panel& p = side ? h.right() : h.left();
            const Morph& me = side ? mR : mL;
            const Morph& other = side ? mL : mR;
            Color acc = side ? pal::with : pal::without;
            Color oacc = side ? pal::without : pal::with;
            p.clear(pal::bg);
            p.glow(590, 430, 300, scale(acc, 0.35f), 0.25f);
            draw_body(p, me.P, view, acc);
            draw_outline(p, me.P, view, lerp(acc, rgb(0xffffff), 0.3f), 0.9f, 2.0f);
            draw_outline(p, other.P, view, oacc, 0.75f, 1.4f);
            p.text(940, 655, side ? "coral outline = Minkowski body (contains this one)" : "teal outline = log body (inside this one)",
                   14, pal::dim, Font::Sans, Align::Right);
            // endpoint thumbnails + lambda bar
            const float ty = 112;
            View3 tk = thumb, tl = thumb;
            tk.cam.cx = 620; tk.cam.cy = ty; tl.cam.cx = 880; tl.cam.cy = ty;
            draw_body(p, K.P, tk, pal::dim, 0.8f, false);
            draw_body(p, L.P, tl, pal::dim, 0.8f, false);
            p.text(620, ty + 62, fmt("K: %s  |K|=%.2f", K.name.c_str(), K.vol), 13, pal::dim, Font::Sans, Align::Center);
            p.text(880, ty + 62, fmt("L: %s  |L|=%.2f", L.name.c_str(), L.vol), 13, pal::dim, Font::Sans, Align::Center);
            p.fill_rounded_rect(690, ty - 3, 120, 6, 3, pal::grid);
            p.fill_rounded_rect(690, ty - 3, float(120 * lam), 6, 3, acc);
            p.text(750, ty + 10, fmt("λ = %.2f", lam), 15, pal::text, Font::Mono, Align::Center);
            p.text(750, ty - 30, fmt("keyframe %d → %d", seg + 1, (seg + 1) % nkeys + 1), 13, pal::dim, Font::Sans, Align::Center);
            draw_plot(p, curves, liveLog, liveMin, lam, side ? mR.vol : mL.vol, side == 1, seg_radial_min);
        }

        Panel& Lp = h.left();
        Panel& Rp = h.right();
        Lp.metric("volume / floor", rL, "%.4f", rL >= 1 - TOL ? Tone::Good : Tone::Bad);
        Lp.metric("min ratio, 0<λ<1, so far", minL > 1e8 ? 1.0 : minL, "%.4f", minL >= 1 - TOL ? Tone::Good : Tone::Bad);
        Lp.metric("max ratio so far (bloat)", maxL, "%.3f", Tone::Warn);
        Lp.metric("volume vs WITH body", mL.vol / mR.vol, "%.3f×", Tone::Warn);
        Lp.metric_text("planes that cut / total", fmt("%d / %zu", mL.planes_cut, mL.dirs.size()));
        Lp.metric_text("guarantee", "≥ floor (Brunn–Minkowski)", Tone::Neutral);

        Rp.metric("volume / floor", rR, "%.4f", rR >= 1 - TOL ? Tone::Good : Tone::Bad);
        Rp.metric("min ratio, 0<λ<1, so far", minR > 1e8 ? 1.0 : minR, "%.4f", minR >= 1 - TOL ? Tone::Good : Tone::Bad);
        Rp.metric("max ratio so far (bloat)", maxR, "%.3f", Tone::Good);
        Rp.metric("volume vs WITHOUT body", mR.vol / mL.vol, "%.3f×", Tone::Good);
        Rp.metric_text("planes that cut / total", fmt("%d / %zu", mR.planes_cut, mR.dirs.size()));
        if (still_every > 0 && f % still_every == still_every / 2) h.save_still(fmt("still_%04d.png", f));
        Rp.metric_text("guarantee", "≥ floor (eq:main)", Tone::Good);
    }
    h.result("directions_M", M);
    h.result("planes_total", (double)mR.dirs.size());
    h.result("clip_tolerance", CLIP_TOL);
    h.result("left_min_volume_over_floor_interior", minL);
    h.result("left_max_volume_over_floor", maxL);
    h.result("right_min_volume_over_floor_interior", minR);
    h.result("right_max_volume_over_floor", maxR);
    h.result("max_minkowski_over_log_volume", max_LoverR);
    h.result("radial_lerp_min_volume_over_floor", min_radial);
    for (int i = 0; i < nkeys; ++i) h.result(fmt("key%d", i), fmt("%s vol=%.3f facets=%zu", keys[i].name.c_str(), keys[i].vol, keys[i].P.faces.size()));
    return h.finish();
}
