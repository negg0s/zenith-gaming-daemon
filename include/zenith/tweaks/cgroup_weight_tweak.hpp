// SPDX-License-Identifier: MIT
#pragma once

#include "zenith/tweaks/tweak_interface.hpp"

namespace zenith {

class CgroupWeightTweak : public ITweak {
public:
    explicit CgroupWeightTweak(fs::path cgroup_root = "/sys/fs/cgroup");

    std::string name() const override { return "cgroup_soft_prioritization"; }
    std::string description() const override { return "cgroups v2 soft resource weighting (cpu.weight, cpu.idle, io.weight)"; }
    uint32_t supported_profiles() const override { return ProfileMask::All; }

    bool probe() override;
    bool apply(Profile profile) override;
    bool revert() override;
    std::string describe() const override;
    std::string explain() const override;

    std::string get_current_value() const override;
    std::string get_target_value(Profile profile) const override;

    const fs::path& get_background_slice() const { return bg_slice_; }

private:
    fs::path cgroup_root_;
    fs::path bg_slice_;
    bool available_ = false;

    std::optional<std::string> orig_cpu_weight_;
    std::optional<std::string> orig_cpu_idle_;
    std::optional<std::string> orig_io_weight_;
};

} // namespace zenith
