#pragma once

#include "zenith/tweaks/tweak_interface.hpp"

namespace zenith {

class UclampTweak : public ITweak {
public:
    UclampTweak();

    std::string name() const override { return "sched_uclamp_min"; }
    std::string description() const override { return "Scheduler utilization clamping (uclamp.min) for game threads"; }
    uint32_t supported_profiles() const override { return ProfileMask::Performance; }

    bool probe() override;
    bool apply(Profile profile) override;
    bool revert() override;
    std::string describe() const override;
    std::string explain() const override;

    std::string get_current_value() const override;
    std::string get_target_value(Profile profile) const override;

    // Direct apply to a specific game process PID
    bool apply_to_pid(pid_t pid, Profile profile);
    bool apply_background_policy(pid_t pid, bool idle);

private:
    bool uclamp_supported_ = false;
    std::vector<pid_t> tuned_pids_;
};

} // namespace zenith
