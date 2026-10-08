// ladder_common.h - shared helpers for the L5 sweep-only load ladders.
// Timing statistics, log-log fits, budget crossings, and the ladder.json writer
// (schema of demos/LADDER_BRIEF.md; extra keys allowed).
#pragma once
#include "demo.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace ladder {

struct Stats {
    double median = 0, p95 = 0, mean = 0, min = 0, max = 0;
    int frames = 0;
};

inline Stats stats(std::vector<double> v) {
    Stats s;
    if (v.empty()) return s;
    std::sort(v.begin(), v.end());
    const size_t n = v.size();
    s.frames = int(n);
    s.median = n % 2 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
    s.p95 = v[std::min(n - 1, size_t(std::ceil(0.95 * n)) - 1)];
    s.min = v.front();
    s.max = v.back();
    double t = 0;
    for (double x : v) t += x;
    s.mean = t / n;
    return s;
}

// least-squares slope of log(y) against log(x)
inline double loglog_slope(const std::vector<double>& x, const std::vector<double>& y) {
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    int k = 0;
    for (size_t i = 0; i < x.size(); ++i) {
        if (!(x[i] > 0) || !(y[i] > 0)) continue;
        double a = std::log(x[i]), b = std::log(y[i]);
        sx += a; sy += b; sxx += a * a; sxy += a * b; ++k;
    }
    if (k < 2) return NAN;
    return (k * sxy - sx * sy) / (k * sxx - sx * sx);
}

inline std::string jnum(double v) {
    if (!std::isfinite(v)) return "null";
    char b[64];
    std::snprintf(b, sizeof b, "%.6g", v);
    return b;
}
inline std::string jstr(const std::string& s) {
    std::string o = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (c == '\n') o += "\\n";
        else o += c;
    }
    return o + "\"";
}

struct Series {          // one method at one stage
    Stats st;
    std::string extra;   // extra JSON members, e.g. "\"steps\": 12644" (no braces, may be empty)
};

struct Stage {
    double n = 0;
    Series without, with;
    std::vector<std::pair<std::string, Series>> more;  // extra series, e.g. {"empirical", ...}
    std::string quality;  // full JSON object for "quality" or empty
    std::string extra;    // extra JSON members at stage level (no braces) or empty
};

inline std::string series_json(const Series& s) {
    std::string o = "{\"median_ms\": " + jnum(s.st.median) + ", \"p95_ms\": " + jnum(s.st.p95) +
                    ", \"mean_ms\": " + jnum(s.st.mean) + ", \"frames\": " + std::to_string(s.st.frames);
    if (!s.extra.empty()) o += ", " + s.extra;
    return o + "}";
}

struct Crossing {
    double max_ok = NAN;       // largest measured n with median <= budget
    double first_over = NAN;   // smallest measured n with median > budget
};
inline Crossing crossing(const std::vector<Stage>& st, double budget, int which /*0 without, 1 with, 2+ more*/) {
    Crossing c;
    for (auto& s : st) {
        const Series& se = which == 0 ? s.without : which == 1 ? s.with : s.more[which - 2].second;
        if (se.st.frames == 0) continue;
        if (se.st.median <= budget) { if (std::isnan(c.max_ok) || s.n > c.max_ok) c.max_ok = s.n; }
        else if (std::isnan(c.first_over) || s.n < c.first_over) c.first_over = s.n;
    }
    return c;
}
inline double fit_of(const std::vector<Stage>& st, int which, size_t from = 0) {
    std::vector<double> x, y;
    for (size_t i = from; i < st.size(); ++i) {
        const Series& se = which == 0 ? st[i].without : which == 1 ? st[i].with : st[i].more[which - 2].second;
        if (se.st.frames == 0) continue;
        x.push_back(st[i].n);
        y.push_back(se.st.median);
    }
    return loglog_slope(x, y);
}

struct Ladder {
    std::string demo, family, load_name, notes;
    double budget_ms = 16.7;
    int threads_without = 1, threads_with = 1;
    std::vector<Stage> stages;
    std::string extra_top;  // extra top-level members (no braces), may be empty

    std::string crossing_json(int which) const {
        Crossing c = crossing(stages, budget_ms, which);
        std::string o = jnum(c.max_ok);
        return o;
    }
    std::string bracket_json(int which) const {
        Crossing c = crossing(stages, budget_ms, which);
        std::string s;
        if (std::isnan(c.first_over)) s = "within budget at every measured stage";
        else if (std::isnan(c.max_ok)) s = "over budget at every measured stage";
        else s = "crosses between n=" + jnum(c.max_ok) + " and n=" + jnum(c.first_over);
        return jstr(s);
    }

    void write(const std::string& path) const {
        std::string o = "{\n";
        o += "  \"demo\": " + jstr(demo) + ",\n";
        o += "  \"family\": " + jstr(family) + ",\n";
        o += "  \"load_name\": " + jstr(load_name) + ",\n";
        o += "  \"budget_ms\": " + jnum(budget_ms) + ",\n";
        o += "  \"threads\": {\"without\": " + std::to_string(threads_without) + ", \"with\": " +
             std::to_string(threads_with) + "},\n";
        o += "  \"stages\": [\n";
        for (size_t i = 0; i < stages.size(); ++i) {
            const Stage& s = stages[i];
            o += "    {\"n\": " + jnum(s.n) + ",\n";
            o += "     \"without\": " + series_json(s.without) + ",\n";
            o += "     \"with\": " + series_json(s.with);
            for (auto& m : s.more) o += ",\n     " + jstr(m.first) + ": " + series_json(m.second);
            o += ",\n     \"ratio_without_over_with\": " + jnum(s.without.st.median / s.with.st.median);
            if (!s.quality.empty()) o += ",\n     \"quality\": " + s.quality;
            if (!s.extra.empty()) o += ",\n     " + s.extra;
            o += "}";
            o += i + 1 < stages.size() ? ",\n" : "\n";
        }
        o += "  ],\n";
        const size_t half = stages.size() / 2;
        o += "  \"fit\": {\"without_exponent\": " + jnum(fit_of(stages, 0)) + ", \"with_exponent\": " + jnum(fit_of(stages, 1));
        for (size_t k = 0; !stages.empty() && k < stages[0].more.size(); ++k)
            o += ", " + jstr(stages[0].more[k].first + "_exponent") + ": " + jnum(fit_of(stages, int(k) + 2));
        o += ", \"upper_half_without_exponent\": " + jnum(fit_of(stages, 0, half)) +
             ", \"upper_half_with_exponent\": " + jnum(fit_of(stages, 1, half));
        for (size_t k = 0; !stages.empty() && k < stages[0].more.size(); ++k)
            o += ", " + jstr("upper_half_" + stages[0].more[k].first + "_exponent") + ": " + jnum(fit_of(stages, int(k) + 2, half));
        o += ", \"method\": \"least-squares slope of log(median_ms) vs log(n); upper_half = last half of the stages\"},\n";
        o += "  \"max_n_within_budget\": {\"without\": " + crossing_json(0) + ", \"with\": " + crossing_json(1);
        for (size_t k = 0; !stages.empty() && k < stages[0].more.size(); ++k)
            o += ", " + jstr(stages[0].more[k].first) + ": " + crossing_json(int(k) + 2);
        o += ", \"bracket_without\": " + bracket_json(0) + ", \"bracket_with\": " + bracket_json(1);
        for (size_t k = 0; !stages.empty() && k < stages[0].more.size(); ++k)
            o += ", " + jstr("bracket_" + stages[0].more[k].first) + ": " + bracket_json(int(k) + 2);
        o += "},\n";
        o += "  \"machine\": {\"cpu\": " + jstr(demo::cpu_name()) + ", \"hw_threads\": " + std::to_string(demo::hw_threads()) +
             ", \"compiler\": " + jstr(std::string("clang ") + __clang_version__) + "},\n";
        if (!extra_top.empty()) o += "  " + extra_top + ",\n";
        o += "  \"notes\": " + jstr(notes) + "\n}\n";
        FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) { std::fprintf(stderr, "cannot write %s\n", path.c_str()); return; }
        std::fwrite(o.data(), 1, o.size(), f);
        std::fclose(f);
    }

    void print_table(const char* qname = nullptr) const {
        std::printf("\n%-10s %12s %12s %9s", "n", "WITHOUT ms", "WITH ms", "ratio");
        for (size_t k = 0; !stages.empty() && k < stages[0].more.size(); ++k) std::printf(" %12s", stages[0].more[k].first.c_str());
        std::printf("  frames(wo/w)\n");
        for (auto& s : stages) {
            std::printf("%-10.0f %12.4g %12.4g %9.3g", s.n, s.without.st.median, s.with.st.median, s.without.st.median / s.with.st.median);
            for (auto& m : s.more) std::printf(" %12.4g", m.second.st.median);
            std::printf("  %d/%d\n", s.without.st.frames, s.with.st.frames);
        }
        std::printf("fit exponents: without %.3f  with %.3f\n", fit_of(stages, 0), fit_of(stages, 1));
        (void)qname;
    }
};

// Out directory = <exe dir>/out unless --out is given.
inline std::string out_dir(int argc, char** argv) {
    std::string o = demo::arg_str(argc, argv, "--out", "");
    if (!o.empty()) return o;
    std::string exe = argv[0];
    size_t p = exe.find_last_of("/\\");
    return (p == std::string::npos ? std::string(".") : exe.substr(0, p)) + "/out";
}

}  // namespace ladder
