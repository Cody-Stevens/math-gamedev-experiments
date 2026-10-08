// 06-dungeon-shuffle - rewire a dungeon while keeping every room's door count.
//
// Family 131, "Polynomial mixing of the switch chain for every graphical degree sequence"
// (preprints/Polynomial-Mixing-of-the-Switch-Chain-for-Every-Graphical-Degree-Sequence-
//  September-25-2026/build/sections/{introduction,setup,mixing}.tex).
//
// A designer template dungeon (20 labeled rooms on a grid, 28 doors, door counts 1..6) is
// shuffled into a fresh layout with exactly the same door count per room.
//   LEFT  (WITHOUT): the conventional lazy double-edge swap (pick two doors, re-pair their
//                    endpoints if the result stays simple), ad hoc burn-in of one swap per door
//                    (28 swaps). Its stationary law is also uniform, but this budget comes with
//                    no guarantee.
//   RIGHT (WITH):    the undirected Curveball pair trade of §2 (setup.tex: a uniform pair
//                    {i,j}; the singleton neighbours are re-dealt by a uniform q-subset, here an
//                    unbiased partial Fisher-Yates shuffle), run for the burn-in certified by §7:
//                    mix:H-poincare  Var(f) <= <f,Hf>, H = sum_a (I - E_a)
//                    => K = (1/B) sum_a E_a = I - H/B is PSD with spectral gap >= 1/B, B = C(n,2)
//                    => TV(t) <= 1/2 sqrt(|Omega|) (1-1/B)^t, and |Omega| <= 2^B gives TV <= 1/4 at
//                       T = ceil(B (1 + B/2) ln 2)  (n = 20: B = 190, T = 12,643 trades).
//                    (This O(n^4) corollary is derived from the checked proof; the paper's headline
//                    theorem is the 2 n^8 single-switch bound.)
// Exact check: for n=6, d=(2,...,2) (70 graphs) and an irregular n=8 sequence, all graphs are
// enumerated and the full law of both chains is propagated exactly -> exact TV to uniform.
// The certificate buys provable fairness (uniform over all labeled realizations, including
// disconnected ones), not speed: the WITH layout costs far more steps.
#include "demo.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

using namespace demo;
using u64 = uint64_t;
constexpr int NMAX = 32;

// ---------------------------------------------------------------- RNG (deterministic)
struct Rng {
    u64 s;
    explicit Rng(u64 seed) : s(seed * 0x9E3779B97F4A7C15ull + 0x1234567ull) {}
    u64 next() {  // splitmix64
        u64 z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    uint32_t below(uint32_t n) {  // unbiased (Lemire)
        uint64_t m = uint64_t(uint32_t(next())) * n;
        uint32_t l = uint32_t(m);
        if (l < n) {
            uint32_t t = uint32_t(-n) % n;
            while (l < t) { m = uint64_t(uint32_t(next())) * n; l = uint32_t(m); }
        }
        return uint32_t(m >> 32);
    }
};

struct Graph {
    int n = 0;
    std::array<u64, NMAX> adj{};
    bool has(int u, int v) const { return (adj[u] >> v) & 1; }
    void toggle(int u, int v) { adj[u] ^= 1ull << v; adj[v] ^= 1ull << u; }
    int edges() const { int s = 0; for (int i = 0; i < n; ++i) s += std::popcount(adj[i]); return s / 2; }
};

// ---------------------------------------------------------------- WITH: Curveball pair trade
// Returns the number of edges toggled (0 when the trade re-deals the same subset).
static int curveball_step(Graph& g, Rng& r) {
    const int n = g.n;
    int i = (int)r.below(n), j = (int)r.below(n - 1);
    if (j >= i) ++j;  // uniform ordered pair -> uniform unordered pair (orientation is irrelevant)
    const u64 bi = 1ull << i, bj = 1ull << j;
    const u64 A = g.adj[i] & ~bj, Bm = g.adj[j] & ~bi;
    const u64 S = A ^ Bm;  // singleton vertices: adjacent to exactly one of i, j
    if (!S) return 0;
    const u64 Ai = A & S;
    const int q = std::popcount(Ai), N = std::popcount(S);
    if (q == 0 || q == N) return 0;
    int list[NMAX], c = 0;
    for (u64 s = S; s; s &= s - 1) list[c++] = std::countr_zero(s);
    u64 newAi = 0;
    for (int k = 0; k < q; ++k) {  // partial Fisher-Yates: a uniform q-subset of the N singletons
        int t = k + (int)r.below(N - k);
        std::swap(list[k], list[t]);
        newAi |= 1ull << list[k];
    }
    const u64 flip = newAi ^ Ai;  // singletons whose endpoint changes
    if (!flip) return 0;
    g.adj[i] ^= flip;
    g.adj[j] ^= flip;
    for (u64 s = flip; s; s &= s - 1) g.adj[std::countr_zero(s)] ^= bi | bj;
    return 2 * std::popcount(flip);
}

// ---------------------------------------------------------------- WITHOUT: double-edge swap
struct SwapGraph {
    Graph g;
    std::vector<std::array<uint8_t, 2>> e;
    void build() {
        e.clear();
        for (int u = 0; u < g.n; ++u)
            for (int v = u + 1; v < g.n; ++v)
                if (g.has(u, v)) e.push_back({(uint8_t)u, (uint8_t)v});
    }
};
static int swap_step(SwapGraph& s, Rng& r) {
    const int m = (int)s.e.size();
    int a = (int)r.below(m), b = (int)r.below(m - 1);
    if (b >= a) ++b;
    int u = s.e[a][0], v = s.e[a][1], x = s.e[b][0], y = s.e[b][1];
    if (r.next() & 1) std::swap(x, y);  // one of the two re-pairings, uniformly
    if (u == x || u == y || v == x || v == y) return 0;
    if (s.g.has(u, x) || s.g.has(v, y)) return 0;  // would create a multi-edge: reject (lazy)
    s.g.toggle(u, v); s.g.toggle(x, y); s.g.toggle(u, x); s.g.toggle(v, y);
    s.e[a] = {(uint8_t)u, (uint8_t)x};
    s.e[b] = {(uint8_t)v, (uint8_t)y};
    return 4;
}

// ---------------------------------------------------------------- exact law propagation
struct Exact {
    int n = 0, B = 0, m = 0;
    std::vector<int> deg;
    std::vector<u64> states;  // edge bitmask over pair index
    std::unordered_map<u64, int> id;
    std::vector<int> pi_, pj_;
    struct CSR { std::vector<int> start, col; std::vector<double> p; };
    CSR K_cb, K_sw;
    std::vector<double> tv_cb, tv_sw;  // TV vs step from start state 0
    double worst_cb_at = 0, worst_sw_at = 0;

    Graph to_graph(u64 mask) const {
        Graph g; g.n = n;
        for (int k = 0; k < B; ++k)
            if ((mask >> k) & 1) g.toggle(pi_[k], pj_[k]);
        return g;
    }
    u64 to_mask(const Graph& g) const {
        u64 mask = 0;
        for (int k = 0; k < B; ++k)
            if (g.has(pi_[k], pj_[k])) mask |= 1ull << k;
        return mask;
    }
    void enumerate(int k, u64 mask, std::vector<int>& res) {
        if (k == B) {
            for (int v = 0; v < n; ++v) if (res[v]) return;
            states.push_back(mask);
            return;
        }
        int i = pi_[k], j = pj_[k];
        // prune: vertex i's last pair is (i, n-1)
        if (res[i] && res[j]) { --res[i]; --res[j]; enumerate(k + 1, mask | (1ull << k), res); ++res[i]; ++res[j]; }
        bool last_i = j == n - 1;
        if (!(last_i && res[i])) enumerate(k + 1, mask, res);
    }
    void init(std::vector<int> d, const Graph& start) {
        n = (int)d.size(); deg = d; B = n * (n - 1) / 2;
        for (int i = 0; i < n; ++i)
            for (int j = i + 1; j < n; ++j) { pi_.push_back(i); pj_.push_back(j); }
        std::vector<int> res = d;
        enumerate(0, 0, res);
        std::sort(states.begin(), states.end());
        u64 s0 = to_mask(start);
        auto it = std::find(states.begin(), states.end(), s0);
        std::swap(*it, states[0]);  // start = state 0
        for (int k = 0; k < (int)states.size(); ++k) id[states[k]] = k;
        m = 0; for (int v : d) m += v; m /= 2;
        build();
    }
    void build() {
        const int S = (int)states.size();
        std::vector<double> row(S, 0.0);
        std::vector<int> touched;
        auto flush = [&](CSR& K) {
            for (int t : touched) if (row[t] != 0) { K.col.push_back(t); K.p.push_back(row[t]); row[t] = 0; }
            touched.clear();
            K.start.push_back((int)K.col.size());
        };
        auto add = [&](int t, double p) { if (row[t] == 0) touched.push_back(t); row[t] += p; };
        K_cb.start = {0}; K_sw.start = {0};
        for (int s = 0; s < S; ++s) {
            Graph g = to_graph(states[s]);
            // Curveball kernel K = (1/B) sum_a E_a
            for (int a = 0; a < B; ++a) {
                int i = pi_[a], j = pj_[a];
                u64 bi = 1ull << i, bj = 1ull << j, A = g.adj[i] & ~bj, Bm = g.adj[j] & ~bi, Sx = A ^ Bm, Ai = A & Sx;
                int q = std::popcount(Ai), N = std::popcount(Sx);
                int list[NMAX], c = 0;
                for (u64 x = Sx; x; x &= x - 1) list[c++] = std::countr_zero(x);
                std::vector<u64> subs;
                for (u64 sub = 0; sub < (1ull << N); ++sub)
                    if (std::popcount(sub) == q) {
                        u64 na = 0;
                        for (int b = 0; b < N; ++b) if ((sub >> b) & 1) na |= 1ull << list[b];
                        subs.push_back(na);
                    }
                for (u64 na : subs) {
                    Graph h = g;
                    u64 flip = na ^ Ai;
                    h.adj[i] ^= flip; h.adj[j] ^= flip;
                    for (u64 x = flip; x; x &= x - 1) h.adj[std::countr_zero(x)] ^= bi | bj;
                    add(id.at(to_mask(h)), 1.0 / B / subs.size());
                }
            }
            flush(K_cb);
            // double-edge swap kernel: unordered edge pair (C(m,2)) x 2 re-pairings, lazy rejection
            std::vector<std::array<int, 2>> E;
            for (int u = 0; u < n; ++u) for (int v = u + 1; v < n; ++v) if (g.has(u, v)) E.push_back({u, v});
            const double pp = 1.0 / (2.0 * (m * (m - 1) / 2));
            for (int a = 0; a < m; ++a)
                for (int b = a + 1; b < m; ++b)
                    for (int o = 0; o < 2; ++o) {
                        int u = E[a][0], v = E[a][1], x = E[b][0], y = E[b][1];
                        if (o) std::swap(x, y);
                        if (u == x || u == y || v == x || v == y || g.has(u, x) || g.has(v, y)) { add(s, pp); continue; }
                        Graph h = g;
                        h.toggle(u, v); h.toggle(x, y); h.toggle(u, x); h.toggle(v, y);
                        add(id.at(to_mask(h)), pp);
                    }
            flush(K_sw);
        }
    }
    static void step(const CSR& K, const std::vector<double>& p, std::vector<double>& q) {
        std::fill(q.begin(), q.end(), 0.0);
        for (size_t s = 0; s + 1 < K.start.size(); ++s) {
            if (p[s] == 0) continue;
            for (int k = K.start[s]; k < K.start[s + 1]; ++k) q[K.col[k]] += p[s] * K.p[k];
        }
    }
    double tv(const std::vector<double>& p) const {
        double u = 1.0 / p.size(), t = 0;
        for (double x : p) t += std::fabs(x - u);
        return 0.5 * t;
    }
    std::vector<double> curve(const CSR& K, int start, int T) const {
        std::vector<double> p(states.size(), 0.0), q(states.size());
        p[start] = 1;
        std::vector<double> out{tv(p)};
        for (int t = 0; t < T; ++t) { step(K, p, q); p.swap(q); out.push_back(tv(p)); }
        return out;
    }
    double bound(int t) const { return 0.5 * std::sqrt(double(states.size()) - 1.0) * std::pow(1.0 - 1.0 / B, t); }
};

static long long certified_T(int n) {  // ceil(B (1 + B/2) ln 2), B = C(n,2)   (from §7 mix:H-poincare)
    double B = n * (n - 1) / 2.0;
    return (long long)std::ceil(B * (1.0 + B / 2.0) * std::log(2.0));
}

// ---------------------------------------------------------------- the dungeon template
struct Dungeon {
    int n = 20, cols = 5, rows = 4;
    Graph g;
    std::vector<Vec2> pos;  // grid coordinates
};
static Dungeon make_template() {
    Dungeon D;
    D.g.n = D.n;
    auto id = [&](int r, int c) { return r * D.cols + c; };
    for (int r = 0; r < D.rows; ++r)
        for (int c = 0; c < D.cols; ++c) D.pos.push_back({(float)c, (float)r});
    for (int r = 0; r < D.rows; ++r)
        for (int c = 0; c + 1 < D.cols; ++c) D.g.toggle(id(r, c), id(r, c + 1));
    for (int r = 0; r + 1 < D.rows; ++r)
        for (int c = 0; c < D.cols; ++c) D.g.toggle(id(r, c), id(r + 1, c));
    // remove some corridors (dead ends, a cut wing), add three secret passages to a hub
    const int rm[][4] = {{0, 1, 0, 2}, {2, 3, 2, 4}, {1, 0, 2, 0}, {2, 2, 3, 2}, {1, 3, 2, 3}, {3, 0, 3, 1}};
    for (auto& e : rm) D.g.toggle(id(e[0], e[1]), id(e[2], e[3]));
    D.g.toggle(id(1, 2), id(3, 4));
    D.g.toggle(id(1, 2), id(0, 0));
    D.g.toggle(id(2, 1), id(0, 4));
    return D;
}

static int components(const Graph& g, std::vector<int>& comp) {
    comp.assign(g.n, -1);
    int k = 0;
    for (int s = 0; s < g.n; ++s) {
        if (comp[s] >= 0) continue;
        std::vector<int> st{s};
        comp[s] = k;
        while (!st.empty()) {
            int u = st.back(); st.pop_back();
            for (u64 x = g.adj[u]; x; x &= x - 1) { int v = std::countr_zero(x); if (comp[v] < 0) { comp[v] = k; st.push_back(v); } }
        }
        ++k;
    }
    return k;
}
static int overlap(const Graph& a, const Graph& b) {
    int s = 0;
    for (int i = 0; i < a.n; ++i) s += std::popcount(a.adj[i] & b.adj[i]);
    return s / 2;
}

// ---------------------------------------------------------------- drawing helpers
static Color deg_color(int d) {
    static const Color c[7] = {pal::dim, rgb(0xffc861), rgb(0x6cb6ff), rgb(0x56d4dd), rgb(0xb392f0), rgb(0xffa657), rgb(0xf2777a)};
    return c[std::clamp(d, 0, 6)];
}

struct TVPlot {
    std::string title;
    const Exact* ex;
    long long budget_l, budget_r, cert_omega;
};

static void draw_tv_plot(Panel& p, float x, float y, float w, float h, const TVPlot& tp, int side, int Tmax) {
    const Exact& E = *tp.ex;
    p.fill_rounded_rect(x, y, w, h, 8, rgb(0x05070b), 0.85f);
    const float gx = x + 46, gy = y + 24, gw = w - 58, gh = h - 46;
    const float lo = -16, hi = 0.7f;
    auto X = [&](double t) { return gx + gw * float(std::log10(1.0 + t) / std::log10(1.0 + Tmax)); };
    auto Y = [&](double v) { return gy + gh * (hi - std::clamp((float)std::log10(std::max(v, 1e-12)), lo, hi)) / (hi - lo); };
    p.text(x + 10, y + 4, tp.title, 13, pal::text, Font::Sans);
    for (int e = -16; e <= 0; e += 4) {
        p.line(gx, Y(std::pow(10, e)), gx + gw, Y(std::pow(10, e)), 1, pal::grid, 0.8f);
        p.text(gx - 4, Y(std::pow(10, e)) - 7, fmt("1e%d", e), 10, pal::dim, Font::Mono, Align::Right);
    }
    p.line(gx, Y(0.25), gx + gw, Y(0.25), 1, pal::warn, 0.35f);
    p.text(gx + 2, Y(0.25) - 13, "¼", 11, pal::warn, Font::Sans);
    for (double t : {1.0, 10.0, 100.0, 1000.0})
        if (t <= Tmax) p.text(X(t), gy + gh + 2, fmt("%g", t), 10, pal::dim, Font::Mono, Align::Center);
    // certified bound 1/2 sqrt(|Omega|-1) (1-1/B)^t for the Curveball kernel
    {
        std::vector<Vec2> pts;
        for (int t = 0; t <= Tmax; t += std::max(1, Tmax / 200)) pts.push_back({X(t), Y(E.bound(t))});
        for (size_t k = 0; k + 1 < pts.size(); k += 2) p.line(pts[k].x, pts[k].y, pts[k + 1].x, pts[k + 1].y, 1.2f, pal::text, 0.45f);
    }
    auto curve = [&](const std::vector<double>& c, Color col, float wd, float al) {
        std::vector<Vec2> pts;
        for (int t = 0; t < (int)c.size() && t <= Tmax; ++t) pts.push_back({X(t), Y(c[t])});
        p.polyline(pts.data(), (int)pts.size(), wd, col, al);
    };
    curve(E.tv_sw, pal::without, side == 0 ? 2.6f : 1.4f, side == 0 ? 1.f : 0.55f);
    curve(E.tv_cb, pal::with, side == 1 ? 2.6f : 1.4f, side == 1 ? 1.f : 0.55f);
    // budget markers
    auto mark = [&](long long t, Color c, const char* lab, bool own, const std::vector<double>& cv) {
        float xx = X((double)t);
        p.line(xx, gy, xx, gy + gh, own ? 1.6f : 1.f, c, own ? 0.9f : 0.4f);
        p.text(xx + 3, gy + 1, lab, 10, c, Font::Mono);
        if (own && t < (long long)cv.size()) {
            p.glow(xx, Y(cv[t]), 10, c, 0.8f);
            p.circle(xx, Y(cv[t]), 4, c);
        }
    };
    mark(tp.budget_l, pal::without, fmt("%lld", tp.budget_l).c_str(), side == 0, E.tv_sw);
    mark(tp.budget_r, pal::with, fmt("%lld", tp.budget_r).c_str(), side == 1, E.tv_cb);
}

// ================================================================== main
int main(int argc, char** argv) {
    Config cfg;
    cfg.name = "06-dungeon-shuffle";
    cfg.title_left = "double-edge swaps, ad hoc burn-in (1 per door)";
    cfg.title_right = "Curveball trades, certified burn-in";
    cfg.caption = "Family 131 — Polynomial mixing of the switch chain for every graphical degree sequence · WITH: §2 pair trade + "
                  "§7 mix:H-poincare ⇒ gap ≥ 1/B ⇒ T = ceil[B(1+B/2)·ln 2] · certifies fair (uniform) layouts, not speed";
    cfg.frames = 1560;
    Harness h(argc, argv, cfg);

    // ---------------- dungeon + budgets
    const Dungeon D = make_template();
    const Graph G0 = D.g;
    const int n = D.n, m = G0.edges(), B = n * (n - 1) / 2;
    const long long T_cert = certified_T(n);
    const long long T_adhoc = m;  // "one swap per door"
    std::vector<int> degs(n);
    for (int i = 0; i < n; ++i) degs[i] = std::popcount(G0.adj[i]);

    // ---------------- exact toys
    Exact toyA, toyB;
    {
        Graph s; s.n = 6;  // two triangles: the designer start of the regular toy
        s.toggle(0, 1); s.toggle(1, 2); s.toggle(2, 0); s.toggle(3, 4); s.toggle(4, 5); s.toggle(5, 3);
        toyA.init({2, 2, 2, 2, 2, 2}, s);
    }
    {
        // irregular n=8 sequence; start = Havel-Hakimi realization (highest residual first)
        std::vector<int> d = {5, 4, 3, 3, 2, 2, 2, 1};
        Graph s; s.n = 8;
        std::vector<int> r = d;
        for (int step = 0; step < 8; ++step) {
            int v = (int)(std::max_element(r.begin(), r.end()) - r.begin());
            int need = r[v];
            r[v] = 0;
            std::vector<int> cand;
            for (int u = 0; u < 8; ++u) if (u != v && r[u] > 0 && !s.has(u, v)) cand.push_back(u);
            std::stable_sort(cand.begin(), cand.end(), [&](int a, int b) { return r[a] > r[b]; });
            for (int k = 0; k < need; ++k) { s.toggle(v, cand[k]); --r[cand[k]]; }
        }
        toyB.init(d, s);
    }
    const long long TcA = certified_T(6), TcB = certified_T(8);
    const long long TcA_omega = (long long)std::ceil(15.0 * (std::log(2.0) + 0.5 * std::log((double)toyA.states.size())));
    const long long TcB_omega = (long long)std::ceil(28.0 * (std::log(2.0) + 0.5 * std::log((double)toyB.states.size())));
    const int TmaxA = 200, TmaxB = 700;
    toyA.tv_cb = toyA.curve(toyA.K_cb, 0, TmaxA);
    toyA.tv_sw = toyA.curve(toyA.K_sw, 0, TmaxA);
    toyB.tv_cb = toyB.curve(toyB.K_cb, 0, TmaxB);
    toyB.tv_sw = toyB.curve(toyB.K_sw, 0, TmaxB);
    // worst case over all 70 starts for the regular toy, at each side's budget
    double worstA_cb = 0, worstA_sw = 0;
    for (int s = 0; s < (int)toyA.states.size(); ++s) {
        worstA_cb = std::max(worstA_cb, toyA.curve(toyA.K_cb, s, (int)TcA).back());
        worstA_sw = std::max(worstA_sw, toyA.curve(toyA.K_sw, s, toyA.m).back());
    }
    bool bound_ok = true;
    for (int t = 0; t <= TmaxA; ++t) bound_ok &= toyA.tv_cb[t] <= toyA.bound(t) + 1e-12;
    for (int t = 0; t <= TmaxB; ++t) bound_ok &= toyB.tv_cb[t] <= toyB.bound(t) + 1e-12;
    const double tvA[2] = {toyA.tv_sw[toyA.m], toyA.tv_cb[TcA]};
    const double tvB[2] = {toyB.tv_sw[toyB.m], toyB.tv_cb[TcB]};

    // ---------------- stationary reference for "template doors kept" (empirical, long runs)
    double ref_overlap = 0, ref_overlap_cb = 0, ref_conn = 0;
    std::vector<double> ref_hist(m + 1, 0.0);
    {
        SwapGraph s; s.g = G0; s.build();
        Rng r(777);
        for (int k = 0; k < 200000; ++k) swap_step(s, r);
        long long cnt = 0; double sum = 0, conn = 0;
        std::vector<int> comp;
        for (int k = 0; k < 40000; ++k) {
            for (int q = 0; q < 100; ++q) swap_step(s, r);
            int ov = overlap(s.g, G0);
            sum += ov; conn += components(s.g, comp) == 1; ++cnt;
            ref_hist[ov] += 1;
        }
        ref_overlap = sum / cnt; ref_conn = conn / cnt;
        for (auto& v : ref_hist) v /= cnt;
        Graph g = G0; Rng r2(778);
        for (int k = 0; k < 200000; ++k) curveball_step(g, r2);
        sum = 0; cnt = 0;
        for (int k = 0; k < 40000; ++k) { for (int q = 0; q < 50; ++q) curveball_step(g, r2); sum += overlap(g, G0); ++cnt; }
        ref_overlap_cb = sum / cnt;
    }
    if (!h.quiet())
        printf("n=%d m=%d B=%d T_cert=%lld |OmegaA|=%zu |OmegaB|=%zu TV A sw@%d=%.3g cb@%lld=%.3g  B sw@%d=%.3g cb@%lld=%.3g ref %.3f/%.3f conn %.3f\n", n, m,
               B, T_cert, toyA.states.size(), toyB.states.size(), toyA.m, tvA[0], TcA, tvA[1], toyB.m, tvB[0], TcB, tvB[1], ref_overlap,
               ref_overlap_cb, ref_conn);

    // ---------------- per-side state
    struct Side {
        long long steps = 0, steps_total = 0, changed_total = 0, layouts = 0, connected = 0;
        double ms_total = 0, overlap_sum = 0, autocorr_sum = 0;
        std::vector<Graph> snaps;  // replay of the displayed level
        std::vector<double> hist;  // template doors kept, over all generated layouts
        Graph shown, prev;
        std::vector<int> age;      // per pair: frames since added (for flashes)
        struct Ghost { int u, v; float a; };
        std::vector<Ghost> ghosts;
    } S[2];
    for (auto& s : S) { s.shown = G0; s.prev = G0; s.age.assign(n * n, 1000); }

    const int LEVEL = 312, T_SHOW0 = 40, T_SHUF = 200;
    auto run_layout = [&](int side, u64 seed, std::vector<Graph>* snaps, long long& changed) -> Graph {
        Rng r(seed);
        changed = 0;
        if (side == 0) {
            SwapGraph s; s.g = G0; s.build();
            for (long long t = 0; t < T_adhoc; ++t) {
                changed += swap_step(s, r);
                if (snaps) snaps->push_back(s.g);
            }
            return s.g;
        }
        Graph g = G0;
        const long long every = std::max<long long>(1, T_cert / T_SHUF);
        for (long long t = 0; t < T_cert; ++t) {
            changed += curveball_step(g, r);
            if (snaps && (t + 1) % every == 0) snaps->push_back(g);
        }
        return g;
    };

    // grid -> panel coordinates for the dungeon
    auto room_xy = [&](int v) -> Vec2 { return {120.f + D.pos[v].x * 180.f, 84.f + D.pos[v].y * 112.f}; };

    h.left().set_compute_label("1 layout");
    h.right().set_compute_label("1 layout");
    h.left().set_hud_corner(Corner::BottomLeft);
    h.right().set_hud_corner(Corner::BottomLeft);
    std::vector<int> comp;

    while (h.next_frame()) {
        const int fr = h.frame(), lvl = fr / LEVEL, lf = fr % LEVEL;
        for (int k = 0; k < 2; ++k) {
            Side& sd = S[k];
            Panel& p = h.panel(k);
            // ---- timed: generate one complete fresh layout from the template
            Graph out;
            long long changed = 0;
            const u64 seed = 1000003ull * (fr + 1) + 17 * k;
            double t0 = 0, t1 = 0;
            p.measure([&] {
                t0 = now_ms();
                out = run_layout(k, seed, nullptr, changed);
                t1 = now_ms();
            });
            sd.ms_total += t1 - t0;
            sd.steps_total += k == 0 ? T_adhoc : T_cert;
            ++sd.layouts;
            const int ov = overlap(out, G0);
            sd.overlap_sum += ov;
            if (sd.hist.empty()) sd.hist.assign(m + 1, 0.0);
            sd.hist[ov] += 1;
            sd.autocorr_sum += (ov - ref_overlap) / (m - ref_overlap);
            sd.connected += components(out, comp) == 1;

            // ---- replay of the level's showcase layout (untimed)
            if (lf == 0) {
                sd.snaps.clear();
                long long c2;
                run_layout(k, 99991ull * (lvl + 1) + k, &sd.snaps, c2);
            }
            int si = -1;
            if (lf >= T_SHOW0) {
                int a = std::min(lf - T_SHOW0, T_SHUF - 1);
                si = (int)((long long)(a + 1) * (long long)sd.snaps.size() / T_SHUF) - 1;
            }
            sd.prev = sd.shown;
            sd.shown = si < 0 ? G0 : sd.snaps[si];
            for (int u = 0; u < n; ++u)
                for (int v = u + 1; v < n; ++v) {
                    bool a = sd.prev.has(u, v), b = sd.shown.has(u, v);
                    int& ag = sd.age[u * n + v];
                    if (b && !a) ag = 0; else if (b) ++ag;
                    if (a && !b) sd.ghosts.push_back({u, v, 1.f});
                }
            for (auto& g : sd.ghosts) g.a -= k == 0 ? 0.03f : 0.12f;
            sd.ghosts.erase(std::remove_if(sd.ghosts.begin(), sd.ghosts.end(), [](auto& g) { return g.a <= 0; }), sd.ghosts.end());

            // ================================================ drawing
            p.clear(pal::bg);
            const Color acc = k == 0 ? pal::without : pal::with;
            // backdrop grid of the dungeon
            for (int v = 0; v < n; ++v) {
                Vec2 q = room_xy(v);
                p.fill_rounded_rect(q.x - 70, q.y - 42, 140, 84, 14, rgb(0x0e1118), 1);
            }
            auto edge_pts = [&](int u, int v, Vec2* pts, int K) {
                Vec2 A = room_xy(u), Bp = room_xy(v);
                float dx = Bp.x - A.x, dy = Bp.y - A.y, L = std::sqrt(dx * dx + dy * dy);
                bool adjg = std::fabs(D.pos[u].x - D.pos[v].x) + std::fabs(D.pos[u].y - D.pos[v].y) == 1;
                float bend = adjg ? 0.f : 0.16f + 0.04f * ((u * 7 + v * 3) % 3);
                Vec2 c{(A.x + Bp.x) * 0.5f - dy * bend, (A.y + Bp.y) * 0.5f + dx * bend};
                (void)L;
                for (int q = 0; q <= K; ++q) {
                    float t = float(q) / K, w = 1 - t;
                    pts[q] = {w * w * A.x + 2 * w * t * c.x + t * t * Bp.x, w * w * A.y + 2 * w * t * c.y + t * t * Bp.y};
                }
            };
            Vec2 pts[25];
            for (auto& g : sd.ghosts) {
                edge_pts(g.u, g.v, pts, 24);
                p.polyline(pts, 25, k == 0 ? 2.f : 1.2f, pal::bad, (k == 0 ? 0.55f : 0.3f) * g.a);
            }
            for (int u = 0; u < n; ++u)
                for (int v = u + 1; v < n; ++v) {
                    if (!sd.shown.has(u, v)) continue;
                    edge_pts(u, v, pts, 24);
                    bool tmpl = G0.has(u, v);
                    int ag = sd.age[u * n + v];
                    float fl = std::max(0.f, 1.f - ag / 25.f);
                    Color c = tmpl ? rgb(0x7d8597) : rgb(0x6cb6ff);
                    if (!tmpl) {
                        p.blend = Blend::Add;
                        p.polyline(pts, 25, 7.f, c, 0.10f + 0.25f * fl);
                        p.blend = Blend::Normal;
                    }
                    p.polyline(pts, 25, tmpl ? 3.f : 2.6f, lerp(c, rgb(0xffffff), 0.6f * fl), tmpl ? 0.75f : 0.95f);
                    if (fl > 0) p.glow(pts[12].x, pts[12].y, 16 * fl + 4, rgb(0xffffff), (k == 0 ? 0.5f : 0.25f) * fl);
                }
            int ncomp = components(sd.shown, comp);
            std::vector<int> csize(ncomp, 0);
            for (int v = 0; v < n; ++v) ++csize[comp[v]];
            int big = (int)(std::max_element(csize.begin(), csize.end()) - csize.begin());
            for (int v = 0; v < n; ++v) {
                Vec2 q = room_xy(v);
                int dg = degs[v];
                float half = 15.f + 3.5f * dg;
                Color c = deg_color(dg);
                p.glow(q.x, q.y, half * 1.9f, c, 0.25f);
                p.fill_rounded_rect(q.x - half, q.y - half * 0.75f, 2 * half, 1.5f * half, 7, rgb(0x161b26), 1);
                bool lost = comp[v] != big;
                p.stroke_rect(q.x - half, q.y - half * 0.75f, 2 * half, 1.5f * half, lost ? 2.5f : 1.8f, lost ? pal::bad : c, 1);
                for (int d = 0; d < dg; ++d)  // door pips
                    p.circle(q.x - (dg - 1) * 5.f + d * 10.f, q.y + half * 0.75f - 7, 2.6f, c);
                p.text(q.x, q.y - 12, fmt("%d", v + 1), 15, pal::text, Font::Bold, Align::Center);
            }
            // header line for the dungeon area
            std::string phase = si < 0 ? "designer template (identical on both sides)"
                                : lf < T_SHOW0 + T_SHUF ? fmt("shuffling … step %lld / %lld", (long long)(k == 0 ? (si + 1) : (si + 1) * std::max<long long>(1, T_cert / T_SHUF)),
                                                              (long long)(k == 0 ? T_adhoc : T_cert))
                                                        : "shuffled layout";
            p.text(16, 10, fmt("Level %d · %s", lvl + 1, phase.c_str()), 17, pal::text, Font::Bold);
            int kept = overlap(sd.shown, G0);
            p.text(944, 10,
                   fmt("template doors kept %d/%d · %s", kept, m, ncomp == 1 ? "connected" : fmt("%d rooms unreachable", n - csize[big]).c_str()),
                   15, ncomp == 1 ? pal::dim : pal::warn, Font::Sans, Align::Right);
            // legend
            p.line(560, 39, 590, 39, 3, rgb(0x7d8597), 0.75f);
            p.text(596, 31, "template door", 13, pal::dim);
            p.line(700, 39, 730, 39, 2.6f, rgb(0x6cb6ff), 0.95f);
            p.text(736, 31, "new door", 13, pal::dim);
            p.text(944, 31, "pips = doors (fixed)", 13, pal::dim, Font::Sans, Align::Right);
            // histogram strip: template doors kept per generated layout vs the uniform reference
            {
                const float hx = 16, hy = 474, hw = 928, hh = 84;
                p.fill_rounded_rect(hx, hy, hw, hh, 8, rgb(0x05070b), 0.85f);
                const float px0 = hx + 300, pw = hw - 312, py0 = hy + 8, ph = hh - 26;
                double tot = 0, mx = 0;
                for (double v : sd.hist) tot += v;
                for (int b = 0; b <= m; ++b) mx = std::max({mx, sd.hist[b] / tot, ref_hist[b]});
                const float bwid = pw / (m + 1);
                for (int b = 0; b <= m; ++b) {
                    float x0 = px0 + b * bwid, v = float(sd.hist[b] / tot / mx), rv = float(ref_hist[b] / mx);
                    p.fill_rect(x0 + 2, py0 + ph * (1 - v), bwid - 4, ph * v, acc, 0.75f);
                    p.line(x0 + 1, py0 + ph * (1 - rv), x0 + bwid - 1, py0 + ph * (1 - rv), 2, pal::text, 0.85f);
                    if (b % 4 == 0) p.text(x0 + bwid * 0.5f, py0 + ph + 2, fmt("%d", b), 11, pal::dim, Font::Mono, Align::Center);
                }
                float mxs = px0 + float(sd.overlap_sum / sd.layouts + 0.5) * bwid, mxr = px0 + float(ref_overlap + 0.5) * bwid;
                p.line(mxr, py0 - 4, mxr, py0 + ph, 1.2f, pal::text, 0.6f);
                p.line(mxs, py0 - 4, mxs, py0 + ph, 2, acc, 0.95f);
                p.text(hx + 12, hy + 8, "template doors kept per layout", 14, pal::text, Font::Bold);
                p.text(hx + 12, hy + 28, fmt("bars: this side, %lld layouts · mean %.2f", sd.layouts, sd.overlap_sum / sd.layouts), 12, acc);
                p.text(hx + 12, hy + 44, fmt("white: uniform reference · mean %.2f", ref_overlap), 12, pal::text);
                p.text(hx + 12, hy + 60, "(empirical; reference = 4M-step swap chain)", 11, pal::dim);
            }

            // ---- budget / formula box and exact TV plots (bottom right)
            const float bx = 470, by = 566, bw = 476;
            p.fill_rounded_rect(bx, by, bw, 70, 8, rgb(0x05070b), 0.85f);
            p.fill_rect(bx + 10, by, bw - 20, 2, acc, 0.9f);
            if (k == 0) {
                p.text(bx + 12, by + 8, fmt("burn-in = 1 swap per door = %lld swaps (ad hoc)", T_adhoc), 15, pal::text, Font::Bold);
                p.text(bx + 12, by + 30, "stationary law is uniform too, but this budget has", 13, pal::dim);
                p.text(bx + 12, by + 47, "no guarantee for this (or any) degree sequence", 13, pal::dim);
            } else {
                p.text(bx + 12, by + 6, fmt("T = ceil[B(1+B/2)·ln 2] = %s trades, B = C(%d,2) = %d", fmt("%lld", T_cert).c_str(), n, B), 15, pal::text,
                       Font::Bold);
                p.text(bx + 12, by + 28, "§7 mix:H-poincare: Var f ≤ ⟨f,Hf⟩ ⇒ gap(K) ≥ 1/B ⇒", 13, pal::dim);
                p.text(bx + 12, by + 45, "TV ≤ ½√|Ω|·e^(−t/B) ≤ ¼ for every start, |Ω| ≤ 2^B", 13, pal::dim);
            }
            TVPlot ta{fmt("exact TV to uniform · n=6, d=(2,…,2), |Ω|=%zu · start: 2 triangles", toyA.states.size()), &toyA,
                      toyA.m, TcA, TcA_omega};
            TVPlot tb{fmt("exact TV · n=8, d=(5,4,3,3,2,2,2,1), |Ω|=%zu · start: Havel–Hakimi", toyB.states.size()), &toyB,
                      toyB.m, TcB, TcB_omega};
            draw_tv_plot(p, bx, by + 76, bw, 148, ta, k, TmaxA);
            draw_tv_plot(p, bx, by + 230, bw, 148, tb, k, TmaxB);
            for (int q = 0; q < 2; ++q) {  // legend inside each plot (bottom-left)
                float ly = by + 76 + q * 154 + 92;
                p.line(bx + 54, ly + 7, bx + 72, ly + 7, 2.4f, pal::without);
                p.text(bx + 76, ly, "double-edge swap", 11, pal::without);
                p.line(bx + 54, ly + 21, bx + 72, ly + 21, 2.4f, pal::with);
                p.text(bx + 76, ly + 14, "Curveball", 11, pal::with);
                for (int dsh = 0; dsh < 3; ++dsh) p.line(bx + 150 + dsh * 7, ly + 21, bx + 154 + dsh * 7, ly + 21, 1.2f, pal::text, 0.6f);
                p.text(bx + 174, ly + 14, "certified bound ½√(|Ω|−1)(1−1/B)^t", 11, pal::dim);
            }

            // ---- HUD
            p.metric_text("burn-in per layout", k == 0 ? fmt("%lld swaps (ad hoc)", T_adhoc) : fmt("%lld trades (certified)", T_cert),
                          k == 0 ? Tone::Warn : Tone::Good);
            p.metric("µs / step (mean)", 1000.0 * sd.ms_total / double(sd.steps_total), "%.3f");
            p.metric(fmt("exact TV · n=6 d=2 @ %lld", k == 0 ? (long long)toyA.m : TcA), tvA[k], "%.2e", tvA[k] < 0.01 ? Tone::Good : Tone::Bad);
            p.metric(fmt("exact TV · n=8 irregular @ %lld", k == 0 ? (long long)toyB.m : TcB), tvB[k], "%.2e", tvB[k] < 0.01 ? Tone::Good : Tone::Bad);
            double ac = sd.autocorr_sum / sd.layouts;
            p.metric("edge autocorr. vs template (all layouts)", ac, "%+.3f", std::fabs(ac) < 0.02 ? Tone::Good : Tone::Bad);
            p.metric("changed doors vs template (this level)", m - kept, "%.0f");
            p.sparkline("edge autocorr. vs template (all layouts)", false);
            sd.changed_total += changed;
        }
    }

    h.result("rooms", n);
    h.result("doors", m);
    h.result("B_pairs", B);
    h.result("certified_burn_in_trades", (double)T_cert);
    h.result("adhoc_burn_in_swaps", (double)T_adhoc);
    h.result("toyA_states", (double)toyA.states.size());
    h.result("toyB_states", (double)toyB.states.size());
    h.result("toyA_certified_generic", (double)TcA);
    h.result("toyA_certified_with_known_Omega", (double)TcA_omega);
    h.result("toyB_certified_generic", (double)TcB);
    h.result("toyB_certified_with_known_Omega", (double)TcB_omega);
    h.result("toyA_TV_swap_at_m", tvA[0]);
    h.result("toyA_TV_curveball_at_cert", tvA[1]);
    h.result("toyA_TV_curveball_at_m", toyA.tv_cb[toyA.m]);
    h.result("toyA_worst_start_TV_swap_at_m", worstA_sw);
    h.result("toyA_worst_start_TV_curveball_at_cert", worstA_cb);
    h.result("toyB_TV_swap_at_m", tvB[0]);
    h.result("toyB_TV_curveball_at_cert", tvB[1]);
    h.result("toyB_TV_curveball_at_m", toyB.tv_cb[toyB.m]);
    h.result("toyB_TV_swap_at_cert", toyB.tv_sw[TcB]);
    h.result("toyB_TV_swap_at_10m", toyB.tv_sw[10 * toyB.m]);
    h.result("exact_TV_below_certified_bound_all_t", bound_ok ? "yes" : "NO");
    h.result("ref_template_overlap_uniform_swapchain", ref_overlap);
    h.result("ref_template_overlap_uniform_curveball", ref_overlap_cb);
    h.result("ref_connected_fraction_uniform", ref_conn);
    for (int k = 0; k < 2; ++k) {
        Side& sd = S[k];
        Panel& p = h.panel(k);
        p.result("layouts_generated", (double)sd.layouts);
        p.result("mean_template_overlap", sd.overlap_sum / sd.layouts);
        p.result("edge_autocorrelation_vs_template", sd.autocorr_sum / sd.layouts);
        p.result("connected_fraction", double(sd.connected) / sd.layouts);
        p.result("us_per_step", 1000.0 * sd.ms_total / double(sd.steps_total));
        p.result("edges_toggled_per_layout", double(sd.changed_total) / sd.layouts);
    }
    return h.finish();
}
