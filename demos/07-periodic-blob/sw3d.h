// sw3d.h - tiny shared helpers for demos 07/08: vec3 math, orbit camera, thread pool.
// Header-only, no dependencies beyond the standard library.
#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace sw3d {

struct V3 {
    double x = 0, y = 0, z = 0;
    V3() = default;
    constexpr V3(double a, double b, double c) : x(a), y(b), z(c) {}
    V3 operator+(V3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    V3 operator-(V3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    V3 operator-() const { return {-x, -y, -z}; }
    V3 operator*(double s) const { return {x * s, y * s, z * s}; }
    V3 operator/(double s) const { return {x / s, y / s, z / s}; }
    V3& operator+=(V3 o) { x += o.x; y += o.y; z += o.z; return *this; }
    V3& operator-=(V3 o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    V3& operator*=(double s) { x *= s; y *= s; z *= s; return *this; }
};
inline V3 operator*(double s, V3 v) { return v * s; }
inline double dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline double len(V3 a) { return std::sqrt(dot(a, a)); }
inline V3 norm(V3 a) { double l = len(a); return l > 0 ? a / l : V3{0, 0, 1}; }

// Orbit camera: looks at `target` from spherical (az, el, dist). Pinhole with vertical fov.
struct Camera {
    V3 eye, fwd, right, up;
    double tan_half = 0.45, cx = 0, cy = 0, f = 1;  // f = focal length in pixels
    void orbit(V3 target, double az, double el, double dist, double fov_y_rad, double img_h, double ccx, double ccy) {
        eye = target + V3{std::cos(el) * std::cos(az), std::sin(el), std::cos(el) * std::sin(az)} * dist;
        fwd = norm(target - eye);
        right = norm(cross(fwd, V3{0, 1, 0}));
        up = cross(right, fwd);
        tan_half = std::tan(fov_y_rad * 0.5);
        f = img_h * 0.5 / tan_half;
        cx = ccx; cy = ccy;
    }
    // world -> pixel (x right, y down). Returns depth along fwd (<=0 means behind).
    double project(V3 p, double& px, double& py) const {
        V3 d = p - eye;
        double z = dot(d, fwd);
        px = cx + f * dot(d, right) / z;
        py = cy - f * dot(d, up) / z;
        return z;
    }
    V3 ray(double px, double py) const {
        return norm(fwd + right * ((px - cx) / f) - up * ((py - cy) / f));
    }
};

// Persistent worker pool. for_range(n, grain, f) calls f(begin, end) over [0, n) in chunks;
// the calling thread participates. Deterministic results as long as f writes disjoint data.
class Pool {
public:
    explicit Pool(int n = 0) {
        if (n <= 0) n = std::max(1, (int)std::thread::hardware_concurrency());
        nthreads_ = n;
        for (int i = 1; i < n; ++i) th_.emplace_back([this] { worker(); });
    }
    ~Pool() {
        { std::lock_guard<std::mutex> l(m_); quit_ = true; ++gen_; }
        cv_.notify_all();
        for (auto& t : th_) t.join();
    }
    int threads() const { return nthreads_; }
    template <class F> void for_range(int n, int grain, F&& f) {
        if (n <= 0) return;
        if (grain < 1) grain = 1;
        if (th_.empty() || n <= grain) { f(0, n); return; }
        std::function<void(int, int)> fn = [&](int a, int b) { f(a, b); };
        {
            std::lock_guard<std::mutex> l(m_);
            job_ = &fn; total_ = n; grain_ = grain; next_.store(0); pending_ = (int)th_.size(); ++gen_;
        }
        cv_.notify_all();
        work();
        std::unique_lock<std::mutex> l(m_);
        done_.wait(l, [&] { return pending_ == 0; });
        job_ = nullptr;
    }
private:
    void work() {
        for (;;) {
            int b = next_.fetch_add(grain_);
            if (b >= total_) break;
            (*job_)(b, std::min(total_, b + grain_));
        }
    }
    void worker() {
        int seen = 0;
        for (;;) {
            {
                std::unique_lock<std::mutex> l(m_);
                cv_.wait(l, [&] { return gen_ != seen; });
                seen = gen_;
                if (quit_) return;
            }
            work();
            { std::lock_guard<std::mutex> l(m_); if (--pending_ == 0) done_.notify_one(); }
        }
    }
    std::vector<std::thread> th_;
    std::mutex m_;
    std::condition_variable cv_, done_;
    std::function<void(int, int)>* job_ = nullptr;
    std::atomic<int> next_{0};
    int total_ = 0, grain_ = 1, pending_ = 0, gen_ = 0, nthreads_ = 1;
    bool quit_ = false;
};

}  // namespace sw3d
