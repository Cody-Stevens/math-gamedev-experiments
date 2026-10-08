// 00-sample - harness self-test.
// The same "Particle Life" swarm (4,000 particles, 6 species, short-range forces on a torus)
// is stepped two ways:
//   LEFT  (WITHOUT): naive all-pairs neighbor search, n(n-1) distance checks per step.
//   RIGHT (WITH):    uniform-grid spatial hash, only the 3x3 surrounding cells are checked.
// Both use the exact same pair-force function and accumulate forces in 64-bit fixed point
// (integer addition is associative, so visiting neighbors in a different order cannot
// change the sum). The trajectories are therefore bit-identical; the HUD shows the
// deviation (should be exactly 0).
//
// Build + run:  ./build.sh 00-sample run      (or: preview)
#include "demo.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

using namespace demo;

constexpr int K = 6;  // species

struct Swarm {
    int n = 0;
    std::vector<float> x, y, vx, vy, ax, ay;
    std::vector<uint8_t> t;
    float A[K][K]{};
    float rmax = 0.06f, beta = 0.3f, dt = 0.02f, force = 10.f, friction = 0.f;
    double checks = 0, pairs = 0;  // last step
};

static inline float wrapd(float d) { return d > 0.5f ? d - 1.f : (d < -0.5f ? d + 1.f : d); }

static float g_rep = 6.f;  // strength of the short-range repulsive core
static inline float kernel(float r, float a, float beta) {
    if (r < beta) return g_rep * (r / beta - 1.f);
    return a * (1.f - std::fabs(2.f * r - 1.f - beta) / (1.f - beta));
}

// Order-independent force accumulator: 32.32 fixed point.
struct Acc {
    int64_t x = 0, y = 0;
    void add(float fx, float fy) {
        x += (int64_t)(fx * 4294967296.f);
        y += (int64_t)(fy * 4294967296.f);
    }
    float fx() const { return float(double(x) * (1.0 / 4294967296.0)); }
    float fy() const { return float(double(y) * (1.0 / 4294967296.0)); }
};

// One pair contribution (shared by both methods -> identical floating point).
static inline bool pair_force(const Swarm& s, float xi, float yi, int ti, float xj, float yj, int tj,
                              float& fx, float& fy) {
    float dx = wrapd(xj - xi), dy = wrapd(yj - yi);
    float r2 = dx * dx + dy * dy;
    if (r2 >= s.rmax * s.rmax || r2 <= 0.f) return false;
    float r = std::sqrt(r2);
    float f = kernel(r / s.rmax, s.A[ti][tj], s.beta) / r;
    fx = dx * f;
    fy = dy * f;
    return true;
}

static void integrate(Swarm& s) {
    float k = s.rmax * s.force * s.dt;
    for (int i = 0; i < s.n; ++i) {
        s.vx[i] = s.vx[i] * s.friction + s.ax[i] * k;
        s.vy[i] = s.vy[i] * s.friction + s.ay[i] * k;
        float nx = s.x[i] + s.vx[i] * s.dt, ny = s.y[i] + s.vy[i] * s.dt;
        nx -= std::floor(nx);
        ny -= std::floor(ny);
        s.x[i] = nx < 1.f ? nx : 0.f;
        s.y[i] = ny < 1.f ? ny : 0.f;
    }
}

// ---- WITHOUT: all pairs
static void step_naive(Swarm& s) {
    long long pairs = 0;
    for (int i = 0; i < s.n; ++i) {
        Acc acc;
        float fx, fy, xi = s.x[i], yi = s.y[i];
        int ti = s.t[i];
        for (int j = 0; j < s.n; ++j) {
            if (j == i) continue;
            if (pair_force(s, xi, yi, ti, s.x[j], s.y[j], s.t[j], fx, fy)) { acc.add(fx, fy); ++pairs; }
        }
        s.ax[i] = acc.fx();
        s.ay[i] = acc.fy();
    }
    s.checks = double(s.n) * (s.n - 1);
    s.pairs = double(pairs);
    integrate(s);
}

// ---- WITH: uniform grid spatial hash
struct Grid {
    int G = 0;
    std::vector<int> start, order, cell;
    std::vector<float> xs, ys;  // positions copied into cell order (cache-friendly scans)
    std::vector<uint8_t> ts;
};

static void step_grid(Swarm& s, Grid& g) {
    g.G = std::max(3, int(1.f / s.rmax));  // cell size >= rmax
    const int G = g.G, C = G * G;
    g.start.assign(C + 1, 0);
    g.order.resize(s.n);
    g.cell.resize(s.n);
    for (int i = 0; i < s.n; ++i) {
        int cx = std::min(G - 1, int(s.x[i] * G)), cy = std::min(G - 1, int(s.y[i] * G));
        g.cell[i] = cy * G + cx;
        g.start[g.cell[i] + 1]++;
    }
    for (int c = 0; c < C; ++c) g.start[c + 1] += g.start[c];
    {
        std::vector<int> fill(g.start.begin(), g.start.end() - 1);
        for (int i = 0; i < s.n; ++i) g.order[fill[g.cell[i]]++] = i;  // stable: index order per cell
    }
    g.xs.resize(s.n); g.ys.resize(s.n); g.ts.resize(s.n);
    for (int k = 0; k < s.n; ++k) { int i = g.order[k]; g.xs[k] = s.x[i]; g.ys[k] = s.y[i]; g.ts[k] = s.t[i]; }
    long long checks = 0, pairs = 0;
    for (int c0 = 0; c0 < C; ++c0) {
        int cx = c0 % G, cy = c0 / G;
        for (int a = g.start[c0]; a < g.start[c0 + 1]; ++a) {  // particle a (cell-sorted slot)
            float xi = g.xs[a], yi = g.ys[a];
            int ti = g.ts[a];
            Acc acc;
            for (int oy = -1; oy <= 1; ++oy)
                for (int ox = -1; ox <= 1; ++ox) {
                    int c = ((cy + oy + G) % G) * G + (cx + ox + G) % G;
                    int k0 = g.start[c], k1 = g.start[c + 1];
                    checks += k1 - k0;
                    for (int k = k0; k < k1; ++k) {
                        if (k == a) continue;
                        float fx, fy;
                        if (pair_force(s, xi, yi, ti, g.xs[k], g.ys[k], g.ts[k], fx, fy)) { acc.add(fx, fy); ++pairs; }
                    }
                }
            int i = g.order[a];
            s.ax[i] = acc.fx();
            s.ay[i] = acc.fy();
        }
    }
    checks -= s.n;  // self
    s.checks = double(checks);
    s.pairs = double(pairs);
    integrate(s);
}

static Swarm make_swarm(int n, unsigned seed) {
    Swarm s;
    s.n = n;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> U(0.f, 1.f), M(-1.f, 1.f);
    s.x.resize(n); s.y.resize(n); s.vx.assign(n, 0); s.vy.assign(n, 0);
    s.ax.assign(n, 0); s.ay.assign(n, 0); s.t.resize(n);
    for (int i = 0; i < n; ++i) { s.x[i] = U(rng); s.y[i] = U(rng); s.t[i] = uint8_t(i % K); }
    for (auto& row : s.A)
        for (auto& a : row) a = M(rng);
    s.friction = std::pow(0.5f, s.dt / 0.04f);
    return s;
}

static Color species_color(int k) {
    static const Color c[K] = {rgb(0x6cb6ff), rgb(0xf2777a), rgb(0x5be49b), rgb(0xffc861), rgb(0xb392f0), rgb(0x56d4dd)};
    return c[k];
}

static void draw_swarm(Panel& p, const Swarm& s, const Grid* g) {
    p.fade(pal::bg, 0.45f);
    View v(p, 0, 0, 1, 1, 26, false);
    Vec2 a = v(0, 0), b = v(1, 1);
    if (g && g->G > 0) {  // the spatial hash, drawn faintly
        for (int k = 1; k < g->G; ++k) {
            float f = float(k) / g->G;
            Vec2 q = v(f, 0), r = v(0, f);
            p.line(q.x, a.y, q.x, b.y, 1, pal::with, 0.07f);
            p.line(a.x, r.y, b.x, r.y, 1, pal::with, 0.07f);
        }
    }
    p.stroke_rect(a.x - 1, a.y - 1, b.x - a.x + 2, b.y - a.y + 2, 1, pal::grid);
    float gr = v.len(s.rmax) * 0.16f;
    for (int i = 0; i < s.n; ++i) {
        Vec2 q = v(s.x[i], s.y[i]);
        p.glow(q.x, q.y, gr, species_color(s.t[i]), 0.16f);
    }
    for (int i = 0; i < s.n; ++i) {
        Vec2 q = v(s.x[i], s.y[i]);
        p.circle(q.x, q.y, 1.9f, lerp(species_color(s.t[i]), rgb(0xffffff), 0.35f), 0.95f);
    }
}

int main(int argc, char** argv) {
    Config cfg;
    cfg.name = "00-sample";
    cfg.title_left = "naive all-pairs O(n²)";
    cfg.title_right = "uniform-grid spatial hash";
    cfg.caption = "00-sample · harness self-test · Particle Life, 4,000 particles, 6 species, torus · "
                  "same pair forces + order-free fixed-point sums ⇒ bit-identical trajectories";
    cfg.frames = 300;
    cfg.budget_ms = 16.7;
    Harness h(argc, argv, cfg);

    const int n = arg_int(argc, argv, "--n", 4000);
    g_rep = (float)arg_double(argc, argv, "--rep", g_rep);
    const unsigned seed = (unsigned)arg_int(argc, argv, "--seed", 7);
    Swarm L = make_swarm(n, seed);
    Grid gw;
    for (int i = 0; i < 300; ++i) step_grid(L, gw);  // pre-warm into structured state (untimed)
    Swarm R = L;
    Grid g;

    h.left().set_compute_label("sim");
    h.right().set_compute_label("sim");
    double max_dev = 0;
    while (h.next_frame()) {
        h.left().measure([&] { step_naive(L); });
        h.right().measure([&] { step_grid(R, g); });

        double dev = 0;  // outside the timers
        for (int i = 0; i < n; ++i)
            dev = std::max(dev, double(std::max(std::fabs(wrapd(L.x[i] - R.x[i])), std::fabs(wrapd(L.y[i] - R.y[i])))));
        max_dev = std::max(max_dev, dev);

        draw_swarm(h.left(), L, nullptr);
        draw_swarm(h.right(), R, &g);

        h.left().metric("particles", n, "si");
        h.left().metric("distance checks / step", L.checks, "si", Tone::Bad);
        h.left().metric("interacting pairs", L.pairs, "si");
        h.right().metric("particles", n, "si");
        h.right().metric("distance checks / step", R.checks, "si", Tone::Good);
        h.right().metric("interacting pairs", R.pairs, "si");
        h.right().metric("max |Δx| vs WITHOUT", dev, "%.3g", dev == 0 ? Tone::Good : Tone::Warn);
    }
    h.result("particles", n);
    h.result("species", K);
    h.result("rmax", L.rmax);
    h.result("grid_cells_per_side", g.G);
    h.result("max_position_deviation", max_dev);
    return h.finish();
}
