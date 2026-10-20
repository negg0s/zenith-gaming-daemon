// SPDX-License-Identifier: MIT
#include "zenith/conflict_detector.hpp"
#include <iostream>

namespace zenith {

bool ConflictDetector::is_process_running(std::string_view name) {
#ifdef __linux__
    std::error_code ec;
    fs::path proc_dir = "/proc";
    if (!fs::exists(proc_dir, ec)) return false;

    for (const auto& entry : fs::directory_iterator(proc_dir, ec)) {
        if (!entry.is_directory()) continue;
        std::string filename = entry.path().filename().string();
        if (filename.empty() || !std::isdigit(filename[0])) continue;

        fs::path comm_path = entry.path() / "comm";
        if (auto comm = read_file_string(comm_path)) {
            if (*comm == name || comm->starts_with(name)) {
                return true;
            }
        }
    }
#else
    (void)name;
#endif
    return false;
}

bool ConflictDetector::is_sched_ext_active() {
    fs::path scx_state = "/sys/kernel/sched_ext/state";
    if (auto state = read_file_string(scx_state)) {
        return (*state == "enabled");
    }
    return false;
}

std::vector<DetectedConflict> ConflictDetector::probe_system_conflicts() {
    std::vector<DetectedConflict> conflicts;

    // 1. Feral GameMode (gamemoded)
    if (is_process_running("gamemoded") || fs::exists("/run/gamemode")) {
        conflicts.push_back({
            "gamemode",
            "Feral Interactive GameMode daemon",
            "Both daemons attempt to adjust CPU governor, GPU clocks, and process niceness simultaneously.",
            "Zenith will coordinate gracefully and avoid fighting GameMode over GPU/CPU clocks."
        });
    }

    // 2. Ananicy / Ananicy-cpp
    if (is_process_running("ananicy") || is_process_running("ananicy-cpp")) {
        conflicts.push_back({
            "ananicy",
            "Ananicy auto-nicer process daemon",
            "Ananicy applies rule-based nice and cgroup rules to game and background processes.",
            "Zenith soft cgroups and uclamp priority will take precedence without overriding static Ananicy rules."
        });
    }

    // 3. Red Hat tuned
    if (is_process_running("tuned")) {
        conflicts.push_back({
            "tuned",
            "Red Hat Tuned profile daemon",
            "Tuned enforces background sysctl profiles that may conflict with Zenith kernel latency tuning.",
            "Zenith logs state changes to journal.json; if Tuned overrides them, consider setting Tuned profile to manual."
        });
    }

    // 4. power-profiles-daemon
    if (is_process_running("power-profiles-daemon")) {
        conflicts.push_back({
            "power-profiles-daemon",
            "Freedesktop power-profiles-daemon",
            "Desktop environment (GNOME/KDE) may attempt to reset EPP registers to 'power' or 'balanced'.",
            "Zenith asserts performance EPP for active gaming sessions."
        });
    }

    // 5. irqbalance
    if (is_process_running("irqbalance")) {
        conflicts.push_back({
            "irqbalance",
            "Kernel IRQ balancer daemon",
            "irqbalance distributes GPU and NVMe hardware interrupts across all CPU cores dynamically.",
            "For competitive low-latency, consider pinning GPU IRQs to gaming cores or stopping irqbalance."
        });
    }

    // 6. sched-ext (scx)
    if (is_sched_ext_active() || is_process_running("scx_")) {
        conflicts.push_back({
            "sched-ext",
            "Linux Extensible Scheduler (scx eBPF)",
            "A custom eBPF scheduler (e.g. scx_rusty, scx_bpfland) is managing thread dispatching.",
            "Zenith detects scx and skips CFS/EEVDF specific latency knobs to prevent interference."
        });
    }

    return conflicts;
}

void ConflictDetector::print_conflict_report(const std::vector<DetectedConflict>& conflicts) {
    if (conflicts.empty()) {
        std::cout << "\033[32m[Zenith Conflict Detector] No competing latency or power daemons detected on system.\033[0m\n";
        return;
    }

    std::cout << "\n\033[1;33m[!] ZENITH CONFLICT DETECTOR: " << conflicts.size() << " potentially competing services detected:\033[0m\n";
    for (const auto& c : conflicts) {
        std::cout << "  - \033[1m" << c.service_name << "\033[0m: " << c.description << "\n";
        std::cout << "    \033[33mImpact\033[0m: " << c.impact << "\n";
        std::cout << "    \033[36mCoordination\033[0m: " << c.recommended_action << "\n\n";
    }
}

} // namespace zenith
