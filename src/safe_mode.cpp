// SPDX-License-Identifier: MIT
#include "zenith/safe_mode.hpp"
#include <chrono>

namespace zenith {

SafeModeManager::SafeModeManager(fs::path history_file, int window_minutes, size_t threshold_crashes)
    : history_file_(std::move(history_file)),
      window_minutes_(window_minutes),
      threshold_crashes_(threshold_crashes) {
    load();
}

static uint64_t current_epoch_seconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void SafeModeManager::load() {
    events_.clear();
    if (!fs::exists(history_file_)) return;

    auto content = read_file_string(history_file_);
    if (!content) return;

    auto lines = split(*content, '\n');
    for (const auto& line : lines) {
        auto trimmed = trim(line);
        if (trimmed.empty()) continue;
        auto tokens = split(trimmed, '|');
        if (!tokens.empty()) {
            CrashEvent ev;
            try { ev.timestamp_epoch_sec = std::stoull(tokens[0]); } catch (...) { continue; }
            if (tokens.size() > 1) ev.reason = tokens[1];
            events_.push_back(ev);
        }
    }
}

void SafeModeManager::save() {
    auto parent = history_file_.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        fs::create_directories(parent, ec);
    }
    std::stringstream ss;
    for (const auto& ev : events_) {
        ss << ev.timestamp_epoch_sec << "|" << ev.reason << "\n";
    }
    write_file_string(history_file_, ss.str());
}

void SafeModeManager::prune_expired(uint64_t now_sec) {
    uint64_t window_sec = static_cast<uint64_t>(window_minutes_) * 60;
    uint64_t cutoff = (now_sec > window_sec) ? (now_sec - window_sec) : 0;

    std::vector<CrashEvent> valid;
    for (const auto& ev : events_) {
        if (ev.timestamp_epoch_sec >= cutoff) {
            valid.push_back(ev);
        }
    }
    events_ = std::move(valid);
}

bool SafeModeManager::evaluate_and_record_boot() {
    uint64_t now = current_epoch_seconds();
    load();
    prune_expired(now);

    recent_crashes_ = events_.size();
    if (recent_crashes_ >= threshold_crashes_) {
        safe_mode_active_ = true;
        explanation_ = "SAFE MODE ENGAGED: Detected " + std::to_string(recent_crashes_) +
                       " crash events in the last " + std::to_string(window_minutes_) +
                       " minutes. All aggressive kernel and GPU mutations are disabled.";
        ZENITH_LOG_WARN(explanation_);
        save();
        return true;
    }

    safe_mode_active_ = false;
    explanation_ = "Normal mode active. Recent crashes in window: " + std::to_string(recent_crashes_);
    save();
    return false;
}

void SafeModeManager::record_crash(std::string_view reason) {
    uint64_t now = current_epoch_seconds();
    CrashEvent ev;
    ev.timestamp_epoch_sec = now;
    ev.reason = std::string(reason);
    events_.push_back(ev);
    prune_expired(now);
    save();
}

void SafeModeManager::record_clean_shutdown() {
    // On clean shutdown, if safe mode was not active, we clear pending crashes or retain history
    save();
}

bool SafeModeManager::reset() {
    events_.clear();
    safe_mode_active_ = false;
    recent_crashes_ = 0;
    explanation_ = "Safe mode reset by operator.";
    std::error_code ec;
    fs::remove(history_file_, ec);
    ZENITH_LOG_INFO("Reset crash history and deactivated Safe Mode.");
    return true;
}

} // namespace zenith
