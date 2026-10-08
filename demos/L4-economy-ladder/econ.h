// econ.h - the 01-economy network, structural check and Rosenbrock 2(3) integrator, copied from
// demos/01-economy/main.cpp (that file has its own main(), so it cannot be #included) and
// adapted to many independent towns with per-town rates. The math is unchanged:
//   xdot = sum_{y->y'} k_{y->y'} x^y (y' - y)          (family 149, §1 eq:mass-action)
// integrated by the Shampine-Reichelt Rosenbrock 2(3) (MATLAB ode23s) with error control,
// positivity rejection + step halving, rtol 1e-6, atol 1e-14, run halted above 1e12.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <string>
#include <vector>

namespace econ {

constexpr int D = 5;
using Vec = std::array<double, D>;
using Cx = std::array<int, D>;
static const char* SPN[D] = {"Ore", "Wood", "Tools", "Food", "Workers"};
static const char* SPS[D] = {"O", "W", "T", "F", "P"};

struct Rx { Cx y, yp; double k; int lc; };
static Cx cx(int O, int W, int T, int F, int P) { return {O, W, T, F, P}; }

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
static const int REMOVED[2] = {1, 6};  // WITHOUT drops 2P->P and P+T+F->P+O+W (as in 01-economy)

// ---------------------------------------------------------------- structure (computed, as 01-economy)
struct Structure {
    std::vector<Cx> cplx;
    std::vector<int> src, dst, scc, lcls;
    int n_scc = 0, n_lc = 0, dimS = 0, deficiency = 0, no_return = 0;
    bool wr = false;
    std::vector<Vec> Zperp;
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
    if (nullsp) {
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

// Weak reversibility = every reaction's target can reach its source in the complex graph
// (every SCC is a union of whole linkage classes). Floyd-Warshall closure; n is tiny.
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
    for (int e = 0; e < m; ++e)
        if (!reach[s.dst[e]][s.src[e]]) ++s.no_return;
    s.wr = s.no_return == 0;
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
    return s;
}

// ---------------------------------------------------------------- mass action (per-town rates)
struct Net {
    int m = 0;
    Cx y[9], dv[9];
};
static Net make_net(const std::vector<Rx>& R) {
    Net n;
    n.m = (int)R.size();
    for (int r = 0; r < n.m; ++r)
        for (int i = 0; i < D; ++i) { n.y[r][i] = R[r].y[i]; n.dv[r][i] = R[r].yp[i] - R[r].y[i]; }
    return n;
}
static inline double mono(const Cx& y, const Vec& x) {
    double m = 1;
    for (int i = 0; i < D; ++i)
        for (int e = 0; e < y[i]; ++e) m *= x[i];
    return m;
}
static inline Vec rhs(const Net& N, const double* k, const Vec& x) {
    Vec out{};
    for (int r = 0; r < N.m; ++r) {
        double m = k[r] * mono(N.y[r], x);
        for (int i = 0; i < D; ++i) out[i] += m * N.dv[r][i];
    }
    return out;
}
static inline void jac(const Net& N, const double* k, const Vec& x, double J[D][D]) {
    for (int i = 0; i < D; ++i)
        for (int j = 0; j < D; ++j) J[i][j] = 0;
    for (int r = 0; r < N.m; ++r)
        for (int j = 0; j < D; ++j) {
            if (!N.y[r][j]) continue;
            double m = k[r] * N.y[r][j];
            for (int i = 0; i < D; ++i) {
                int e = N.y[r][i] - (i == j ? 1 : 0);
                for (int q = 0; q < e; ++q) m *= x[i];
            }
            for (int i = 0; i < D; ++i) J[i][j] += m * N.dv[r][i];
        }
}

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

constexpr double RTOL = 1e-6, ATOL = 1e-14;
inline double HALT = 1e12;  // as 01-economy (sweep can raise it for the "never halt" variant)
constexpr double DEAD = 1e-6, RUNAWAY = 1e6;               // "dead" / "runaway" resource thresholds

struct Town {
    Vec x, x0;
    double k[9];
    double t = 0, h = 1e-3;
    bool halted = false;   // some resource passed HALT = 1e12: the run is stopped (as in 01-economy)
    long long acc = 0, rej = 0;
};

// Shampine-Reichelt Rosenbrock 2(3) (MATLAB ode23s), autonomous. Adds attempted steps
// (accepted + rejected) to `steps`. Identical code for both sides.
static void advance(const Net& N, Town& r, double t1, long long& steps) {
    static const double d = 1.0 / (2.0 + std::sqrt(2.0)), e32 = 6.0 + std::sqrt(2.0);
    int guard = 0;
    while (r.t < t1 && !r.halted && guard++ < 200000) {
        double h = std::min(r.h, t1 - r.t);
        double J[D][D];
        jac(N, r.k, r.x, J);
        LU W;
        for (int i = 0; i < D; ++i)
            for (int j = 0; j < D; ++j) W.a[i][j] = (i == j ? 1.0 : 0.0) - h * d * J[i][j];
        ++steps;
        if (!W.factor()) { r.h *= 0.5; ++r.rej; continue; }
        Vec F0 = rhs(N, r.k, r.x);
        Vec k1 = W.solve(F0), y1, k2, yn, k3, b;
        for (int i = 0; i < D; ++i) y1[i] = r.x[i] + 0.5 * h * k1[i];
        bool pos = true;
        for (int i = 0; i < D; ++i) pos &= y1[i] > 0;
        Vec F1 = rhs(N, r.k, y1);
        for (int i = 0; i < D; ++i) b[i] = F1[i] - k1[i];
        k2 = W.solve(b);
        for (int i = 0; i < D; ++i) { k2[i] += k1[i]; yn[i] = r.x[i] + h * k2[i]; pos &= yn[i] > 0; }
        if (!pos) { r.h = h * 0.5; ++r.rej; continue; }
        Vec F2 = rhs(N, r.k, yn);
        for (int i = 0; i < D; ++i) b[i] = F2[i] - e32 * (k2[i] - F1[i]) - 2.0 * (k1[i] - F0[i]);
        k3 = W.solve(b);
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
                if (r.x[i] > HALT) r.halted = true;
        } else {
            ++r.rej;
            r.h = h * std::max(0.1, 0.8 * std::pow(en, -1.0 / 3.0));
        }
    }
}

// What a game would ship instead: fixed-step explicit midpoint (RK2), `sub` substeps per frame.
// No error control, no positivity fix-up. Flags the first negative / non-finite state.
struct FixedTown {
    Vec x;
    bool broken = false;   // went negative or non-finite at some point
    double t_broken = -1;
};
static void advance_fixed(const Net& N, const double* k, FixedTown& f, double t, double dt, int sub) {
    if (f.broken) return;
    const double h = dt / sub;
    for (int s = 0; s < sub; ++s) {
        Vec a = rhs(N, k, f.x), mid;
        for (int i = 0; i < D; ++i) mid[i] = f.x[i] + 0.5 * h * a[i];
        Vec b = rhs(N, k, mid);
        bool ok = true;
        for (int i = 0; i < D; ++i) { f.x[i] += h * b[i]; ok &= std::isfinite(f.x[i]) && f.x[i] >= 0; }
        if (!ok) { f.broken = true; f.t_broken = t + (s + 1) * h; return; }
    }
}

// ---------------------------------------------------------------- seeded towns, one class
// Every town starts in the stoichiometric class O + W + 2T = 4.5 (the conservation law of both
// networks); F and P are free directions. Starts are log-uniform; rates are the 01-economy rates
// times a seeded log-uniform factor in [1/jit, jit] (jit = 1 -> exact 01-economy rates).
// Town i's start and rates depend only on (seed, i), so stage n is a prefix of stage 2n.
static void seed_town(Town& T, unsigned seed, int i, const std::vector<Rx>& FULL, double jit) {
    std::mt19937_64 g(seed * 0x9E3779B97F4A7C15ull + (unsigned long long)i * 0xBF58476D1CE4E5B9ull + 1);
    std::uniform_real_distribution<double> U(0.0, 1.0);
    auto lu = [&](double lo, double hi) { return lo * std::pow(hi / lo, U(g)); };
    double a = lu(0.01, 1), b = lu(0.01, 1), c = lu(0.01, 1);
    double s = 4.5 / (a + b + 2 * c);
    T.x[0] = a * s; T.x[1] = b * s; T.x[2] = c * s;
    T.x[3] = lu(1e-3, 30); T.x[4] = lu(1e-3, 30);
    T.x0 = T.x;
    for (size_t r = 0; r < FULL.size(); ++r) T.k[r] = FULL[r].k * (jit > 1 ? lu(1 / jit, jit) : 1.0);
    T.t = 0; T.h = 1e-3; T.halted = false; T.acc = T.rej = 0;
}
// WITHOUT town: same start, the same rates for the 7 kept reactions.
static void drop_removed(Town& T) {
    double k[9];
    int j = 0;
    for (int r = 0; r < 9; ++r)
        if (r != REMOVED[0] && r != REMOVED[1]) k[j++] = T.k[r];
    for (int r = 0; r < 9; ++r) T.k[r] = r < j ? k[r] : 0;
}

}  // namespace econ
