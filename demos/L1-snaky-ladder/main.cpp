// L1-snaky-ladder - load ladder for 03-snaky (Family 187, Maker-Breaker Snaky on 17x17).
//   Load N = concurrent games per side; each frame every game's Maker makes one decision
//   (then the shared, seeded Breaker replies, untimed). Finished games restart at once.
//   LEFT  (WITHOUT): 03-snaky's alpha-beta game AI (threat eval, width-9 ordering, iterative deepening,
//                    6000 nodes/move) upgraded with incremental eval/ordering + a transposition table.
//   RIGHT (WITH):    the 21-move certificate policy (728 cards, first surviving child), 03-snaky's port.
// Modes: video (default) · --sweep (timing + fixed-budget quality ladder -> out/ladder.json)
//        --engine-check G (03-snaky engine vs this ladder's engine, same 6000-node budget)
#include "demo.h"
#include "ladder_core.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

using namespace demo;
using namespace snaky;

static const char* kDefaultCert =
    "<openai/math>/preprints/Snaky-in-21-Maker-moves-September-25-2026/verification/certificate.txt";
static constexpr double kBudget = 16.7;

extern "C" __declspec(dllimport) int __stdcall QueryPerformanceFrequency(long long* f);
static double tick_ms() {  // QueryPerformanceCounter resolution
    static double t = [] { long long f = 0; QueryPerformanceFrequency(&f); return f > 0 ? 1000.0 / double(f) : 1e-4; }();
    return t;
}
// WITHOUT/WITH ratio; when WITH is within 3 timer ticks it is resolution-limited and only a lower bound is reported
static bool ratio_exact(double with_ms) { return with_ms >= 3 * tick_ms() - 1e-12; }
static double ratio_value(double wo_ms, double with_ms) {
    return ratio_exact(with_ms) ? wo_ms / with_ms : wo_ms / (std::round(with_ms / tick_ms()) + 1) / tick_ms();
}

static double quant(std::vector<double> v, double q) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    double pos = q * (v.size() - 1);
    size_t i = size_t(pos);
    double fr = pos - i;
    return i + 1 < v.size() ? v[i] * (1 - fr) + v[i + 1] * fr : v[i];
}
static std::string group(long long v) {
    std::string s = std::to_string(v), o;
    int c = 0;
    for (int i = int(s.size()) - 1; i >= 0; --i) { o.insert(o.begin(), s[i]); if (++c % 3 == 0 && i > 0) o.insert(o.begin(), ','); }
    return o;
}
static std::string ms_str(double ms) {
    if (ms >= 1000) return fmt("%.2f s", ms / 1000);
    if (ms >= 10) return fmt("%.1f ms", ms);
    if (ms >= 1) return fmt("%.2f ms", ms);
    if (ms >= 0.01) return fmt("%.1f µs", ms * 1000);
    return fmt("%.2f µs", ms * 1000);
}
static std::string ratio_str(double r) {
    if (r >= 1e6) return fmt("×%.1fM", r / 1e6);
    if (r >= 1e4) return fmt("×%.0fk", r / 1e3);
    if (r >= 1e3) return fmt("×%.1fk", r / 1e3);
    if (r >= 10) return fmt("×%.0f", r);
    return fmt("×%.1f", r);
}

// ======================================================================== engine check
static int engine_check(const Certificate& C, int G) {
    std::printf("engine check: %d games per Breaker kind, Maker node budget 6000, Breaker alpha-beta = 03-snaky SearchAI 6000 nodes\n", G);
    FastSearch eng;
    for (int e = 0; e < 2; ++e) {
        MK mk = e ? MK::Fast : MK::Orig;
        double t0 = now_ms();
        for (int kind = 0; kind < BRK_COUNT; ++kind) {
            long long wins = 0, dec = 0, nodes = 0, maxw = 0; double ms = 0;
            for (int j = 0; j < G; ++j) {
                OneResult r = play_one(mk, kind, 0xE9C0ull + kind * 100003ull + j, C, 6000, 0, &eng);
                wins += r.status == 1; dec += r.decisions; ms += r.maker_ms; nodes += r.nodes;
                if (r.status == 1) maxw = std::max<long long>(maxw, r.moves);
            }
            std::printf("  %-26s vs %-18s wins %3lld/%d  max moves in win %2lld  %8.1f us/decision  %6.0f nodes/decision  %.0f ns/node\n",
                        e ? "ladder engine (TT+incr.)" : "03-snaky SearchAI", breaker_name(kind), wins, G, maxw, 1000 * ms / dec,
                        double(nodes) / dec, 1e6 * ms / std::max<long long>(1, nodes));
        }
        std::printf("  (%.1f s)\n", (now_ms() - t0) / 1000);
    }
    return 0;
}

// ======================================================================== sweep
struct Timing { double median = 0, p95 = 0; int frames = 0, warm = 0; bool run = false; double us_per_dec = 0; double wall_s = 0; };

static Timing time_side(Side& S, int n, int ut_threads) {
    Timing T;
    S.clear();
    S.keep_hist = false;
    S.resize(n);
    double w0 = now_ms();
    // warm-up: >= 3 frames and 0.3 s, or a single frame when one frame takes > 1 s
    for (;;) {
        double t = now_ms();
        S.timed();
        double dt = now_ms() - t;
        S.untimed(ut_threads);
        ++T.warm;
        if (dt > 1000 || (T.warm >= 3 && now_ms() - w0 > 300)) break;
    }
    std::vector<double> ms;
    double m0 = now_ms();
    for (;;) {
        double t = now_ms();
        S.timed();
        double dt = now_ms() - t;
        S.untimed(ut_threads);
        ms.push_back(dt);
        double el = now_ms() - m0;
        size_t need = 15;
        bool slow = ms[0] > 800;
        if (slow) need = ms[0] > 5000 ? 1 : size_t(std::ceil(8000 / ms[0]));
        if (ms.size() >= need && (el >= 1500 || slow)) break;
        if (ms.size() >= 300000) break;
    }
    T.run = true;
    T.frames = int(ms.size());
    T.median = quant(ms, 0.5);
    T.p95 = quant(ms, 0.95);
    T.us_per_dec = 1000 * T.median / n;
    T.wall_s = (now_ms() - w0) / 1000;
    return T;
}

struct Quality {
    bool run = false;
    int games = 0;  // per Breaker kind
    double allot_us = 0;
    double s_ab = 0, s_gr = 0, c_ab = 0, c_gr = 0;   // win rates
    double s_us = 0, s_nodes = 0, c_us = 0;          // measured per-move cost, mean nodes/move
    long long s_maxw = 0, c_maxw = 0;
    double wall_s = 0;
};

// n > 0: search Maker with a 16.7/n ms per-move deadline; n == 0: certificate Maker (budget-independent, played once)
static Quality quality_stage(const Certificate& C, int n, int G, int qthreads) {
    Quality Q;
    Q.run = true; Q.games = G;
    const double allot_ms = n > 0 ? kBudget / n : 0;
    Q.allot_us = allot_ms * 1000;
    struct Task { int mk, kind, j; OneResult r; };
    std::vector<Task> tasks;
    const int kinds[2] = {BRK_SEARCH, BRK_GREEDY};
    for (int k = 0; k < 2; ++k)
        for (int j = 0; j < G; ++j) tasks.push_back({n > 0 ? 0 : 1, kinds[k], j, {}});
    std::atomic<int> next{0};
    double t0 = now_ms();
    std::vector<std::thread> ts;
    for (int t = 0; t < qthreads; ++t)
        ts.emplace_back([&] {
            FastSearch eng;
            for (;;) {
                int i = next.fetch_add(1);
                if (i >= int(tasks.size())) break;
                Task& tk = tasks[i];
                uint64_t seed = 0x9A1Bull + uint64_t(tk.kind) * 100003ull + uint64_t(tk.j);
                tk.r = play_one(tk.mk == 0 ? MK::Fast : MK::Cert, tk.kind, seed, C, 0, allot_ms, &eng);
            }
        });
    for (auto& th : ts) th.join();
    Q.wall_s = (now_ms() - t0) / 1000;
    double sms = 0, cms = 0; long long sdec = 0, cdec = 0, snodes = 0;
    long long w[2][2] = {};
    for (auto& tk : tasks) {
        int ki = tk.kind == BRK_SEARCH ? 0 : 1;
        if (tk.r.status == 1) {
            ++w[tk.mk][ki];
            long long& mx = tk.mk ? Q.c_maxw : Q.s_maxw;
            mx = std::max<long long>(mx, tk.r.moves);
        }
        if (tk.mk == 0) { sms += tk.r.maker_ms; sdec += tk.r.decisions; snodes += tk.r.nodes; }
        else { cms += tk.r.maker_ms; cdec += tk.r.decisions; }
    }
    Q.s_ab = double(w[0][0]) / G; Q.s_gr = double(w[0][1]) / G;
    Q.c_ab = double(w[1][0]) / G; Q.c_gr = double(w[1][1]) / G;
    Q.s_us = 1000 * sms / std::max<long long>(1, sdec);
    Q.c_us = 1000 * cms / std::max<long long>(1, cdec);
    Q.s_nodes = double(snodes) / std::max<long long>(1, sdec);
    return Q;
}

static double fit_slope(const std::vector<double>& x, const std::vector<double>& y) {
    int n = int(x.size());
    if (n < 2) return 0;
    double mx = 0, my = 0;
    for (int i = 0; i < n; ++i) { mx += std::log(x[i]); my += std::log(y[i]); }
    mx /= n; my /= n;
    double sxy = 0, sxx = 0;
    for (int i = 0; i < n; ++i) { double a = std::log(x[i]) - mx; sxy += a * (std::log(y[i]) - my); sxx += a * a; }
    return sxx > 0 ? sxy / sxx : 0;
}

static int run_sweep(const Certificate& C, int argc, char** argv) {
    const int max_n = arg_int(argc, argv, "--max-n", 262144);
    const int ut = arg_int(argc, argv, "--ut-threads", 1);
    const int qthreads = arg_int(argc, argv, "--qthreads", 6);
    const int G = arg_int(argc, argv, "--qgames", 60);
    const int q_max_n = arg_int(argc, argv, "--q-max-n", 65536);
    const double stop_ms = arg_double(argc, argv, "--stop-ms", 5000);
    const bool no_quality = arg_flag(argc, argv, "--no-quality");
    const bool quality_only = arg_flag(argc, argv, "--quality-only");  // debugging: skip the timing ladder
    Quality qcert;
    if (!no_quality) {
        qcert = quality_stage(C, 0, G, qthreads);
        std::printf("certificate Maker quality (budget-independent, same seeds as every stage): vs alpha-beta %.0f%%, vs greedy %.0f%%, max moves in a win %lld, %.3f us/move [%.0fs]\n",
                    100 * qcert.c_ab, 100 * qcert.c_gr, qcert.c_maxw, qcert.c_us, qcert.wall_s);
    }
    std::string out = arg_str(argc, argv, "--out", "");
    if (out.empty()) {
        std::string exe = argv[0];
        size_t p = exe.find_last_of("/\\");
        out = (p == std::string::npos ? std::string(".") : exe.substr(0, p)) + "/out";
    }
    std::vector<int> ns;
    for (int n = 1; n <= max_n; n *= 2) ns.push_back(n);
    std::printf("sweep: stages 1..%d (x2), untimed referee/Breaker threads %d, quality threads %d, %d games per Breaker kind\n", max_n, ut, qthreads, G);
    std::printf("timing Breakers: random-local / greedy ES blocker (alternating by slot); quality Breakers: alpha-beta (03-snaky, 6000 nodes) and greedy\n");
    std::printf("%8s | %10s %10s %6s | %10s %10s %6s | %9s | %-24s | %s\n", "n", "W/O med", "W/O p95", "frm", "WITH med", "WITH p95", "frm",
                "ratio", "quality a-b: srch / cert", "greedy: srch / cert, srch us/move (allot), nodes");
    Side S, R;
    S.cert = false; S.C = &C; S.eng.budget = 6000;
    R.cert = true; R.C = &C;
    std::vector<Timing> tw(ns.size()), tc(ns.size());
    std::vector<Quality> qs(ns.size());
    bool stopped = false;
    int stopped_at = -1;
    double t_all = now_ms();
    for (size_t i = 0; i < ns.size(); ++i) {
        int n = ns[i];
        if (quality_only) stopped = true;
        if (!stopped) {
            tw[i] = time_side(S, n, ut);
            S.clear();
            if (tw[i].median > stop_ms) { stopped = true; stopped_at = n; }
        }
        if (!quality_only) { tc[i] = time_side(R, n, ut); R.clear(); }
        if (!no_quality && n <= q_max_n) {
            qs[i] = quality_stage(C, n, G, qthreads);
            qs[i].c_ab = qcert.c_ab; qs[i].c_gr = qcert.c_gr; qs[i].c_us = qcert.c_us; qs[i].c_maxw = qcert.c_maxw;
        }
        std::string ratio = tw[i].run ? (ratio_exact(tc[i].median) ? "" : ">=") + ratio_str(ratio_value(tw[i].median, tc[i].median)) : "-";
        std::printf("%8s | %10s %10s %6d | %10s %10s %6d | %9s | ", group(n).c_str(), tw[i].run ? ms_str(tw[i].median).c_str() : "-",
                    tw[i].run ? ms_str(tw[i].p95).c_str() : "-", tw[i].frames, ms_str(tc[i].median).c_str(), ms_str(tc[i].p95).c_str(),
                    tc[i].frames, ratio.c_str());
        if (qs[i].run)
            std::printf("%5.0f%% / %5.0f%%            | %4.0f%% / %4.0f%%, %.2f us (%.3g), %.0f nodes  [%.0fs]\n", 100 * qs[i].s_ab, 100 * qs[i].c_ab,
                        100 * qs[i].s_gr, 100 * qs[i].c_gr, qs[i].s_us, qs[i].allot_us, qs[i].s_nodes, qs[i].wall_s);
        else std::printf("\n");
        std::fflush(stdout);
    }
    // ---- fits and budget crossings
    std::vector<double> xw, yw, xc, yc, xc64, yc64, xw4, yw4, xcr, ycr;
    int maxw = 0, maxc = 0;
    for (size_t i = 0; i < ns.size(); ++i) {
        if (tw[i].run) { xw.push_back(ns[i]); yw.push_back(tw[i].median); if (tw[i].median <= kBudget) maxw = ns[i]; }
        if (tw[i].run && ns[i] >= 4) { xw4.push_back(ns[i]); yw4.push_back(tw[i].median); }
        xc.push_back(ns[i]); yc.push_back(std::max(1e-9, tc[i].median));
        if (ns[i] >= 64) { xc64.push_back(ns[i]); yc64.push_back(tc[i].median); }
        if (tc[i].median >= 10 * tick_ms()) { xcr.push_back(ns[i]); ycr.push_back(tc[i].median); }  // >= 10 timer ticks
        if (tc[i].median <= kBudget) maxc = ns[i];
    }
    double ew = fit_slope(xw, yw), ec = fit_slope(xc, yc), ec64 = fit_slope(xc64, yc64), ew4 = fit_slope(xw4, yw4), ecr = fit_slope(xcr, ycr);
    const int ecr_from = xcr.empty() ? 0 : int(xcr[0]);
    int first_over_c = -1, first_over_w = -1;
    for (size_t i = 0; i < ns.size(); ++i) {
        if (first_over_w < 0 && tw[i].run && tw[i].median > kBudget) first_over_w = ns[i];
        if (first_over_c < 0 && tc[i].median > kBudget) first_over_c = ns[i];
    }
    std::printf("fit (log-log slope of median ms vs n): WITHOUT %.3f (all), %.3f (n>=4) | WITH %.3f (n>=%d, medians >= 10 timer ticks), "
                "%.3f (all, resolution-limited at small n), %.3f (n>=64)\n", ew, ew4, ecr, ecr_from, ec, ec64);
    std::printf("timer tick %.3g us\n", tick_ms() * 1000);
    std::printf("max n within 16.7 ms: WITHOUT %d (first over: %d) | WITH %d (first over: %d)\n", maxw, first_over_w, maxc, first_over_c);
    if (stopped) std::printf("WITHOUT not timed above n=%d (median > %.0f ms/frame there)\n", stopped_at, stop_ms);
    std::printf("sweep wall %.1f s\n", (now_ms() - t_all) / 1000);

    // ---- ladder.json
    std::string js = "{\n";
    js += "  \"demo\": \"L1-snaky-ladder\",\n  \"family\": \"187\",\n  \"load_name\": \"concurrent games\",\n";
    js += fmt("  \"budget_ms\": %.1f,\n  \"threads\": {\"without\": 1, \"with\": 1},\n", kBudget);
    js += "  \"stages\": [\n";
    for (size_t i = 0; i < ns.size(); ++i) {
        int n = ns[i];
        js += fmt("    {\"n\": %d, ", n);
        if (tw[i].run)
            js += fmt("\"without\": {\"median_ms\": %.6g, \"p95_ms\": %.6g, \"frames\": %d, \"warmup_frames\": %d, \"us_per_decision\": %.5g}, ", tw[i].median,
                      tw[i].p95, tw[i].frames, tw[i].warm, tw[i].us_per_dec);
        else
            js += fmt("\"without\": {\"median_ms\": null, \"p95_ms\": null, \"frames\": 0, \"skipped\": \"not timed: median > %.0f ms/frame already at n=%d\"}, ", stop_ms, stopped_at);
        js += fmt("\"with\": {\"median_ms\": %.6g, \"p95_ms\": %.6g, \"frames\": %d, \"warmup_frames\": %d, \"us_per_decision\": %.5g}", tc[i].median,
                  tc[i].p95, tc[i].frames, tc[i].warm, tc[i].us_per_dec);
        if (!ratio_exact(tc[i].median)) js += ", \"with_resolution_limited\": true";
        if (tw[i].run) {
            if (ratio_exact(tc[i].median)) js += fmt(", \"ratio_without_over_with\": %.6g", ratio_value(tw[i].median, tc[i].median));
            else js += fmt(", \"ratio_without_over_with\": null, \"ratio_lower_bound\": %.6g", ratio_value(tw[i].median, tc[i].median));
        }
        if (qs[i].run) {
            const Quality& q = qs[i];
            js += fmt(", \"quality\": {\"name\": \"Maker win rate vs alpha-beta search Breaker, Maker given 16.7 ms/frame split evenly over n games\", "
                      "\"without\": %.4g, \"with\": %.4g, \"higher_is_better\": true, \"games\": %d, "
                      "\"without_vs_greedy\": %.4g, \"with_vs_greedy\": %.4g, \"games_vs_greedy\": %d, "
                      "\"allotted_us_per_move\": %.5g, \"without_measured_us_per_move\": %.5g, \"without_mean_nodes_per_move\": %.5g, "
                      "\"with_measured_us_per_move\": %.5g, \"with_fits_frame_at_this_n\": %s, \"without_max_moves_in_win\": %lld, \"with_max_moves_in_win\": %lld}",
                      q.s_ab, q.c_ab, q.games, q.s_gr, q.c_gr, q.games, q.allot_us, q.s_us, q.s_nodes, q.c_us,
                      tc[i].median <= kBudget ? "true" : "false", q.s_maxw, q.c_maxw);
        }
        js += i + 1 < ns.size() ? "},\n" : "}\n";
    }
    js += "  ],\n";
    js += fmt("  \"fit\": {\"without_exponent\": %.4g, \"with_exponent\": %.4g, \"with_exponent_fit_from_n\": %d, \"with_exponent_all_stages\": %.4g, "
              "\"with_exponent_n_ge_64\": %.4g, \"without_exponent_n_ge_4\": %.4g, \"note\": \"least-squares slope of log(median_ms) on log(n); "
              "with_exponent uses the stages whose WITH median is >= 10 QPC ticks (%.2g us)\"},\n",
              ew, ecr, ecr_from, ec, ec64, ew4, 10 * tick_ms() * 1000);
    js += fmt("  \"max_n_within_budget\": {\"without\": %d, \"with\": %d, \"without_first_over\": %d, \"with_first_over\": %s},\n", maxw, maxc, first_over_w,
              first_over_c > 0 ? std::to_string(first_over_c).c_str() : "null");
    js += fmt("  \"machine\": {\"cpu\": \"%s\", \"hw_threads\": %d},\n", cpu_name().c_str(), hw_threads());
    js += fmt("  \"settings\": {\"without_node_budget_per_move\": 6000, \"timing_breakers\": \"random-local and greedy ES blocker, alternating by game slot (untimed)\", "
              "\"quality_breakers\": \"alpha-beta search (03-snaky SearchAI, 6000 nodes) and greedy ES blocker\", \"quality_games_per_breaker\": %d, "
              "\"quality_threads\": %d, \"untimed_threads\": %d, \"stop_without_above_ms\": %.0f},\n", G, qthreads, ut, stop_ms);
    std::string notes =
        "Single-threaded Maker compute per frame on each side, timed with QueryPerformanceCounter around the Maker policy only (absorbing Breaker's "
        "previous reply + choosing a cell, for all n games); Breaker replies, referee and restarts are untimed. Games start synchronized at each stage "
        "and restart as soon as they end, so every frame holds exactly n Maker decisions. WITHOUT is 03-snaky's alpha-beta game AI at 6000 nodes/move "
        "with incremental evaluation and a transposition table added (same move choice rules, faster per node). The certificate's win in <= 21 "
        "Maker moves against every Breaker is proved by the paper (Thm thm:main, Cor. cor:finite-board); all costs, crossings, exponents and win rates "
        "here are measurements on one machine, with other jobs possibly running. WITH medians at small n sit at the 0.1 us QueryPerformanceCounter resolution: there the WITHOUT/WITH ratio is reported only as a lower bound. ";
    if (stopped) notes += fmt("WITHOUT was not timed above n=%d because one frame already took > %.0f ms (bracket, no extrapolation). ", stopped_at, stop_ms);
    notes += "Quality: the search Maker gets a hard wall-clock budget of 16.7/n ms per move (checked before every root move and at every interior node; the result is the best fully searched root move, "
             "or the top-ordered move if nothing finished), quality games run on several threads at once. The certificate's quality games do not depend on the budget and are played once with the same seeds.";
    js += "  \"notes\": \"" + notes + "\"\n}\n";
    std::string path = out + "/ladder.json";
    if (FILE* f = std::fopen(path.c_str(), "wb")) { std::fputs(js.c_str(), f); std::fclose(f); std::printf("wrote %s\n", path.c_str()); }
    return 0;
}

#include "video.h"
