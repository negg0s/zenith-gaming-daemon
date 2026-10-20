// SPDX-License-Identifier: MIT
#pragma once

#include "zenith/tweaks/tweak_interface.hpp"
#include <unordered_map>

namespace zenith {

class ZramSwapTweak : public ITweak {
public:
    explicit ZramSwapTweak(fs::path proc_sys_vm = "/proc/sys/vm",
                           fs::path zram_probe_dir = "/sys/block");

    std::string name() const override { return "zram_and_swap_optimizer"; }
    std::string description() const override { return "zram zero-cluster paging, watermark boost suppression, and MGLRU"; }
    uint32_t supported_profiles() const override { return ProfileMask::All; }

    bool probe() override;
    bool apply(Profile profile) override;
    bool revert() override;
    std::string describe() const override;
    std::string explain() const override;

    std::string get_current_value() const override;
    std::string get_target_value(Profile profile) const override;

    bool is_zram_detected() const { return zram_detected_; }

private:
    fs::path vm_dir_;
    fs::path zram_probe_dir_;
    bool zram_detected_ = false;
    std::unordered_map<std::string, std::string> saved_vm_values_;
};

} // namespace zenith
