// SPDX-License-Identifier: MIT
#include "zenith/tweaks/cgroup_weight_tweak.hpp"

namespace zenith {

CgroupWeightTweak::CgroupWeightTweak(fs::path cgroup_root)
    : cgroup_root_(std::move(cgroup_root)),
      bg_slice_(cgroup_root_ / "zenith_background") {}

bool CgroupWeightTweak::probe() {
    std::error_code ec;
    if (!fs::exists(cgroup_root_, ec)) {
        available_ = false;
        return false;
    }
    available_ = true;
    return true;
}

bool CgroupWeightTweak::apply(Profile profile) {
    if (!probe()) return false;

    std::error_code ec;
    fs::create_directories(bg_slice_, ec);
    if (ec) return false;

    fs::path cpu_weight_path = bg_slice_ / "cpu.weight";
    fs::path cpu_idle_path = bg_slice_ / "cpu.idle";
    fs::path io_weight_path = bg_slice_ / "io.weight";

    // 1. cpu.weight (default 100, we lower background to 50 or 20)
    std::string target_weight = (profile == Profile::Aggressive) ? "20" : "50";
    if (fs::exists(cpu_weight_path)) {
        if (!orig_cpu_weight_) orig_cpu_weight_ = read_file_string(cpu_weight_path);
        write_file_string(cpu_weight_path, target_weight);
    }

    // 2. cpu.idle (Linux 5.12+ SCHED_IDLE weight for cgroup)
    if (fs::exists(cpu_idle_path)) {
        if (!orig_cpu_idle_) orig_cpu_idle_ = read_file_string(cpu_idle_path);
        write_file_string(cpu_idle_path, "1");
    }

    // 3. io.weight
    if (fs::exists(io_weight_path)) {
        if (!orig_io_weight_) orig_io_weight_ = read_file_string(io_weight_path);
        write_file_string(io_weight_path, target_weight);
    }

    return true;
}

bool CgroupWeightTweak::revert() {
    if (orig_cpu_weight_) {
        write_file_string(bg_slice_ / "cpu.weight", *orig_cpu_weight_);
        orig_cpu_weight_.reset();
    }
    if (orig_cpu_idle_) {
        write_file_string(bg_slice_ / "cpu.idle", *orig_cpu_idle_);
        orig_cpu_idle_.reset();
    }
    if (orig_io_weight_) {
        write_file_string(bg_slice_ / "io.weight", *orig_io_weight_);
        orig_io_weight_.reset();
    }
    return true;
}

std::string CgroupWeightTweak::describe() const {
    return "Soft prioritization slice: " + bg_slice_.string();
}

std::string CgroupWeightTweak::explain() const {
    return "Applies soft cgroup v2 weighting (cpu.weight=50, cpu.idle=1, io.weight=50) to background apps instead of hard-pinning them. "
           "Allows games 100% CPU time when needed without starving audio or IPC threads during idle moments. "
           "Risks: Near zero. Eliminates audio crackle from hard cpuset constraints.";
}

std::string CgroupWeightTweak::get_current_value() const {
    std::string w = "100";
    std::string id = "0";
    if (auto cur = read_file_string(bg_slice_ / "cpu.weight")) w = *cur;
    if (auto cur = read_file_string(bg_slice_ / "cpu.idle")) id = *cur;
    return "cpu.weight=" + w + ", cpu.idle=" + id;
}

std::string CgroupWeightTweak::get_target_value(Profile profile) const {
    std::string w = (profile == Profile::Aggressive) ? "20" : "50";
    return "cpu.weight=" + w + ", cpu.idle=1, io.weight=" + w;
}

} // namespace zenith
