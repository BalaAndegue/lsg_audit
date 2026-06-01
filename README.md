# lsg-audit

**Passive HTTP timing framework for remote characterization of development servers exposed on a LAN.**

A userspace audit tool that decomposes the HTTP Round-Trip Time to isolate the
pure server processing time (`T_calcul`) from network jitter, using libcurl's
internal timing counters. No kernel privileges required. No packet forgery.
Operates exclusively within RFC 1918 address space.

> Research context: this tool is the implementation artifact of a scientific
> paper submitted to an IEEE conference on network security.
> It serves as empirical validation of a formal timing telemetry model.

---

## Table of Contents

1. [What it does](#what-it-does)
2. [The Localhost Security Gap](#the-localhost-security-gap)
3. [Mathematical Model](#mathematical-model)
4. [Architecture](#architecture)
5. [Source Layout](#source-layout)
6. [Dependencies](#dependencies)
7. [Build](#build)
8. [Usage](#usage)
9. [Target Servers](#target-servers)
10. [Running the Full Experiment](#running-the-full-experiment)
11. [Output Format](#output-format)
12. [Statistical Method in Detail](#statistical-method-in-detail)
13. [Security and Ethics](#security-and-ethics)
14. [Branch Structure](#branch-structure)

---

## What it does

During software development, engineers routinely start built-in HTTP servers
(`python3 -m http.server`, `node server.js`, `php -S`, etc.) and configure them
to listen on `0.0.0.0` to allow testing from other machines. This exposes
unprotected services to the entire local network segment.

`lsg-audit` sends calibrated sequences of HTTP requests to these servers and:

- **Measures** the application processing time `T_calcul` on each port
- **Detects** statistically significant performance degradation (Welch z-score)
- **Classifies** degradation cause: applicative vs. network jitter (dual-probe)
- **Quantifies** effect size (Cohen's d) and saturation curves (stress test)
- **Exports** all metrics to CSV for further analysis and chart generation

The result is a rigorous, quantitative picture of the resilience (or lack thereof)
of each exposed server — without any intrusion into the host operating system.

---

## The Localhost Security Gap

A service bound to `0.0.0.0` accepts connections from any interface, including
the LAN-facing one. This creates an unintentional attack surface that combines
several weaknesses simultaneously:

| Property                  | Production server      | Development server (typical) |
|---------------------------|------------------------|------------------------------|
| TLS encryption            | Yes                    | No                           |
| Authentication            | Yes                    | No                           |
| Thread pool / async I/O   | Yes (nginx, gunicorn)  | No (single-threaded)         |
| Rate limiting             | Yes                    | No                           |
| Binding interface         | Specific               | `0.0.0.0` (all interfaces)   |

Single-threaded servers (Python `http.server`, PHP built-in, bare Node.js HTTP)
process one request at a time. A minimal concurrent load is sufficient to
saturate them — a property measurable purely from timing telemetry.

---

## Mathematical Model

### RTT decomposition

For an audit machine `A` probing a target machine `B` on port `p`:

```
T_total(p)  =  T_reseau  +  T_calcul(p)  +  ε
```

| Term                | Meaning                                               |
|---------------------|-------------------------------------------------------|
| `T_reseau`          | Network round-trip time (TCP handshake)               |
| `T_calcul(p)`       | Server application processing time on port `p`        |
| `ε ~ N(0, σ²)`      | Network jitter (approximately normal on a wired LAN)  |

### Isolation via libcurl timing counters

libcurl exposes two timestamps per request:

```
CURLINFO_CONNECT_TIME       →  t_connect   (TCP handshake complete)
CURLINFO_STARTTRANSFER_TIME →  t_TTFB      (first byte of response received)
```

The server processing time estimator is:

```
T_calcul_hat  =  t_TTFB − t_connect
```

This subtracts the pure network cost and isolates the interval during which
the server was processing the request before writing its first response byte.

### Robust baseline estimator

`N` sequential lightweight GET requests build a calibration sample
`x = (x₁, …, xN)`. Outliers caused by scheduler preemption, GC pauses or CPU
frequency scaling are removed via the IQR filter (Tukey fence ×1.5):

```
X_filtered = { xᵢ | Q1 − 1.5·IQR ≤ xᵢ ≤ Q3 + 1.5·IQR }

T_base = mean(X_filtered)
```

### Degradation detection

```
ΔT(p)  =  T_load_mean(p) − T_base(p)

        T_load_mean − T_base
z  =  ────────────────────────────
         σ_base / √(n_load)

Reject H0 (no degradation) if |z| > 3  →  p < 0.001
```

### Dual-probe verification

When `ΔT > 3σ` is measured on a heavy POST request, a lightweight GET witness
is fired immediately. The ratio:

```
r  =  ΔT_complex / ΔT_static
```

disambiguates the cause:

- `r ≈ 1` — both requests degrade equally → network spike → jitter artifact
- `r > 2` — only the complex request degrades → applicative saturation confirmed

---

## Architecture

```
┌──────────────────────────────────────────────────────────────────────┐
│  main.cpp  —  argument parsing, security check, top-level control    │
│                                                                       │
│   argv  ──►  parse_args()  ──►  Config                               │
│                                    │                                  │
│                          RFC1918Validator::is_private()               │
│                                    │ (exit 2 if public IP)            │
│                                    ▼                                  │
│                             CurlContext  (RAII global init)           │
│                                    │                                  │
│                             Auditor(target_ip)                        │
│                            ┌───────┴────────────────┐                 │
│                    measure_port()           stress_port()             │
│                         │                       │                     │
│              ┌──────────┼──────────┐      N threads (std::thread)    │
│           calibrate() load_probe() dual_probe()   │                  │
│              │           │            │     ProbeEngine::probe()      │
│              └───────────┴────────────┘                               │
│                          │                                            │
│                    StatsEngine                                        │
│                  (IQR, Welch z, Cohen d)                              │
│                          │                                            │
│                       Reporter                                        │
│                  (console table + CSV)                                │
└──────────────────────────────────────────────────────────────────────┘
```

### Design principles

- **Single responsibility** — each module has exactly one reason to change
- **No shared mutable state** — `StatsEngine` and `RFC1918Validator` are stateless
- **RAII throughout** — `CurlContext` wraps curl global lifecycle; each probe owns and destroys its curl handle
- **Thread safety** — stress test threads each own an independent `ProbeEngine` call; shared results are mutex-protected
- **No kernel privileges** — pure userspace, no raw sockets, no pcap

---

## Source Layout

```
.
├── src/
│   ├── validator.hpp        RFC 1918 guard — no deps, not instantiable
│   ├── probe.hpp            Measurement struct declaration
│   ├── probe.cpp            ProbeEngine — one libcurl handle per call (RAII)
│   ├── stats.hpp            Stats struct + StatsEngine declaration
│   ├── stats.cpp            IQR filter, Welch z-score, Cohen's d
│   ├── auditor.hpp          CurlContext, PortReport, StressReport, Auditor
│   ├── auditor.cpp          calibrate(), load_probe(), dual_probe(), stress_once()
│   ├── reporter.hpp         Reporter namespace declaration
│   ├── reporter.cpp         print_table(), export_csv()
│   └── main.cpp             Config, parse_args(), main()
│
├── target/
│   ├── server_node_vulnerable.js    Node.js built-in HTTP   — port 3000
│   ├── server_node_express.js       Express + busy-wait     — port 5000
│   ├── server_python.py             Python http.server      — port 8080
│   └── server_php.php               PHP built-in            — port 8082
│
├── results/                 Runtime output (CSV, charts) — git-ignored
├── analyze.py               Statistical summary + PDF chart generation
├── run_experiment.sh        End-to-end experiment automation script
├── Makefile
└── LICENSE
```

---

## Dependencies

### Runtime

| Library    | Min version | Purpose                                      |
|------------|-------------|----------------------------------------------|
| libcurl    | 7.61.0      | HTTP probing and internal timing counters     |
| pthreads   | POSIX       | Concurrent stress-test threads               |

### Build

| Tool       | Min version | Notes                                        |
|------------|-------------|----------------------------------------------|
| g++ / clang| C++17       | `std::thread`, lambdas, structured bindings  |
| GNU make   | 4.x         |                                              |
| curl-config| —           | Auto-detects include/link paths              |

### Target servers (machine B only)

| Runtime    | Version | Server file                    |
|------------|---------|--------------------------------|
| Node.js    | ≥ 18    | server_node_vulnerable.js      |
| npm/express| latest  | server_node_express.js         |
| Python     | ≥ 3.10  | server_python.py               |
| PHP        | ≥ 8.0   | server_php.php                 |

### Analysis (optional)

```bash
pip3 install matplotlib
```

### Install libcurl headers

```bash
# Debian / Ubuntu
sudo apt install libcurl4-openssl-dev

# Anaconda (auto-detected by Makefile via curl-config)
# no action needed if curl-config resolves to anaconda/bin/curl-config
```

---

## Build

```bash
make            # compile all translation units → ./telemetry
make clean      # remove object files and binary
make check_deps # verify libcurl is findable
make article    # compile the LaTeX paper (requires texlive)
```

The Makefile resolves libcurl paths automatically:

```makefile
CURL_INC := $(shell curl-config --cflags)
CURL_LIB := $(shell curl-config --libs)
```

Manual path override:

```bash
make CURL_INC="-I/usr/include" CURL_LIB="-lcurl"
```

---

## Usage

```
./telemetry <TARGET_IP> [OPTIONS]
```

`TARGET_IP` must be a valid IPv4 address in RFC 1918 or loopback space.
Any other value causes immediate exit with code `2` and an error message.

### Options

| Flag         | Default                 | Description                                    |
|--------------|-------------------------|------------------------------------------------|
| `-p <ports>` | `3000,5000,8080,8082`   | Comma-separated target ports                   |
| `-n <N>`     | `100`                   | Calibration probes per port (builds baseline)  |
| `-m <M>`     | `60`                    | Load probes per port (GET + POST alternating)  |
| `-o <file>`  | `results/mesures.csv`   | CSV output path                                |
| `--stress`   | disabled                | Run concurrent stress test on first open port  |

### Exit codes

| Code | Meaning                          |
|------|----------------------------------|
| `0`  | Success                          |
| `1`  | Missing arguments                |
| `2`  | Security violation (non-RFC1918) |

### Examples

```bash
# Scan a LAN target on default ports
./telemetry 192.168.1.42

# Custom ports, reduced sample count (faster iteration)
./telemetry 192.168.1.42 -p 3000,8080 -n 50 -m 30

# Full experiment: telemetry + stress test, named output
./telemetry 192.168.1.42 -n 100 -m 60 --stress -o results/run_01.csv

# Block on public IP (expected: exit 2)
./telemetry 8.8.8.8

# Local smoke test (requires a server on 8080)
./telemetry 127.0.0.1 -p 8080 -n 30 -m 20
```

---

## Target Servers

Start these on **machine B** before running the audit from machine A.

```bash
# Node.js built-in HTTP — port 3000
node target/server_node_vulnerable.js

# Express (install dependency first) — port 5000
npm install express
node target/server_node_express.js

# Python http.server — port 8080
python3 target/server_python.py

# PHP built-in — port 8082
php -S 0.0.0.0:8082 target/server_php.php
```

Each server exposes two endpoints designed to create a measurable timing contrast:

| Endpoint        | Method | Behavior                                          |
|-----------------|--------|---------------------------------------------------|
| `/`             | GET    | Static plain-text response (~10 bytes)            |
| `/api/compute`  | POST   | Iterative SHA-256 over the request body (N loops) |

The `/` endpoint serves as the **witness probe** in dual-probe verification.
The `/api/compute` endpoint is the **complex probe** that triggers measurable
`T_calcul` under load.

### Vulnerability profile per server

| Server              | Threading model      | Saturation behavior          |
|---------------------|----------------------|------------------------------|
| Node.js built-in    | Single-threaded, sync| Queues then drops under load |
| Express             | Event loop           | More resilient, degrades later|
| Python http.server  | Single-threaded, sync| Serializes all requests       |
| PHP built-in        | Single-process       | One request at a time         |

---

## Running the Full Experiment

```bash
./run_experiment.sh <TARGET_IP>
```

This script performs in sequence:

1. **Connectivity check** — `curl` ping on each port, reports open/closed
2. **Telemetry run** — calibration (N=100) + load probing (M=60) on all 4 ports
3. **Stress test** — concurrency sweep: 5 → 10 → 20 → 50 simultaneous requests
4. **Timestamped output** — writes `results/mesures_YYYYMMDD_HHMMSS.csv`

After the run, generate statistical summaries and PDF charts:

```bash
pip3 install matplotlib   # once, if not installed
python3 analyze.py results/mesures_<TIMESTAMP>.csv
```

**Outputs from `analyze.py`:**

- Console table: mean, σ, ΔT, z-score, Cohen's d per port
- `results/fig_latency_comparison.pdf` — T_base vs T_load bar chart with ±σ error bars
- `results/fig_zscores.pdf` — horizontal bar chart with z=±3 rejection threshold

---

## Output Format

### Console table (live, during execution)

```
Port  T_base(ms) σ(ms)  T_load(ms) ΔT(ms)   Z-score  Cohen d Dual-r   Alerte
────────────────────────────────────────────────────────────────────────────────
3000   1.24       0.08     8.91      7.67      412.3    2.31    186.2   DEGRADATION DUAL-CONFIRME
5000   1.31       0.11     3.20      1.89       73.8    0.98     41.7   DEGRADATION
8080   0.62       0.01     7.68      7.06     1256.2    1.53   1252.5   DEGRADATION DUAL-CONFIRME
8082   0.95       0.04    11.43     10.48      948.1    3.12    876.3   DEGRADATION DUAL-CONFIRME
```

### CSV — telemetry section

```
port, t_baseline_mean_ms, t_baseline_sigma_ms, t_baseline_median_ms,
t_load_mean_ms, t_load_sigma_ms, t_load_p95_ms, t_load_p99_ms,
delta_T_ms, z_score, cohen_d, dual_ratio, dual_confirmed, n_calib, n_load
```

### CSV — stress section (appended after blank line)

```
port, concurrence, succes_pct, latence_moy_ms, latence_p99_ms, erreurs
```

### Field reference

| Field              | Description                                                  |
|--------------------|--------------------------------------------------------------|
| `t_baseline_mean`  | Mean `T_calcul` during calibration (IQR-filtered)           |
| `t_baseline_sigma` | Standard deviation of the calibration sample                |
| `t_load_mean`      | Mean `T_calcul` during load probing                         |
| `delta_T_ms`       | `t_load_mean − t_baseline_mean`                             |
| `z_score`          | Welch z: σ distance between load and baseline means         |
| `cohen_d`          | Effect size via pooled std dev (> 0.8 = large effect)       |
| `dual_ratio`       | `ΔT_complex / ΔT_static` from dual-probe verification       |
| `dual_confirmed`   | `OUI` if `dual_ratio > 2` AND `ΔT_complex > 3σ`            |

---

## Statistical Method in Detail

### Phase 1 — Calibration

`N` sequential GET requests to the root endpoint (`/`). Each measurement yields
`T_calcul_hat = t_TTFB − t_connect`. The IQR filter (Tukey ×1.5) removes outliers
caused by:
- OS scheduler preemption during measurement
- CPU frequency scaling (P-states)
- JVM / Python GC pauses on the target
- ARP resolution on first contact

The filtered mean `T_base` and standard deviation `σ_base` form the reference.

### Phase 2 — Load probing

`M` requests alternating GET and POST (with a 512-byte payload). This mimics
realistic mixed traffic. `T_calcul_hat` is recorded for each successful request.
The same IQR filter is applied before computing `T_load_mean`.

### Phase 3 — Hypothesis testing

```
H0 : ΔT(p) = 0       server is not degraded
H1 : ΔT(p) > k·σ     applicative degradation present

Welch z-score:

        T_load_mean − T_base
z  =  ─────────────────────────────
          σ_base / √(n_load)

Decision rule: reject H0 if |z| > 3  (p < 0.001, one-sided)
```

### Phase 4 — Dual-probe verification

Fired only when the load phase triggers H1. Sends two requests back-to-back:

1. `POST /api/compute` with a 4 KB payload (complex probe)
2. `GET /` (static witness probe)

The ratio `r = ΔT_complex / ΔT_static` is the discrimination criterion:

| `r` value | Interpretation                              | Decision     |
|-----------|---------------------------------------------|--------------|
| `≈ 1`     | Both requests equally affected              | Network spike, H0 retained |
| `> 2`     | Complex request disproportionately slow     | Applicative saturation, H1 confirmed |

### Phase 5 — Effect size

Cohen's d quantifies the practical significance (not just statistical):

```
       T_load_mean − T_base
d  =  ──────────────────────
              s_p

s_p  =  √[ ((n₁−1)σ₁² + (n₂−1)σ₂²) / (n₁+n₂−2) ]
```

| `|d|` range | Interpretation |
|-------------|----------------|
| < 0.2       | Negligible     |
| 0.2 – 0.5   | Small          |
| 0.5 – 0.8   | Medium         |
| > 0.8       | Large          |

### Phase 6 — Stress test

Concurrency levels `C ∈ {5, 10, 20, 50}` are tested sequentially. At each level,
`C` threads are spawned simultaneously, each sending one GET request and recording
its `T_calcul_hat`. Results expose the saturation curve: single-threaded servers
show a linear or super-linear growth in latency with concurrency, while
event-loop servers degrade more gracefully.

---

## Security and Ethics

### Technical enforcement — RFC 1918

The binary validates the target IP before initializing any libcurl handle:

```cpp
if (!RFC1918Validator::is_private(cfg.target_ip)) {
    std::cerr << "[SECURITE] non-RFC1918 address. Abort.\n";
    return 2;
}
```

Accepted ranges: `10.0.0.0/8`, `172.16.0.0/12`, `192.168.0.0/16`, `127.0.0.0/8`.
This check cannot be bypassed without modifying and recompiling the source.

### No data collection

The response body of every HTTP probe is silently discarded via a libcurl
write callback that does nothing. The tool stores only numeric timing metrics.

### Authorized use only

This tool is intended for:

- Audit of infrastructure you own or have **explicit written authorization** to test
- Academic research on isolated, dedicated testbeds
- Security awareness and configuration hardening in institutional environments

Unauthorized scanning of third-party systems — even on a shared LAN — may
violate applicable law:
- Cameroon Law No. 2010/012 on cybersecurity and cybercrime
- Budapest Convention on Cybercrime (Art. 2–5)

---

## Branch Structure

| Branch | Contents                                                    |
|--------|-------------------------------------------------------------|
| `main` | C++ framework source, target servers, Makefile              |
| `docs` | README, IEEE paper (`article_localhost.tex`), `analyze.py`, `run_experiment.sh` |

The `docs` branch contains the full paper draft. Once the physical LAN
experiments are completed and result tables are filled in, a pull request
from `docs` → `main` will consolidate the final artifact.

---

*Francois Lionnel Bala Andegue — ENSPY, Département Génie Informatique et Cybersécurité, Yaoundé, Cameroun*
