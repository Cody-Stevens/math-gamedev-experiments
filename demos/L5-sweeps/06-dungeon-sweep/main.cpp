// L5-sweeps/06-dungeon-sweep - sweep-only load ladder for demos/06-dungeon-shuffle (family 131).
//
// Load n = rooms: 10, 20, 40, 80, 160, 320. Template dungeon = rows x cols grid of rooms, ~19% of
// the grid corridors removed, ~3n/20 long "secret passages" from a few hub rooms (n = 20 is the
// original demo's template exactly), so door counts stay ~1..6 and doors/rooms ~1.4.
// One frame = generate one complete fresh layout from the template (same degree sequence).
//   WITHOUT:   lazy double-edge swap, ad hoc burn-in of 1 swap per door (m swaps).
//   empirical: Curveball pair trades with the same ad hoc budget (m trades) - what a dev might
//              use after reading about Curveball, without the certificate.
//   WITH:      Curveball pair trades (sec. 2, setup.tex eq:pair-operators) run for the burn-in
//              certified by sec. 7 (mixing.tex mix:H-poincare => spectral gap >= 1/B):
//              T = ceil(B (1 + B/2) ln 2), B = C(n,2)  -> TV <= 1/4 from every start.
//   (preprints/Polynomial-Mixing-of-the-Switch-Chain-for-Every-Graphical-Degree-Sequence-September-25-2026)
// The step kernels are the original demo's, generalized from one 64-bit word per adjacency row
// to ceil(n/64) words. All timing is single-threaded (a layout is one sequential Markov chain).
// Quality proxy (EMPIRICAL): "template doors kept" per layout vs a long-run reference
// (swap chain run for >= 200 m swaps, then sampled every m swaps; cross-checked by a long
// Curveball run). Reported as edge autocorrelation (mean kept - ref)/(m - ref) +- 1 standard error,
// and the TV distance between the kept-doors histograms (with its sampling-noise floor).
// Exact TV is only computable for tiny n; the original demo reports it for n = 6 and n = 8.
//
// Build/run (Git Bash, from demos/):  ./build.sh L5-sweeps/06-dungeon-sweep && L5-sweeps/06-dungeon-sweep/demo.exe
// Flags: --max-n N (default 320)  --min-ms 1500  --qthreads T (untimed quality samples, default 12)  --out DIR
#include "../ladder_common.h"

#include <array>
#include <atomic>
#include <bit>
#include <cstdint>
#include <random>
#include <thread>

using namespace demo;
using u64 = uint64_t;

// ---------------------------------------------------------------- RNG (copied from 06)
struct Rng {
    u64 s;
    explicit Rng(u64 seed) : s(seed * 0x9E3779B97F4A7C15ull + 0x1234567ull) {}
    u64 next() {
        u64 z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    uint32_t below(uint32_t n) {
        uint64_t m = uint64_t(uint32_t(next())) * n;
        uint32_t l = uint32_t(m);
        if (l < n) {
            uint32_t t = uint32_t(-n) % n;
            while (l < t) { m = uint64_t(uint32_t(next())) * n; l = uint32_t(m); }
        }
        return uint32_t(m >> 32);
    }
};

// ---------------------------------------------------------------- multi-word adjacency bitsets
constexpr int WMAX = 8;  // up to 512 rooms
struct Graph {
    int n = 0, W = 1;
    std::vector<u64> adj;  // n rows x W words
    void init(int n_) { n = n_; W = (n + 63) / 64; adj.assign(size_t(n) * W, 0); }
    u64* row(int u) { return &adj[size_t(u) * W]; }
    const u64* row(int u) const { return &adj[size_t(u) * W]; }
    bool has(int u, int v) const { return (row(u)[v >> 6] >> (v & 63)) & 1; }
    void toggle(int u, int v) { row(u)[v >> 6] ^= 1ull << (v & 63); row(v)[u >> 6] ^= 1ull << (u & 63); }
    int deg(int u) const { int s = 0; for (int w = 0; w < W; ++w) s += std::popcount(row(u)[w]); return s; }
    int edges() const { int s = 0; for (int u = 0; u < n; ++u) s += deg(u); return s / 2; }
};

// WITH / empirical: undirected Curveball pair trade (06's curveball_step, W words per row)
static int curveball_step(Graph& g, Rng& r) {
    const int n = g.n, W = g.W;
    int i = (int)r.below(n), j = (int)r.below(n - 1);
    if (j >= i) ++j;
    u64* ri = g.row(i);
    u64* rj = g.row(j);
    u64 S[WMAX], Ai[WMAX];
    int q = 0, N = 0;
    for (int w = 0; w < W; ++w) {
        u64 A = ri[w], Bm = rj[w];
        if (w == (j >> 6)) A &= ~(1ull << (j & 63));
        if (w == (i >> 6)) Bm &= ~(1ull << (i & 63));
        S[w] = A ^ Bm;
        Ai[w] = A & S[w];
        q += std::popcount(Ai[w]);
        N += std::popcount(S[w]);
    }
    if (q == 0 || q == N) return 0;
    int list[64 * WMAX], c = 0;
    for (int w = 0; w < W; ++w)
        for (u64 s = S[w]; s; s &= s - 1) list[c++] = (w << 6) | std::countr_zero(s);
    u64 flip[WMAX] = {};
    for (int k = 0; k < q; ++k) {  // partial Fisher-Yates: uniform q-subset of the N singletons
        int t = k + (int)r.below(N - k);
        std::swap(list[k], list[t]);
        flip[list[k] >> 6] |= 1ull << (list[k] & 63);
    }
    int changed = 0;
    for (int w = 0; w < W; ++w) {
        flip[w] ^= Ai[w];
        if (!flip[w]) continue;
        ri[w] ^= flip[w];
        rj[w] ^= flip[w];
        changed += 2 * std::popcount(flip[w]);
        for (u64 s = flip[w]; s; s &= s - 1) {
            int v = (w << 6) | std::countr_zero(s);
            u64* rv = g.row(v);
            rv[i >> 6] ^= 1ull << (i & 63);
            rv[j >> 6] ^= 1ull << (j & 63);
        }
    }
    return changed;
}

// WITHOUT: lazy double-edge swap (06's swap_step)
struct SwapGraph {
    Graph g;
    std::vector<std::array<uint16_t, 2>> e;
};
static int swap_step(SwapGraph& s, Rng& r) {
    const int m = (int)s.e.size();
    int a = (int)r.below(m), b = (int)r.below(m - 1);
    if (b >= a) ++b;
    int u = s.e[a][0], v = s.e[a][1], x = s.e[b][0], y = s.e[b][1];
    if (r.next() & 1) std::swap(x, y);
    if (u == x || u == y || v == x || v == y) return 0;
    if (s.g.has(u, x) || s.g.has(v, y)) return 0;
    s.g.toggle(u, v); s.g.toggle(x, y); s.g.toggle(u, x); s.g.toggle(v, y);
    s.e[a] = {(uint16_t)u, (uint16_t)x};
    s.e[b] = {(uint16_t)v, (uint16_t)y};
    return 4;
}

static int overlap(const Graph& a, const Graph& b) {
    int s = 0;
    for (size_t k = 0; k < a.adj.size(); ++k) s += std::popcount(a.adj[k] & b.adj[k]);
    return s / 2;
}

// ---------------------------------------------------------------- templates
static Graph make_template(int n) {
    int rows, cols;
    switch (n) {
        case 10: rows = 2; cols = 5; break;
        case 20: rows = 4; cols = 5; break;
        case 40: rows = 5; cols = 8; break;
        case 80: rows = 8; cols = 10; break;
        case 160: rows = 10; cols = 16; break;
        default: rows = 16; cols = 20; break;  // 320
    }
    Graph g;
    g.init(rows * cols);
    auto id = [&](int r, int c) { return r * cols + c; };
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c + 1 < cols; ++c) g.toggle(id(r, c), id(r, c + 1));
    for (int r = 0; r + 1 < rows; ++r)
        for (int c = 0; c < cols; ++c) g.toggle(id(r, c), id(r + 1, c));
    if (n == 20) {  // the original demo's template, exactly
        const int rm[][4] = {{0, 1, 0, 2}, {2, 3, 2, 4}, {1, 0, 2, 0}, {2, 2, 3, 2}, {1, 3, 2, 3}, {3, 0, 3, 1}};
        for (auto& e : rm) g.toggle(id(e[0], e[1]), id(e[2], e[3]));
        g.toggle(id(1, 2), id(3, 4));
        g.toggle(id(1, 2), id(0, 0));
        g.toggle(id(2, 1), id(0, 4));
        return g;
    }
    // same recipe at scale: remove ~6/31 of the grid corridors (never isolating a room), then add
    // ~3n/20 secret passages, each from one of ~n/10 hub rooms to a non-adjacent room
    Rng r(1310 + n);
    std::vector<std::array<int, 2>> grid;
    for (int u = 0; u < g.n; ++u)
        for (int v = u + 1; v < g.n; ++v)
            if (g.has(u, v)) grid.push_back({u, v});
    const int nrm = int(std::lround(grid.size() * 6.0 / 31.0));
    for (int k = 0, done = 0; done < nrm && k < 100000; ++k) {
        auto e = grid[r.below((uint32_t)grid.size())];
        if (!g.has(e[0], e[1]) || g.deg(e[0]) <= 1 || g.deg(e[1]) <= 1) continue;
        g.toggle(e[0], e[1]);
        ++done;
    }
    const int nsec = int(std::lround(3.0 * n / 20.0)), nhub = std::max(1, n / 10);
    std::vector<int> hubs;
    while ((int)hubs.size() < nhub) {
        int h = (int)r.below(g.n);
        if (std::find(hubs.begin(), hubs.end(), h) == hubs.end()) hubs.push_back(h);
    }
    for (int k = 0, done = 0; done < nsec && k < 100000; ++k) {
        int h = hubs[done % nhub], v = (int)r.below(g.n);
        int dr = std::abs(h / cols - v / cols), dc = std::abs(h % cols - v % cols);
        if (v == h || dr + dc <= 1 || g.has(h, v)) continue;
        g.toggle(h, v);
        ++done;
    }
    return g;
}

static long long certified_T(int n) {  // ceil(B (1 + B/2) ln 2), B = C(n,2)  (sec. 7, mix:H-poincare)
    double B = n * (n - 1) / 2.0;
    return (long long)std::ceil(B * (1.0 + B / 2.0) * std::log(2.0));
}

// ---------------------------------------------------------------- layout generators
struct Ctx {
    Graph G0;
    std::vector<std::array<uint16_t, 2>> E0;  // template edge list (built once, copied per layout)
    int m = 0;
};
static Graph layout_swap(const Ctx& c, long long T, u64 seed) {
    Rng r(seed);
    SwapGraph s;
    s.g = c.G0;
    s.e = c.E0;
    for (long long t = 0; t < T; ++t) swap_step(s, r);
    return s.g;
}
static Graph layout_cb(const Ctx& c, long long T, u64 seed) {
    Rng r(seed);
    Graph g = c.G0;
    for (long long t = 0; t < T; ++t) curveball_step(g, r);
    return g;
}

struct Sampler {  // kept-doors samples of one method
    std::vector<int> kept;
    double mean() const { double s = 0; for (int k : kept) s += k; return kept.empty() ? NAN : s / kept.size(); }
    double sd() const {
        if (kept.size() < 2) return NAN;
        double m = mean(), s = 0;
        for (int k : kept) s += (k - m) * (k - m);
        return std::sqrt(s / (kept.size() - 1));
    }
};
static double hist_tv(const std::vector<int>& a, const std::vector<double>& ref) {
    std::vector<double> h(ref.size(), 0.0);
    for (int k : a) h[std::min<size_t>(k, h.size() - 1)] += 1.0 / a.size();
    double t = 0;
    for (size_t i = 0; i < h.size(); ++i) t += std::fabs(h[i] - ref[i]);
    return 0.5 * t;
}

int main(int argc, char** argv) {
    const int max_n = arg_int(argc, argv, "--max-n", 320);
    const double min_ms = arg_double(argc, argv, "--min-ms", 1500);
    const int qthreads = arg_int(argc, argv, "--qthreads", 12);
    const int NQ = 4000;  // quality samples for the cheap methods
    const std::string out = ladder::out_dir(argc, argv);
    const double t_start = now_ms();

    ladder::Ladder L;
    L.demo = "L5-sweeps/06-dungeon-sweep";
    L.family = "131";
    L.load_name = "rooms (template doors ~1.4 per room, door counts 1..6)";
    L.threads_without = L.threads_with = 1;
    std::printf("06-dungeon-sweep: 1 layout per frame, single-threaded timing; untimed WITH quality samples on %d threads\n", qthreads);

    double prev_with_ms = 0;
    for (int n = 10; n <= max_n; n *= 2) {
        Ctx c;
        c.G0 = make_template(n);
        for (int u = 0; u < n; ++u)
            for (int v = u + 1; v < n; ++v)
                if (c.G0.has(u, v)) c.E0.push_back({(uint16_t)u, (uint16_t)v});
        c.m = (int)c.E0.size();
        const int m = c.m, B = n * (n - 1) / 2;
        const long long Tcert = certified_T(n), Tadhoc = m;
        int dmin = 99, dmax = 0;
        for (int u = 0; u < n; ++u) { dmin = std::min(dmin, c.G0.deg(u)); dmax = std::max(dmax, c.G0.deg(u)); }

        // ---- long-run reference for "template doors kept" (empirical)
        std::vector<double> ref_hist(m + 1, 0.0);
        double ref_mean = 0, ref_mean_cb = 0;
        std::vector<int> ref_draws;  // for the histogram-TV noise floor
        {
            Rng r(777 + n);
            SwapGraph s;
            s.g = c.G0;
            s.e = c.E0;
            for (long long k = 0; k < std::max<long long>(200000, 200LL * m); ++k) swap_step(s, r);
            const int NS = 20000;
            for (int k = 0; k < NS; ++k) {
                for (int q = 0; q < m; ++q) swap_step(s, r);
                int ov = overlap(s.g, c.G0);
                ref_hist[ov] += 1.0 / NS;
                ref_mean += double(ov) / NS;
            }
            for (int k = 0; k < NQ; ++k) {  // independent draws of the same size as the cheap samples
                for (int q = 0; q < m; ++q) swap_step(s, r);
                ref_draws.push_back(overlap(s.g, c.G0));
            }
            Graph g = c.G0;
            Rng r2(778 + n);
            for (long long k = 0; k < std::max<long long>(200000, 20LL * B); ++k) curveball_step(g, r2);
            const int NC = 4000;
            for (int k = 0; k < NC; ++k) {
                for (int q = 0; q < 4 * n; ++q) curveball_step(g, r2);
                ref_mean_cb += double(overlap(g, c.G0)) / NC;
            }
        }
        const double noise_floor = hist_tv(ref_draws, ref_hist);

        // ---- timing: one complete layout per frame. Layouts far below the timer resolution are timed
        // in batches of K (each sample = batch time / K, K chosen so a batch takes >= 0.5 ms).
        auto time_method = [&](auto gen, long long T, u64 seedbase, Sampler& smp, double est_ms, int& K) {
            std::vector<double> t;
            double w0 = now_ms();
            if (est_ms < 50) gen(T, seedbase + 999999);  // warm-up layout (also calibrates K)
            else gen(std::min<long long>(T, 200000), seedbase + 999999);  // partial warm-up for slow stages
            const double one = now_ms() - w0;
            K = (est_ms < 50 && one < 0.5) ? int(std::min(100000.0, std::ceil(0.5 / std::max(one, 1e-5)))) : 1;
            if (K > 1) {  // re-calibrate on a warm batch
                double b0 = now_ms();
                for (int r = 0; r < K; ++r) gen(T, seedbase + 888888 + r);
                double per = (now_ms() - b0) / K;
                K = std::max(1, int(std::min(100000.0, std::ceil(0.5 / std::max(per, 1e-6)))));
            }
            double sum = 0;
            u64 sink = 0;
            for (int k = 0;; ++k) {
                double t0 = now_ms();
                Graph g;
                for (int r = 0; r < K; ++r) { g = gen(T, seedbase + u64(k) * K + r); sink += g.adj[0]; }
                double dt = now_ms() - t0;
                t.push_back(dt / K);
                sum += dt;
                smp.kept.push_back(overlap(g, c.G0));  // last layout of the batch (untimed)
                const bool slow = dt > 250;
                if (!slow && k + 1 >= 15 && sum >= min_ms) break;
                if (slow && sum >= 5000) break;  // very slow stage: >= 5 s of samples (>= 1 layout)
                if (k + 1 >= 100000) break;
            }
            if (sink == 42) std::printf(" ");
            return t;
        };
        Sampler sw, emp, cb;
        int Ksw = 1, Kemp = 1, Kcb = 1;
        auto gsw = [&](long long T, u64 s) { return layout_swap(c, T, s); };
        auto gcb = [&](long long T, u64 s) { return layout_cb(c, T, s); };
        std::vector<double> tsw = time_method(gsw, Tadhoc, 1000000ull * n + 1, sw, 0, Ksw);
        std::vector<double> temp = time_method(gcb, Tadhoc, 1000000ull * n + 2000000000ull, emp, 0, Kemp);
        std::vector<double> tcb = time_method(gcb, Tcert, 1000000ull * n + 4000000000ull, cb, prev_with_ms * 16, Kcb);
        const int with_timed = (int)tcb.size();

        // ---- untimed quality samples
        for (int k = (int)sw.kept.size(); k < NQ; ++k) sw.kept.push_back(overlap(layout_swap(c, Tadhoc, 7000000000ull + 1000000ull * n + k), c.G0));
        for (int k = (int)emp.kept.size(); k < NQ; ++k) emp.kept.push_back(overlap(layout_cb(c, Tadhoc, 8000000000ull + 1000000ull * n + k), c.G0));
        {
            const double med = ladder::stats(tcb).median;
            // budget ~12 s of wall on qthreads threads (or NQ samples, whichever is fewer); at least one batch
            // skipped when one layout takes > 12 s (then only the timed layouts are quality samples)
            long long want = med > 12000 ? 0
                                         : std::min<long long>(NQ - (long long)cb.kept.size(),
                                                               std::max<long long>(qthreads, (long long)(12000.0 * qthreads / std::max(med, 1e-3))));
            want = std::max<long long>(0, want);
            std::atomic<long long> next{0};
            std::vector<std::vector<int>> part(qthreads);
            std::vector<std::thread> th;
            for (int q = 0; q < qthreads; ++q)
                th.emplace_back([&, q] {
                    for (long long k; (k = next.fetch_add(1)) < want;)
                        part[q].push_back(overlap(layout_cb(c, Tcert, 9000000000ull + 1000000ull * n + k), c.G0));
                });
            for (auto& x : th) x.join();
            for (auto& p : part) cb.kept.insert(cb.kept.end(), p.begin(), p.end());
        }
        auto qobj = [&](const Sampler& s) {
            double mu = s.mean(), se = s.sd() / std::sqrt(double(s.kept.size()));
            std::string o = "{\"samples\": " + std::to_string(s.kept.size()) + ", \"mean_kept\": " + ladder::jnum(mu) +
                            ", \"autocorr\": " + ladder::jnum((mu - ref_mean) / (m - ref_mean)) +
                            ", \"autocorr_se\": " + ladder::jnum(se / (m - ref_mean));
            o += ", \"hist_tv\": " + (s.kept.size() >= 1000 ? ladder::jnum(hist_tv(s.kept, ref_hist)) : std::string("null"));
            return o + "}";
        };
        ladder::Stage st;
        st.n = n;
        st.without.st = ladder::stats(tsw);
        st.with.st = ladder::stats(tcb);
        ladder::Series es;
        es.st = ladder::stats(temp);
        st.more.push_back({"empirical", es});
        st.without.extra = "\"layouts_per_timed_sample\": " + std::to_string(Ksw) + ", \"steps\": " + std::to_string(Tadhoc) + ", \"method\": \"double-edge swap, 1 swap per door\"";
        st.with.extra = "\"layouts_per_timed_sample\": " + std::to_string(Kcb) + ", \"steps\": " + std::to_string(Tcert) + ", \"method\": \"Curveball, certified T = ceil(B(1+B/2) ln 2)\"" +
                        ", \"us_per_step\": " + ladder::jnum(1000.0 * ladder::stats(tcb).median / Tcert);
        st.more[0].second.extra = "\"layouts_per_timed_sample\": " + std::to_string(Kemp) + ", \"steps\": " + std::to_string(Tadhoc) + ", \"method\": \"Curveball, 1 trade per door (no certificate)\"";
        auto aut = [&](const Sampler& s) { return (s.mean() - ref_mean) / (m - ref_mean); };
        st.quality = "{\"name\": \"edge autocorrelation vs template, (mean doors kept - ref)/(m - ref), EMPIRICAL; 0 = uniform\", "
                     "\"without\": " + ladder::jnum(aut(sw)) + ", \"with\": " + ladder::jnum(aut(cb)) + ", \"empirical\": " +
                     ladder::jnum(aut(emp)) + ", \"higher_is_better\": false, \"target\": 0, " +
                     "\"detail\": {\"without\": " + qobj(sw) + ", \"with\": " + qobj(cb) + ", \"empirical\": " + qobj(emp) + "}, " +
                     "\"reference_mean_kept_swapchain\": " + ladder::jnum(ref_mean) + ", \"reference_mean_kept_curveball\": " +
                     ladder::jnum(ref_mean_cb) + ", \"hist_tv_noise_floor_at_4000\": " + ladder::jnum(noise_floor) + "}";
        st.extra = "\"doors_m\": " + std::to_string(m) + ", \"B_pairs\": " + std::to_string(B) + ", \"certified_T\": " +
                   std::to_string(Tcert) + ", \"degree_min\": " + std::to_string(dmin) + ", \"degree_max\": " + std::to_string(dmax) +
                   ", \"with_frames_timed\": " + std::to_string(with_timed);
        L.stages.push_back(st);
        prev_with_ms = st.with.st.median;
        std::printf("n=%3d m=%3d deg %d..%d T=%lld | WITHOUT %.4g ms  empirical %.4g ms  WITH %.4g ms (%d frames, %.4f us/step) | autocorr %+.4f / %+.4f / %+.4f (n_with=%zu) ref %.2f/%.2f floor %.3f  [%.0f s]\n",
                    n, m, dmin, dmax, Tcert, st.without.st.median, es.st.median, st.with.st.median, with_timed,
                    1000.0 * st.with.st.median / Tcert, aut(sw), aut(emp), aut(cb), cb.kept.size(), ref_mean, ref_mean_cb, noise_floor,
                    (now_ms() - t_start) / 1000);
        std::fflush(stdout);
    }
    L.notes =
        "Proved (sec. 7, mix:H-poincare): the Curveball chain has spectral gap >= 1/B, so T = ceil(B(1+B/2) ln 2) trades give "
        "TV <= 1/4 to uniform from every start for every graphical degree sequence; T grows ~n^4/8, so WITH's cost per layout "
        "grows ~n^4 and falls off the 16.7 ms budget early. The certified budget is a worst-case guarantee, not the actual "
        "mixing time. Measured (EMPIRICAL proxy, not TV): template doors kept per layout vs a long-run reference; autocorr 0 "
        "means the layout keeps template doors at the uniform rate. Exact TV is only computable for tiny graphs (original demo: "
        "n=6 and n=8 toys, swap 0.215/0.459 at 1 swap per door, certified Curveball ~1e-14). Single-threaded timing on every "
        "side; WITH quality samples beyond the timed ones are generated untimed on several threads. Template: grid rooms, "
        "~19% of grid corridors removed, ~3n/20 hub passages (n = 20 is the original template).";
    L.write(out + "/ladder.json");
    L.print_table();
    std::printf("wrote %s/ladder.json  (total %.1f s)\n", out.c_str(), (now_ms() - t_start) / 1000);
    return 0;
}
