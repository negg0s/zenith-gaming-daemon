#pragma once

#include "zenith/tweaks/tweak_interface.hpp"
#include "zenith/pressure_monitor.hpp"
#include <string>
#include <vector>
#include <functional>
#include <chrono>

namespace zenith {

struct AdaptiveTransition {
    Profile from_profile = Profile::Balanced;
    Profile to_profile = Profile::Balanced;
    std::string reason;
    SystemPressure pressure_snapshot;
    std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

using ProfileChangeCallback = std::function<void(Profile, Profile, const std::string&)>;

class AdaptiveController {
public:
    explicit AdaptiveController(Profile base_target_profile = Profile::Balanced);

    void set_profile_change_callback(ProfileChangeCallback callback) {
        callback_ = std::move(callback);
    }

    // Process a pressure sample and evaluate state machine
    bool evaluate(const SystemPressure& sp);

    // Profile target control
    void set_base_profile(Profile p);
    [[nodiscard]] Profile base_profile() const { return base_target_profile_; }
    [[nodiscard]] Profile active_profile() const { return active_profile_; }

    // Platform clamps
    void set_battery_mode(bool on_battery);
    void set_handheld_mode(bool is_handheld);
    [[nodiscard]] bool is_battery_mode() const { return on_battery_; }
    [[nodiscard]] bool is_handheld_mode() const { return is_handheld_; }

    // Tunable thresholds & hysteresis parameters
    void set_memory_threshold(double avg10) { max_mem_some_avg10_ = avg10; }
    void set_io_threshold(double avg10) { max_io_some_avg10_ = avg10; }
    void set_thermal_threshold_c(uint32_t c) { max_temp_c_ = c; }
    void set_hysteresis_cycles(uint32_t cycles) { recovery_hysteresis_cycles_ = cycles; }
    void set_cooldown_seconds(uint32_t seconds) { cooldown_seconds_ = seconds; }

    [[nodiscard]] const std::vector<AdaptiveTransition>& history() const { return transitions_; }
    [[nodiscard]] uint32_t healthy_cycles_count() const { return healthy_cycles_count_; }

private:
    Profile base_target_profile_ = Profile::Balanced;
    Profile active_profile_ = Profile::Balanced;

    bool on_battery_ = false;
    bool is_handheld_ = false;

    // Thresholds
    double max_mem_some_avg10_ = 25.0;
    double max_io_some_avg10_ = 30.0;
    uint32_t max_temp_c_ = 85;

    // Hysteresis & anti-oscillation
    uint32_t recovery_hysteresis_cycles_ = 5;
    uint32_t cooldown_seconds_ = 10;
    uint32_t healthy_cycles_count_ = 0;
    std::chrono::steady_clock::time_point last_transition_time_ = std::chrono::steady_clock::now();

    std::vector<AdaptiveTransition> transitions_;
    ProfileChangeCallback callback_;

    void transition_to(Profile new_profile, const std::string& reason, const SystemPressure& sp);
    [[nodiscard]] bool is_severe_pressure(const SystemPressure& sp) const;
    [[nodiscard]] bool is_moderate_pressure(const SystemPressure& sp) const;
    [[nodiscard]] bool is_healthy(const SystemPressure& sp) const;
    [[nodiscard]] Profile get_platform_clamped_profile(Profile requested) const;
};

} // namespace zenith
