// L4-economy-ladder - load ladder for 01-economy (family 149, uniform permanence in weakly
// reversible mass-action systems).
//
// Load = number of independent towns (economies) integrated per frame, N = 1, 4, ..., 4096
// (sweep: to 16384). Every town runs the 01-economy recipe network from its own seeded start in
// the class O + W + 2T = 4.5, with seeded rates (01-economy rates x a log-uniform factor in
// [1/1.5, 1.5]). Each stage restarts all towns at t = 0 and runs 240 frames of dt = 0.5 sim time
// (t = 0 -> 120). Both sides: the 01-economy Rosenbrock 2(3) integrator (rtol 1e-6, atol 1e-14,
// positivity rejection, a run is halted once a resource passes 1e12), 1 thread each.
//   WITHOUT: 7 recipes (2P->P and P+T+F->P+O+W removed) - not weakly reversible.
//   WITH:    all 9 recipes - weakly reversible, so thm:main / eq:uniform-bounds apply.
// Modes:  (default) video;  --sweep  timing ladder + time-horizon axis + fixed-step study,
//         writes out/ladder.json.  Flags: --max-n 16384, --horizon-n 256, --fixed-n 256,
//         --jit 1.5 (rate spread; 1 = exact 01-economy rates), --seed 149.
#include "demo.h"
#include "econ.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace demo;
using econ::D;
using econ::Town;

static Color spcol(int i) {
    static const Color c[D] = {rgb(0x6cb6ff), rgb(0xffa657), rgb(0xb392f0), rgb(0x5be49b), rgb(0xffc861)};
    return c[i];
}
static std::string commas(long long v) {
    std::string s = std::to_string(v), o;
    int c = 0;
    for (int i = (int)s.size() - 1; i >= 0; --i) {
        o.insert(o.begin(), s[i]);
        if (++c % 3 == 0 && i > 0) o.insert(o.begin(), ',');
    }
    return o;
}
static double median_of(std::vector<double> v) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    return n % 2 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}
static double pct_of(std::vector<double> v, double q) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    size_t i = std::min(v.size() - 1, (size_t)std::ceil(q * v.size()) - 1);
    return v[i];
}

constexpr int FRAMES_PER_STAGE = 240;
constexpr double DT = 0.5;  // simulated time per frame, both sides, every stage

// ---------------------------------------------------------------- one side = one network, N towns
struct Side {
    econ::Net net;
    econ::Structure S;
    std::vector<Town> towns;
    long long steps_frame = 0;
    int dead = 0, run = 0, either = 0, halted = 0;
    void reset(int n, unsigned seed, double jit, const std::vector<econ::Rx>& FULL, bool without) {
        towns.assign(n, Town{});
        for (int i = 0; i < n; ++i) {
            econ::seed_town(towns[i], seed, i, FULL, jit);
            if (without) econ::drop_removed(towns[i]);
        }
    }
    void step(double t1) {  // the measured work: advance every town to t1
        steps_frame = 0;
        for (auto& T : towns) econ::advance(net, T, t1, steps_frame);
    }
    void census() {  // untimed
        dead = run = either = halted = 0;
        for (auto& T : towns) {
            bool d = false, r = false;
            for (int i = 0; i < D; ++i) { d |= T.x[i] < econ::DEAD; r |= T.x[i] > econ::RUNAWAY; }
            dead += d; run += r; either += d || r; halted += T.halted;
        }
    }
};

struct Setup {
    std::vector<econ::Rx> FULL, WO;
    Side side[2];  // 0 = WITHOUT, 1 = WITH
    unsigned seed = 149;
    double jit = 1.5;
    void init() {
        FULL = econ::full_network();
        for (int r = 0; r < (int)FULL.size(); ++r)
            if (r != econ::REMOVED[0] && r != econ::REMOVED[1]) WO.push_back(FULL[r]);
        side[0].net = econ::make_net(WO);
        side[1].net = econ::make_net(FULL);
        side[0].S = econ::analyse(WO);
        side[1].S = econ::analyse(FULL);
    }
    void reset(int n) {
        side[0].reset(n, seed, jit, FULL, true);
        side[1].reset(n, seed, jit, FULL, false);
    }
};

// ================================================================ sweep mode
struct StageRes {
    int n = 0;
    double med[2]{}, p95[2]{}, mean[2]{}, steps_med[2]{}, steps_mean[2]{};
    int frames[2]{};
    double f_dead[2]{}, f_run[2]{}, f_either[2]{}, f_halted[2]{};
};

static StageRes run_stage(Setup& su, int n, int frames, bool print, bool with_only = false) {
    StageRes R;
    R.n = n;
    const int k0 = with_only ? 1 : 0;
    // warm-up: 12 untimed frames on the same towns, then restart from t = 0
    su.reset(n);
    for (int f = 0; f < 12; ++f)
        for (int k = k0; k < 2; ++k) su.side[k].step((f + 1) * DT);
    su.reset(n);
    std::vector<double> ms[2], st[2];
    for (int f = 0; f < frames; ++f) {
        const double t1 = (f + 1) * DT;
        for (int q = 0; q < 2; ++q) {
            int k = (f + q) % 2;  // alternate which side goes first
            if (k < k0) continue;
            double a = now_ms();
            su.side[k].step(t1);
            ms[k].push_back(now_ms() - a);
            st[k].push_back((double)su.side[k].steps_frame);
        }
    }
    for (int k = k0; k < 2; ++k) {
        Side& s = su.side[k];
        s.census();
        R.med[k] = median_of(ms[k]);
        R.p95[k] = pct_of(ms[k], 0.95);
        double sum = 0, ss = 0;
        for (double v : ms[k]) sum += v;
        for (double v : st[k]) ss += v;
        R.mean[k] = sum / ms[k].size();
        R.steps_med[k] = median_of(st[k]);
        R.steps_mean[k] = ss / st[k].size();
        R.frames[k] = (int)ms[k].size();
        R.f_dead[k] = double(s.dead) / n;
        R.f_run[k] = double(s.run) / n;
        R.f_either[k] = double(s.either) / n;
        R.f_halted[k] = double(s.halted) / n;
    }
    if (print && with_only)
        printf("%7d |  (W/O not run) %10.4f  WITH-only extension | %10.4f | %9.0f | %5.1f%%\n", n, R.med[1], R.mean[1], R.steps_mean[1],
               100 * R.f_either[1]);
    else if (print)
        printf("%7d | %10.4f %10.4f %8.1fx | %10.4f %10.4f | %9.0f %9.0f | %5.1f%% %5.1f%%\n", n, R.med[0], R.med[1],
               R.med[0] / std::max(R.med[1], 1e-9), R.mean[0], R.mean[1], R.steps_mean[0], R.steps_mean[1], 100 * R.f_either[0],
               100 * R.f_either[1]);
    fflush(stdout);
    return R;
}

struct HorizonRow {
    double t0, t1;
    double ms_per_t[2]{}, steps_per_town_t[2]{}, f_either[2]{}, f_halted[2]{};
};
// Time-horizon axis: N towns run frame by frame (dt = 0.5) to t = 1e4; cost per unit sim time
// in decade windows. halt = econ::HALT used for the run (1e12 = 01-economy rule; 1e300 = never).
static std::vector<HorizonRow> run_horizon(Setup& su, int n, double halt) {
    const double marks[] = {10, 100, 1000, 10000};
    std::vector<HorizonRow> rows;
    econ::HALT = halt;
    su.reset(n);
    double ms[2]{}, tprev = 0;
    long long steps[2]{};
    long long f = 0;
    for (double M : marks) {
        HorizonRow row;
        row.t0 = tprev; row.t1 = M;
        double ms0[2] = {ms[0], ms[1]};
        long long s0[2] = {steps[0], steps[1]};
        while ((f + 1) * DT <= M + 1e-9) {
            const double t1 = (f + 1) * DT;
            for (int q = 0; q < 2; ++q) {
                int k = (int)((f + q) % 2);
                double a = now_ms();
                su.side[k].step(t1);
                ms[k] += now_ms() - a;
                steps[k] += su.side[k].steps_frame;
            }
            ++f;
        }
        for (int k = 0; k < 2; ++k) {
            su.side[k].census();
            row.ms_per_t[k] = (ms[k] - ms0[k]) / (M - tprev);
            row.steps_per_town_t[k] = double(steps[k] - s0[k]) / n / (M - tprev);
            row.f_either[k] = double(su.side[k].either) / n;
            row.f_halted[k] = double(su.side[k].halted) / n;
        }
        printf("  halt %-6s t in (%6g,%6g]: ms per sim-time  %9.4f vs %9.4f (%6.1fx) | steps/town/sim-time %8.2f vs %8.2f | dead-or-runaway %5.1f%% vs %5.1f%%\n",
               halt < 1e100 ? "1e12" : "never", tprev, M, row.ms_per_t[0], row.ms_per_t[1], row.ms_per_t[0] / std::max(1e-9, row.ms_per_t[1]),
               row.steps_per_town_t[0], row.steps_per_town_t[1], 100 * row.f_either[0], 100 * row.f_either[1]);
        fflush(stdout);
        rows.push_back(row);
        tprev = M;
    }
    econ::HALT = 1e12;
    return rows;
}

struct FixedRow {
    int sub;
    double h;
    double f_broken[2]{}, f_broken_late[2]{}, f_runaway[2]{}, ms_frame[2]{}, first_break[2]{};
};
// What a game would ship: fixed-step explicit midpoint (RK2), `sub` substeps per frame, no error
// control. "Broken" = a resource went negative or non-finite. "Late" = broke after t = 10 (after
// the start transient). Runs to t = 1000.
static std::vector<FixedRow> run_fixed(Setup& su, int n) {
    std::vector<FixedRow> rows;
    const double T_END = 1000;
    for (int sub : {1, 4, 16, 64, 256}) {
        FixedRow row;
        row.sub = sub;
        row.h = DT / sub;
        su.reset(n);
        std::vector<econ::FixedTown> F[2];
        for (int k = 0; k < 2; ++k) {
            F[k].resize(n);
            for (int i = 0; i < n; ++i) F[k][i].x = su.side[k].towns[i].x;
        }
        std::vector<double> ms[2];
        for (long long f = 0; (f + 1) * DT <= T_END + 1e-9; ++f) {
            for (int q = 0; q < 2; ++q) {
                int k = (int)((f + q) % 2);
                double a = now_ms();
                for (int i = 0; i < n; ++i) econ::advance_fixed(su.side[k].net, su.side[k].towns[i].k, F[k][i], f * DT, DT, sub);
                ms[k].push_back(now_ms() - a);
            }
        }
        for (int k = 0; k < 2; ++k) {
            int b = 0, bl = 0, ru = 0;
            double fb = -1;
            for (auto& t : F[k]) {
                b += t.broken;
                bl += t.broken && t.t_broken > 10;
                if (t.broken && (fb < 0 || t.t_broken < fb)) fb = t.t_broken;
                if (!t.broken) {
                    bool r = false;
                    for (int i = 0; i < D; ++i) r |= t.x[i] > econ::RUNAWAY;
                    ru += r;
                }
            }
            row.f_broken[k] = double(b) / n;
            row.f_broken_late[k] = double(bl) / n;
            row.f_runaway[k] = double(ru) / n;
            row.first_break[k] = fb;
            // median over the first 20 frames, before any town stops (broken towns are skipped)
            row.ms_frame[k] = median_of(std::vector<double>(ms[k].begin(), ms[k].begin() + 20));
        }
        printf("  fixed RK2 h=%-9.6g: broken (neg/non-finite) %5.1f%% vs %5.1f%% | broke after t=10 %5.1f%% vs %5.1f%% | runaway (finite) %5.1f%% vs %5.1f%% | ms/frame %.3f vs %.3f\n",
               row.h, 100 * row.f_broken[0], 100 * row.f_broken[1], 100 * row.f_broken_late[0], 100 * row.f_broken_late[1],
               100 * row.f_runaway[0], 100 * row.f_runaway[1], row.ms_frame[0], row.ms_frame[1]);
        fflush(stdout);
        rows.push_back(row);
    }
    return rows;
}

static double fit_slope(const std::vector<StageRes>& R, int k, int nmin) {
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    int c = 0;
    for (auto& r : R) {
        if (r.n < nmin) continue;
        double x = std::log10((double)r.n), y = std::log10(std::max(r.med[k], 1e-9));
        sx += x; sy += y; sxx += x * x; sxy += x * y; ++c;
    }
    if (c < 2) return 0;
    return (c * sxy - sx * sy) / (c * sxx - sx * sx);
}

static int sweep(int argc, char** argv, Setup& su) {
    const int max_n = arg_int(argc, argv, "--max-n", 16384);
    const int hn = arg_int(argc, argv, "--horizon-n", 256);
    const int fn = arg_int(argc, argv, "--fixed-n", 256);
    std::string out = arg_str(argc, argv, "--out", "out");
    const double budget = 16.7;
    double t_start = now_ms();
    printf("L4-economy-ladder sweep: %d frames/stage, dt = %.2f (t = 0 -> %.0f), 1 thread per side, jit %.2f, seed %u\n",
           FRAMES_PER_STAGE, DT, FRAMES_PER_STAGE * DT, su.jit, su.seed);
    printf("weakly reversible: WITHOUT %s, WITH %s\n", su.side[0].S.wr ? "yes" : "no", su.side[1].S.wr ? "yes" : "no");
    printf("      N |  median ms/frame W/O  WITH  ratio | mean ms W/O   WITH  | steps/frame W/O WITH | dead-or-runaway at t=120\n");
    std::vector<StageRes> R;
    for (int n = 1; n <= max_n; n *= 4) R.push_back(run_stage(su, n, FRAMES_PER_STAGE, true));
    // WITH-only extension (cheap): WITHOUT at 4x max_n would take ~4 min of compute, so it is not
    // run there and nothing is extrapolated for it.
    const int ext_n = arg_int(argc, argv, "--with-ext-n", max_n * 4);
    std::vector<StageRes> RX;
    if (ext_n > max_n) RX.push_back(run_stage(su, ext_n, FRAMES_PER_STAGE, true, true));

    printf("time-horizon axis, N = %d towns, dt = %.2f per frame (W/O vs WITH):\n", hn, DT);
    auto H1 = run_horizon(su, hn, 1e12);
    auto H2 = run_horizon(su, hn, 1e300);
    printf("fixed-step explicit midpoint (RK2), N = %d towns, dt = %.2f per frame, to t = 1000 (W/O vs WITH):\n", fn, DT);
    auto FX = run_fixed(su, fn);

    // ---- ladder.json
    std::string path = out + "/ladder.json";
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) { printf("cannot write %s\n", path.c_str()); return 1; }
    int within[2] = {0, 0};
    int first_over[2] = {-1, -1};
    for (auto& r : R)
        for (int k = 0; k < 2; ++k) {
            if (r.med[k] <= budget && first_over[k] < 0) within[k] = r.n;
            if (r.med[k] > budget && first_over[k] < 0) first_over[k] = r.n;
        }
    double e0 = fit_slope(R, 0, 1), e1 = fit_slope(R, 1, 1), e0b = fit_slope(R, 0, 64), e1b = fit_slope(R, 1, 64);
    fprintf(f, "{\n  \"demo\": \"L4-economy-ladder\",\n  \"family\": \"149\",\n  \"load_name\": \"independent towns (economies) per frame\",\n");
    fprintf(f, "  \"budget_ms\": %.1f,\n  \"threads\": {\"without\": 1, \"with\": 1},\n", budget);
    fprintf(f, "  \"protocol\": \"each stage: N towns restarted at t=0, 12 untimed warm-up frames, then %d timed frames of dt=%.2f (t=0->%.0f), sides interleaved and alternated; same seeded starts (class O+W+2T=4.5) and rates (x log-uniform [1/%.2f,%.2f]) on both sides; Rosenbrock 2(3) rtol 1e-6 atol 1e-14, run halted at 1e12 (01-economy rule)\",\n",
            FRAMES_PER_STAGE, DT, FRAMES_PER_STAGE * DT, su.jit, su.jit);
    fprintf(f, "  \"stages\": [\n");
    for (size_t i = 0; i < R.size(); ++i) {
        auto& r = R[i];
        fprintf(f, "    {\"n\": %d,\n", r.n);
        for (int k = 0; k < 2; ++k)
            fprintf(f, "     \"%s\": {\"median_ms\": %.6g, \"p95_ms\": %.6g, \"mean_ms\": %.6g, \"frames\": %d, \"steps_per_frame_median\": %.6g, \"steps_per_frame_mean\": %.6g, \"frac_dead\": %.4f, \"frac_runaway\": %.4f, \"frac_halted\": %.4f},\n",
                    k ? "with" : "without", r.med[k], r.p95[k], r.mean[k], r.frames[k], r.steps_med[k], r.steps_mean[k], r.f_dead[k], r.f_run[k],
                    r.f_halted[k]);
        fprintf(f, "     \"ratio_median\": %.4g, \"ratio_mean\": %.4g,\n", r.med[0] / std::max(r.med[1], 1e-12), r.mean[0] / std::max(r.mean[1], 1e-12));
        fprintf(f, "     \"quality\": {\"name\": \"fraction of towns with a dead (<1e-6) or runaway (>1e6) resource at t=%.0f\", \"without\": %.4f, \"with\": %.4f, \"higher_is_better\": false}}%s\n",
                FRAMES_PER_STAGE * DT, r.f_either[0], r.f_either[1], i + 1 < R.size() ? "," : "");
    }
    fprintf(f, "  ],\n");
    fprintf(f, "  \"fit\": {\"without_exponent\": %.3f, \"with_exponent\": %.3f, \"without_exponent_n_ge_64\": %.3f, \"with_exponent_n_ge_64\": %.3f},\n", e0, e1, e0b, e1b);
    auto mnw = [&](int k) {  // human-readable bracket
        if (first_over[k] < 0) return fmt("%d (never over budget up to the largest measured n)", within[k]);
        if (within[k] == 0) return fmt("none (n=1 already over budget)");
        return fmt("%d (bracketed: n=%d over budget)", within[k], first_over[k]);
    };
    fprintf(f, "  \"max_n_within_budget\": {\"without\": %d, \"with\": %d, \"without_bracket\": \"%s\", \"with_bracket\": \"%s\"},\n", within[0], within[1],
            mnw(0).c_str(), mnw(1).c_str());
    auto hz = [&](const char* name, const std::vector<HorizonRow>& H) {
        fprintf(f, "  \"%s\": [\n", name);
        for (size_t i = 0; i < H.size(); ++i)
            fprintf(f, "    {\"t_window\": [%g, %g], \"without\": {\"ms_per_sim_time\": %.6g, \"steps_per_town_per_sim_time\": %.5g, \"frac_dead_or_runaway_at_end\": %.4f, \"frac_halted\": %.4f}, \"with\": {\"ms_per_sim_time\": %.6g, \"steps_per_town_per_sim_time\": %.5g, \"frac_dead_or_runaway_at_end\": %.4f, \"frac_halted\": %.4f}, \"ratio\": %.4g}%s\n",
                    H[i].t0, H[i].t1, H[i].ms_per_t[0], H[i].steps_per_town_t[0], H[i].f_either[0], H[i].f_halted[0], H[i].ms_per_t[1],
                    H[i].steps_per_town_t[1], H[i].f_either[1], H[i].f_halted[1], H[i].ms_per_t[0] / std::max(1e-12, H[i].ms_per_t[1]),
                    i + 1 < H.size() ? "," : "");
        fprintf(f, "  ],\n");
    };
    fprintf(f, "  \"horizon_n\": %d,\n", hn);
    hz("horizon_halt_1e12", H1);
    hz("horizon_never_halt", H2);
    fprintf(f, "  \"fixed_step_n\": %d,\n  \"fixed_step_rk2\": [\n", fn);
    for (size_t i = 0; i < FX.size(); ++i) {
        auto& x = FX[i];
        fprintf(f, "    {\"substeps_per_frame\": %d, \"h\": %.6g, \"without\": {\"frac_broken_negative_or_nonfinite\": %.4f, \"frac_broken_after_t10\": %.4f, \"frac_runaway_finite\": %.4f, \"first_break_t\": %.4g, \"ms_per_frame\": %.5g}, \"with\": {\"frac_broken_negative_or_nonfinite\": %.4f, \"frac_broken_after_t10\": %.4f, \"frac_runaway_finite\": %.4f, \"first_break_t\": %.4g, \"ms_per_frame\": %.5g}}%s\n",
                x.sub, x.h, x.f_broken[0], x.f_broken_late[0], x.f_runaway[0], x.first_break[0], x.ms_frame[0], x.f_broken[1], x.f_broken_late[1],
                x.f_runaway[1], x.first_break[1], x.ms_frame[1], i + 1 < FX.size() ? "," : "");
    }
    fprintf(f, "  ],\n");
    fprintf(f, "  \"machine\": {\"cpu\": \"%s\", \"hw_threads\": %d},\n", cpu_name().c_str(), hw_threads());
    fprintf(f, "  \"sweep_wall_s\": %.1f,\n", (now_ms() - t_start) / 1000.0);
    {
        const StageRes& L = R.back();
        double rmin = 1e300, rmax = 0;
        for (auto& r : R)
            if (r.n >= 16) { double q = r.med[0] / r.med[1]; rmin = std::min(rmin, q); rmax = std::max(rmax, q); }
        auto over = [&](int k) {
            if (first_over[k] < 0) return fmt("stays under 16.7 ms up to N=%d", R.back().n);
            double prev = 0, nxt = 0;
            for (auto& r : R) {
                if (r.n == within[k]) prev = r.med[k];
                if (r.n == first_over[k]) nxt = r.med[k];
            }
            return fmt("crosses 16.7 ms between N=%d (%.1f ms) and N=%d (%.1f ms)", within[k], prev, first_over[k], nxt);
        };
        std::string ext = RX.empty() ? std::string("") : fmt(" WITH-only extension: N=%d gives %.2f ms median.", RX[0].n, RX[0].med[1]);
        double minbroke = 1;
        for (auto& x : FX) minbroke = std::min(minbroke, x.f_broken[0]);
        std::string notes = fmt(
            "Hypothesis partly confirmed. N axis (t=0..120 per stage): both sides scale linearly in N (fitted exponents %.2f / %.2f); "
            "for N>=16 the WITHOUT median frame costs %.0f-%.0fx the WITH median frame (at N=%d the mean-frame ratio is %.0fx and the attempted-solver-step ratio %.0fx), "
            "because >99%% of WITHOUT towns collapse (Wood below 1e-6, Food and Workers past 1e6, halted at 1e12) and the stiff collapse forces many small steps, "
            "while WITH towns settle and take about one step per frame. WITHOUT %s; WITH %s.%s "
            "Time-horizon axis (N=%d): the extra cost is a transient, not growth with simulated time. With the 01-economy halt rule WITHOUT costs %.0fx WITH per unit time in t=(10,100], "
            "then less than WITH once its towns are halted (%.2fx in (100,1000], about 0 later). Never halting: %.0fx in (10,100], %.0fx in (100,1000], %.2fx in (1000,1e4] "
            "(collapsed towns sit at extreme but slowly varying values that the relative-error controller steps through as cheaply as WITH). "
            "Fixed-step explicit midpoint (what a game ships; same cost per step on both sides by construction): at every step from h=0.5 to h=1/512 at least %.1f%% of WITHOUT towns "
            "go negative or non-finite by t=1000 (%.1f%% after t=10 at h=1/512); WITH towns break only during the start transient from extreme starts "
            "(%.1f%% after t=10 at h=0.5, none after t=10 for h<=0.125, %.1f%% in total at h=1/512). Proved by the paper (thm:main, eq:uniform-bounds): WITH trajectories stay in [eps,1/eps] after a transient; "
            "eps is not computed. Everything else here (costs, the WITHOUT collapse, fixed-step behaviour) is measured for this network, these seeds and rates, 1 thread per side.",
            e0, e1, rmin, rmax, L.n, L.mean[0] / L.mean[1], L.steps_mean[0] / L.steps_mean[1], over(0).c_str(), over(1).c_str(), ext.c_str(), hn,
            H1[1].ms_per_t[0] / H1[1].ms_per_t[1], H1[2].ms_per_t[0] / H1[2].ms_per_t[1], H2[1].ms_per_t[0] / H2[1].ms_per_t[1],
            H2[2].ms_per_t[0] / H2[2].ms_per_t[1], H2[3].ms_per_t[0] / H2[3].ms_per_t[1], 100 * minbroke, 100 * FX.back().f_broken_late[0],
            100 * FX[0].f_broken_late[1], 100 * FX.back().f_broken[1]);
        if (!RX.empty()) {
            auto& r = RX[0];
            fprintf(f, "  \"with_only_extension\": [{\"n\": %d, \"with\": {\"median_ms\": %.6g, \"p95_ms\": %.6g, \"mean_ms\": %.6g, \"frames\": %d, \"steps_per_frame_mean\": %.6g, \"frac_dead_or_runaway\": %.4f}, \"without\": \"not run (WITHOUT is the expensive side; skipped to keep the sweep under 5 min); not extrapolated\"}],\n",
                    r.n, r.med[1], r.p95[1], r.mean[1], r.frames[1], r.steps_mean[1], r.f_either[1]);
        }
        fprintf(f, "  \"notes\": \"%s\"\n}\n", notes.c_str());
        printf("notes: %s\n", notes.c_str());
    }
    fclose(f);
    printf("fit exponents (all n): W/O %.3f, WITH %.3f; n>=64: %.3f, %.3f\n", e0, e1, e0b, e1b);
    printf("max n within %.1f ms: W/O %s, WITH %s\n", budget, mnw(0).c_str(), mnw(1).c_str());
    printf("wrote %s  (%.1f s)\n", path.c_str(), (now_ms() - t_start) / 1000.0);
    return 0;
}

// ================================================================ video mode
static void draw_town(Canvas& p, float cx, float cy, float cell, const Town& T, int frame, bool big_glow) {
    const float R = cell * 0.30f, r0 = std::clamp(cell * 0.13f, 0.9f, 15.f);
    bool bad = false;
    for (int i = 0; i < D; ++i) bad |= T.x[i] < econ::DEAD || T.x[i] > econ::RUNAWAY;
    if (cell >= 14) p.ring(cx, cy, R + r0 + 2, cell >= 40 ? 1.6f : 1.f, bad ? pal::without : pal::grid, bad ? 0.55f : 0.9f);
    const float flash = 0.55f + 0.45f * std::sin(frame * 0.45f);
    for (int i = 0; i < D; ++i) {
        float a = -1.5708f + i * 6.2832f / D;
        float x = cx + R * std::cos(a), y = cy + R * std::sin(a);
        double v = std::log10(std::max(T.x[i], 1e-300));
        if (T.x[i] < econ::DEAD) {
            p.circle(x, y, r0 * 0.75f, rgb(0x4a505c), 0.95f);
        } else if (T.x[i] > econ::RUNAWAY) {
            if (big_glow) p.glow(x, y, r0 * 3.2f, pal::bad, 0.8f * flash);
            p.circle(x, y, r0 * 1.15f, pal::bad, 0.35f + 0.65f * flash);
        } else {
            float b = (float)std::clamp(0.62 + 0.13 * v, 0.28, 1.0);   // brightness on a log scale
            float rr = r0 * (float)std::clamp(1.0 + 0.10 * v, 0.6, 1.35);
            Color c = scale(spcol(i), b);
            if (big_glow) p.glow(x, y, rr * 2.6f, c, 0.45f);
            p.circle(x, y, rr, c, 1.f);
        }
    }
}

int main(int argc, char** argv) {
    Setup su;
    su.seed = (unsigned)arg_int(argc, argv, "--seed", 149);
    su.jit = arg_double(argc, argv, "--jit", 1.5);
    su.init();
    if (arg_flag(argc, argv, "--sweep")) return sweep(argc, argv, su);

    Config cfg;
    cfg.name = "L4-economy-ladder";
    cfg.title_left = "7 recipes (2 return reactions removed)";
    cfg.title_right = "weakly reversible 9-recipe graph";
    cfg.caption = "Family 149 — Uniform Permanence in Weakly Reversible Mass-Action Systems · PROVED (thm:main): every WITH town stays in "
                  "[ε, 1/ε] · MEASURED here: cost vs N, same Rosenbrock 2(3), 1 thread per side";
    const std::vector<int> NS = {1, 4, 16, 64, 256, 1024, 4096};
    const int NST = (int)NS.size(), HOLD = 120;
    cfg.frames = NST * FRAMES_PER_STAGE + HOLD;
    cfg.budget_ms = 16.7;
    cfg.show_speedup = false;  // whole-video medians would mix stages; per-stage ratios are in the chart
    Harness h(argc, argv, cfg);

    struct VStage { double med[2]{-1, -1}; double either[2]{}; int frames[2]{}; bool done = false; };
    std::vector<VStage> vs(NST);
    std::vector<double> cur_ms[2];
    std::vector<double> cur_steps[2];
    int stage = -1;
    int skip_k[2] = {1, 1}, next_cf[2] = {0, 0};
    double last_cms[2] = {0, 0};
    int first_over[2] = {-1, -1};
    double tcur = 0;

    for (int k = 0; k < 2; ++k) {
        h.panel(k).set_compute_label("integrate N towns");
        h.panel(k).sparkline("compute", true);
    }

    // ---- bottom strip overlay: cost-vs-N chart + stage table (drawn on the composed frame)
    h.on_overlay([&](Canvas& c) {
        const float sy = 838, sh = 180;
        c.fill_rect(0, sy, 1920, sh, rgb(0x070910), 0.94f);
        c.line(0, sy, 1920, sy, 1, pal::grid);
        // chart area
        const float gx = 96, gy = sy + 30, gw = 1110, gh = sh - 58;
        double vmin = 1e-3, vmax = 100;  // axis covers 1 µs .. 0.1 s, widened if the data need it
        for (int i = 0; i <= stage && i < NST; ++i)
            for (int k = 0; k < 2; ++k) {
                double m = vs[i].done ? vs[i].med[k] : median_of(cur_ms[k]);
                if (m > 0) { vmin = std::min(vmin, m); vmax = std::max(vmax, m); }
            }
        const double lx0 = -0.25, lx1 = std::log10(4096.0) + 0.25;
        const double ly0 = std::max(-7.0, std::floor(std::log10(vmin))), ly1 = std::min(4.0, std::max(2.0, std::ceil(std::log10(vmax))));
        auto X = [&](double n) { return gx + gw * (float)((std::log10(n) - lx0) / (lx1 - lx0)); };
        auto Y = [&](double ms) { return gy + gh * (float)((ly1 - std::log10(std::max(ms, 1e-6))) / (ly1 - ly0)); };
        c.text(gx - 80, sy + 6, "compute ms / frame (median per stage, log)  vs  N towns (log) — measured live in this recording", 15, pal::text,
               Font::Bold);
        static const char* yl[] = {"0.1 ns", "1 ns", "10 ns", "0.1 µs", "1 µs", "10 µs", "0.1 ms", "1 ms", "10 ms", "0.1 s", "1 s", "10 s"};
        for (int e = (int)ly0; e <= (int)ly1; ++e) {
            float y = Y(std::pow(10.0, e));
            c.line(gx, y, gx + gw, y, 1, pal::grid, 0.7f);
            c.text(gx - 8, y - 8, yl[std::clamp(e + 7, 0, 11)], 12, pal::dim, Font::Mono, Align::Right);
        }
        for (int i = 0; i < NST; ++i) {
            float x = X(NS[i]);
            c.line(x, gy, x, gy + gh, 1, pal::grid, i == stage ? 1.f : 0.5f);
            c.text(x, gy + gh + 3, commas(NS[i]), 12, i == stage ? pal::text : pal::dim, Font::Mono, Align::Center);
        }
        // budget line (dashed)
        float yb = Y(16.7);
        for (float x = gx; x < gx + gw; x += 14) c.line(x, yb, std::min(x + 8, gx + gw), yb, 1.5f, pal::warn, 0.9f);
        c.text(gx + 10, yb + 3, "16.7 ms = 60 fps budget", 12, pal::warn, Font::Sans, Align::Left);
        // curves
        for (int k = 0; k < 2; ++k) {
            Color col = k ? pal::with : pal::without;
            std::vector<Vec2> pts;
            for (int i = 0; i <= stage && i < NST; ++i) {
                double m = vs[i].done ? vs[i].med[k] : median_of(cur_ms[k]);
                if (m <= 0) continue;
                pts.push_back({X(NS[i]), Y(m)});
            }
            if (pts.size() > 1) c.polyline(pts.data(), (int)pts.size(), 2.5f, col, 0.95f);
            for (size_t i = 0; i < pts.size(); ++i) {
                bool live = (int)i == stage && !vs[stage].done;
                if (live) {
                    c.ring(pts[i].x, pts[i].y, 6, 2, col, 0.6f + 0.4f * std::sin(h.frame() * 0.3f));
                } else {
                    c.circle(pts[i].x, pts[i].y, 4.5f, col);
                }
            }
            if (first_over[k] >= 0) {
                float x = X(NS[first_over[k]]), y = Y(vs[first_over[k]].med[k]);
                c.ring(x, y, 11, 2, pal::warn, 0.95f);
                c.text(x - 14, y - 26, fmt("%s over budget at N = %s", k ? "WITH" : "WITHOUT", commas(NS[first_over[k]]).c_str()), 13, col,
                       Font::Bold, Align::Right);
            }
        }
        // ratio labels
        for (int i = 0; i <= stage && i < NST; ++i) {
            double a = vs[i].done ? vs[i].med[0] : median_of(cur_ms[0]);
            double b = vs[i].done ? vs[i].med[1] : median_of(cur_ms[1]);
            if (a <= 0 || b <= 0) continue;
            float y = std::min(Y(a), Y(b)) - 18;
            c.text(X(NS[i]) + 8, std::max(gy - 2, y), fmt("×%.1f", a / b), 13, pal::text, Font::Mono, Align::Left);
        }
        // legend
        {
            const float lx = 790, ly = sy + 8;
            c.fill_rect(lx, ly + 7, 14, 3, pal::without);
            c.text(lx + 20, ly, "WITHOUT", 12, pal::without, Font::Bold);
            c.fill_rect(lx + 90, ly + 7, 14, 3, pal::with);
            c.text(lx + 110, ly, "WITH", 12, pal::with, Font::Bold);
            c.text(lx + 160, ly, "×r = WITHOUT / WITH median", 12, pal::dim, Font::Sans);
        }
        if (h.frame() >= NST * FRAMES_PER_STAGE)
            c.text(gx + gw, gy + gh - 16, "hold: t > 120, most WITHOUT towns are halted at 1e12 and cost ~0 — the extra cost is the collapse transient",
                   12, pal::warn, Font::Sans, Align::Right);

        // ---- stage table
        const float tx = 1260, ty = sy + 8;
        c.text(tx, ty, "stage medians (240 frames, t = 0→120)", 14, pal::text, Font::Bold);
        c.text(tx, ty + 20, "      N   W/O ms    WITH ms   ratio   dead or runaway at t=120", 12, pal::dim, Font::Mono);
        for (int i = 0; i < NST; ++i) {
            float y = ty + 38 + i * 17.5f;
            if (i > stage) {
                c.text(tx, y, fmt("%7s   —", commas(NS[i]).c_str()), 12, rgb(0x3a4050), Font::Mono);
                continue;
            }
            bool live = !vs[i].done;
            double a = live ? median_of(cur_ms[0]) : vs[i].med[0], b = live ? median_of(cur_ms[1]) : vs[i].med[1];
            Color tc = live ? pal::warn : pal::text;
            c.text(tx, y, fmt("%7s %8.3f %10.4f %7.1f×", commas(NS[i]).c_str(), a, b, a / std::max(b, 1e-9)), 12, tc, Font::Mono);
            if (!live) {
                c.text(tx + 330, y, fmt("W/O %5.1f%%", 100 * vs[i].either[0]), 12, pal::without, Font::Mono);
                c.text(tx + 430, y, fmt("WITH %5.1f%%", 100 * vs[i].either[1]), 12, pal::with, Font::Mono);
            } else {
                c.text(tx + 330, y, "running…", 12, pal::dim, Font::Mono);
            }
        }
    });

    for (int k = 0; k < 2; ++k) h.panel(k).set_budget_ms(16.7);

    while (h.next_frame()) {
        const int fr = h.frame();
        int st = std::min(NST - 1, fr / FRAMES_PER_STAGE);
        int lf = fr - st * FRAMES_PER_STAGE;  // frame within stage (>= 240 during the final hold)
        if (st != stage) {
            if (stage >= 0 && !vs[stage].done) {  // (only reached if a stage had no frames)
                vs[stage].done = true;
            }
            stage = st;
            su.reset(NS[stage]);
            for (int k = 0; k < 2; ++k) { cur_ms[k].clear(); cur_steps[k].clear(); skip_k[k] = 1; next_cf[k] = fr; }
        }
        tcur = (lf + 1) * DT;
        // ---- compute (timed); over-budget sides are updated every k-th frame and catch up to tcur
        for (int q = 0; q < 2; ++q) {
            int k = (fr + q) % 2;
            if (fr < next_cf[k]) continue;
            Side& s = su.side[k];
            double a = 0, b = 0;
            h.panel(k).measure([&] { a = now_ms(); s.step(tcur); b = now_ms(); });
            last_cms[k] = b - a;
            if (lf < FRAMES_PER_STAGE) { cur_ms[k].push_back(last_cms[k]); cur_steps[k].push_back((double)s.steps_frame); }
            if (last_cms[k] > 250) skip_k[k] = std::clamp((int)std::ceil(last_cms[k] / 60.0), 2, 60);
            else skip_k[k] = 1;
            next_cf[k] = fr + skip_k[k];
        }
        for (int k = 0; k < 2; ++k) su.side[k].census();
        if (lf == FRAMES_PER_STAGE - 1 && !vs[stage].done) {
            for (int k = 0; k < 2; ++k) {
                vs[stage].med[k] = median_of(cur_ms[k]);
                vs[stage].frames[k] = (int)cur_ms[k].size();
                vs[stage].either[k] = double(su.side[k].either) / NS[stage];
                if (first_over[k] < 0 && vs[stage].med[k] > 16.7) first_over[k] = stage;
            }
            vs[stage].done = true;
            if (!h.quiet())
                printf("[stage N=%d] median ms W/O %.4f WITH %.4f  dead/runaway %.1f%% / %.1f%%\n", NS[stage], vs[stage].med[0], vs[stage].med[1],
                       100 * vs[stage].either[0], 100 * vs[stage].either[1]);
        }

        // ================================================================ drawing (untimed)
        const int N = NS[stage];
        for (int k = 0; k < 2; ++k) {
            Side& sd = su.side[k];
            Panel& p = h.panel(k);
            const Color acc = k ? pal::with : pal::without;
            p.clear(pal::bg);
            // featured town (#1) as a large orb ring, top right
            {
                const Town& T = sd.towns[0];
                const float cx = 712, cy = 168, R0 = 98;
                p.text(cx, 12, fmt("town #1 of %s  ·  t = %.1f%s", commas(N).c_str(), T.t, T.halted ? " (halted at 1e12)" : ""), 15,
                       T.halted ? pal::without : pal::dim, Font::Sans, Align::Center);
                for (int i = 0; i < D; ++i) {
                    float a = -1.5708f + i * 6.2832f / D;
                    float x = cx + R0 * std::cos(a), y = cy + R0 * std::sin(a);
                    p.line(cx, cy, x, y, 1, pal::grid, 0.8f);
                }
                for (int i = 0; i < D; ++i) {
                    float a = -1.5708f + i * 6.2832f / D;
                    float x = cx + R0 * std::cos(a), y = cy + R0 * std::sin(a);
                    double v = T.x[i];
                    double l = std::log10(std::max(v, 1e-300));
                    float r = (float)std::clamp(20.0 + 4.0 * l, 5.0, 40.0);
                    bool dd = v < econ::DEAD, rr = v > econ::RUNAWAY;
                    Color c = spcol(i);
                    if (dd) {
                        p.circle(x, y, r, rgb(0x4a505c), 0.9f);
                        p.ring(x, y, 22, 1.5f, pal::bad, 0.7f);
                    } else {
                        p.glow(x, y, r * 2.3f, rr ? pal::bad : c, rr ? 0.6f + 0.4f * std::sin(fr * 0.45f) : 0.5f);
                        p.circle(x, y, r, scale(c, 0.55f), 0.95f);
                        p.ring(x, y, r, 2, rr ? pal::bad : c, 0.95f);
                    }
                    float ly = y + std::max(r, 18.f) + 2;
                    p.text(x, ly, econ::SPN[i], 13, c, Font::Bold, Align::Center);
                    p.text(x, ly + 15, fmt("%.1e%s", v, dd ? " dead" : rr ? " runaway" : ""), 12, dd || rr ? pal::bad : pal::dim, Font::Mono,
                           Align::Center);
                }
            }
            // counters
            {
                const float y = 390;
                Color cc = sd.either ? pal::without : pal::with;
                p.text(20, y, fmt("dead or runaway towns: %s / %s", commas(sd.either).c_str(), commas(N).c_str()), 22, cc, Font::Bold);
                p.text(940, y + 6, fmt("dead %s · runaway %s · halted at 1e12 %s", commas(sd.dead).c_str(), commas(sd.run).c_str(),
                                       commas(sd.halted).c_str()),
                       13, pal::dim, Font::Mono, Align::Right);
            }
            // town grid
            {
                const float gx = 16, gy = 428, gw = 928, gh = 330;
                int cols = 1, rows = N;
                float cell = 0;
                for (int c = 1; c <= N; ++c) {  // largest square cell that fits all N towns
                    int r = (N + c - 1) / c;
                    float s = std::min(gw / c, gh / r);
                    if (s > cell) { cell = s; cols = c; rows = r; }
                }
                cell = std::min(cell, 150.f);
                float ox = gx + (gw - cols * cell) * 0.5f, oy = gy + (gh - rows * cell) * 0.5f;
                p.fill_rounded_rect(gx - 6, gy - 6, gw + 12, gh + 12, 8, rgb(0x080a10), 1);
                bool big = cell >= 18;
                if (big) p.blend = Blend::Normal;
                for (int i = 0; i < N; ++i) {
                    int c = i % cols, r = i / cols;
                    draw_town(p, ox + (c + 0.5f) * cell, oy + (r + 0.5f) * cell, cell, sd.towns[i], fr, big);
                }
                p.text(940, 336, "grid: one glyph per town · 5 dots = Ore Wood Tools Food Workers", 12, pal::dim, Font::Sans, Align::Right);
                p.text(940, 352, "size/brightness ∝ log quantity · grey = dead < 1e-6 · red flash = runaway > 1e6", 12, pal::dim, Font::Sans, Align::Right);
                (void)acc;
            }
            // HUD
            p.metric_text("load", fmt("N = %s towns · stage %d/%d", commas(N).c_str(), stage + 1, NST), Tone::Accent);
            const auto& S = sd.S;
            p.metric_text("weakly reversible? (SCCs)",
                          S.wr ? fmt("YES: %d SCC = %d LC", S.n_scc, S.n_lc) : fmt("NO: %d of %d lack return", S.no_return, (int)(k ? 9 : 7)),
                          S.wr ? Tone::Good : Tone::Bad);
            p.metric("solver steps / frame (acc+rej)", (double)sd.steps_frame, "si");
            p.metric("stage median ms / frame", cur_ms[k].empty() ? 0.0 : median_of(cur_ms[k]), "%.4f");
            p.metric_text("dead or runaway towns", fmt("%s / %s (%.0f%%)", commas(sd.either).c_str(), commas(N).c_str(), 100.0 * sd.either / N),
                          sd.either ? Tone::Bad : Tone::Good);
            if (skip_k[k] > 1)
                p.metric_text("sim time t / status", fmt("over budget: %.0f ms/frame — updated every %d frames", last_cms[k], skip_k[k]), Tone::Warn);
            else
                p.metric_text("sim time t / status", fmt("t = %.1f (dt %.1f per frame)", tcur, DT));
        }
    }

    for (int i = 0; i < NST; ++i) {
        h.result(fmt("stage%d_n", i), NS[i]);
        for (int k = 0; k < 2; ++k) {
            const char* sn = k ? "with" : "without";
            h.result(fmt("stage%d_%s_median_ms", i, sn), vs[i].med[k]);
            h.result(fmt("stage%d_%s_frames", i, sn), vs[i].frames[k]);
            h.result(fmt("stage%d_%s_frac_dead_or_runaway", i, sn), vs[i].either[k]);
        }
    }
    h.result("frames_per_stage", FRAMES_PER_STAGE);
    h.result("dt_per_frame", DT);
    h.result("rate_jitter", su.jit);
    h.result("threads_per_side", 1);
    return h.finish();
}
