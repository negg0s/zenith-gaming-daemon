// SPDX-License-Identifier: MIT
#pragma once

#include "zenith/tweaks/tweak_interface.hpp"
#include <unordered_map>

namespace zenith {

class UsbAutosuspendTweak : public ITweak {
public:
    explicit UsbAutosuspendTweak(fs::path usb_devices_root = "/sys/bus/usb/devices");

    std::string name() const override { return "usb_autosuspend_inhibitor"; }
    std::string description() const override { return "Disables USB power autosuspend for gaming input devices"; }
    uint32_t supported_profiles() const override { return ProfileMask::Performance; }

    bool probe() override;
    bool apply(Profile profile) override;
    bool revert() override;
    std::string describe() const override;
    std::string explain() const override;

    std::string get_current_value() const override;
    std::string get_target_value(Profile profile) const override;

private:
    fs::path usb_root_;
    std::vector<fs::path> target_power_controls_;
    std::unordered_map<std::string, std::string> saved_controls_;
};

} // namespace zenith
