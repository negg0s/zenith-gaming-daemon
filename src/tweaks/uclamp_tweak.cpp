// SPDX-License-Identifier: MIT
#include "zenith/tweaks/uclamp_tweak.hpp"

#ifdef __linux__
#include <unistd.h>
#include <sys/syscall.h>
#include <sched.h>

#ifndef SYS_sched_setattr
#if defined(__x86_64__)
#define SYS_sched_setattr 314
#elif defined(__aarch64__)
#define SYS_sched_setattr 274
#endif
#endif

#ifndef SCHED_FLAG_UTIL_CLAMP_MIN
#define SCHED_FLAG_UTIL_CLAMP_MIN 0x20
#endif
#ifndef SCHED_FLAG_KEEP_ALL
#define SCHED_FLAG_KEEP_ALL 0x18
#endif

struct zenith_sched_attr {
    uint32_t size;
    uint32_t sched_policy;
    uint64_t sched_flags;
    int32_t sched_nice;
    uint32_t sched_priority;
    uint64_t sched_runtime;
    uint64_t sched_deadline;
    uint64_t sched_period;
    uint32_t sched_util_min;
    uint32_t sched_util_max;
};
#endif

namespace zenith {

UclampTweak::UclampTweak() {
    probe();
}

bool UclampTweak::probe() {
#ifdef __linux__
#ifdef SYS_sched_setattr
    zenith_sched_attr attr{};
    attr.size = sizeof(attr);
    attr.sched_flags = SCHED_FLAG_KEEP_ALL;
    // Probe on calling thread (pid 0)
    int ret = static_cast<int>(::syscall(SYS_sched_setattr, 0, &attr, 0));
    uclamp_supported_ = (ret == 0);
    return uclamp_supported_;
#else
    uclamp_supported_ = false;
    return false;
#endif
#else
    uclamp_supported_ = true;
    return true;
#endif
}

bool UclampTweak::apply_to_pid(pid_t pid, Profile profile) {
    if (!uclamp_supported_) return false;

    uint32_t util_min = (profile == Profile::Aggressive) ? 1024 : 512;

#ifdef __linux__
#ifdef SYS_sched_setattr
    zenith_sched_attr attr{};
    attr.size = sizeof(attr);
    attr.sched_flags = SCHED_FLAG_KEEP_ALL | SCHED_FLAG_UTIL_CLAMP_MIN;
    attr.sched_util_min = util_min;

    int ret = static_cast<int>(::syscall(SYS_sched_setattr, pid, &attr, 0));
    if (ret == 0) {
        tuned_pids_.push_back(pid);
        return true;
    }
    return false;
#else
    (void)pid;
    (void)util_min;
    return false;
#endif
#else
    (void)pid;
    tuned_pids_.push_back(pid);
    return true;
#endif
}

bool UclampTweak::apply_background_policy(pid_t pid, bool idle) {
#ifdef __linux__
    int policy = idle ? SCHED_IDLE : SCHED_BATCH;
    struct sched_param param{};
    param.sched_priority = 0;
    return ::sched_setscheduler(pid, policy, &param) == 0;
#else
    (void)pid;
    (void)idle;
    return true;
#endif
}

bool UclampTweak::apply(Profile profile) {
    (void)profile;
    return uclamp_supported_;
}

bool UclampTweak::revert() {
#ifdef __linux__
#ifdef SYS_sched_setattr
    for (pid_t pid : tuned_pids_) {
        zenith_sched_attr attr{};
        attr.size = sizeof(attr);
        attr.sched_flags = SCHED_FLAG_KEEP_ALL | SCHED_FLAG_UTIL_CLAMP_MIN;
        attr.sched_util_min = 0;
        (void)::syscall(SYS_sched_setattr, pid, &attr, 0);
    }
#endif
#endif
    tuned_pids_.clear();
    return true;
}

std::string UclampTweak::describe() const {
    return "uclamp.min floor compute capacity for active game threads";
}

std::string UclampTweak::explain() const {
    return "Sets scheduler utilization clamping (uclamp.min) to 50% (balanced) or 100% (aggressive) for game render threads. "
           "Informs the CPU frequency governor to keep frequency high during active frame production without locking clocks permanently. "
           "Risks: Higher power consumption while the game process is running.";
}

std::string UclampTweak::get_current_value() const {
    return uclamp_supported_ ? "uclamp_supported=true" : "uclamp_supported=false";
}

std::string UclampTweak::get_target_value(Profile profile) const {
    switch (profile) {
        case Profile::Safe: return "uclamp.min=(untouched)";
        case Profile::Balanced: return "uclamp.min=512 (50%)";
        case Profile::Aggressive: return "uclamp.min=1024 (100%)";
    }
    return "uclamp.min=512";
}

} // namespace zenith
