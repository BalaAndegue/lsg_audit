#pragma once
// stats.hpp — Descriptive statistics with IQR-based outlier filtering.
// All methods are pure functions; StatsEngine has no mutable state.

#include <vector>
#include <cstddef>

struct Stats {
    double mean   = 0.0;
    double stddev = 0.0;
    double median = 0.0;
    double p95    = 0.0;
    double p99    = 0.0;
    double iqr    = 0.0;
    size_t count  = 0;

    bool valid() const noexcept { return count >= 4; }
};

class StatsEngine {
public:
    // Compute descriptive statistics after IQR outlier removal (Tukey ×1.5).
    static Stats compute(std::vector<double> samples);

    // Welch z-score: how many σ is obs_mean above the baseline mean?
    static double welch_z(const Stats& baseline,
                          double obs_mean, size_t obs_n) noexcept;

    // Cohen's d using pooled standard deviation.
    static double cohen_d(const Stats& baseline, const Stats& load) noexcept;

    StatsEngine() = delete;
};
