#pragma once

#include "zenith/common.hpp"

namespace zenith {

struct CrashEvent {
    uint64_t timestamp_epoch_sec = 0;
    std::string reason;
};

class SafeModeManager {
public:
    explicit SafeModeManager(fs::path history_file = "/run/zenith/crash_history.json",
                             int window_minutes = 10,
                             size_t threshold_crashes = 3);

    // Evaluates history, logs explanation if threshold met, and returns whether Safe Mode is active
    bool evaluate_and_record_boot();

    // Invoked on clean shutdown
    void record_clean_shutdown();

    // Explicit crash registration
    void record_crash(std::string_view reason);

    // Operator reset
    bool reset();

    bool is_safe_mode_active() const { return safe_mode_active_; }
    size_t get_recent_crash_count() const { return recent_crashes_; }
    const std::string& get_reason() const { return explanation_; }

private:
    fs::path history_file_;
    int window_minutes_;
    size_t threshold_crashes_;
    bool safe_mode_active_ = false;
    size_t recent_crashes_ = 0;
    std::string explanation_;
    std::vector<CrashEvent> events_;

    void load();
    void save();
    void prune_expired(uint64_t now_sec);
};

} // namespace zenith
