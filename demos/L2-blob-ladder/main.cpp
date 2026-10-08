// L2-blob-ladder - load ladder for 07-periodic-blob: the isoperimetric minimizer in the cubic flat
// 3-torus at grid resolutions 32^3 ... 256^3, measured as TIME-TO-TARGET.
//
// Family 354, "The Isoperimetric Conjecture for the Cubic Flat Three-Torus",
// preprints/The-Isoperimetric-Conjecture-for-the-Cubic-Flat-Three-Torus-September-24-2026/
// build/paper.tex, Theorem intro:main + eq. intro:profile:
//     I(V) = min{ (36 pi)^(1/3) v^(2/3), 2 sqrt(pi v), 2 },   v = min(V, 1-V)
// minimizers: ball (v <= 4pi/81), solid tube about a closed geodesic (4pi/81 <= v <= 1/pi),
// slab (v >= 1/pi), complements for V > 1/2.
//
// WITHOUT: volume-preserving threshold dynamics (MBO / Ruuth-Wetton) from smooth random noise.
//          Gaussian sigma is FIXED IN PHYSICAL UNITS (default 1/16 of the cell), so finer grids solve
//          the same continuum problem. The Gaussian is a periodic recursive (Young-van Vliet) IIR,
//          O(1) per voxel independent of sigma, and the k-largest selection is a parallel histogram
//          select (O(n)). Both are upgrades over 07's FIR + serial nth_element, which would scale
//          as N^4 / be single-threaded at fixed physical sigma.
//          Cost = wall time from the noise field to the first iterate whose area is within 2 % of the
//          exact I(V) (oracle stopping test, untimed - a favour to the baseline: without the paper
//          there is no exact target to stop at).
// WITH:    the certified minimizer of Theorem intro:main in closed form, voxelized on the same grid
//          with the same voxel count k (same parallel select). Cost = one build.
// Both scored by the same isotropic area estimator as 07 (integral of |grad(G_1 * chi)|, sigma = 1 voxel).
//
//   ./build.sh L2-blob-ladder run                  video (1920x1080, ~33 s)
//   ./L2-blob-ladder/demo.exe --sweep              timing ladder -> out/ladder.json (no video)
#include "demo.h"
#include "../07-periodic-blob/sw3d.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <random>
#include <string>
#include <vector>

using namespace demo;
using sw3d::Pool;
using sw3d::V3;

static constexpr double PI = 3.14159265358979323846;
static const double T_BALL = 4 * PI / 81;  // ball <-> tube
static const double T_TUBE = 1 / PI;       // tube <-> slab

// ---------------------------------------------------------------- the theorem (eq. intro:profile)
static double ball_branch(double v) { return std::cbrt(36 * PI) * std::pow(v, 2.0 / 3.0); }
static double tube_branch(double v) { return 2 * std::sqrt(PI * v); }
static double I_unit(double V) {
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
    static const char* nm[] = {"ball", "tube", "slab"};
    Phase p = phase_of(V);
    if (p == SLAB) return "slab";
    return std::string(nm[p]) + (V > 0.5 ? " complement" : "");
}

static std::string group_int(long long v) {
    std::string s = std::to_string(v), o;
    int c = 0;
    for (int i = (int)s.size() - 1; i >= 0; --i) {
        o.insert(o.begin(), s[i]);
        if (++c % 3 == 0 && i > 0) o.insert(o.begin(), ',');
    }
    return o;
}
static std::string ms_str(double ms) {
    if (ms < 0) return "--";
    if (ms < 0.1) return fmt("%.3f ms", ms);
    if (ms < 10) return fmt("%.2f ms", ms);
    if (ms < 1000) return fmt("%.1f ms", ms);
    return fmt("%.2f s", ms / 1000);
}

static void to_float(Pool& pool, const uint8_t* chi, float* f, size_t n) {
    const int chunks = (int)std::max<size_t>(1, n / 16384);
    pool.for_range(chunks, 1, [&](int a, int b) {
        size_t i0 = n * a / chunks, i1 = n * b / chunks;
        for (size_t i = i0; i < i1; ++i) f[i] = chi[i];
    });
}

// ---------------------------------------------------------------- FIR periodic Gaussian (from 07)
// Used ONLY by the shared area estimator (sigma = 1 voxel, identical to 07) and display smoothing.
struct Blur {
    int N = 0, R = 0;
    std::vector<float> w;
    std::vector<int> m;
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
    void apply(Pool& pool, const float* in, float* out) {
        const int n = N, r2 = 2 * R;
        float* a = A.data(); float* b = B.data();
        const float* W = w.data(); const int* M = m.data();
        pool.for_range(n * n, 16, [&](int r0, int r1) {
            for (int row = r0; row < r1; ++row) {
                const float* src = in + (size_t)row * n; float* dst = a + (size_t)row * n;
                for (int i = 0; i < n; ++i) { float s = 0; for (int t = 0; t <= r2; ++t) s += W[t] * src[M[i + t]]; dst[i] = s; }
            }
        });
        pool.for_range(n * n, 16, [&](int r0, int r1) {
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
        pool.for_range(n * n, 16, [&](int r0, int r1) {
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

// ---------------------------------------------------------------- recursive periodic Gaussian
// Young & van Vliet (1995) 3rd-order causal + anticausal IIR, cost O(1) per voxel per axis for any
// sigma. Periodicity: each line's recursion is started P samples early on the wrapped signal (and the
// backward pass P samples late on the wrapped forward output), P ~ 6 sigma, so the start-up transient
// has decayed below float noise. In place.
static inline void iir_row(float* o, const float* x, const float* p1, const float* p2, const float* p3, int L,
                           float B, float a1, float a2, float a3) {
    for (int l = 0; l < L; ++l) o[l] = B * x[l] + a1 * p1[l] + a2 * p2[l] + a3 * p3[l];
}
struct RecGauss {
    int N = 0, P = 0;
    double sigma = 0;
    float B = 0, a1 = 0, a2 = 0, a3 = 0;
    RecGauss() = default;
    RecGauss(int N_, double s, double warm = 6.0) : N(N_), sigma(s) {
        double q = s >= 2.5 ? 0.98711 * s - 0.96330 : 3.97156 - 4.14554 * std::sqrt(1 - 0.26891 * s);
        double b0 = 1.57825 + 2.44413 * q + 1.4281 * q * q + 0.422205 * q * q * q;
        double b1 = 2.44413 * q + 2.85619 * q * q + 1.26661 * q * q * q;
        double b2 = -(1.4281 * q * q + 1.26661 * q * q * q);
        double b3 = 0.422205 * q * q * q;
        a1 = float(b1 / b0); a2 = float(b2 / b0); a3 = float(b3 / b0);
        B = 1.f - (a1 + a2 + a3);
        P = std::min(N, (int)std::ceil(warm * s) + 4);
    }
    // element (pos q, lane l) = d[q*ps + l], q in [0,N) periodic, L lanes contiguous
    void lines(float* d, int L, size_t ps, float* ring) const {
        const int n = N;
        auto row = [&](int q) { return d + (size_t)q * ps; };
        float* r0 = ring; float* r1 = ring + L; float* r2 = ring + 2 * L;
        {
            const float* x0 = row(n - P);
            for (int l = 0; l < L; ++l) r0[l] = r1[l] = r2[l] = x0[l];
        }
        float *w1 = r0, *w2 = r1, *w3 = r2;
        for (int q = n - P; q < n; ++q) { float* o = w3; iir_row(o, row(q), w1, w2, w3, L, B, a1, a2, a3); w3 = w2; w2 = w1; w1 = o; }
        const float *p1 = w1, *p2 = w2, *p3 = w3;
        for (int q = 0; q < n; ++q) { float* o = row(q); iir_row(o, o, p1, p2, p3, L, B, a1, a2, a3); p3 = p2; p2 = p1; p1 = o; }
        {
            const float* x0 = row(P - 1);
            for (int l = 0; l < L; ++l) r0[l] = r1[l] = r2[l] = x0[l];
        }
        w1 = r0; w2 = r1; w3 = r2;
        for (int q = P - 1; q >= 0; --q) { float* o = w3; iir_row(o, row(q), w1, w2, w3, L, B, a1, a2, a3); w3 = w2; w2 = w1; w1 = o; }
        p1 = w1; p2 = w2; p3 = w3;
        for (int q = n - 1; q >= 0; --q) { float* o = row(q); iir_row(o, o, p1, p2, p3, L, B, a1, a2, a3); p3 = p2; p2 = p1; p1 = o; }
    }
    void apply(Pool& pool, float* d) const {
        const int n = N;
        const size_t n2 = (size_t)n * n;
        pool.for_range(n, 1, [&](int a, int b) {  // z: lanes i, positions k
            std::vector<float> ring(3 * (size_t)n);
            for (int j = a; j < b; ++j) lines(d + (size_t)j * n, n, n2, ring.data());
        });
        pool.for_range(n, 1, [&](int a, int b) {  // y: lanes i, positions j
            std::vector<float> ring(3 * (size_t)n);
            for (int k = a; k < b; ++k) lines(d + (size_t)k * n2, n, n, ring.data());
        });
        const int nb = n / 16;  // x: transpose 16 rows at a time so the lanes are contiguous
        pool.for_range(n * nb, 2, [&](int a, int b) {
            std::vector<float> tmp((size_t)n * 16), ring(48);
            for (int t = a; t < b; ++t) {
                int k = t / nb, j0 = (t % nb) * 16;
                float* base = d + (size_t)k * n2 + (size_t)j0 * n;
                for (int l = 0; l < 16; ++l) { const float* r = base + (size_t)l * n; for (int i = 0; i < n; ++i) tmp[(size_t)i * 16 + l] = r[i]; }
                lines(tmp.data(), 16, 16, ring.data());
                for (int l = 0; l < 16; ++l) { float* r = base + (size_t)l * n; for (int i = 0; i < n; ++i) r[i] = tmp[(size_t)i * 16 + l]; }
            }
        });
    }
};

// ---------------------------------------------------------------- parallel exact top-k select
// Keep exactly the k voxels with the largest score, ties broken by lower index (same semantics as 07).
// Pass 1: per-chunk 4096-bin histogram over [lo, hi]; pass 2: write chi for bins above the threshold bin
// and collect the threshold bin's members; exact selection among those. Used by BOTH sides.
struct TopKFast {
    static constexpr int NB = 4096;
    std::vector<uint32_t> hist;
    std::vector<std::vector<uint32_t>> cand;
    std::vector<uint32_t> all;
    void run(Pool& pool, const float* u, size_t n, size_t k, uint8_t* chi, float lo, float hi) {
        if (k == 0) { std::memset(chi, 0, n); return; }
        if (k >= n) { std::memset(chi, 1, n); return; }
        const int C = (int)std::clamp<size_t>(n / 65536, 1, 96);
        hist.assign((size_t)C * NB, 0);
        cand.resize(C);
        const float sc = NB / (hi - lo);
        auto bin = [=](float v) { int b = (int)((v - lo) * sc); return b < 0 ? 0 : (b >= NB ? NB - 1 : b); };
        pool.for_range(C, 1, [&](int a, int b) {
            for (int c = a; c < b; ++c) {
                uint32_t* H = hist.data() + (size_t)c * NB;
                size_t i0 = n * c / C, i1 = n * (c + 1) / C;
                for (size_t i = i0; i < i1; ++i) ++H[bin(u[i])];
            }
        });
        std::vector<uint64_t> tot(NB, 0);
        for (int c = 0; c < C; ++c)
            for (int b = 0; b < NB; ++b) tot[b] += hist[(size_t)c * NB + b];
        uint64_t above = 0;
        int bs = 0;
        for (int b = NB - 1; b >= 0; --b) {
            if (above + tot[b] >= k) { bs = b; break; }
            above += tot[b];
        }
        const size_t m = k - above;
        pool.for_range(C, 1, [&](int a, int b) {
            for (int c = a; c < b; ++c) {
                auto& cv = cand[c];
                cv.clear();
                size_t i0 = n * c / C, i1 = n * (c + 1) / C;
                for (size_t i = i0; i < i1; ++i) {
                    int bb = bin(u[i]);
                    chi[i] = bb > bs;
                    if (bb == bs) cv.push_back((uint32_t)i);
                }
            }
        });
        all.clear();
        for (int c = 0; c < C; ++c) all.insert(all.end(), cand[c].begin(), cand[c].end());
        auto better = [&](uint32_t x, uint32_t y) { return u[x] > u[y] || (u[x] == u[y] && x < y); };
        if (m < all.size()) std::nth_element(all.begin(), all.begin() + m, all.end(), better);
        for (size_t i = 0; i < m && i < all.size(); ++i) chi[all[i]] = 1;
    }
};

// ---------------------------------------------------------------- WITHOUT: volume-preserving MBO
struct MBO {
    int N; size_t n;
    std::vector<uint8_t> chi;
    std::vector<float> u, f;
    RecGauss g;
    std::unique_ptr<Blur> fir;  // exact FIR Gaussian, used instead when it is faster (small sigma in voxels)
    bool use_fir = false;
    TopKFast sel;
    long long iters = 0;
    MBO(Pool& pool, int N_, double sigma_vox, int force = 0) : N(N_), n((size_t)N_ * N_ * N_), chi(n), u(n), g(N_, sigma_vox) {
        // pick the faster periodic Gaussian for this grid (the baseline gets the best of both)
        if (force == 2 || (force == 0 && sigma_vox <= 6)) {
            fir = std::make_unique<Blur>(N, sigma_vox);
            f.resize(n);
            if (force == 2) use_fir = true;
            else {
                double tf = 1e30, tr = 1e30;
                for (int rep = 0; rep < 5; ++rep) {
                    double t0 = now_ms(); fir->apply(pool, u.data(), f.data()); tf = std::min(tf, now_ms() - t0);
                    t0 = now_ms(); g.apply(pool, u.data()); tr = std::min(tr, now_ms() - t0);
                }
                use_fir = tf < tr;
            }
            if (!use_fir) { fir.reset(); f.clear(); f.shrink_to_fit(); }
        }
    }
    const char* blur_name() const { return use_fir ? "FIR" : "recursive"; }
    void init(Pool& pool, const float* noise, size_t k) { sel.run(pool, noise, n, k, chi.data(), -0.05f, 1.05f); iters = 0; }
    void step(Pool& pool, size_t k) {
        if (use_fir) { to_float(pool, chi.data(), f.data(), n); fir->apply(pool, f.data(), u.data()); }
        else { to_float(pool, chi.data(), u.data(), n); g.apply(pool, u.data()); }
        sel.run(pool, u.data(), n, k, chi.data(), -0.05f, 1.05f);
        ++iters;
    }
};

// smooth random field, the same PHYSICAL field at every resolution: a 16^3 lattice of uniform values
// (seeded), periodic trilinear upsampling, then a Gaussian of 1/32 cell. Untimed (it is the input).
static void make_noise(Pool& pool, int N, unsigned seed, std::vector<float>& out) {
    const int G = 16;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> U(0.f, 1.f);
    std::vector<float> lat((size_t)G * G * G);
    for (auto& x : lat) x = U(rng);
    const size_t n = (size_t)N * N * N;
    out.resize(n);
    pool.for_range(N * N, 8, [&](int a, int b) {
        for (int row = a; row < b; ++row) {
            int k = row / N, j = row % N;
            auto sp = [&](int i, int& i0, int& i1, float& t) {
                double g = (i + 0.5) / N * G;
                double f = std::floor(g);
                t = float(g - f); i0 = ((int)f % G + G) % G; i1 = (i0 + 1) % G;
            };
            int j0, j1, k0, k1; float tj, tk;
            sp(j, j0, j1, tj); sp(k, k0, k1, tk);
            float* dst = out.data() + (size_t)row * N;
            for (int i = 0; i < N; ++i) {
                int i0, i1; float ti;
                sp(i, i0, i1, ti);
                auto L = [&](int x, int y, int z) { return lat[((size_t)z * G + y) * G + x]; };
                float c00 = L(i0, j0, k0) + (L(i1, j0, k0) - L(i0, j0, k0)) * ti;
                float c10 = L(i0, j1, k0) + (L(i1, j1, k0) - L(i0, j1, k0)) * ti;
                float c01 = L(i0, j0, k1) + (L(i1, j0, k1) - L(i0, j0, k1)) * ti;
                float c11 = L(i0, j1, k1) + (L(i1, j1, k1) - L(i0, j1, k1)) * ti;
                float c0 = c00 + (c10 - c00) * tj, c1 = c01 + (c11 - c01) * tj;
                dst[i] = c0 + (c1 - c0) * tk;
            }
        }
    });
    RecGauss(N, N / 32.0).apply(pool, out.data());
}

// ---------------------------------------------------------------- WITH: closed-form certified minimizer
// Ranking voxels by squared periodic distance to the centre point / axis / plane gives exactly the
// ball / tube / slab of Theorem intro:main at voxel count k (complements: reverse the ranking).
struct ClosedForm {
    int N; size_t n;
    std::vector<uint8_t> chi;
    std::vector<float> s, d2;
    TopKFast sel;
    Phase ph = BALL; bool comp = false;
    ClosedForm(int N_) : N(N_), n((size_t)N_ * N_ * N_), chi(n), s(n), d2(N_) {}
    void build(Pool& pool, double V, size_t k) {
        comp = V > 0.5;
        ph = phase_of(V);
        for (int i = 0; i < N; ++i) { double d = (i + 0.5) / N - 0.5; d2[i] = float(d * d); }
        const float sg = comp ? 1.f : -1.f;
        const Phase p = ph; const int Nn = N; const float* D = d2.data();
        pool.for_range(N * N, 32, [&](int r0, int r1) {
            for (int row = r0; row < r1; ++row) {
                int kk = row / Nn, j = row % Nn;
                float* dst = s.data() + (size_t)row * Nn;
                if (p == BALL) { float yz = D[j] + D[kk]; for (int i = 0; i < Nn; ++i) dst[i] = sg * (D[i] + yz); }
                else if (p == TUBE) { float z = D[kk]; for (int i = 0; i < Nn; ++i) dst[i] = sg * (D[i] + z); }  // tube along y
                else { for (int i = 0; i < Nn; ++i) dst[i] = sg * D[i]; }                                       // slab normal x
            }
        });
        sel.run(pool, s.data(), n, k, chi.data(), -0.8f, 0.8f);
    }
};

// ---------------------------------------------------------------- shared area estimator (from 07)
struct AreaEst {
    int N; size_t n;
    Blur b1;
    std::vector<float> f, phi;
    std::vector<double> rowsum;
    AreaEst(int N_) : N(N_), n((size_t)N_ * N_ * N_), b1(N_, 1.0), f(n), phi(n), rowsum((size_t)N_ * N_) {}
    double area(Pool& pool, const uint8_t* chi) {
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
        return s * 0.5 * h * h;
    }
};

// ---------------------------------------------------------------- shape classification (from 07)
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
static bool shape_matches(const ShapeInfo& s, double V) {
    return s.phase == (int)phase_of(V) && (s.phase == SLAB || s.comp == (V > 0.5));
}

// ---------------------------------------------------------------- statistics helpers
static double quantile(std::vector<double> v, double q) {
    if (v.empty()) return -1;
    std::sort(v.begin(), v.end());
    double pos = q * (v.size() - 1);
    size_t i = (size_t)pos;
    double t = pos - i;
    return i + 1 < v.size() ? v[i] * (1 - t) + v[i + 1] * t : v[i];
}
static double fit_slope(const std::vector<double>& x, const std::vector<double>& y) {
    if (x.size() < 2) return 0;
    double mx = 0, my = 0;
    for (size_t i = 0; i < x.size(); ++i) { mx += std::log(x[i]); my += std::log(y[i]); }
    mx /= x.size(); my /= x.size();
    double sxy = 0, sxx = 0;
    for (size_t i = 0; i < x.size(); ++i) { double dx = std::log(x[i]) - mx; sxy += dx * (std::log(y[i]) - my); sxx += dx * dx; }
    return sxx > 0 ? sxy / sxx : 0;
}
static std::vector<double> parse_list(const std::string& s) {
    std::vector<double> v;
    size_t p = 0;
    while (p < s.size()) {
        size_t q = s.find(',', p);
        if (q == std::string::npos) q = s.size();
        if (q > p) v.push_back(std::atof(s.substr(p, q - p).c_str()));
        p = q + 1;
    }
    return v;
}

// ---------------------------------------------------------------- one MBO run to the 2 % target
struct RunResult {
    bool reached = false, reached_alt = false, fixed_point = false, stalled = false;
    int it_hit = -1, it_alt = -1, iters = 0;
    double ms_hit = -1, ms_alt = -1, ms_total = 0, final_excess = 0, best_excess = 1e9, excess_at_hit = 0;
    std::vector<double> iter_ms;
    std::string shape;
    bool shape_ok = false;
};
// cap_it: iteration cap. Stops at the 2 % target (primary: A <= 1.02 I(V)) once the alternative
// target (A <= 1.02 A_closedform, same estimator) has also been met, at a fixed point, at the cap,
// or after `stall` iterations without a 0.01 % improvement of the best area.
template <class Start>
static RunResult run_mbo(Pool& pool, MBO& m, AreaEst& an, Start&& start, size_t k, double I, double Awith,
                         int cap_it, int stall, bool do_classify, double V, double ms0 = 0) {
    RunResult r;
    std::vector<uint8_t> prev(m.n);
    double t0 = now_ms();
    start();  // timed: initial threshold of the noise field (or prolongation from a coarse grid)
    double ms = ms0 + now_ms() - t0;
    int last_improve = 0;
    double best_A = 1e30;
    for (int it = 0;; ++it) {
        double A = an.area(pool, m.chi.data());  // oracle stopping test - untimed
        double ex = A / I - 1;
        r.final_excess = ex;
        r.best_excess = std::min(r.best_excess, ex);
        if (A < best_A * (1 - 1e-4)) { best_A = A; last_improve = it; }
        if (!r.reached && ex <= 0.02) { r.reached = true; r.it_hit = it; r.ms_hit = ms; r.excess_at_hit = ex; }
        if (!r.reached_alt && A <= 1.02 * Awith) { r.reached_alt = true; r.it_alt = it; r.ms_alt = ms; }
        if (r.reached && r.reached_alt) break;
        if (it >= cap_it) break;
        if (it - last_improve > stall) { r.stalled = true; break; }
        std::memcpy(prev.data(), m.chi.data(), m.n);
        double t1 = now_ms();
        m.step(pool, k);
        double dt = now_ms() - t1;
        ms += dt;
        r.iter_ms.push_back(dt);
        if (std::memcmp(prev.data(), m.chi.data(), m.n) == 0) {  // exact fixed point: nothing will change
            double A2 = an.area(pool, m.chi.data());
            r.final_excess = A2 / I - 1;
            r.fixed_point = true;
            r.iters = it + 1;
            break;
        }
        r.iters = it + 1;
    }
    r.ms_total = ms;
    if (do_classify) {
        ShapeInfo s = classify(m.chi.data(), m.N);
        r.shape = s.name;
        r.shape_ok = shape_matches(s, V);
    }
    return r;
}

// Coarse-to-fine variant of the baseline (sweep only, reported as an extra): MBO on a 32^3 grid
// (same physical sigma, same physical noise) to its own 2 % test, trilinear prolongation of the coarse
// indicator to N^3 + exact top-k, then MBO at N^3 to the 2 % target. All of it timed.
struct C2F {
    int Nc;
    std::unique_ptr<MBO> mc;
    std::unique_ptr<AreaEst> anc;
    std::vector<float> noise_c;
};
static RunResult run_c2f(Pool& pool, C2F& c, MBO& m, AreaEst& an, unsigned seed, size_t k, double V, double I, double Aw, int cap, int stall) {
    const int Nc = c.Nc, N = m.N;
    const size_t nc = (size_t)Nc * Nc * Nc, kc = (size_t)std::llround(V * nc);
    make_noise(pool, Nc, seed, c.noise_c);
    RunResult rc = run_mbo(pool, *c.mc, *c.anc, [&] { c.mc->init(pool, c.noise_c.data(), kc); }, kc, I_unit(double(kc) / nc), 1e30, cap, stall, false, V);
    auto prolong = [&] {
        const uint8_t* src = c.mc->chi.data();
        float* dst = m.u.data();
        pool.for_range(N * N, 8, [&](int a, int b) {
            for (int row = a; row < b; ++row) {
                int kk = row / N, j = row % N;
                auto sp = [&](int i, int& i0, int& i1, float& t) {
                    double g = (i + 0.5) / N * Nc - 0.5, f = std::floor(g);
                    t = float(g - f); i0 = ((int)f % Nc + Nc) % Nc; i1 = (i0 + 1) % Nc;
                };
                int j0, j1, k0, k1; float tj, tk;
                sp(j, j0, j1, tj); sp(kk, k0, k1, tk);
                for (int i = 0; i < N; ++i) {
                    int i0, i1; float ti;
                    sp(i, i0, i1, ti);
                    auto L = [&](int x, int y, int z) { return (float)src[((size_t)z * Nc + y) * Nc + x]; };
                    float c00 = L(i0, j0, k0) + (L(i1, j0, k0) - L(i0, j0, k0)) * ti;
                    float c10 = L(i0, j1, k0) + (L(i1, j1, k0) - L(i0, j1, k0)) * ti;
                    float c01 = L(i0, j0, k1) + (L(i1, j0, k1) - L(i0, j0, k1)) * ti;
                    float c11 = L(i0, j1, k1) + (L(i1, j1, k1) - L(i0, j1, k1)) * ti;
                    float c0 = c00 + (c10 - c00) * tj, c1 = c01 + (c11 - c01) * tj;
                    dst[(size_t)row * N + i] = c0 + (c1 - c0) * tk;
                }
            }
        });
        m.sel.run(pool, m.u.data(), m.n, k, m.chi.data(), -0.05f, 1.05f);
        m.iters = 0;
    };
    RunResult r = run_mbo(pool, m, an, prolong, k, I, Aw, cap, stall, false, V, rc.ms_total);
    r.iters += rc.iters;  // fine iterations + coarse iterations
    return r;
}

// ---------------------------------------------------------------- sweep mode
static int run_sweep(int argc, char** argv) {
    const std::vector<double> Ns = parse_list(arg_str(argc, argv, "--ns", "32,48,64,96,128,192,256,384"));
    const std::vector<double> Vs = parse_list(arg_str(argc, argv, "--vs", "0.10,0.25,0.40"));
    const int seeds = arg_int(argc, argv, "--seeds", 3);
    const double sig_phys = arg_double(argc, argv, "--sigma-phys", 1.0 / 16);
    const int cap_it = arg_int(argc, argv, "--cap-it", 300);
    const int stall = arg_int(argc, argv, "--stall", 60);
    const int threads = arg_int(argc, argv, "--threads", hw_threads());
    const double budget = 16.7;
    std::string out_dir = arg_str(argc, argv, "--out", "");
    if (out_dir.empty()) {
        std::string exe = argv[0];
        size_t p = exe.find_last_of("/\\");
        out_dir = (p == std::string::npos ? std::string(".") : exe.substr(0, p)) + "/out";
    }
    Pool pool(threads);
    const double T0 = now_ms();
    std::printf("[sweep] threads %d (both sides) · sigma = %.4f cell (fixed physical) · V = %s · %d seeds · cap %d it\n", threads, sig_phys,
                arg_str(argc, argv, "--vs", "0.10,0.25,0.40").c_str(), seeds, cap_it);
    std::printf("%6s %12s | %10s %10s %6s %8s | %10s %10s | %8s | %s\n", "N", "voxels", "MBO ms2%", "p95", "fail", "ms/it", "CF ms",
                "CF p95", "ratio", "excess L / R");
    struct Stage {
        int N; double wo_med, wo_p95, wo_med_succ, wo_alt_med, it_med, ms_per_it, w_med, w_p95, exL_med, exR_mean; double c2f_med = -1; int runs, fails, w_frames, fails_alt, c2f_fails = 0, c2f_runs = 0;
        std::string run_json, blur;
    };
    std::vector<Stage> st;
    for (double Nd : Ns) {
        const int N = (int)Nd;
        const size_t n = (size_t)N * N * N;
        Stage S{};
        S.N = N;
        auto mbo = std::make_unique<MBO>(pool, N, sig_phys * N, arg_int(argc, argv, "--blur", 0));
        auto cf = std::make_unique<ClosedForm>(N);
        S.blur = mbo->blur_name();
        auto an = std::make_unique<AreaEst>(N);
        C2F c2f{32, nullptr, nullptr, {}};
        const bool do_c2f = N >= 64 && !arg_flag(argc, argv, "--no-c2f");
        if (do_c2f) { c2f.mc = std::make_unique<MBO>(pool, 32, sig_phys * 32, arg_int(argc, argv, "--blur", 0)); c2f.anc = std::make_unique<AreaEst>(32); }
        std::vector<double> t_c2f;
        std::vector<float> noise;
        std::vector<double> t_hit, t_alt, its, iter_ms, wms, exL, exR;
        int fails = 0, fails_alt = 0;
        std::string rj;
        for (double V : Vs) {
            const size_t k = (size_t)std::llround(V * n);
            const double Vq = double(k) / n, I = I_unit(Vq);
            // WITH: build time, >= 15 builds and >= 0.5 s per V (>= 1.5 s per stage with 3 V)
            for (int w = 0; w < 3; ++w) cf->build(pool, Vq, k);
            std::vector<double> b;
            double tb0 = now_ms();
            while ((int)b.size() < 15 || now_ms() - tb0 < 1500.0 / Vs.size()) {
                double t0 = now_ms();
                cf->build(pool, Vq, k);
                b.push_back(now_ms() - t0);
                if (b.size() > 200000) break;
            }
            wms.insert(wms.end(), b.begin(), b.end());
            const double Aw = an->area(pool, cf->chi.data());
            exR.push_back(Aw / I - 1);
            for (int s = 0; s < seeds; ++s) {
                const unsigned seed = 354 + 1000 * s + (unsigned)std::llround(V * 100);
                make_noise(pool, N, seed, noise);
                RunResult r = run_mbo(pool, *mbo, *an, [&] { mbo->init(pool, noise.data(), k); }, k, I, Aw, cap_it, stall, N <= 256, Vq);
                t_hit.push_back(r.reached ? r.ms_hit : 1e300);
                t_alt.push_back(r.reached_alt ? r.ms_alt : 1e300);
                its.push_back(r.reached ? r.it_hit : 1e300);
                iter_ms.insert(iter_ms.end(), r.iter_ms.begin(), r.iter_ms.end());
                exL.push_back(r.reached ? r.excess_at_hit : r.final_excess);
                if (!r.reached) ++fails;
                if (!r.reached_alt) ++fails_alt;
                rj += fmt("%s{\"V\": %.4f, \"seed\": %u, \"reached\": %s, \"it_to_2pct\": %d, \"ms_to_2pct\": %.4f, \"reached_vs_closedform\": %s, "
                          "\"ms_to_2pct_vs_closedform\": %.4f, \"iters_run\": %d, \"ms_run\": %.3f, \"final_excess\": %.5f, \"best_excess\": %.5f, "
                          "\"stop\": \"%s\", \"final_shape\": \"%s\", \"final_shape_matches_theorem\": %s, \"closed_form_excess\": %.5f}",
                          rj.empty() ? "" : ", ", Vq, seed, r.reached ? "true" : "false", r.it_hit, r.ms_hit, r.reached_alt ? "true" : "false",
                          r.ms_alt, r.iters, r.ms_total, r.final_excess, r.best_excess,
                          r.reached && r.reached_alt ? "target" : r.fixed_point ? "fixed point" : r.stalled ? "stalled" : "iteration cap",
                          r.shape.c_str(), r.shape_ok ? "true" : "false", Aw / I - 1);
                if (do_c2f) {
                    RunResult rc = run_c2f(pool, c2f, *mbo, *an, seed, k, Vq, I, Aw, cap_it, stall);
                    t_c2f.push_back(rc.reached ? rc.ms_hit : 1e300);
                    if (!rc.reached) ++S.c2f_fails;
                    ++S.c2f_runs;
                    rj += fmt(", {\"variant\": \"coarse-to-fine\", \"V\": %.4f, \"seed\": %u, \"reached\": %s, \"ms_to_2pct\": %.4f, \"iters_coarse_plus_fine\": %d}",
                              Vq, seed, rc.reached ? "true" : "false", rc.ms_hit, rc.iters);
                    if (arg_flag(argc, argv, "--verbose")) std::printf("      c2f: %s ms %.2f\n", rc.reached ? "hit" : "MISS", rc.ms_hit);
                }
                if (arg_flag(argc, argv, "--verbose"))
                    std::printf("    N=%d V=%.2f seed %u: %s it %d ms %.2f | alt %s %.2f | final %+.2f%% %s (%s)\n", N, Vq, seed, r.reached ? "hit" : "MISS",
                                r.it_hit, r.ms_hit, r.reached_alt ? "hit" : "miss", r.ms_alt, 100 * r.final_excess, r.shape.c_str(),
                                r.fixed_point ? "fixed pt" : r.stalled ? "stalled" : "");
            }
        }
        const int runs = (int)t_hit.size();
        auto finite_or = [](double x) { return x >= 1e299 ? -1.0 : x; };
        S.runs = runs; S.fails = fails; S.fails_alt = fails_alt;
        S.wo_med = finite_or(quantile(t_hit, 0.5));
        S.wo_p95 = finite_or(quantile(t_hit, 0.95));
        S.wo_alt_med = finite_or(quantile(t_alt, 0.5));
        S.it_med = finite_or(quantile(its, 0.5));
        std::vector<double> succ;
        for (double x : t_hit) if (x < 1e299) succ.push_back(x);
        S.wo_med_succ = succ.empty() ? -1 : quantile(succ, 0.5);
        S.ms_per_it = quantile(iter_ms, 0.5);
        S.w_med = quantile(wms, 0.5);
        S.w_p95 = quantile(wms, 0.95);
        S.w_frames = (int)wms.size();
        S.exL_med = quantile(exL, 0.5);
        double sr = 0; for (double x : exR) sr += x; S.exR_mean = sr / exR.size();
        S.c2f_med = t_c2f.empty() ? -1 : finite_or(quantile(t_c2f, 0.5));
        S.run_json = rj;
        st.push_back(S);
        std::printf("%4d^3 %12s | %10.2f %10.2f %3d/%-2d %8.3f | %10.4f %10.4f | %8.0f | %+.2f%% / %+.2f%% | c2f %8.2f (%d fail) %s (%.0f s)\n", N,
                    group_int((long long)n).c_str(), S.wo_med, S.wo_p95, fails, runs, S.ms_per_it, S.w_med, S.w_p95,
                    S.wo_med > 0 ? S.wo_med / S.w_med : -1.0, 100 * S.exL_med, 100 * S.exR_mean, S.c2f_med, S.c2f_fails, S.blur.c_str(), (now_ms() - T0) / 1000);
        std::fflush(stdout);
    }
    // fits & budget
    std::vector<double> xn, yw, xw, yc;
    int maxN_wo = 0, maxN_w = 0;
    for (auto& S : st) {
        if (S.wo_med > 0) { xn.push_back((double)S.N * S.N * S.N); yw.push_back(S.wo_med); }
        xw.push_back((double)S.N * S.N * S.N); yc.push_back(S.w_med);
        if (S.wo_med > 0 && S.wo_med <= budget) maxN_wo = std::max(maxN_wo, S.N);
        if (S.w_med <= budget) maxN_w = std::max(maxN_w, S.N);
    }
    const double ew = fit_slope(xn, yw), ec = fit_slope(xw, yc);
    std::vector<double> xl, ywl, xcl, ycl, x2, y2;
    for (auto& S : st) {
        const double n3 = (double)S.N * S.N * S.N;
        if (S.N >= 128 && S.wo_med > 0) { xl.push_back(n3); ywl.push_back(S.wo_med); }
        if (S.N >= 128) { xcl.push_back(n3); ycl.push_back(S.w_med); }
        if (S.c2f_med > 0) { x2.push_back(n3); y2.push_back(S.c2f_med); }
    }
    const double ewl = fit_slope(xl, ywl), ecl = fit_slope(xcl, ycl), e2 = fit_slope(x2, y2);
    std::string js = "{\n";
    js += "  \"demo\": \"L2-blob-ladder\",\n  \"family\": \"354\",\n";
    js += "  \"load_name\": \"grid voxels (n = N^3, periodic unit cube)\",\n";
    js += fmt("  \"budget_ms\": %.1f,\n  \"threads\": {\"without\": %d, \"with\": %d},\n", budget, threads, threads);
    js += "  \"metric\": \"without.median_ms = median wall ms (MBO compute only) from the noise field to the first iterate with estimated area <= 1.02 I(V); runs that never get there count as +infinity in the median; with.median_ms = median ms to build the closed-form minimizer at the same N and k\",\n";
    js += "  \"stages\": [\n";
    for (size_t i = 0; i < st.size(); ++i) {
        auto& S = st[i];
        const long long n = (long long)S.N * S.N * S.N;
        js += fmt("    {\"n\": %lld, \"grid\": \"%d^3\", \"N\": %d,\n", n, S.N, S.N);
        js += S.wo_med > 0 ? fmt("     \"without\": {\"median_ms\": %.4f, ", S.wo_med) : std::string("     \"without\": {\"median_ms\": null, \"note\": \"more than half of the runs never reached 2 %\", ");
        js += S.wo_p95 > 0 ? fmt("\"p95_ms\": %.4f, ", S.wo_p95) : std::string("\"p95_ms\": null, \"p95_note\": \"the slowest 5 % of runs never reached 2 % (failure counted as infinite)\", ");
        js += fmt("\"frames\": %d, \"frames_note\": \"runs (V x seeds), not frames\", \"median_ms_successful_runs_only\": %.4f, \"median_iterations_to_2pct\": %.1f, "
                  "\"ms_per_iteration_median\": %.4f, \"gaussian\": \"%s\", \"failures\": %d, \"median_ms_to_within_2pct_of_closed_form_same_estimator\": %.4f, \"failures_vs_closed_form\": %d},\n",
                  S.runs, S.wo_med_succ, S.it_med, S.ms_per_it, S.blur.c_str(), S.fails, S.wo_alt_med, S.fails_alt);
        if (S.c2f_runs) js += fmt("     \"without_coarse_to_fine\": {\"median_ms\": %s, \"failures\": %d, \"runs\": %d, \"note\": \"extra baseline variant: MBO at 32^3 to 2 %%, prolong, MBO at N^3 to 2 %%; all timed\"},\n",
                                  S.c2f_med > 0 ? fmt("%.4f", S.c2f_med).c_str() : "null", S.c2f_fails, S.c2f_runs);
        js += fmt("     \"with\": {\"median_ms\": %.5f, \"p95_ms\": %.5f, \"frames\": %d},\n", S.w_med, S.w_p95, S.w_frames);
        js += fmt("     \"ratio_without_over_with\": %s,\n", S.wo_med > 0 ? fmt("%.1f", S.wo_med / S.w_med).c_str() : "null");
        js += fmt("     \"quality\": {\"name\": \"area excess A_est/I(V) - 1 reached (WITHOUT: median over runs of the excess at the stopping iterate - at the first iterate within 2 %% if reached, else the final one; WITH: mean estimator bias of the exact shape)\", "
                  "\"without\": %.5f, \"with\": %.5f, \"higher_is_better\": false, \"without_failures\": %d, \"runs\": %d},\n",
                  S.exL_med, S.exR_mean, S.fails, S.runs);
        js += "     \"runs\": [" + S.run_json + "]}";
        js += i + 1 < st.size() ? ",\n" : "\n";
    }
    js += "  ],\n";
    js += fmt("  \"fit\": {\"without_exponent\": %.3f, \"with_exponent\": %.3f, \"note\": \"log-log slope of median_ms vs n = N^3 over all stages with a finite WITHOUT median; small grids are dominated by fixed per-call overheads, see fit_large_n\"},\n", ew, ec);
    js += fmt("  \"fit_large_n\": {\"without_exponent\": %.3f, \"with_exponent\": %.3f, \"without_coarse_to_fine_exponent_all_stages\": %.3f, \"note\": \"slopes over stages with N >= 128 (coarse-to-fine: all stages where it ran)\"},\n", ewl, ecl, e2);
    js += fmt("  \"max_n_within_budget_coarse_to_fine\": %lld,\n", [&] { long long b = 0; for (auto& S : st) if (S.c2f_med > 0 && S.c2f_med <= budget) b = std::max(b, (long long)S.N * S.N * S.N); return b; }());
    js += fmt("  \"max_n_within_budget\": {\"without\": %lld, \"with\": %lld, \"note\": \"largest measured n whose median time-to-target is <= 16.7 ms (0 = none)\"},\n",
              (long long)maxN_wo * maxN_wo * maxN_wo, (long long)maxN_w * maxN_w * maxN_w);
    js += fmt("  \"params\": {\"sigma_physical\": %.5f, \"volumes\": \"%s\", \"seeds\": %d, \"iteration_cap\": %d, \"stall_window\": %d, \"target\": 0.02},\n",
              sig_phys, arg_str(argc, argv, "--vs", "0.10,0.25,0.40").c_str(), seeds, cap_it, stall);
    js += fmt("  \"machine\": {\"cpu\": \"%s\", \"hw_threads\": %d},\n", cpu_name().c_str(), hw_threads());
    js += fmt("  \"sweep_wall_s\": %.1f,\n", (now_ms() - T0) / 1000);
    js += "  \"notes\": \"Both sides produce a voxel set with exactly k = round(V N^3) voxels on the periodic N^3 grid, 24 threads each. WITHOUT is volume-preserving MBO "
          "(recursive periodic Gaussian, sigma fixed at 1/16 of the cell, parallel histogram top-k) started from a smooth random field that is the same physical field at "
          "every N; its time counts only MBO compute (initial threshold + iterations) and stops at the first iterate whose estimated area is within 2 % of the exact "
          "I(V) - that stopping test is an oracle supplied by the paper and is not timed. WITH builds the closed-form minimizer of Theorem intro:main (O(N^3) "
          "voxelization + the same top-k). Both costs scale ~linearly in n; the gap is the MBO iteration count (roughly resolution independent at fixed physical "
          "sigma) times the per-iteration/build cost ratio, plus the runs that never reach the target (local minima, counted as failures). The paper proves which "
          "shape is optimal and the value I(V); every timing here is measured, not proved.\"\n";
    js += "}\n";
    std::string path = out_dir + "/ladder.json";
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f) { std::fwrite(js.data(), 1, js.size(), f); std::fclose(f); std::printf("[sweep] wrote %s (%.0f s)\n", path.c_str(), (now_ms() - T0) / 1000); }
    std::printf("[sweep] fitted exponents vs n: WITHOUT %.2f, WITH %.2f (N >= 128: %.2f / %.2f; coarse-to-fine %.2f)\n", ew, ec, ewl, ecl, e2);
    return 0;
}

// ---------------------------------------------------------------- self-check of the recursive blur
static int blur_check(int argc, char** argv) {
    Pool pool(arg_int(argc, argv, "--threads", hw_threads()));
    for (int N : {32, 64, 128, 256}) {
        const size_t n = (size_t)N * N * N;
        const double sig = N / 16.0;
        std::vector<float> a(n), b(n);
        std::mt19937 rng(1);
        std::uniform_real_distribution<float> U(0, 1);
        for (auto& x : a) x = U(rng) < 0.3f ? 1.f : 0.f;
        b = a;
        RecGauss g(N, sig);
        double t0 = now_ms();
        g.apply(pool, b.data());
        double tr = now_ms() - t0;
        std::vector<float> c(n);
        double tf = -1, err = 0;
        if (N <= 128) {
            Blur fir(N, sig);
            t0 = now_ms();
            fir.apply(pool, a.data(), c.data());
            tf = now_ms() - t0;
            for (size_t i = 0; i < n; ++i) err = std::max(err, (double)std::fabs(b[i] - c[i]));
        }
        double sa = 0, sb = 0;
        for (size_t i = 0; i < n; ++i) { sa += a[i]; sb += b[i]; }
        std::printf("N=%d sigma=%.1f vox: recursive %.2f ms, FIR %.2f ms, max |rec-FIR| %.2e, mass drift %.2e, P=%d\n", N, sig, tr, tf, err,
                    std::fabs(sb - sa) / sa, g.P);
        ClosedForm cf(N);
        AreaEst an(N);
        for (double V : {0.10, 0.25, 0.40}) {
            size_t k = (size_t)std::llround(V * n);
            t0 = now_ms();
            cf.build(pool, double(k) / n, k);
            double tb = now_ms() - t0;
            size_t cnt = 0;
            for (size_t i = 0; i < n; ++i) cnt += cf.chi[i];
            std::printf("   closed form V=%.2f: %.3f ms, count ok %d, est. bias %+.3f%%\n", V, tb, cnt == k, 100 * (an.area(pool, cf.chi.data()) / I_unit(double(k) / n) - 1));
        }
    }
    return 0;
}

// ---------------------------------------------------------------- rendering (untimed; from 07)
struct Renderer {
    int N = 64;
    sw3d::Camera cam;
    int rx = 0, ry = 0, rw = 960, rh = 655;
    static inline float tri(const float* a, int N, float x, float y, float z) {
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
            p.fill_rect(0, (float)y, (float)p.width(), 1, lerp(rgb(0x141a26), pal::bg, std::sqrt(t)));
        }
        for (int axis = 0; axis < 3; ++axis)
            for (int a = 0; a <= 2; ++a)
                for (int b = 0; b <= 2; ++b) {
                    double c[3];
                    c[axis] = 0; c[(axis + 1) % 3] = a; c[(axis + 2) % 3] = b; V3 p0{c[0], c[1], c[2]};
                    c[axis] = 2; V3 p1{c[0], c[1], c[2]};
                    double x0, y0, x1, y1;
                    cam.project(p0, x0, y0); cam.project(p1, x1, y1);
                    p.line((float)x0, (float)y0, (float)x1, (float)y1, 1.2f, pal::dim, 0.22f);
                }
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
    void render(Pool& pool, Panel& p, const float* phi, const float* D, Color base) {
        uint8_t* px = p.data(); const int stride = p.stride();
        const float Nf = (float)N, h = 1.f / N;
        const V3 L = sw3d::norm(V3{-0.45, 0.8, 0.35});
        pool.for_range(rh, 2, [&](int y0, int y1) {
            for (int y = ry + y0; y < ry + y1; ++y) {
                uint8_t* row = px + (size_t)y * stride;
                for (int x = rx; x < rx + rw; ++x) {
                    V3 rd = cam.ray(x + 0.5, y + 0.5), ro = cam.eye;
                    double t0 = 0, t1 = 1e9;
                    double o[3] = {ro.x, ro.y, ro.z}, d[3] = {rd.x, rd.y, rd.z};
                    for (int a = 0; a < 3; ++a) {
                        double inv = 1.0 / d[a];
                        double ta = (0 - o[a]) * inv, tb = (2 - o[a]) * inv;
                        if (ta > tb) std::swap(ta, tb);
                        t0 = std::max(t0, ta); t1 = std::min(t1, tb);
                    }
                    if (t1 <= t0) continue;
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
                            if (inside) {
                                float tr = std::exp(-kabs * float(th - t));
                                float e = T * (1 - tr) * 0.55f;
                                Cr += e * base.r; Cg += e * base.g; Cb += e * base.b; T *= tr;
                            }
                            V3 ph = ro + rd * th;
                            float e = 0.6f;
                            float qx = float(ph.x * Nf - 0.5), qy = float(ph.y * Nf - 0.5), qz = float(ph.z * Nf - 0.5);
                            float gx = tri(phi, N, qx + e, qy, qz) - tri(phi, N, qx - e, qy, qz);
                            float gy = tri(phi, N, qx, qy + e, qz) - tri(phi, N, qx, qy - e, qz);
                            float gz = tri(phi, N, qx, qy, qz + e) - tri(phi, N, qx, qy, qz - e);
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
                            if ((val > 0) == inside) val = inside ? -1e-4f : 1e-4f;
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

// Display copy of a voxel set at resolution D (= N, or N/2 for N > 128): box-averaged occupancy,
// smoothed for shading, plus a periodic distance field for sphere tracing. Untimed.
struct Display {
    int D = 0; size_t n = 0;
    std::vector<float> occ, rphi, dist;
    std::vector<uint8_t> chi;
    std::unique_ptr<Blur> br;
    void setup(int D_) {
        D = D_; n = (size_t)D * D * D;
        occ.assign(n, 0); rphi.assign(n, 0); dist.assign(n, 0); chi.assign(n, 0);
        br = std::make_unique<Blur>(D, 1.5);
    }
    void update(Pool& pool, const uint8_t* src, int N) {
        const int f = N / D; const float inv = 1.f / (f * f * f); const int Dd = D;
        pool.for_range(D * D, 8, [&](int a, int b) {
            for (int row = a; row < b; ++row) {
                int K = row / Dd, J = row % Dd;
                for (int I = 0; I < Dd; ++I) {
                    int s = 0;
                    for (int dz = 0; dz < f; ++dz)
                        for (int dy = 0; dy < f; ++dy) {
                            const uint8_t* r = src + ((size_t)(K * f + dz) * N + (J * f + dy)) * N + (size_t)I * f;
                            for (int dx = 0; dx < f; ++dx) s += r[dx];
                        }
                    float o = s * inv;
                    occ[(size_t)row * Dd + I] = o;
                    chi[(size_t)row * Dd + I] = o >= 0.5f;
                }
            }
        });
        br->apply(pool, occ.data(), rphi.data());
        distance(pool);
    }
    void distance(Pool& pool) {  // periodic EDT to interface voxels (from 07)
        const int Nn = D;
        const double INF = 1e9;
        std::vector<double> g(n);
        pool.for_range(D * D, 16, [&](int r0, int r1) {
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
            pool.for_range(D * D, 8, [&](int r0, int r1) {
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
        for (size_t i = 0; i < n; ++i) dist[i] = (float)std::sqrt(std::min(g[i], 1e6));
    }
};

// ---------------------------------------------------------------- video mode
struct StageRec {
    int N = 0;
    bool started = false, done = false, reached = false, capped = false;
    int it = 0, it_hit = -1;
    double wo_ms = 0, wo_hit = -1, with_ms = -1, with_first = -1, exL = 0, exR = 0;
    double iter_ms_sum = 0; int iter_n = 0;
    std::vector<double> with_samples;
};

int main(int argc, char** argv) {
    if (arg_flag(argc, argv, "--sweep")) return run_sweep(argc, argv);
    if (arg_flag(argc, argv, "--blur-check")) return blur_check(argc, argv);

    const std::vector<double> Nl = parse_list(arg_str(argc, argv, "--ns", "32,48,64,96,128,192,256"));
    const int S = (int)Nl.size();
    const int SF = arg_int(argc, argv, "--stage-frames", 240);
    const int OUTRO = arg_int(argc, argv, "--outro", 150);
    const double Vv = arg_double(argc, argv, "--v", 0.25);
    const unsigned seed = (unsigned)arg_int(argc, argv, "--seed", 1379);
    const double sig_phys = arg_double(argc, argv, "--sigma-phys", 1.0 / 16);
    const int threads = arg_int(argc, argv, "--threads", hw_threads());

    Config cfg;
    cfg.name = "L2-blob-ladder";
    cfg.title_left = "volume-preserving MBO from noise, run to 2 %";
    cfg.title_right = "certified minimizer, closed form (Thm intro:main)";
    cfg.caption = "Family 354 — Isoperimetric Conjecture for the Cubic Flat 3-Torus · PROVED: the ball/tube/slab of Thm intro:main is optimal and "
                  "I(V) (eq. intro:profile) is exact · MEASURED here: every ms (24 threads both sides; stop test untimed)";
    cfg.frames = S * SF + OUTRO;
    cfg.budget_ms = 16.7;
    cfg.show_speedup = false;
    Harness h(argc, argv, cfg);
    Pool pool(threads);

    h.left().set_compute_label("MBO step");
    h.right().set_compute_label("build");
    h.left().sparkline("");   // the strip chart below carries the history
    h.right().sparkline("");

    std::vector<StageRec> rec(S);
    int cur = -1;
    std::unique_ptr<MBO> mbo;
    std::unique_ptr<ClosedForm> cf;
    std::unique_ptr<AreaEst> an;
    std::vector<float> noise;
    Display dL, dR;
    Renderer rend;
    size_t k = 0; double Vq = 0, I = 0;
    std::string shapeL = "--"; bool shapeL_ok = false;
    int skip_every = 1;
    double last_iter_ms = 0;

    // ---------------- overlay: stage banner + building chart + table (drawn over the composed frame)
    h.on_overlay([&](Canvas& c) {
        const float X0 = 14, Y0 = 728, W = 1892, H = 280;
        c.fill_rounded_rect(X0, Y0, W, H, 12, rgb(0x080a0f), 0.93f);
        c.stroke_rect(X0, Y0, W, H, 1, pal::grid);
        const int s = std::clamp(cur, 0, S - 1);
        const int N = (int)Nl[s];
        c.text(X0 + 18, Y0 + 8, "time to reach within 2 % of the exact optimum I(V)  vs  grid resolution", 19, pal::text, Font::Bold);
        c.text(X0 + 18, Y0 + 34, fmt("log-log · measured in this run · V = %.2f (%s regime) · same noise seed at every grid · one MBO iteration per video frame", Vv,
                                     phase_name(Vv).c_str()), 14, pal::dim, Font::Sans);
        // chart
        const float cx0 = X0 + 92, cx1 = X0 + 1040, cy0 = Y0 + 66, cy1 = Y0 + H - 40;
        double ymin = 0.01, ymax = 1e4;
        for (auto& r : rec) if (r.started) ymax = std::max(ymax, std::max(r.wo_ms, r.wo_hit) * 3);
        ymax = std::pow(10, std::ceil(std::log10(ymax)));
        const double xmin = std::log(Nl.front() * 0.85), xmax = std::log(Nl.back() * 1.12);
        auto XP = [&](double n) { return float(cx0 + (cx1 - cx0) * (std::log(n) - xmin) / (xmax - xmin)); };
        auto YP = [&](double ms) { return float(cy1 - (cy1 - cy0) * (std::log10(std::max(ms, ymin)) - std::log10(ymin)) / (std::log10(ymax) - std::log10(ymin))); };
        for (double d = ymin; d <= ymax * 1.01; d *= 10) {
            c.line(cx0, YP(d), cx1, YP(d), 1, pal::grid, 0.9f);
            std::string lab = d < 1 ? fmt("%g ms", d) : d < 1000 ? fmt("%.0f ms", d) : fmt("%.0f s", d / 1000);
            c.text(cx0 - 8, YP(d) - 8, lab, 12, pal::dim, Font::Mono, Align::Right);
        }
        for (int i = 0; i < S; ++i) {
            float x = XP(Nl[i]);
            c.line(x, cy0, x, cy1, 1, pal::grid, i == s ? 0.9f : 0.4f);
            c.text(x, cy1 + 6, fmt("%d³", (int)Nl[i]), 13, i == s ? pal::text : pal::dim, Font::Mono, Align::Center);
        }
        c.text(cx1, cy1 + 22, "grid N³ (voxels = load)", 12, pal::dim, Font::Sans, Align::Right);
        // budget line
        {
            float y = YP(16.7);
            for (float x = cx0; x < cx1; x += 12) c.line(x, y, std::min(x + 6, cx1), y, 1.6f, pal::warn, 0.85f);
            c.text(cx0 + 6, y + 3, "16.7 ms = one 60 fps frame", 13, pal::warn, Font::Sans);
        }
        // series
        std::vector<Vec2> pw, pc;
        for (int i = 0; i < S; ++i) {
            auto& r = rec[i];
            if (!r.started) continue;
            float x = XP(Nl[i]);
            if (r.with_ms > 0) pc.push_back({x, YP(r.with_ms)});
            double wv = r.reached ? r.wo_hit : r.wo_ms;
            pw.push_back({x, YP(wv)});
        }
        if (pc.size() > 1) c.polyline(pc.data(), (int)pc.size(), 2.4f, pal::with, 0.95f);
        if (pw.size() > 1) c.polyline(pw.data(), (int)pw.size(), 2.4f, pal::without, 0.95f);
        bool crossW = false, crossC = false;
        for (int i = 0; i < S; ++i) {
            auto& r = rec[i];
            if (!r.started) continue;
            float x = XP(Nl[i]);
            if (r.with_ms > 0) {
                float y = YP(r.with_ms);
                c.circle(x, y, 5, pal::with);
                if (!crossC && r.with_ms > 16.7) {
                    crossC = true;
                    c.ring(x, y, 11, 2, pal::warn);
                    c.text(x + 14, y + 4, "WITH > 1 frame", 13, pal::with, Font::Bold);
                }
            }
            double wv = r.reached ? r.wo_hit : r.wo_ms;
            float y = YP(wv);
            if (r.reached) c.circle(x, y, 5.5f, pal::without);
            else if (r.done) { c.line(x - 6, y - 6, x + 6, y + 6, 2.4f, pal::bad); c.line(x - 6, y + 6, x + 6, y - 6, 2.4f, pal::bad); }
            else { float pulse = 0.5f + 0.5f * std::sin(h.frame() * 0.25f); c.ring(x, y, 6 + 3 * pulse, 2, pal::without); }
            if (!crossW && wv > 16.7 && (r.reached || r.done)) {
                crossW = true;
                c.ring(x, y, 12, 2, pal::warn);
                c.text(x + 14, y - 24, "WITHOUT > 1 frame", 13, pal::without, Font::Bold);
            }
            if (r.with_ms > 0 && (r.reached || r.done)) {
                std::string rt = r.reached ? fmt("×%s", group_int((long long)std::llround(r.wo_hit / r.with_ms)).c_str())
                                           : fmt("> ×%s", group_int((long long)std::llround(r.wo_ms / r.with_ms)).c_str());
                c.text(x, std::min(YP(r.with_ms), y) - 36, rt, 14, pal::text, Font::Bold, Align::Center);
            }
        }
        // legend
        c.circle(X0 + 1086, Y0 + 44, 5, pal::without);
        c.text(X0 + 1096, Y0 + 35, "WITHOUT: MBO ms to 2 % (✗ = not reached in the stage)", 13, pal::without, Font::Sans);
        c.circle(X0 + 1466, Y0 + 44, 5, pal::with);
        c.text(X0 + 1476, Y0 + 35, "WITH: median closed-form build ms", 13, pal::with, Font::Sans);
        // table
        const float tx = X0 + 1080;
        float ty = Y0 + 62;
        const float colx[6] = {tx, tx + 120, tx + 300, tx + 470, tx + 590, tx + 700};
        const char* hd[6] = {"grid", "MBO: it · ms to 2 %", "closed form ms", "ratio", "excess L", "excess R"};
        for (int i = 0; i < 6; ++i) c.text(colx[i], ty, hd[i], 13, pal::dim, Font::Sans);
        ty += 22;
        for (int i = 0; i < S; ++i) {
            auto& r = rec[i];
            Color tc = i == s ? pal::text : rgb(0xb8c0cf);
            c.text(colx[0], ty, fmt("%d³", (int)Nl[i]), 15, tc, Font::Mono);
            if (r.started) {
                std::string wo = r.reached ? fmt("%d · %s", r.it_hit, ms_str(r.wo_hit).c_str())
                                 : r.done  ? fmt("✗ %d · >%s", r.it, ms_str(r.wo_ms).c_str())
                                           : fmt("%d · %s…", r.it, ms_str(r.wo_ms).c_str());
                c.text(colx[1], ty, wo, 15, r.reached ? pal::without : (r.done ? pal::bad : pal::warn), Font::Mono);
                c.text(colx[2], ty, ms_str(r.with_ms), 15, pal::with, Font::Mono);
                if (r.reached || r.done)
                    c.text(colx[3], ty, (r.reached ? "" : ">") + fmt("×%.0f", (r.reached ? r.wo_hit : r.wo_ms) / r.with_ms), 15, tc, Font::Mono);
                c.text(colx[4], ty, fmt("%+.1f%%", 100 * r.exL), 15, r.exL <= 0.02 ? pal::good : pal::bad, Font::Mono);
                c.text(colx[5], ty, fmt("%+.1f%%", 100 * r.exR), 15, pal::good, Font::Mono);
            } else {
                c.text(colx[1], ty, "·", 15, pal::dim, Font::Mono);
            }
            ty += 24;
        }
        c.text(tx, Y0 + H - 22, "excess = A_est/I(V) − 1 (same estimator both sides; WITH's value is estimator bias)", 12, pal::dim, Font::Sans);
        // stage banner (top right of the strip)
        const long long nv = (long long)N * N * N;
        c.text(X0 + W - 18, Y0 + 8, fmt("stage %d / %d  ·  grid %d³ = %s voxels", s + 1, S, N, group_int(nv).c_str()), 20, pal::warn, Font::Bold, Align::Right);
    });

    while (h.next_frame()) {
        const int f = h.frame();
        const int s = std::min(S - 1, f / SF);
        const int local = f - s * SF;
        const bool outro = f >= S * SF;
        if (s != cur) {  // ---- stage setup (untimed)
            cur = s;
            const int N = (int)Nl[s];
            mbo.reset(); cf.reset(); an.reset();
            mbo = std::make_unique<MBO>(pool, N, sig_phys * N, arg_int(argc, argv, "--blur", 0));
            cf = std::make_unique<ClosedForm>(N);
            an = std::make_unique<AreaEst>(N);
            const size_t n = (size_t)N * N * N;
            k = (size_t)std::llround(Vv * n);
            Vq = double(k) / n; I = I_unit(Vq);
            make_noise(pool, N, seed, noise);
            // untimed warm-up of both sides (first-touch page faults of the fresh buffers), as in the sweep
            mbo->init(pool, noise.data(), k); mbo->step(pool, k);
            cf->build(pool, Vq, k);
            const int D = N <= 128 ? N : N / 2;
            dL.setup(D); dR.setup(D);
            rend.N = D;
            rec[s].N = N; rec[s].started = true;
            skip_every = 1;
            shapeL = "--";
        }
        StageRec& r = rec[s];
        const int N = r.N;
        bool changedL = false;
        // ---- WITHOUT (timed): initial threshold on frame 0, then one MBO iteration per frame until 2 %
        if (!outro && !r.done) {
            if (local == 0) {
                double t0 = now_ms();
                h.left().measure([&] { mbo->init(pool, noise.data(), k); });
                r.wo_ms += now_ms() - t0;
                changedL = true;
            } else if (local % skip_every == 0) {
                double t0 = now_ms();
                h.left().measure([&] { mbo->step(pool, k); });
                last_iter_ms = now_ms() - t0;
                r.wo_ms += last_iter_ms; r.iter_ms_sum += last_iter_ms; ++r.iter_n;
                r.it = (int)mbo->iters;
                changedL = true;
                skip_every = last_iter_ms > 250 ? std::min(30, (int)std::ceil(last_iter_ms / 50)) : 1;
            }
            if (changedL) {
                double A = an->area(pool, mbo->chi.data());  // oracle stop test: untimed
                r.exL = A / I - 1;
                if (r.exL <= 0.02) { r.reached = true; r.done = true; r.it_hit = r.it; r.wo_hit = r.wo_ms; }
            }
            if (!r.done && local + skip_every >= SF) { r.done = true; r.capped = true; }
        }
        // ---- WITH (timed): build the closed form every frame of the stage (identical result each time)
        if (!outro) {
            double t0 = now_ms();
            h.right().measure([&] { cf->build(pool, Vq, k); });
            double dt = now_ms() - t0;
            if (r.with_first < 0) r.with_first = dt;
            r.with_samples.push_back(dt); r.with_ms = quantile(r.with_samples, 0.5);  // median build so far this stage
            if (local == 0) { r.exR = an->area(pool, cf->chi.data()) / I - 1; dR.update(pool, cf->chi.data(), N); }
        }
        if (changedL || (local == 0 && !outro)) {
            dL.update(pool, mbo->chi.data(), N);
            ShapeInfo si = classify(dL.chi.data(), dL.D);
            shapeL = si.name; shapeL_ok = shape_matches(si, Vq);
        }

        // ---- draw (untimed)
        const double tsec = f / h.fps();
        rend.cam.orbit(V3{1, 1, 1}, 0.55 + 0.16 * tsec, 0.42, 6.0, 40 * PI / 180, 655, 650, 320);
        for (int side = 0; side < 2; ++side) {
            Panel& p = side ? h.right() : h.left();
            Display& d = side ? dR : dL;
            p.clear(pal::bg);
            rend.background(p);
            rend.render(pool, p, d.rphi.data(), d.dist.data(), side ? pal::with : pal::without);
            p.text(945, 12, fmt("2×2×2 periodic copies · shown at %d³%s", d.D, d.D < N ? fmt(" (solved at %d³)", N).c_str() : ""), 14, pal::dim,
                   Font::Sans, Align::Right);
        }
        // time-to-target counters
        {
            Panel& L = h.left();
            Color c = r.reached ? pal::good : (r.done ? pal::bad : pal::warn);
            L.text(940, 560, "time to within 2 % of I(V)  (MBO compute only)", 16, pal::dim, Font::Sans, Align::Right);
            std::string v = r.reached ? fmt("%s  ·  %d iterations", ms_str(r.wo_hit).c_str(), r.it_hit)
                            : r.done  ? fmt("not reached: %s, %d it", ms_str(r.wo_ms).c_str(), r.it)
                                      : fmt("%s  ·  %d it …", ms_str(r.wo_ms).c_str(), r.it);
            L.text(940, 582, v, 34, c, Font::Bold, Align::Right);
            if (skip_every > 1 && !r.done)
                L.text(940, 530, fmt("over budget: %s/iteration — updated every %dth frame", ms_str(last_iter_ms).c_str(), skip_every), 16, pal::bad,
                       Font::Bold, Align::Right);
            Panel& R = h.right();
            R.text(940, 560, "time to within 2 % of I(V)  (one closed-form build)", 16, pal::dim, Font::Sans, Align::Right);
            R.text(940, 582, fmt("%s  ·  0 iterations", ms_str(r.with_ms).c_str()), 34, pal::good, Font::Bold, Align::Right);
        }
        Panel& L = h.left();
        Panel& R = h.right();
        L.metric_text("grid", fmt("%d³ = %s voxels", N, group_int((long long)N * N * N).c_str()));
        L.metric("MBO iterations", (double)r.it, "%.0f");
        L.metric("A/I(V)−1 %", 100 * r.exL, "%+.2f", r.exL > 0.02 ? Tone::Bad : Tone::Good);
        L.metric_text("shape (display grid)", shapeL + (shapeL_ok ? " ✓" : " ✗"), shapeL_ok ? Tone::Good : Tone::Bad);
        L.metric_text("σ (fixed physical)", fmt("1/16 cell = %.1f vox", sig_phys * N));
        L.metric_text("stop test (untimed)", "oracle A ≤ 1.02·I(V)");
        R.metric_text("grid", fmt("%d³ = %s voxels", N, group_int((long long)N * N * N).c_str()));
        R.metric("volume V", Vq, "%.4f");
        R.metric("A/I(V)−1 %", 100 * r.exR, "%+.2f", Tone::Good);
        R.metric_text("certified shape", phase_name(Vq), Tone::Accent);
        R.metric("exact I(V), eq. intro:profile", I, "%.4f", Tone::Accent);
        R.metric_text("rebuilt every frame", "same voxels each time");
    }
    for (int i = 0; i < S; ++i) {
        auto& r = rec[i];
        h.result(fmt("stage%d_N", i + 1), r.N);
        h.result(fmt("stage%d_without_reached", i + 1), r.reached ? 1.0 : 0.0);
        h.result(fmt("stage%d_without_ms_to_2pct", i + 1), r.reached ? r.wo_hit : -1.0);
        h.result(fmt("stage%d_without_iterations", i + 1), r.reached ? r.it_hit : r.it);
        h.result(fmt("stage%d_without_ms_per_iteration_mean", i + 1), r.iter_n ? r.iter_ms_sum / r.iter_n : -1.0);
        h.result(fmt("stage%d_with_first_build_ms", i + 1), r.with_first);
        h.result(fmt("stage%d_with_median_build_ms", i + 1), quantile(r.with_samples, 0.5));
        h.result(fmt("stage%d_excess_without", i + 1), r.exL);
        h.result(fmt("stage%d_excess_with", i + 1), r.exR);
    }
    h.result("volume_V", Vv);
    h.result("seed", (double)seed);
    h.result("sigma_physical", sig_phys);
    h.result("threads", threads);
    return h.finish();
}
