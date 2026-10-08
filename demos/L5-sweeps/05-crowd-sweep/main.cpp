// L5-sweeps/05-crowd-sweep - sweep-only load ladder for demos/05-crowd-hub (family 374).
//
// Load n = agents (side x side jittered grid on [-1,1]^2): 10k, 40k, 160k, 640k, 2.56M, 4M, plus two
// extension stages 8.41M and 16M (cheap to run; they locate the 16.7 ms crossing).
// Each stage replays one hub cycle of the original schedule at a = 0.45 (125 frames: hub at rest,
// moves up by b = a/2, holds, comes back), repeated until each side has >= 1.5 s of samples.
//   WITHOUT: frozen labels - agents are labelled only while the hub is at rest (s = 0), then
//            only streamed toward their (now stale) destination.
//   WITH:    recomputed optimal power-cell labels every frame, argmax{-x1, x1, a + s b x2}
//            (Lemma cells:optimality; sharpness.tex in
//            preprints/Sharp-One-Third-Stability-of-Brenier-Maps-September-25-2026/build/source/).
// Both sides time exactly what the original demo timed (labelling + streaming every 4th agent),
// single-threaded on both sides.
// Quality (untimed) at the full hub move s = 1: fraction of agents routed sub-optimally and the
// transport-cost excess over the optimum, both sampled from the n agents and exact
// (exact = slice integrals of the original demo: a/4 and a^2/12).
//
// Build/run (Git Bash, from demos/):  ./build.sh L5-sweeps/05-crowd-sweep && L5-sweeps/05-crowd-sweep/demo.exe
// Flags: --max-side S (default 4000)  --min-ms 1500  --out DIR
#include "../ladder_common.h"

#include <random>

using namespace demo;

// ------------------------------------------------ copied from 05-crowd-hub/main.cpp
struct Exact {
    double changed = 0, rms_with = 0, rms_frozen = 0, w2 = 0, cost_opt = 0, cost_frozen = 0;
};
static double slice_cost(double w, double t, double hy) {
    double c = 2 * w * w * w / 3 + 2 * w * (t - hy) * (t - hy);
    double o = (1 - w);
    c += 2 * (o * o * o / 3 + o * t * t);
    return 0.5 * c;
}
static Exact exact(double a, double s) {
    const double b = a / 2, hy = s * b;
    static const double gx[5] = {-0.9061798459386640, -0.5384693101056831, 0.0, 0.5384693101056831, 0.9061798459386640};
    static const double gw[5] = {0.2369268850561891, 0.4786286704993665, 0.5688888888888889, 0.4786286704993665, 0.2369268850561891};
    Exact e;
    double ch = 0, r2w = 0, r2f = 0, co = 0, cf = 0;
    for (int half = 0; half < 2; ++half)
        for (int k = 0; k < 5; ++k) {
            double t = half == 0 ? -0.5 + 0.5 * gx[k] : 0.5 + 0.5 * gx[k];
            double wt = 0.5 * gw[k] * 0.5;
            double w0 = a, w1 = a + hy * t;
            double d = std::fabs(w1 - w0);
            ch += wt * d;
            r2w += wt * (d + hy * hy * w1);
            r2f += wt * (hy * hy * w0);
            co += wt * slice_cost(w1, t, hy);
            cf += wt * slice_cost(w0, t, hy);
        }
    e.changed = ch; e.rms_with = std::sqrt(r2w); e.rms_frozen = std::sqrt(r2f);
    e.w2 = std::sqrt(a) * hy; e.cost_opt = co; e.cost_frozen = cf;
    return e;
}
struct Crowd {
    int n = 0;
    std::vector<float> x, y, ph;
    std::vector<uint8_t> lab, ref;
    std::vector<float> sx, sy;
    int stride = 4;
};
static Crowd make_crowd(int side, unsigned seed) {
    Crowd c;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> U(0.f, 1.f);
    c.n = side * side;
    c.x.resize(c.n); c.y.resize(c.n); c.ph.resize(c.n);
    for (int j = 0; j < side; ++j)
        for (int i = 0; i < side; ++i) {
            int k = j * side + i;
            c.x[k] = -1.f + 2.f * (i + U(rng)) / side;
            c.y[k] = -1.f + 2.f * (j + U(rng)) / side;
            c.ph[k] = U(rng);
        }
    c.lab.assign(c.n, 0);
    c.ref.assign(c.n, 0);
    c.sx.assign(c.n / c.stride + 1, 0);
    c.sy.assign(c.n / c.stride + 1, 0);
    return c;
}
static inline uint8_t label_of(float x1, float x2, float a, float hy) {
    float l0 = -x1, l1 = x1, l2 = a + hy * x2;
    uint8_t k = 0;
    float m = l0;
    if (l1 > m) { m = l1; k = 1; }
    if (l2 > m) k = 2;
    return k;
}
static inline void dest(uint8_t lab, float hy, float& tx, float& ty) {
    if (lab == 0) { tx = -1; ty = 0; }
    else if (lab == 1) { tx = 1; ty = 0; }
    else { tx = 0; ty = hy; }
}
static void stream(Crowd& c, float hy, float time) {
    int m = 0;
    for (int i = 0; i < c.n; i += c.stride, ++m) {
        float tau = c.ph[i] + time * 0.42f;
        tau -= std::floor(tau);
        float tx, ty;
        dest(c.lab[i], hy, tx, ty);
        c.sx[m] = c.x[i] + tau * (tx - c.x[i]);
        c.sy[m] = c.y[i] + tau * (ty - c.y[i]);
    }
}
static double ease(double u) { u = std::clamp(u, 0.0, 1.0); return u * u * (3 - 2 * u); }
static double s_of(int f, int cyc_len) {  // hub displacement fraction in one cycle (Sched::at, original)
    const double u = double(f % cyc_len) / cyc_len;
    if (u < 0.2) return 0;
    if (u < 0.48) return ease((u - 0.2) / 0.28);
    if (u < 0.8) return 1;
    return 1 - ease((u - 0.8) / 0.2);
}
// ------------------------------------------------ end of copied code

int main(int argc, char** argv) {
    const int max_side = arg_int(argc, argv, "--max-side", 4000);
    const double min_ms = arg_double(argc, argv, "--min-ms", 1500);
    const std::string out = ladder::out_dir(argc, argv);
    const double a = 0.45;
    const int cyc_len = 125;
    const Exact ex = exact(a, 1.0);
    const double exact_changed = ex.changed, exact_excess = ex.cost_frozen - ex.cost_opt;

    ladder::Ladder L;
    L.demo = "L5-sweeps/05-crowd-sweep";
    L.family = "374";
    L.load_name = "agents";
    L.threads_without = L.threads_with = 1;
    std::printf("05-crowd-sweep: a = %.2f, one hub cycle = %d frames, 1 thread per side; exact mis-routed %.4f, excess %.5g\n", a,
                cyc_len, exact_changed, exact_excess);

    for (int side : {100, 200, 400, 800, 1600, 2000, 2900, 4000}) {  // 2900, 4000: extension past the requested 4M
        if (side > max_side) break;
        Crowd Lc = make_crowd(side, 374), Rc = Lc;
        std::vector<double> tO, tW;
        double sumO = 0, sumW = 0, q_frac = NAN, q_excess = NAN;
        const float fa = float(a);
        int f = -6;  // 6 warm-up frames
        while (true) {
            const int ff = std::max(0, f);
            const double s = s_of(ff, cyc_len);
            const float hy = float(s * a / 2), t = float(ff / 60.0);
            auto timeO = [&] {
                double t0 = now_ms();
                if (s == 0)
                    for (int i = 0; i < Lc.n; ++i) Lc.lab[i] = label_of(Lc.x[i], Lc.y[i], fa, 0.f);
                stream(Lc, hy, t);
                return now_ms() - t0;
            };
            auto timeW = [&] {
                double t0 = now_ms();
                for (int i = 0; i < Rc.n; ++i) Rc.lab[i] = label_of(Rc.x[i], Rc.y[i], fa, hy);
                stream(Rc, hy, t);
                return now_ms() - t0;
            };
            double a_ms, b_ms;
            if (f & 1) { b_ms = timeW(); a_ms = timeO(); } else { a_ms = timeO(); b_ms = timeW(); }
            if (f >= 0) {
                tO.push_back(a_ms); tW.push_back(b_ms);
                sumO += a_ms; sumW += b_ms;
                if (s == 1 && std::isnan(q_frac)) {  // untimed quality at the full hub move
                    long long wrong = 0, wrongW = 0;
                    double exc = 0;
                    for (int i = 0; i < Lc.n; ++i) {
                        uint8_t opt = label_of(Lc.x[i], Lc.y[i], fa, hy);
                        wrong += Lc.lab[i] != opt;
                        wrongW += Rc.lab[i] != opt;
                        float tx, ty, ox, oy;
                        dest(Lc.lab[i], hy, tx, ty);
                        dest(opt, hy, ox, oy);
                        double cf = (Lc.x[i] - tx) * (Lc.x[i] - tx) + (Lc.y[i] - ty) * (Lc.y[i] - ty);
                        double co = (Lc.x[i] - ox) * (Lc.x[i] - ox) + (Lc.y[i] - oy) * (Lc.y[i] - oy);
                        exc += cf - co;
                    }
                    q_frac = double(wrong) / Lc.n;
                    q_excess = exc / Lc.n;
                    if (wrongW) std::printf("  WARNING: WITH has %lld non-optimal labels\n", wrongW);
                }
            }
            ++f;
            if (f >= cyc_len && f % cyc_len == 0 && sumO >= min_ms && sumW >= min_ms) break;
            if (f >= 200 * cyc_len) break;  // sub-ms stages: cap at 200 cycles (25,000 frames)
        }
        ladder::Stage st;
        st.n = double(Lc.n);
        st.without.st = ladder::stats(tO);
        st.with.st = ladder::stats(tW);
        {  // WITHOUT has two kinds of frames: relabel (hub at rest) and stream-only (hub moved)
            std::vector<double> rel, str;
            for (size_t i = 0; i < tO.size(); ++i) (s_of(int(i), cyc_len) == 0 ? rel : str).push_back(tO[i]);
            st.without.extra = "\"median_ms_relabel_frames\": " + ladder::jnum(ladder::stats(rel).median) +
                               ", \"median_ms_frozen_frames\": " + ladder::jnum(ladder::stats(str).median);
        }
        st.extra = std::string("\"extension_past_requested_range\": ") + (Lc.n > 4000000 ? "true" : "false");
        st.quality = "{\"name\": \"agents routed sub-optimally at full hub move (a=0.45), sampled\", \"without\": " + ladder::jnum(q_frac) +
                     ", \"with\": 0, \"higher_is_better\": false, \"exact\": " + ladder::jnum(exact_changed) +
                     ", \"cost_excess_without_sampled\": " + ladder::jnum(q_excess) + ", \"cost_excess_with\": 0" +
                     ", \"cost_excess_exact\": " + ladder::jnum(exact_excess) + "}";
        L.stages.push_back(st);
        std::printf("n=%8d  WITHOUT %8.4f ms (p95 %.4f)  WITH %8.4f ms  ratio %.3f  frames %d | mis-routed %.5f (exact %.5f) excess %.6f (exact %.6f)\n",
                    Lc.n, st.without.st.median, st.without.st.p95, st.with.st.median, st.without.st.median / st.with.st.median,
                    st.with.st.frames, q_frac, exact_changed, q_excess, exact_excess);
        std::fflush(stdout);
    }
    L.notes =
        "Both sides are a single linear pass over the agents, so both scale ~n; neither is a solver race. WITHOUT's median is "
        "a frozen-label frame (stream only; 100 of every 125 frames), its p95 is a relabel frame (hub at rest) that costs about "
        "what WITH costs every frame. WITH = recompute the optimal power-cell labels every frame (exactly optimal by Lemma "
        "cells:optimality; any classical power-cell labeller gives the same labels). Quality is load-independent in exact terms "
        "(mis-routed fraction a/4 = 11.25%, cost excess a^2/12 at a=0.45); the sampled values converge to it as n grows. The "
        "paper's actual result (RMS map change ~ W2^(1/3), sharp) is a sensitivity warning about how many agents the optimal "
        "assignment reshuffles, not a speedup. Single thread on both sides; 1 frame = label pass + streaming every 4th agent.";
    L.write(out + "/ladder.json");
    L.print_table();
    std::printf("wrote %s/ladder.json\n", out.c_str());
    return 0;
}
