#include "zenith/report.hpp"
#include "zenith/doctor.hpp"
#include "zenith/cpu_topology.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>

#if defined(__linux__)
#include <sys/utsname.h>
#include <unistd.h>
#endif

namespace zenith {

std::string IssueReportGenerator::sanitize_text(const std::string& input) {
    std::string text = input;

    // 1. Redact username
    const char* user = std::getenv("USER");
    if (user && std::strlen(user) > 0) {
        std::string username(user);
        size_t pos = 0;
        while ((pos = text.find(username, pos)) != std::string::npos) {
            text.replace(pos, username.length(), "$USER");
            pos += 5;
        }
    }

    // 2. Redact home directories like /home/$USER/...
    std::regex home_regex(R"(/home/[a-zA-Z0-9_\-]+)");
    text = std::regex_replace(text, home_regex, "/home/$USER");

    // 3. Redact non-loopback IP addresses
    std::regex ip_regex(R"(\b(?!127\.0\.0\.1)(?!0\.0\.0\.0)\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\b)");
    text = std::regex_replace(text, ip_regex, "XXX.XXX.XXX.XXX");

    return text;
}

std::string IssueReportGenerator::generate_anonymized_report() {
    std::ostringstream ss;

    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);

    ss << "### Zenith System Diagnostics & Issue Report\n\n";
    ss << "- **Timestamp**: " << std::put_time(std::gmtime(&now_c), "%Y-%m-%d %H:%M:%S UTC") << "\n";
    ss << "- **Zenith Version**: 1.0.0\n";

#if defined(__linux__)
    struct utsname buf;
    if (uname(&buf) == 0) {
        ss << "- **Kernel**: Linux " << buf.release << " (" << buf.machine << ")\n";
    }
#else
    ss << "- **Kernel**: Development Host\n";
#endif

    // Topology Summary
    CpuTopology topo = CpuTopology::detect();
    if (topo.total_cores() > 0) {
        ss << "- **Total Cores / Threads**: " << topo.total_cores() << "\n";
        ss << "- **Architecture**: " << (topo.has_vcache() ? "AMD 3D V-Cache" : (topo.has_hybrid() ? "Intel Hybrid (P/E)" : "Standard SMP")) << "\n";
    }

    ss << "\n#### System Doctor Results\n\n```text\n";
    auto items = Doctor::run_all_checks();
    for (const auto& it : items) {
        std::string mark = "[PASS]";
        if (it.status == DoctorStatus::Warning) mark = "[WARN]";
        if (it.status == DoctorStatus::Fail) mark = "[FAIL]";

        ss << std::left << std::setw(8) << mark << std::setw(28) << it.name << it.message << "\n";
        if (!it.advice.empty() && it.status != DoctorStatus::Pass) {
            ss << "         Advice: " << it.advice << "\n";
        }
    }
    ss << "```\n\n";

    // Active /etc/zenith/zenith.conf
    ss << "#### Active Configuration (`zenith.conf`)\n\n```ini\n";
    std::ifstream cfg_file("/etc/zenith/zenith.conf");
    if (cfg_file.is_open()) {
        std::string line;
        while (std::getline(cfg_file, line)) {
            ss << line << "\n";
        }
    } else {
        ss << "# Standard built-in default configuration\n";
    }
    ss << "```\n";

    return sanitize_text(ss.str());
}

} // namespace zenith
