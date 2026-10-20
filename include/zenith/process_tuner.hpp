// SPDX-License-Identifier: MIT
#pragma once

#include "zenith/common.hpp"
#include "zenith/proc_fs.hpp"
#include <string>
#include <vector>
#include <string_view>

namespace zenith {

enum class ThreadRole {
    Main,
    Render,
    Audio,
    ShaderCompile,
    IoWorker,
    AnticheatOrOverlay,
    Unknown
};

struct ThreadInfo {
    int tid = 0;
    std::string comm;
    ThreadRole role = ThreadRole::Unknown;
};

class ProcessTuner {
public:
    [[nodiscard]] static ThreadRole classify_thread(int tid, int pid, std::string_view comm);
    [[nodiscard]] static std::vector<ThreadInfo> inspect_threads(int pid, IProcFs* fs = nullptr);

    static bool set_cpu_affinity(int tid, const std::vector<int>& core_ids);
    static bool set_realtime_priority(int tid, int priority = 50);
    static bool set_realtime_ioprio(int tid);
    static bool set_nice(int tid, int nice_value = -20);
    static bool set_uclamp_min(int tid, unsigned int uclamp_min = 512);

    // Fine-grained thread-level tuning:
    // - Main & Render: pinned to perf_cores, uclamp.min=512, nice -10
    // - Audio: nice -11, low latency
    // - ShaderCompile: allowed on all_cores, nice 0
    // - IoWorker: nice 0
    // - AnticheatOrOverlay: STRICTLY UNTOUCHED (Zero elevation / modification)
    static bool tune_process_fine_grained(
        int pid,
        const std::vector<int>& perf_cores,
        const std::vector<int>& all_cores,
        IProcFs* fs = nullptr
    );

    // Tree tuning (backward-compatible)
    static bool tune_entire_process_tree(int pid, const std::vector<int>& core_ids, bool realtime);
    [[nodiscard]] static std::vector<int> get_child_threads(int pid, IProcFs* fs = nullptr);
};

} // namespace zenith
