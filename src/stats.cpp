#include "stats.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>

Stats StatsEngine::compute(std::vector<double> v) {
    Stats s;
    if (v.size() < 4) return s;
    std::sort(v.begin(), v.end());

    const double q1  = v[v.size() / 4];
    const double q3  = v[3 * v.size() / 4];
    const double iqr = q3 - q1;

    // Tukey fence: remove outliers beyond Q1-1.5·IQR and Q3+1.5·IQR
    v.erase(std::remove_if(v.begin(), v.end(), [&](double x) {
        return x < q1 - 1.5 * iqr || x > q3 + 1.5 * iqr;
    }), v.end());

    if (v.empty()) return s;

    s.count  = v.size();
    s.iqr    = iqr;
    s.mean   = std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(s.count);
    s.median = v[s.count / 2];
    s.p95    = v[std::min(static_cast<size_t>(s.count * 0.95), s.count - 1)];
    s.p99    = v[std::min(static_cast<size_t>(s.count * 0.99), s.count - 1)];

    const double mean = s.mean;
    const double var  = std::accumulate(v.begin(), v.end(), 0.0,
        [mean](double acc, double x) { return acc + (x - mean) * (x - mean); });
    s.stddev = std::sqrt(var / static_cast<double>(s.count));

    return s;
}

double StatsEngine::welch_z(const Stats& baseline,
                              double obs_mean, size_t obs_n) noexcept {
    if (!baseline.valid() || baseline.stddev < 1e-9 || obs_n == 0) return 0.0;
    const double se = baseline.stddev / std::sqrt(static_cast<double>(obs_n));
    return (obs_mean - baseline.mean) / se;
}

double StatsEngine::cohen_d(const Stats& baseline, const Stats& load) noexcept {
    if (baseline.count < 2 || load.count < 2) return 0.0;
    const double num = (baseline.count - 1) * baseline.stddev * baseline.stddev
                     + (load.count    - 1) * load.stddev    * load.stddev;
    const double denom = static_cast<double>(baseline.count + load.count - 2);
    const double sp = std::sqrt(num / denom);
    return sp > 1e-9 ? (load.mean - baseline.mean) / sp : 0.0;
}
