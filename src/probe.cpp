#include "probe.hpp"
#include <curl/curl.h>

ProbeEngine::ProbeEngine(long timeout_ms) noexcept
    : timeout_ms_(timeout_ms) {}

size_t ProbeEngine::discard_body(void*, size_t sz, size_t nmemb, void*) noexcept {
    return sz * nmemb;
}

Measurement ProbeEngine::probe(const std::string& url,
                                const char* post_data) const
{
    Measurement m;
    CURL* h = curl_easy_init();
    if (!h) return m;

    curl_easy_setopt(h, CURLOPT_URL,               url.c_str());
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION,     &ProbeEngine::discard_body);
    curl_easy_setopt(h, CURLOPT_TIMEOUT_MS,        timeout_ms_);
    curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT_MS, 2000L);
    curl_easy_setopt(h, CURLOPT_NOSIGNAL,          1L);
    curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION,    1L);
    if (post_data)
        curl_easy_setopt(h, CURLOPT_POSTFIELDS, post_data);

    if (curl_easy_perform(h) == CURLE_OK) {
        double ct, st;
        curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE,     &m.http_code);
        curl_easy_getinfo(h, CURLINFO_CONNECT_TIME,       &ct);
        curl_easy_getinfo(h, CURLINFO_STARTTRANSFER_TIME, &st); // true TTFB
        m.connect_ms     = ct * 1e3;
        m.ttfb_ms        = st * 1e3;
        m.app_latency_ms = (st - ct) * 1e3; // server processing time
        m.success        = (m.http_code >= 100 && m.http_code < 600);
    }

    curl_easy_cleanup(h);
    return m;
}
