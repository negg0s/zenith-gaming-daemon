// SPDX-License-Identifier: MIT
#pragma once

#include "zenith/common.hpp"
#include <string>
#include <vector>

namespace zenith {

struct DetectedConflict {
    std::string service_name;
    std::string description;
    std::string impact;
    std::string recommended_action;
};

class ConflictDetector {
public:
    static std::vector<DetectedConflict> probe_system_conflicts();
    static void print_conflict_report(const std::vector<DetectedConflict>& conflicts);

    // Checks running process names in /proc
    static bool is_process_running(std::string_view name);

    // Checks if sched-ext (scx) is actively running an eBPF scheduler
    static bool is_sched_ext_active();
};

} // namespace zenith
