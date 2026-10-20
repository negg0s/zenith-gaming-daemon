#include "zenith/pressure_monitor.hpp"
#include <sstream>
#include <algorithm>

namespace zenith {

namespace {

void parse_psi_line(std::string_view line, PsiMetric& out) {
    std::string line_str(line);
    std::istringstream stream(line_str);
    std::string token;
    while (stream >> token) {
        if (token.starts_with("avg10=")) {
            try { out.avg10 = std::stod(token.substr(6)); } catch (...) {}
        } else if (token.starts_with("avg60=")) {
            try { out.avg60 = std::stod(token.substr(6)); } catch (...) {}
        } else if (token.starts_with("avg300=")) {
            try { out.avg300 = std::stod(token.substr(7)); } catch (...) {}
        } else if (token.starts_with("total=")) {
            try { out.total_us = std::stoull(token.substr(6)); } catch (...) {}
        }
    }
}

} // namespace

PressureMonitor::PressureMonitor(IPressureFs* fs)
    : fs_(fs ? fs : &RealPressureFs::instance()) {}

bool PressureMonitor::is_psi_supported() const {
    return fs_->exists("/proc/pressure/cpu");
}

void PressureMonitor::parse_psi_file(
    std::string_view content,
    PsiMetric& out_some,
    std::optional<std::reference_wrapper<PsiMetric>> out_full
) {
    std::string content_str(content);
    std::istringstream stream(content_str);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.starts_with("some ")) {
            parse_psi_line(line.substr(5), out_some);
        } else if (line.starts_with("full ") && out_full.has_value()) {
            parse_psi_line(line.substr(5), out_full->get());
        }
    }
}

void PressureMonitor::sample_psi(SystemPressure& sp) const {
    // 1. CPU Pressure
    if (auto cpu_data = fs_->read_file("/proc/pressure/cpu")) {
        parse_psi_file(*cpu_data, sp.cpu_some);
    }

    // 2. Memory Pressure
    if (auto mem_data = fs_->read_file("/proc/pressure/memory")) {
        parse_psi_file(*mem_data, sp.memory_some, sp.memory_full);
    }

    // 3. I/O Pressure
    if (auto io_data = fs_->read_file("/proc/pressure/io")) {
        parse_psi_file(*io_data, sp.io_some, sp.io_full);
    }
}

void PressureMonitor::sample_thermals(SystemPressure& sp) {
    uint32_t max_cpu_temp = 0;

    // Scan hwmon for core temperatures
    fs::path hwmon_dir = "/sys/class/hwmon";
    auto entries = fs_->list_directory(hwmon_dir);
    for (const auto& hm : entries) {
        for (int i = 1; i <= 8; ++i) {
            auto temp_file = hm / ("temp" + std::to_string(i) + "_input");
            if (auto t_str = fs_->read_file(temp_file)) {
                try {
                    uint32_t millideg = std::stoul(trim(*t_str));
                    uint32_t deg = millideg / 1000;
                    if (deg > max_cpu_temp && deg < 150) {
                        max_cpu_temp = deg;
                    }
                } catch (...) {}
            }
        }
    }

    // Fallback to thermal zones
    if (max_cpu_temp == 0) {
        fs::path tz_dir = "/sys/class/thermal";
        auto tzs = fs_->list_directory(tz_dir);
        for (const auto& tz : tzs) {
            std::string name = tz.filename().string();
            if (name.starts_with("thermal_zone")) {
                if (auto t_str = fs_->read_file(tz / "temp")) {
                    try {
                        uint32_t millideg = std::stoul(trim(*t_str));
                        uint32_t deg = millideg / 1000;
                        if (deg > max_cpu_temp && deg < 150) {
                            max_cpu_temp = deg;
                        }
                    } catch (...) {}
                }
            }
        }
    }
    sp.cpu_max_temp_c = max_cpu_temp;

    // Read CPU core throttle counts
    uint64_t total_throttle = 0;
    fs::path cpu_dir = "/sys/devices/system/cpu";
    auto cpus = fs_->list_directory(cpu_dir);
    for (const auto& cpu : cpus) {
        std::string name = cpu.filename().string();
        if (name.starts_with("cpu") && name.length() > 3 && std::isdigit(static_cast<unsigned char>(name[3]))) {
            auto th_file = cpu / "thermal_throttle" / "core_throttle_count";
            if (auto cnt = fs_->read_file(th_file)) {
                try { total_throttle += std::stoull(trim(*cnt)); } catch (...) {}
            }
        }
    }

    sp.thermal_throttle_count = total_throttle;
    if (baseline_initialized_) {
        if (total_throttle > last_throttle_count_) {
            sp.is_thermal_throttling = true;
        }
    } else {
        baseline_initialized_ = true;
    }
    last_throttle_count_ = total_throttle;
}

SystemPressure PressureMonitor::sample() {
    SystemPressure sp;
    sample_psi(sp);
    sample_thermals(sp);
    return sp;
}

} // namespace zenith
