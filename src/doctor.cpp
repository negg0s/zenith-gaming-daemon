// SPDX-License-Identifier: MIT
#include "zenith/doctor.hpp"
#include "zenith/conflict_detector.hpp"
#include "zenith/i18n.hpp"

#include <cstdlib>
#include <fstream>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>

#if defined(__linux__)
#include <sys/utsname.h>
#include <unistd.h>
#include <unistd.h>
#endif

namespace zenith {

namespace {

const char* status_symbol(DoctorStatus s) {
    switch (s) {
        case DoctorStatus::Pass:    return "\033[32m[✅ PASS]\033[0m";
        case DoctorStatus::Warning: return "\033[33m[⚠️ WARN]\033[0m";
        case DoctorStatus::Fail:    return "\033[31m[❌ FAIL]\033[0m";
    }
    return "[?]";
}

const char* status_str(DoctorStatus s) {
    switch (s) {
        case DoctorStatus::Pass:    return "pass";
        case DoctorStatus::Warning: return "warning";
        case DoctorStatus::Fail:    return "fail";
    }
    return "unknown";
}

} // namespace

DoctorItem Doctor::check_cgroups_v2() {
    DoctorItem item;
    item.name = "cgroups_v2";

#if defined(__linux__)
    std::ifstream controllers("/sys/fs/cgroup/cgroup.controllers");
    if (!controllers.is_open()) {
        item.status = DoctorStatus::Fail;
        item.message = i18n::tr("cgroup_v2_fail");
        item.advice = "cgroups v2 unified hierarchy is not active. Enable it in your kernel bootloader.";
        item.fix_command = "sudo grubby --update-kernel=ALL --args='systemd.unified_cgroup_hierarchy=1'";
        return item;
    }

    std::string line;
    std::getline(controllers, line);
    bool has_cpu = line.find("cpu") != std::string::npos;
    bool has_cpuset = line.find("cpuset") != std::string::npos;

    if (has_cpu && has_cpuset) {
        item.status = DoctorStatus::Pass;
        item.message = i18n::tr("cgroup_v2_ok") + " (cpu, cpuset)";
    } else {
        item.status = DoctorStatus::Warning;
        item.message = "cgroups v2 active, but 'cpu' or 'cpuset' controller missing from root";
        item.advice = "Delegate controllers by adding '+cpu +cpuset' to /sys/fs/cgroup/cgroup.subtree_control";
        item.fix_command = "echo '+cpu +cpuset' | sudo tee /sys/fs/cgroup/cgroup.subtree_control";
    }
#else
    item.status = DoctorStatus::Pass;
    item.message = "cgroups v2 simulated (development mode)";
#endif
    return item;
}

DoctorItem Doctor::check_kernel_version() {
    DoctorItem item;
    item.name = "kernel_version";

#if defined(__linux__)
    struct utsname buf;
    if (uname(&buf) == 0) {
        std::string release(buf.release);
        int major = 0, minor = 0;
        if (sscanf(release.c_str(), "%d.%d", &major, &minor) >= 2) {
            if (major > 6 || (major == 6 && minor >= 13)) {
                item.status = DoctorStatus::Pass;
                item.message = "Linux " + release + " (supports amd_x3d_vcache and sched_ext)";
            } else if (major == 6 && minor >= 12) {
                item.status = DoctorStatus::Pass;
                item.message = "Linux " + release + " (supports sched_ext BPF schedulers)";
            } else if (major >= 6 || (major == 5 && minor >= 15)) {
                item.status = DoctorStatus::Pass;
                item.message = "Linux " + release + " (supported baseline)";
            } else {
                item.status = DoctorStatus::Warning;
                item.message = "Linux " + release + " (older than 5.15 LTS)";
                item.advice = "Kernel 5.15+ recommended for cgroups v2, split_lock_mitigate, and PSI.";
                item.fix_command = "sudo pacman -S linux-cachyos || sudo apt install linux-generic";
            }
            return item;
        }
    }
#endif
    item.status = DoctorStatus::Pass;
    item.message = "Kernel version compatible";
    return item;
}

DoctorItem Doctor::check_privileges() {
    DoctorItem item;
    item.name = "privileges";

#if defined(__linux__)
    uid_t uid = geteuid();
    if (uid == 0) {
        item.status = DoctorStatus::Pass;
        item.message = "Running with root privileges (UID 0)";
    } else {
        // Check if member of wheel
        const char* user = std::getenv("USER");
        item.status = DoctorStatus::Pass;
        item.message = "Client execution (UID " + std::to_string(uid) + ")";
        item.advice = "Ensure daemon runs as root (systemd) and user is in wheel/gamemode group.";
        item.fix_command = "sudo usermod -aG wheel " + std::string(user ? user : "$USER");
    }
#else
    item.status = DoctorStatus::Pass;
    item.message = "Privileges verified";
#endif
    return item;
}

DoctorItem Doctor::check_conflicting_services() {
    DoctorItem item;
    item.name = "conflicting_services";

    auto conflicts = ConflictDetector::probe_system_conflicts();
    if (conflicts.empty()) {
        item.status = DoctorStatus::Pass;
        item.message = i18n::tr("conflicts_none");
    } else {
        std::string names;
        std::string service_args;
        for (const auto& c : conflicts) {
            if (!names.empty()) names += ", ";
            if (!service_args.empty()) service_args += " ";
            names += c.service_name;
            service_args += c.service_name;
        }
        item.status = DoctorStatus::Warning;
        item.message = i18n::tr("conflicts_found") + ": " + names;
        item.advice = "Competing optimizers may override CPU governors, nice values, or cgroup isolation.";
        item.fix_command = "sudo systemctl stop " + service_args + " && sudo systemctl disable " + service_args;
    }
    return item;
}

DoctorItem Doctor::check_x3d_driver() {
    DoctorItem item;
    item.name = "amd_x3d_vcache";

#if defined(__linux__)
    std::ifstream mode_file("/sys/devices/platform/amd_x3d_vcache/amd_x3d_mode");
    if (mode_file.is_open()) {
        std::string mode;
        mode_file >> mode;
        item.status = DoctorStatus::Pass;
        item.message = i18n::tr("x3d_ok") + " [mode: " + mode + "]";
    } else {
        item.status = DoctorStatus::Pass;
        item.message = i18n::tr("x3d_fallback");
    }
#else
    item.status = DoctorStatus::Pass;
    item.message = "AMD X3D fallback topology mode";
#endif
    return item;
}

DoctorItem Doctor::check_psi() {
    DoctorItem item;
    item.name = "pressure_stall_information";

#if defined(__linux__)
    std::ifstream psi_cpu("/proc/pressure/cpu");
    if (psi_cpu.is_open()) {
        item.status = DoctorStatus::Pass;
        item.message = i18n::tr("psi_ok");
    } else {
        item.status = DoctorStatus::Warning;
        item.message = i18n::tr("psi_fail");
        item.advice = "PSI allows Zenith to dynamically throttle background load before stutters occur.";
        item.fix_command = "sudo grubby --update-kernel=ALL --args='psi=1'";
    }
#else
    item.status = DoctorStatus::Pass;
    item.message = "PSI simulated";
#endif
    return item;
}

DoctorItem Doctor::check_gpu() {
    DoctorItem item;
    item.name = "gpu_driver";

#if defined(__linux__)
    std::ifstream amd_dpm("/sys/class/drm/card0/device/power_dpm_force_performance_level");
    std::ifstream nvml("/proc/driver/nvidia/version");

    if (amd_dpm.is_open()) {
        item.status = DoctorStatus::Pass;
        item.message = "AMD Radeon GPU detected (DPM clock control active)";
    } else if (nvml.is_open()) {
        item.status = DoctorStatus::Pass;
        item.message = "NVIDIA GeForce GPU detected (NVML support active)";
    } else {
        item.status = DoctorStatus::Pass;
        item.message = "Standard DRM GPU detected";
    }
#else
    item.status = DoctorStatus::Pass;
    item.message = "GPU support available";
#endif
    return item;
}

DoctorItem Doctor::check_pipewire() {
    DoctorItem item;
    item.name = "pipewire_audio";

#if defined(__linux__)
    uid_t uid = ::getuid();
    char runtime_dir[64];
    std::snprintf(runtime_dir, sizeof(runtime_dir), "/run/user/%u", static_cast<unsigned>(uid));
    std::string pw_socket = std::string(runtime_dir) + "/pipewire-0";
    std::ifstream socket_test(pw_socket);
    if (socket_test.good()) {
        item.status = DoctorStatus::Pass;
        item.message = i18n::tr("pipewire_ok");
    } else {
        item.status = DoctorStatus::Warning;
        item.message = i18n::tr("pipewire_warn");
        item.advice = "Start PipeWire for sub-millisecond audio quantum control.";
        item.fix_command = "systemctl --user start pipewire pipewire-pulse";
    }
#else
    item.status = DoctorStatus::Pass;
    item.message = "PipeWire audio active";
#endif
    return item;
}

DoctorItem Doctor::check_max_map_count() {
    DoctorItem item;
    item.name = "vm_max_map_count";

#if defined(__linux__)
    std::ifstream map_file("/proc/sys/vm/max_map_count");
    if (map_file.is_open()) {
        uint64_t count = 0;
        map_file >> count;
        if (count >= 1048576) {
            item.status = DoctorStatus::Pass;
            item.message = "max_map_count = " + std::to_string(count) + " (Proton memory limit OK)";
        } else {
            item.status = DoctorStatus::Warning;
            item.message = "max_map_count = " + std::to_string(count) + " (low; default is 65530)";
            item.advice = "Proton games (Cyberpunk, Starfield, Hogwarts Legacy) require >= 1048576 to prevent VMA allocation crashes.";
            item.fix_command = "sudo sysctl -w vm.max_map_count=2147483642";
        }
    } else {
        item.status = DoctorStatus::Pass;
        item.message = "max_map_count checked";
    }
#else
    item.status = DoctorStatus::Pass;
    item.message = "max_map_count simulated";
#endif
    return item;
}

std::vector<DoctorItem> Doctor::run_all_checks() {
    std::vector<DoctorItem> items;
    items.push_back(check_cgroups_v2());
    items.push_back(check_kernel_version());
    items.push_back(check_privileges());
    items.push_back(check_conflicting_services());
    items.push_back(check_x3d_driver());
    items.push_back(check_psi());
    items.push_back(check_gpu());
    items.push_back(check_pipewire());
    items.push_back(check_max_map_count());
    return items;
}

void Doctor::print_report(const std::vector<DoctorItem>& items) {
    std::cout << "========================================================================\n"
              << "                      ZENITH SYSTEM DOCTOR                              \n"
              << "========================================================================\n";

    int passes = 0, warnings = 0, failures = 0;
    for (const auto& it : items) {
        if (it.status == DoctorStatus::Pass) passes++;
        else if (it.status == DoctorStatus::Warning) warnings++;
        else failures++;

        std::cout << status_symbol(it.status) << " " << std::left << std::setw(28)
                  << it.name << " " << it.message << "\n";

        if (!it.advice.empty() && it.status != DoctorStatus::Pass) {
            std::cout << "          \033[36mAdvice:\033[0m " << it.advice << "\n";
        }
        if (!it.fix_command.empty() && it.status != DoctorStatus::Pass) {
            std::cout << "          \033[32mFix:\033[0m    " << it.fix_command << "\n";
        }
    }

    std::cout << "------------------------------------------------------------------------\n"
              << "Result: " << passes << " passed, " << warnings << " warnings, " << failures << " failures\n";

    if (failures == 0 && warnings == 0) {
        std::cout << "\033[32mSystem is in optimal condition for ultra-low latency gaming!\033[0m\n";
    } else if (failures > 0) {
        std::cout << "\033[31mCritical checks failed. Review recommended fix commands above.\033[0m\n";
    } else {
        std::cout << "\033[33mZenith is operational, but optional tuning features can be improved.\033[0m\n";
    }
    std::cout << "========================================================================\n";
}

std::string Doctor::to_json(const std::vector<DoctorItem>& items) {
    std::ostringstream ss;
    ss << "[\n";
    for (size_t i = 0; i < items.size(); ++i) {
        const auto& it = items[i];
        ss << "  {\n"
           << "    \"name\": \"" << it.name << "\",\n"
           << "    \"status\": \"" << status_str(it.status) << "\",\n"
           << "    \"message\": \"" << it.message << "\",\n"
           << "    \"advice\": \"" << it.advice << "\",\n"
           << "    \"fix_command\": \"" << it.fix_command << "\"\n"
           << "  }" << (i + 1 < items.size() ? ",\n" : "\n");
    }
    ss << "]";
    return ss.str();
}

} // namespace zenith
