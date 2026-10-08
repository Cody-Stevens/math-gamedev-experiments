// fk.h - critical (q = 1) square-lattice FK Dobrushin interface in the unit square,
// built exactly as in Family 223, 01-introduction.tex, "The lattice model and the main result":
//
//   * primal vertices n^{-1} Z^2 ∩ [0,1]^2, all nearest-neighbour edges;
//   * marked corners a = (0,0), b = (1,1); F = bottom + right sides (counterclockwise a -> b),
//     W = top + left sides;
//   * W boundary edges are removed from the sampled set and declared OPEN for drawing;
//     every other edge (interior and F boundary) is sampled. At q = 1, p_q = 1/2 and the
//     cluster factor q^{k} is 1, so edges are independent fair bits (eq:parameters);
//   * Dobrushin medial strand from a to b: at an open primal edge the four medial half-edges
//     are paired in the two turns around the adjacent dual faces (the strand stays in its
//     square), at a closed edge in the turns around the primal endpoints (the strand crosses).
//     Outside the square we place a one-square collar whose edges are open on the W side
//     (primal wired arc) and closed on the F side (dual wired arc); its medial edges are the
//     "boundary traversals". The two terminal half-edges sit in the corner squares at a and b
//     where the two boundary conditions meet.
//   * N_n counts full medial-edge traversals, boundary traversals included, terminal
//     half-edges excluded (eq:counting-measure). A medial vertex may be visited twice.
//   * Positions are projected by Pi(u,v) = clamp to [0,1]^2.
//
// Edges are sampled lazily from a counter-based hash, so a sample is a deterministic function
// of (seed, n) and exploring only the edges the strand touches is exact for independent bits.
#pragma once
#include <cstdint>
#include <vector>

namespace fk {

static inline uint64_t mix64(uint64_t z) {
    z += 0x9e3779b97f4a7c15ull;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

// Sides of a square: 0 = B, 1 = R, 2 = T, 3 = L.  Corners: 0 = BL, 1 = BR, 2 = TR, 3 = TL.
// Corner k touches sides {B,L},{B,R},{R,T},{T,L}.
struct Tracer {
    int n = 0;
    uint64_t key = 0;

    // primal edge state; horizontal (i,j)-(i+1,j) or vertical (i,j)-(i,j+1)
    bool open_h(int i, int j) const {
        if (i >= 0 && i + 1 <= n && j >= 0 && j <= n) {
            if (j == n) return true;  // top side: W, declared open
            return bit(uint64_t((j + 2) * (n + 4) + (i + 2)) * 2);
        }
        return i < 0 || j > n;  // collar: W side open, F side closed
    }
    bool open_v(int i, int j) const {
        if (i >= 0 && i <= n && j >= 0 && j + 1 <= n) {
            if (i == 0) return true;  // left side: W, declared open
            return bit(uint64_t((j + 2) * (n + 4) + (i + 2)) * 2 + 1);
        }
        return i < 0 || j + 1 > n;
    }
    bool bit(uint64_t id) const { return mix64(key ^ (id * 0xd1342543de82ef95ull)) >> 63; }

    // Trace the strand. For every full traversal calls visit(x, y) with the projected midpoint
    // of the medial edge in unit-square coordinates. If path != nullptr, it receives the
    // projected medial vertices (polyline, interleaved x,y). Returns N_n (or -1 on error).
    template <class Visit>
    long long trace(Visit&& visit, std::vector<float>* path = nullptr) const {
        static const int side_a[4] = {0, 0, 1, 2}, side_b[4] = {3, 1, 2, 3};
        static const int cdx[4] = {0, 1, 1, 0}, cdy[4] = {0, 0, 1, 1};
        const float inv = 1.f / float(n);
        auto clampu = [](float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); };
        int sx = 0, sy = -1, k = 3, sin = 3;  // start: square (0,-1), corner TL, entering from L
        if (path) {
            path->clear();
            path->push_back(0.f); path->push_back(0.f);  // terminal connector to a
        }
        long long N = 0;
        const long long cap = 64ll * (n + 2) * (n + 2);
        while (true) {
            // full traversal of medial edge (S, k)
            ++N;
            float mx = (float(sx + cdx[k]) + float(sx) + 0.5f) * 0.5f, my = (float(sy + cdy[k]) + float(sy) + 0.5f) * 0.5f;
            visit(clampu(mx * inv), clampu(my * inv));
            int sout = (side_a[k] == sin) ? side_b[k] : side_a[k];
            // exit side midpoint + primal edge
            int ux = sx + cdx[k], uy = sy + cdy[k];  // corner vertex u
            int vx, vy, nsx = sx, nsy = sy;
            bool open;
            float ex, ey;
            switch (sout) {
                case 0: open = open_h(sx, sy);     ex = sx + .5f; ey = float(sy);     vx = (ux == sx) ? sx + 1 : sx; vy = sy;     nsy = sy - 1; break;
                case 2: open = open_h(sx, sy + 1); ex = sx + .5f; ey = float(sy + 1); vx = (ux == sx) ? sx + 1 : sx; vy = sy + 1; nsy = sy + 1; break;
                case 3: open = open_v(sx, sy);     ex = float(sx); ey = sy + .5f;     vx = sx;     vy = (uy == sy) ? sy + 1 : sy; nsx = sx - 1; break;
                default: open = open_v(sx + 1, sy); ex = float(sx + 1); ey = sy + .5f; vx = sx + 1; vy = (uy == sy) ? sy + 1 : sy; nsx = sx + 1; break;
            }
            if (path) { path->push_back(clampu(ex * inv)); path->push_back(clampu(ey * inv)); }
            int wx, wy;
            if (open) {  // stay in S, turn around the other endpoint v
                wx = vx; wy = vy; sin = sout;
            } else {     // cross into the neighbouring square, keep turning around u
                sx = nsx; sy = nsy; wx = ux; wy = uy; sin = sout ^ 2;
            }
            int dx = wx - sx, dy = wy - sy;
            k = dy == 0 ? (dx == 0 ? 0 : 1) : (dx == 1 ? 2 : 3);
            if (sx == n && sy == n) {  // terminal half-edge at b
                if (path) { path->push_back(1.f); path->push_back(1.f); }
                return (k == 0) ? N : -1;
            }
            if ((sx == -1 && sy == -1) || N > cap) return -1;
        }
    }
};

}  // namespace fk
