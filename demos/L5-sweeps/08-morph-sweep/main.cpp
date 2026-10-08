// L5-sweeps/08-morph-sweep - sweep-only load ladder for demos/08-convex-morph (family 091).
//
// Load n = number of half-space directions u (u.x <= h(u)) that each in-between body is clipped
// by: every facet normal of K and L (both signs) plus M rotated Fibonacci pairs, with M chosen so
// the total is ~n. Stages n = 50, 100, ..., 6400 (factor 2).
//   WITHOUT: Minkowski interpolation h = (1-l) h_K + l h_L (Brunn-Minkowski floor, bloats).
//   WITH:    logarithmic Wulff body W[h_K^(1-l) h_L^l], thm:main / eq:main
//            (preprints/The-logarithmic-Brunn-Minkowski-conjecture-September-23-2026/build/introduction.tex).
// Both sides: same directions, same clipper (copied from 08-convex-morph unchanged), 1 thread.
// One "frame" = one full build of the in-between body (support values + box clip by all planes).
// Frames cycle through 5 keyframe segments x lambda in {0.05, 0.25, 0.5, 0.75, 0.95}; quality is taken
// over the first full pass (25 bodies per side): min and max volume / floor |K|^(1-l)|L|^l.
//
// Build/run (Git Bash, from demos/):  ./build.sh L5-sweeps/08-morph-sweep && L5-sweeps/08-morph-sweep/demo.exe
// Flags: --max-n N (default 6400)  --min-ms 1500  --min-frames 25  --out DIR
#include "../ladder_common.h"
#include "../../07-periodic-blob/sw3d.h"

#include <random>

using namespace demo;
using sw3d::V3;
using sw3d::dot;
using sw3d::cross;

static constexpr double PI = 3.14159265358979323846;

// ------------------------------------------------ copied verbatim from 08-convex-morph/main.cpp
struct Poly {
    std::vector<std::vector<V3>> faces;
    std::vector<V3> normals;
};
static Poly make_box(double R) {
    Poly P;
    V3 c[8];
    for (int i = 0; i < 8; ++i) c[i] = V3{(i & 1) ? R : -R, (i & 2) ? R : -R, (i & 4) ? R : -R};
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
struct Body {
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
    if (type == 0) {
        V3 ax = rand_dir(rng);
        b.n.push_back(ax); b.a.push_back(0.28);
        for (int i = 0; i < 7; ++i) { b.n.push_back(perp_dir(rng, ax, 0.5 * (U(rng) - 0.5))); b.a.push_back(0.85 + 0.45 * U(rng)); }
        b.name = "tablet";
    } else if (type == 1) {
        V3 ax = rand_dir(rng);
        for (int i = 0; i < 6; ++i) { b.n.push_back(perp_dir(rng, ax, 0.25 * (U(rng) - 0.5))); b.a.push_back(0.28 + 0.12 * U(rng)); }
        for (int i = 0; i < 2; ++i) { b.n.push_back(sw3d::norm(ax + rand_dir(rng) * 0.35)); b.a.push_back(1.4 + 0.3 * U(rng)); }
        b.name = "needle";
    } else {
        for (int i = 0; i < 9; ++i) { b.n.push_back(rand_dir(rng)); b.a.push_back(0.55 + 0.6 * U(rng)); }
        b.name = "crystal";
    }
    b.build();
    b.normalize(target_vol);
    return b;
}
struct Morph {
    std::vector<V3> dirs;
    std::vector<double> h;
    Poly P;
    double vol = 0;
    int planes_cut = 0;
    void build(const Body& K, const Body& L, double lam, bool geometric) {
        const size_t m = dirs.size();
        h.resize(m);
        double hmax = 0;
        for (size_t j = 0; j < m; ++j) {
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
// ------------------------------------------------ end of copied code

int main(int argc, char** argv) {
    const int max_n = arg_int(argc, argv, "--max-n", 6400);
    const double min_ms = arg_double(argc, argv, "--min-ms", 1500);
    const int min_frames = arg_int(argc, argv, "--min-frames", 25);
    const unsigned seed = 91;
    const std::string out = ladder::out_dir(argc, argv);

    std::vector<Body> keys = {make_body(0, seed + 1, 1.2), make_body(1, seed + 2, 0.8), make_body(2, seed + 3, 1.5),
                              make_body(0, seed + 4, 0.9), make_body(2, seed + 5, 1.1)};
    const int nkeys = (int)keys.size();
    const double lams[5] = {0.05, 0.25, 0.5, 0.75, 0.95};
    const int ncfg = nkeys * 5;

    ladder::Ladder L;
    L.demo = "L5-sweeps/08-morph-sweep";
    L.family = "091";
    L.load_name = "half-space directions per in-between body";
    L.threads_without = L.threads_with = 1;

    std::printf("08-morph-sweep: 5 keyframe segments x 5 lambdas per pass, 1 thread per side\n");
    for (int n = 50; n <= max_n; n *= 2) {
        // per-segment direction sets with ~n directions in total
        std::vector<std::vector<V3>> dirs(nkeys);
        double ndir_mean = 0;
        for (int s = 0; s < nkeys; ++s) {
            const Body &K = keys[s], &Lb = keys[(s + 1) % nkeys];
            int nf = 2 * int(K.n.size() + Lb.n.size());
            int M = std::max(1, int(std::lround((n - nf) / 2.0)));
            dirs[s] = direction_set(K, Lb, M, seed * 31 + s);
            ndir_mean += double(dirs[s].size()) / nkeys;
        }
        Morph mW, mO;  // mO = WITHOUT (Minkowski), mW = WITH (log)
        std::vector<double> tO, tW;
        double minO = 1e9, maxO = 0, minW = 1e9, maxW = 0, maxOW = 0, cutO = 0, cutW = 0, facesO = 0, facesW = 0;
        auto run = [&](int i, bool record_quality) {
            const int s = (i / 5) % nkeys;
            const double lam = lams[i % 5];
            const Body &K = keys[s], &Lb = keys[(s + 1) % nkeys];
            mO.dirs = dirs[s];
            mW.dirs = dirs[s];
            double a, b;
            auto timeO = [&] { double t0 = now_ms(); mO.build(K, Lb, lam, false); return now_ms() - t0; };
            auto timeW = [&] { double t0 = now_ms(); mW.build(K, Lb, lam, true); return now_ms() - t0; };
            if (i & 1) { b = timeW(); a = timeO(); } else { a = timeO(); b = timeW(); }  // alternate order
            if (record_quality) {
                const double fl = std::pow(K.vol, 1 - lam) * std::pow(Lb.vol, lam);
                minO = std::min(minO, mO.vol / fl); maxO = std::max(maxO, mO.vol / fl);
                minW = std::min(minW, mW.vol / fl); maxW = std::max(maxW, mW.vol / fl);
                maxOW = std::max(maxOW, mO.vol / mW.vol);
                cutO += double(mO.planes_cut) / ncfg; cutW += double(mW.planes_cut) / ncfg;
                facesO += double(mO.P.faces.size()) / ncfg; facesW += double(mW.P.faces.size()) / ncfg;
            }
            return std::pair<double, double>(a, b);
        };
        run(0, false); run(1, false);  // warm-up
        double t_start = now_ms();
        int i = 0;
        double sumO = 0, sumW = 0;
        while (true) {
            auto [a, b] = run(i, i < ncfg);
            tO.push_back(a); tW.push_back(b);
            sumO += a; sumW += b;
            ++i;
            if (i >= ncfg && i >= min_frames && sumO >= min_ms && sumW >= min_ms) break;
            if (i >= ncfg && now_ms() - t_start > 60000) break;  // slow stage: one full pass is enough
        }
        ladder::Stage st;
        st.n = n;
        st.without.st = ladder::stats(tO);
        st.with.st = ladder::stats(tW);
        st.without.extra = "\"planes_that_cut_mean\": " + ladder::jnum(cutO) + ", \"faces_mean\": " + ladder::jnum(facesO);
        st.with.extra = "\"planes_that_cut_mean\": " + ladder::jnum(cutW) + ", \"faces_mean\": " + ladder::jnum(facesW);
        st.quality = "{\"name\": \"min volume/floor over 0<lambda<1 (>= 1 is the guarantee)\", \"without\": " + ladder::jnum(minO) +
                     ", \"with\": " + ladder::jnum(minW) + ", \"higher_is_better\": true, \"min_ok_threshold\": 1, " +
                     "\"max_bloat_without\": " + ladder::jnum(maxO) + ", \"max_bloat_with\": " + ladder::jnum(maxW) +
                     ", \"max_minkowski_over_log_volume\": " + ladder::jnum(maxOW) + "}";
        st.extra = "\"directions_actual_mean\": " + ladder::jnum(ndir_mean);
        L.stages.push_back(st);
        std::printf("n=%5d (actual %.0f dirs)  WITHOUT %9.4f ms  WITH %9.4f ms  ratio %.2f  frames %d  | min/floor %.4f / %.4f  bloat %.3f / %.3f  cuts %.0f / %.0f\n",
                    n, ndir_mean, st.without.st.median, st.with.st.median, st.without.st.median / st.with.st.median, st.with.st.frames, minO,
                    minW, maxO, maxW, cutO, cutW);
        std::fflush(stdout);
    }
    L.notes =
        "Both sides intersect the same ~n half-spaces with the same O(V)-per-plane clipper copied unchanged from 08-convex-morph, "
        "so both curves share the clipper's cost model: every plane scans all current vertices and every plane that cuts rebuilds "
        "the whole polytope (worst case ~n^2); measured growth is only ~n^1.1-1.3 because the number of planes that cut and the "
        "face count grow sublinearly (they converge to the smooth body); "
        "the WITH body is smaller (contained in the Minkowski body by AM-GM), so fewer planes cut it and it has fewer faces, "
        "which is the only reason it is cheaper. A dual convex-hull half-space intersection, O(n log n), would lower both curves "
        "alike; it is not used on either side. Proved (thm:main/eq:main): log-body volume >= |K|^(1-l)|L|^l for every direction "
        "set (a finite set gives a body containing the continuum Wulff body); Minkowski also has a floor (Brunn-Minkowski). "
        "Measured here: min ratio and max bloat at each n over 25 bodies (5 segments x 5 lambdas). Timing is single-threaded, "
        "1 frame = 1 full body build.";
    L.write(out + "/ladder.json");
    L.print_table();
    std::printf("wrote %s/ladder.json\n", out.c_str());
    return 0;
}
