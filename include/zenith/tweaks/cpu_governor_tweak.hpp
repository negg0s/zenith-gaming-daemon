// SPDX-License-Identifier: MIT
#pragma once

#include "zenith/tweaks/tweak_interface.hpp"
#include <unordered_map>

namespace zenith {

class CpuGovernorTweak : public ITweak {
public:
    explicit CpuGovernorTweak(fs::path sysfs_root = "/sys/devices/system/cpu");

    std::string name() const override { return "cpu_governor_epp"; }
    std::string description() const override { return "CPU Frequency Governor and Energy Performance Preference (EPP)"; }
    uint32_t supported_profiles() const override { return ProfileMask::All; }

    bool probe() override;
    bool apply(Profile profile) override;
    bool revert() override;
    std::string describe() const override;
    std::string explain() const override;

    std::string get_current_value() const override;
    std::string get_target_value(Profile profile) const override;

private:
    fs::path sysfs_root_;
    bool available_ = false;
    std::unordered_map<std::string, std::string> saved_governors_;
    std::unordered_map<std::string, std::string> saved_epp_;
    std::vector<fs::path> cpu_dirs_;
};

} // namespace zenith
