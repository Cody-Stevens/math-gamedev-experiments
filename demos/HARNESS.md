# Demo harness: "WITHOUT vs WITH" split-screen videos

`harness/demo.h` + `harness/demo.cpp` record a 1920x1080 video of one problem computed two
ways: the left panel uses the baseline method (WITHOUT) and the right panel uses the method
built on the new math result (WITH). Each panel has a live HUD with its measured compute
time. A run produces an H.264 mp4, `poster.png` and `results.json`.

The header comment in `harness/demo.h` is the authoritative API reference. This file
summarizes it.

## Build and run

From `demos/` in Git Bash:

```bash
./build.sh 01-mydemo                      # -> 01-mydemo/demo.exe  (-O2 -std=c++20 -march=native)
./build.sh 01-mydemo preview              # build, then 30 frames, no video, writes out/poster.png
./build.sh 01-mydemo run                  # build, then full run -> 01-mydemo/out/
./build.sh 01-mydemo run --frames 600 --nvenc   # extra args go to the demo
```

The harness object is cached in `harness/build/demo.o` and rebuilt only when `demo.h` or
`demo.cpp` changes. A demo is a single `main.cpp`; you can add headers next to it.
`harness/selftest/main.cpp` exercises every primitive and is a good reference
(`./build.sh harness/selftest preview`).

## CLI flags (every demo gets them)

| flag | effect |
|---|---|
| `--frames N` | number of frames (default `cfg.frames`) |
| `--fps F` | output video fps (default 60) |
| `--out DIR` | output directory (default `<exe dir>/out`) |
| `--preview` | quick iteration: no video, 30 frames unless `--frames` is given, writes `poster.png` + `results.json` |
| `--novideo` | full-length run without video |
| `--poster K` | frame index used for `poster.png` (default: last frame) |
| `--quiet` | no progress output |
| `--crf N` | x264 quality (default 23) |
| `--nvenc` | encode with `h264_nvenc` instead of `libx264` |
| `--enc-threads N` | x264 threads (default 6, which leaves the CPU free for the measured compute) |

Use `demo::arg_int / arg_double / arg_flag / arg_str (argc, argv, "--flag", default)` for your
own flags (e.g. `--n 8000`); the harness ignores flags it does not know.

Outputs in `--out`: `<cfg.name>.mp4` (libx264 `-preset medium -crf 23 -pix_fmt yuv420p
-movflags +faststart`), `poster.png`, `results.json`, and any `save_still()` PNGs. Frames are
streamed to ffmpeg through a pipe, so memory stays at about one framebuffer plus the panels.

## Skeleton `main.cpp`

```cpp
#include "demo.h"
using namespace demo;

struct Method { void step(); void draw(Panel& p) const; double error() const; /* ... */ };

int main(int argc, char** argv) {
    Config cfg;
    cfg.name        = "01-mydemo";                  // video file stem
    cfg.title_left  = "explicit Euler";             // shown as "WITHOUT: explicit Euler"
    cfg.title_right = "symplectic leapfrog";        // shown as "WITH: symplectic leapfrog"
    cfg.caption     = "Family 12 — energy drift bound · math repo preprints/...";
    cfg.frames      = 300;                          // 5 s at 60 fps
    // cfg.budget_ms = 16.7;                        // optional per-frame budget line in the HUD
    Harness h(argc, argv, cfg);                     // parses the CLI flags

    const int n = arg_int(argc, argv, "--n", 4000); // demo-specific flag
    Method a(n), b(n);                              // identical initial state

    h.left().set_compute_label("sim");              // HUD reads "sim · compute only ..."
    h.right().set_compute_label("sim");
    while (h.next_frame()) {
        h.left().measure([&] { a.step(); });        // ONLY the method's compute is timed
        h.right().measure([&] { b.step(); });

        h.left().clear(pal::bg);  a.draw(h.left()); // drawing is never timed
        h.right().clear(pal::bg); b.draw(h.right());

        h.left().metric("energy error", a.error(), "%.2e", Tone::Bad);
        h.right().metric("energy error", b.error(), "%.2e", Tone::Good);
    }
    h.result("n", n);                               // extra key/values in results.json
    return h.finish();                              // writes results.json; returns 0 if ok
}
```

## API summary (namespace `demo`)

**Harness** `Harness h(argc, argv, cfg)`
- `bool next_frame()` presents the previous frame and starts the next one. It returns false
  after `frames()` frames. Presenting records timings, draws the title, footer and HUDs on top of
  the panels, pipes the frame to ffmpeg and saves the poster and stills.
- `left()`, `right()`, `panel(i)`, `num_panels()`. In single-panel mode (`cfg.single = true`)
  `left()`, `right()` and `panel(0)` are the same 1920x950 panel, titled `cfg.title`.
- `frame()`, `frames()`, `time()` (video seconds), `fps()`, `preview()`, `quiet()`, `out_dir()`.
- `save_still("name")` saves a PNG of the current frame as presented. `set_poster_frame(k)`
  sets the poster frame (`--poster` takes precedence). `set_caption(s)` changes the footer.
- `on_overlay([](Canvas& full){...})` draws over the whole composed 1920x1080 frame.
- `result(key, number|string)` adds a top-level extra. `finish()` closes the video and writes
  `results.json`. It is also called by the destructor, and if called mid-loop it presents the
  current frame first.
- `Config` fields: `name, title_left, title_right, title, caption, single, width, height,
  title_h (70), footer_h (60), frames, fps, warmup_frames (3), poster_frame, share_spark_scale
  (true), show_speedup (true), budget_ms`.

**Panel** (derives from Canvas; 960x950 px; persistent buffer, so clear or fade it yourself)
- `measure([&]{...})` or `auto t = p.measure();` (RAII). Several calls in one frame add up.
- `metric(name, value, fmt = "%.4g", tone)`: up to 6 HUD lines in first-call order. `fmt` is a
  printf format or `"si"` (16.0M, 2.31k). `Tone::{Neutral, Good, Bad, Warn, Accent}` sets the
  value color. `metric_text(name, text, tone)` is a display-only line.
- `sparkline("compute")` (default) or `sparkline("metric name", log_scale)`. Both panels share
  the y-scale when they plot the same series (`cfg.share_spark_scale`). Isolated spikes above
  1.6x the window p95 are clipped, and the label shows the peak.
- `set_budget_ms(ms)`, `set_compute_label("solve")`, `set_steps_per_frame(k)` (results then
  also report ms/step), `set_hud(false)`, `set_hud_corner(Corner::TopRight)`,
  `result(key, value)` for per-panel extras.
- `last_ms()`, `ema_ms()`, `median_ms()`.

**Canvas** primitives take float pixel coordinates with (0,0) at the top left, are anti-aliased,
take `(Color, alpha)` and follow `canvas.blend` (`Blend::Normal` or `Blend::Add`).
- `clear(c)`, `fade(c, amount)` (motion trails), `put`, `blend_pixel`, `splat` (bilinear point)
- `fill_rect`, `stroke_rect`, `fill_rounded_rect`, `line(x0,y0,x1,y1,thickness,...)`, `polyline`
- `circle` (filled disk), `ring`, `glow(x,y,r,color,intensity)` (always additive, soft falloff)
- `triangle(a,b,c,...,aa)`, `polygon(pts,n,...,aa)` take convex shapes only. Pass `aa=false` for seam-free meshes.
- `field(v,nx,ny,x,y,w,h,cmap,vmin,vmax,smooth)` draws a row-major scalar grid with row 0 at
  the top, using bilinear or nearest sampling. `image(rgb,iw,ih,x,y,w,h)`.
  `colorbar(x,y,w,h,cmap,vmin,vmax,label)`.
- `text(x, y, utf8, size, color, Font::{Mono,Sans,Bold}, Align::{Left,Center,Right})` or
  `text(x, y, utf8, TextStyle{... backdrop = true ...})`. `size` is the em size in px and `y` is
  the top of a line box about 1.2×size tall. `Canvas::text_width(s, size, font)`. Fonts:
  Consolas, Segoe UI and Segoe UI Bold, falling back to Segoe UI Symbol or Cambria for math
  glyphs (→ ≈ ⇒ × ² Δ ∑ ∫ ≤ √).
- Performance on this machine: 50k r=2 circles take about 7 ms, 50k r=6 glows about 33 ms.
  Long 900-px lines cost about 45 µs each.

**View** `View v(panel, x0, y0, x1, y1, margin = 20, flip_y = true)` maps a world rectangle into
the panel, keeping the aspect ratio. `Vec2 p = v(x, y)`; `v.len(r)` converts a world length to px.

**Colors**: `rgb(0x0b0d12)`, `lerp`, `hsv`, `scale`. Palette `pal::bg, band, grid, text, dim,
without (coral), with (teal), good, bad, warn, blue, violet, orange, cat(i)`.
`colormap(Cmap::{Viridis, Inferno, Turbo, Diverging, DivergingDark}, t)`.

**Misc**: `now_ms()` (QueryPerformanceCounter), `fmt(printf...)`, `cpu_name()`, `hw_threads()`.

## Honesty rule for metrics

1. **The HUD's ms/frame and fps cover compute only.** They come from the time spent inside
   that panel's `measure()` scopes during the frame. Rendering, the HUD, PNG writing and video
   encoding are never included. "→ N fps" is `1000 / compute-ms` (EMA), labeled "compute only".
2. **Time all of a method's per-step work and nothing else.** That includes work the method
   needs every step, such as rebuilding a spatial hash or factorizing a matrix. It excludes
   drawing, error metrics, and comparisons against the other panel or a reference solution.
3. **Make the two sides comparable.** Use the same problem size, initial state, precision and
   thread count, and give the baseline a reasonable implementation, not a strawman. If the WITH
   side is approximate, show its error in a metric next to the speed.
4. **Show equivalence or error.** When both methods should agree, display a deviation metric,
   as `00-sample` does with "max |Δx| vs WITHOUT = 0". When they differ by design, display
   the quantity the new math improves (drift, overlaps, residual, violations).
5. **results.json is the record.** It lists mean, median, p95, min, max and stdev of compute
   ms/frame per panel, excluding the first `warmup_frames` (3), along with fps derived from
   the median and mean, the median speedup, metric aggregates (last/mean/min/max), and the
   machine (CPU brand string, thread count, RAM, compiler). Quote the median.
6. ffmpeg encodes concurrently with `-threads 6`. If your demo is multithreaded and uses
   the whole CPU, take the published numbers from a `--novideo` run.

## Look and feel

The background is `pal::bg` (#0b0d12). Use `pal::cat(i)` or colormaps for data and keep the
WITHOUT/WITH accents (coral/teal) for the comparison. Panels are persistent: for trails use
`p.fade(pal::bg, 0.3–0.5)` instead of `clear`. Leave the top-left ~460x400 px of each panel
for the HUD, or move it with `set_hud_corner`. Always look at `poster.png` with an image
viewer before recording the final video.
