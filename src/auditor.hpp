#pragma once
// auditor.hpp — Orchestrates the full measurement pipeline for one target.
//
// Auditor owns a ProbeEngine and a StatsEngine.
// It exposes two high-level operations:
//   - measure_port  : calibration + load probing + dual-probe verification
//   - stress_port   : concurrent probing at increasing concurrency levels
//
// CurlContext must be constructed before any Auditor and destroyed after.

#include <string>
#include <vector>
#include "probe.hpp"
#include "stats.hpp"

// ---- RAII wrapper for curl_global_init / curl_global_cleanup ----
struct CurlContext {
    CurlContext();
    ~CurlContext();
    CurlContext(const CurlContext&) = delete;
    CurlContext& operator=(const CurlContext&) = delete;
};

// ---- Result types ----

struct DualProbeResult {
    double delta_complex_ms = 0.0;
    double delta_static_ms  = 0.0;
    double ratio            = 0.0;
    bool   confirmed        = false; // ratio > 2 AND delta_complex > 3σ
};

struct PortReport {
    int             port     = 0;
    bool            is_open  = false;
    Stats           baseline;
    Stats           load;
    double          z_score  = 0.0;
    double          cohen_d  = 0.0;
    DualProbeResult dual;
};

struct StressPoint {
    int    concurrency   = 0;
    double success_rate  = 0.0; // percentage
    double mean_ms       = 0.0;
    double p99_ms        = 0.0;
    int    errors        = 0;
};

struct StressReport {
    int                      port;
    std::vector<StressPoint> points;
};

// ---- Auditor ----

class Auditor {
public:
    explicit Auditor(std::string target_ip, long timeout_ms = 6000);

    PortReport   measure_port(int port, int calib_n, int probe_n) const;
    StressReport stress_port(int port,
                             const std::vector<int>& concurrencies) const;

private:
    std::string target_ip_;
    ProbeEngine engine_;

    std::string  url_for(int port, const char* path = "/") const;
    Stats        calibrate(int port, int n) const;
    Stats        load_probe(int port, int n) const;
    DualProbeResult dual_probe(int port,
                                double t_base_mean_ms,
                                double sigma_ms) const;
    StressPoint  stress_once(int port, int concurrency) const;
};
