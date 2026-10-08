// demo.h - split-screen "WITHOUT vs WITH" demo recorder with live benchmark HUDs.
// ============================================================================
//
// One header + demo.cpp (built automatically by demos/build.sh). C++20, no deps
// beyond vendored stb_truetype / stb_image_write. Windows (QPC timer, _popen).
//
// MINIMAL DEMO
// ------------
//   #include "demo.h"
//   int main(int argc, char** argv) {
//       demo::Config cfg;
//       cfg.name        = "01-mydemo";                 // output file stem
//       cfg.title_left  = "explicit Euler";            // drawn as "WITHOUT: explicit Euler"
//       cfg.title_right = "symplectic leapfrog";       // drawn as "WITH: symplectic leapfrog"
//       cfg.caption     = "Family 12 - energy drift bound · math repo preprints/...";
//       cfg.frames      = 300;                         // default length (CLI --frames overrides)
//       demo::Harness h(argc, argv, cfg);
//
//       SimA a; SimB b;                                // your two methods, same initial state
//       while (h.next_frame()) {                       // presents the previous frame, false at end
//           h.left().measure([&] { a.step(); });       // ONLY the compute goes inside measure()
//           h.right().measure([&] { b.step(); });
//
//           h.left().clear(demo::pal::bg);  a.draw(h.left());   // drawing is NOT timed
//           h.right().clear(demo::pal::bg); b.draw(h.right());
//
//           h.left().metric("energy drift", a.drift(), "%.2e", demo::Tone::Bad);
//           h.right().metric("energy drift", b.drift(), "%.2e", demo::Tone::Good);
//       }
//       h.result("n_bodies", 4000);                    // any extra key/values for results.json
//       return h.finish();                             // writes results.json, prints summary
//   }
//
// FRAME LOOP CONTRACT
//   * next_frame() finalizes the previous frame (records timings, composes title/footer/HUD,
//     pipes the frame to ffmpeg, saves poster/stills) and starts a new one. It returns false
//     after cfg.frames frames; the video is closed at that point.
//   * Panels are persistent pixel buffers: they are NOT cleared for you. Call clear() each
//     frame, or fade() for motion trails. The HUD/title/footer are composited on top at
//     present time and never leak into the panel buffers.
//   * In single-panel mode (cfg.single) left(), right() and panel(0) are the same panel.
//   * Panel coordinates: float pixels, (0,0) = top-left of the panel, y down.
//     Default panel size is 960 x 950 (two-panel) or 1920 x 950 (cfg.single = true).
//     Use demo::View to map world coordinates to panel pixels.
//
// HONESTY RULE (benchmark numbers)
//   * The HUD "ms/frame" and "fps" for a panel come ONLY from time spent inside that panel's
//     measure() scopes during the frame (summed if called several times). Rendering, HUD,
//     PNG and video encoding are never included. The fps label is 1000 / compute-ms.
//   * Put the whole per-frame computation of a method inside measure() - including any
//     setup the method genuinely needs each step (e.g. rebuilding a spatial hash) - and
//     nothing else (no drawing, no metric bookkeeping for the other panel).
//   * Compare like with like: same problem size, same thread count, same compiler flags.
//     Any "error vs reference" metric must be computed outside measure().
//   * The first cfg.warmup_frames (default 3) samples are excluded from results.json stats.
//   * ffmpeg encodes concurrently with a capped thread count (--enc-threads, default 6);
//     multi-threaded demos that saturate the CPU should mention that or use --novideo runs
//     for the published numbers.
//
// CLI (parsed by Harness; unknown flags are left for the demo - see demo::arg_int etc.)
//   --frames N      number of frames (default cfg.frames)
//   --fps F         output video fps (default 60)
//   --out DIR       output directory (default <exe dir>/out)
//   --preview       cheap iteration: no video, frames=30 unless --frames given, writes poster.png
//   --novideo       full run, no video (results.json + poster only)
//   --poster K      frame index for poster.png (default: last frame)
//   --quiet         no progress output
//   --crf N         x264 quality (default 23)   --nvenc   use h264_nvenc instead of libx264
//   --enc-threads N ffmpeg encoder threads (default 6)
//
// OUTPUTS (in --out DIR):  <name>.mp4, poster.png, results.json, any save_still() PNGs.
// ============================================================================
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace demo {

// ---------------------------------------------------------------- colors
struct Color { float r = 0, g = 0, b = 0; };          // linear-ish 0..1 sRGB values
constexpr Color rgb(uint32_t hex) {                    // rgb(0x0b0d12)
    return {((hex >> 16) & 255) / 255.f, ((hex >> 8) & 255) / 255.f, (hex & 255) / 255.f};
}
Color lerp(Color a, Color b, float t);
Color hsv(float h01, float s, float v);                // hue in [0,1)
Color scale(Color c, float k);

namespace pal {                                        // house palette (dark theme)
inline constexpr Color bg       = rgb(0x0b0d12);       // panel background
inline constexpr Color band     = rgb(0x0f121a);       // title / footer band
inline constexpr Color grid     = rgb(0x1c2130);       // subtle grid / borders
inline constexpr Color text     = rgb(0xe6e9ef);       // primary text
inline constexpr Color dim      = rgb(0x8a93a6);       // secondary text
inline constexpr Color without  = rgb(0xf2777a);       // WITHOUT accent (coral)
inline constexpr Color with     = rgb(0x4fd1a5);       // WITH accent (teal)
inline constexpr Color good     = rgb(0x5be49b);
inline constexpr Color bad      = rgb(0xff6b6b);
inline constexpr Color warn     = rgb(0xffc861);
inline constexpr Color blue     = rgb(0x6cb6ff);
inline constexpr Color violet   = rgb(0xb392f0);
inline constexpr Color orange   = rgb(0xffa657);
// 8 well-separated categorical colors for dark backgrounds
Color cat(int i);
}  // namespace pal

enum class Cmap { Viridis, Inferno, Turbo, Diverging /*blue-white-red*/, DivergingDark /*blue-black-red*/ };
Color colormap(Cmap m, float t);                       // t clamped to [0,1]

struct Vec2 { float x = 0, y = 0; };

enum class Blend { Normal, Add };                      // Add = saturating additive ("glow")
enum class Font { Mono, Sans, Bold };                  // Consolas / Segoe UI / Segoe UI Bold
enum class Align { Left, Center, Right };
enum class Tone { Neutral, Good, Bad, Warn, Accent };  // HUD metric value color
enum class Corner { TopLeft, TopRight, BottomLeft, BottomRight };

struct TextStyle {
    float size = 20;                                   // line height in px
    Color color = pal::text;
    Font font = Font::Sans;
    Align align = Align::Left;                         // relative to x
    bool backdrop = false;                             // dark translucent box behind text
    Color backdrop_color = rgb(0x000000);
    float backdrop_alpha = 0.6f;
    float pad = 6;
    float alpha = 1;
};

// ---------------------------------------------------------------- Canvas
// A clipped RGB8 drawing surface. All coordinates are float pixels; (0,0) is top-left.
// Every primitive is anti-aliased unless noted, takes (color, alpha), and honors `blend`.
class Canvas {
public:
    Canvas() = default;
    Canvas(uint8_t* px, int w, int h, int stride_bytes) : px_(px), w_(w), h_(h), stride_(stride_bytes) {}
    int width() const { return w_; }
    int height() const { return h_; }
    uint8_t* data() { return px_; }                    // row-major RGB8, stride() bytes per row
    int stride() const { return stride_; }

    Blend blend = Blend::Normal;                       // state used by all primitives below

    void clear(Color c);
    void fade(Color toward, float amount);             // lerp every pixel toward color (trails)
    void put(int x, int y, Color c);                   // overwrite one pixel (no blend)
    void blend_pixel(int x, int y, Color c, float alpha);
    void splat(float x, float y, Color c, float alpha = 1);  // bilinear 1-px point (cheap)

    void fill_rect(float x, float y, float w, float h, Color c, float alpha = 1);
    void stroke_rect(float x, float y, float w, float h, float thickness, Color c, float alpha = 1);
    void fill_rounded_rect(float x, float y, float w, float h, float radius, Color c, float alpha = 1);
    void line(float x0, float y0, float x1, float y1, float thickness, Color c, float alpha = 1);
    void polyline(const Vec2* p, int n, float thickness, Color c, float alpha = 1, bool closed = false);
    void circle(float cx, float cy, float r, Color c, float alpha = 1);          // filled disk
    void ring(float cx, float cy, float r, float thickness, Color c, float alpha = 1);
    void glow(float cx, float cy, float r, Color c, float intensity = 1);         // always additive, soft falloff
    void triangle(Vec2 a, Vec2 b, Vec2 c, Color col, float alpha = 1, bool aa = true);
    void polygon(const Vec2* p, int n, Color c, float alpha = 1, bool aa = true); // CONVEX only
    // aa=false gives seam-free tiling for meshes of adjacent triangles.

    // Scalar field (row-major, v[j*nx+i], row 0 drawn at the TOP) mapped through a colormap
    // into the destination rect. smooth = bilinear, else nearest.
    void field(const float* v, int nx, int ny, float x, float y, float w, float h, Cmap m,
               float vmin, float vmax, bool smooth = true);
    void image(const uint8_t* rgb, int iw, int ih, float x, float y, float w, float h);  // nearest
    void colorbar(float x, float y, float w, float h, Cmap m, float vmin, float vmax,
                  std::string_view label = {});

    // Text. size = em size in px (like CSS font-size); the line box is ~1.2*size tall and
    // y is its TOP. Returns the advance width in px. UTF-8; missing glyphs fall back to
    // Segoe UI Symbol / Cambria, so math symbols like → ≈ ⇒ × · ² Δ ∑ render.
    float text(float x, float y, std::string_view utf8, const TextStyle& st);
    float text(float x, float y, std::string_view utf8, float size, Color c,
               Font f = Font::Sans, Align a = Align::Left);
    static float text_width(std::string_view utf8, float size, Font f = Font::Sans);

    void plot(int x, int y, float r255, float g255, float b255, float a);  // low level, clipped
protected:
    uint8_t* px_ = nullptr;
    int w_ = 0, h_ = 0, stride_ = 0;
};

// Maps a world rectangle into a canvas, preserving aspect ratio, centered, with margin.
// flip_y = true puts world y-up (math convention).
struct View {
    float ox = 0, oy = 0, s = 1;                       // pixel = (o + s*(world - w0))
    double wx0 = 0, wy0 = 0, wx1 = 1, wy1 = 1;
    bool flip = true;
    View() = default;
    View(const Canvas& c, double x0, double y0, double x1, double y1, float margin = 20, bool flip_y = true);
    Vec2 operator()(double x, double y) const;         // world -> pixel
    float len(double world_len) const { return float(world_len * s); }  // world length -> px
};

// ---------------------------------------------------------------- timing
double now_ms();                                       // QueryPerformanceCounter, milliseconds

// ---------------------------------------------------------------- Panel
class Harness;
class Panel : public Canvas {
public:
    // RAII compute timer: everything until the Scope dies counts as this panel's compute.
    struct Scope {
        Panel* p; double t0;
        Scope(Panel* p_, double t) : p(p_), t0(t) {}
        Scope(const Scope&) = delete;
        Scope(Scope&& o) noexcept : p(o.p), t0(o.t0) { o.p = nullptr; }
        ~Scope();
    };
    [[nodiscard]] Scope measure() { return Scope(this, now_ms()); }
    template <class F> void measure(F&& f) { Scope s = measure(); f(); }

    // Custom HUD metric (shown in insertion order, up to 6 lines). Call every frame you want
    // it updated; last value persists. Aggregates (last/mean/min/max) go to results.json.
    // fmt is a printf format for one double, or "si" for engineering suffixes (16.0M, 2.31k).
    void metric(std::string_view name, double value, const char* fmt = "%.4g", Tone tone = Tone::Neutral);
    void metric_text(std::string_view name, std::string_view text, Tone tone = Tone::Neutral);  // display only

    void sparkline(std::string_view metric_name = "compute", bool log_scale = false);  // "" hides it
    void set_budget_ms(double ms);                     // e.g. 16.7: HUD shows within / over budget
    void set_compute_label(std::string_view s);        // default "compute" (e.g. "sim", "solve")
    void set_steps_per_frame(int k);                   // results also report ms/step = ms/frame / k
    void set_hud(bool on) { hud_on_ = on; }
    void set_hud_corner(Corner c) { corner_ = c; }
    void result(std::string_view key, double v);       // per-panel extra result
    void result(std::string_view key, std::string_view v);

    // live stats (compute ms per frame)
    double last_ms() const { return last_ms_; }
    double ema_ms() const { return ema_ms_; }
    double median_ms() const;                          // over post-warmup samples so far
    const std::string& name() const { return name_; }

    // implementation details ---------------------------------------------------------------
    struct Metric {
        std::string name, display; Tone tone = Tone::Neutral; bool numeric = false;
        double last = 0, sum = 0, mn = 0, mx = 0; long long n = 0;
        std::vector<float> hist;
    };
    struct KV { std::string key; bool is_str; double d; std::string s; };
    Panel(int w, int h, std::string name, int side);
    std::vector<uint8_t> buf_;
    std::string name_, label_ = "compute", spark_ = "compute";
    int side_ = 0, steps_ = 1, warmup_ = 3;
    bool spark_log_ = false, hud_on_ = true, measured_ = false;
    Corner corner_ = Corner::TopLeft;
    double accum_ = 0, last_ms_ = 0, ema_ms_ = 0, budget_ = 0;
    long long over_budget_ = 0;
    std::vector<double> samples_;                      // one per frame where measure() ran
    std::vector<int> sample_frame_;
    std::vector<Metric> metrics_;
    std::vector<KV> results_;
};

// ---------------------------------------------------------------- Harness
struct Config {
    std::string name = "demo";                         // file stem for video
    std::string title_left = "baseline";               // "WITHOUT: <title_left>"
    std::string title_right = "new method";            // "WITH: <title_right>"
    std::string title;                                 // single-panel mode title
    std::string caption;                               // footer line (family, theorem, source)
    bool single = false;                               // one full-width panel
    int width = 1920, height = 1080, title_h = 70, footer_h = 60;
    int frames = 300;
    double fps = 60;
    int warmup_frames = 3;                             // excluded from results.json stats
    int poster_frame = -1;                             // -1 = last frame
    bool share_spark_scale = true;                     // same y-range on both sparklines (honest)
    bool show_speedup = true;                          // right HUD: median compute ratio vs left
    double budget_ms = 0;                              // >0: applied to all panels
};

class Harness {
public:
    Harness(int argc, char** argv, Config cfg);
    ~Harness();
    Harness(const Harness&) = delete;

    Panel& left() { return *panels_[0]; }
    Panel& right() { return *panels_[panels_.size() > 1 ? 1 : 0]; }
    Panel& panel(int i = 0) { return *panels_[i]; }
    int num_panels() const { return int(panels_.size()); }

    bool next_frame();
    int frame() const { return frame_; }               // 0-based index of the current frame
    int frames() const { return cfg_.frames; }
    double time() const { return frame_ / cfg_.fps; }  // video time of current frame (s)
    double fps() const { return cfg_.fps; }
    bool preview() const { return preview_; }
    bool quiet() const { return quiet_; }
    const std::string& out_dir() const { return out_dir_; }
    const Config& config() const { return cfg_; }

    void set_caption(std::string_view s) { cfg_.caption = s; }
    void set_poster_frame(int k) { if (!poster_cli_) cfg_.poster_frame = k; }
    void save_still(std::string_view filename);        // PNG of the CURRENT frame (as presented)
    void on_overlay(std::function<void(Canvas& full_frame)> f) { overlay_ = std::move(f); }

    void result(std::string_view key, double v);       // top-level extras in results.json
    void result(std::string_view key, std::string_view v);
    void result(std::string_view key, const char* v) { result(key, std::string_view(v)); }

    int finish();                                      // idempotent; 0 = ok

private:
    void present();
    void draw_hud(Panel& p, Canvas& c, float spark_lo, float spark_hi, bool have_range);
    void open_video();
    void close_video();
    Config cfg_;
    std::vector<std::unique_ptr<Panel>> panels_;
    std::vector<uint8_t> fb_;
    std::vector<std::string> pending_stills_, stills_;
    std::vector<Panel::KV> results_;
    std::function<void(Canvas&)> overlay_;
    std::string out_dir_, video_path_, poster_path_, exe_name_;
    void* pipe_ = nullptr;
    int frame_ = -1, presented_ = 0, crf_ = 23, enc_threads_ = 6, video_status_ = 0;
    bool preview_ = false, novideo_ = false, quiet_ = false, nvenc_ = false, poster_cli_ = false;
    bool finished_ = false, frames_cli_ = false, poster_saved_ = false;
    double t_start_ = 0, t_present_total_ = 0, t_last_report_ = 0;
};

// ---------------------------------------------------------------- CLI helpers for demo flags
int arg_int(int argc, char** argv, std::string_view flag, int def);
double arg_double(int argc, char** argv, std::string_view flag, double def);
bool arg_flag(int argc, char** argv, std::string_view flag);
std::string arg_str(int argc, char** argv, std::string_view flag, std::string_view def);

// ---------------------------------------------------------------- misc
std::string fmt(const char* f, ...);                   // printf into std::string
std::string cpu_name();
int hw_threads();

}  // namespace demo
