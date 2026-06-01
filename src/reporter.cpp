#include "reporter.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>

// ---- Console ----

void Reporter::print_banner() {
    std::cout
        << "\n╔══════════════════════════════════════════════════════╗\n"
        << "║   Localhost Security Gap — Telemetrie Differentielle ║\n"
        << "║   ENSPY / Cybersecurite  —  RFC1918 uniquement        ║\n"
        << "╚══════════════════════════════════════════════════════╝\n\n";
}

void Reporter::print_table(const std::vector<PortReport>& reports) {
    std::cout << "\n── Rapport de Telemetrie ─────────────────────────────────────────\n";
    std::cout << std::left
              << std::setw(6)  << "Port"
              << std::setw(11) << "T_base(ms)"
              << std::setw(8)  << "σ(ms)"
              << std::setw(11) << "T_load(ms)"
              << std::setw(10) << "ΔT(ms)"
              << std::setw(9)  << "Z-score"
              << std::setw(8)  << "Cohen d"
              << std::setw(9)  << "Dual-r"
              << "Alerte\n"
              << std::string(80, '-') << "\n";

    for (const auto& r : reports) {
        if (!r.is_open) {
            std::cout << std::setw(6) << r.port << "  [FERME]\n";
            continue;
        }
        std::string alert;
        if (std::abs(r.z_score) > 3.0)  alert += "DEGRADATION ";
        if (r.dual.confirmed)            alert += "DUAL-CONFIRME";

        std::cout << std::fixed << std::setprecision(2)
                  << std::setw(6)  << r.port
                  << std::setw(11) << r.baseline.mean
                  << std::setw(8)  << r.baseline.stddev
                  << std::setw(11) << r.load.mean
                  << std::setw(10) << (r.load.mean - r.baseline.mean)
                  << std::setw(9)  << r.z_score
                  << std::setw(8)  << r.cohen_d
                  << std::setw(9)  << r.dual.ratio
                  << alert << "\n";
    }
    std::cout << std::string(80, '-') << "\n";
}

// ---- CSV export ----

bool Reporter::export_csv(const std::vector<PortReport>& port_reports,
                           const std::vector<StressReport>& stress_reports,
                           const std::string& path)
{
    std::ofstream f(path);
    if (!f) {
        std::cerr << "[WARN] Impossible d'écrire : " << path << "\n";
        return false;
    }

    f << "port,t_baseline_mean_ms,t_baseline_sigma_ms,t_baseline_median_ms,"
         "t_load_mean_ms,t_load_sigma_ms,t_load_p95_ms,t_load_p99_ms,"
         "delta_T_ms,z_score,cohen_d,dual_ratio,dual_confirmed,n_calib,n_load\n";

    for (const auto& r : port_reports) {
        if (!r.is_open) continue;
        f << std::fixed << std::setprecision(4)
          << r.port                               << ","
          << r.baseline.mean                      << ","
          << r.baseline.stddev                    << ","
          << r.baseline.median                    << ","
          << r.load.mean                          << ","
          << r.load.stddev                        << ","
          << r.load.p95                           << ","
          << r.load.p99                           << ","
          << (r.load.mean - r.baseline.mean)      << ","
          << r.z_score                            << ","
          << r.cohen_d                            << ","
          << r.dual.ratio                         << ","
          << (r.dual.confirmed ? "OUI" : "NON")  << ","
          << r.baseline.count                     << ","
          << r.load.count                         << "\n";
    }

    if (!stress_reports.empty()) {
        f << "\nport,concurrence,succes_pct,latence_moy_ms,latence_p99_ms,erreurs\n";
        for (const auto& sr : stress_reports)
            for (const auto& sp : sr.points)
                f << sr.port         << ","
                  << sp.concurrency  << ","
                  << sp.success_rate << ","
                  << sp.mean_ms      << ","
                  << sp.p99_ms       << ","
                  << sp.errors       << "\n";
    }

    std::cout << "[OK] Résultats exportés → " << path << "\n";
    return true;
}
