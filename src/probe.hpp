#pragma once
// probe.hpp — Single HTTP probe abstraction.
// ProbeEngine owns the libcurl configuration; each call to probe() creates
// and destroys a curl handle (RAII), making the engine safe to use from
// multiple threads without shared state.

#include <string>

struct Measurement {
    bool   success        = false;
    long   http_code      = 0;
    double connect_ms     = 0.0;   // TCP handshake duration
    double ttfb_ms        = 0.0;   // Time-To-First-Byte (STARTTRANSFER)
    double app_latency_ms = 0.0;   // ttfb - connect  ≈  T_calcul
};

class ProbeEngine {
public:
    // timeout_ms: maximum total operation time.
    // 20 000 ms by default to handle slow SSR dev servers over WiFi.
    explicit ProbeEngine(long timeout_ms = 20000) noexcept;

    // Send one HTTP request. post_data == nullptr → GET, otherwise POST.
    // The transfer is aborted after the first response byte so that the
    // body (potentially large for SSR pages) is never fully downloaded.
    // TTFB and connect_time are captured before the abort, so all timing
    // metrics remain valid.
    Measurement probe(const std::string& url,
                      const char* post_data = nullptr) const;

private:
    long timeout_ms_;

    // Aborts the transfer after the first chunk by returning 0.
    // curl reports CURLE_WRITE_ERROR, which probe() treats as success
    // because all timing counters are already recorded at that point.
    static size_t abort_after_ttfb(void*, size_t sz, size_t nmemb,
                                   void*) noexcept;
};
