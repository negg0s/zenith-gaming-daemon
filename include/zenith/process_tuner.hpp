#pragma once

#include "zenith/common.hpp"

namespace zenith {

class ProcessTuner {
public:
    static bool set_cpu_affinity(int pid, const std::vector<int>& core_ids);
    static bool set_realtime_priority(int pid, int priority = 50);
    static bool set_realtime_ioprio(int pid);
    static bool set_nice(int pid, int nice_value = -20);
    static std::vector<int> get_child_threads(int pid);
    static bool tune_entire_process_tree(int pid, const std::vector<int>& core_ids, bool realtime);
};

} // namespace zenith
