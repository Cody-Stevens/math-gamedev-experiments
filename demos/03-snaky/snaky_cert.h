// snaky_cert.h - C++ port of the 21-move Snaky certificate (Family 187).
//
// Reconstruction follows verification/supporting/route21/primary.py line by line:
//   * bases (eq:bases):        card j = (S \ {s_j}, S, 1), j = 0..5
//   * combination (eq:combination): T = {p} u U T_i,  A = (U A_i  u  n T_i) \ {p},  h = 1 + max h_i
//   * placements (eq:symmetries):   negate x if bit 2, negate y if bit 4, THEN swap if bit 1, then translate
//   * inline "(XY ...)" coordinates stay in the enclosing line's frame (not pivot offsets)
// The policy follows build/strategy.tex (proof of Thm 1 + Cor. cor:finite-board):
//   claim the placed pivot F(p) (or, if Maker already owns it, the first free cell of T_727 in a
//   fixed order); after Breaker's reply choose the FIRST child whose placed envelope avoids it;
//   reference child j:G -> placement F o G, inline child -> keep F. At a base, claim the missing cell.
#pragma once
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace snaky {

constexpr int N = 17;            // board {0..16}^2 (Cor. cor:finite-board)
constexpr int NC = N * N;        // 289 cells; cell index = x*17 + y (so bit order == lexicographic (x,y))
inline int cid(int x, int y) { return x * N + y; }
inline bool onb(int x, int y) { return x >= 0 && y >= 0 && x < N && y < N; }

static const int SNAKE[6][2] = {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {3, 1}, {4, 1}};  // eq:target

// 289-bit set
struct Bits {
    uint64_t w[5] = {0, 0, 0, 0, 0};
    void set(int i) { w[i >> 6] |= 1ull << (i & 63); }
    void reset(int i) { w[i >> 6] &= ~(1ull << (i & 63)); }
    bool test(int i) const { return (w[i >> 6] >> (i & 63)) & 1; }
    Bits operator|(const Bits& o) const { Bits r; for (int k = 0; k < 5; ++k) r.w[k] = w[k] | o.w[k]; return r; }
    Bits operator&(const Bits& o) const { Bits r; for (int k = 0; k < 5; ++k) r.w[k] = w[k] & o.w[k]; return r; }
    Bits minus(const Bits& o) const { Bits r; for (int k = 0; k < 5; ++k) r.w[k] = w[k] & ~o.w[k]; return r; }
    bool subset_of(const Bits& o) const { for (int k = 0; k < 5; ++k) if (w[k] & ~o.w[k]) return false; return true; }
    bool any() const { return w[0] | w[1] | w[2] | w[3] | w[4]; }
    bool operator==(const Bits& o) const { return std::memcmp(w, o.w, sizeof w) == 0; }
    int count() const { int c = 0; for (int k = 0; k < 5; ++k) c += __builtin_popcountll(w[k]); return c; }
    template <class F> void each(F&& f) const {
        for (int k = 0; k < 5; ++k) for (uint64_t m = w[k]; m; m &= m - 1) f(k * 64 + __builtin_ctzll(m));
    }
};

// Affine map x -> R x + t with R a signed permutation (group G of the paper).
struct Aff {
    int a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0;  // (x,y) -> (a x + b y + tx, c x + d y + ty)
    void apply(int x, int y, int& ox, int& oy) const { ox = a * x + b * y + tx; oy = c * x + d * y + ty; }
    void inv_apply(int X, int Y, int& ox, int& oy) const {  // R^-1 = R^T for signed permutations
        int u = X - tx, v = Y - ty;
        ox = a * u + c * v; oy = b * u + d * v;
    }
    Aff then_inner(const Aff& g) const {  // this o g  (apply g first)
        Aff r;
        r.a = a * g.a + b * g.c; r.b = a * g.b + b * g.d;
        r.c = c * g.a + d * g.c; r.d = c * g.b + d * g.d;
        r.tx = a * g.tx + b * g.ty + tx; r.ty = c * g.tx + d * g.ty + ty;
        return r;
    }
    static Aff from_code(int s, int dx, int dy) {  // eq:symmetries: negate x (bit 2), negate y (bit 4), then swap (bit 1)
        int sx = (s & 2) ? -1 : 1, sy = (s & 4) ? -1 : 1;
        Aff r;
        if (s & 1) { r.a = 0; r.b = sy; r.c = sx; r.d = 0; }  // (x,y) -> (sy*y, sx*x)
        else { r.a = sx; r.b = 0; r.c = 0; r.d = sy; }
        r.tx = dx; r.ty = dy;
        return r;
    }
};

struct Child {
    bool ref = true;
    int idx = 0;      // ref: card number; inline: node id
    Aff g;            // ref placement (identity for inline)
};

struct Node {
    int px = 0, py = 0;            // pivot in the line's frame (bases: the missing cell s_j)
    bool base = false;
    int h = 1;
    Bits A, T;
    std::vector<Child> kids;
};

struct Certificate {
    std::vector<Node> nodes;       // bases 0..5, then every combination node in postorder
    std::vector<int> card_node;    // card j -> node id
    std::vector<int> postorder;    // combination nodes (numbered and inline), postorder
    std::vector<int> t727_order;   // T_727 cells in lexicographic order (replacement ordering)
    long long references = 0, replies = 0, inline_nodes = 0;
    size_t text_bytes_raw = 0, text_bytes_lf = 0;
    std::string sha_lf, err;
    bool ok = false;

    // ---- placement of a stored set by an affine map; returns false if a cell leaves the board
    static bool place(const Bits& s, const Aff& f, Bits& out) {
        out = Bits{};
        bool good = true;
        s.each([&](int i) {
            int x, y; f.apply(i / N, i % N, x, y);
            if (!onb(x, y)) good = false; else out.set(cid(x, y));
        });
        return good;
    }

    // runtime memory actually needed by the policy (pivot, height, T, child list per node)
    size_t runtime_bytes() const {
        size_t b = 0;
        for (auto& n : nodes) b += 2 + 1 + sizeof(Bits) + n.kids.size() * (2 + 1 + 2);  // card/node id, sym, (dx,dy)
        return b + card_node.size() * 2;
    }

    bool fail(const std::string& m) { err = m; ok = false; return false; }

    bool load(const char* path) {
        FILE* f = std::fopen(path, "rb");
        if (!f) return fail(std::string("cannot open ") + path);
        std::string raw;
        char buf[65536];
        size_t k;
        while ((k = std::fread(buf, 1, sizeof buf, f)) > 0) raw.append(buf, k);
        std::fclose(f);
        text_bytes_raw = raw.size();
        std::string lf;  // CRLF -> LF in memory (as the verifier did)
        lf.reserve(raw.size());
        for (size_t i = 0; i < raw.size(); ++i)
            if (!(raw[i] == '\r' && i + 1 < raw.size() && raw[i + 1] == '\n')) lf.push_back(raw[i]);
        text_bytes_lf = lf.size();
        sha_lf = sha256_hex(lf);
        return parse(lf);
    }

    // ---- tokenizer helpers
    static int sym(char c) {
        static const char* S = "0123456789ABCDEFG";
        const char* p = std::strchr(S, c);
        return (p && c) ? int(p - S) : -1;
    }

    bool parse(const std::string& data) {
        if (data.empty() || data.back() != '\n') return fail("missing final newline");
        nodes.clear(); card_node.clear(); postorder.clear();
        references = replies = inline_nodes = 0;
        Bits S;
        for (auto& s : SNAKE) S.set(cid(s[0], s[1]));
        for (int j = 0; j < 6; ++j) {  // Lemma lem:bases
            Node n; n.base = true; n.px = SNAKE[j][0]; n.py = SNAKE[j][1]; n.h = 1;
            n.T = S; n.A = S; n.A.reset(cid(n.px, n.py));
            nodes.push_back(n); card_node.push_back(j);
        }
        size_t pos = 0;
        int rows = 0;
        while (pos < data.size()) {
            size_t e = data.find('\n', pos);
            std::string line = data.substr(pos, e - pos);
            pos = e + 1;
            std::vector<std::string> tok;
            std::string cur;
            for (char c : line) {
                if (c == '(' || c == ')' || c == ' ') {
                    if (!cur.empty()) tok.push_back(cur), cur.clear();
                    if (c != ' ') tok.push_back(std::string(1, c));
                } else cur.push_back(c);
            }
            if (!cur.empty()) tok.push_back(cur);
            if (tok.size() < 3 || tok[0] != std::to_string(card_node.size())) return fail("bad row index at row " + std::to_string(rows));
            size_t p = 1;
            int id = expression(tok, p, false);
            if (id < 0) return false;
            if (p != tok.size()) return fail("unconsumed tokens");
            card_node.push_back(id);
            ++rows;
        }
        if (rows != 722) return fail("expected 722 rows");
        if (card_node.size() != 728 || postorder.size() != 1620) return fail("wrong cardinalities");
        if (references != 4089 || replies != 37042) return fail("wrong reference/reply counts");
        const Node& fin = nodes[card_node[727]];
        if (fin.A.any() || fin.T.count() != 251 || fin.h != 21 || fin.px != 8 || fin.py != 8) return fail("wrong final card");
        int hmax = 0;
        for (int j = 0; j < 728; ++j) hmax = std::max(hmax, nodes[card_node[j]].h);
        if (hmax != 21) return fail("wrong maximum height");
        // elementary cards 6 and 7 (eq:first-card and the A_7/T_7 display)
        {
            Bits a6, t6, a7, t7;
            for (int y = 0; y < 5; ++y) a6.set(cid(0, y));
            t6 = a6; t6.set(cid(1, 3)); t6.set(cid(1, 4)); t6.set(cid(1, 5));
            for (int y = 1; y < 5; ++y) a7.set(cid(0, y));
            for (int y = 1; y < 6; ++y) t7.set(cid(0, y));
            for (int y : {0, 1, 2, 4, 5, 6}) t7.set(cid(1, y));
            const Node &n6 = nodes[card_node[6]], &n7 = nodes[card_node[7]];
            if (!(n6.A == a6 && n6.T == t6 && n6.h == 2)) return fail("wrong card 6");
            if (!(n7.A == a7 && n7.T == t7 && n7.h == 3)) return fail("wrong card 7");
        }
        // Table tab:top: cumulative intersections of the 32 children of card 727
        {
            const int groups[6][6] = {{648, cid(4, 5), 87, 15, 4, 35}, {708, cid(8, 8), 225, 20, 4, 31},
                                      {712, cid(5, 5), 96, 15, 8, 21}, {713, cid(5, 5), 98, 15, 4, 12},
                                      {725, cid(7, 7), 167, 20, 8, 4}, {726, cid(7, 7), 169, 20, 4, 0}};
            if (fin.kids.size() != 32) return fail("wrong final degree");
            Bits common; bool first = true; size_t used = 0;
            Bits only88; only88.set(cid(8, 8));
            for (auto& g : groups) {
                const Node& c = nodes[card_node[g[0]]];
                Bits sole; sole.set(g[1]);
                if (!(c.A == sole) || c.T.count() != g[2] || c.h != g[3]) return fail("wrong group card");
                for (int q = 0; q < g[4]; ++q, ++used) {
                    const Child& ch = fin.kids[used];
                    if (!ch.ref || ch.idx != g[0]) return fail("wrong final child");
                    Bits pa, pt;
                    place(c.A, ch.g, pa); place(c.T, ch.g, pt);
                    if (!(pa == only88)) return fail("final child requirement");
                    common = first ? pt : (common & pt); first = false;
                }
                if (common.minus(only88).count() != g[5]) return fail("wrong cumulative intersection");
            }
            if (!(common == only88)) return fail("wrong final intersection");
        }
        // cards / postorder digests in the verifier's JSON serialization (data-manifest.json)
        cards_sha = digest_nodes(card_node);
        postorder_sha = digest_nodes(postorder);
        t727_order.clear();
        fin.T.each([&](int i) { t727_order.push_back(i); });  // bit order == lexicographic (x,y)
        ok = true;
        return true;
    }
    std::string cards_sha, postorder_sha;

    // Recursive evaluation of "pivot child child ..." (primary.py: expression()).
    int expression(const std::vector<std::string>& tok, size_t& p, bool nested) {
        if (p >= tok.size() || tok[p].size() != 2 || sym(tok[p][0]) < 0 || sym(tok[p][1]) < 0) { fail("bad pivot"); return -1; }
        Node n;
        n.px = sym(tok[p][0]); n.py = sym(tok[p][1]);
        ++p;
        std::vector<Bits> ca, ct;
        std::vector<int> ch;
        while (p < tok.size() && tok[p] != ")") {
            Child c;
            Bits a, t; int h;
            if (tok[p] == "(") {
                ++p;
                int id = expression(tok, p, true);
                if (id < 0) return -1;
                c.ref = false; c.idx = id;
                a = nodes[id].A; t = nodes[id].T; h = nodes[id].h;
                ++inline_nodes;
            } else {
                const std::string& s = tok[p++];
                size_t colon = s.find(':');
                std::string num = s.substr(0, colon);
                if (num.empty() || (num.size() > 1 && num[0] == '0')) { fail("bad ref " + s); return -1; }
                for (char d : num) if (d < '0' || d > '9') { fail("bad ref " + s); return -1; }
                int j = std::stoi(num);
                if (j >= int(card_node.size())) { fail("reference is not backward"); return -1; }
                int sc = 0, dx = 0, dy = 0;
                if (colon != std::string::npos) {
                    if (s.size() != colon + 4 || s[colon + 1] < '0' || s[colon + 1] > '7' || sym(s[colon + 2]) < 0 || sym(s[colon + 3]) < 0) { fail("bad placement " + s); return -1; }
                    sc = s[colon + 1] - '0'; dx = sym(s[colon + 2]); dy = sym(s[colon + 3]);
                }
                c.ref = true; c.idx = j; c.g = Aff::from_code(sc, dx, dy);
                const Node& src = nodes[card_node[j]];
                if (!place(src.A, c.g, a) || !place(src.T, c.g, t)) { fail("placement leaves the 17x17 frame"); return -1; }
                h = src.h;
                ++references;
            }
            n.kids.push_back(c);
            ca.push_back(a); ct.push_back(t); ch.push_back(h);
        }
        if (nested) {
            if (p >= tok.size() || tok[p] != ")") { fail("unclosed inline expression"); return -1; }
            ++p;
        } else if (p != tok.size()) { fail("unmatched closing parenthesis"); return -1; }
        if (n.kids.empty()) { fail("empty child list"); return -1; }
        // eq:combination
        Bits ua, it = ct[0], ut;
        int hm = 0;
        for (size_t i = 0; i < ca.size(); ++i) { ua = ua | ca[i]; it = it & ct[i]; ut = ut | ct[i]; hm = std::max(hm, ch[i]); }
        Bits piv; piv.set(cid(n.px, n.py));
        n.A = (ua | it).minus(piv);
        n.T = ut | piv;
        n.h = 1 + hm;
        if (!n.A.subset_of(n.T)) { fail("requirement outside envelope"); return -1; }
        Bits owned = n.A | piv;
        for (size_t i = 0; i < ca.size(); ++i)
            if (!ca[i].subset_of(owned) || !ct[i].subset_of(n.T) || ch[i] >= n.h) { fail("invalid child"); return -1; }
        bool bad = false;
        n.T.minus(owned).each([&](int r) {  // every internal reply leaves some child alive
            bool cov = false;
            for (auto& t : ct) if (!t.test(r)) { cov = true; break; }
            if (!cov) bad = true;
            ++replies;
        });
        if (bad) { fail("uncovered internal reply"); return -1; }
        ++replies;  // the exterior reply class
        nodes.push_back(std::move(n));
        int id = int(nodes.size()) - 1;
        postorder.push_back(id);
        return id;
    }

    // [{"A":[[x,y],...],"T":[...],"height":h},...] with sort_keys and (',',':') separators
    std::string digest_nodes(const std::vector<int>& ids) const {
        std::string s = "[";
        auto pts = [&](const Bits& b) {
            s += '[';
            bool first = true;
            b.each([&](int i) {
                if (!first) s += ',';
                first = false;
                s += '[' + std::to_string(i / N) + ',' + std::to_string(i % N) + ']';
            });
            s += ']';
        };
        for (size_t k = 0; k < ids.size(); ++k) {
            const Node& n = nodes[ids[k]];
            if (k) s += ',';
            s += "{\"A\":"; pts(n.A); s += ",\"T\":"; pts(n.T); s += ",\"height\":" + std::to_string(n.h) + "}";
        }
        s += "]";
        return sha256_hex(s);
    }

    // ---------------------------------------------------------------- SHA-256 (FIPS 180-4)
    static std::string sha256_hex(const std::string& msg) {
        static const uint32_t K[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be,
            0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa,
            0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85,
            0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
            0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
            0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        uint32_t H[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
        std::string m = msg;
        uint64_t bitlen = uint64_t(msg.size()) * 8;
        m.push_back(char(0x80));
        while (m.size() % 64 != 56) m.push_back(0);
        for (int i = 7; i >= 0; --i) m.push_back(char((bitlen >> (8 * i)) & 255));
        auto ror = [](uint32_t x, int r) { return (x >> r) | (x << (32 - r)); };
        for (size_t off = 0; off < m.size(); off += 64) {
            uint32_t w[64];
            for (int i = 0; i < 16; ++i)
                w[i] = (uint32_t(uint8_t(m[off + 4 * i])) << 24) | (uint32_t(uint8_t(m[off + 4 * i + 1])) << 16) |
                       (uint32_t(uint8_t(m[off + 4 * i + 2])) << 8) | uint32_t(uint8_t(m[off + 4 * i + 3]));
            for (int i = 16; i < 64; ++i) {
                uint32_t s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
                uint32_t s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
                w[i] = w[i - 16] + s0 + w[i - 7] + s1;
            }
            uint32_t a = H[0], b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];
            for (int i = 0; i < 64; ++i) {
                uint32_t S1 = ror(e, 6) ^ ror(e, 11) ^ ror(e, 25), chh = (e & f) ^ (~e & g);
                uint32_t t1 = h + S1 + chh + K[i] + w[i];
                uint32_t S0 = ror(a, 2) ^ ror(a, 13) ^ ror(a, 22), maj = (a & b) ^ (a & c) ^ (b & c);
                uint32_t t2 = S0 + maj;
                h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
            }
            H[0] += a; H[1] += b; H[2] += c; H[3] += d; H[4] += e; H[5] += f; H[6] += g; H[7] += h;
        }
        char out[65];
        for (int i = 0; i < 8; ++i) std::snprintf(out + 8 * i, 9, "%08x", H[i]);
        return std::string(out, 64);
    }
};

// ---------------------------------------------------------------- the policy (strategy.tex)
// own[c]: 0 free, 1 Maker, 2 Breaker.
struct CertPolicy {
    const Certificate* C = nullptr;
    int node = 0;
    Aff F;                     // placement of the active node's frame onto the board
    int replacements = 0;      // pivot already owned -> fresh claim (counted as a real move)
    int invariant_breaks = 0;  // must stay 0 (checked outside timing)

    void reset(const Certificate& c) { C = &c; node = c.card_node[727]; F = Aff{}; replacements = 0; invariant_breaks = 0; }
    int height() const { return C->nodes[node].h; }

    int decide(const uint8_t* own) {
        const Node& n = C->nodes[node];
        if (n.base) {  // placed base: claim the one missing target cell
            for (auto& s : SNAKE) {
                int x, y; F.apply(s[0], s[1], x, y);
                int c = cid(x, y);
                if (own[c] != 1) return c;   // free by the invariant (Breaker owns none of F(S))
            }
            return first_free(own);          // target already complete (formal continuation)
        }
        int x, y; F.apply(n.px, n.py, x, y);
        int c = cid(x, y);
        if (own[c] == 0) return c;
        ++replacements;                      // pivot already Maker's: one actual fresh claim
        return first_free(own);
    }
    int first_free(const uint8_t* own) const {
        for (int c : C->t727_order) if (own[c] == 0) return c;
        for (int c = 0; c < NC; ++c) if (own[c] == 0) return c;
        return -1;
    }
    // Breaker replied at cell b: pass to the first child whose placed envelope avoids b.
    void observe(int b) {
        const Node& n = C->nodes[node];
        if (n.base) return;
        int lx, ly; F.inv_apply(b / N, b % N, lx, ly);  // reply in the node's frame
        for (const Child& k : n.kids) {
            if (k.ref) {
                const Node& src = C->nodes[C->card_node[k.idx]];
                int cx, cy; k.g.inv_apply(lx, ly, cx, cy);
                if (onb(cx, cy) && src.T.test(cid(cx, cy))) continue;
                F = F.then_inner(k.g);
                node = C->card_node[k.idx];
                return;
            } else {
                if (onb(lx, ly) && C->nodes[k.idx].T.test(cid(lx, ly))) continue;
                node = k.idx;
                return;
            }
        }
        ++invariant_breaks;  // impossible by Lemma lem:combination
    }
    // Definition def:claim invariant: F(A) subset of Maker, F(T) disjoint from Breaker (untimed check).
    bool invariant(const uint8_t* own) const {
        const Node& n = C->nodes[node];
        bool good = true;
        n.A.each([&](int i) { int x, y; F.apply(i / N, i % N, x, y); if (!onb(x, y) || own[cid(x, y)] != 1) good = false; });
        n.T.each([&](int i) { int x, y; F.apply(i / N, i % N, x, y); if (!onb(x, y) || own[cid(x, y)] == 2) good = false; });
        return good;
    }
    // placed envelope / pivot for drawing
    void placed_T(Bits& out) const { Certificate::place(C->nodes[node].T, F, out); }
    int placed_pivot() const {
        const Node& n = C->nodes[node];
        int x, y; F.apply(n.px, n.py, x, y);
        return onb(x, y) ? cid(x, y) : -1;
    }
    // which child (and its height) would survive reply b (used only by the stress adversary)
    int surviving_height(int b) const {
        const Node& n = C->nodes[node];
        if (n.base) return 0;
        int lx, ly; F.inv_apply(b / N, b % N, lx, ly);
        for (const Child& k : n.kids) {
            const Node* src; int cx = lx, cy = ly;
            if (k.ref) { src = &C->nodes[C->card_node[k.idx]]; k.g.inv_apply(lx, ly, cx, cy); }
            else src = &C->nodes[k.idx];
            if (onb(cx, cy) && src->T.test(cid(cx, cy))) continue;
            return src->h;
        }
        return 99;
    }
};

}  // namespace snaky
