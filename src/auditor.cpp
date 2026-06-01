#include "auditor.hpp"
#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <sstream>
#include <curl/curl.h>

// ---- CurlContext ----

CurlContext::CurlContext()  { curl_global_init(CURL_GLOBAL_ALL); }
CurlContext::~CurlContext() { curl_global_cleanup(); }

// ---- Auditor ----

Auditor::Auditor(std::string target_ip, long timeout_ms)
    : target_ip_(std::move(target_ip)), engine_(timeout_ms) {}

std::string Auditor::url_for(int port, const char* path) const {
    return "http://" + target_ip_ + ":" + std::to_string(port) + path;
}

// --- Calibration: N sequential lightweight GET requests ---
Stats Auditor::calibrate(int port, int n) const {
    std::vector<double> samples;
    samples.reserve(n);

    std::cout << "  [CALIB] port " << port << " — " << n << " sondes... " << std::flush;
    for (int i = 0; i < n; ++i) {
        const Measurement m = engine_.probe(url_for(port, "/"), /*post=*/nullptr);
        if (m.success) samples.push_back(m.app_latency_ms);
    }
    std::cout << samples.size() << " valides.\n";
    return StatsEngine::compute(samples);
}

// --- Load probing: M requests alternating GET / POST with payload ---
Stats Auditor::load_probe(int port, int n) const {
    const std::string payload(512, 'A');
    std::vector<double> samples;
    samples.reserve(n);

    std::cout << "  [MESURE] port " << port << " — " << n << " sondes... " << std::flush;
    for (int i = 0; i < n; ++i) {
        const bool use_post = (i % 2 == 0);
        const Measurement m = engine_.probe(
            url_for(port, "/"),
            use_post ? payload.c_str() : nullptr);
        if (m.success) samples.push_back(m.app_latency_ms);
    }
    std::cout << samples.size() << " valides.\n";
    return StatsEngine::compute(samples);
}

// --- Dual-probe: complex vs. static witness ---
DualProbeResult Auditor::dual_probe(int port,
                                     double t_base_mean_ms,
                                     double sigma_ms) const
{
    const std::string heavy_payload(4096, 'X');

    const Measurement complex = engine_.probe(
        url_for(port, "/api/compute"), heavy_payload.c_str());
    const Measurement witness = engine_.probe(
        url_for(port, "/"), /*post=*/nullptr);

    DualProbeResult r;
    r.delta_complex_ms = complex.success
        ? complex.app_latency_ms - t_base_mean_ms : 0.0;
    r.delta_static_ms  = (witness.success && witness.app_latency_ms > 1e-3)
        ? witness.app_latency_ms - t_base_mean_ms : 0.01; // guard /0

    r.ratio     = r.delta_complex_ms / r.delta_static_ms;
    r.confirmed = (r.ratio > 2.0) && (r.delta_complex_ms > 3.0 * sigma_ms);
    return r;
}

// --- Stress: C concurrent threads, each sends one request ---
StressPoint Auditor::stress_once(int port, int concurrency) const {
    std::mutex          mu;
    std::vector<double> latencies;
    std::atomic<int>    errors{0};

    std::cout << "  [STRESS c=" << concurrency << "] port " << port
              << "... " << std::flush;

    std::vector<std::thread> threads;
    threads.reserve(concurrency);
    for (int i = 0; i < concurrency; ++i) {
        threads.emplace_back([&]() {
            const Measurement m = engine_.probe(url_for(port, "/"), nullptr);
            if (m.success) {
                std::lock_guard<std::mutex> lock(mu);
                latencies.push_back(m.app_latency_ms);
            } else {
                ++errors;
            }
        });
    }
    for (auto& t : threads) t.join();

    StressPoint sp;
    sp.concurrency  = concurrency;
    sp.errors       = errors.load();
    sp.success_rate = 100.0 * static_cast<double>(latencies.size()) / concurrency;

    if (!latencies.empty()) {
        const Stats st = StatsEngine::compute(latencies);
        sp.mean_ms = st.mean;
        sp.p99_ms  = st.p99;
    }
    std::cout << sp.success_rate << "% succès, " << sp.mean_ms << " ms moy.\n";
    return sp;
}

// ---- Public interface ----

PortReport Auditor::measure_port(int port, int calib_n, int probe_n) const {
    PortReport rep;
    rep.port = port;

    const Measurement ping = engine_.probe(url_for(port, "/"), nullptr);
    rep.is_open = ping.success;
    if (!rep.is_open) {
        std::cout << "  [PORT " << port << "] Fermé ou injoignable.\n";
        return rep;
    }

    rep.baseline = calibrate(port, calib_n);
    if (!rep.baseline.valid()) {
        std::cerr << "  [WARN] Échantillon de calibration insuffisant (port "
                  << port << ").\n";
        return rep;
    }

    rep.load    = load_probe(port, probe_n);
    rep.z_score = StatsEngine::welch_z(rep.baseline, rep.load.mean, rep.load.count);
    rep.cohen_d = StatsEngine::cohen_d(rep.baseline, rep.load);
    rep.dual    = dual_probe(port, rep.baseline.mean, rep.baseline.stddev);
    return rep;
}

StressReport Auditor::stress_port(int port,
                                   const std::vector<int>& concurrencies) const
{
    StressReport sr;
    sr.port = port;
    for (int c : concurrencies)
        sr.points.push_back(stress_once(port, c));
    return sr;
}
