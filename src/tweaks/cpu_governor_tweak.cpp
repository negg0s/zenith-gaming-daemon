// SPDX-License-Identifier: MIT
#include "zenith/tweaks/cpu_governor_tweak.hpp"
#include <sstream>

namespace zenith {

CpuGovernorTweak::CpuGovernorTweak(fs::path sysfs_root)
    : sysfs_root_(std::move(sysfs_root)) {}

bool CpuGovernorTweak::probe() {
    cpu_dirs_.clear();
    std::error_code ec;
    if (!fs::exists(sysfs_root_, ec) || !fs::is_directory(sysfs_root_, ec)) {
        available_ = false;
        return false;
    }

    for (const auto& entry : fs::directory_iterator(sysfs_root_, ec)) {
        if (!entry.is_directory()) continue;
        std::string filename = entry.path().filename().string();
        if (filename.starts_with("cpu") && filename.size() > 3 && std::isdigit(filename[3])) {
            fs::path cpufreq = entry.path() / "cpufreq";
            if (fs::exists(cpufreq, ec)) {
                cpu_dirs_.push_back(cpufreq);
            }
        }
    }

    available_ = !cpu_dirs_.empty();
    return available_;
}

bool CpuGovernorTweak::apply(Profile profile) {
    if (!probe()) return false;

    std::string target_gov;
    std::string target_epp;

    switch (profile) {
        case Profile::Safe:
            target_epp = "balance_performance";
            break;
        case Profile::Balanced:
            target_epp = "performance";
            break;
        case Profile::Ultra:
        case Profile::Aggressive:
            target_gov = "performance";
            target_epp = "performance";
            break;
    }

    bool any_applied = false;
    for (const auto& cpufreq : cpu_dirs_) {
        // Governor
        if (!target_gov.empty()) {
            fs::path gov_path = cpufreq / "scaling_governor";
            if (fs::exists(gov_path)) {
                auto cur = read_file_string(gov_path);
                if (cur && saved_governors_.find(gov_path.string()) == saved_governors_.end()) {
                    saved_governors_[gov_path.string()] = *cur;
                }
                if (write_file_string(gov_path, target_gov)) {
                    any_applied = true;
                }
            }
        }

        // EPP
        if (!target_epp.empty()) {
            fs::path epp_path = cpufreq / "energy_performance_preference";
            if (fs::exists(epp_path)) {
                auto cur = read_file_string(epp_path);
                if (cur && saved_epp_.find(epp_path.string()) == saved_epp_.end()) {
                    saved_epp_[epp_path.string()] = *cur;
                }
                if (write_file_string(epp_path, target_epp)) {
                    any_applied = true;
                }
            }
        }

        // Intel Energy Performance Bias (EPB)
        fs::path epb_path = cpufreq.parent_path() / "power" / "energy_perf_bias";
        if (fs::exists(epb_path)) {
            auto cur = read_file_string(epb_path);
            if (cur && saved_epb_.find(epb_path.string()) == saved_epb_.end()) {
                saved_epb_[epb_path.string()] = *cur;
            }
            if (write_file_string(epb_path, "0")) {
                any_applied = true;
            }
        }
    }

    return any_applied;
}

bool CpuGovernorTweak::revert() {
    bool all_ok = true;

    for (const auto& [path, original] : saved_governors_) {
        if (!write_file_string(path, original)) all_ok = false;
    }
    saved_governors_.clear();

    for (const auto& [path, original] : saved_epp_) {
        if (!write_file_string(path, original)) all_ok = false;
    }
    saved_epp_.clear();

    for (const auto& [path, original] : saved_epb_) {
        if (!write_file_string(path, original)) all_ok = false;
    }
    saved_epb_.clear();

    return all_ok;
}

std::string CpuGovernorTweak::describe() const {
    return "CPU Frequency scaling governor and CPPC EPP hints for " + std::to_string(cpu_dirs_.size()) + " cores";
}

std::string CpuGovernorTweak::explain() const {
    return "Sets CPU scaling governor to performance and hardware EPP (Energy Performance Preference) register to performance. "
           "Eliminates 5-15ms core frequency ramp latency during bursty frame rendering spikes. "
           "Risks: Increased idle power consumption and thermal load.";
}

std::string CpuGovernorTweak::get_current_value() const {
    if (cpu_dirs_.empty()) return "unavailable";
    std::string sample_gov = "unknown";
    std::string sample_epp = "unknown";

    fs::path g = cpu_dirs_.front() / "scaling_governor";
    fs::path e = cpu_dirs_.front() / "energy_performance_preference";
    if (auto val = read_file_string(g)) sample_gov = *val;
    if (auto val = read_file_string(e)) sample_epp = *val;

    return "gov=" + sample_gov + ", epp=" + sample_epp;
}

std::string CpuGovernorTweak::get_target_value(Profile profile) const {
    switch (profile) {
        case Profile::Safe: return "gov=(untouched), epp=balance_performance";
        case Profile::Balanced: return "gov=(untouched), epp=performance";
        case Profile::Ultra:
        case Profile::Aggressive: return "gov=performance, epp=performance";
    }
    return "gov=performance, epp=performance";
}

} // namespace zenith
