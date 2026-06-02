#include "probe.hpp"
#include <curl/curl.h>

ProbeEngine::ProbeEngine(long timeout_ms) noexcept
    : timeout_ms_(timeout_ms) {}

// Returning 0 signals CURLE_WRITE_ERROR to curl, aborting the transfer.
// This is intentional: we only need TTFB, not the full response body.
size_t ProbeEngine::abort_after_ttfb(void*, size_t, size_t, void*) noexcept {
    return 0;
}

Measurement ProbeEngine::probe(const std::string& url,
                                const char* post_data) const
{
    Measurement m;
    CURL* h = curl_easy_init();
    if (!h) return m;

    curl_easy_setopt(h, CURLOPT_URL,               url.c_str());
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION,     &ProbeEngine::abort_after_ttfb);
    curl_easy_setopt(h, CURLOPT_TIMEOUT_MS,        timeout_ms_);
    curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT_MS, 5000L);
    curl_easy_setopt(h, CURLOPT_NOSIGNAL,          1L);
    curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION,    1L);
    curl_easy_setopt(h, CURLOPT_MAXREDIRS,         3L);
    if (post_data)
        curl_easy_setopt(h, CURLOPT_POSTFIELDS, post_data);

    CURLcode rc = curl_easy_perform(h);

    // CURLE_WRITE_ERROR is expected: our callback aborts intentionally after
    // the first byte. All timing counters are already recorded at that point.
    if (rc == CURLE_OK || rc == CURLE_WRITE_ERROR) {
        double ct, st;
        curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE,     &m.http_code);
        curl_easy_getinfo(h, CURLINFO_CONNECT_TIME,       &ct);
        curl_easy_getinfo(h, CURLINFO_STARTTRANSFER_TIME, &st);
        m.connect_ms     = ct * 1e3;
        m.ttfb_ms        = st * 1e3;
        m.app_latency_ms = (st - ct) * 1e3;
        m.success        = (m.http_code >= 100 && m.http_code < 600);
    }

    curl_easy_cleanup(h);
    return m;
}
