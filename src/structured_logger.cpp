// SPDX-License-Identifier: MIT
#include "zenith/structured_logger.hpp"
#include <iomanip>
#include <ctime>

namespace zenith {

StructuredLogger& StructuredLogger::instance() {
    static StructuredLogger logger;
    return logger;
}

const char* StructuredLogger::level_to_string(StructuredLogLevel level) noexcept {
    switch (level) {
        case StructuredLogLevel::Trace: return "TRACE";
        case StructuredLogLevel::Debug: return "DEBUG";
        case StructuredLogLevel::Info:  return "INFO";
        case StructuredLogLevel::Warn:  return "WARN";
        case StructuredLogLevel::Error: return "ERROR";
        case StructuredLogLevel::Critical: return "CRITICAL";
    }
    return "UNKNOWN";
}

const char* StructuredLogger::level_to_journald_prefix(StructuredLogLevel level) noexcept {
    switch (level) {
        case StructuredLogLevel::Trace: return "<7>"; // SD_DEBUG
        case StructuredLogLevel::Debug: return "<7>"; // SD_DEBUG
        case StructuredLogLevel::Info:  return "<6>"; // SD_INFO
        case StructuredLogLevel::Warn:  return "<4>"; // SD_WARNING
        case StructuredLogLevel::Error: return "<3>"; // SD_ERR
        case StructuredLogLevel::Critical: return "<2>"; // SD_CRIT
    }
    return "<6>";
}

void StructuredLogger::log(StructuredLogLevel level,
                           std::string_view subsystem,
                           std::string_view message,
                           const std::vector<LogField>& fields) {
    if (level < min_level_) return;

    std::lock_guard<std::mutex> lock(output_mutex_);
    std::ostream& out = custom_sink_ ? *custom_sink_ : std::cout;

    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm_buf{};
#if defined(_WIN32)
    gmtime_s(&tm_buf, &now_time_t);
#else
    gmtime_r(&now_time_t, &tm_buf);
#endif

    if (mode_ == LogFormatMode::Json) {
        // Output single-line JSON format
        out << "{\"timestamp\":\""
            << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S")
            << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z\""
            << ",\"level\":\"" << level_to_string(level) << "\""
            << ",\"subsystem\":\"" << subsystem << "\""
            << ",\"message\":\"" << message << "\"";
        if (!fields.empty()) {
            out << ",\"fields\":{";
            for (size_t i = 0; i < fields.size(); ++i) {
                if (i > 0) out << ",";
                out << "\"" << fields[i].key << "\":\"" << fields[i].value << "\"";
            }
            out << "}";
        }
        out << "}\n";
    } else if (mode_ == LogFormatMode::Journald) {
        // Systemd native stream protocol: <PRIORITY>[subsystem] message key=val
        out << level_to_journald_prefix(level)
            << "[" << subsystem << "] "
            << message;
        for (const auto& field : fields) {
            out << " " << field.key << "=\"" << field.value << "\"";
        }
        out << "\n";
    } else {
        // ANSI Color Text Format
        const char* color = "\033[0m";
        switch (level) {
            case StructuredLogLevel::Trace: color = "\033[90m"; break;
            case StructuredLogLevel::Debug: color = "\033[36m"; break;
            case StructuredLogLevel::Info:  color = "\033[32m"; break;
            case StructuredLogLevel::Warn:  color = "\033[33m"; break;
            case StructuredLogLevel::Error: color = "\033[31m"; break;
            case StructuredLogLevel::Critical: color = "\033[1;31m"; break;
        }

        out << color
            << "[" << std::put_time(&tm_buf, "%H:%M:%S") << "." << std::setfill('0') << std::setw(3) << ms.count() << "] "
            << "[" << level_to_string(level) << "] "
            << "[" << subsystem << "] "
            << message;
        if (!fields.empty()) {
            out << " (";
            for (size_t i = 0; i < fields.size(); ++i) {
                if (i > 0) out << ", ";
                out << fields[i].key << "=" << fields[i].value;
            }
            out << ")";
        }
        out << "\033[0m\n";
    }
    out.flush();
}

void StructuredLogger::log_rate_limited(const std::string& key,
                                       StructuredLogLevel level,
                                       std::string_view subsystem,
                                       std::string_view message,
                                       const std::vector<LogField>& fields,
                                       uint32_t /*max_per_sec*/) {
    uint64_t suppressed = 0;
    if (rate_limiter_.check(key, suppressed)) {
        if (suppressed > 0) {
            std::string note = std::string(message) + " [suppressed " + std::to_string(suppressed) + " identical logs]";
            log(level, subsystem, note, fields);
        } else {
            log(level, subsystem, message, fields);
        }
    }
}

} // namespace zenith
