#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <iostream>
#include <sstream>
#include <cstdint>

namespace zenith {

enum class StructuredLogLevel : uint8_t {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Critical
};

enum class LogFormatMode : uint8_t {
    Text,
    Journald,
    Json
};

struct LogField {
    std::string key;
    std::string value;
};

class RateLimiter {
public:
    struct LimitState {
        uint64_t count{0};
        uint64_t suppressed{0};
        std::chrono::steady_clock::time_point window_start;
    };

    explicit RateLimiter(uint32_t max_per_second = 5)
        : max_per_second_(max_per_second) {}

    // Returns true if message is allowed; false if suppressed
    // out_suppressed is set to number of previously suppressed messages if this is the first message of a new window
    bool check(const std::string& key, uint64_t& out_suppressed) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        auto& state = entries_[key];

        if (state.count == 0) {
            state.window_start = now;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - state.window_start).count();
        if (elapsed >= 1) {
            out_suppressed = state.suppressed;
            state.count = 1;
            state.suppressed = 0;
            state.window_start = now;
            return true;
        }

        if (state.count < max_per_second_) {
            state.count++;
            out_suppressed = 0;
            return true;
        }

        state.suppressed++;
        out_suppressed = 0;
        return false;
    }

private:
    std::mutex mutex_;
    uint32_t max_per_second_;
    std::unordered_map<std::string, LimitState> entries_;
};

class StructuredLogger {
public:
    static StructuredLogger& instance();

    void set_min_level(StructuredLogLevel level) { min_level_ = level; }
    [[nodiscard]] StructuredLogLevel min_level() const { return min_level_; }

    void set_format_mode(LogFormatMode mode) { mode_ = mode; }
    [[nodiscard]] LogFormatMode format_mode() const { return mode_; }

    void set_custom_sink(std::ostream* sink) { custom_sink_ = sink; }

    void log(StructuredLogLevel level,
             std::string_view subsystem,
             std::string_view message,
             const std::vector<LogField>& fields = {});

    void log_rate_limited(const std::string& key,
                          StructuredLogLevel level,
                          std::string_view subsystem,
                          std::string_view message,
                          const std::vector<LogField>& fields = {},
                          uint32_t max_per_sec = 5);

    static const char* level_to_string(StructuredLogLevel level) noexcept;
    static const char* level_to_journald_prefix(StructuredLogLevel level) noexcept;

private:
    StructuredLogger() = default;

    std::mutex output_mutex_;
    StructuredLogLevel min_level_{StructuredLogLevel::Info};
    LogFormatMode mode_{LogFormatMode::Text};
    std::ostream* custom_sink_{nullptr};
    RateLimiter rate_limiter_{5};
};

} // namespace zenith

#define ZENITH_LOG_TRACE(subsys, msg, ...) ::zenith::StructuredLogger::instance().log(::zenith::StructuredLogLevel::Trace, (subsys), (msg), ##__VA_ARGS__)
#define ZENITH_LOG_DEBUG(subsys, msg, ...) ::zenith::StructuredLogger::instance().log(::zenith::StructuredLogLevel::Debug, (subsys), (msg), ##__VA_ARGS__)
#define ZENITH_LOG_INFO(subsys, msg, ...)  ::zenith::StructuredLogger::instance().log(::zenith::StructuredLogLevel::Info,  (subsys), (msg), ##__VA_ARGS__)
#define ZENITH_LOG_WARN(subsys, msg, ...)  ::zenith::StructuredLogger::instance().log(::zenith::StructuredLogLevel::Warn,  (subsys), (msg), ##__VA_ARGS__)
#define ZENITH_LOG_ERROR(subsys, msg, ...) ::zenith::StructuredLogger::instance().log(::zenith::StructuredLogLevel::Error, (subsys), (msg), ##__VA_ARGS__)
#define ZENITH_LOG_CRIT(subsys, msg, ...)  ::zenith::StructuredLogger::instance().log(::zenith::StructuredLogLevel::Critical, (subsys), (msg), ##__VA_ARGS__)
