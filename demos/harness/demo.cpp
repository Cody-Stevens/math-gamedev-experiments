// demo.cpp - implementation of demo.h (see the header for the API and the honesty rule).
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <intrin.h>

#include "demo.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <io.h>
#include <filesystem>
#include <mutex>
#include <thread>
#include <unordered_map>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wall"
#pragma clang diagnostic ignored "-Wextra"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "vendor/stb_truetype.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include "vendor/stb_image_write.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

namespace fs = std::filesystem;

namespace demo {

// =============================================================== small utils
static inline float clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

std::string fmt(const char* f, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, f);
    int n = vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    if (n < 0) return {};
    if (n < (int)sizeof buf) return std::string(buf, n);
    std::string s(n + 1, '\0');
    va_start(ap, f);
    vsnprintf(s.data(), s.size(), f, ap);
    va_end(ap);
    s.resize(n);
    return s;
}

static std::string si_fmt(double v) {
    double a = std::fabs(v);
    const char* suf[] = {"", "k", "M", "G", "T"};
    int k = 0;
    while (a >= 1000 && k < 4) { a /= 1000; v /= 1000; ++k; }
    if (k == 0) return std::fabs(v) == std::floor(std::fabs(v)) ? fmt("%.0f", v) : fmt("%.3g", v);
    return fmt(a < 10 ? "%.2f%s" : (a < 100 ? "%.1f%s" : "%.0f%s"), v, suf[k]);
}

static std::string ms_fmt(double ms) {
    if (ms < 1) return fmt("%.3f", ms);
    if (ms < 10) return fmt("%.2f", ms);
    if (ms < 100) return fmt("%.1f", ms);
    return fmt("%.0f", ms);
}

static std::string group_int(double v) {  // 12345 -> "12,345"
    if (!std::isfinite(v)) return "inf";
    long long x = (long long)std::llround(v);
    std::string s = std::to_string(x < 0 ? -x : x), o;
    int c = 0;
    for (int i = (int)s.size() - 1; i >= 0; --i) {
        o.push_back(s[i]);
        if (++c % 3 == 0 && i > 0) o.push_back(',');
    }
    if (x < 0) o.push_back('-');
    std::reverse(o.begin(), o.end());
    return o;
}

double now_ms() {
    static LARGE_INTEGER freq = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f; }();
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return double(t.QuadPart) * 1000.0 / double(freq.QuadPart);
}

std::string cpu_name() {
    int regs[4] = {};
    char brand[49] = {};
    __cpuid(regs, 0x80000000);
    if ((unsigned)regs[0] >= 0x80000004u) {
        for (int i = 0; i < 3; ++i) {
            __cpuid(regs, 0x80000002 + i);
            memcpy(brand + 16 * i, regs, 16);
        }
    }
    std::string s(brand);
    while (!s.empty() && s.back() == ' ') s.pop_back();
    size_t p = s.find_first_not_of(' ');
    return p == std::string::npos ? std::string("unknown") : s.substr(p);
}

int hw_threads() { return (int)std::thread::hardware_concurrency(); }

// =============================================================== colors
Color lerp(Color a, Color b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
Color scale(Color c, float k) { return {c.r * k, c.g * k, c.b * k}; }
Color hsv(float h, float s, float v) {
    h = h - std::floor(h);
    float i = std::floor(h * 6), f = h * 6 - i;
    float p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s);
    switch (int(i) % 6) {
        case 0: return {v, t, p};
        case 1: return {q, v, p};
        case 2: return {p, v, t};
        case 3: return {p, q, v};
        case 4: return {t, p, v};
        default: return {v, p, q};
    }
}
Color pal::cat(int i) {
    static const Color c[8] = {rgb(0x6cb6ff), rgb(0xf2777a), rgb(0x5be49b), rgb(0xffc861),
                               rgb(0xb392f0), rgb(0xff9b54), rgb(0x56d4dd), rgb(0xf778ba)};
    return c[((i % 8) + 8) % 8];
}

static Color poly6(const float c[7][3], float t) {
    Color o;
    float* out[3] = {&o.r, &o.g, &o.b};
    for (int k = 0; k < 3; ++k) {
        float v = c[6][k];
        for (int j = 5; j >= 0; --j) v = v * t + c[j][k];
        *out[k] = clamp01(v);
    }
    return o;
}
static Color keys(const float (*k)[4], int n, float t) {  // k[i] = {t, r, g, b}
    for (int i = 1; i < n; ++i)
        if (t <= k[i][0] || i == n - 1) {
            float u = clamp01((t - k[i - 1][0]) / (k[i][0] - k[i - 1][0]));
            return lerp({k[i - 1][1], k[i - 1][2], k[i - 1][3]}, {k[i][1], k[i][2], k[i][3]}, u);
        }
    return {k[0][1], k[0][2], k[0][3]};
}
static Color cmap_eval(Cmap m, float t) {
    // viridis / inferno: 6th-order polynomial fits (Matt Zucker, CC0); turbo: Google's polynomial.
    static const float vir[7][3] = {{0.2777273272234177f, 0.005407344544966578f, 0.3340998053353061f},
                                    {0.1050930431085774f, 1.404613529898575f, 1.384590162594685f},
                                    {-0.3308618287255563f, 0.214847559468213f, 0.09509516302823659f},
                                    {-4.634230498983486f, -5.799100973351585f, -19.33244095627987f},
                                    {6.228269936347081f, 14.17993336680509f, 56.69055260068105f},
                                    {4.776384997670288f, -13.74514537774601f, -65.35303263337234f},
                                    {-5.435455855934631f, 4.645852612178535f, 26.3124352495832f}};
    static const float inf[7][3] = {{0.0002189403691192265f, 0.001651004631001012f, -0.01948089843709184f},
                                    {0.1065134194856116f, 0.5639564367884091f, 3.932712388889277f},
                                    {11.60249308247187f, -3.972853965665698f, -15.9423941062914f},
                                    {-41.70399613139459f, 17.43639888205313f, 44.35414519872813f},
                                    {77.162935699427f, -33.40235894210092f, -81.80730925738993f},
                                    {-71.31942824499214f, 32.62606426397723f, 73.20951985803202f},
                                    {25.13112622477341f, -12.24266895238567f, -23.07032500287172f}};
    static const float tur[7][3] = {{0.13572138f, 0.09140261f, 0.10667330f},
                                    {4.61539260f, 2.19418839f, 12.64194608f},
                                    {-42.66032258f, 4.84296658f, -60.58204836f},
                                    {132.13108234f, -14.18503333f, 110.36276771f},
                                    {-152.94239396f, 4.27729857f, -89.90310912f},
                                    {59.28637943f, 2.82956604f, 27.34824973f},
                                    {0, 0, 0}};
    static const float div[5][4] = {{0.00f, 0.230f, 0.299f, 0.754f}, {0.25f, 0.552f, 0.690f, 0.996f},
                                    {0.50f, 0.865f, 0.865f, 0.865f}, {0.75f, 0.958f, 0.604f, 0.482f},
                                    {1.00f, 0.706f, 0.016f, 0.150f}};
    static const float dvd[5][4] = {{0.00f, 0.60f, 0.86f, 1.00f}, {0.25f, 0.16f, 0.46f, 0.86f},
                                    {0.50f, 0.07f, 0.08f, 0.10f}, {0.75f, 0.86f, 0.30f, 0.16f},
                                    {1.00f, 1.00f, 0.80f, 0.50f}};
    switch (m) {
        case Cmap::Viridis: return poly6(vir, t);
        case Cmap::Inferno: return poly6(inf, t);
        case Cmap::Turbo: return poly6(tur, t);
        case Cmap::Diverging: return keys(div, 5, t);
        case Cmap::DivergingDark: return keys(dvd, 5, t);
    }
    return {t, t, t};
}
static constexpr int kLut = 1024;
static const Color* cmap_lut(Cmap m) {
    static Color lut[5][kLut];
    static std::once_flag once;
    std::call_once(once, [] {
        for (int k = 0; k < 5; ++k)
            for (int i = 0; i < kLut; ++i) lut[k][i] = cmap_eval(Cmap(k), i / float(kLut - 1));
    });
    return lut[int(m)];
}
Color colormap(Cmap m, float t) {
    if (!(t == t)) t = 0;  // NaN
    return cmap_lut(m)[int(clamp01(t) * (kLut - 1) + 0.5f)];
}

// =============================================================== fonts
namespace {
struct Face {
    std::vector<unsigned char> data;
    stbtt_fontinfo info{};
};
struct Glyph {
    int w = 0, h = 0, xoff = 0, yoff = 0;
    std::vector<uint8_t> bmp;
};
struct FontSys {
    std::vector<Face> faces;
    std::vector<int> chain[3];
    std::unordered_map<uint32_t, uint64_t> cpmap[3];  // codepoint -> (face<<32 | glyph)
    std::unordered_map<uint64_t, Glyph> cache;
    std::mutex mu;
    FontSys() {
        const char* dir = "C:/Windows/Fonts/";
        std::unordered_map<std::string, int> loaded;
        auto load = [&](const char* name) -> int {
            auto it = loaded.find(name);
            if (it != loaded.end()) return it->second;
            int idx = -1;
            std::string path = std::string(dir) + name;
            if (FILE* f = fopen(path.c_str(), "rb")) {
                Face fc;
                fseek(f, 0, SEEK_END);
                long n = ftell(f);
                fseek(f, 0, SEEK_SET);
                fc.data.resize(n > 0 ? n : 0);
                size_t got = n > 0 ? fread(fc.data.data(), 1, n, f) : 0;
                fclose(f);
                int off = stbtt_GetFontOffsetForIndex(fc.data.data(), 0);
                if (got == (size_t)n && off >= 0 && stbtt_InitFont(&fc.info, fc.data.data(), off)) {
                    faces.push_back(std::move(fc));
                    idx = (int)faces.size() - 1;
                    // stbtt_fontinfo keeps a pointer to data; vector move keeps the heap buffer.
                    faces.back().info.data = faces.back().data.data();
                }
            }
            loaded[name] = idx;
            return idx;
        };
        const char* mono[] = {"consola.ttf", "cour.ttf", "seguisym.ttf", "segoeui.ttf", "cambria.ttc", "arial.ttf"};
        const char* sans[] = {"segoeui.ttf", "arial.ttf", "seguisym.ttf", "cambria.ttc"};
        const char* bold[] = {"segoeuib.ttf", "seguisb.ttf", "arialbd.ttf", "segoeui.ttf", "seguisym.ttf", "cambria.ttc"};
        for (auto n : mono) if (int i = load(n); i >= 0) chain[0].push_back(i);
        for (auto n : sans) if (int i = load(n); i >= 0) chain[1].push_back(i);
        for (auto n : bold) if (int i = load(n); i >= 0) chain[2].push_back(i);
        for (int k = 0; k < 3; ++k)
            if (chain[k].empty())
                fprintf(stderr, "[demo] warning: no font found for style %d; text will be skipped\n", k);
    }
    // returns face index (or -1) and glyph
    uint64_t lookup(int style, uint32_t cp) {
        auto& m = cpmap[style];
        auto it = m.find(cp);
        if (it != m.end()) return it->second;
        uint64_t r = ~0ull;
        for (int fi : chain[style]) {
            int g = stbtt_FindGlyphIndex(&faces[fi].info, (int)cp);
            if (g) { r = (uint64_t(fi) << 32) | uint32_t(g); break; }
        }
        if (r == ~0ull && !chain[style].empty()) {  // missing everywhere: use '?' of primary
            int fi = chain[style][0];
            r = (uint64_t(fi) << 32) | uint32_t(stbtt_FindGlyphIndex(&faces[fi].info, '?'));
        }
        m[cp] = r;
        return r;
    }
    const Glyph& glyph(int fi, int g, float scale, int sub) {
        uint64_t key = (uint64_t(fi) << 56) | (uint64_t(g & 0xFFFFFF) << 32) |
                       (uint64_t(uint32_t(scale * 65536.f)) & 0xFFFFFFF0ull) | uint64_t(sub);
        auto it = cache.find(key);
        if (it != cache.end()) return it->second;
        Glyph gl;
        int x0, y0, x1, y1;
        float sx = sub * 0.25f;
        stbtt_GetGlyphBitmapBoxSubpixel(&faces[fi].info, g, scale, scale, sx, 0, &x0, &y0, &x1, &y1);
        gl.w = x1 - x0;
        gl.h = y1 - y0;
        gl.xoff = x0;
        gl.yoff = y0;
        if (gl.w > 0 && gl.h > 0) {
            gl.bmp.resize(size_t(gl.w) * gl.h);
            stbtt_MakeGlyphBitmapSubpixel(&faces[fi].info, gl.bmp.data(), gl.w, gl.h, gl.w, scale, scale, sx, 0, g);
        }
        return cache.emplace(key, std::move(gl)).first->second;
    }
};
FontSys& fonts() {
    static FontSys fs;
    return fs;
}
uint32_t next_cp(std::string_view s, size_t& i) {
    unsigned char c = (unsigned char)s[i++];
    if (c < 0x80) return c;
    int n = (c >= 0xF0) ? 3 : (c >= 0xE0) ? 2 : (c >= 0xC0) ? 1 : 0;
    uint32_t cp = c & (0x3F >> n);
    for (int k = 0; k < n && i < s.size(); ++k) cp = (cp << 6) | ((unsigned char)s[i++] & 0x3F);
    return cp;
}
// Lays out text; calls emit(face, glyph, scale, pen_x) for each glyph. Returns advance width.
template <class F>
float layout(std::string_view s, float size, Font f, F&& emit) {
    FontSys& fsys = fonts();
    int style = int(f);
    float pen = 0;
    int prev_face = -1, prev_g = 0;
    for (size_t i = 0; i < s.size();) {
        uint32_t cp = next_cp(s, i);
        uint64_t r = fsys.lookup(style, cp);
        if (r == ~0ull) continue;
        int fi = int(r >> 32), g = int(r & 0xFFFFFFFF);
        const stbtt_fontinfo* info = &fsys.faces[fi].info;
        float sc = stbtt_ScaleForMappingEmToPixels(info, size);
        if (prev_face == fi) pen += sc * stbtt_GetGlyphKernAdvance(info, prev_g, g);
        emit(fi, g, sc, pen);
        int adv, lsb;
        stbtt_GetGlyphHMetrics(info, g, &adv, &lsb);
        pen += adv * sc;
        prev_face = fi;
        prev_g = g;
    }
    return pen;
}
}  // namespace

float Canvas::text_width(std::string_view s, float size, Font f) {
    std::lock_guard<std::mutex> lk(fonts().mu);
    return layout(s, size, f, [](int, int, float, float) {});
}

float Canvas::text(float x, float y, std::string_view s, float size, Color c, Font f, Align a) {
    TextStyle st;
    st.size = size;
    st.color = c;
    st.font = f;
    st.align = a;
    return text(x, y, s, st);
}

float Canvas::text(float x, float y, std::string_view s, const TextStyle& st) {
    FontSys& fsys = fonts();
    std::lock_guard<std::mutex> lk(fsys.mu);
    float w = layout(s, st.size, st.font, [](int, int, float, float) {});
    if (st.align == Align::Center) x -= w * 0.5f;
    else if (st.align == Align::Right) x -= w;
    if (st.backdrop) {
        Blend b = blend;
        blend = Blend::Normal;
        fill_rounded_rect(x - st.pad, y - st.pad * 0.6f, w + 2 * st.pad, st.size * 1.2f + st.pad * 1.2f,
                          std::min(6.f, st.pad), st.backdrop_color, st.backdrop_alpha);
        blend = b;
    }
    float baseline = std::round(y + st.size * 0.93f);
    float cr = st.color.r * 255, cg = st.color.g * 255, cb = st.color.b * 255;
    layout(s, st.size, st.font, [&](int fi, int g, float sc, float pen) {
        float px = x + pen;
        float fl = std::floor(px);
        int sub = clampi(int((px - fl) * 4), 0, 3);
        const Glyph& gl = fsys.glyph(fi, g, sc, sub);
        int gx = int(fl) + gl.xoff, gy = int(baseline) + gl.yoff;
        for (int j = 0; j < gl.h; ++j) {
            int yy = gy + j;
            if (yy < 0 || yy >= h_) continue;
            const uint8_t* row = gl.bmp.data() + size_t(j) * gl.w;
            for (int i = 0; i < gl.w; ++i)
                if (row[i]) plot(gx + i, yy, cr, cg, cb, row[i] * (1 / 255.f) * st.alpha);
        }
    });
    return w;
}

// =============================================================== Canvas primitives
void Canvas::plot(int x, int y, float r, float g, float b, float a) {
    if ((unsigned)x >= (unsigned)w_ || (unsigned)y >= (unsigned)h_ || !(a > 0)) return;
    uint8_t* p = px_ + size_t(y) * stride_ + size_t(x) * 3;
    if (blend == Blend::Add) {
        int v0 = p[0] + int(r * a + 0.5f), v1 = p[1] + int(g * a + 0.5f), v2 = p[2] + int(b * a + 0.5f);
        p[0] = uint8_t(v0 > 255 ? 255 : v0);
        p[1] = uint8_t(v1 > 255 ? 255 : v1);
        p[2] = uint8_t(v2 > 255 ? 255 : v2);
    } else if (a >= 1) {
        p[0] = uint8_t(r + 0.5f);
        p[1] = uint8_t(g + 0.5f);
        p[2] = uint8_t(b + 0.5f);
    } else {
        p[0] = uint8_t(p[0] + (r - p[0]) * a + 0.5f);
        p[1] = uint8_t(p[1] + (g - p[1]) * a + 0.5f);
        p[2] = uint8_t(p[2] + (b - p[2]) * a + 0.5f);
    }
}

static inline uint8_t to8(float v) { return uint8_t(clamp01(v) * 255.f + 0.5f); }

void Canvas::clear(Color c) {
    if (!px_) return;
    uint8_t r = to8(c.r), g = to8(c.g), b = to8(c.b);
    for (int x = 0; x < w_; ++x) { px_[x * 3] = r; px_[x * 3 + 1] = g; px_[x * 3 + 2] = b; }
    for (int y = 1; y < h_; ++y) memcpy(px_ + size_t(y) * stride_, px_, size_t(w_) * 3);
}

void Canvas::fade(Color c, float amount) {
    float a = clamp01(amount);
    int ia = int(a * 256 + 0.5f), cr = to8(c.r), cg = to8(c.g), cb = to8(c.b);
    for (int y = 0; y < h_; ++y) {
        uint8_t* p = px_ + size_t(y) * stride_;
        for (int x = 0; x < w_ * 3; x += 3) {
            p[x] = uint8_t(p[x] + (((cr - p[x]) * ia) >> 8));
            p[x + 1] = uint8_t(p[x + 1] + (((cg - p[x + 1]) * ia) >> 8));
            p[x + 2] = uint8_t(p[x + 2] + (((cb - p[x + 2]) * ia) >> 8));
        }
    }
}

void Canvas::put(int x, int y, Color c) {
    if ((unsigned)x >= (unsigned)w_ || (unsigned)y >= (unsigned)h_) return;
    uint8_t* p = px_ + size_t(y) * stride_ + size_t(x) * 3;
    p[0] = to8(c.r);
    p[1] = to8(c.g);
    p[2] = to8(c.b);
}

void Canvas::blend_pixel(int x, int y, Color c, float alpha) { plot(x, y, c.r * 255, c.g * 255, c.b * 255, alpha); }

void Canvas::splat(float x, float y, Color c, float alpha) {
    float fx = x - 0.5f, fy = y - 0.5f;
    int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
    float tx = fx - ix, ty = fy - iy;
    float r = c.r * 255, g = c.g * 255, b = c.b * 255;
    plot(ix, iy, r, g, b, alpha * (1 - tx) * (1 - ty));
    plot(ix + 1, iy, r, g, b, alpha * tx * (1 - ty));
    plot(ix, iy + 1, r, g, b, alpha * (1 - tx) * ty);
    plot(ix + 1, iy + 1, r, g, b, alpha * tx * ty);
}

void Canvas::fill_rect(float x, float y, float w, float h, Color c, float alpha) {
    if (w <= 0 || h <= 0) return;
    float x1 = x + w, y1 = y + h;
    int i0 = std::max(0, (int)std::floor(x)), i1 = std::min(w_ - 1, (int)std::ceil(x1) - 1);
    int j0 = std::max(0, (int)std::floor(y)), j1 = std::min(h_ - 1, (int)std::ceil(y1) - 1);
    float r = c.r * 255, g = c.g * 255, b = c.b * 255;
    for (int j = j0; j <= j1; ++j) {
        float cy = std::min(float(j + 1), y1) - std::max(float(j), y);
        for (int i = i0; i <= i1; ++i) {
            float cx = std::min(float(i + 1), x1) - std::max(float(i), x);
            plot(i, j, r, g, b, alpha * cx * cy);
        }
    }
}

void Canvas::stroke_rect(float x, float y, float w, float h, float t, Color c, float alpha) {
    fill_rect(x, y, w, t, c, alpha);
    fill_rect(x, y + h - t, w, t, c, alpha);
    fill_rect(x, y + t, t, h - 2 * t, c, alpha);
    fill_rect(x + w - t, y + t, t, h - 2 * t, c, alpha);
}

void Canvas::fill_rounded_rect(float x, float y, float w, float h, float rad, Color c, float alpha) {
    if (w <= 0 || h <= 0) return;
    rad = std::min(rad, std::min(w, h) * 0.5f);
    float cx = x + w * 0.5f, cy = y + h * 0.5f, hx = w * 0.5f - rad, hy = h * 0.5f - rad;
    int i0 = std::max(0, (int)std::floor(x)), i1 = std::min(w_ - 1, (int)std::ceil(x + w));
    int j0 = std::max(0, (int)std::floor(y)), j1 = std::min(h_ - 1, (int)std::ceil(y + h));
    float r = c.r * 255, g = c.g * 255, b = c.b * 255;
    for (int j = j0; j <= j1; ++j) {
        float qy = std::fabs(j + 0.5f - cy) - hy;
        for (int i = i0; i <= i1; ++i) {
            float qx = std::fabs(i + 0.5f - cx) - hx;
            float ox = std::max(qx, 0.f), oy = std::max(qy, 0.f);
            float d = std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.f) - rad;
            float cov = clamp01(0.5f - d);
            if (cov > 0) plot(i, j, r, g, b, alpha * cov);
        }
    }
}

void Canvas::line(float x0, float y0, float x1, float y1, float th, Color c, float alpha) {
    float R = th * 0.5f;
    if (R < 0.5f) { alpha *= std::max(th, 0.f); R = 0.5f; }
    float dx = x1 - x0, dy = y1 - y0, L2 = dx * dx + dy * dy;
    float inv = L2 > 1e-12f ? 1.f / L2 : 0.f;
    float ext = R + 1.f;
    int j0 = std::max(0, (int)std::floor(std::min(y0, y1) - ext));
    int j1 = std::min(h_ - 1, (int)std::ceil(std::max(y0, y1) + ext));
    float r = c.r * 255, g = c.g * 255, b = c.b * 255;
    for (int j = j0; j <= j1; ++j) {
        float py = j + 0.5f, xa, xb;
        if (std::fabs(dy) < 1e-6f) {
            xa = std::min(x0, x1) - ext;
            xb = std::max(x0, x1) + ext;
        } else {
            float t0 = (py - ext - y0) / dy, t1 = (py + ext - y0) / dy;
            if (t0 > t1) std::swap(t0, t1);
            t0 = std::max(t0, 0.f);
            t1 = std::min(t1, 1.f);
            if (t0 > t1) { if (t1 <= 0.f) t0 = t1 = 0.f; else t0 = t1 = 1.f; }
            float xa0 = x0 + t0 * dx, xb0 = x0 + t1 * dx;
            xa = std::min(xa0, xb0) - ext;
            xb = std::max(xa0, xb0) + ext;
        }
        int i0 = std::max(0, (int)std::floor(xa)), i1 = std::min(w_ - 1, (int)std::ceil(xb));
        for (int i = i0; i <= i1; ++i) {
            float px = i + 0.5f;
            float t = clamp01(((px - x0) * dx + (py - y0) * dy) * inv);
            float ex = px - (x0 + t * dx), ey = py - (y0 + t * dy);
            float cov = clamp01(R + 0.5f - std::sqrt(ex * ex + ey * ey));
            if (cov > 0) plot(i, j, r, g, b, alpha * cov);
        }
    }
}

void Canvas::polyline(const Vec2* p, int n, float th, Color c, float alpha, bool closed) {
    for (int i = 0; i + 1 < n; ++i) line(p[i].x, p[i].y, p[i + 1].x, p[i + 1].y, th, c, alpha);
    if (closed && n > 2) line(p[n - 1].x, p[n - 1].y, p[0].x, p[0].y, th, c, alpha);
}

void Canvas::circle(float cx, float cy, float rad, Color c, float alpha) {
    if (rad <= 0) return;
    if (rad < 0.56f) { splat(cx, cy, c, alpha * std::min(1.f, 3.14159265f * rad * rad)); return; }
    int i0 = std::max(0, (int)std::floor(cx - rad - 0.5f)), i1 = std::min(w_ - 1, (int)std::ceil(cx + rad + 0.5f));
    int j0 = std::max(0, (int)std::floor(cy - rad - 0.5f)), j1 = std::min(h_ - 1, (int)std::ceil(cy + rad + 0.5f));
    float r = c.r * 255, g = c.g * 255, b = c.b * 255;
    float rin = rad - 0.5f, rin2 = rin > 0 ? rin * rin : -1.f, rout2 = (rad + 0.5f) * (rad + 0.5f);
    for (int j = j0; j <= j1; ++j) {
        float dy = j + 0.5f - cy;
        for (int i = i0; i <= i1; ++i) {
            float dx = i + 0.5f - cx, d2 = dx * dx + dy * dy;
            if (d2 >= rout2) continue;
            float cov = d2 <= rin2 ? 1.f : clamp01(rad + 0.5f - std::sqrt(d2));
            plot(i, j, r, g, b, alpha * cov);
        }
    }
}

void Canvas::ring(float cx, float cy, float rad, float th, Color c, float alpha) {
    float hw = std::max(th, 1.f) * 0.5f;
    if (th < 1) alpha *= std::max(th, 0.f);
    float ro = rad + hw + 0.5f;
    int i0 = std::max(0, (int)std::floor(cx - ro)), i1 = std::min(w_ - 1, (int)std::ceil(cx + ro));
    int j0 = std::max(0, (int)std::floor(cy - ro)), j1 = std::min(h_ - 1, (int)std::ceil(cy + ro));
    float r = c.r * 255, g = c.g * 255, b = c.b * 255;
    for (int j = j0; j <= j1; ++j) {
        float dy = j + 0.5f - cy;
        for (int i = i0; i <= i1; ++i) {
            float dx = i + 0.5f - cx;
            float cov = clamp01(hw + 0.5f - std::fabs(std::sqrt(dx * dx + dy * dy) - rad));
            if (cov > 0) plot(i, j, r, g, b, alpha * cov);
        }
    }
}

void Canvas::glow(float cx, float cy, float rad, Color c, float intensity) {
    if (rad <= 0) return;
    Blend saved = blend;
    blend = Blend::Add;
    int i0 = std::max(0, (int)std::floor(cx - rad)), i1 = std::min(w_ - 1, (int)std::ceil(cx + rad));
    int j0 = std::max(0, (int)std::floor(cy - rad)), j1 = std::min(h_ - 1, (int)std::ceil(cy + rad));
    float r = c.r * 255, g = c.g * 255, b = c.b * 255, inv = 1.f / (rad * rad);
    for (int j = j0; j <= j1; ++j) {
        float dy = j + 0.5f - cy;
        for (int i = i0; i <= i1; ++i) {
            float dx = i + 0.5f - cx, q = 1.f - (dx * dx + dy * dy) * inv;
            if (q > 0) plot(i, j, r, g, b, intensity * q * q);
        }
    }
    blend = saved;
}

void Canvas::triangle(Vec2 a, Vec2 b, Vec2 c, Color col, float alpha, bool aa) {
    Vec2 p[3] = {a, b, c};
    polygon(p, 3, col, alpha, aa);
}

void Canvas::polygon(const Vec2* p, int n, Color c, float alpha, bool aa) {
    if (n < 3 || n > 64) return;
    float area = 0, mnx = p[0].x, mxx = p[0].x, mny = p[0].y, mxy = p[0].y;
    for (int i = 0; i < n; ++i) {
        const Vec2 &u = p[i], &v = p[(i + 1) % n];
        area += u.x * v.y - v.x * u.y;
        mnx = std::min(mnx, u.x); mxx = std::max(mxx, u.x);
        mny = std::min(mny, u.y); mxy = std::max(mxy, u.y);
    }
    if (std::fabs(area) < 1e-9f) return;
    float sgn = area > 0 ? 1.f : -1.f;
    float nx[64], ny[64], off[64];
    for (int i = 0; i < n; ++i) {
        const Vec2 &u = p[i], &v = p[(i + 1) % n];
        float ex = v.x - u.x, ey = v.y - u.y, L = std::sqrt(ex * ex + ey * ey);
        if (L < 1e-12f) { nx[i] = ny[i] = 0; off[i] = -1e30f; continue; }
        // outward normal for orientation sgn
        nx[i] = sgn * ey / L;
        ny[i] = -sgn * ex / L;
        off[i] = nx[i] * u.x + ny[i] * u.y;
    }
    float pad = aa ? 1.f : 0.f;
    int i0 = std::max(0, (int)std::floor(mnx - pad)), i1 = std::min(w_ - 1, (int)std::ceil(mxx + pad));
    int j0 = std::max(0, (int)std::floor(mny - pad)), j1 = std::min(h_ - 1, (int)std::ceil(mxy + pad));
    float r = c.r * 255, g = c.g * 255, b = c.b * 255;
    for (int j = j0; j <= j1; ++j) {
        float py = j + 0.5f;
        for (int i = i0; i <= i1; ++i) {
            float px = i + 0.5f, sd = -1e30f;
            bool tie_out = false;
            for (int k = 0; k < n; ++k) {
                float d = nx[k] * px + ny[k] * py - off[k];
                if (d > sd) sd = d;
                if (!aa && d == 0 && (nx[k] > 0 || (nx[k] == 0 && ny[k] > 0))) tie_out = true;
            }
            float cov = aa ? clamp01(0.5f - sd) : ((sd < 0 || (sd == 0 && !tie_out)) ? 1.f : 0.f);
            if (cov > 0) plot(i, j, r, g, b, alpha * cov);
        }
    }
}

void Canvas::field(const float* v, int nx, int ny, float x, float y, float w, float h, Cmap m, float vmin,
                   float vmax, bool smooth) {
    if (nx <= 0 || ny <= 0 || w <= 0 || h <= 0) return;
    const Color* lut = cmap_lut(m);
    float span = vmax - vmin, inv = span != 0 ? 1.f / span : 0.f;
    int i0 = std::max(0, (int)std::floor(x)), i1 = std::min(w_ - 1, (int)std::ceil(x + w) - 1);
    int j0 = std::max(0, (int)std::floor(y)), j1 = std::min(h_ - 1, (int)std::ceil(y + h) - 1);
    for (int j = j0; j <= j1; ++j) {
        float fv = (j + 0.5f - y) / h * ny;
        for (int i = i0; i <= i1; ++i) {
            float fu = (i + 0.5f - x) / w * nx, val;
            if (smooth) {
                float u = fu - 0.5f, vv = fv - 0.5f;
                int a = (int)std::floor(u), bq = (int)std::floor(vv);
                float tu = u - a, tv = vv - bq;
                int a0 = clampi(a, 0, nx - 1), a1 = clampi(a + 1, 0, nx - 1);
                int b0 = clampi(bq, 0, ny - 1), b1 = clampi(bq + 1, 0, ny - 1);
                float top = v[size_t(b0) * nx + a0] * (1 - tu) + v[size_t(b0) * nx + a1] * tu;
                float bot = v[size_t(b1) * nx + a0] * (1 - tu) + v[size_t(b1) * nx + a1] * tu;
                val = top * (1 - tv) + bot * tv;
            } else {
                val = v[size_t(clampi((int)fv, 0, ny - 1)) * nx + clampi((int)fu, 0, nx - 1)];
            }
            float t = (val - vmin) * inv;
            if (!(t == t)) t = 0;
            const Color& cc = lut[int(clamp01(t) * (kLut - 1) + 0.5f)];
            plot(i, j, cc.r * 255, cc.g * 255, cc.b * 255, 1.f);
        }
    }
}

void Canvas::image(const uint8_t* rgbp, int iw, int ih, float x, float y, float w, float h) {
    if (iw <= 0 || ih <= 0 || w <= 0 || h <= 0) return;
    int i0 = std::max(0, (int)std::floor(x)), i1 = std::min(w_ - 1, (int)std::ceil(x + w) - 1);
    int j0 = std::max(0, (int)std::floor(y)), j1 = std::min(h_ - 1, (int)std::ceil(y + h) - 1);
    for (int j = j0; j <= j1; ++j) {
        int sy = clampi(int((j + 0.5f - y) / h * ih), 0, ih - 1);
        for (int i = i0; i <= i1; ++i) {
            int sx = clampi(int((i + 0.5f - x) / w * iw), 0, iw - 1);
            const uint8_t* s = rgbp + (size_t(sy) * iw + sx) * 3;
            plot(i, j, s[0], s[1], s[2], 1.f);
        }
    }
}

void Canvas::colorbar(float x, float y, float w, float h, Cmap m, float vmin, float vmax, std::string_view label) {
    bool vert = h > w;
    const Color* lut = cmap_lut(m);
    int i0 = std::max(0, (int)x), i1 = std::min(w_ - 1, (int)(x + w) - 1);
    int j0 = std::max(0, (int)y), j1 = std::min(h_ - 1, (int)(y + h) - 1);
    for (int j = j0; j <= j1; ++j)
        for (int i = i0; i <= i1; ++i) {
            float t = vert ? 1.f - (j + 0.5f - y) / h : (i + 0.5f - x) / w;
            const Color& cc = lut[int(clamp01(t) * (kLut - 1) + 0.5f)];
            plot(i, j, cc.r * 255, cc.g * 255, cc.b * 255, 1.f);
        }
    stroke_rect(x - 1, y - 1, w + 2, h + 2, 1, pal::grid, 1);
    std::string lo = fmt("%.3g", vmin), hi = fmt("%.3g", vmax);
    if (vert) {
        text(x + w + 6, y - 2, hi, 13, pal::dim, Font::Mono);
        text(x + w + 6, y + h - 14, lo, 13, pal::dim, Font::Mono);
        if (!label.empty()) text(x + w * 0.5f, y - 24, label, 13, pal::dim, Font::Sans, Align::Center);
    } else {
        text(x, y + h + 4, lo, 13, pal::dim, Font::Mono);
        text(x + w, y + h + 4, hi, 13, pal::dim, Font::Mono, Align::Right);
        if (!label.empty()) text(x + w * 0.5f, y + h + 4, label, 13, pal::dim, Font::Sans, Align::Center);
    }
}

// =============================================================== View
View::View(const Canvas& c, double x0, double y0, double x1, double y1, float margin, bool flip_y)
    : wx0(x0), wy0(y0), wx1(x1), wy1(y1), flip(flip_y) {
    double aw = c.width() - 2.0 * margin, ah = c.height() - 2.0 * margin;
    double sw = x1 - x0, sh = y1 - y0;
    s = float(std::min(aw / (sw != 0 ? sw : 1), ah / (sh != 0 ? sh : 1)));
    ox = float((c.width() - s * sw) * 0.5);
    oy = float((c.height() - s * sh) * 0.5);
}
Vec2 View::operator()(double x, double y) const {
    return {float(ox + s * (x - wx0)), float(flip ? oy + s * (wy1 - y) : oy + s * (y - wy0))};
}

// =============================================================== Panel
Panel::Scope::~Scope() {
    if (p) {
        p->accum_ += now_ms() - t0;
        p->measured_ = true;
    }
}

Panel::Panel(int w, int h, std::string name, int side) : name_(std::move(name)), side_(side) {
    buf_.resize(size_t(w) * h * 3);
    px_ = buf_.data();
    w_ = w;
    h_ = h;
    stride_ = w * 3;
    clear(pal::bg);
}

static Panel::Metric& find_metric(std::vector<Panel::Metric>& v, std::string_view name) {
    for (auto& m : v)
        if (m.name == name) return m;
    v.push_back({});
    v.back().name = std::string(name);
    return v.back();
}

void Panel::metric(std::string_view name, double value, const char* f, Tone tone) {
    Metric& m = find_metric(metrics_, name);
    m.numeric = true;
    m.tone = tone;
    m.display = (!f || std::string_view(f) == "si") ? si_fmt(value) : fmt(f, value);
    m.last = value;
    if (std::isfinite(value)) {
        if (m.n == 0) m.mn = m.mx = value;
        m.mn = std::min(m.mn, value);
        m.mx = std::max(m.mx, value);
        m.sum += value;
        ++m.n;
    }
    m.hist.push_back(float(value));
}
void Panel::metric_text(std::string_view name, std::string_view text, Tone tone) {
    Metric& m = find_metric(metrics_, name);
    m.display = std::string(text);
    m.tone = tone;
}
void Panel::sparkline(std::string_view n, bool lg) { spark_ = std::string(n); spark_log_ = lg; }
void Panel::set_budget_ms(double ms) { budget_ = ms; }
void Panel::set_compute_label(std::string_view s) { label_ = std::string(s); }
void Panel::set_steps_per_frame(int k) { steps_ = std::max(1, k); }
void Panel::result(std::string_view k, double v) { results_.push_back({std::string(k), false, v, {}}); }
void Panel::result(std::string_view k, std::string_view v) { results_.push_back({std::string(k), true, 0, std::string(v)}); }

static std::vector<double> post_warmup(const Panel& p) {
    std::vector<double> v;
    for (size_t i = 0; i < p.samples_.size(); ++i)
        if (p.sample_frame_[i] >= p.warmup_) v.push_back(p.samples_[i]);
    return v;
}
static double quantile(std::vector<double> v, double q) {
    if (v.empty()) return NAN;
    std::sort(v.begin(), v.end());
    double pos = q * (v.size() - 1);
    size_t i = (size_t)pos;
    double t = pos - i;
    return i + 1 < v.size() ? v[i] * (1 - t) + v[i + 1] * t : v[i];
}
double Panel::median_ms() const {
    auto v = post_warmup(*this);
    if (v.empty()) return last_ms_;
    return quantile(std::move(v), 0.5);
}

// =============================================================== CLI helpers
static int find_arg(int argc, char** argv, std::string_view flag) {
    for (int i = 1; i < argc; ++i)
        if (flag == argv[i]) return i;
    return -1;
}
int arg_int(int argc, char** argv, std::string_view flag, int def) {
    int i = find_arg(argc, argv, flag);
    return (i > 0 && i + 1 < argc) ? atoi(argv[i + 1]) : def;
}
double arg_double(int argc, char** argv, std::string_view flag, double def) {
    int i = find_arg(argc, argv, flag);
    return (i > 0 && i + 1 < argc) ? atof(argv[i + 1]) : def;
}
bool arg_flag(int argc, char** argv, std::string_view flag) { return find_arg(argc, argv, flag) > 0; }
std::string arg_str(int argc, char** argv, std::string_view flag, std::string_view def) {
    int i = find_arg(argc, argv, flag);
    return (i > 0 && i + 1 < argc) ? std::string(argv[i + 1]) : std::string(def);
}

// =============================================================== Harness
static std::string slashes(std::string s) {
    for (auto& c : s)
        if (c == '\\') c = '/';
    return s;
}
static std::string exe_dir() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return ".";
    return fs::path(buf).parent_path().string();
}

Harness::Harness(int argc, char** argv, Config cfg) : cfg_(std::move(cfg)) {
    if (find_arg(argc, argv, "--frames") > 0) { cfg_.frames = arg_int(argc, argv, "--frames", cfg_.frames); frames_cli_ = true; }
    cfg_.fps = arg_double(argc, argv, "--fps", cfg_.fps);
    preview_ = arg_flag(argc, argv, "--preview");
    novideo_ = preview_ || arg_flag(argc, argv, "--novideo");
    quiet_ = arg_flag(argc, argv, "--quiet");
    nvenc_ = arg_flag(argc, argv, "--nvenc");
    crf_ = arg_int(argc, argv, "--crf", 23);
    enc_threads_ = arg_int(argc, argv, "--enc-threads", 6);
    if (find_arg(argc, argv, "--poster") > 0) { cfg_.poster_frame = arg_int(argc, argv, "--poster", -1); poster_cli_ = true; }
    if (preview_ && !frames_cli_) cfg_.frames = std::min(cfg_.frames, 30);
    if (cfg_.frames < 1) cfg_.frames = 1;
    if (!(cfg_.fps > 0)) cfg_.fps = 60;

    out_dir_ = arg_str(argc, argv, "--out", (fs::path(exe_dir()) / "out").string());
    std::error_code ec;
    fs::create_directories(out_dir_, ec);
    out_dir_ = slashes(fs::absolute(out_dir_, ec).string());
    exe_name_ = cfg_.name;

    int ph = cfg_.height - cfg_.title_h - cfg_.footer_h;
    if (cfg_.single) {
        panels_.push_back(std::make_unique<Panel>(cfg_.width, ph, cfg_.title.empty() ? cfg_.title_right : cfg_.title, 1));
    } else {
        panels_.push_back(std::make_unique<Panel>(cfg_.width / 2, ph, cfg_.title_left, 0));
        panels_.push_back(std::make_unique<Panel>(cfg_.width - cfg_.width / 2, ph, cfg_.title_right, 1));
    }
    for (auto& p : panels_) {
        p->warmup_ = cfg_.warmup_frames;
        if (cfg_.budget_ms > 0) p->budget_ = cfg_.budget_ms;
    }
    fb_.assign(size_t(cfg_.width) * cfg_.height * 3, 0);
    poster_path_ = out_dir_ + "/poster.png";
    if (!novideo_) open_video();
    t_start_ = t_last_report_ = now_ms();
    if (!quiet_)
        fprintf(stderr, "[%s] %d frames @ %.0f fps, %s -> %s\n", cfg_.name.c_str(), cfg_.frames, cfg_.fps,
                preview_ ? "preview (no video)" : (novideo_ ? "no video" : (nvenc_ ? "h264_nvenc" : "libx264")),
                out_dir_.c_str());
}

Harness::~Harness() { finish(); }

void Harness::open_video() {
    video_path_ = out_dir_ + "/" + cfg_.name + ".mp4";
    std::string enc = nvenc_ ? fmt("-c:v h264_nvenc -preset p5 -rc vbr -cq %d -b:v 0", crf_)
                             : fmt("-c:v libx264 -preset medium -crf %d -threads %d", crf_, enc_threads_);
    std::string cmd = fmt(
        "ffmpeg -hide_banner -loglevel error -y -f rawvideo -pix_fmt rgb24 -s %dx%d -r %g -i - %s "
        "-pix_fmt yuv420p -movflags +faststart \"%s\"",
        cfg_.width, cfg_.height, cfg_.fps, enc.c_str(), video_path_.c_str());
    fflush(stdout);
    fflush(stderr);
    pipe_ = _popen(cmd.c_str(), "wb");
    if (!pipe_) {
        fprintf(stderr, "[%s] ERROR: could not start ffmpeg (is it on PATH?)\n", cfg_.name.c_str());
        video_status_ = -1;
        video_path_.clear();
    }
}

void Harness::close_video() {
    if (!pipe_) return;
    FILE* f = (FILE*)pipe_;
    fflush(f);
    int st = _pclose(f);
    pipe_ = nullptr;
    if (st != 0) {
        fprintf(stderr, "[%s] ERROR: ffmpeg exited with status %d\n", cfg_.name.c_str(), st);
        video_status_ = st;
    }
}

bool Harness::next_frame() {
    if (finished_) return false;
    if (frame_ >= 0 && frame_ < cfg_.frames) present();
    ++frame_;
    if (frame_ >= cfg_.frames) {
        frame_ = cfg_.frames - 1;
        close_video();
        if (!quiet_ && _isatty(_fileno(stderr))) fprintf(stderr, "\n");
        frame_ = cfg_.frames;  // mark as done
        return false;
    }
    for (auto& p : panels_) { p->accum_ = 0; p->measured_ = false; }
    return true;
}

void Harness::save_still(std::string_view name) { pending_stills_.push_back(std::string(name)); }
void Harness::result(std::string_view k, double v) { results_.push_back({std::string(k), false, v, {}}); }
void Harness::result(std::string_view k, std::string_view v) { results_.push_back({std::string(k), true, 0, std::string(v)}); }

static Color tone_color(Tone t, Color accent) {
    switch (t) {
        case Tone::Good: return pal::good;
        case Tone::Bad: return pal::bad;
        case Tone::Warn: return pal::warn;
        case Tone::Accent: return accent;
        default: return pal::text;
    }
}

// series used by a panel's sparkline (last N values)
static constexpr int kSparkN = 240;
static std::vector<float> spark_series(const Panel& p) {
    std::vector<float> s;
    if (p.spark_.empty()) return s;
    if (p.spark_ == "compute") {
        size_t n = p.samples_.size(), a = n > kSparkN ? n - kSparkN : 0;
        for (size_t i = a; i < n; ++i) s.push_back(float(p.samples_[i]));
    } else {
        for (auto& m : p.metrics_)
            if (m.name == p.spark_) {
                size_t n = m.hist.size(), a = n > kSparkN ? n - kSparkN : 0;
                s.assign(m.hist.begin() + a, m.hist.end());
            }
    }
    if (p.spark_log_)
        for (auto& v : s) v = std::log10(std::max(std::fabs(v), 1e-30f));
    return s;
}

// Robust top of the sparkline range: one OS hiccup must not flatten the whole graph.
// Values above the returned top are drawn clipped at the top edge.
static float robust_top(std::vector<float> v) {
    if (v.empty()) return 1;
    std::sort(v.begin(), v.end());
    float mx = v.back(), p95 = v[size_t(0.95 * (v.size() - 1))];
    return std::min(mx, p95 * 1.6f + 1e-12f);
}

void Harness::draw_hud(Panel& p, Canvas& c, float lo, float hi, bool have_range) {
    if (!p.hud_on_) return;
    Color accent = p.side_ == 0 ? pal::without : pal::with;
    const float W = 440, padx = 16;
    // ---- measure height
    std::vector<const Panel::Metric*> shown;
    for (auto& m : p.metrics_)
        if (shown.size() < 6) shown.push_back(&m);
    bool show_speed = cfg_.show_speedup && !cfg_.single && p.side_ == 1 && !panels_[0]->samples_.empty() &&
                      !p.samples_.empty();
    float H = 12 + 21 + 38 + 21;                       // header, big line, p50/p95
    if (p.budget_ > 0) H += 22;
    if (show_speed) H += 26;
    if (!shown.empty()) H += 12 + 26.f * shown.size();
    bool spark = !p.spark_.empty();
    if (spark) H += 6 + 50 + 3 + 17;
    H += 10;
    float x = 16, y = 16;
    if (p.corner_ == Corner::TopRight || p.corner_ == Corner::BottomRight) x = c.width() - W - 16;
    if (p.corner_ == Corner::BottomLeft || p.corner_ == Corner::BottomRight) y = c.height() - H - 16;
    c.fill_rounded_rect(x, y, W, H, 10, rgb(0x05070b), 0.84f);
    c.fill_rect(x + 10, y, W - 20, 2, accent, 0.9f);  // accent strip
    float cx = x + padx, cy = y + 12;
    // ---- header
    std::string head = p.label_ == "compute" ? std::string("measured compute only (render + encode excluded)")
                                             : p.label_ + " · compute only (render + encode excluded)";
    c.text(cx, cy, head, 14, pal::dim, Font::Mono);
    cy += 21;
    // ---- big line: "sim 23.41 ms/frame → 43 fps"
    double ms = p.ema_ms_;
    bool has = !p.samples_.empty();
    float bx = cx;
    std::string val = has ? ms_fmt(ms) : "--";
    bx += c.text(bx, cy, val, 30, pal::text, Font::Mono);
    bx += c.text(bx + 4, cy + 11, "ms/frame", 16, pal::dim, Font::Mono) + 4;
    std::string f = has && ms > 0 ? group_int(1000.0 / ms) : "--";
    c.text(x + W - padx, cy + 4, "→ " + f + " fps", 24, accent, Font::Mono, Align::Right);
    cy += 38;
    // ---- p50/p95 window
    {
        size_t n = p.samples_.size(), a = n > 120 ? n - 120 : 0;
        std::vector<double> w(p.samples_.begin() + a, p.samples_.end());
        std::string s = w.empty() ? "EMA · waiting for samples"
                                  : fmt("EMA · last %zu: p50 %s  p95 %s ms", w.size(), ms_fmt(quantile(w, 0.5)).c_str(),
                                        ms_fmt(quantile(w, 0.95)).c_str());
        c.text(cx, cy, s, 15, pal::dim, Font::Mono);
        cy += 21;
    }
    if (p.budget_ > 0) {
        bool over = has && ms > p.budget_;
        std::string s = !has ? fmt("budget %s ms", ms_fmt(p.budget_).c_str())
                        : over ? fmt("budget %s ms: OVER ×%.2f", ms_fmt(p.budget_).c_str(), ms / p.budget_)
                               : fmt("budget %s ms: within (%.0f%% used)", ms_fmt(p.budget_).c_str(), 100 * ms / p.budget_);
        c.text(cx, cy, s, 15, over ? pal::bad : pal::good, Font::Mono);
        cy += 22;
    }
    if (show_speed) {
        double l = panels_[0]->median_ms(), r = p.median_ms();
        std::string s = (r > 0 && l > 0)
                            ? (l >= r ? fmt("%.1f× less compute than WITHOUT", l / r)
                                      : fmt("%.2f× the compute of WITHOUT (slower)", r / l))
                            : std::string("speedup: n/a");
        c.text(cx, cy, s, 17, l >= r ? pal::with : pal::warn, Font::Bold);
        c.text(x + W - padx, cy + 4, "median", 12, pal::dim, Font::Mono, Align::Right);
        cy += 26;
    }
    // ---- metrics
    if (!shown.empty()) {
        cy += 4;
        c.fill_rect(cx, cy, W - 2 * padx, 1, rgb(0xffffff), 0.08f);
        cy += 8;
        for (auto* m : shown) {
            c.text(cx, cy, m->name, 17, rgb(0xa3abbd), Font::Sans);
            c.text(x + W - padx, cy, m->display, 18, tone_color(m->tone, accent), Font::Mono, Align::Right);
            cy += 26;
        }
    }
    // ---- sparkline
    if (spark) {
        cy += 6;
        auto s = spark_series(p);
        float gx = cx, gy = cy, gw = W - 2 * padx, gh = 50;
        c.fill_rect(gx, gy, gw, gh, rgb(0xffffff), 0.035f);
        if (s.size() >= 2) {
            float mn = lo, mx = hi;
            if (!have_range) {
                mn = *std::min_element(s.begin(), s.end());
                mx = p.spark_log_ ? *std::max_element(s.begin(), s.end()) : robust_top(s);
                if (p.spark_ == "compute" && !p.spark_log_) mn = 0;
            }
            float peak = *std::max_element(s.begin(), s.end());
            if (mx - mn < 1e-12f) { mx = mn + 1; }
            std::vector<Vec2> pts;
            for (size_t i = 0; i < s.size(); ++i) {
                float px = gx + gw * float(i + kSparkN - s.size()) / (kSparkN - 1);
                float py = gy + gh - 2 - (gh - 4) * clamp01((s[i] - mn) / (mx - mn));
                pts.push_back({px, py});
            }
            for (size_t i = 0; i + 1 < pts.size(); ++i) {  // area fill
                Vec2 q[4] = {pts[i], pts[i + 1], {pts[i + 1].x, gy + gh}, {pts[i].x, gy + gh}};
                c.polygon(q, 4, accent, 0.16f, false);
            }
            c.polyline(pts.data(), (int)pts.size(), 1.6f, accent, 0.95f);
            c.circle(pts.back().x, pts.back().y, 2.6f, accent);
            if (p.budget_ > 0 && p.spark_ == "compute" && !p.spark_log_ && p.budget_ <= mx) {
                float by = gy + gh - 2 - (gh - 4) * clamp01(float((p.budget_ - mn) / (mx - mn)));
                c.line(gx, by, gx + gw, by, 1, pal::warn, 0.6f);
            }
            auto lab = [&](double v) { return p.spark_log_ ? fmt("1e%.0f", v) : (p.spark_ == "compute" ? ms_fmt(v) : si_fmt(v)); };
            cy += gh + 3;
            std::string name = p.spark_ == "compute" ? p.label_ + " ms/frame" : p.spark_;
            c.text(cx, cy, fmt("%s · last %d%s", name.c_str(), kSparkN, (have_range && cfg_.share_spark_scale && !cfg_.single) ? " · shared scale" : ""), 13, pal::dim, Font::Mono);
            std::string top = "top " + lab(mx);
            if (!p.spark_log_ && peak > mx * 1.001f) top += " (peak " + lab(peak) + ")";
            c.text(x + W - padx, cy, top, 13, pal::dim, Font::Mono, Align::Right);
        } else {
            c.text(gx + gw / 2, gy + gh / 2 - 8, "collecting…", 13, pal::dim, Font::Mono, Align::Center);
            cy += gh + 3;
        }
    }
}

void Harness::present() {
    double t0 = now_ms();
    // ---- record timings
    for (auto& p : panels_) {
        if (p->measured_) {
            double s = p->accum_;
            p->samples_.push_back(s);
            p->sample_frame_.push_back(frame_);
            p->last_ms_ = s;
            p->ema_ms_ = p->samples_.size() == 1 ? s : p->ema_ms_ + 0.1 * (s - p->ema_ms_);
            if (p->budget_ > 0 && s > p->budget_ && frame_ >= p->warmup_) ++p->over_budget_;
        }
    }
    const int W = cfg_.width, H = cfg_.height, th = cfg_.title_h, fh = cfg_.footer_h;
    Canvas full(fb_.data(), W, H, W * 3);
    full.blend = Blend::Normal;
    full.fill_rect(0, 0, (float)W, (float)th, pal::band);
    full.fill_rect(0, (float)(H - fh), (float)W, (float)fh, pal::band);
    // ---- panels
    int x0 = 0;
    std::vector<int> px0;
    for (auto& p : panels_) {
        px0.push_back(x0);
        for (int y = 0; y < p->height(); ++y)
            memcpy(fb_.data() + (size_t(th + y) * W + x0) * 3, p->buf_.data() + size_t(y) * p->stride(), size_t(p->width()) * 3);
        x0 += p->width();
    }
    // ---- HUDs (shared sparkline scale)
    float lo = 0, hi = 0;
    bool have = false;
    if (cfg_.share_spark_scale && panels_.size() > 1) {
        bool same = true;
        for (auto& p : panels_) same = same && p->spark_ == panels_[0]->spark_ && p->spark_log_ == panels_[0]->spark_log_;
        if (same && !panels_[0]->spark_.empty()) {
            std::vector<float> all;
            for (auto& p : panels_) {
                auto s = spark_series(*p);
                all.insert(all.end(), s.begin(), s.end());
            }
            have = !all.empty();
            if (have) {
                lo = *std::min_element(all.begin(), all.end());
                hi = panels_[0]->spark_log_ ? *std::max_element(all.begin(), all.end()) : robust_top(all);
            }
            if (have && panels_[0]->spark_ == "compute" && !panels_[0]->spark_log_) lo = 0;
        }
    }
    for (size_t i = 0; i < panels_.size(); ++i) {
        Panel& p = *panels_[i];
        Canvas sub(fb_.data() + (size_t(th) * W + px0[i]) * 3, p.width(), p.height(), W * 3);
        draw_hud(p, sub, lo, hi, have);
    }
    // ---- separators
    full.fill_rect(0, (float)th - 1, (float)W, 1, pal::grid);
    full.fill_rect(0, (float)(H - fh), (float)W, 1, pal::grid);
    if (panels_.size() > 1) full.fill_rect((float)px0[1] - 1, (float)th, 2, (float)(H - th - fh), pal::grid);
    // ---- titles
    for (size_t i = 0; i < panels_.size(); ++i) {
        Panel& p = *panels_[i];
        float cxm = px0[i] + p.width() * 0.5f;
        std::string pre = cfg_.single ? "" : (i == 0 ? "WITHOUT:  " : "WITH:  ");
        std::string name = cfg_.single ? (cfg_.title.empty() ? p.name() : cfg_.title) : p.name();
        float size = 30;
        float wt;
        while (true) {
            wt = Canvas::text_width(pre, size * 0.8f, Font::Bold) + Canvas::text_width(name, size, Font::Bold);
            if (wt <= p.width() - 48 || size <= 16) break;
            size -= 1;
        }
        float ty = (th - size * 1.2f) * 0.5f - 1;
        float tx = cxm - wt * 0.5f;
        Color acc = i == 0 && !cfg_.single ? pal::without : pal::with;
        if (!pre.empty()) tx += full.text(tx, ty + size * 0.13f, pre, size * 0.8f, acc, Font::Bold);
        full.text(tx, ty, name, size, pal::text, Font::Bold);
    }
    // ---- footer
    {
        std::string right = fmt("frame %d / %d   ·   t = %.2f s", frame_ + 1, cfg_.frames, (frame_ + 1) / cfg_.fps);
        float rw = Canvas::text_width(right, 16, Font::Mono);
        float avail = W - 48 - rw - 40;
        std::string cap = cfg_.caption;
        float size = 18;
        while (size > 13 && Canvas::text_width(cap, size, Font::Sans) > avail) size -= 0.5f;
        if (Canvas::text_width(cap, size, Font::Sans) > avail) {
            while (!cap.empty() && Canvas::text_width(cap + "…", size, Font::Sans) > avail) {
                cap.pop_back();
                while (!cap.empty() && (cap.back() & 0xC0) == 0x80) cap.pop_back();
            }
            cap += "…";
        }
        float fy = H - fh + (fh - 3 - size * 1.2f) * 0.5f;
        full.text(24, fy, cap, size, rgb(0xaab2c3), Font::Sans);
        full.text(W - 24.f, H - fh + (fh - 3 - 16 * 1.2f) * 0.5f, right, 16, pal::dim, Font::Mono, Align::Right);
        float prog = float(frame_ + 1) / cfg_.frames;
        full.fill_rect(0, H - 3.f, (float)W, 3, pal::grid);
        full.fill_rect(0, H - 3.f, W * prog, 3, panels_.size() > 1 ? pal::with : pal::with, 0.85f);
    }
    if (overlay_) overlay_(full);
    // ---- outputs
    if (pipe_) {
        size_t n = fwrite(fb_.data(), 1, fb_.size(), (FILE*)pipe_);
        if (n != fb_.size()) {
            fprintf(stderr, "\n[%s] ERROR: writing to ffmpeg failed (frame %d)\n", cfg_.name.c_str(), frame_);
            close_video();
            if (video_status_ == 0) video_status_ = -2;
        }
    }
    int poster = cfg_.poster_frame < 0 ? cfg_.frames - 1 : std::min(cfg_.poster_frame, cfg_.frames - 1);
    if (frame_ == poster) {
        stbi_write_png(poster_path_.c_str(), W, H, 3, fb_.data(), W * 3);
        poster_saved_ = true;
    }
    for (auto& s : pending_stills_) {
        std::string path = out_dir_ + "/" + s;
        if (path.size() < 4 || path.substr(path.size() - 4) != ".png") path += ".png";
        stbi_write_png(path.c_str(), W, H, 3, fb_.data(), W * 3);
        stills_.push_back(path);
    }
    pending_stills_.clear();
    ++presented_;
    t_present_total_ += now_ms() - t0;
    // ---- progress
    double now = now_ms();
    static const bool tty = _isatty(_fileno(stderr)) != 0;  // \r-updating line only on a console
    if (!quiet_ && (now - t_last_report_ > (tty ? 1000 : 5000) || frame_ == cfg_.frames - 1)) {
        t_last_report_ = now;
        std::string s = fmt("%s[%s] frame %d/%d", tty ? "\r" : "", cfg_.name.c_str(), frame_ + 1, cfg_.frames);
        for (auto& p : panels_) s += fmt("  %s %s ms", p->side_ == 0 ? "L" : "R", ms_fmt(p->ema_ms_).c_str());
        s += fmt("  (%.1f s)%s", (now - t_start_) / 1000, tty ? "   " : "\n");
        fputs(s.c_str(), stderr);
    }
}

// =============================================================== results.json
static std::string jstr(std::string_view s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) o += fmt("\\u%04x", c);
                else o += char(c);
        }
    }
    return o + "\"";
}
static std::string jnum(double v) {
    if (!std::isfinite(v)) return "null";
    return fmt("%.6g", v);
}
static std::string jkv(const std::vector<Panel::KV>& kv, const char* ind) {
    std::string o = "{";
    for (size_t i = 0; i < kv.size(); ++i)
        o += fmt("%s\n%s  %s: %s", i ? "," : "", ind, jstr(kv[i].key).c_str(),
                 kv[i].is_str ? jstr(kv[i].s).c_str() : jnum(kv[i].d).c_str());
    return o + (kv.empty() ? "}" : fmt("\n%s}", ind));
}

int Harness::finish() {
    if (finished_) return video_status_ == 0 ? 0 : 1;
    // finish called mid-loop: present the in-progress frame
    if (frame_ >= 0 && frame_ < cfg_.frames) {
        present();
        cfg_.frames = frame_ + 1;
    }
    close_video();
    finished_ = true;
    int presented = presented_;
    if (!poster_saved_ && presented > 0) {
        stbi_write_png(poster_path_.c_str(), cfg_.width, cfg_.height, 3, fb_.data(), cfg_.width * 3);
        poster_saved_ = true;
    }
    double wall = (now_ms() - t_start_) / 1000.0;

    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof ms;
    GlobalMemoryStatusEx(&ms);
    char tbuf[64];
    std::time_t tt = std::time(nullptr);
    std::tm tmv{};
    gmtime_s(&tmv, &tt);
    std::strftime(tbuf, sizeof tbuf, "%Y-%m-%dT%H:%M:%SZ", &tmv);

    std::string o = "{\n";
    o += fmt("  \"demo\": %s,\n", jstr(cfg_.name).c_str());
    o += fmt("  \"generated_utc\": \"%s\",\n", tbuf);
    if (cfg_.single) o += fmt("  \"title\": %s,\n", jstr(cfg_.title).c_str());
    else o += fmt("  \"title_left\": %s,\n  \"title_right\": %s,\n", jstr(cfg_.title_left).c_str(), jstr(cfg_.title_right).c_str());
    o += fmt("  \"caption\": %s,\n", jstr(cfg_.caption).c_str());
    o += fmt("  \"frames\": %d,\n  \"fps\": %g,\n  \"warmup_frames_excluded\": %d,\n", presented, cfg_.fps, cfg_.warmup_frames);
    o += fmt("  \"preview\": %s,\n", preview_ ? "true" : "false");
    o += fmt("  \"video\": %s,\n", (video_path_.empty() || video_status_ != 0) ? "null" : jstr(video_path_).c_str());
    o += fmt("  \"poster\": %s,\n", poster_saved_ ? jstr(poster_path_).c_str() : "null");
    o += "  \"stills\": [";
    for (size_t i = 0; i < stills_.size(); ++i) o += (i ? ", " : "") + jstr(stills_[i]);
    o += "],\n";
    o += "  \"timing_note\": \"compute_ms_per_frame = wall time inside the panel's measure() scopes per frame (QueryPerformanceCounter); rendering, HUD, PNG and video encoding excluded. fps = 1000 / ms.\",\n";
    o += fmt("  \"machine\": {\"cpu\": %s, \"hw_threads\": %d, \"ram_gb\": %.1f, \"os\": \"Windows\", \"compiler\": %s},\n",
             jstr(cpu_name()).c_str(), hw_threads(), ms.ullTotalPhys / 1073741824.0,
#if defined(__clang__)
             jstr(std::string("clang ") + __clang_version__).c_str()
#else
             jstr("unknown").c_str()
#endif
    );
    o += fmt("  \"wall_seconds\": %.2f,\n  \"harness_ms_per_frame\": %.2f,\n", wall,
             presented ? t_present_total_ / presented : 0.0);
    o += "  \"panels\": [";
    for (size_t pi = 0; pi < panels_.size(); ++pi) {
        Panel& p = *panels_[pi];
        auto v = post_warmup(p);
        double mean = 0, sd = 0;
        for (double x : v) mean += x;
        if (!v.empty()) mean /= v.size();
        for (double x : v) sd += (x - mean) * (x - mean);
        sd = v.size() > 1 ? std::sqrt(sd / (v.size() - 1)) : 0;
        double med = quantile(v, 0.5), p95 = quantile(v, 0.95);
        double mn = v.empty() ? NAN : *std::min_element(v.begin(), v.end());
        double mx = v.empty() ? NAN : *std::max_element(v.begin(), v.end());
        o += pi ? ",\n    {" : "\n    {";
        const char* side = cfg_.single ? "single" : (p.side_ == 0 ? "left" : "right");
        o += fmt("\n      \"side\": \"%s\",\n      \"role\": \"%s\",\n      \"method\": %s,\n", side,
                 cfg_.single ? "single" : (p.side_ == 0 ? "WITHOUT" : "WITH"), jstr(p.name()).c_str());
        o += fmt("      \"compute_label\": %s,\n      \"steps_per_frame\": %d,\n", jstr(p.label_).c_str(), p.steps_);
        o += fmt("      \"compute_ms_per_frame\": {\"n\": %zu, \"mean\": %s, \"median\": %s, \"p95\": %s, \"min\": %s, \"max\": %s, \"stdev\": %s},\n",
                 v.size(), jnum(mean).c_str(), jnum(med).c_str(), jnum(p95).c_str(), jnum(mn).c_str(), jnum(mx).c_str(), jnum(sd).c_str());
        o += fmt("      \"ms_per_step_median\": %s,\n", jnum(med / p.steps_).c_str());
        o += fmt("      \"fps_from_median\": %s,\n      \"fps_from_mean\": %s,\n", jnum(1000 / med).c_str(), jnum(1000 / mean).c_str());
        if (p.budget_ > 0)
            o += fmt("      \"budget_ms\": %s,\n      \"frames_over_budget\": %lld,\n", jnum(p.budget_).c_str(), p.over_budget_);
        o += "      \"metrics\": {";
        for (size_t mi = 0; mi < p.metrics_.size(); ++mi) {
            auto& m = p.metrics_[mi];
            o += fmt("%s\n        %s: ", mi ? "," : "", jstr(m.name).c_str());
            if (m.numeric)
                o += fmt("{\"last\": %s, \"mean\": %s, \"min\": %s, \"max\": %s, \"n\": %lld, \"display\": %s}",
                         jnum(m.last).c_str(), jnum(m.n ? m.sum / m.n : NAN).c_str(), jnum(m.n ? m.mn : NAN).c_str(),
                         jnum(m.n ? m.mx : NAN).c_str(), m.n, jstr(m.display).c_str());
            else
                o += fmt("{\"display\": %s}", jstr(m.display).c_str());
        }
        o += p.metrics_.empty() ? "},\n" : "\n      },\n";
        o += "      \"extra\": " + jkv(p.results_, "      ") + "\n    }";
    }
    o += "\n  ],\n";
    if (panels_.size() > 1) {
        double l = panels_[0]->median_ms(), r = panels_[1]->median_ms();
        o += fmt("  \"speedup_median_compute\": %s,\n", jnum(l / r).c_str());
    }
    o += "  \"extra\": " + jkv(results_, "  ") + "\n}\n";
    std::string rp = out_dir_ + "/results.json";
    if (FILE* f = fopen(rp.c_str(), "wb")) {
        fwrite(o.data(), 1, o.size(), f);
        fclose(f);
    }
    if (!quiet_) {
        fprintf(stderr, "[%s] done: %d frames in %.1f s\n", cfg_.name.c_str(), presented, wall);
        for (auto& p : panels_) {
            auto v = post_warmup(*p);
            double med = quantile(v, 0.5);
            fprintf(stderr, "  %-6s %-40.40s median %s ms/frame  p95 %s  (%s fps compute-only)\n",
                    cfg_.single ? "panel" : (p->side_ == 0 ? "LEFT" : "RIGHT"), p->name().c_str(), ms_fmt(med).c_str(),
                    ms_fmt(quantile(v, 0.95)).c_str(), group_int(1000 / med).c_str());
        }
        if (panels_.size() > 1)
            fprintf(stderr, "  speedup (median compute): %.2fx\n", panels_[0]->median_ms() / panels_[1]->median_ms());
        if (!video_path_.empty() && video_status_ == 0) fprintf(stderr, "  video:   %s\n", video_path_.c_str());
        fprintf(stderr, "  poster:  %s\n  results: %s\n", poster_path_.c_str(), rp.c_str());
    }
    return video_status_ == 0 ? 0 : 1;
}

}  // namespace demo
