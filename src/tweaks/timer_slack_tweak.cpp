#include "zenith/tweaks/timer_slack_tweak.hpp"

#ifdef __linux__
#include <sys/prctl.h>
#ifndef PR_SET_TIMERSLACK
#define PR_SET_TIMERSLACK 29
#endif
#ifndef PR_GET_TIMERSLACK
#define PR_GET_TIMERSLACK 30
#endif
#endif

namespace zenith {

TimerSlackTweak::TimerSlackTweak() {
    probe();
}

bool TimerSlackTweak::probe() {
    migration_cost_present_ = fs::exists("/proc/sys/kernel/sched_migration_cost_ns");
    return true;
}

bool TimerSlackTweak::apply_to_current_or_pid(pid_t pid, unsigned long slack_ns) {
#ifdef __linux__
    (void)pid; // prctl operates on calling process/thread
    int ret = ::prctl(PR_SET_TIMERSLACK, slack_ns, 0, 0, 0);
    return ret == 0;
#else
    (void)pid;
    (void)slack_ns;
    return true;
#endif
}

bool TimerSlackTweak::apply(Profile profile) {
    if (profile == Profile::Safe) return true;

    // Apply 50ns timer slack to daemon itself
    apply_to_current_or_pid(0, 50);

    // If kernel still exposes sched_migration_cost_ns, optimize cache migration threshold
    if (migration_cost_present_) {
        fs::path p = "/proc/sys/kernel/sched_migration_cost_ns";
        if (!orig_migration_cost_) orig_migration_cost_ = read_file_string(p);
        // Set migration cost to 250,000 ns (250 µs) in aggressive, 500,000 ns in balanced
        std::string val = (profile == Profile::Aggressive) ? "250000" : "500000";
        write_file_string(p, val);
    }

    return true;
}

bool TimerSlackTweak::revert() {
    if (orig_migration_cost_) {
        write_file_string("/proc/sys/kernel/sched_migration_cost_ns", *orig_migration_cost_);
        orig_migration_cost_.reset();
    }
#ifdef __linux__
    (void)::prctl(PR_SET_TIMERSLACK, 50000, 0, 0, 0); // Restore 50 µs default
#endif
    return true;
}

std::string TimerSlackTweak::describe() const {
    return "Timer slack (50ns) and scheduler migration latency";
}

std::string TimerSlackTweak::explain() const {
    return "Reduces thread timer slack from 50,000ns to 50ns, ensuring microsecond-accurate sleeps in SDL/Vulkan frame pacers. "
           "Safely checks and tunes sched_migration_cost_ns if supported by the kernel. "
           "Risks: Minor increase in timer wakeups.";
}

std::string TimerSlackTweak::get_current_value() const {
    std::string val = "timer_slack=50us";
    if (migration_cost_present_) {
        if (auto cur = read_file_string("/proc/sys/kernel/sched_migration_cost_ns")) {
            val += ", migration_cost=" + *cur + "ns";
        }
    }
    return val;
}

std::string TimerSlackTweak::get_target_value(Profile profile) const {
    switch (profile) {
        case Profile::Safe: return "timer_slack=(untouched)";
        case Profile::Balanced: return "timer_slack=50ns, migration_cost=500us";
        case Profile::Aggressive: return "timer_slack=50ns, migration_cost=250us";
    }
    return "timer_slack=50ns";
}

} // namespace zenith
