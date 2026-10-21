// SPDX-License-Identifier: MIT
#include "zenith/process_tuner.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>

#ifdef __linux__
#include <sched.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <linux/sched/types.h>

#ifndef SCHED_FLAG_UTIL_CLAMP_MIN
#define SCHED_FLAG_UTIL_CLAMP_MIN 0x20
#endif
#endif

namespace zenith {

namespace {

std::string to_lower_str(std::string_view sv) {
    std::string s(sv);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

} // namespace

ThreadRole ProcessTuner::classify_thread(int tid, int pid, std::string_view comm) {
    std::string lcomm = to_lower_str(comm);

    // 1. Anticheat & Overlay threads - Must be recognized first and strictly protected
    if (lcomm.find("eac") != std::string::npos ||
        lcomm.find("easyanticheat") != std::string::npos ||
        lcomm.find("battleye") != std::string::npos ||
        lcomm.find("beservice") != std::string::npos ||
        lcomm.find("vgk") != std::string::npos ||
        lcomm.find("discord") != std::string::npos ||
        lcomm.find("obs") != std::string::npos ||
        lcomm.find("geforce") != std::string::npos) {
        return ThreadRole::AnticheatOrOverlay;
    }

    // 2. Main Thread
    if (tid == pid) {
        return ThreadRole::Main;
    }

    // 3. Audio Threads
    if (lcomm.find("audio") != std::string::npos ||
        lcomm.find("sound") != std::string::npos ||
        lcomm.find("mixer") != std::string::npos ||
        lcomm.find("pipewire") != std::string::npos ||
        lcomm.find("pulse") != std::string::npos ||
        lcomm.find("fmod") != std::string::npos ||
        lcomm.find("wwise") != std::string::npos) {
        return ThreadRole::Audio;
    }

    // 4. Shader Compilation Threads
    if (lcomm.find("shader") != std::string::npos ||
        lcomm.find("compile") != std::string::npos ||
        lcomm.find("llvm") != std::string::npos ||
        lcomm.find("vk_worker") != std::string::npos) {
        return ThreadRole::ShaderCompile;
    }

    // 5. Render Threads
    if (lcomm.find("render") != std::string::npos ||
        lcomm.find("direct3d") != std::string::npos ||
        lcomm.find("d3d") != std::string::npos ||
        lcomm.find("vk") != std::string::npos ||
        lcomm.find("vulkan") != std::string::npos ||
        lcomm.find("rhi") != std::string::npos ||
        lcomm.find("gpu") != std::string::npos ||
        lcomm.find("gl") != std::string::npos ||
        lcomm.find("draw") != std::string::npos) {
        return ThreadRole::Render;
    }

    // 6. I/O & Worker Pool Threads
    if (lcomm.find("io") != std::string::npos ||
        lcomm.find("async") != std::string::npos ||
        lcomm.find("disk") != std::string::npos ||
        lcomm.find("file") != std::string::npos ||
        lcomm.find("worker") != std::string::npos ||
        lcomm.find("pool") != std::string::npos ||
        lcomm.find("job") != std::string::npos) {
        return ThreadRole::IoWorker;
    }

    return ThreadRole::Unknown;
}

std::vector<ThreadInfo> ProcessTuner::inspect_threads(int pid, IProcFs* fs) {
    std::vector<ThreadInfo> result;
    IProcFs* effective_fs = fs ? fs : &RealProcFs::instance();
    auto tasks = effective_fs->list_tasks(pid);

    for (int tid : tasks) {
        ThreadInfo info;
        info.tid = tid;
        auto comm = effective_fs->read_file("/proc/" + std::to_string(pid) + "/task/" + std::to_string(tid) + "/comm");
        info.comm = comm ? trim(*comm) : "";
        info.role = classify_thread(tid, pid, info.comm);
        result.push_back(info);
    }

    return result;
}

bool ProcessTuner::set_cpu_affinity(int tid, const std::vector<int>& core_ids) {
    if (core_ids.empty() || tid <= 0) return false;

#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int core : core_ids) {
        CPU_SET(core, &cpuset);
    }
    if (sched_setaffinity(tid, sizeof(cpu_set_t), &cpuset) == 0) {
        return true;
    }
    return false;
#else
    (void)tid;
    (void)core_ids;
    return true;
#endif
}

bool ProcessTuner::set_realtime_priority(int tid, int priority) {
    if (tid <= 0) return false;

#ifdef __linux__
    struct sched_param param;
    param.sched_priority = priority;
    if (sched_setscheduler(tid, SCHED_RR, &param) == 0) {
        return true;
    }
    if (sched_setscheduler(tid, SCHED_FIFO, &param) == 0) {
        return true;
    }
    return false;
#else
    (void)tid;
    (void)priority;
    return true;
#endif
}

bool ProcessTuner::set_nice(int tid, int nice_value) {
    if (tid <= 0) return false;
#ifdef __linux__
    if (setpriority(PRIO_PROCESS, tid, nice_value) == 0) {
        return true;
    }
    return false;
#else
    (void)tid;
    (void)nice_value;
    return true;
#endif
}

bool ProcessTuner::set_realtime_ioprio(int tid) {
    if (tid <= 0) return false;
#ifdef __linux__
    int ioprio = (1 << 13) | 4;
#ifdef SYS_ioprio_set
    if (syscall(SYS_ioprio_set, 1 /* IOPRIO_WHO_PROCESS */, tid, ioprio) == 0) {
        return true;
    }
#endif
#endif
    return false;
}

bool ProcessTuner::set_uclamp_min(int tid, unsigned int uclamp_min) {
    if (tid <= 0) return false;
#ifdef __linux__
#ifdef SYS_sched_setattr
    struct {
        uint32_t size;
        uint32_t sched_mode;
        uint64_t sched_flags;
        int32_t  sched_nice;
        uint32_t sched_priority;
        uint64_t sched_runtime;
        uint64_t sched_deadline;
        uint64_t sched_period;
        uint32_t sched_util_min;
        uint32_t sched_util_max;
    } attr;

    std::memset(&attr, 0, sizeof(attr));
    attr.size = sizeof(attr);
    attr.sched_flags = SCHED_FLAG_UTIL_CLAMP_MIN;
    attr.sched_util_min = (uclamp_min > 1024) ? 1024 : uclamp_min;

    if (syscall(SYS_sched_setattr, tid, &attr, 0) == 0) {
        return true;
    }
#endif
#endif
    (void)tid;
    (void)uclamp_min;
    return true;
}

bool ProcessTuner::tune_process_fine_grained(
    int pid,
    const std::vector<int>& perf_cores,
    const std::vector<int>& all_cores,
    IProcFs* fs
) {
    if (pid <= 0) return false;

    auto threads = inspect_threads(pid, fs);
    if (threads.empty()) {
        // Fallback to tuning pid directly if thread list couldn't be retrieved
        if (!perf_cores.empty()) {
            set_cpu_affinity(pid, perf_cores);
        }
        set_nice(pid, -10);
        set_uclamp_min(pid, 512);
        return true;
    }

    for (const auto& thread : threads) {
        // Anti-Cheat & Third-party overlays: STRICTLY UNTOUCHED
        if (thread.role == ThreadRole::AnticheatOrOverlay) {
            ZENITH_LOG_DEBUG("Skipping anticheat/overlay TID " + std::to_string(thread.tid) + " (" + thread.comm + ")");
            continue;
        }

        switch (thread.role) {
            case ThreadRole::Main:
            case ThreadRole::Render:
                if (!perf_cores.empty()) {
                    set_cpu_affinity(thread.tid, perf_cores);
                }
                set_nice(thread.tid, -10);
                set_uclamp_min(thread.tid, 1024);
                set_realtime_ioprio(thread.tid);
                break;

            case ThreadRole::Audio:
                if (!perf_cores.empty()) {
                    set_cpu_affinity(thread.tid, perf_cores);
                }
                set_nice(thread.tid, -11);
                set_realtime_ioprio(thread.tid);
                break;

            case ThreadRole::ShaderCompile:
                // Allow shader compilers to leverage all cores at normal nice level
                if (!all_cores.empty()) {
                    set_cpu_affinity(thread.tid, all_cores);
                }
                set_nice(thread.tid, 0);
                break;

            case ThreadRole::IoWorker:
                if (!perf_cores.empty()) {
                    set_cpu_affinity(thread.tid, perf_cores);
                }
                set_nice(thread.tid, -5);
                set_realtime_ioprio(thread.tid);
                break;

            case ThreadRole::Unknown:
            default:
                if (!perf_cores.empty()) {
                    set_cpu_affinity(thread.tid, perf_cores);
                }
                set_nice(thread.tid, 0);
                break;
        }
    }

    return true;
}

bool ProcessTuner::tune_entire_process_tree(int pid, const std::vector<int>& core_ids, bool realtime) {
    bool ok = set_cpu_affinity(pid, core_ids);
    set_nice(pid, -20);
    set_realtime_ioprio(pid);
    if (realtime) {
        set_realtime_priority(pid, 50);
    }
    auto threads = get_child_threads(pid);
    for (int tid : threads) {
        set_cpu_affinity(tid, core_ids);
        set_nice(tid, -20);
        if (realtime) {
            set_realtime_priority(tid, 50);
        }
    }
    return ok;
}

std::vector<int> ProcessTuner::get_child_threads(int pid, IProcFs* fs) {
    IProcFs* effective_fs = fs ? fs : &RealProcFs::instance();
    return effective_fs->list_tasks(pid);
}

} // namespace zenith
