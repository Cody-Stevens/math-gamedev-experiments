// 07-periodic-blob - minimum-surface blob in a periodic (wrap-around) cube world.
//
// Family 354, "The Isoperimetric Conjecture for the Cubic Flat Three-Torus",
// preprints/The-Isoperimetric-Conjecture-for-the-Cubic-Flat-Three-Torus-September-24-2026/
// build/paper.tex, Theorem intro:main + eq. intro:profile:
//     I(V) = min{ (36 pi)^(1/3) v^(2/3), 2 sqrt(pi v), 2 },   v = min(V, 1-V)
// minimizers: ball (v <= 4pi/81), solid tube about a closed geodesic (4pi/81 <= v <= 1/pi),
// slab (v >= 1/pi), complements for V > 1/2. No other equality cases.
//
// LEFT  (WITHOUT): volume-preserving threshold dynamics (MBO / Ruuth-Wetton): diffuse the
//                  occupancy with a periodic Gaussian, keep exactly the k voxels with the
//                  largest value. Started from random smooth noise (re-seeded every few
//                  seconds), warm-started while the volume sweeps.
// RIGHT (WITH):    the certified global minimizer from Theorem intro:main in closed form,
//                  voxelized onto the same grid with the same exact voxel count k.
// Both outputs are scored with the same isotropic area estimator (sum |grad (G_1 * chi)|,
// coarea of the Gaussian-smoothed indicator) and compared with the exact I(V).
//
// Build + run:  ./build.sh 07-periodic-blob run      (or: preview)
#include "demo.h"
#include "sw3d.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include <vector>

using namespace demo;
using sw3d::V3;

static constexpr double PI = 3.14159265358979323846;
static const double T_BALL = 4 * PI / 81;  // ball <-> tube
static const double T_TUBE = 1 / PI;       // tube <-> slab

// ---------------------------------------------------------------- the theorem
static double ball_branch(double v) { return std::cbrt(36 * PI) * std::pow(v, 2.0 / 3.0); }
static double tube_branch(double v) { return 2 * std::sqrt(PI * v); }
static double I_unit(double V) {  // eq. intro:profile
    double v = std::min(V, 1 - V);
    if (v <= 0) return 0;
    return std::min({ball_branch(v), tube_branch(v), 2.0});
}
enum Phase { BALL = 0, TUBE = 1, SLAB = 2 };
static Phase phase_of(double V) {
    double v = std::min(V, 1 - V);
    return v < T_BALL ? BALL : (v < T_TUBE ? TUBE : SLAB);
}
static std::string phase_name(double V) {
    double v = std::min(V, 1 - V);
    const char* comp = V > 0.5 ? " complement" : "";
    if (std::fabs(v - T_BALL) < 0.0025) return std::string("ball = tube") + comp + " (tie)";
    if (std::fabs(v - T_TUBE) < 0.0025) return std::string("tube = slab") + (V > 0.5 ? " compl." : "") + " (tie)";
    static const char* nm[] = {"ball", "tube", "slab"};
    Phase p = phase_of(V);
    if (p == SLAB) return "slab";
    return std::string(nm[p]) + comp;
}

// ---------------------------------------------------------------- periodic grid tools
struct Blur {  // separable periodic Gaussian, sigma in voxels
    int N = 0, R = 0;
    std::vector<float> w;
    std::vector<int> m;  // m[q] = (q - R) mod N
    std::vector<float> A, B;
    Blur(int N_, double sigma) : N(N_) {
        R = std::max(1, (int)std::ceil(3 * sigma));
        R = std::min(R, N / 2 - 1);
        double s = 0;
        for (int t = -R; t <= R; ++t) { w.push_back((float)std::exp(-0.5 * t * t / (sigma * sigma))); s += w.back(); }
        for (auto& x : w) x = float(x / s);
        for (int q = 0; q < N + 2 * R; ++q) m.push_back(((q - R) % N + N) % N);
        A.resize((size_t)N * N * N); B.resize((size_t)N * N * N);
    }
    void apply(sw3d::Pool& pool, const float* in, float* out) {
        const int n = N, r2 = 2 * R;
        float* a = A.data(); float* b = B.data();
        const float* W = w.data(); const int* M = m.data();
        pool.for_range(n * n, 16, [&](int r0, int r1) {  // x: in -> A
            for (int row = r0; row < r1; ++row) {
                const float* src = in + (size_t)row * n; float* dst = a + (size_t)row * n;
                for (int i = 0; i < n; ++i) { float s = 0; for (int t = 0; t <= r2; ++t) s += W[t] * src[M[i + t]]; dst[i] = s; }
            }
        });
        pool.for_range(n * n, 16, [&](int r0, int r1) {  // y: A -> B
            for (int row = r0; row < r1; ++row) {
                int k = row / n, j = row % n;
                float* dst = b + (size_t)row * n;
                for (int i = 0; i < n; ++i) dst[i] = 0;
                for (int t = 0; t <= r2; ++t) {
                    const float* src = a + ((size_t)k * n + M[j + t]) * n; float wt = W[t];
                    for (int i = 0; i < n; ++i) dst[i] += wt * src[i];
                }
            }
        });
        pool.for_range(n * n, 16, [&](int r0, int r1) {  // z: B -> out
            for (int row = r0; row < r1; ++row) {
                int k = row / n, j = row % n;
                float* dst = out + (size_t)row * n;
                for (int i = 0; i < n; ++i) dst[i] = 0;
                for (int t = 0; t <= r2; ++t) {
                    const float* src = b + ((size_t)M[k + t] * n + j) * n; float wt = W[t];
                    for (int i = 0; i < n; ++i) dst[i] += wt * src[i];
                }
            }
        });
    }
};

// keep exactly the k voxels with the largest score (ties broken by index) - used by BOTH sides
struct TopK {
    std::vector<float> s;
    void run(sw3d::Pool& pool, const float* u, size_t n, size_t k, uint8_t* chi) {
        if (k == 0) { std::memset(chi, 0, n); return; }
        if (k >= n) { std::memset(chi, 1, n); return; }
        s.assign(u, u + n);
        std::nth_element(s.begin(), s.begin() + (n - k), s.end());
        const float t = s[n - k];
        const int rows = (int)(n / 4096);
        pool.for_range(rows, 4, [&](int a, int b) {
            for (size_t i = (size_t)a * 4096; i < (size_t)b * 4096; ++i) chi[i] = u[i] > t;
        });
        size_t cnt = 0;
        for (size_t i = 0; i < n; ++i) cnt += chi[i];
        for (size_t i = 0; i < n && cnt < k; ++i)
            if (u[i] == t && !chi[i]) { chi[i] = 1; ++cnt; }
    }
};

static void to_float(sw3d::Pool& pool, const uint8_t* chi, float* f, size_t n) {
    pool.for_range((int)(n / 4096), 4, [&](int a, int b) {
        for (size_t i = (size_t)a * 4096; i < (size_t)b * 4096; ++i) f[i] = chi[i];
    });
}

// ---------------------------------------------------------------- WITHOUT: volume-preserving MBO
struct MBO {
    int N; size_t n;
    std::vector<uint8_t> chi;
    std::vector<float> f, u;
    Blur blur, noise_blur;
    TopK sel;
    long long iters = 0;
    MBO(int N_, double sigma) : N(N_), n((size_t)N_ * N_ * N_), chi(n), f(n), u(n), blur(N_, sigma), noise_blur(N_, 3.5) {}
    void init_noise(sw3d::Pool& pool, unsigned seed, size_t k) {
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> U(0.f, 1.f);
        for (auto& x : f) x = U(rng);
        noise_blur.apply(pool, f.data(), u.data());
        sel.run(pool, u.data(), n, k, chi.data());
        iters = 0;
    }
    void step(sw3d::Pool& pool, size_t k) {
        to_float(pool, chi.data(), f.data(), n);
        blur.apply(pool, f.data(), u.data());
        sel.run(pool, u.data(), n, k, chi.data());
        ++iters;
    }
};

// ---------------------------------------------------------------- WITH: closed-form certified minimizer
static inline double wrap1(double d) { return d - std::floor(d + 0.5); }  // to [-0.5, 0.5)
struct ClosedForm {
    int N; size_t n;
    std::vector<uint8_t> chi;
    std::vector<float> s;
    TopK sel;
    Phase ph = BALL; bool comp = false; double radius = 0;
    ClosedForm(int N_) : N(N_), n((size_t)N_ * N_ * N_), chi(n), s(n) {}
    // signed distance of the minimizer of volume v <= 1/2 (centered at 0.5, tube along y, slab normal x)
    static double sdf(Phase p, double r, double x, double y, double z) {
        double dx = wrap1(x - 0.5), dy = wrap1(y - 0.5), dz = wrap1(z - 0.5);
        if (p == BALL) return std::sqrt(dx * dx + dy * dy + dz * dz) - r;
        if (p == TUBE) return std::sqrt(dx * dx + dz * dz) - r;
        return std::fabs(dx) - r;  // slab half-width
    }
    void build(sw3d::Pool& pool, double V, size_t k) {
        comp = V > 0.5;
        double v = comp ? 1 - V : V;
        ph = phase_of(V);
        radius = ph == BALL ? std::cbrt(3 * v / (4 * PI)) : ph == TUBE ? std::sqrt(v / PI) : 0.5 * v;
        const double h = 1.0 / N; const Phase p = ph; const double r = radius; const bool c = comp;
        pool.for_range(N * N, 16, [&](int r0, int r1) {
            for (int row = r0; row < r1; ++row) {
                int kk = row / N, j = row % N;
                float* dst = s.data() + (size_t)row * N;
                for (int i = 0; i < N; ++i) {
                    double d = sdf(p, r, (i + 0.5) * h, (j + 0.5) * h, (kk + 0.5) * h);
                    // score: larger = more inside (exact ties, e.g. a partially filled slab layer, are
                    // filled in index order, which gives a straight one-voxel step - the cheapest option)
                    dst[i] = float(c ? d : -d);
                }
            }
        });
        sel.run(pool, s.data(), n, k, chi.data());  // exact same voxel count as the solver
    }
};

// ---------------------------------------------------------------- shared measurement (untimed)
struct Analysis {
    int N; size_t n;
    Blur b1, brender;
    std::vector<float> f, phi, rphi, D;
    std::vector<double> rowsum;
    Analysis(int N_) : N(N_), n((size_t)N_ * N_ * N_), b1(N_, 1.0), brender(N_, 1.5), f(n), phi(n), rphi(n), D(n), rowsum((size_t)N_ * N_) {}
    // isotropic area estimator: integral of |grad (G_sigma * chi)| (coarea of the smoothed indicator)
    double area(sw3d::Pool& pool, const uint8_t* chi) {
        to_float(pool, chi, f.data(), n);
        b1.apply(pool, f.data(), phi.data());
        const int Nn = N; const float* P = phi.data();
        pool.for_range(N * N, 16, [&](int r0, int r1) {
            for (int row = r0; row < r1; ++row) {
                int k = row / Nn, j = row % Nn;
                int jp = (j + 1) % Nn, jm = (j + Nn - 1) % Nn, kp = (k + 1) % Nn, km = (k + Nn - 1) % Nn;
                double acc = 0;
                for (int i = 0; i < Nn; ++i) {
                    int ip = (i + 1) % Nn, im = (i + Nn - 1) % Nn;
                    auto at = [&](int a, int b, int c) { return P[((size_t)c * Nn + b) * Nn + a]; };
                    double gx = at(ip, j, k) - at(im, j, k), gy = at(i, jp, k) - at(i, jm, k), gz = at(i, j, kp) - at(i, j, km);
                    acc += std::sqrt(gx * gx + gy * gy + gz * gz);
                }
                rowsum[row] = acc;
            }
        });
        double s = 0;
        for (double x : rowsum) s += x;
        double h = 1.0 / N;
        return s * 0.5 * h * h;  // (|diff|/2h) * h^3
    }
    // periodic Euclidean distance (voxels) to the interface voxels, for sphere tracing
    void distance(sw3d::Pool& pool, const uint8_t* chi) {
        const int Nn = N;
        const double INF = 1e9;
        std::vector<double> g(n);
        pool.for_range(N * N, 16, [&](int r0, int r1) {
            for (int row = r0; row < r1; ++row) {
                int k = row / Nn, j = row % Nn;
                for (int i = 0; i < Nn; ++i) {
                    auto at = [&](int a, int b, int c) { return chi[((size_t)((c + Nn) % Nn) * Nn + (b + Nn) % Nn) * Nn + (a + Nn) % Nn]; };
                    uint8_t c0 = at(i, j, k);
                    bool iface = at(i + 1, j, k) != c0 || at(i - 1, j, k) != c0 || at(i, j + 1, k) != c0 ||
                                 at(i, j - 1, k) != c0 || at(i, j, k + 1) != c0 || at(i, j, k - 1) != c0;
                    g[(size_t)row * Nn + i] = iface ? 0 : INF;
                }
            }
        });
        for (int axis = 0; axis < 3; ++axis) {
            pool.for_range(N * N, 8, [&](int r0, int r1) {
                const int L = 3 * Nn;
                std::vector<double> fl(L), dl(L), z(L + 1);
                std::vector<int> v(L);
                for (int line = r0; line < r1; ++line) {
                    int a = line / Nn, b = line % Nn;
                    auto idx = [&](int q) -> size_t {
                        if (axis == 0) return ((size_t)a * Nn + b) * Nn + q;
                        if (axis == 1) return ((size_t)a * Nn + q) * Nn + b;
                        return ((size_t)q * Nn + a) * Nn + b;
                    };
                    for (int q = 0; q < L; ++q) fl[q] = g[idx(q % Nn)];
                    int kk = 0; v[0] = 0; z[0] = -1e300; z[1] = 1e300;
                    auto isect = [&](int q, int p) { return ((fl[q] + (double)q * q) - (fl[p] + (double)p * p)) / (2.0 * q - 2.0 * p); };
                    for (int q = 1; q < L; ++q) {
                        double s = isect(q, v[kk]);
                        while (s <= z[kk]) { --kk; s = isect(q, v[kk]); }
                        ++kk; v[kk] = q; z[kk] = s; z[kk + 1] = 1e300;
                    }
                    kk = 0;
                    for (int q = 0; q < L; ++q) {
                        while (z[kk + 1] < q) ++kk;
                        double d = q - v[kk];
                        dl[q] = d * d + fl[v[kk]];
                    }
                    for (int q = 0; q < Nn; ++q) g[idx(q)] = dl[Nn + q];
                }
            });
        }
        for (size_t i = 0; i < n; ++i) D[i] = (float)std::sqrt(std::min(g[i], 1e6));
    }
};

// shape classification of a voxel set: wrapped axes + periodic connected components
static int wraps(const uint8_t* chi, int N, uint8_t val) {
    int w = 0;
    for (int axis = 0; axis < 3; ++axis) {
        bool any = false;
        for (int a = 0; a < N && !any; ++a)
            for (int b = 0; b < N && !any; ++b) {
                bool all = true;
                for (int q = 0; q < N && all; ++q) {
                    size_t id = axis == 0 ? ((size_t)a * N + b) * N + q : axis == 1 ? ((size_t)a * N + q) * N + b : ((size_t)q * N + a) * N + b;
                    all = chi[id] == val;
                }
                any = all;
            }
        w += any;
    }
    return w;
}
static int components(const uint8_t* chi, int N, uint8_t val, int min_size = 8) {
    size_t n = (size_t)N * N * N;
    std::vector<int> lab(n, -1), stack;
    int comps = 0;
    for (size_t s = 0; s < n; ++s) {
        if (chi[s] != val || lab[s] >= 0) continue;
        int size = 0;
        stack.push_back((int)s); lab[s] = comps;
        while (!stack.empty()) {
            int id = stack.back(); stack.pop_back(); ++size;
            int i = id % N, j = (id / N) % N, k = id / (N * N);
            int nb[6] = {((k * N + j) * N + (i + 1) % N), ((k * N + j) * N + (i + N - 1) % N),
                         ((k * N + (j + 1) % N) * N + i), ((k * N + (j + N - 1) % N) * N + i),
                         ((((k + 1) % N) * N + j) * N + i), ((((k + N - 1) % N) * N + j) * N + i)};
            for (int q : nb)
                if (chi[q] == val && lab[q] < 0) { lab[q] = comps; stack.push_back(q); }
        }
        if (size >= min_size) ++comps;
    }
    return comps;
}
struct ShapeInfo { std::string name; int phase = -1; bool comp = false; };
static ShapeInfo classify(const uint8_t* chi, int N) {
    int w1 = wraps(chi, N, 1), w0 = wraps(chi, N, 0);
    int c1 = components(chi, N, 1), c0 = components(chi, N, 0);
    ShapeInfo s;
    auto cnt = [](int c, const char* one, const char* many) { return c == 1 ? std::string(one) : fmt("%d %s", c, many); };
    if (w1 == 0 && w0 == 3) { s.name = cnt(c1, "ball", "blobs"); s.phase = c1 == 1 ? BALL : -1; }
    else if (w1 == 1 && w0 == 3) { s.name = cnt(c1, "tube", "tubes"); s.phase = c1 == 1 ? TUBE : -1; }
    else if (w1 == 2 && w0 == 2) { s.name = cnt(c1, "slab", "slabs"); s.phase = c1 == 1 ? SLAB : -1; }
    else if (w1 == 3 && w0 == 1) { s.name = c0 == 1 ? "tube complement" : fmt("%d tunnels", c0); s.phase = c0 == 1 ? TUBE : -1; s.comp = true; }
    else if (w1 == 3 && w0 == 0) { s.name = c0 == 1 ? "ball complement" : fmt("%d holes", c0); s.phase = c0 == 1 ? BALL : -1; s.comp = true; }
    else s.name = fmt("other (wraps %d/%d axes)", w1, w0);
    return s;
}

// ---------------------------------------------------------------- rendering (untimed)
struct Renderer {
    int N;
    sw3d::Camera cam;
    int rx = 0, ry = 0, rw = 960, rh = 700;  // render rect inside the panel
    static inline float tri(const float* a, int N, float x, float y, float z) {  // voxel coords
        float fx = std::floor(x), fy = std::floor(y), fz = std::floor(z);
        int i0 = (int)fx, j0 = (int)fy, k0 = (int)fz;
        float tx = x - fx, ty = y - fy, tz = z - fz;
        i0 = ((i0 % N) + N) % N; j0 = ((j0 % N) + N) % N; k0 = ((k0 % N) + N) % N;
        int i1 = (i0 + 1) % N, j1 = (j0 + 1) % N, k1 = (k0 + 1) % N;
        auto A = [&](int i, int j, int k) { return a[((size_t)k * N + j) * N + i]; };
        float c00 = A(i0, j0, k0) + (A(i1, j0, k0) - A(i0, j0, k0)) * tx;
        float c10 = A(i0, j1, k0) + (A(i1, j1, k0) - A(i0, j1, k0)) * tx;
        float c01 = A(i0, j0, k1) + (A(i1, j0, k1) - A(i0, j0, k1)) * tx;
        float c11 = A(i0, j1, k1) + (A(i1, j1, k1) - A(i0, j1, k1)) * tx;
        float c0 = c00 + (c10 - c00) * ty, c1 = c01 + (c11 - c01) * ty;
        return c0 + (c1 - c0) * tz;
    }
    void background(Panel& p) {
        for (int y = ry; y < ry + rh; ++y) {
            float t = float(y - ry) / rh;
            Color c = lerp(rgb(0x141a26), pal::bg, std::sqrt(t));
            p.fill_rect(0, (float)y, (float)p.width(), 1, c);
        }
        // 2x2x2 lattice of unit cells
        for (int axis = 0; axis < 3; ++axis)
            for (int a = 0; a <= 2; ++a)
                for (int b = 0; b <= 2; ++b) {
                    V3 p0, p1;
                    double c[3];
                    c[axis] = 0; c[(axis + 1) % 3] = a; c[(axis + 2) % 3] = b; p0 = {c[0], c[1], c[2]};
                    c[axis] = 2; p1 = {c[0], c[1], c[2]};
                    double x0, y0, x1, y1;
                    cam.project(p0, x0, y0); cam.project(p1, x1, y1);
                    p.line((float)x0, (float)y0, (float)x1, (float)y1, 1.2f, pal::dim, 0.22f);
                }
        // the fundamental cell [0,1]^3 brighter
        for (int axis = 0; axis < 3; ++axis)
            for (int a = 0; a <= 1; ++a)
                for (int b = 0; b <= 1; ++b) {
                    double c[3];
                    c[axis] = 0; c[(axis + 1) % 3] = a; c[(axis + 2) % 3] = b; V3 p0{c[0], c[1], c[2]};
                    c[axis] = 1; V3 p1{c[0], c[1], c[2]};
                    double x0, y0, x1, y1;
                    cam.project(p0, x0, y0); cam.project(p1, x1, y1);
                    p.line((float)x0, (float)y0, (float)x1, (float)y1, 1.6f, pal::warn, 0.35f);
                }
    }
    void render(sw3d::Pool& pool, Panel& p, const float* phi, const float* D, Color base) {
        uint8_t* px = p.data(); const int stride = p.stride();
        const float Nf = (float)N, h = 1.f / N;
        const V3 L = sw3d::norm(V3{-0.45, 0.8, 0.35});
        pool.for_range(rh, 2, [&](int y0, int y1) {
            for (int y = ry + y0; y < ry + y1; ++y) {
                uint8_t* row = px + (size_t)y * stride;
                for (int x = rx; x < rx + rw; ++x) {
                    V3 rd = cam.ray(x + 0.5, y + 0.5), ro = cam.eye;
                    // slab test against [0,2]^3
                    double t0 = 0, t1 = 1e9;
                    double o[3] = {ro.x, ro.y, ro.z}, d[3] = {rd.x, rd.y, rd.z};
                    bool miss = false;
                    for (int a = 0; a < 3; ++a) {
                        double inv = 1.0 / d[a];
                        double ta = (0 - o[a]) * inv, tb = (2 - o[a]) * inv;
                        if (ta > tb) std::swap(ta, tb);
                        t0 = std::max(t0, ta); t1 = std::min(t1, tb);
                    }
                    if (t1 <= t0) miss = true;
                    if (miss) continue;
                    auto sphi = [&](double t) {
                        return tri(phi, N, float((o[0] + d[0] * t) * Nf - 0.5f), float((o[1] + d[1] * t) * Nf - 0.5f),
                                   float((o[2] + d[2] * t) * Nf - 0.5f)) - 0.5f;
                    };
                    auto sD = [&](double t) {
                        return tri(D, N, float((o[0] + d[0] * t) * Nf - 0.5f), float((o[1] + d[1] * t) * Nf - 0.5f),
                                   float((o[2] + d[2] * t) * Nf - 0.5f));
                    };
                    float Cr = 0, Cg = 0, Cb = 0, T = 1;
                    double t = t0 + 1e-4;
                    float val = sphi(t);
                    int hits = 0;
                    const float kabs = 0.9f;
                    while (t < t1 && T > 0.025f && hits < 10) {
                        float dv = sD(t);
                        double step = dv > 2.4f ? (dv - 1.8f) * h : 0.4f * h;
                        double tn = std::min(t + step, t1);
                        float vn = sphi(tn);
                        bool inside = val > 0;
                        if ((vn > 0) != inside) {
                            double a = t, b = tn; float va = val;
                            for (int it = 0; it < 7; ++it) {
                                double m = 0.5 * (a + b); float vm = sphi(m);
                                if ((vm > 0) == (va > 0)) { a = m; va = vm; } else b = m;
                            }
                            double th = 0.5 * (a + b);
                            if (inside) {  // absorption inside the solid up to the hit
                                float tr = std::exp(-kabs * float(th - t));
                                float e = T * (1 - tr) * 0.55f;
                                Cr += e * base.r; Cg += e * base.g; Cb += e * base.b; T *= tr;
                            }
                            V3 ph = ro + rd * th;
                            float gx = 0, gy = 0, gz = 0, e = 0.6f;
                            float qx = float(ph.x * Nf - 0.5), qy = float(ph.y * Nf - 0.5), qz = float(ph.z * Nf - 0.5);
                            gx = tri(phi, N, qx + e, qy, qz) - tri(phi, N, qx - e, qy, qz);
                            gy = tri(phi, N, qx, qy + e, qz) - tri(phi, N, qx, qy - e, qz);
                            gz = tri(phi, N, qx, qy, qz + e) - tri(phi, N, qx, qy, qz - e);
                            V3 nout = sw3d::norm(V3{-gx, -gy, -gz});
                            bool entering = sw3d::dot(rd, nout) < 0;
                            V3 nf = entering ? nout : -nout;
                            float cosv = (float)std::max(0.0, -sw3d::dot(rd, nf));
                            float diff = (float)std::max(0.0, sw3d::dot(nf, L));
                            float hemi = float(0.5 + 0.5 * nf.y);
                            V3 H = sw3d::norm(L - rd);
                            float spec = std::pow((float)std::max(0.0, sw3d::dot(nf, H)), 60.f);
                            float fres = std::pow(1 - cosv, 3.f);
                            float amb = 0.12f + 0.22f * hemi, kd = 0.7f * diff;
                            float shade = entering ? 1.f : 0.55f;
                            float cr = (base.r * (amb + kd) * shade + 0.9f * spec + 0.55f * fres * 0.75f);
                            float cg = (base.g * (amb + kd) * shade + 0.9f * spec + 0.55f * fres * 0.85f);
                            float cb = (base.b * (amb + kd) * shade + 0.9f * spec + 0.55f * fres * 1.0f);
                            float alpha = entering ? 0.42f + 0.5f * fres : 0.22f + 0.4f * fres;
                            Cr += T * alpha * cr; Cg += T * alpha * cg; Cb += T * alpha * cb;
                            T *= 1 - alpha;
                            ++hits;
                            t = th + 0.05 * h;
                            val = sphi(t);
                            if ((val > 0) == inside) val = inside ? -1e-4f : 1e-4f;  // force the flip
                            continue;
                        }
                        if (inside) {
                            float tr = std::exp(-kabs * float(tn - t));
                            float e = T * (1 - tr) * 0.55f;
                            Cr += e * base.r; Cg += e * base.g; Cb += e * base.b; T *= tr;
                        }
                        t = tn; val = vn;
                        if (tn >= t1) break;
                    }
                    uint8_t* q = row + x * 3;
                    float r = Cr + T * q[0] / 255.f, g = Cg + T * q[1] / 255.f, b = Cb + T * q[2] / 255.f;
                    q[0] = (uint8_t)std::clamp(r * 255.f + 0.5f, 0.f, 255.f);
                    q[1] = (uint8_t)std::clamp(g * 255.f + 0.5f, 0.f, 255.f);
                    q[2] = (uint8_t)std::clamp(b * 255.f + 0.5f, 0.f, 255.f);
                }
            }
        });
    }
};

// profile plot: exact I(V), the three branches, thresholds, and this panel's measured trail
struct Trail { std::vector<float> V, A; };
static void draw_profile(Panel& p, const Trail& tr, double Vnow, double Anow, Color accent, bool is_with) {
    const float x0 = 70, x1 = 935, y0 = 718, y1 = 925;
    p.fill_rounded_rect(x0 - 50, y0 - 18, x1 - x0 + 70, y1 - y0 + 50, 10, rgb(0x0f131b), 0.92f);
    auto X = [&](double V) { return float(x0 + (x1 - x0) * V); };
    const double Amax = 2.3;
    auto Y = [&](double A) { return float(y1 - (y1 - y0) * std::min(A, Amax + 0.2) / Amax); };
    for (double a : {0.0, 1.0, 2.0}) {
        p.line(x0, Y(a), x1, Y(a), 1, pal::grid);
        p.text(x0 - 10, Y(a) - 8, fmt("%.0f", a), 13, pal::dim, Font::Mono, Align::Right);
    }
    const double th[4] = {T_BALL, T_TUBE, 1 - T_TUBE, 1 - T_BALL};
    for (double t : th)
        for (float yy = y0; yy < y1; yy += 8) p.line(X(t), yy, X(t), yy + 4, 1, pal::dim, 0.5f);
    const char* reg[5] = {"ball", "tube", "slab", "tube°", "ball°"};
    const double edges[6] = {0, T_BALL, T_TUBE, 1 - T_TUBE, 1 - T_BALL, 1};
    for (int i = 0; i < 5; ++i)
        p.text((X(edges[i]) + X(edges[i + 1])) / 2, y0 - 16, reg[i], 13, pal::dim, Font::Sans, Align::Center);
    // branches (dim) and the certified profile (bright)
    std::vector<Vec2> pb, pt, pi;
    for (int i = 1; i < 400; ++i) {
        double V = i / 400.0, v = std::min(V, 1 - V);
        pb.push_back({X(V), Y(ball_branch(v))});
        pt.push_back({X(V), Y(tube_branch(v))});
        pi.push_back({X(V), Y(I_unit(V))});
    }
    p.polyline(pb.data(), (int)pb.size(), 1.2f, pal::violet, 0.45f);
    p.polyline(pt.data(), (int)pt.size(), 1.2f, pal::blue, 0.45f);
    p.line(X(0), Y(2), X(1), Y(2), 1.2f, pal::orange, 0.45f);
    p.polyline(pi.data(), (int)pi.size(), 2.6f, pal::text, 0.95f);
    // measured trail
    for (size_t i = 0; i < tr.V.size(); ++i) p.circle(X(tr.V[i]), Y(tr.A[i]), 1.6f, accent, 0.55f);
    p.glow(X(Vnow), Y(Anow), 16, accent, 0.9f);
    p.circle(X(Vnow), Y(Anow), 4.5f, lerp(accent, rgb(0xffffff), 0.4f));
    p.text(x0, y1 + 6, "V  (occupied volume fraction)", 13, pal::dim, Font::Sans);
    p.text(x1, y1 + 6,
           is_with ? "white: exact I(V), eq. intro:profile · dots: closed form scored by the area estimator"
                   : "white: exact I(V), eq. intro:profile · dots: MBO result scored by the same estimator",
           13, pal::dim, Font::Sans, Align::Right);
    p.text(x0 + 6, y0 - 2, "area", 13, pal::dim, Font::Sans);
}

// ---------------------------------------------------------------- grid refinement + multi-start study
static void refinement_check(sw3d::Pool& pool, Harness& h, bool print) {
    const double Vs[4] = {0.03, 0.10, 0.25, 0.45};
    for (int N : {32, 48, 64, 96, 128}) {
        ClosedForm cf(N);
        Analysis an(N);
        size_t n = (size_t)N * N * N;
        std::string line = fmt("[refine] N=%3d", N);
        for (double V : Vs) {
            size_t k = (size_t)std::llround(V * n);
            double Vq = double(k) / n;
            cf.build(pool, Vq, k);
            double A = an.area(pool, cf.chi.data());
            double bias = A / I_unit(Vq) - 1;
            h.result(fmt("refine_bias_N%d_V%.2f", N, V), bias);
            line += fmt("   V=%.2f bias %+.3f%%", V, 100 * bias);
        }
        if (print) std::printf("%s\n", line.c_str());
    }
}

static void multistart_study(sw3d::Pool& pool, Harness& h, int N, double sigma, int iters, int seeds) {
    const double Vs[6] = {0.05, 0.12, 0.25, 0.40, 0.70, 0.88};
    size_t n = (size_t)N * N * N;
    Analysis an(N);
    ClosedForm cf(N);
    int global_hits = 0, global_runs = 0;
    for (double V : Vs) {
        size_t k = (size_t)std::llround(V * n);
        double Vq = double(k) / n;
        cf.build(pool, Vq, k);
        double Aref = an.area(pool, cf.chi.data());
        double best = 1e9, worst = -1e9, sum_ms_hit = 0;
        int hit = 0;
        std::string shapes;
        for (int s = 0; s < seeds; ++s) {
            MBO m(N, sigma);
            m.init_noise(pool, 9000 + 97 * s + (unsigned)(V * 1000), k);
            double ms = 0, ms_hit = -1;
            for (int it = 0; it < iters; ++it) {
                double t0 = now_ms();
                m.step(pool, k);
                ms += now_ms() - t0;
                if (ms_hit < 0 && (it % 4 == 3)) {
                    double ex = an.area(pool, m.chi.data()) / Aref - 1;
                    if (ex <= 0.02) ms_hit = ms;
                }
            }
            double ex = an.area(pool, m.chi.data()) / Aref - 1;
            best = std::min(best, ex); worst = std::max(worst, ex);
            if (ms_hit >= 0) { ++hit; sum_ms_hit += ms_hit; }
            ShapeInfo si = classify(m.chi.data(), N);
            shapes += (s ? ", " : "") + si.name;
            ++global_runs; global_hits += ex <= 0.02;
        }
        std::printf("[study] V=%.2f (%s): %d/%d starts within 2%% of certified area in %d iters; excess best %+.2f%% worst %+.2f%%; mean ms-to-2%% %.1f; shapes: %s\n",
                    V, phase_name(V).c_str(), hit, seeds, iters, 100 * best, 100 * worst, hit ? sum_ms_hit / hit : -1.0, shapes.c_str());
        h.result(fmt("study_V%.2f_hits", V), hit);
        h.result(fmt("study_V%.2f_best_excess", V), best);
        h.result(fmt("study_V%.2f_worst_excess", V), worst);
        h.result(fmt("study_V%.2f_mean_ms_to_2pct", V), hit ? sum_ms_hit / hit : -1.0);
        h.result(fmt("study_V%.2f_shapes", V), shapes);
    }
    h.result("study_runs", global_runs);
    h.result("study_runs_within_2pct", global_hits);
    h.result("study_iters_per_run", iters);
}

// ---------------------------------------------------------------- main
int main(int argc, char** argv) {
    Config cfg;
    cfg.name = "07-periodic-blob";
    cfg.title_left = "volume-preserving MBO relaxation (random starts)";
    cfg.title_right = "certified minimizer, closed form (Thm intro:main)";
    cfg.caption = "Family 354 — The Isoperimetric Conjecture for the Cubic Flat Three-Torus · WITH uses Theorem intro:main "
                  "(ball/tube/slab at 4π/81, 1/π) + exact profile eq. intro:profile as the target · shapes were "
                  "conjectured before; the paper certifies them as the global optimum";
    cfg.frames = 1500;
    cfg.show_speedup = false;  // the two sides do different jobs (iterate vs evaluate); ms are shown, no ratio
    Harness h(argc, argv, cfg);

    const int N = arg_int(argc, argv, "--n", 64);
    const double sigma = arg_double(argc, argv, "--sigma", 2.5);
    const int steps = arg_int(argc, argv, "--mbo-steps", 3);
    const int restart_every = arg_int(argc, argv, "--restart", 240);
    const double vfix = arg_double(argc, argv, "--vfix", -1);
    const unsigned seed = (unsigned)arg_int(argc, argv, "--seed", 354);
    const int threads = arg_int(argc, argv, "--threads", hw_threads());
    sw3d::Pool pool(threads);

    if (arg_flag(argc, argv, "--refine") || (!h.preview() && !arg_flag(argc, argv, "--no-refine")))
        refinement_check(pool, h, !h.quiet());
    if (arg_flag(argc, argv, "--study"))
        multistart_study(pool, h, N, sigma, arg_int(argc, argv, "--study-iters", 240), arg_int(argc, argv, "--study-seeds", 6));

    const size_t n = (size_t)N * N * N;
    MBO mbo(N, sigma);
    ClosedForm cf(N);
    Analysis anL(N), anR(N);
    Renderer rend{N};
    Trail trL, trR;

    auto volume_at = [&](int f) {
        if (vfix > 0) return vfix;
        const int F = h.frames(), hold = std::max(1, F * 6 / 100);
        double s = std::clamp(double(f - hold) / std::max(1, F - 2 * hold), 0.0, 1.0);
        return 0.03 + 0.94 * s;
    };

    h.left().set_compute_label("relax");
    h.right().set_compute_label("closed form");
    h.left().set_steps_per_frame(steps);
    h.left().sparkline("A/I(V)−1 %");
    h.right().sparkline("A/I(V)−1 %");

    int restarts = 0, last_restart_frame = -1000;
    const int still_every = arg_int(argc, argv, "--still-every", 0);
    double ms_since = 0, ms_hit = -1;
    long long it_hit = -1;
    double max_exL = -1e9, max_exR = -1e9, min_exR = 1e9, sum_exL = 0, sum_exR = 0;
    int nfr = 0, match_frames = 0;
    std::string lastL, lastR;
    while (h.next_frame()) {
        const int f = h.frame();
        const double Vtarget = volume_at(f);
        const size_t k = (size_t)std::llround(Vtarget * n);
        const double V = double(k) / n;

        // ---- WITHOUT: relaxation (timed)
        if (f == 0 || (restart_every > 0 && f % restart_every == 0)) {
            h.left().measure([&] { mbo.init_noise(pool, seed + 7919u * restarts, k); });
            ++restarts; ms_since = 0; ms_hit = -1; it_hit = -1; last_restart_frame = f;
        }
        {
            double t0 = now_ms();
            h.left().measure([&] { for (int s = 0; s < steps; ++s) mbo.step(pool, k); });
            ms_since += now_ms() - t0;
        }
        // ---- WITH: closed-form certified minimizer (timed)
        h.right().measure([&] { cf.build(pool, V, k); });

        // ---- shared measurement (untimed)
        const double I = I_unit(V);
        const double AL = anL.area(pool, mbo.chi.data()), AR = anR.area(pool, cf.chi.data());
        const double exL = AL / I - 1, exR = AR / I - 1, exLR = AL / AR - 1;
        if (ms_hit < 0 && exLR <= 0.02) { ms_hit = ms_since; it_hit = mbo.iters; }
        ShapeInfo sL = classify(mbo.chi.data(), N);
        const std::string certified = phase_name(V);
        bool match = false;
        {
            const double v = std::min(V, 1 - V);
            const bool compOK = sL.phase == SLAB || sL.comp == (V > 0.5);
            int ph = (int)phase_of(V);
            match = compOK && (sL.phase == ph || (std::fabs(v - T_BALL) < 0.0025 && (sL.phase == BALL || sL.phase == TUBE)) ||
                               (std::fabs(v - T_TUBE) < 0.0025 && (sL.phase == TUBE || sL.phase == SLAB)));
        }
        ++nfr; match_frames += match;
        sum_exL += exL; sum_exR += exR;
        if (f > 10) { max_exL = std::max(max_exL, exL); }
        max_exR = std::max(max_exR, exR); min_exR = std::min(min_exR, exR);
        trL.V.push_back((float)V); trL.A.push_back((float)AL);
        trR.V.push_back((float)V); trR.A.push_back((float)AR);
        lastL = sL.name; lastR = certified;

        // ---- draw
        const double tsec = f / h.fps();
        rend.cam.orbit(V3{1, 1, 1}, 0.55 + 0.16 * tsec, 0.40, 5.3, 40 * PI / 180, 700, 585, 395);
        for (int side = 0; side < 2; ++side) {
            Panel& p = side ? h.right() : h.left();
            Analysis& an = side ? anR : anL;
            const uint8_t* chi = side ? cf.chi.data() : mbo.chi.data();
            p.clear(pal::bg);
            rend.background(p);
            an.distance(pool, chi);
            to_float(pool, chi, an.f.data(), n);
            an.brender.apply(pool, an.f.data(), an.rphi.data());  // smoother field for shading only
            rend.render(pool, p, an.rphi.data(), an.D.data(), side ? pal::with : pal::without);
            p.text(945, 14, "2×2×2 periodic copies · yellow = one unit cell", 14, pal::dim, Font::Sans, Align::Right);
            draw_profile(p, side ? trR : trL, V, side ? AR : AL, side ? pal::with : pal::without, side == 1);
        }
        if (f - last_restart_frame < 75)
            h.left().text(950, 620, fmt("random restart #%d (new noise seed)", restarts), 18, pal::warn, Font::Bold, Align::Right);
        if (still_every > 0 && f % still_every == still_every / 2) h.save_still(fmt("still_%04d.png", f));

        // ---- HUD
        Panel& L = h.left();
        Panel& R = h.right();
        L.metric("volume V", V, "%.4f");
        L.metric("area (isotropic estimate)", AL, "%.4f");
        L.metric("A/I(V)−1 %", 100 * exL, "%+.2f", exL > 0.03 ? Tone::Bad : (exL > 0.01 ? Tone::Warn : Tone::Good));
        L.metric_text("shape", sL.name + (match ? " ✓" : " ✗"), match ? Tone::Good : Tone::Bad);
        L.metric_text("first ≤2% since restart",
                      ms_hit >= 0 ? fmt("%.0f ms (%lld it)", ms_hit, it_hit) : fmt("not yet, %.0f ms", ms_since),
                      ms_hit >= 0 ? Tone::Good : Tone::Warn);
        L.metric("excess vs WITH (same est.) %", 100 * exLR, "%+.2f", exLR > 0.02 ? Tone::Bad : Tone::Good);

        R.metric("volume V", V, "%.4f");
        R.metric("area (isotropic estimate)", AR, "%.4f");
        R.metric("A/I(V)−1 %", 100 * exR, "%+.2f", Tone::Good);
        R.metric_text("certified shape", certified, Tone::Accent);
        R.metric("exact I(V), eq. intro:profile", I, "%.4f", Tone::Accent);
        R.metric_text("excess above =", "estimator bias", Tone::Neutral);
    }
    h.result("grid_N", N);
    h.result("mbo_sigma_voxels", sigma);
    h.result("mbo_steps_per_frame", steps);
    h.result("restart_every_frames", restart_every);
    h.result("threads", threads);
    h.result("frames_left_shape_matches_certified_type", (double)match_frames / std::max(1, nfr));
    h.result("left_mean_area_excess", sum_exL / std::max(1, nfr));
    h.result("left_max_area_excess", max_exL);
    h.result("right_mean_estimator_bias", sum_exR / std::max(1, nfr));
    h.result("right_min_estimator_bias", min_exR);
    h.result("right_max_estimator_bias", max_exR);
    h.result("left_last_shape", lastL);
    h.result("right_last_shape", lastR);
    return h.finish();
}
