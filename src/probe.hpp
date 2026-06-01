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
    explicit ProbeEngine(long timeout_ms = 6000) noexcept;

    // Send one HTTP request. post_data == nullptr  →  GET, otherwise POST.
    Measurement probe(const std::string& url,
                      const char* post_data = nullptr) const;

private:
    long timeout_ms_;

    // C-linkage-compatible write callback; discards the response body.
    static size_t discard_body(void*, size_t sz, size_t nmemb, void*) noexcept;
};
