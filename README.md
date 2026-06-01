# Localhost Security Gap — Differential Temporal Telemetry Framework

Userspace audit tool for passive characterization of development HTTP servers
exposed on a LAN via permissive `0.0.0.0` binding.

The tool isolates the pure application processing time (`T_calcul`) from network
jitter by decomposing the HTTP Round-Trip Time using libcurl's internal timing
counters. A dual-probe verification mechanism distinguishes genuine application
degradation from random jitter artifacts.

> **Scope:** LAN-only. Any target outside RFC 1918 / loopback space is rejected
> at startup. The binary is technically incapable of operating over the public
> internet.

---

## Table of Contents

1. [Architecture](#architecture)
2. [Dependencies](#dependencies)
3. [Build](#build)
4. [Usage](#usage)
5. [Target Servers](#target-servers)
6. [Running the Full Experiment](#running-the-full-experiment)
7. [Output Format](#output-format)
8. [Statistical Method](#statistical-method)
9. [Source Layout](#source-layout)
10. [Security & Ethics](#security--ethics)

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│  main.cpp  (entry point, argument parsing, orchestration)       │
│                                                                  │
│    Config ──► RFC1918Validator                                   │
│                      │                                           │
│                       ▼                                          │
│              CurlContext (RAII)                                   │
│                      │                                           │
│                       ▼                                          │
│             Auditor(target_ip)                                   │
│            ┌──────────┴──────────────────┐                       │
│     measure_port()             stress_port()                     │
│      │                               │                           │
│      ├─ calibrate()         stress_once() × C threads            │
│      │    └─ ProbeEngine::probe()  (GET ×N)                      │
│      ├─ load_probe()                                             │
│      │    └─ ProbeEngine::probe()  (GET+POST ×M)                 │
│      └─ dual_probe()                                             │
│           ├─ ProbeEngine::probe()  (POST heavy)                  │
│           └─ ProbeEngine::probe()  (GET witness)                 │
│                      │                                           │
│               StatsEngine (IQR filter, Welch z, Cohen d)        │
│                      │                                           │
│                       ▼                                          │
│             Reporter (console table + CSV)                       │
└─────────────────────────────────────────────────────────────────┘
```

Each module has a single responsibility:

| Module           | Responsibility                                           |
|------------------|----------------------------------------------------------|
| `validator`      | RFC 1918 address guard — no side effects                 |
| `probe`          | One HTTP request via libcurl; owns handle lifetime (RAII)|
| `stats`          | IQR filter, descriptive stats, Welch z-score, Cohen's d  |
| `auditor`        | Measurement pipeline: calibrate, load probe, dual-probe, stress |
| `reporter`       | All output: formatted console table, CSV file            |
| `main`           | Config, argument parsing, top-level orchestration        |

---

## Dependencies

| Dependency   | Version | Purpose                         |
|--------------|---------|---------------------------------|
| C++ compiler | C++17   | `std::thread`, structured bindings, `std::string_view` |
| libcurl      | ≥ 7.61  | HTTP probing and internal timing counters |
| pthreads     | POSIX   | Concurrent stress-test threads  |
| Python 3     | ≥ 3.10  | Target servers + analysis script |
| Node.js      | ≥ 18    | Node.js / Express target servers |
| matplotlib   | ≥ 3.5   | PDF/PNG charts (optional)       |

**Install libcurl development headers:**

```bash
# Debian / Ubuntu
sudo apt install libcurl4-openssl-dev

# If Anaconda is present, curl-config is found automatically via the Makefile
```

---

## Build

```bash
make          # compiles all .cpp units into ./telemetry
make clean    # removes object files and binary
make article  # compiles the LaTeX paper (requires texlive)
```

The `Makefile` auto-detects include and link paths through `curl-config`.
Manual override:

```bash
make CURL_INC="-I/usr/include" CURL_LIB="-lcurl"
```

**Compiler requirements:** GCC ≥ 12 or Clang ≥ 14 with `-std=c++17`.

---

## Usage

```
./telemetry <TARGET_IP> [OPTIONS]
```

`TARGET_IP` must be a valid IPv4 address in RFC 1918 or loopback space.
Any other address causes immediate exit with code `2`.

### Options

| Flag            | Default                   | Description                               |
|-----------------|---------------------------|-------------------------------------------|
| `-p <ports>`    | `3000,5000,8080,8082`     | Comma-separated list of ports to scan     |
| `-n <N>`        | `100`                     | Number of calibration probes per port     |
| `-m <M>`        | `60`                      | Number of load probes per port            |
| `-o <file>`     | `results/mesures.csv`     | CSV output path                           |
| `--stress`      | off                       | Run concurrent stress test on first open port |

### Examples

```bash
# Basic scan of a LAN target
./telemetry 192.168.1.42

# Custom ports, reduced sample count (faster)
./telemetry 192.168.1.42 -p 3000,8080 -n 50 -m 30

# Full experiment: scan + stress test, named output
./telemetry 192.168.1.42 -n 100 -m 60 --stress -o results/run_01.csv

# Loopback test (development / CI)
./telemetry 127.0.0.1 -p 8080 -n 30 -m 20
```

---

## Target Servers

Four representative development servers are provided under `target/`.
Deploy them on the **target machine** (machine B):

```bash
# Node.js built-in HTTP (port 3000)
node target/server_node_vulnerable.js 3000

# Node.js Express (port 5000)  —  requires: npm install express
node target/server_node_express.js 5000

# Python http.server (port 8080)
python3 target/server_python.py 8080

# PHP built-in server (port 8082)
php -S 0.0.0.0:8082 target/server_php.php
```

Each server exposes two endpoints:

| Endpoint        | Method | Behavior                                      |
|-----------------|--------|-----------------------------------------------|
| `/`             | GET    | Static response — lightweight witness probe   |
| `/api/compute`  | POST   | Iterative SHA-256 hash — heavy computation    |

The contrast between these two endpoints is what the dual-probe mechanism
uses to confirm applicative vs. network-induced degradation.

---

## Running the Full Experiment

The shell script automates the complete measurement sequence:

```bash
./run_experiment.sh <TARGET_IP>
```

This performs in order:

1. **Connectivity check** — verifies which ports are reachable
2. **Telemetry run** — calibration (N=100) + load probing (M=60) on all ports
3. **Stress test** — concurrency sweep: 5 → 10 → 20 → 50 simultaneous requests

After the run, generate statistical summaries and PDF charts:

```bash
python3 analyze.py results/mesures_<TIMESTAMP>.csv
```

Outputs:
- Console table with mean, σ, ΔT, z-score, Cohen's d per port
- `results/fig_latency_comparison.pdf` — bar chart T_base vs T_load ±σ
- `results/fig_zscores.pdf` — horizontal bar chart of z-scores with rejection threshold

---

## Output Format

### Console table

```
Port  T_base(ms) σ(ms)  T_load(ms) ΔT(ms)   Z-score  Cohen d Dual-r   Alerte
──────────────────────────────────────────────────────────────────────────────
8080  0.62       0.01    7.68       7.06      1256.2   1.53    1252.5   DEGRADATION DUAL-CONFIRME
```

- **T_base** — mean app latency during calibration (IQR-filtered)
- **σ** — standard deviation of the calibration sample
- **T_load** — mean app latency during load probing
- **ΔT** — `T_load − T_base`
- **Z-score** — Welch z: how many σ above baseline is T_load?
- **Cohen d** — effect size (> 0.8 = large effect)
- **Dual-r** — ratio `ΔT_complex / ΔT_static`; > 2.0 implies applicative cause

### CSV columns

```
port, t_baseline_mean_ms, t_baseline_sigma_ms, t_baseline_median_ms,
t_load_mean_ms, t_load_sigma_ms, t_load_p95_ms, t_load_p99_ms,
delta_T_ms, z_score, cohen_d, dual_ratio, dual_confirmed, n_calib, n_load
```

Stress section (appended after a blank line):

```
port, concurrence, succes_pct, latence_moy_ms, latence_p99_ms, erreurs
```

---

## Statistical Method

### Time decomposition

```
T_total(p)  =  T_reseau  +  T_calcul(p)  +  ε
```

- `T_reseau` — network round-trip (TCP handshake, measured via `CURLINFO_CONNECT_TIME`)
- `T_calcul` — server processing time, isolated as `CURLINFO_STARTTRANSFER_TIME − CURLINFO_CONNECT_TIME`
- `ε ~ N(0, σ²)` — jitter, filtered via IQR (Tukey fence ×1.5)

### Calibration estimator

N sequential lightweight GET requests build a baseline.
Outliers (scheduler spikes, GC pauses, cache misses) are removed using
the IQR filter before computing the reference mean `T_base`.

### Degradation test

```
H0: ΔT = 0   (server is not degraded)
H1: ΔT > k·σ (applicative degradation detected)

        T_load_mean − T_base_mean
z  =  ─────────────────────────────
           σ_base / √(n_load)

Reject H0 if |z| > 3  →  p < 0.001
```

### Dual-probe verification

When `ΔT > 3σ` is observed on a heavy POST request, an immediate GET
witness request is fired. The ratio `r = ΔT_complex / ΔT_static` determines
the cause:

- `r ≈ 1` — both requests are affected equally → network spike → H0 retained
- `r > 2` AND `ΔT_complex > 3σ` → only the complex request degrades → applicative cause → H1 confirmed

---

## Source Layout

```
.
├── src/
│   ├── validator.hpp       RFC 1918 address guard (no dependencies)
│   ├── probe.hpp / .cpp    ProbeEngine + Measurement — libcurl wrapper
│   ├── stats.hpp / .cpp    StatsEngine — IQR, Welch z, Cohen d
│   ├── auditor.hpp / .cpp  Auditor + CurlContext — measurement pipeline
│   ├── reporter.hpp / .cpp Reporter namespace — console table + CSV
│   └── main.cpp            Config, argument parsing, orchestration
├── target/
│   ├── server_node_vulnerable.js   Node.js HTTP (port 3000)
│   ├── server_node_express.js      Express     (port 5000)
│   ├── server_python.py            Python      (port 8080)
│   └── server_php.php              PHP         (port 8082)
├── results/                        CSV output + generated charts
├── analyze.py                      Statistical analysis + PDF charts
├── run_experiment.sh               End-to-end experiment script
├── Makefile
└── article_localhost.tex           IEEE paper (IEEEtran)
```

---

## Security & Ethics

**Technical restriction — RFC 1918 enforcement**

The binary performs an `inet_pton` + subnet mask check before initializing
any libcurl handle. Targets outside `10/8`, `172.16/12`, `192.168/16`,
and `127/8` cause an immediate `exit(2)`. This check cannot be bypassed
without modifying and recompiling the source.

**Authorized use only**

This tool is designed for:
- Audit of infrastructure you own or have explicit written authorization to test
- Academic research on closed, dedicated testbeds
- Defensive security awareness and configuration hardening

Unauthorized scanning of third-party systems, even on a shared LAN,
may violate applicable law (Cameroon Law No. 2010/012 on cybersecurity,
Budapest Convention on Cybercrime).

---

*Francois Lionnel Bala Andegue — ENSPY, Département Génie Informatique et Cybersécurité*
