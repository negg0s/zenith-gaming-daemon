#include "zenith/process_tuner.hpp"

#ifdef __linux__
#include <sched.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace zenith {

bool ProcessTuner::set_cpu_affinity(int pid, const std::vector<int>& core_ids) {
    if (core_ids.empty() || pid <= 0) return false;

#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int core : core_ids) {
        CPU_SET(core, &cpuset);
    }
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpuset) == 0) {
        ZENITH_LOG_DEBUG("Set CPU affinity for PID " + std::to_string(pid));
        return true;
    }
    return false;
#else
    (void)pid;
    (void)core_ids;
    return true;
#endif
}

bool ProcessTuner::set_realtime_priority(int pid, int priority) {
    if (pid <= 0) return false;

#ifdef __linux__
    struct sched_param param;
    param.sched_priority = priority;
    // Attempt SCHED_RR (round-robin real-time) or fallback to SCHED_FIFO
    if (sched_setscheduler(pid, SCHED_RR, &param) == 0) {
        ZENITH_LOG_DEBUG("Set SCHED_RR priority for PID " + std::to_string(pid));
        return true;
    }
    if (sched_setscheduler(pid, SCHED_FIFO, &param) == 0) {
        ZENITH_LOG_DEBUG("Set SCHED_FIFO priority for PID " + std::to_string(pid));
        return true;
    }
    return false;
#else
    (void)pid;
    (void)priority;
    return true;
#endif
}

bool ProcessTuner::set_nice(int pid, int nice_value) {
    if (pid <= 0) return false;
#ifdef __linux__
    if (setpriority(PRIO_PROCESS, pid, nice_value) == 0) {
        return true;
    }
    return false;
#else
    (void)pid;
    (void)nice_value;
    return true;
#endif
}

bool ProcessTuner::set_realtime_ioprio(int pid) {
    if (pid <= 0) return false;
#ifdef __linux__
    // IOPRIO_CLASS_RT = 1 << 13
    int ioprio = (1 << 13) | 4;
#ifdef SYS_ioprio_set
    if (syscall(SYS_ioprio_set, 1 /* IOPRIO_WHO_PROCESS */, pid, ioprio) == 0) {
        return true;
    }
#endif
#endif
    return false;
}

std::vector<int> ProcessTuner::get_child_threads(int pid) {
    std::vector<int> threads;
    fs::path task_dir = "/proc/" + std::to_string(pid) + "/task";
    if (fs::exists(task_dir)) {
        for (const auto& entry : fs::directory_iterator(task_dir)) {
            try {
                threads.push_back(std::stoi(entry.path().filename().string()));
            } catch (...) {}
        }
    }
    return threads;
}

bool ProcessTuner::tune_entire_process_tree(int pid, const std::vector<int>& core_ids, bool realtime) {
    bool ok = set_cpu_affinity(pid, core_ids);
    set_nice(pid, -20);
    set_realtime_ioprio(pid);

    if (realtime) {
        set_realtime_priority(pid, 50);
    }

    // Tune all thread children (Wine audio, render, worker threads)
    auto threads = get_child_threads(pid);
    for (int tid : threads) {
        set_cpu_affinity(tid, core_ids);
        if (realtime) set_realtime_priority(tid, 45);
    }

    ZENITH_LOG_INFO("Applied CPU affinity and priority tuning to PID " + std::to_string(pid) + " (" +
                    std::to_string(threads.size()) + " threads).");
    return ok;
}

} // namespace zenith
