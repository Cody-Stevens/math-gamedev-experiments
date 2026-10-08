// Harness self-test / primitive gallery. Not a real demo: it exercises every drawing call,
// single-panel mode (--single), stills, overlays, log-scale metric sparklines and HUD corners.
//   ./build.sh harness/selftest preview            (two panels)
//   ./build.sh harness/selftest preview --single   (one full-width panel)
#include "demo.h"

#include <cmath>
#include <vector>

using namespace demo;

static void gallery(Panel& p, double t) {
    p.clear(pal::bg);
    // lines of varying thickness and angle
    for (int i = 0; i < 12; ++i) {
        float a = float(i) / 12 * 3.14159f + float(t) * 0.3f;
        p.line(150, 520, 150 + 120 * std::cos(a), 520 + 120 * std::sin(a), 0.5f + i * 0.5f, pal::cat(i));
    }
    // circles: many small ones (perf) + big AA ones + rings
    for (int i = 0; i < 2000; ++i) {
        float x = 300 + float(std::fmod(i * 37.1, 600.0)), y = 420 + float(std::fmod(i * 13.7, 180.0));
        p.circle(x, y, 1.2f + (i % 5) * 0.5f, colormap(Cmap::Turbo, (i % 100) / 99.f), 0.8f);
    }
    p.circle(400, 720, 60, pal::blue, 0.8f);
    p.circle(450, 740, 50, pal::bad, 0.5f);
    p.ring(600, 720, 55, 4, pal::warn);
    p.ring(600, 720, 35, 0.6f, pal::text);
    // glow (additive)
    for (int i = 0; i < 5; ++i) p.glow(760 + i * 30.f, 720, 40, pal::cat(i), 0.6f);
    // polygons / triangles
    p.triangle({60, 820}, {160, 820}, {110, 900}, pal::good);
    Vec2 hex[6];
    for (int i = 0; i < 6; ++i) hex[i] = {260 + 50 * std::cos(i * 1.0472f), 860 + 50 * std::sin(i * 1.0472f)};
    p.polygon(hex, 6, pal::violet, 0.85f);
    // seam-free mesh (aa=false)
    for (int j = 0; j < 4; ++j)
        for (int i = 0; i < 6; ++i) {
            float x = 360 + i * 22.f, y = 820 + j * 22.f;
            Color c = colormap(Cmap::Viridis, (i + j) / 9.f);
            p.triangle({x, y}, {x + 22, y}, {x, y + 22}, c, 1, false);
            p.triangle({x + 22, y}, {x + 22, y + 22}, {x, y + 22}, c, 1, false);
        }
    // rects + text
    p.fill_rounded_rect(560, 810, 300, 90, 12, pal::grid);
    p.stroke_rect(560, 810, 300, 90, 1.5f, pal::dim, 0.6f);
    p.text(575, 820, "Text: → ≈ ⇒ × · ² Δ ∑ ∫ ε ≤ √", 20, pal::text, Font::Sans);
    p.text(575, 855, "mono 0123456789 1.2e-3", 18, pal::good, Font::Mono);
    TextStyle st;
    st.size = 22; st.backdrop = true; st.font = Font::Bold; st.align = Align::Center;
    p.text(p.width() * 0.5f, 380, "backdrop text, centered", st);
}

static void colormaps(Panel& p, double t) {
    p.clear(pal::bg);
    const Cmap maps[5] = {Cmap::Viridis, Cmap::Inferno, Cmap::Turbo, Cmap::Diverging, Cmap::DivergingDark};
    const char* names[5] = {"viridis", "inferno", "turbo", "diverging", "diverging-dark"};
    for (int k = 0; k < 5; ++k) {
        float y = 420 + k * 46.f;
        p.colorbar(150, y, 520, 22, maps[k], 0, 1);
        p.text(140, y + 1, names[k], 16, pal::dim, Font::Sans, Align::Right);
    }
    // scalar field
    const int nx = 64, ny = 40;
    std::vector<float> f(nx * ny);
    for (int j = 0; j < ny; ++j)
        for (int i = 0; i < nx; ++i)
            f[j * nx + i] = std::sin(i * 0.2f + float(t)) * std::cos(j * 0.25f);
    p.field(f.data(), nx, ny, 60, 680, 400, 250, Cmap::DivergingDark, -1, 1, true);
    p.field(f.data(), nx, ny, 480, 680, 400, 250, Cmap::Inferno, -1, 1, false);
    p.colorbar(895, 690, 14, 230, Cmap::Inferno, -1, 1, "value");
}

int main(int argc, char** argv) {
    Config cfg;
    cfg.name = "selftest";
    cfg.single = arg_flag(argc, argv, "--single");
    cfg.title_left = "primitive gallery";
    cfg.title_right = "colormaps + fields";
    cfg.title = "single-panel mode";
    cfg.caption = "harness self-test · every primitive, both HUD styles · ignore the numbers";
    cfg.frames = 40;
    Harness h(argc, argv, cfg);
    h.on_overlay([](Canvas& c) { c.text(c.width() - 20.f, 80, "overlay()", 14, pal::warn, Font::Mono, Align::Right); });
    h.left().set_budget_ms(1.0);
    // NOTE: in single-panel mode left(), right() and panel(0) are the same panel.
    if (!cfg.single) h.right().set_hud_corner(Corner::TopRight);
    h.left().sparkline("residual", true);
    h.right().sparkline("residual", true);
    if (arg_flag(argc, argv, "--bench-draw")) {  // drawing throughput check
        Panel& p = h.left();
        double t0 = now_ms();
        for (int i = 0; i < 50000; ++i) p.circle(float(i % 900) + 30, float((i * 7) % 900) + 20, 2.0f, pal::blue, 0.8f);
        double t1 = now_ms();
        for (int i = 0; i < 50000; ++i) p.glow(float(i % 900) + 30, float((i * 7) % 900) + 20, 6.0f, pal::blue, 0.2f);
        double t2 = now_ms();
        for (int i = 0; i < 5000; ++i) p.line(float(i % 900), 10, float((i * 13) % 900), 900, 1.5f, pal::good, 0.5f);
        double t3 = now_ms();
        fprintf(stderr, "50k circles r=2: %.2f ms   50k glows r=6: %.2f ms   5k long lines: %.2f ms\n", t1 - t0, t2 - t1, t3 - t2);
    }
    double x = 0;
    while (h.next_frame()) {
        h.left().measure([&] { for (int i = 0; i < 200000; ++i) x += std::sin(i * 1e-3); });
        if (!cfg.single) h.right().measure([&] { for (int i = 0; i < 20000; ++i) x += std::sin(i * 1e-3); });
        gallery(h.left(), h.time());
        if (!cfg.single) colormaps(h.right(), h.time());
        double res = std::pow(10.0, -1 - 6.0 * h.frame() / h.frames());
        h.left().metric("residual", res, "%.2e", Tone::Warn);
        h.left().metric_text("status", "text-only metric", Tone::Accent);
        if (!cfg.single) h.right().metric("residual", res * 0.01, "%.2e", Tone::Good);
        if (h.frame() == 10) h.save_still("still_frame10");
    }
    h.result("checksum", x);
    h.result("note", "selftest");
    return h.finish();
}
