// SPDX-License-Identifier: MIT
#include "zenith/adaptive_controller.hpp"

namespace zenith {

AdaptiveController::AdaptiveController(Profile base_target_profile)
    : base_target_profile_(base_target_profile),
      active_profile_(base_target_profile) {}

Profile AdaptiveController::get_platform_clamped_profile(Profile requested) const {
    if (on_battery_) {
        // Force at most Balanced on battery power
        if (requested == Profile::Aggressive || requested == Profile::Ultra) {
            return Profile::Balanced;
        }
    }
    if (is_handheld_) {
        // Handheld ceiling: Balanced
        if (requested == Profile::Aggressive || requested == Profile::Ultra) {
            return Profile::Balanced;
        }
    }
    return requested;
}

void AdaptiveController::set_base_profile(Profile p) {
    base_target_profile_ = p;
    Profile clamped = get_platform_clamped_profile(p);
    if (active_profile_ != clamped) {
        SystemPressure empty_sp;
        transition_to(clamped, "Base profile updated", empty_sp);
    }
}

void AdaptiveController::set_battery_mode(bool on_battery) {
    on_battery_ = on_battery;
    if (on_battery_ && (active_profile_ == Profile::Aggressive || active_profile_ == Profile::Ultra)) {
        SystemPressure sp;
        transition_to(Profile::Balanced, "Battery mode engaged: clamped to Balanced", sp);
    }
}

void AdaptiveController::set_handheld_mode(bool is_handheld) {
    is_handheld_ = is_handheld;
    if (is_handheld_ && (active_profile_ == Profile::Aggressive || active_profile_ == Profile::Ultra)) {
        SystemPressure sp;
        transition_to(Profile::Balanced, "Handheld form factor: clamped to Balanced", sp);
    }
}

bool AdaptiveController::is_severe_pressure(const SystemPressure& sp) const {
    if (sp.cpu_max_temp_c >= (max_temp_c_ + 5) || sp.gpu_max_temp_c >= (max_temp_c_ + 5)) return true;
    if (sp.memory_some.avg10 > (max_mem_some_avg10_ * 1.5)) return true;
    if (sp.memory_full.avg10 > 10.0) return true;
    if (sp.io_some.avg10 > (max_io_some_avg10_ * 1.5)) return true;
    if (sp.io_full.avg10 > 15.0) return true;
    return false;
}

bool AdaptiveController::is_moderate_pressure(const SystemPressure& sp) const {
    if (sp.cpu_max_temp_c >= max_temp_c_ || sp.gpu_max_temp_c >= max_temp_c_) return true;
    if (sp.is_thermal_throttling) return true;
    if (sp.memory_some.avg10 > max_mem_some_avg10_) return true;
    if (sp.io_some.avg10 > max_io_some_avg10_) return true;
    return false;
}

bool AdaptiveController::is_healthy(const SystemPressure& sp) const {
    if (sp.cpu_max_temp_c >= (max_temp_c_ - 5) || sp.gpu_max_temp_c >= (max_temp_c_ - 5)) return false;
    if (sp.is_thermal_throttling) return false;
    if (sp.memory_some.avg10 > (max_mem_some_avg10_ * 0.6)) return false;
    if (sp.io_some.avg10 > (max_io_some_avg10_ * 0.6)) return false;
    return true;
}

void AdaptiveController::transition_to(Profile new_profile, const std::string& reason, const SystemPressure& sp) {
    Profile old_p = active_profile_;
    active_profile_ = new_profile;
    last_transition_time_ = std::chrono::steady_clock::now();

    AdaptiveTransition tr;
    tr.from_profile = old_p;
    tr.to_profile = new_profile;
    tr.reason = reason;
    tr.pressure_snapshot = sp;
    tr.timestamp = last_transition_time_;
    transitions_.push_back(tr);

    ZENITH_LOG_WARN("Adaptive Profile Change: [" + profile_to_string(old_p) + "] -> [" +
                    profile_to_string(new_profile) + "] | Reason: " + reason);

    if (callback_) {
        callback_(old_p, new_profile, reason);
    }
}

bool AdaptiveController::evaluate(const SystemPressure& sp) {
    Profile target_clamped = get_platform_clamped_profile(base_target_profile_);

    // 1. Degradation Evaluation (Ultra -> Aggressive -> Balanced -> Safe)
    if (active_profile_ == Profile::Ultra) {
        if (is_moderate_pressure(sp) || is_severe_pressure(sp)) {
            std::string reason = "System pressure or thermal load in Ultra; stepped down to Aggressive";
            transition_to(Profile::Aggressive, reason, sp);
            healthy_cycles_count_ = 0;
            return true;
        }
    } else if (active_profile_ == Profile::Aggressive) {
        if (is_moderate_pressure(sp) || is_severe_pressure(sp)) {
            std::string reason;
            if (sp.cpu_max_temp_c >= max_temp_c_) {
                reason = "CPU temperature (" + std::to_string(sp.cpu_max_temp_c) + "C) exceeded safety limit (" +
                         std::to_string(max_temp_c_) + "C)";
            } else if (sp.gpu_max_temp_c >= max_temp_c_) {
                reason = "GPU temperature (" + std::to_string(sp.gpu_max_temp_c) + "C) exceeded safety limit";
            } else if (sp.is_thermal_throttling) {
                reason = "Hardware thermal throttle count incremented";
            } else if (sp.memory_some.avg10 > max_mem_some_avg10_) {
                reason = "Memory pressure avg10 (" + std::to_string(sp.memory_some.avg10) + "%) exceeded threshold";
            } else if (sp.io_some.avg10 > max_io_some_avg10_) {
                reason = "I/O pressure avg10 (" + std::to_string(sp.io_some.avg10) + "%) exceeded threshold";
            } else {
                reason = "Moderate system pressure detected";
            }

            transition_to(Profile::Balanced, reason, sp);
            healthy_cycles_count_ = 0;
            return true;
        }
    } else if (active_profile_ == Profile::Balanced) {
        if (is_severe_pressure(sp)) {
            std::string reason = "Severe system pressure / thermal spike sustained on Balanced";
            transition_to(Profile::Safe, reason, sp);
            healthy_cycles_count_ = 0;
            return true;
        }
    }

    // 2. Recovery / Promotion Evaluation (Safe -> Balanced -> Aggressive)
    if (active_profile_ != target_clamped) {
        if (is_healthy(sp)) {
            healthy_cycles_count_++;
            auto elapsed_sec = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - last_transition_time_
            ).count();

            if (healthy_cycles_count_ >= recovery_hysteresis_cycles_ &&
                elapsed_sec >= static_cast<int64_t>(cooldown_seconds_)) {

                Profile next_profile = active_profile_;
                if (active_profile_ == Profile::Safe) {
                    next_profile = Profile::Balanced;
                } else if (active_profile_ == Profile::Balanced) {
                    next_profile = (target_clamped == Profile::Ultra) ? Profile::Aggressive : target_clamped;
                } else if (active_profile_ == Profile::Aggressive) {
                    next_profile = (target_clamped == Profile::Ultra) ? Profile::Ultra : Profile::Aggressive;
                }
                next_profile = get_platform_clamped_profile(next_profile);

                if (next_profile != active_profile_) {
                    std::string reason = "Metrics normalized over " + std::to_string(healthy_cycles_count_) +
                                         " hysteresis cycles (" + std::to_string(elapsed_sec) + "s cooldown elapsed)";
                    transition_to(next_profile, reason, sp);
                    healthy_cycles_count_ = 0;
                    return true;
                }
            }
        } else {
            healthy_cycles_count_ = 0;
        }
    } else {
        healthy_cycles_count_ = 0;
    }

    return false;
}

} // namespace zenith
