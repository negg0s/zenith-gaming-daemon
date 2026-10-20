#pragma once

#include "zenith/tweaks/tweak_interface.hpp"

namespace zenith {

class TimerSlackTweak : public ITweak {
public:
    TimerSlackTweak();

    std::string name() const override { return "timer_slack_and_sched_latency"; }
    std::string description() const override { return "Timer slack (50ns) and scheduler latency knobs"; }
    uint32_t supported_profiles() const override { return ProfileMask::Performance; }

    bool probe() override;
    bool apply(Profile profile) override;
    bool revert() override;
    std::string describe() const override;
    std::string explain() const override;

    std::string get_current_value() const override;
    std::string get_target_value(Profile profile) const override;

    bool apply_to_current_or_pid(pid_t pid, unsigned long slack_ns = 50);

private:
    bool migration_cost_present_ = false;
    std::optional<std::string> orig_migration_cost_;
};

} // namespace zenith
