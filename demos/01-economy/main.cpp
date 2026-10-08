// 01-economy - a continuous crafting economy as a mass-action reaction network.
//
// Family 149, "Uniform Permanence in Weakly Reversible Mass-Action Systems"
// (preprints/Uniform-Permanence-in-Weakly-Reversible-Mass-Action-Systems-October-5-2026/
//  build/sections/{introduction,trapping,permanence}.tex).
//
// Five resources O (ore), W (wood), T (tools), F (food), P (workers) evolve by
//   xdot = sum_{y->y'} k_{y->y'} x^y (y' - y)                            (§1 eq:mass-action)
// over nine "recipes" arranged as three coupled linkage classes, each a directed 3-cycle
// of complexes:
//   LC1 village   P+F -> 2P -> P -> P+F
//   LC2 forge     O+W -> T  -> 2O -> O+W
//   LC3 workshop  P+T+F -> P+O+W -> 2P+T -> P+T+F
// RIGHT (WITH):    all nine recipes. Every reaction lies on a directed cycle (weakly reversible),
//                  so thm:main / eq:uniform-bounds give one positive lower and one finite upper
//                  bound eps_P <= x_i(t) <= 1/eps_P for every start in the class, after a
//                  start-dependent transient.
// LEFT (WITHOUT):  the same recipes and rates with two return reactions removed (2P -> P and
//                  P+T+F -> P+O+W). Not weakly reversible; this particular network runs away / goes
//                  extinct (a demonstrated case, not a theorem about all such networks).
// Both sides: identical Rosenbrock (ode23s-type, L-stable) adaptive integrator with error
// control, positivity rejection + step halving, same six starts in ONE stoichiometric class.
// The structural checks shown on screen (SCCs of the complex graph, linkage classes, dim S,
// deficiency, conservation law, unbounded class, strong-endotactic witness, complex-balance
// residual) are all computed below, not hard-coded.
#include "demo.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
#include <string>
#include <vector>

using namespace demo;

constexpr int D = 5;
using Vec = std::array<double, D>;
using Cx = std::array<int, D>;
static const char* SPN[D] = {"Ore", "Wood", "Tools", "Food", "Workers"};
static const char* SPS[D] = {"O", "W", "T", "F", "P"};
static Color spcol(int i) {
    static const Color c[D] = {rgb(0x6cb6ff), rgb(0xffa657), rgb(0xb392f0), rgb(0x5be49b), rgb(0xffc861)};
    return c[i];
}

struct Rx { Cx y, yp; double k; int lc; };

static Cx cx(int O, int W, int T, int F, int P) { return {O, W, T, F, P}; }
static std::string cxname(const Cx& c) {
    std::string s;
    for (int i = 0; i < D; ++i)
        if (c[i]) {
            if (!s.empty()) s += "+";
            if (c[i] > 1) s += std::to_string(c[i]);
            s += SPS[i];
        }
    return s.empty() ? "0" : s;
}

// ---------------------------------------------------------------- the network
static std::vector<Rx> full_network() {
    return {
        {cx(0, 0, 0, 1, 1), cx(0, 0, 0, 0, 2), 1.00, 0},  // 0 eat + recruit
        {cx(0, 0, 0, 0, 2), cx(0, 0, 0, 0, 1), 0.35, 0},  // 1 crowding         (return)
        {cx(0, 0, 0, 0, 1), cx(0, 0, 0, 1, 1), 0.60, 0},  // 2 farm
        {cx(1, 1, 0, 0, 0), cx(0, 0, 1, 0, 0), 1.20, 1},  // 3 craft tool
        {cx(0, 0, 1, 0, 0), cx(2, 0, 0, 0, 0), 0.25, 1},  // 4 melt tool        (return)
        {cx(2, 0, 0, 0, 0), cx(1, 1, 0, 0, 0), 0.80, 1},  // 5 trade ore->wood
        {cx(0, 0, 1, 1, 1), cx(1, 1, 0, 0, 1), 0.90, 2},  // 6 salvage
        {cx(1, 1, 0, 0, 1), cx(0, 0, 1, 0, 2), 0.50, 2},  // 7 craft + recruit
        {cx(0, 0, 1, 0, 2), cx(0, 0, 1, 1, 1), 0.70, 2},  // 8 retire to farm
    };
}
static const int REMOVED[2] = {1, 6};

// ---------------------------------------------------------------- structure (computed)
struct Structure {
    std::vector<Cx> cplx;              // complexes
    std::vector<int> src, dst;         // reaction -> complex index
    std::vector<int> scc, lcls;        // per complex
    int n_scc = 0, n_lc = 0, dimS = 0, deficiency = 0;
    bool wr = false;
    int no_return = 0;                 // reactions whose target cannot reach the source
    std::vector<bool> on_cycle;        // per reaction
    std::vector<Vec> Zperp;            // basis of S-perp
    bool unbounded = false;            // class contains a nonnegative recession direction
    std::array<int, D> witness{};      // non-strong-endotactic witness (if found)
    bool has_witness = false;
    bool coupled = false;              // linkage classes share species
};

static int rank_of(std::vector<Vec> M, std::vector<Vec>* nullsp = nullptr) {
    int r = 0;
    std::vector<int> piv;
    for (int c = 0; c < D && r < (int)M.size(); ++c) {
        int best = -1;
        double bv = 1e-10;
        for (int i = r; i < (int)M.size(); ++i)
            if (std::fabs(M[i][c]) > bv) { bv = std::fabs(M[i][c]); best = i; }
        if (best < 0) continue;
        std::swap(M[r], M[best]);
        double p = M[r][c];
        for (int j = 0; j < D; ++j) M[r][j] /= p;
        for (int i = 0; i < (int)M.size(); ++i)
            if (i != r && std::fabs(M[i][c]) > 0) {
                double f = M[i][c];
                for (int j = 0; j < D; ++j) M[i][j] -= f * M[r][j];
            }
        piv.push_back(c);
        ++r;
    }
    if (nullsp) {  // null space of M (row space = S) -> S-perp
        nullsp->clear();
        for (int f = 0; f < D; ++f) {
            if (std::find(piv.begin(), piv.end(), f) != piv.end()) continue;
            Vec z{};
            z[f] = 1;
            for (int i = 0; i < r; ++i) z[piv[i]] = -M[i][f];
            nullsp->push_back(z);
        }
    }
    return r;
}

static Structure analyse(const std::vector<Rx>& R) {
    Structure s;
    auto id = [&](const Cx& c) {
        for (size_t i = 0; i < s.cplx.size(); ++i)
            if (s.cplx[i] == c) return (int)i;
        s.cplx.push_back(c);
        return (int)s.cplx.size() - 1;
    };
    for (auto& r : R) { s.src.push_back(id(r.y)); s.dst.push_back(id(r.yp)); }
    const int n = (int)s.cplx.size(), m = (int)R.size();
    // reachability (Floyd-Warshall closure; n is tiny) -> strongly connected components
    std::vector<std::vector<char>> reach(n, std::vector<char>(n, 0));
    for (int i = 0; i < n; ++i) reach[i][i] = 1;
    for (int e = 0; e < m; ++e) reach[s.src[e]][s.dst[e]] = 1;
    for (int k = 0; k < n; ++k)
        for (int i = 0; i < n; ++i)
            if (reach[i][k])
                for (int j = 0; j < n; ++j)
                    if (reach[k][j]) reach[i][j] = 1;
    s.scc.assign(n, -1);
    for (int i = 0; i < n; ++i)
        if (s.scc[i] < 0) {
            for (int j = 0; j < n; ++j)
                if (reach[i][j] && reach[j][i]) s.scc[j] = s.n_scc;
            ++s.n_scc;
        }
    s.on_cycle.resize(m);
    for (int e = 0; e < m; ++e) {
        s.on_cycle[e] = reach[s.dst[e]][s.src[e]];
        if (!s.on_cycle[e]) ++s.no_return;
    }
    s.wr = s.no_return == 0;
    // linkage classes (undirected components)
    s.lcls.assign(n, -1);
    for (int i = 0; i < n; ++i)
        if (s.lcls[i] < 0) {
            std::vector<int> st{i};
            s.lcls[i] = s.n_lc;
            while (!st.empty()) {
                int u = st.back(); st.pop_back();
                for (int e = 0; e < m; ++e) {
                    int v = s.src[e] == u ? s.dst[e] : s.dst[e] == u ? s.src[e] : -1;
                    if (v >= 0 && s.lcls[v] < 0) { s.lcls[v] = s.n_lc; st.push_back(v); }
                }
            }
            ++s.n_lc;
        }
    std::vector<Vec> V;
    for (auto& r : R) {
        Vec v;
        for (int i = 0; i < D; ++i) v[i] = r.yp[i] - r.y[i];
        V.push_back(v);
    }
    s.dimS = rank_of(V, &s.Zperp);
    s.deficiency = n - s.n_lc - s.dimS;
    // unbounded class <=> S contains a nonzero nonnegative vector. With S-perp = span(Z), a
    // coordinate direction e_i lies in S iff every z has z_i = 0 (sufficient check used here).
    for (int i = 0; i < D; ++i) {
        bool zero = true;
        for (auto& z : s.Zperp) zero &= std::fabs(z[i]) < 1e-12;
        if (zero) s.unbounded = true;
    }
    // strongly endotactic needs, for every w not orthogonal to S, a reaction from a w-maximal
    // source pointing strictly inward. Search small integer w for a counterexample.
    for (int code = 0; code < 243 && !s.has_witness; ++code) {
        std::array<int, D> w;
        int c = code;
        for (int i = 0; i < D; ++i) { w[i] = c % 3 - 1; c /= 3; }
        bool notperp = false;
        for (auto& v : V) {
            double d = 0;
            for (int i = 0; i < D; ++i) d += w[i] * v[i];
            notperp |= std::fabs(d) > 0;
        }
        if (!notperp) continue;
        int top = -1 << 30;
        for (auto& r : R) {
            int d = 0;
            for (int i = 0; i < D; ++i) d += w[i] * r.y[i];
            top = std::max(top, d);
        }
        bool inward = false;
        for (auto& r : R) {
            int d = 0, dv = 0;
            for (int i = 0; i < D; ++i) { d += w[i] * r.y[i]; dv += w[i] * (r.yp[i] - r.y[i]); }
            if (d == top && dv < 0) inward = true;
        }
        if (!inward) { s.has_witness = true; s.witness = w; }
    }
    // coupling: do two linkage classes involve a common species?
    std::vector<std::array<bool, D>> used(s.n_lc, std::array<bool, D>{});
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < D; ++j)
            if (s.cplx[i][j]) used[s.lcls[i]][j] = true;
    for (int a = 0; a < s.n_lc; ++a)
        for (int b = a + 1; b < s.n_lc; ++b)
            for (int j = 0; j < D; ++j) s.coupled |= used[a][j] && used[b][j];
    return s;
}

// ---------------------------------------------------------------- mass action + integrator
struct Model {
    std::vector<Rx> R;
    void flux(const Vec& x, std::vector<double>& fl) const {
        fl.resize(R.size());
        for (size_t r = 0; r < R.size(); ++r) {
            double m = R[r].k;
            for (int i = 0; i < D; ++i)
                for (int e = 0; e < R[r].y[i]; ++e) m *= x[i];
            fl[r] = m;
        }
    }
    Vec f(const Vec& x) const {
        Vec out{};
        for (auto& r : R) {
            double m = r.k;
            for (int i = 0; i < D; ++i)
                for (int e = 0; e < r.y[i]; ++e) m *= x[i];
            for (int i = 0; i < D; ++i) out[i] += m * (r.yp[i] - r.y[i]);
        }
        return out;
    }
    void jac(const Vec& x, double J[D][D]) const {
        for (int i = 0; i < D; ++i)
            for (int j = 0; j < D; ++j) J[i][j] = 0;
        for (auto& r : R)
            for (int j = 0; j < D; ++j) {
                if (!r.y[j]) continue;
                double m = r.k * r.y[j];  // d/dx_j of x^y
                for (int i = 0; i < D; ++i) {
                    int e = r.y[i] - (i == j ? 1 : 0);
                    for (int q = 0; q < e; ++q) m *= x[i];
                }
                for (int i = 0; i < D; ++i) J[i][j] += m * (r.yp[i] - r.y[i]);
            }
    }
};

struct LU {
    double a[D][D];
    int p[D];
    bool factor() {
        for (int i = 0; i < D; ++i) p[i] = i;
        for (int c = 0; c < D; ++c) {
            int b = c;
            for (int i = c + 1; i < D; ++i)
                if (std::fabs(a[i][c]) > std::fabs(a[b][c])) b = i;
            if (std::fabs(a[b][c]) < 1e-300) return false;
            if (b != c) { std::swap(a[b], a[c]); std::swap(p[b], p[c]); }
            for (int i = c + 1; i < D; ++i) {
                a[i][c] /= a[c][c];
                for (int j = c + 1; j < D; ++j) a[i][j] -= a[i][c] * a[c][j];
            }
        }
        return true;
    }
    Vec solve(const Vec& b) const {
        Vec y;
        for (int i = 0; i < D; ++i) {
            double s = b[p[i]];
            for (int j = 0; j < i; ++j) s -= a[i][j] * y[j];
            y[i] = s;
        }
        for (int i = D - 1; i >= 0; --i) {
            double s = y[i];
            for (int j = i + 1; j < D; ++j) s -= a[i][j] * y[j];
            y[i] = s / a[i][i];
        }
        return y;
    }
};

struct Run {
    Vec x, x0;
    double t = 0, h = 1e-3;
    bool runaway = false;
    long long acc = 0, rej = 0, pos_rej = 0;
};

constexpr double RTOL = 1e-6, ATOL = 1e-14, RUNAWAY = 1e12;

// Shampine-Reichelt Rosenbrock 2(3) (MATLAB ode23s) for autonomous systems.
static void advance(const Model& M, Run& r, double t1, long long& steps) {
    static const double d = 1.0 / (2.0 + std::sqrt(2.0)), e32 = 6.0 + std::sqrt(2.0);
    int guard = 0;
    while (r.t < t1 && !r.runaway && guard++ < 200000) {
        double h = std::min(r.h, t1 - r.t);
        double J[D][D];
        M.jac(r.x, J);
        LU W;
        for (int i = 0; i < D; ++i)
            for (int j = 0; j < D; ++j) W.a[i][j] = (i == j ? 1.0 : 0.0) - h * d * J[i][j];
        if (!W.factor()) { r.h *= 0.5; ++r.rej; continue; }
        Vec F0 = M.f(r.x);
        Vec k1 = W.solve(F0), y1, k2, yn, k3, rhs;
        for (int i = 0; i < D; ++i) y1[i] = r.x[i] + 0.5 * h * k1[i];
        bool pos = true;
        for (int i = 0; i < D; ++i) pos &= y1[i] > 0;
        Vec F1 = M.f(y1);
        for (int i = 0; i < D; ++i) rhs[i] = F1[i] - k1[i];
        k2 = W.solve(rhs);
        for (int i = 0; i < D; ++i) { k2[i] += k1[i]; yn[i] = r.x[i] + h * k2[i]; pos &= yn[i] > 0; }
        ++steps;
        if (!pos) { r.h = h * 0.5; ++r.rej; ++r.pos_rej; continue; }  // positivity: halve and retry
        Vec F2 = M.f(yn);
        for (int i = 0; i < D; ++i) rhs[i] = F2[i] - e32 * (k2[i] - F1[i]) - 2.0 * (k1[i] - F0[i]);
        k3 = W.solve(rhs);
        double en = 0;
        for (int i = 0; i < D; ++i) {
            double err = h / 6.0 * (k1[i] - 2.0 * k2[i] + k3[i]);
            double sc = ATOL + RTOL * std::max(std::fabs(r.x[i]), std::fabs(yn[i]));
            en = std::max(en, std::fabs(err) / sc);
        }
        if (en <= 1.0) {
            r.x = yn;
            r.t += h;
            ++r.acc;
            double fac = en > 0 ? 0.8 * std::pow(en, -1.0 / 3.0) : 5.0;
            r.h = h * std::clamp(fac, 0.2, 5.0);
            for (int i = 0; i < D; ++i)
                if (r.x[i] > RUNAWAY) r.runaway = true;
        } else {
            ++r.rej;
            r.h = h * std::max(0.1, 0.8 * std::pow(en, -1.0 / 3.0));  // error test failed: shrink
        }
    }
}

// ---------------------------------------------------------------- experiment
struct Side {
    Model M;
    Structure S;
    std::vector<Run> runs;
    std::vector<std::vector<std::array<float, D>>> hist;  // [run][frame] log10 x
    std::vector<float> hist_s;                            // warped time coordinate per sample
    long long steps_total = 0, steps_frame = 0;
    double ms_frame = 0, ms_total = 0;
    double min_now = 1, max_now = 1, resid = 0;
    int extinct = 0, runaway = 0;
    double env_lo = 1e300, env_hi = 0;  // empirical envelope after T_SETTLE
    bool removed[16]{};
    // particles (featured run)
    struct Part { int arc; float u, speed, jitter; };
    std::vector<Part> parts;
    std::vector<double> emit_acc;
};

struct Arc { int r, a, b; float bend; };  // reaction r draws flow from species a to b (b<0: out)

static std::vector<Arc> make_arcs(const std::vector<Rx>& R) {
    std::vector<Arc> arcs;
    int pairc[D + 1][D + 1] = {};
    for (int r = 0; r < (int)R.size(); ++r) {
        std::vector<int> cons, prod, cat;
        for (int i = 0; i < D; ++i) {
            int dv = R[r].yp[i] - R[r].y[i];
            if (dv < 0) cons.push_back(i);
            if (dv > 0) prod.push_back(i);
            if (dv == 0 && R[r].y[i] > 0) cat.push_back(i);
        }
        if (cons.empty()) cons = cat;                          // pure production: from catalyst
        if (prod.empty()) { arcs.push_back({r, cons[0], -1, 0}); continue; }  // pure decay
        for (int a : cons)
            for (int b : prod) {
                int k = pairc[a][b]++ + pairc[b][a];
                arcs.push_back({r, a, b, 0.18f + 0.13f * k});
            }
    }
    return arcs;
}

static constexpr double T_END = 140.0, T_SETTLE = 40.0, WARP = 2.0;
static double warp_t(double s) { return T_END * (std::exp(WARP * s) - 1.0) / (std::exp(WARP) - 1.0); }
static double unwarp(double t) { return std::log(1.0 + t / T_END * (std::exp(WARP) - 1.0)) / WARP; }

int main(int argc, char** argv) {
    Config cfg;
    cfg.name = "01-economy";
    cfg.title_left = "recipes with 2 return reactions removed";
    cfg.title_right = "weakly reversible recipe graph";
    cfg.caption = "Family 149 — Uniform Permanence in Weakly Reversible Mass-Action Systems · WITH: every recipe on a "
                  "directed cycle ⇒ thm:main, eq:uniform-bounds (§1, §§3–4) · same rates, same 6 starts, one class";
    cfg.frames = 1560;
    cfg.show_speedup = false;  // the contrast is behaviour, not speed (LEFT's cost comes from stiffness)
    Harness h(argc, argv, cfg);

    std::vector<Rx> FULL = full_network();
    Side side[2];
    side[1].M.R = FULL;
    for (int r = 0; r < (int)FULL.size(); ++r) {
        bool rm = r == REMOVED[0] || r == REMOVED[1];
        side[0].removed[r] = rm;
        if (!rm) side[0].M.R.push_back(FULL[r]);
    }
    for (auto& s : side) s.S = analyse(s.M.R);
    Structure Sfull = side[1].S;
    std::vector<Arc> arcs = make_arcs(FULL);

    // six positive starts in ONE stoichiometric class (all have O + W + 2T = 4.5)
    const double starts[6][D] = {
        {1.0, 1.0, 1.25, 1.0, 1.0},     {4.0, 0.3, 0.1, 0.01, 20.0},  {0.02, 3.0, 0.74, 8.0, 0.05},
        {0.5, 0.5, 1.75, 1e-3, 2e-3},   {1e-3, 1e-3, 2.249, 30.0, 0.3}, {2.0, 2.4, 0.05, 0.2, 3.0}};
    for (auto& s : side) {
        for (auto& st : starts) {
            Run r;
            for (int i = 0; i < D; ++i) r.x[i] = r.x0[i] = st[i];
            s.runs.push_back(r);
        }
        s.hist.assign(6, {});
        s.emit_acc.assign(arcs.size(), 0.0);
    }
    // conservation laws (S-perp) of the WITH network; class check for the starts
    const auto& Z = Sfull.Zperp;
    double class_spread = 0;
    for (auto& z : Z) {
        double v0 = 0;
        for (int i = 0; i < D; ++i) v0 += z[i] * starts[0][i];
        for (auto& st : starts) {
            double v = 0;
            for (int i = 0; i < D; ++i) v += z[i] * st[i];
            class_spread = std::max(class_spread, std::fabs(v - v0));
        }
    }

    std::mt19937 rng(149);
    std::uniform_real_distribution<float> U(0.f, 1.f);
    const int F = h.frames();
    int featured = 0;

    for (int k = 0; k < 2; ++k) h.panel(k).set_compute_label("integrate 6 starts");
    while (h.next_frame()) {
        const int fr = h.frame();
        const double s1 = double(fr + 1) / F, t1 = warp_t(s1);
        featured = std::min(5, fr * 6 / F);
        for (int k = 0; k < 2; ++k) {
            Side& sd = side[k];
            Panel& p = h.panel(k);
            sd.steps_frame = 0;
            double tm0 = 0, tm1 = 0;
            p.measure([&] {
                tm0 = now_ms();
                for (auto& r : sd.runs) advance(sd.M, r, t1, sd.steps_frame);
                tm1 = now_ms();
            });
            sd.ms_frame = tm1 - tm0;
            sd.ms_total += sd.ms_frame;
            sd.steps_total += sd.steps_frame;
            // ---- metrics (untimed)
            sd.min_now = 1e300; sd.max_now = 0; sd.extinct = 0; sd.runaway = 0; sd.resid = 0;
            for (int ri = 0; ri < 6; ++ri) {
                auto& r = sd.runs[ri];
                std::array<float, D> lg;
                for (int i = 0; i < D; ++i) {
                    sd.min_now = std::min(sd.min_now, r.x[i]);
                    sd.max_now = std::max(sd.max_now, r.x[i]);
                    if (r.x[i] < 1e-6) ++sd.extinct;
                    lg[i] = (float)std::log10(std::max(r.x[i], 1e-300));
                    if (r.t >= T_SETTLE) { sd.env_lo = std::min(sd.env_lo, r.x[i]); sd.env_hi = std::max(sd.env_hi, r.x[i]); }
                }
                sd.runaway += r.runaway;
                for (auto& z : Z) {  // conservation residual relative to the conserved total
                    double a = 0, b = 0, sc = 0;
                    for (int i = 0; i < D; ++i) { a += z[i] * r.x[i]; b += z[i] * r.x0[i]; sc += std::fabs(z[i] * r.x0[i]); }
                    sd.resid = std::max(sd.resid, std::fabs(a - b) / sc);
                }
                sd.hist[ri].push_back(lg);
            }
            sd.hist_s.push_back((float)s1);
        }

        // ================================================================ drawing
        for (int k = 0; k < 2; ++k) {
            Side& sd = side[k];
            Panel& p = h.panel(k);
            const Color acc = k == 0 ? pal::without : pal::with;
            p.clear(pal::bg);
            const Run& fr_run = sd.runs[featured];
            std::vector<double> fl;
            sd.M.flux(fr_run.x, fl);
            std::vector<double> flux_full(FULL.size(), 0.0);
            for (size_t r = 0, j = 0; r < FULL.size(); ++r)
                if (!sd.removed[r]) flux_full[r] = fl[j++];

            // ---- orb layout (pentagon, right of the HUD)
            const float cxp = 705, cyp = 290, R0 = 170;
            Vec2 orb[D];
            for (int i = 0; i < D; ++i) {
                float a = -1.5708f + i * 6.2832f / D;
                orb[i] = {cxp + R0 * std::cos(a), cyp + R0 * std::sin(a)};
            }
            auto orb_r = [&](double x) {
                double l = std::log10(std::max(x, 1e-300));
                return (float)std::clamp(30.0 + 6.0 * l, 5.0, 54.0);
            };
            auto bez = [&](const Arc& a, float u) -> Vec2 {
                Vec2 A = orb[a.a], B;
                if (a.b < 0) {  // decay: outward from the orb
                    Vec2 dir{A.x - cxp, A.y - cyp};
                    float L = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                    B = {A.x + dir.x / L * 120, A.y + dir.y / L * 120};
                } else B = orb[a.b];
                Vec2 m{(A.x + B.x) * 0.5f, (A.y + B.y) * 0.5f}, n{-(B.y - A.y), B.x - A.x};
                Vec2 c{m.x + n.x * a.bend, m.y + n.y * a.bend};
                float v = 1 - u;
                return {v * v * A.x + 2 * v * u * c.x + u * u * B.x, v * v * A.y + 2 * v * u * c.y + u * u * B.y};
            };
            static const Color lccol[3] = {rgb(0x5be49b), rgb(0xffa657), rgb(0xb392f0)};
            // arcs
            for (size_t ai = 0; ai < arcs.size(); ++ai) {
                const Arc& a = arcs[ai];
                bool rm = sd.removed[a.r];
                Vec2 pts[33];
                for (int q = 0; q <= 32; ++q) pts[q] = bez(a, q / 32.f);
                if (rm) {
                    for (int q = 0; q < 32; q += 2) p.line(pts[q].x, pts[q].y, pts[q + 1].x, pts[q + 1].y, 1.6f, pal::bad, 0.45f);
                    Vec2 mid = pts[16];
                    p.text(mid.x, mid.y - 11, "✕", 18, pal::bad, Font::Bold, Align::Center);
                } else {
                    double lf = std::log10(std::max(flux_full[a.r], 1e-30));
                    float w = (float)std::clamp(0.6 + 0.5 * (lf + 4), 0.6, 4.0);
                    p.polyline(pts, 33, w, lccol[FULL[a.r].lc], 0.28f);
                }
            }
            // particles
            const float dt = 1.f / 60.f;
            for (size_t ai = 0; ai < arcs.size(); ++ai) {
                if (sd.removed[arcs[ai].r]) continue;
                double lf = std::log10(std::max(flux_full[arcs[ai].r], 1e-30));
                double rate = std::clamp(4.0 * (lf + 3.0), 0.0, 30.0);  // particles / s (log flux)
                sd.emit_acc[ai] += rate * dt;
                while (sd.emit_acc[ai] >= 1.0) {
                    sd.emit_acc[ai] -= 1.0;
                    sd.parts.push_back({(int)ai, 0.f, 0.55f + 0.3f * U(rng), (U(rng) - 0.5f) * 10.f});
                }
            }
            p.blend = Blend::Add;
            for (auto& q : sd.parts) {
                q.u += q.speed * dt;
                const Arc& a = arcs[q.arc];
                Vec2 P = bez(a, std::min(q.u, 1.f));
                Color c = a.b < 0 ? spcol(a.a) : lerp(spcol(a.a), spcol(a.b), q.u);
                float al = a.b < 0 ? 1.f - q.u : 1.f;
                p.glow(P.x, P.y + q.jitter * 0.2f, 7, c, 0.55f * al);
                p.circle(P.x, P.y + q.jitter * 0.2f, 1.7f, lerp(c, rgb(0xffffff), 0.5f), 0.9f * al);
            }
            p.blend = Blend::Normal;
            sd.parts.erase(std::remove_if(sd.parts.begin(), sd.parts.end(), [](auto& q) { return q.u >= 1.f; }), sd.parts.end());
            // orbs
            for (int i = 0; i < D; ++i) {
                double x = fr_run.x[i];
                float r = orb_r(x);
                bool ext = x < 1e-6, run = x > 1e6;
                Color c = spcol(i);
                if (ext) {
                    p.ring(orb[i].x, orb[i].y, 26, 2, pal::bad, 0.8f);
                    p.circle(orb[i].x, orb[i].y, r, scale(c, 0.5f), 0.7f);
                } else {
                    p.glow(orb[i].x, orb[i].y, r * 2.4f, run ? pal::bad : c, run ? 0.9f : 0.55f);
                    p.circle(orb[i].x, orb[i].y, r, scale(c, 0.55f), 0.95f);
                    p.circle(orb[i].x - r * 0.25f, orb[i].y - r * 0.25f, r * 0.45f, lerp(c, rgb(0xffffff), 0.45f), 0.35f);
                    p.ring(orb[i].x, orb[i].y, r, 2, c, 0.95f);
                }
                float ly = orb[i].y + std::max(r, 26.f) + 4;
                p.text(orb[i].x, ly, SPN[i], 17, c, Font::Bold, Align::Center);
                std::string val = fmt("%.2e", x);
                if (ext) val += "  EXTINCT";
                if (run) val += "  RUNAWAY";
                p.text(orb[i].x, ly + 20, val, 14, ext || run ? pal::bad : pal::dim, Font::Mono, Align::Center);
            }
            p.text(cxp, 590, fmt("orbs = start #%d of 6 at t = %.1f · radius ∝ log quantity · particles ∝ log flux", featured + 1, fr_run.t),
                   13, pal::dim, Font::Sans, Align::Center);

            // ---- complex graph (the object the structural check runs on)
            {
                const float bx = 16, by = 400, bw = 440, bh = 160;
                p.fill_rounded_rect(bx, by, bw, bh, 10, rgb(0x05070b), 0.84f);
                p.fill_rect(bx + 10, by, bw - 20, 2, acc, 0.9f);
                const Structure& S = sd.S;
                p.text(bx + 14, by + 8, fmt("recipe graph: %d complexes, %d SCCs, %d linkage classes", (int)S.cplx.size(), S.n_scc, S.n_lc), 14,
                       pal::dim, Font::Mono);
                static const char* lcn[3] = {"village", "forge", "workshop"};
                for (int l = 0; l < 3; ++l) {
                    float ox = bx + 76 + l * 146, oy = by + 92;
                    Vec2 v[3] = {{ox, oy - 42}, {ox + 52, oy + 34}, {ox - 52, oy + 34}};
                    for (int j = 0; j < 3; ++j) {
                        int r = l * 3 + j;
                        Vec2 A = v[j], B = v[(j + 1) % 3];
                        float dx = B.x - A.x, dy = B.y - A.y, L = std::sqrt(dx * dx + dy * dy);
                        dx /= L; dy /= L;
                        Vec2 a0{A.x + dx * 15, A.y + dy * 15}, a1{B.x - dx * 15, B.y - dy * 15};
                        bool rm = sd.removed[r];
                        bool oncyc = false;
                        for (size_t q = 0, jj = 0; q < FULL.size(); ++q)
                            if (!sd.removed[q]) { if ((int)q == r) oncyc = S.on_cycle[jj]; ++jj; }
                        Color c = rm ? pal::bad : oncyc ? lccol[l] : pal::warn;
                        if (rm) {
                            for (int q = 0; q < 8; q += 2) {
                                float u0 = q / 8.f, u1 = (q + 1) / 8.f;
                                p.line(a0.x + (a1.x - a0.x) * u0, a0.y + (a1.y - a0.y) * u0, a0.x + (a1.x - a0.x) * u1,
                                       a0.y + (a1.y - a0.y) * u1, 1.5f, c, 0.7f);
                            }
                        } else {
                            p.line(a0.x, a0.y, a1.x, a1.y, 2, c, 0.9f);
                            Vec2 t1{a1.x - dx * 9 - dy * 5, a1.y - dy * 9 + dx * 5}, t2{a1.x - dx * 9 + dy * 5, a1.y - dy * 9 - dx * 5};
                            p.triangle(a1, t1, t2, c, 0.95f);
                        }
                    }
                    for (int j = 0; j < 3; ++j) {
                        const Cx& c = FULL[l * 3 + j].y;
                        // a complex in a nontrivial SCC is teal, a trivial (one-way) one coral
                        int ci = -1;
                        for (size_t q = 0; q < S.cplx.size(); ++q)
                            if (S.cplx[q] == c) ci = (int)q;
                        int sz = 0;
                        for (size_t q = 0; q < S.cplx.size(); ++q) sz += S.scc[q] == S.scc[ci];
                        Color nc = sz > 1 ? pal::with : pal::without;
                        p.circle(v[j].x, v[j].y, 5, nc);
                        float ty = v[j].y + (j == 0 ? -24 : 7);
                        p.text(v[j].x, ty, cxname(c), 15, pal::text, Font::Sans, Align::Center);
                    }
                    p.text(ox, oy + 4, lcn[l], 13, lccol[l], Font::Sans, Align::Center);
                }
            }
            // ---- scope box (computed checks)
            {
                const float bx = 16, by = 570, bw = 440;
                const Structure& S = Sfull;
                std::string w = "(";
                for (int i = 0; i < D; ++i) w += fmt("%s%d", i ? "," : "", S.witness[i]);
                w += ")";
                std::vector<std::string> L = {
                    fmt("d=%d species · dim S=%d · deficiency %d · LCs coupled: %s", D, S.dimS, S.deficiency, S.coupled ? "yes" : "no"),
                    fmt("class O+W+2T = 4.5 (all 6 starts) · unbounded: %s", S.unbounded ? "yes" : "no"),
                    S.has_witness ? fmt("not strongly endotactic: witness w=%s", w.c_str()) : std::string("strongly endotactic: no witness found"),
                };
                p.fill_rounded_rect(bx, by, bw, 18.f * L.size() + 14, 8, rgb(0x05070b), 0.6f);
                for (size_t i = 0; i < L.size(); ++i) p.text(bx + 12, by + 7 + 18.f * i, L[i], 13, pal::dim, Font::Mono);
            }

            // ---- time series (log scale), all 6 starts x 5 resources
            {
                const float gx = 70, gy = 668, gw = 860, gh = 252;
                const float lo = -14, hi = 13;
                auto Y = [&](float l) { return gy + gh * (hi - std::clamp(l, lo - 1, hi + 1)) / (hi - lo); };
                auto X = [&](float s) { return gx + gw * s; };
                p.fill_rect(gx, gy, gw, gh, rgb(0x080a10), 1);
                for (int e = (int)lo; e <= (int)hi; e += 2) {
                    p.line(gx, Y(e), gx + gw, Y(e), 1, pal::grid, 0.8f);
                    p.text(gx - 6, Y(e) - 8, fmt("1e%d", e), 12, pal::dim, Font::Mono, Align::Right);
                }
                const double tt[] = {1, 5, 10, 20, 40, 80, 140};
                for (double t : tt) {
                    float x = X((float)unwarp(t));
                    p.line(x, gy, x, gy + gh, 1, pal::grid, 0.6f);
                    p.text(x, gy + gh + 3, fmt("t=%g", t), 12, pal::dim, Font::Mono, Align::Center);
                }
                // extinction / runaway thresholds
                p.line(gx, Y(-6), gx + gw, Y(-6), 1.2f, pal::bad, 0.6f);
                p.text(gx + gw - 4, Y(-6) + 2, "extinct < 1e-6", 12, pal::bad, Font::Sans, Align::Right);
                p.line(gx, Y(6), gx + gw, Y(6), 1.2f, pal::warn, 0.4f);
                p.text(gx + gw - 4, Y(6) - 17, "runaway > 1e6 (run halted at 1e12)", 12, pal::warn, Font::Sans, Align::Right);
                // empirical envelope after T_SETTLE
                if (sd.env_hi > 0) {
                    float x0 = X((float)unwarp(T_SETTLE)), y0 = Y((float)std::log10(sd.env_hi)), y1 = Y((float)std::log10(std::max(sd.env_lo, 1e-300)));
                    p.fill_rect(x0, y0, X(sd.hist_s.back()) - x0, y1 - y0, acc, 0.08f);
                    p.line(x0, y0, X(sd.hist_s.back()), y0, 1, acc, 0.5f);
                    p.line(x0, y1, X(sd.hist_s.back()), y1, 1, acc, 0.5f);
                    p.text(x0 + 6, std::max(gy + 24, y0 - 17), fmt("empirical envelope, t ≥ %g, 6 starts: [%.1e, %.1e]", T_SETTLE, sd.env_lo, sd.env_hi),
                           12, acc, Font::Sans);
                }
                const int nS = (int)sd.hist_s.size();
                std::vector<Vec2> pts(nS);
                for (int ri = 0; ri < 6; ++ri)
                    for (int i = 0; i < D; ++i) {
                        for (int q = 0; q < nS; ++q) pts[q] = {X(sd.hist_s[q]), Y(sd.hist[ri][q][i])};
                        p.polyline(pts.data(), nS, ri == featured ? 2.0f : 1.1f, spcol(i), ri == featured ? 0.95f : 0.45f);
                    }
                for (int i = 0; i < D; ++i) {
                    p.fill_rect(gx + 8 + i * 92, gy + 6, 10, 10, spcol(i));
                    p.text(gx + 22 + i * 92, gy + 2, SPN[i], 13, spcol(i), Font::Sans);
                }
                p.text(gx + 10, Y(-12.2f), k == 1 ? "thm:main: some ε_P exists with ε_P ≤ x_i(t) ≤ 1/ε_P for t ≥ T(x⁰) — ε_P is not computed; the band is empirical"
                                                 : "not weakly reversible ⇒ thm:main does not apply; this network's collapse is a demonstrated case",
                       13, k == 1 ? pal::with : pal::without, Font::Sans);
                p.text(gx, gy - 22, "log10 quantity vs time (warped axis) — all 6 starts, all 5 resources; thick = orb start", 13, pal::dim,
                       Font::Sans);
            }

            // ---- HUD metrics
            const Structure& S = sd.S;
            p.metric_text("weakly reversible? (SCCs)",
                          S.wr ? fmt("YES: %d SCC = %d LC", S.n_scc, S.n_lc) : fmt("NO: %d of %d lack return", S.no_return, (int)sd.M.R.size()),
                          S.wr ? Tone::Good : Tone::Bad);
            p.metric("min resource (6 starts)", sd.min_now, "%.2e", sd.min_now < 1e-6 ? Tone::Bad : Tone::Good);
            p.metric("max resource (6 starts)", sd.max_now, "%.2e", sd.max_now > 1e6 ? Tone::Bad : Tone::Good);
            p.metric(fmt("extinct < 1e-6  (of %d)", 6 * D), sd.extinct, "%.0f", sd.extinct ? Tone::Bad : Tone::Good);
            p.metric("conservation residual  |Δ(O+W+2T)|", sd.resid, "%.1e", sd.resid < 1e-5 ? Tone::Good : Tone::Warn);
            if (sd.steps_total > 0) p.metric("µs / solver step (running mean)", 1000.0 * sd.ms_total / double(sd.steps_total), "%.2f");
            p.sparkline("min resource (6 starts)", true);
        }
        if (h.preview() && (fr % 10 == 0)) {
            for (int k = 0; k < 2; ++k)
                printf("fr %d side %d t=%.2f min %.2e max %.2e ext %d run %d steps %lld\n", fr, k, side[k].runs[0].t, side[k].min_now,
                       side[k].max_now, side[k].extinct, side[k].runaway, side[k].steps_frame);
        }
    }

    // ---- complex-balance residual at the WITH end state (start #1): nonzero => not complex balanced
    double cb = 0, fscale = 0;
    {
        const Structure& S = Sfull;
        std::vector<double> net(S.cplx.size(), 0.0), fl;
        side[1].M.flux(side[1].runs[0].x, fl);
        for (size_t r = 0; r < FULL.size(); ++r) { net[S.src[r]] -= fl[r]; net[S.dst[r]] += fl[r]; fscale = std::max(fscale, fl[r]); }
        for (double v : net) cb = std::max(cb, std::fabs(v));
        cb /= fscale;
    }
    Vec fe = side[1].M.f(side[1].runs[0].x);
    double fres = 0;
    for (double v : fe) fres = std::max(fres, std::fabs(v));

    h.result("species", D);
    h.result("complexes", (double)Sfull.cplx.size());
    h.result("linkage_classes", Sfull.n_lc);
    h.result("dim_S", Sfull.dimS);
    h.result("deficiency", Sfull.deficiency);
    h.result("class_unbounded", Sfull.unbounded ? "yes" : "no");
    h.result("strongly_endotactic_witness", Sfull.has_witness ? "found (not strongly endotactic)" : "none found");
    h.result("complex_balance_residual_rel_at_end", cb);
    h.result("max_abs_f_at_end_WITH", fres);
    h.result("starts_class_spread", class_spread);
    h.result("rtol", RTOL);
    h.result("atol", ATOL);
    for (int k = 0; k < 2; ++k) {
        Side& sd = side[k];
        Panel& p = h.panel(k);
        long long acc = 0, rej = 0, prej = 0;
        for (auto& r : sd.runs) { acc += r.acc; rej += r.rej; prej += r.pos_rej; }
        p.result("weakly_reversible", sd.S.wr ? "yes" : "no");
        p.result("accepted_steps", (double)acc);
        p.result("rejected_steps", (double)rej);
        p.result("positivity_rejections", (double)prej);
        p.result("final_min", sd.min_now);
        p.result("final_max", sd.max_now);
        p.result("final_extinct", sd.extinct);
        p.result("runs_runaway", sd.runaway);
        p.result("empirical_envelope_lo_t_ge_40", sd.env_lo);
        p.result("empirical_envelope_hi_t_ge_40", sd.env_hi);
        p.result("conservation_residual_final", sd.resid);
        for (int ri = 0; ri < 6; ++ri)
            for (int i = 0; i < D; ++i) p.result(fmt("final_run%d_%s", ri + 1, SPS[i]), sd.runs[ri].x[i]);
    }
    return h.finish();
}
