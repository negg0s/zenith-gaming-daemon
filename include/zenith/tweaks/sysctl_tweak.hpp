// SPDX-License-Identifier: MIT
#pragma once

#include "zenith/tweaks/tweak_interface.hpp"
#include <unordered_map>

namespace zenith {

class SysctlTweak : public ITweak {
public:
    explicit SysctlTweak(fs::path proc_root = "/proc", fs::path sys_root = "/sys");

    std::string name() const override { return "kernel_memory_and_split_lock"; }
    std::string description() const override { return "Transparent HugePages, compaction proactiveness, and split-lock mitigation"; }
    uint32_t supported_profiles() const override { return ProfileMask::All; }

    bool probe() override;
    bool apply(Profile profile) override;
    bool revert() override;
    std::string describe() const override;
    std::string explain() const override;

    std::string get_current_value() const override;
    std::string get_target_value(Profile profile) const override;

private:
    fs::path proc_root_;
    fs::path sys_root_;
    std::unordered_map<std::string, std::string> saved_values_;
};

} // namespace zenith
