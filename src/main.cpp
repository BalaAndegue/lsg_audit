#include <iostream>
#include <sstream>
#include <cstring>
#include <stdexcept>
#include "validator.hpp"
#include "auditor.hpp"
#include "reporter.hpp"

struct Config {
    std::string       target_ip;
    std::vector<int>  ports        = {3000, 5000, 8080, 8082};
    int               calib_n      = 100;
    int               probe_n      = 60;
    std::vector<int>  stress_conc  = {5, 10, 20, 50};
    bool              do_stress    = false;
    std::string       csv_output   = "results/mesures.csv";
};

static Config parse_args(int argc, char* argv[]) {
    Config cfg;
    cfg.target_ip = argv[1];

    for (int i = 2; i < argc; ++i) {
        const std::string flag = argv[i];

        if (flag == "--stress") { cfg.do_stress = true; continue; }

        if (i + 1 >= argc) continue; // flags below need a value
        const std::string val = argv[++i];

        if (flag == "-p") {
            cfg.ports.clear();
            std::stringstream ss(val);
            std::string tok;
            while (std::getline(ss, tok, ','))
                cfg.ports.push_back(std::stoi(tok));
        } else if (flag == "-n") {
            cfg.calib_n = std::stoi(val);
        } else if (flag == "-m") {
            cfg.probe_n = std::stoi(val);
        } else if (flag == "-o") {
            cfg.csv_output = val;
        }
    }
    return cfg;
}

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " <IP_RFC1918> [options]\n"
              << "  -p <ports>   ex: 3000,5000,8080   (défaut: 3000,5000,8080,8082)\n"
              << "  -n <N>       calibration samples   (défaut: 100)\n"
              << "  -m <M>       mesures par port      (défaut: 60)\n"
              << "  -o <fichier> sortie CSV\n"
              << "  --stress     activer le stress test\n";
}

int main(int argc, char* argv[]) {
    Reporter::print_banner();

    if (argc < 2) { print_usage(argv[0]); return 1; }

    Config cfg;
    try {
        cfg = parse_args(argc, argv);
        if (!RFC1918Validator::is_private(cfg.target_ip)) {
            std::cerr << "[SECURITE] " << cfg.target_ip
                      << " n'appartient pas aux plages RFC1918. Abandon.\n";
            return 2;
        }
    } catch (const std::exception& e) {
        std::cerr << "[ERREUR] " << e.what() << "\n";
        return 2;
    }

    CurlContext curl_ctx; // RAII: init avant tout, cleanup à la sortie
    const Auditor auditor(cfg.target_ip);

    std::vector<PortReport>   port_results;
    std::vector<StressReport> stress_results;

    for (int port : cfg.ports) {
        std::cout << "\n=== PORT " << port << " ===\n";
        port_results.push_back(auditor.measure_port(port, cfg.calib_n, cfg.probe_n));
    }

    if (cfg.do_stress) {
        std::cout << "\n=== STRESS TEST ===\n";
        for (const auto& r : port_results) {
            if (r.is_open) {
                stress_results.push_back(
                    auditor.stress_port(r.port, cfg.stress_conc));
                break;
            }
        }
    }

    Reporter::print_table(port_results);
    Reporter::export_csv(port_results, stress_results, cfg.csv_output);
    return 0;
}
