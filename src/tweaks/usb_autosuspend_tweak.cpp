#include "zenith/tweaks/usb_autosuspend_tweak.hpp"

namespace zenith {

UsbAutosuspendTweak::UsbAutosuspendTweak(fs::path usb_devices_root)
    : usb_root_(std::move(usb_devices_root)) {}

bool UsbAutosuspendTweak::probe() {
    target_power_controls_.clear();
    std::error_code ec;
    if (!fs::exists(usb_root_, ec) || !fs::is_directory(usb_root_, ec)) {
        return false;
    }

    for (const auto& entry : fs::directory_iterator(usb_root_, ec)) {
        fs::path pwr = entry.path() / "power" / "control";
        if (fs::exists(pwr, ec)) {
            target_power_controls_.push_back(pwr);
        }
    }

    return !target_power_controls_.empty();
}

bool UsbAutosuspendTweak::apply(Profile profile) {
    if (profile == Profile::Safe) return true;
    if (!probe()) return false;

    bool any_applied = false;
    for (const auto& pwr : target_power_controls_) {
        auto cur = read_file_string(pwr);
        if (cur && saved_controls_.find(pwr.string()) == saved_controls_.end()) {
            saved_controls_[pwr.string()] = *cur;
        }
        // "on" disables autosuspend (keeps device powered)
        if (write_file_string(pwr, "on")) {
            any_applied = true;
        }
    }

    return any_applied;
}

bool UsbAutosuspendTweak::revert() {
    bool all_ok = true;
    for (const auto& [path, original] : saved_controls_) {
        if (!write_file_string(path, original)) all_ok = false;
    }
    saved_controls_.clear();
    return all_ok;
}

std::string UsbAutosuspendTweak::describe() const {
    return "USB power autosuspend inhibitor for " + std::to_string(target_power_controls_.size()) + " USB devices";
}

std::string UsbAutosuspendTweak::explain() const {
    return "Sets power/control to 'on' for USB gaming devices, preventing the kernel from putting mice/keyboards into power-saving suspend. "
           "Eliminates initial mouse-wake latency and sensor jitter after brief pauses. "
           "Risks: Negligible idle battery consumption (a few milliwatts).";
}

std::string UsbAutosuspendTweak::get_current_value() const {
    if (target_power_controls_.empty()) return "no_usb_devices_found";
    std::string sample = "unknown";
    if (auto cur = read_file_string(target_power_controls_.front())) {
        sample = *cur;
    }
    return "control=" + sample;
}

std::string UsbAutosuspendTweak::get_target_value(Profile profile) const {
    if (profile == Profile::Safe) return "control=(untouched)";
    return "control=on";
}

} // namespace zenith
