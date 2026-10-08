// L3-fractal-ladder - load ladder for 04-fractal-frontier (Family 223).
//
// Question tested: because thm:main proves that c * n^{-7/4} * N_n (the delta^{7/4}-scaled
// counting measure of the critical q = 1 FK Dobrushin interface) converges, can a game run the
// interface on a COARSE grid and still get the timing / occupation statistics of a FINE grid?
//
// Load = grid size n (32 ... 8192). The simulated object is the same on both sides: the exact
// q = 1 FK interface of fk.h (reused from 04-fractal-frontier). Only the clock differs:
//   WITHOUT: length clock, each traversal costs C_len * delta (delta = 1/n). C_len is tuned at
//            the reference grid n_ref (both clocks agree there), as in the original demo.
//   WITH:    natural clock, each traversal costs 1 s * delta^{7/4}, one constant for every n.
// The reference is an independent sample at n_ref = 8192 (6,000 curves). At every stage we
// measure (a) compute cost per curve (ms, 1 thread; lattice steps) and (b) the error of the
// median duration and of the 8x8 occupation measure (mean time per bin) against the reference,
// with bootstrap 95% CIs. Key output: cheapest n meeting a target accuracy, per clock.
//
//   ./build.sh L3-fractal-ladder run                 -> video (out/L3-fractal-ladder.mp4)
//   ./demo.exe --sweep                               -> out/ladder.json + table (no video)
//   flags: --stats-scale X (curve counts x X), --boot B, --threads T, --stage-frames F,
//          --from-stage K (preview a later stage), --no-extra (sweep: skip 16384 timing stage)
#include "demo.h"
#include "../04-fractal-frontier/fk.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <filesystem>
#include <vector>

using namespace demo;

constexpr int NB = 8, NBB = NB * NB;
static const int STAGE_N[] = {16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192};
constexpr int NSTAGE = 10;
constexpr int NREF = 8192;
constexpr double TARGET_MED = 0.05, TARGET_OCC = 0.10, BUDGET = 16.7;
constexpr uint64_t SALT_STAGE = 0x51A6E5EEDull, SALT_REF = 0x8EFE8E0CEull;

static double dt_with(int n) { return std::pow(double(n), -1.75); }                     // 1 s * delta^{7/4}
static double dt_without(int n) { return std::pow(double(NREF), -0.75) / double(n); }   // C_len * delta, agrees at n_ref
static double dt_of(int side, int n) { return side ? dt_with(n) : dt_without(n); }
static uint64_t curve_key(uint64_t salt, int n, long long idx) {
    return fk::mix64(fk::mix64(salt * 0x9E3779B97F4A7C15ull + uint64_t(n)) + uint64_t(idx));
}
static int stage_curves(int n) {
    if (n <= 512) return 16000;
    if (n <= 1024) return 12000;
    if (n <= 2048) return 8000;
    if (n <= 4096) return 4000;
    return 3000;
}
constexpr int REF_CURVES = 6000;

// ------------------------------------------------------------------ the simulation step
// Sample + trace one curve at grid n, bin every traversal into the 8x8 occupation grid.
// If PATH, also record every stride-th projected midpoint (drawing only, never timed).
template <bool PATH>
static long long trace_one(int n, uint64_t key, int32_t* cnt, std::vector<float>* path = nullptr, long long stride = 1) {
    fk::Tracer t;
    t.n = n;
    t.key = key;
    long long c = 0;
    if constexpr (PATH) { path->clear(); path->push_back(0.f); path->push_back(0.f); }
    long long N = t.trace([&](float x, float y) {
        int bx = std::min(NB - 1, int(x * NB)), by = std::min(NB - 1, int(y * NB));
        ++cnt[by * NB + bx];
        if constexpr (PATH) {
            if (++c == stride) { c = 0; path->push_back(x); path->push_back(y); }
        }
    });
    if constexpr (PATH) { path->push_back(1.f); path->push_back(1.f); }
    if (N < 0) { std::fprintf(stderr, "trace failed n=%d\n", n); std::exit(3); }
    return N;
}

// What one side does per "frame" (this is the timed work): one curve, its clock time and its
// occupation time per bin. The two sides do identical work except for the dt value.
struct ClockSide {
    int side = 0;
    double occ[NBB] = {}, last_dur = 0;
    long long curves = 0, steps = 0;
    long long step(int n, uint64_t key) {
        int32_t cnt[NBB] = {};
        long long N = trace_one<false>(n, key, cnt);
        const double dt = dt_of(side, n);
        for (int k = 0; k < NBB; ++k) occ[k] += double(cnt[k]) * dt;
        last_dur = double(N) * dt;
        ++curves; steps += N;
        return N;
    }
};

// ------------------------------------------------------------------ statistics (untimed)
struct SampleSet {
    int n = 0, M = 0;
    std::vector<long long> N;
    std::vector<int32_t> cnt;  // M * 64
    double secs = 0;
};

static SampleSet sample_set(int n, uint64_t salt, int M, int threads) {
    SampleSet s;
    s.n = n; s.M = M;
    s.N.resize(M);
    s.cnt.assign(size_t(M) * NBB, 0);
    std::atomic<int> next{0};
    double t0 = now_ms();
    auto work = [&] {
        int i;
        while ((i = next.fetch_add(1)) < M) s.N[i] = trace_one<false>(n, curve_key(salt, n, i), &s.cnt[size_t(i) * NBB]);
    };
    std::vector<std::thread> th;
    for (int t = 0; t < threads; ++t) th.emplace_back(work);
    for (auto& t : th) t.join();
    s.secs = (now_ms() - t0) / 1000;
    return s;
}

struct Agg { double medN = 0, meanN = 0; double cnt[NBB] = {}; };
static Agg aggregate(const SampleSet& s, const std::vector<int>* pick) {
    Agg a;
    const int M = pick ? int(pick->size()) : s.M;
    std::vector<long long> v(M);
    for (int j = 0; j < M; ++j) {
        int i = pick ? (*pick)[j] : j;
        v[j] = s.N[i];
        a.meanN += double(s.N[i]);
        const int32_t* c = &s.cnt[size_t(i) * NBB];
        for (int k = 0; k < NBB; ++k) a.cnt[k] += c[k];
    }
    a.meanN /= M;
    for (int k = 0; k < NBB; ++k) a.cnt[k] /= M;
    size_t h = size_t(M - 1) / 2;
    std::nth_element(v.begin(), v.begin() + h, v.end());
    double lo = double(v[h]);
    if (M % 2 == 0) {
        double hi = double(*std::min_element(v.begin() + h + 1, v.end()));
        a.medN = 0.5 * (lo + hi);
    } else a.medN = lo;
    return a;
}

struct Errs { double med[2] = {}, occ[2] = {}, shape = 0; };
static Errs errs(const Agg& a, int n, const Agg& r) {
    Errs e;
    const double dr = dt_with(NREF), Dref = r.medN * dr;
    double tot_r = 0, tot_a = 0;
    for (int k = 0; k < NBB; ++k) { tot_r += r.cnt[k]; tot_a += a.cnt[k]; }
    for (int sd = 0; sd < 2; ++sd) {
        double dt = dt_of(sd, n);
        e.med[sd] = a.medN * dt / Dref - 1;
        double l1 = 0;
        for (int k = 0; k < NBB; ++k) l1 += std::fabs(a.cnt[k] * dt - r.cnt[k] * dr);
        e.occ[sd] = l1 / (tot_r * dr);
    }
    for (int k = 0; k < NBB; ++k) e.shape += std::fabs(a.cnt[k] / tot_a - r.cnt[k] / tot_r);
    return e;
}

struct StageStats {
    int n = 0, M = 0;
    double medN = 0, meanN = 0, meanN_se = 0, scaled = 0, scaled_se = 0;  // scaled = mean N n^{-7/4}
    double med_err[2] = {}, med_lo[2] = {}, med_hi[2] = {};   // signed relative error, 95% CI
    double occ_err[2] = {}, occ_lo[2] = {}, occ_hi[2] = {};   // L1 relative error, 95% CI
    double shape = 0, shape_lo = 0, shape_hi = 0;             // clock-independent shape L1
    double occ_floor = 0;                                     // expected L1 from sampling noise alone (WITH)
    double ks[2] = {};
    double med_dur[2] = {};
    double occ_mean[2][NBB] = {};
    std::vector<float> q[2];                                  // 201 duration quantiles per clock
    double secs = 0;
    bool meets(int sd) const { return std::fabs(med_err[sd]) <= TARGET_MED && occ_err[sd] <= TARGET_OCC; }
    bool meets_ci(int sd) const {
        return std::max(std::fabs(med_lo[sd]), std::fabs(med_hi[sd])) <= TARGET_MED && occ_hi[sd] <= TARGET_OCC;
    }
};
struct RefStats {
    int n = NREF, M = 0;
    double medN = 0, meanN = 0, scaled = 0, scaled_se = 0, med_dur = 0, mean_dur = 0, secs = 0;
    double occ[NBB] = {};
    std::vector<float> q;
};

static uint64_t g_rng = 0x1234567ull;
static uint64_t rnd() { g_rng += 0x9e3779b97f4a7c15ull; return fk::mix64(g_rng); }

static std::vector<float> quantiles(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    std::vector<float> q(201);
    for (int k = 0; k <= 200; ++k) q[k] = float(v[size_t(double(k) / 200 * (v.size() - 1))]);
    return q;
}
static double ks_dist(std::vector<double> a, std::vector<double> b) {
    std::sort(a.begin(), a.end()); std::sort(b.begin(), b.end());
    size_t i = 0, j = 0; double d = 0;
    while (i < a.size() && j < b.size()) {
        double x = std::min(a[i], b[j]);
        while (i < a.size() && a[i] <= x) ++i;
        while (j < b.size() && b[j] <= x) ++j;
        d = std::max(d, std::fabs(double(i) / a.size() - double(j) / b.size()));
    }
    return d;
}
static double pct(std::vector<double> v, double p) {
    std::sort(v.begin(), v.end());
    return v[size_t(p * (v.size() - 1) + 0.5)];
}

struct AllStats { RefStats ref; std::vector<StageStats> st; double secs = 0; int boot = 0; double scale = 1; };

static void compute_stats(AllStats& A, const std::vector<int>& stage_ns, double scale, int boot, int threads, bool quiet) {
    double t0 = now_ms();
    A.boot = boot; A.scale = scale;
    const int Mref = std::max(50, int(REF_CURVES * scale));
    if (!quiet) std::fprintf(stderr, "[stats] reference n=%d, %d curves, %d threads...\n", NREF, Mref, threads);
    SampleSet R = sample_set(NREF, SALT_REF, Mref, threads);
    Agg ra = aggregate(R, nullptr);
    {
        RefStats& rs = A.ref;
        rs.M = Mref; rs.medN = ra.medN; rs.meanN = ra.meanN; rs.secs = R.secs;
        double s2 = 0, sc = std::pow(double(NREF), -1.75);
        for (long long v : R.N) s2 += (v - ra.meanN) * (v - ra.meanN);
        rs.scaled = ra.meanN * sc; rs.scaled_se = std::sqrt(s2 / (Mref - 1) / Mref) * sc;
        rs.med_dur = ra.medN * dt_with(NREF); rs.mean_dur = ra.meanN * dt_with(NREF);
        for (int k = 0; k < NBB; ++k) rs.occ[k] = ra.cnt[k] * dt_with(NREF);
        std::vector<double> d(Mref);
        for (int i = 0; i < Mref; ++i) d[i] = R.N[i] * dt_with(NREF);
        rs.q = quantiles(d);
    }
    std::vector<double> dref(Mref);
    for (int i = 0; i < Mref; ++i) dref[i] = R.N[i] * dt_with(NREF);
    // per-bin variance of the reference (for the noise floor)
    double rvar[NBB] = {};
    for (int i = 0; i < Mref; ++i)
        for (int k = 0; k < NBB; ++k) { double d = R.cnt[size_t(i) * NBB + k] - ra.cnt[k]; rvar[k] += d * d; }
    for (int k = 0; k < NBB; ++k) rvar[k] /= (Mref - 1);

    A.st.assign(stage_ns.size(), StageStats{});
    for (size_t s = 0; s < stage_ns.size(); ++s) {
        const int n = stage_ns[s];
        const int M = std::max(50, int(stage_curves(n) * scale));
        SampleSet S = sample_set(n, SALT_STAGE, M, threads);
        StageStats& st = A.st[s];
        st.n = n; st.M = M; st.secs = S.secs;
        Agg a = aggregate(S, nullptr);
        Errs e = errs(a, n, ra);
        st.medN = a.medN; st.meanN = a.meanN;
        double s2 = 0;
        for (long long v : S.N) s2 += (v - a.meanN) * (v - a.meanN);
        st.meanN_se = std::sqrt(s2 / (M - 1) / M);
        double sc = std::pow(double(n), -1.75);
        st.scaled = a.meanN * sc; st.scaled_se = st.meanN_se * sc;
        for (int sd = 0; sd < 2; ++sd) {
            st.med_err[sd] = e.med[sd]; st.occ_err[sd] = e.occ[sd];
            double dt = dt_of(sd, n);
            st.med_dur[sd] = a.medN * dt;
            for (int k = 0; k < NBB; ++k) st.occ_mean[sd][k] = a.cnt[k] * dt;
            std::vector<double> d(M);
            for (int i = 0; i < M; ++i) d[i] = S.N[i] * dt;
            st.q[sd] = quantiles(d);
            st.ks[sd] = ks_dist(d, dref);
        }
        st.shape = e.shape;
        // noise floor of the WITH occupation L1 if the true difference were zero
        {
            double svar[NBB] = {};
            for (int i = 0; i < M; ++i)
                for (int k = 0; k < NBB; ++k) { double d = S.cnt[size_t(i) * NBB + k] - a.cnt[k]; svar[k] += d * d; }
            double dt = dt_with(n), dr = dt_with(NREF), num = 0, den = 0;
            for (int k = 0; k < NBB; ++k) {
                num += std::sqrt(2 / 3.14159265358979) * std::sqrt(dt * dt * svar[k] / (M - 1) / M + dr * dr * rvar[k] / Mref);
                den += ra.cnt[k] * dr;
            }
            st.occ_floor = num / den;
        }
        // bootstrap: resample stage curves and reference curves independently
        std::vector<double> bm[2], bo[2], bs;
        std::vector<int> pa(M), pr(Mref);
        for (int b = 0; b < boot; ++b) {
            for (int j = 0; j < M; ++j) pa[j] = int(rnd() % uint64_t(M));
            for (int j = 0; j < Mref; ++j) pr[j] = int(rnd() % uint64_t(Mref));
            Errs eb = errs(aggregate(S, &pa), n, aggregate(R, &pr));
            for (int sd = 0; sd < 2; ++sd) { bm[sd].push_back(eb.med[sd]); bo[sd].push_back(eb.occ[sd]); }
            bs.push_back(eb.shape);
        }
        if (boot > 1) {
            for (int sd = 0; sd < 2; ++sd) {
                st.med_lo[sd] = pct(bm[sd], 0.025); st.med_hi[sd] = pct(bm[sd], 0.975);
                st.occ_lo[sd] = pct(bo[sd], 0.025); st.occ_hi[sd] = pct(bo[sd], 0.975);
            }
            st.shape_lo = pct(bs, 0.025); st.shape_hi = pct(bs, 0.975);
        }
        if (!quiet)
            std::fprintf(stderr, "[stats] n=%5d M=%5d  %.1fs  medErr WITHOUT %+.3f WITH %+.4f [%+.4f,%+.4f]  occL1 WITHOUT %.3f WITH %.4f [%.4f,%.4f] floor %.4f  N/n^1.75=%.4f\n",
                         n, M, S.secs, st.med_err[0], st.med_err[1], st.med_lo[1], st.med_hi[1], st.occ_err[0], st.occ_err[1],
                         st.occ_lo[1], st.occ_hi[1], st.occ_floor, st.scaled);
    }
    A.secs = (now_ms() - t0) / 1000;
}

// ------------------------------------------------------------------ formatting helpers
static std::string commas(long long v) {
    std::string s = std::to_string(v), o;
    int c = 0;
    for (int i = int(s.size()) - 1; i >= 0; --i) { o.insert(o.begin(), s[i]); if (++c % 3 == 0 && i > 0) o.insert(o.begin(), ','); }
    return o;
}
static std::string si(double v) {
    if (v >= 1e9) return fmt("%.2fG", v / 1e9);
    if (v >= 1e6) return fmt("%.2fM", v / 1e6);
    if (v >= 1e3) return fmt("%.1fk", v / 1e3);
    return fmt("%.0f", v);
}
static std::string ms_str(double ms) {
    if (ms < 0.1) return fmt("%.3f ms", ms);
    if (ms < 10) return fmt("%.2f ms", ms);
    if (ms < 1000) return fmt("%.0f ms", ms);
    return fmt("%.2f s", ms / 1000);
}
static std::string nlab(int n) { return n >= 1024 ? fmt("%dk", n / 1024) : fmt("%d", n); }
static double median_of(std::vector<double> v) {
    if (v.empty()) return 0;
    size_t h = v.size() / 2;
    std::nth_element(v.begin(), v.begin() + h, v.end());
    return v[h];
}
static double p95_of(std::vector<double> v) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    return v[size_t(0.95 * (v.size() - 1))];
}
static double slope(const std::vector<double>& x, const std::vector<double>& y) {
    double mx = 0, my = 0;
    for (size_t i = 0; i < x.size(); ++i) { mx += x[i]; my += y[i]; }
    mx /= x.size(); my /= y.size();
    double sxy = 0, sxx = 0;
    for (size_t i = 0; i < x.size(); ++i) { sxy += (x[i] - mx) * (y[i] - my); sxx += (x[i] - mx) * (x[i] - mx); }
    return sxx > 0 ? sxy / sxx : 0;
}

// ------------------------------------------------------------------ sweep mode
struct Timing { std::vector<double> ms[2]; long long steps = 0; int curves = 0; bool reduced = false; };

static Timing time_stage(int n, double min_ms, int min_frames) {
    Timing T;
    ClockSide cs[2];
    cs[0].side = 0; cs[1].side = 1;
    // warm-up: a few untimed curves (or 50 ms worth)
    {
        double t0 = now_ms();
        for (int i = 0; i < 3 || now_ms() - t0 < 50; ++i) { cs[0].step(n, curve_key(SALT_STAGE, n, 1000000 + i)); if (i > 10000) break; }
    }
    double tot[2] = {0, 0};
    for (long long idx = 0;; ++idx) {
        uint64_t key = curve_key(SALT_STAGE, n, idx);
        for (int sd = 0; sd < 2; ++sd) {
            int s = (idx & 1) ? 1 - sd : sd;  // alternate order
            double a = now_ms();
            long long N = cs[s].step(n, key);
            double b = now_ms();
            T.ms[s].push_back(b - a);
            tot[s] += b - a;
            if (sd == 0) T.steps += N;
        }
        T.curves++;
        bool enough = int(T.ms[0].size()) >= min_frames && tot[0] >= min_ms && tot[1] >= min_ms;
        if (enough) break;
        // very slow stages: accept >= 5 curves once a side has used > 30 s
        if (int(T.ms[0].size()) >= 5 && tot[0] > 30000) { T.reduced = true; break; }
    }
    return T;
}

static int run_sweep(int argc, char** argv) {
    const double scale = arg_double(argc, argv, "--stats-scale", 1.0);
    const int boot = arg_int(argc, argv, "--boot", 400);
    const int threads = arg_int(argc, argv, "--threads", hw_threads());
    const bool extra = !arg_flag(argc, argv, "--no-extra");
    const double min_ms = arg_double(argc, argv, "--min-ms", 1500);
    std::string out = arg_str(argc, argv, "--out", "");
    if (out.empty()) {
        std::string exe = argv[0];
        size_t p = exe.find_last_of("/\\");
        out = (p == std::string::npos ? std::string(".") : exe.substr(0, p)) + "/out";
    }
    double t_start = now_ms();
    AllStats A;
    // sweep goes below the video ladder (n = 8) to find where the WITH clock breaks down
    std::vector<int> stat_ns = {8};
    stat_ns.insert(stat_ns.end(), STAGE_N, STAGE_N + NSTAGE);
    const int NS = int(stat_ns.size());
    compute_stats(A, stat_ns, scale, boot, threads, false);

    std::vector<int> ns = stat_ns;
    if (extra) ns.push_back(16384);
    std::vector<Timing> tm;
    for (int n : ns) {
        std::fprintf(stderr, "[time] n=%d ...", n);
        tm.push_back(time_stage(n, min_ms, 15));
        std::fprintf(stderr, " %zu curves/side, median %.4f / %.4f ms\n", tm.back().ms[0].size(), median_of(tm.back().ms[0]), median_of(tm.back().ms[1]));
    }

    // fits
    std::vector<double> lx, ly0, ly1, lxs, lys0, lys1, lxn, lyn;
    for (size_t i = 0; i < ns.size(); ++i) {
        double x = std::log(double(ns[i]));
        lx.push_back(x); ly0.push_back(std::log(median_of(tm[i].ms[0]))); ly1.push_back(std::log(median_of(tm[i].ms[1])));
        if (ns[i] >= 256) { lxs.push_back(x); lys0.push_back(ly0.back()); lys1.push_back(ly1.back()); }
    }
    for (int s = 0; s < NS; ++s) { lxn.push_back(std::log(double(stat_ns[s]))); lyn.push_back(std::log(A.st[s].meanN)); }
    double fit_all[2] = {slope(lx, ly0), slope(lx, ly1)}, fit_big[2] = {slope(lxs, lys0), slope(lxs, lys1)};
    double fit_steps = slope(lxn, lyn);
    int maxn_budget[2] = {0, 0}, minn_over[2] = {0, 0};
    for (size_t i = 0; i < ns.size(); ++i)
        for (int sd = 0; sd < 2; ++sd) {
            double m = median_of(tm[i].ms[sd]);
            if (m <= BUDGET) maxn_budget[sd] = std::max(maxn_budget[sd], ns[i]);
            else if (!minn_over[sd]) minn_over[sd] = ns[i];
        }
    // cost to target
    int first[2] = {-1, -1}, first_ci[2] = {-1, -1}, first_stable[2] = {-1, -1};
    for (int sd = 0; sd < 2; ++sd) {
        for (int s = 0; s < NS; ++s) {
            if (first[sd] < 0 && A.st[s].meets(sd)) first[sd] = s;
            if (first_ci[sd] < 0 && A.st[s].meets_ci(sd)) first_ci[sd] = s;
        }
        for (int s = NS - 1; s >= 0 && A.st[s].meets(sd); --s) first_stable[sd] = s;
    }

    // table
    std::printf("\nL3-fractal-ladder sweep  (reference n=%d, %d curves; threads for timing: 1 per side; stats: %d threads, untimed)\n", NREF, A.ref.M, threads);
    std::printf("%6s %7s %11s %11s %6s %6s | %16s %16s | %16s %16s | %7s\n", "n", "curves", "WITHOUT ms", "WITH ms", "ratio", "frames",
                "medErr WITHOUT", "medErr WITH", "occL1 WITHOUT", "occL1 WITH", "N/n^1.75");
    for (size_t i = 0; i < ns.size(); ++i) {
        double m0 = median_of(tm[i].ms[0]), m1 = median_of(tm[i].ms[1]);
        if (i < size_t(NS)) {
            const StageStats& s = A.st[i];
            std::printf("%6d %7d %11.4f %11.4f %6.3f %6zu | %+15.2f%% %+15.2f%% | %15.2f%% %15.2f%% | %7.4f\n", ns[i], s.M, m0, m1, m0 / m1,
                        tm[i].ms[0].size(), 100 * s.med_err[0], 100 * s.med_err[1], 100 * s.occ_err[0], 100 * s.occ_err[1], s.scaled);
        } else
            std::printf("%6d %7s %11.4f %11.4f %6.3f %6zu | (timing only)\n", ns[i], "-", m0, m1, m0 / m1, tm[i].ms[0].size());
    }
    std::printf("reference N/n^1.75 = %.4f +- %.4f\n", A.ref.scaled, A.ref.scaled_se);
    std::printf("cost exponent (median ms vs n): all %.3f / %.3f, n>=256 %.3f / %.3f;  steps exponent (mean N vs n) %.4f\n",
                fit_all[0], fit_all[1], fit_big[0], fit_big[1], fit_steps);
    auto pr = [&](const char* nm, int s) {
        if (s < 0) std::printf("  %s: never within ladder\n", nm);
        else std::printf("  %s: n=%d (%.4f ms/curve)\n", nm, stat_ns[s], median_of(tm[s].ms[0]));
    };
    std::printf("cost to target (|median err| <= 5%%, occupation L1 <= 10%%):\n");
    pr("WITHOUT point", first[0]); pr("WITH point", first[1]); pr("WITHOUT CI", first_ci[0]); pr("WITH CI", first_ci[1]);

    // ---- ladder.json
    std::error_code ec;
    std::filesystem::create_directories(out, ec);
    std::string path = out + "/ladder.json";
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) { std::fprintf(stderr, "cannot write %s\n", path.c_str()); return 1; }
    std::fprintf(f, "{\n  \"demo\": \"L3-fractal-ladder\",\n  \"family\": \"223\",\n");
    std::fprintf(f, "  \"load_name\": \"grid size n (n x n lattice in the unit square); one curve per frame\",\n");
    std::fprintf(f, "  \"budget_ms\": %.1f,\n  \"threads\": {\"without\": 1, \"with\": 1},\n", BUDGET);
    std::fprintf(f, "  \"timing_unit\": \"ms per curve = one frame of work: sample + trace one q=1 FK interface at grid n, bin traversals 8x8, apply the clock. Same curve (same seed) on both sides; the clocks differ only by a constant per traversal, so the cost is identical by construction.\",\n");
    std::fprintf(f, "  \"reference\": {\"n\": %d, \"curves\": %d, \"median_duration_s\": %.6g, \"mean_duration_s\": %.6g, \"mean_N_over_n^1.75\": %.6g, \"se\": %.3g, \"seeds\": \"independent of the stage seeds\"},\n",
                 NREF, A.ref.M, A.ref.med_dur, A.ref.mean_dur, A.ref.scaled, A.ref.scaled_se);
    std::fprintf(f, "  \"clocks\": {\"without\": \"length clock: dt = %d^(-3/4) s * delta per traversal (tuned so it agrees with WITH at the reference grid)\", \"with\": \"natural clock: dt = 1 s * delta^(7/4) per traversal, one constant for all n\"},\n", NREF);
    std::fprintf(f, "  \"stages\": [\n");
    for (size_t i = 0; i < ns.size(); ++i) {
        double m0 = median_of(tm[i].ms[0]), m1 = median_of(tm[i].ms[1]);
        std::fprintf(f, "    {\"n\": %d,\n", ns[i]);
        std::fprintf(f, "     \"without\": {\"median_ms\": %.6g, \"p95_ms\": %.6g, \"frames\": %zu},\n", m0, p95_of(tm[i].ms[0]), tm[i].ms[0].size());
        std::fprintf(f, "     \"with\":    {\"median_ms\": %.6g, \"p95_ms\": %.6g, \"frames\": %zu},\n", m1, p95_of(tm[i].ms[1]), tm[i].ms[1].size());
        std::fprintf(f, "     \"ratio_without_over_with\": %.4f,\n", m0 / m1);
        std::fprintf(f, "     \"mean_steps_per_timed_curve\": %.6g", double(tm[i].steps) / tm[i].curves);
        if (tm[i].reduced) std::fprintf(f, ",\n     \"timing_note\": \"slow stage: fewer than 15 frames\"");
        if (i < size_t(NS)) {
            const StageStats& s = A.st[i];
            std::fprintf(f, ",\n     \"steps_per_curve\": {\"mean\": %.6g, \"median\": %.6g, \"mean_over_n^1.75\": %.6g, \"se\": %.3g},\n", s.meanN, s.medN, s.scaled, s.scaled_se);
            std::fprintf(f, "     \"quality\": {\"name\": \"relative error of the median curve duration vs the %d^2 reference (|x|, lower is better)\", \"without\": %.6g, \"with\": %.6g, \"higher_is_better\": false,\n",
                         NREF, std::fabs(s.med_err[0]), std::fabs(s.med_err[1]));
            std::fprintf(f, "       \"curves\": %d, \"bootstrap\": %d,\n", s.M, A.boot);
            std::fprintf(f, "       \"median_duration_s\": {\"without\": %.6g, \"with\": %.6g, \"reference\": %.6g},\n", s.med_dur[0], s.med_dur[1], A.ref.med_dur);
            std::fprintf(f, "       \"median_err_signed\": {\"without\": %.6g, \"with\": %.6g, \"without_ci95\": [%.6g, %.6g], \"with_ci95\": [%.6g, %.6g]},\n",
                         s.med_err[0], s.med_err[1], s.med_lo[0], s.med_hi[0], s.med_lo[1], s.med_hi[1]);
            std::fprintf(f, "       \"occupation_L1\": {\"without\": %.6g, \"with\": %.6g, \"without_ci95\": [%.6g, %.6g], \"with_ci95\": [%.6g, %.6g], \"with_noise_floor\": %.4g},\n",
                         s.occ_err[0], s.occ_err[1], s.occ_lo[0], s.occ_hi[0], s.occ_lo[1], s.occ_hi[1], s.occ_floor);
            std::fprintf(f, "       \"occupation_shape_L1_clock_independent\": {\"value\": %.6g, \"ci95\": [%.6g, %.6g]},\n", s.shape, s.shape_lo, s.shape_hi);
            std::fprintf(f, "       \"duration_KS\": {\"without\": %.5g, \"with\": %.5g},\n", s.ks[0], s.ks[1]);
            std::fprintf(f, "       \"meets_target\": {\"without\": %s, \"with\": %s, \"without_ci\": %s, \"with_ci\": %s}}",
                         s.meets(0) ? "true" : "false", s.meets(1) ? "true" : "false", s.meets_ci(0) ? "true" : "false", s.meets_ci(1) ? "true" : "false");
        }
        std::fprintf(f, "}%s\n", i + 1 < ns.size() ? "," : "");
    }
    std::fprintf(f, "  ],\n");
    std::fprintf(f, "  \"fit\": {\"without_exponent\": %.4f, \"with_exponent\": %.4f, \"range\": \"all timed stages %d..%d\", \"without_exponent_n_ge_256\": %.4f, \"with_exponent_n_ge_256\": %.4f, \"steps_exponent_mean_N\": %.4f, \"expected\": 1.75},\n",
                 fit_all[0], fit_all[1], ns.front(), ns.back(), fit_big[0], fit_big[1], fit_steps);
    std::fprintf(f, "  \"max_n_within_budget\": {\"without\": %d, \"with\": %d, \"bracket\": \"budget crossed between n=%d and n=%d (WITHOUT), n=%d and n=%d (WITH)\"},\n",
                 maxn_budget[0], maxn_budget[1], maxn_budget[0], minn_over[0], maxn_budget[1], minn_over[1]);
    auto ctt = [&](int s) -> std::string {
        if (s < 0) return "null";
        return fmt("{\"n\": %d, \"median_ms_per_curve\": %.6g, \"mean_steps_per_curve\": %.6g, \"within_budget\": %s}", stat_ns[s],
                   median_of(tm[s].ms[0]), A.st[s].meanN, median_of(tm[s].ms[0]) <= BUDGET ? "true" : "false");
    };
    auto ratio = [&](int a, int b, bool steps) -> std::string {
        if (a < 0 || b < 0) return "null";
        return steps ? fmt("%.4g", A.st[a].meanN / A.st[b].meanN) : fmt("%.4g", median_of(tm[a].ms[0]) / median_of(tm[b].ms[1]));
    };
    std::fprintf(f, "  \"cost_to_target\": {\n");
    std::fprintf(f, "    \"target\": {\"median_duration_rel_err\": %.2f, \"occupation_L1_rel_err\": %.2f, \"reference_n\": %d},\n", TARGET_MED, TARGET_OCC, NREF);
    std::fprintf(f, "    \"criterion\": \"cheapest ladder n whose point estimates meet both targets\",\n");
    std::fprintf(f, "    \"without\": %s,\n    \"with\": %s,\n", ctt(first[0]).c_str(), ctt(first[1]).c_str());
    std::fprintf(f, "    \"cost_ratio_ms\": %s,\n    \"cost_ratio_steps\": %s,\n", ratio(first[0], first[1], false).c_str(), ratio(first[0], first[1], true).c_str());
    std::fprintf(f, "    \"ci_criterion\": {\"rule\": \"95%% bootstrap upper bounds of both errors meet the targets\", \"without\": %s, \"with\": %s, \"cost_ratio_ms\": %s, \"cost_ratio_steps\": %s},\n",
                 ctt(first_ci[0]).c_str(), ctt(first_ci[1]).c_str(), ratio(first_ci[0], first_ci[1], false).c_str(), ratio(first_ci[0], first_ci[1], true).c_str());
    std::fprintf(f, "    \"stable_from\": {\"rule\": \"smallest n from which every larger ladder stage also meets the target (point estimates)\", \"without\": %d, \"with\": %d},\n",
                 first_stable[0] >= 0 ? stat_ns[first_stable[0]] : -1, first_stable[1] >= 0 ? stat_ns[first_stable[1]] : -1);
    {
        // cheapest n vs target tightness (occupation target = 2 x median target)
        std::fprintf(f, "    \"target_sweep\": [");
        const double tg[] = {0.01, 0.02, 0.03, 0.05, 0.10, 0.20};
        for (int t = 0; t < 6; ++t) {
            double tm_ = tg[t], to_ = 2 * tg[t];
            int pt[2] = {-1, -1}, ci[2] = {-1, -1};
            for (int sd = 0; sd < 2; ++sd)
                for (int s = 0; s < NS; ++s) {
                    const StageStats& q = A.st[s];
                    bool okp = std::fabs(q.med_err[sd]) <= tm_ && q.occ_err[sd] <= to_;
                    bool okc = std::max(std::fabs(q.med_lo[sd]), std::fabs(q.med_hi[sd])) <= tm_ && q.occ_hi[sd] <= to_;
                    if (okp && pt[sd] < 0) pt[sd] = s;
                    if (okc && ci[sd] < 0) ci[sd] = s;
                }
            auto nn = [&](int s) { return s < 0 ? std::string("null") : std::to_string(stat_ns[s]); };
            auto rr = [&](int a, int b) { return (a < 0 || b < 0) ? std::string("null") : fmt("%.4g", median_of(tm[a].ms[0]) / median_of(tm[b].ms[1])); };
            std::fprintf(f, "%s\n      {\"median_target\": %.2f, \"occupation_target\": %.2f, \"without_n\": %s, \"with_n\": %s, \"cost_ratio_ms\": %s, \"without_n_ci\": %s, \"with_n_ci\": %s, \"cost_ratio_ms_ci\": %s}",
                         t ? "," : "", tm_, to_, nn(pt[0]).c_str(), nn(pt[1]).c_str(), rr(pt[0], pt[1]).c_str(), nn(ci[0]).c_str(), nn(ci[1]).c_str(), rr(ci[0], ci[1]).c_str());
            std::printf("  target med<=%.0f%% occ<=%.0f%%: WITHOUT n=%s (CI %s), WITH n=%s (CI %s)\n", 100 * tm_, 100 * to_, nn(pt[0]).c_str(), nn(ci[0]).c_str(), nn(pt[1]).c_str(), nn(ci[1]).c_str());
        }
        std::fprintf(f, "\n    ],\n");
    }
    std::fprintf(f, "    \"note\": \"WITHOUT is tuned at the reference grid, so it meets the target only at n = %d by construction (its durations scale as (n/%d)^(3/4)). The ratio grows like (n_ref/n_with)^(7/4) with the chosen reference resolution. A length clock re-tuned at every n (which needs either a reference run per n or the classical 7/4 exponent) would match WITH.\"\n  },\n", NREF, NREF);
    std::fprintf(f, "  \"stats_compute\": {\"threads\": %d, \"seconds\": %.1f, \"stats_scale\": %.3g, \"untimed\": true},\n", threads, A.secs, scale);
    std::fprintf(f, "  \"wall_seconds\": %.1f,\n  \"machine\": {\"cpu\": \"%s\", \"hw_threads\": %d},\n", (now_ms() - t_start) / 1000, cpu_name().c_str(), hw_threads());
    std::fprintf(f, "  \"notes\": \"Proved (Family 223, thm:main): c(1) n^(-7/4) N_n converges in law jointly with the curve and its occupation measure; no rate. Measured here: the WITH errors at every n, the finite-size drift of mean N/n^1.75 (%.4f at n=8, %.4f at n=32, %.4f at the n=8192 reference), costs and exponents. Both sides trace the same curve, so cost per curve is identical; the gain is purely that the coarse-grid statistics under the n^(-7/4) clock already match the fine grid, so the cheap grid can be used. The 7/4 exponent is classical (Beffara); the paper's new content is the proof that one constant with no slowly varying correction is right. Occupation shape (normalized) is clock-independent and identical for both sides. Errors include sampling noise (see with_noise_floor and CIs). Timing taken on a shared machine.\"\n}\n",
                 A.st[0].scaled, A.st[2].scaled, A.ref.scaled);
    std::fclose(f);
    std::printf("wrote %s (%.1f s)\n", path.c_str(), (now_ms() - t_start) / 1000);
    return 0;
}

// ------------------------------------------------------------------ video: lanes
static Color time_color(double t) {
    static const Color stops[5] = {rgb(0x56d4dd), rgb(0x6cb6ff), rgb(0xb392f0), rgb(0xff7eb6), rgb(0xffc861)};
    float u = float(std::clamp(t / 5.0, 0.0, 1.0)) * 4.f;
    int i = std::min(3, int(u));
    return lerp(stops[i], stops[i + 1], u - i);
}
static long long path_stride(int n) { return std::max<long long>(1, (long long)(3.1 * std::pow(double(n), 1.75) / 40000.0)); }

struct Lane {
    int S = 240;
    std::vector<float> acc, path;
    long long N = 0, stride = 1, drawn = 0, curves = 0;
    double dt = 0, t = 0, T = 0, hold = 0;
    bool done = false, has = false;
    float wline = 1;
    static constexpr double HOLD = 0.25;
    void init(int S_) { S = S_; acc.assign(size_t(S) * S * 3, 0.f); }
    void reset() { std::fill(acc.begin(), acc.end(), 0.f); has = false; curves = 0; }
    void set_curve(std::vector<float>& p, long long N_, long long stride_, double dt_) {
        path.swap(p);
        N = N_; stride = stride_; dt = dt_;
        T = double(N) * dt; t = 0; drawn = 0; done = false; hold = 0; has = true;
        double len = 0;
        for (size_t i = 2; i + 1 < path.size(); i += 2) len += std::hypot(path[i] - path[i - 2], path[i + 1] - path[i - 1]);
        len *= (S - 4);
        wline = float(std::clamp(0.32 * S * S / std::max(1.0, len), 0.06, 1.6));
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
                float* a = &acc[size_t(id[q]) * 3];
                a[0] += c.r * wt[q]; a[1] += c.g * wt[q]; a[2] += c.b * wt[q];
            }
        }
    }
    // advance by h seconds of clock time; returns true when a new curve is wanted
    bool advance(double h) {
        if (!has) return true;
        if (done) { hold += h; for (float& v : acc) v *= 0.88f; return hold >= HOLD; }
        t += h;
        long long P = (long long)(path.size() / 2);
        long long target = t >= T ? P - 1 : std::min<long long>(P - 1, (long long)std::floor(t / (dt * double(stride))));
        for (long long j = drawn; j < target; ++j)
            deposit(path[2 * j], path[2 * j + 1], path[2 * j + 2], path[2 * j + 3], time_color(std::min(T, double(j * stride) * dt)));
        drawn = std::max(drawn, target);
        if (t >= T) { done = true; curves++; }
        return false;
    }
    void tip(float& x, float& y) const {
        long long j = std::min<long long>(drawn, (long long)(path.size() / 2) - 1);
        x = path[2 * j]; y = path[2 * j + 1];
    }
    void draw(Canvas& p, float x, float y, std::vector<uint8_t>& img, Color frame) const {
        img.resize(size_t(S) * S * 3);
        for (size_t k = 0; k < img.size(); ++k) {
            float m = 1.f - std::exp(-acc[k] * 1.6f);
            img[k] = uint8_t(std::clamp(int((0.043f + m * 0.957f) * 255.f + 0.5f), 0, 255));
        }
        p.fill_rect(x - 2, y - 2, S + 4, S + 4, pal::bg);
        p.image(img.data(), S, S, x, y, S, S);
        p.stroke_rect(x - 1, y - 1, S + 2, S + 2, 1, frame, 0.5f);
        p.line(x, y + S, x, y, 3, pal::text, 0.3f);      // wired arc W (top + left)
        p.line(x, y, x + S, y, 3, pal::text, 0.3f);
        if (has && !done && !path.empty()) {
            float tx, ty;
            tip(tx, ty);
            float px = x + 2 + tx * (S - 4), py = y + 2 + (1 - ty) * (S - 4);
            p.glow(px, py, 20, time_color(t), 0.9f);
            p.circle(px, py, 3.f, rgb(0xffffff), 0.95f);
        }
        p.circle(x + 2, y + S - 2, 3.5f, pal::text, 0.7f);
        p.circle(x + S - 2, y + 2, 3.5f, pal::text, 0.7f);
    }
};

// ------------------------------------------------------------------ video: charts
struct LogAx {
    float x0, y0, x1, y1;
    double xa, xb, ya, yb;
    float X(double v) const { return x0 + float((std::log(std::max(v, 1e-30)) - std::log(xa)) / (std::log(xb) - std::log(xa))) * (x1 - x0); }
    float Y(double v) const {
        double t = (std::log(std::max(v, 1e-30)) - std::log(ya)) / (std::log(yb) - std::log(ya));
        return y1 - float(std::clamp(t, -0.02, 1.02)) * (y1 - y0);
    }
};

static void dashed_h(Canvas& c, float x0, float x1, float y, Color col, float a, float th = 1.5f) {
    for (float x = x0; x < x1; x += 12) c.line(x, y, std::min(x + 7, x1), y, th, col, a);
}

struct StageLive { std::vector<double> ms[2]; int skip[2] = {1, 1}; };

int main(int argc, char** argv) {
    if (arg_flag(argc, argv, "--sweep")) return run_sweep(argc, argv);

    const bool preview = arg_flag(argc, argv, "--preview");
    const double scale = arg_double(argc, argv, "--stats-scale", preview ? 0.05 : 1.0);
    const int boot = arg_int(argc, argv, "--boot", preview ? 50 : 400);
    const int threads = arg_int(argc, argv, "--threads", hw_threads());
    const int SF = arg_int(argc, argv, "--stage-frames", 210);
    const int from_stage = std::clamp(arg_int(argc, argv, "--from-stage", 0), 0, NSTAGE - 1);
    const int END_EXTRA = 110;

    AllStats A;
    compute_stats(A, std::vector<int>(STAGE_N, STAGE_N + NSTAGE), scale, boot, threads, arg_flag(argc, argv, "--quiet"));
    int first_meet[2] = {-1, -1};
    for (int sd = 0; sd < 2; ++sd)
        for (int s = 0; s < NSTAGE; ++s)
            if (first_meet[sd] < 0 && A.st[s].meets(sd)) first_meet[sd] = s;

    // reference lane paths (untimed, drawing only)
    const int NREFP = 3;
    std::vector<std::vector<float>> ref_paths(NREFP);
    std::vector<long long> ref_N(NREFP);
    for (int i = 0; i < NREFP; ++i) {
        int32_t c[NBB] = {};
        ref_N[i] = trace_one<true>(NREF, curve_key(SALT_REF, NREF, i), c, &ref_paths[i], path_stride(NREF));
    }

    Config cfg;
    cfg.name = "L3-fractal-ladder";
    cfg.title_left = fmt("length clock · dt ∝ δ, tuned at the %d² reference", NREF);
    cfg.title_right = "natural clock · dt = 1 s × δ^(7/4), one constant";
    cfg.caption = "Family 223 — Natural Occupation Measures for Critical Square-Lattice FK Interfaces · PROVED (thm:main): c·n^(−7/4)·N_n "
                  "converges with the curve + occupation, no rate · MEASURED: errors vs 8192² reference, finite-size drift, ms/curve";
    cfg.frames = (NSTAGE - from_stage) * SF + END_EXTRA;
    cfg.budget_ms = BUDGET;
    cfg.show_speedup = false;  // identical work; the ratio is shown per stage in the cost chart
    Harness h(argc, argv, cfg);

    ClockSide cs[2];
    cs[0].side = 0; cs[1].side = 1;
    Lane lane[2], ref_lane;
    for (auto& l : lane) l.init(240);
    ref_lane.init(240);
    int ref_next = 0;
    StageLive live[NSTAGE];
    int cur_stage = -1;
    std::vector<uint8_t> img;
    bool fresh[2] = {false, false};
    uint64_t fresh_key[2] = {0, 0};
    long long fresh_N[2] = {0, 0};

    for (int sd = 0; sd < 2; ++sd) {
        Panel& p = sd ? h.right() : h.left();
        p.set_compute_label("1 curve");
        p.sparkline("compute", true);
    }
    const Color side_col[2] = {pal::without, pal::with};

    // ---------------- overlay: stage banner + 3 charts across the bottom strip
    h.on_overlay([&](Canvas& c) {
        if (cur_stage < 0) return;
        const float top = 70 + 712, bot = 1020;
        c.fill_rect(0, top, 1920, bot - top, rgb(0x080a0f), 0.97f);
        c.fill_rect(0, top, 1920, 1, pal::grid);
        const StageStats& S = A.st[cur_stage];
        int fi = h.frame() - (cur_stage - from_stage) * SF;
        // banner
        c.text(16, top + 6, fmt("STAGE %d / %d", cur_stage + 1, NSTAGE), 17, pal::warn, Font::Bold);
        c.text(140, top + 6, fmt("grid n = %s  (%s cells) · ~%s lattice steps per curve · 1 curve per frame per side, 1 thread",
                                 commas(S.n).c_str(), commas((long long)S.n * S.n).c_str(), si(S.meanN).c_str()), 17, pal::text, Font::Sans);
        c.text(1904, top + 8, fmt("stats: %s curves per stage vs %s-curve %d² reference (untimed, precomputed, bootstrap 95%% CI)",
                                  commas(S.M).c_str(), commas(A.ref.M).c_str(), NREF), 14, pal::dim, Font::Sans, Align::Right);
        const float cy0 = top + 40, cy1 = bot - 30;
        // ---- chart 1: cost per curve vs n
        {
            LogAx ax{78, cy0 + 18, 520, cy1, 13, 10000, 0.0005, 2000};
            c.text(16, cy0 - 4, "compute per curve (= per frame) vs grid n · log-log", 14, pal::dim);
            for (double v : {0.001, 0.01, 0.1, 1.0, 10.0, 100.0, 1000.0}) {
                c.line(ax.x0, ax.Y(v), ax.x1, ax.Y(v), 1, pal::grid, 0.7f);
                c.text(ax.x0 - 6, ax.Y(v) - 8, v < 1 ? fmt("%g ms", v) : fmt("%.0f ms", v), 12, pal::dim, Font::Mono, Align::Right);
            }
            for (int s = 0; s < NSTAGE; ++s) {
                float x = ax.X(STAGE_N[s]);
                c.line(x, ax.y0, x, ax.y1, 1, pal::grid, 0.4f);
                c.text(x, ax.y1 + 4, nlab(STAGE_N[s]), 12, s == cur_stage ? pal::text : pal::dim, Font::Mono, Align::Center);
            }
            dashed_h(c, ax.x0, ax.x1, ax.Y(BUDGET), pal::warn, 0.85f);
            c.text(ax.x0 + 4, ax.Y(BUDGET) - 17, "16.7 ms budget (60 fps)", 12, pal::warn, Font::Mono);
            for (int sd = 0; sd < 2; ++sd) {
                std::vector<Vec2> pts;
                int crossed = -1;
                for (int s = from_stage; s <= cur_stage; ++s) {
                    if (live[s].ms[sd].empty()) continue;
                    double m = median_of(live[s].ms[sd]);
                    pts.push_back({ax.X(STAGE_N[s]), ax.Y(m)});
                    if (crossed < 0 && m > BUDGET) crossed = s;
                }
                if (pts.size() > 1) c.polyline(pts.data(), int(pts.size()), sd ? 1.8f : 4.f, side_col[sd], sd ? 1.f : 0.75f);
                for (auto& q : pts) c.circle(q.x, q.y, sd ? 3.f : 5.5f, side_col[sd]);
                if (crossed >= 0) {
                    float x = ax.X(STAGE_N[crossed]), y = ax.Y(median_of(live[crossed].ms[sd]));
                    c.ring(x, y, 10, 2, pal::warn);
                    if (sd == 0) c.text(x - 14, y - 10, fmt("over budget from n = %d", STAGE_N[crossed]), 13, pal::warn, Font::Bold, Align::Right);
                }
            }
            for (int s = from_stage; s <= cur_stage; ++s) {
                if (live[s].ms[0].empty() || live[s].ms[1].empty()) continue;
                double r = median_of(live[s].ms[0]) / median_of(live[s].ms[1]);
                c.text(ax.X(STAGE_N[s]), ax.y1 - 15, fmt("%.2f×", r), 11, pal::dim, Font::Mono, Align::Center);
            }
            c.text(ax.x0 + 8, ax.y0 - 4, "coral WITHOUT, teal WITH: same curve, same work", 12, pal::dim, Font::Sans);
            c.text(ax.x0 + 8, ax.y0 + 12, "(lines overlap) · WITHOUT/WITH ratio at the bottom", 12, pal::dim, Font::Sans);
        }
        // ---- charts 2 and 3: error vs cost
        for (int ch = 0; ch < 2; ++ch) {
            float X0 = ch == 0 ? 618.f : 1110.f;
            LogAx ax{X0, cy0 + 18, X0 + 410, cy1, 0.0007, 1500, 0.002, 2.0};
            c.text(X0 - 50, cy0 - 4, ch == 0 ? "median-duration error vs reference, by cost" : "occupation L1 error (8×8 bins) vs reference, by cost", 14, pal::dim);
            for (double v : {0.01, 0.1, 1.0}) {
                c.line(ax.x0, ax.Y(v), ax.x1, ax.Y(v), 1, pal::grid, 0.7f);
                c.text(ax.x0 - 6, ax.Y(v) - 8, fmt("%g%%", v * 100), 12, pal::dim, Font::Mono, Align::Right);
            }
            for (double v : {0.001, 0.01, 0.1, 1.0, 10.0, 100.0, 1000.0}) {
                c.line(ax.X(v), ax.y0, ax.X(v), ax.y1, 1, pal::grid, 0.4f);
                c.text(ax.X(v), ax.y1 + 4, v < 1 ? fmt("%g", v) : fmt("%.0f", v), 12, pal::dim, Font::Mono, Align::Center);
            }
            c.text(ax.x1 + 14, ax.y1 + 4, "ms", 12, pal::dim, Font::Mono, Align::Left);
            const double tgt = ch == 0 ? TARGET_MED : TARGET_OCC;
            c.fill_rect(ax.x0, ax.Y(tgt), ax.x1 - ax.x0, ax.y1 - ax.Y(tgt), pal::good, 0.05f);
            dashed_h(c, ax.x0, ax.x1, ax.Y(tgt), pal::good, 0.9f);
            c.text(ax.x1 - 4, ax.Y(tgt) - 17, fmt("target ≤ %.0f%%", tgt * 100), 12, pal::good, Font::Mono, Align::Right);
            for (float y = ax.y0; y < ax.y1; y += 10) c.line(ax.X(BUDGET), y, ax.X(BUDGET), std::min(y + 5, ax.y1), 1.2f, pal::warn, 0.7f);
            for (int sd = 0; sd < 2; ++sd) {
                std::vector<Vec2> pts;
                for (int s = from_stage; s <= cur_stage; ++s) {
                    if (live[s].ms[sd].size() < 3) continue;
                    const StageStats& st = A.st[s];
                    double m = median_of(live[s].ms[sd]);
                    double e = ch == 0 ? std::fabs(st.med_err[sd]) : st.occ_err[sd];
                    double lo = ch == 0 ? st.med_lo[sd] : st.occ_lo[sd], hi = ch == 0 ? st.med_hi[sd] : st.occ_hi[sd];
                    if (ch == 0) {  // CI of |signed error|
                        double a = std::fabs(lo), b = std::fabs(hi);
                        hi = std::max(a, b);
                        lo = (st.med_lo[sd] < 0 && st.med_hi[sd] > 0) ? 0.0 : std::min(a, b);
                    }
                    float x = ax.X(m), y = ax.Y(e);
                    c.line(x, ax.Y(std::max(lo, 0.0021)), x, ax.Y(hi), 2, side_col[sd], 0.6f);
                    pts.push_back({x, y});
                    c.circle(x, y, 4.5f, side_col[sd]);
                    if (s == first_meet[sd]) { c.ring(x, y, 9, 2, pal::good); c.glow(x, y, 16, pal::good, 0.5f); }
                    if (sd == 1 || s == cur_stage) c.text(x + (sd ? 7.f : -7.f), y + (sd ? 2.f : -18.f), nlab(STAGE_N[s]), 11, side_col[sd], Font::Mono, sd ? Align::Left : Align::Right);
                }
                if (pts.size() > 1) c.polyline(pts.data(), int(pts.size()), 1.6f, side_col[sd], 0.8f);
            }
        }
        // ---- callout column (cost to target)
        {
            const float x = 1580, w = 330;
            auto box = [&](float y, float hgt, Color col, bool on) {
                c.fill_rounded_rect(x, y, w, hgt, 7, rgb(0x05070b), 0.95f);
                c.stroke_rect(x, y, w, hgt, 1.5f, col, on ? 0.95f : 0.3f);
            };
            c.text(x, cy0 - 4, "target: median err ≤ 5%, occupation L1 ≤ 10%", 14, pal::dim);
            for (int k = 0; k < 2; ++k) {
                int sd = 1 - k;  // WITH first
                float y = cy0 + 20 + k * 54;
                int s = first_meet[sd];
                bool on = s >= 0 && s <= cur_stage && !live[s].ms[sd].empty();
                box(y, 48, side_col[sd], on);
                if (on) {
                    double m = median_of(live[s].ms[sd]);
                    c.text(x + 10, y + 4, fmt("%s first meets it at n = %d", sd ? "WITH" : "WITHOUT", STAGE_N[s]), 15, side_col[sd], Font::Bold);
                    c.text(x + 10, y + 25, fmt("%s/curve · %s", ms_str(m).c_str(), m <= BUDGET ? "fits a 60 fps frame" : fmt("%.1f× over budget", m / BUDGET).c_str()), 13, pal::text, Font::Mono);
                } else {
                    c.text(x + 10, y + 4, fmt("%s: not met at any n so far", sd ? "WITH" : "WITHOUT"), 15, demo::scale(side_col[sd], 0.7f), Font::Bold);
                    if (!sd) c.text(x + 10, y + 25, fmt("tuned at %d², drifts as (n/%d)^(3/4)", NREF, NREF), 13, pal::dim, Font::Mono);
                }
            }
            float y = cy0 + 128;
            bool on = first_meet[0] >= 0 && first_meet[1] >= 0 && cur_stage >= first_meet[0] && !live[first_meet[0]].ms[0].empty() &&
                      !live[first_meet[1]].ms[1].empty();
            box(y, 82, pal::warn, on);
            if (on) {
                double r = median_of(live[first_meet[0]].ms[0]) / median_of(live[first_meet[1]].ms[1]);
                double rs = A.st[first_meet[0]].meanN / A.st[first_meet[1]].meanN;
                c.text(x + 10, y + 3, fmt("cost to target: ×%s (steps ×%s)", commas((long long)std::llround(r)).c_str(), commas((long long)std::llround(rs)).c_str()), 15, pal::warn, Font::Bold);
                c.text(x + 10, y + 24, fmt("grows like (n_ref/n)^(7/4): set by n_ref = %d", NREF), 12, pal::text);
                c.text(x + 10, y + 41, "a length clock re-tuned per n (classical 7/4) ≈ WITH", 12, pal::dim);
                c.text(x + 10, y + 58, "measured, 1 thread; the theorem gives no finite-size rate", 12, pal::dim);
            } else {
                c.text(x + 10, y + 3, "cost ratio to target: pending", 15, demo::scale(pal::warn, 0.6f), Font::Bold);
                c.text(x + 10, y + 24, "shown once both clocks meet the target", 12, pal::dim);
            }
        }
        (void)fi;
    });

    while (h.next_frame()) {
        const int f = h.frame();
        const int s = std::min(NSTAGE - 1, from_stage + f / SF);
        const int fi = f - (s - from_stage) * SF;
        const int n = STAGE_N[s];
        if (s != cur_stage) {
            cur_stage = s;
            for (int sd = 0; sd < 2; ++sd) { lane[sd].reset(); fresh[sd] = false; }
        }
        // ---- timed work: one curve per frame per side (same seed both sides)
        const uint64_t key = curve_key(SALT_STAGE, n, fi);
        for (int k = 0; k < 2; ++k) {
            const int sd = (fi & 1) ? 1 - k : k;  // alternate which side runs first
            Panel& p = sd ? h.right() : h.left();
            StageLive& L = live[s];
            if (fi % L.skip[sd] != 0) continue;
            double a = now_ms();
            long long N = 0;
            p.measure([&] { N = cs[sd].step(n, key); });
            double ms = now_ms() - a;
            L.ms[sd].push_back(ms);
            // over-budget handling: decided on the stage's running median (>= 3 samples), not one cold frame
            if (L.ms[sd].size() >= 3) {
                double med = median_of(L.ms[sd]);
                L.skip[sd] = med > 250 ? std::clamp(int(std::ceil(med / 40)), 2, 60) : 1;
            }
            fresh[sd] = true; fresh_key[sd] = key; fresh_N[sd] = N;
        }
        // ---- lanes (untimed: re-trace with path recording for drawing)
        const double hstep = 1.0 / h.fps();
        for (int sd = 0; sd < 2; ++sd) {
            if (lane[sd].advance(hstep) && fresh[sd]) {
                std::vector<float> path;
                int32_t c[NBB] = {};
                long long st = path_stride(n);
                long long N = trace_one<true>(n, fresh_key[sd], c, &path, st);
                (void)fresh_N;
                lane[sd].set_curve(path, N, st, dt_of(sd, n));
                fresh[sd] = false;
            }
        }
        if (ref_lane.advance(hstep)) {
            std::vector<float> p = ref_paths[ref_next % NREFP];
            ref_lane.set_curve(p, ref_N[ref_next % NREFP], path_stride(NREF), dt_with(NREF));
            ref_next++;
        }

        // ---- draw panels
        const StageStats& S = A.st[s];
        float occ_vmax = 0;
        for (int k = 0; k < NBB; ++k) occ_vmax = std::max(occ_vmax, float(A.ref.occ[k]));
        occ_vmax *= 1.15f;
        for (int sd = 0; sd < 2; ++sd) {
            Panel& p = sd ? h.right() : h.left();
            const Color col = side_col[sd];
            p.clear(pal::bg);
            // ---- duration CDF vs reference
            {
                const float x0 = 520, x1 = 930, y0 = 52, y1 = 318;
                p.fill_rounded_rect(472, 12, 476, 378, 10, rgb(0x10131b), 0.92f);
                p.text(486, 20, "duration per curve · empirical CDF vs reference", 15, pal::dim);
                LogAx ax{x0, y0, x1, y1, 0.004, 25, 1, 2};
                for (double tk : {0.01, 0.1, 1.0, 10.0}) {
                    p.line(ax.X(tk), y0, ax.X(tk), y1, 1, pal::grid);
                    p.text(ax.X(tk), y1 + 6, tk < 1 ? fmt("%g s", tk) : fmt("%.0f s", tk), 13, pal::dim, Font::Mono, Align::Center);
                }
                auto Yq = [&](double q) { return y1 - float(q) * (y1 - y0); };
                for (double q : {0.25, 0.5, 0.75}) {
                    p.line(x0, Yq(q), x1, Yq(q), 1, pal::grid, q == 0.5 ? 1.f : 0.5f);
                    p.text(x0 - 6, Yq(q) - 8, fmt("%.2f", q), 12, pal::dim, Font::Mono, Align::Right);
                }
                auto curve = [&](const std::vector<float>& q, Color c, float th, float a) {
                    std::vector<Vec2> pts;
                    for (int k = 0; k <= 200; ++k) pts.push_back({ax.X(q[k]), Yq(k / 200.0)});
                    p.polyline(pts.data(), int(pts.size()), th, c, a);
                };
                for (int ps = from_stage; ps < s; ++ps) {
                    curve(A.st[ps].q[sd], col, 1.2f, 0.35f);
                    if (!sd) p.text(ax.X(A.st[ps].med_dur[sd]) - 3, Yq(0.5) - 15 - 12 * (ps % 2), nlab(STAGE_N[ps]), 10, col, Font::Mono, Align::Right);
                }
                curve(A.ref.q, pal::text, 2.5f, 0.85f);
                float a_in = std::min(1.f, (fi + 1) / 20.f);
                p.blend = Blend::Add;
                curve(S.q[sd], col, 6.f, 0.2f * a_in);
                p.blend = Blend::Normal;
                curve(S.q[sd], col, 2.6f, a_in);
                float xm = ax.X(S.med_dur[sd]), xr = ax.X(A.ref.med_dur);
                p.circle(xm, Yq(0.5), 5, col);
                p.circle(xr, Yq(0.5), 4, pal::text);
                if (std::fabs(xm - xr) > 14) {
                    p.line(xm, Yq(0.5) + 14, xr, Yq(0.5) + 14, 2, col, 0.9f);
                    p.circle(xr, Yq(0.5) + 14, 3, col);
                }
                p.text(486, y1 + 28, fmt("n = %d: median %.3g s", n, S.med_dur[sd]), 15, col, Font::Mono);
                p.text(486, y1 + 48, fmt("%d² reference: median %.3g s", NREF, A.ref.med_dur), 15, pal::text, Font::Mono);
                p.text(934, y1 + 28, fmt("error %+.1f%%", 100 * S.med_err[sd]), 16, S.meets(sd) ? pal::good : pal::bad, Font::Bold, Align::Right);
                p.text(934, y1 + 48, fmt("KS %.3f", S.ks[sd]), 13, pal::dim, Font::Mono, Align::Right);
            }
            // ---- lanes
            const float ly = 420;
            lane[sd].draw(p, 22, ly, img, col);
            ref_lane.draw(p, 282, ly, img, pal::text);
            p.text(22, ly - 24, fmt("n = %d · this side's clock", n), 15, col, Font::Bold);
            p.text(282, ly - 24, fmt("%d² reference (both panels)", NREF), 15, pal::dim, Font::Bold);
            auto bar = [&](const Lane& L, float x, Color c, bool cnt) {
                float by = ly + 248;
                double frac = L.T > 0 ? std::min(1.0, L.t / L.T) : 0;
                p.fill_rounded_rect(x, by, 240, 7, 3.5f, pal::grid);
                p.fill_rounded_rect(x, by, std::max(7.f, float(240 * frac)), 7, 3.5f, time_color(L.t), 0.95f);
                p.text(x, by + 11, fmt("t = %.2f s of %.2f s", std::min(L.t, L.T), L.T), 14, pal::text, Font::Mono);
                if (cnt) p.text(x + 240, by + 11, fmt("#%lld", L.curves + (L.done ? 0 : 1)), 13, c, Font::Mono, Align::Right);
            };
            bar(lane[sd], 22, col, true);
            bar(ref_lane, 282, pal::text, true);
            if (live[s].skip[sd] > 1) {
                double m = median_of(live[s].ms[sd]);
                TextStyle ts; ts.size = 14; ts.color = pal::warn; ts.backdrop = true; ts.font = Font::Bold;
                p.text(26, ly + 6, fmt("over budget: %s/frame — updated every %d%s frame", ms_str(m).c_str(), live[s].skip[sd],
                                       live[s].skip[sd] == 2 ? "nd" : (live[s].skip[sd] == 3 ? "rd" : "th")), ts);
            }
            // ---- occupation maps (mean time per 8x8 bin)
            {
                const float ox = 552, oy = 420, sz = 156, gap = 38;
                p.text(ox, oy - 24, "mean time per screen bin (8×8)", 15, pal::dim, Font::Bold);
                auto map = [&](const double* o, float x, const char* lab, Color c) {
                    float fld[NBB];
                    for (int j = 0; j < NB; ++j)
                        for (int i = 0; i < NB; ++i) fld[j * NB + i] = float(o[(NB - 1 - j) * NB + i]);
                    p.field(fld, NB, NB, x, oy, sz, sz, Cmap::Inferno, 0, occ_vmax, false);
                    p.stroke_rect(x, oy, sz, sz, 1, c, 0.6f);
                    double tot = 0;
                    for (int k = 0; k < NBB; ++k) tot += o[k];
                    p.text(x, oy + sz + 6, lab, 13, c, Font::Sans);
                    p.text(x + sz, oy + sz + 6, fmt("Σ %.2f s", tot), 13, c, Font::Mono, Align::Right);
                };
                map(S.occ_mean[sd], ox, fmt("n = %d", n).c_str(), col);
                map(A.ref.occ, ox + sz + gap, fmt("%d² ref", NREF).c_str(), pal::text);
                {
                    float g[64];
                    for (int k = 0; k < 64; ++k) g[k] = occ_vmax * k / 63.f;
                    p.field(g, 64, 1, ox, oy + sz + 28, 2 * sz + gap, 7, Cmap::Inferno, 0, occ_vmax, true);
                }
                p.text(ox, oy + sz + 38, "0 s", 12, pal::dim, Font::Mono);
                p.text(ox + 2 * sz + gap, oy + sz + 38, fmt("%.3f s per bin · shared scale", occ_vmax), 12, pal::dim, Font::Mono, Align::Right);
                p.text(ox, oy + sz + 60, fmt("L1 error %.1f%%", 100 * S.occ_err[sd]), 17, S.occ_err[sd] <= TARGET_OCC ? pal::good : pal::bad, Font::Bold);
                p.text(ox + 2 * sz + gap, oy + sz + 63, fmt("shape only %.1f%% (same both)", 100 * S.shape), 13, pal::dim, Font::Mono, Align::Right);
            }
            // ---- HUD metrics
            p.metric_text("grid · lattice steps per curve", fmt("%d² · %s", n, si(S.meanN).c_str()));
            p.metric_text("median duration (ref)", fmt("%.3g s (%.3g s)", S.med_dur[sd], A.ref.med_dur), S.meets(sd) ? Tone::Good : Tone::Bad);
            p.metric("median-duration error", 100 * std::fabs(S.med_err[sd]), "%.1f %%", std::fabs(S.med_err[sd]) <= TARGET_MED ? Tone::Good : Tone::Bad);
            p.metric("occupation L1 error", 100 * S.occ_err[sd], "%.1f %%", S.occ_err[sd] <= TARGET_OCC ? Tone::Good : Tone::Bad);
            p.metric_text("dt per traversal", sd ? fmt("δ^(7/4) = %.2e s", dt_with(n)) : fmt("%d^(-3/4)·δ = %.2e s", NREF, dt_without(n)), Tone::Accent);
            std::string tgt = first_meet[sd] >= 0 && first_meet[sd] <= s ? fmt("met from n = %d", STAGE_N[first_meet[sd]]) : std::string("not met yet");
            p.metric_text("target ≤5% median, ≤10% L1", tgt, first_meet[sd] >= 0 && first_meet[sd] <= s ? Tone::Good : Tone::Warn);
        }
    }

    // results extras
    for (int s = from_stage; s < NSTAGE; ++s)
        for (int sd = 0; sd < 2; ++sd) {
            const char* nm = sd ? "with" : "without";
            h.result(fmt("n%d_%s_median_ms_per_curve", STAGE_N[s], nm), median_of(live[s].ms[sd]));
            h.result(fmt("n%d_%s_frames", STAGE_N[s], nm), double(live[s].ms[sd].size()));
            h.result(fmt("n%d_%s_median_err", STAGE_N[s], nm), A.st[s].med_err[sd]);
            h.result(fmt("n%d_%s_occ_L1", STAGE_N[s], nm), A.st[s].occ_err[sd]);
        }
    for (int s = 0; s < NSTAGE; ++s) h.result(fmt("n%d_mean_N_over_n^1.75", STAGE_N[s]), A.st[s].scaled);
    h.result("reference_n", NREF);
    h.result("reference_curves", A.ref.M);
    h.result("first_meets_target_without_n", first_meet[0] >= 0 ? STAGE_N[first_meet[0]] : -1);
    h.result("first_meets_target_with_n", first_meet[1] >= 0 ? STAGE_N[first_meet[1]] : -1);
    h.result("stats_scale", scale);
    h.result("stats_seconds_untimed", A.secs);
    h.result("note", "HUD times one curve per frame per side on 1 thread (identical curve, identical work). Error statistics are precomputed untimed with all threads, same seeds as --sweep.");
    return h.finish();
}
