#pragma once
// reporter.hpp — All output logic: console table and CSV export.
// Pure output functions; no side effects beyond writing to streams/files.

#include <string>
#include <vector>
#include "auditor.hpp"

namespace Reporter {
    void print_banner();
    void print_table(const std::vector<PortReport>& reports);
    bool export_csv(const std::vector<PortReport>& port_reports,
                    const std::vector<StressReport>& stress_reports,
                    const std::string& path);
}
