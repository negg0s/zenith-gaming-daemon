#pragma once

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <filesystem>
#include <chrono>
#include <format>
#include <fstream>
#include <sstream>
#include <optional>
#include <functional>
#include <unordered_map>
#include <algorithm>
#include <cstdint>
#include <cstring>

namespace zenith {

namespace fs = std::filesystem;

enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error
};

class Logger {
public:
    static void init(LogLevel min_level = LogLevel::Info) {
        get_min_level() = min_level;
    }

    static void log(LogLevel level, std::string_view msg) {
        if (level < get_min_level()) return;
        const char* prefix = "[INFO]";
        const char* color = "\033[0m";
        switch (level) {
            case LogLevel::Debug: prefix = "[DEBUG]"; color = "\033[36m"; break;
            case LogLevel::Info:  prefix = "[INFO] "; color = "\033[32m"; break;
            case LogLevel::Warn:  prefix = "[WARN] "; color = "\033[33m"; break;
            case LogLevel::Error: prefix = "[ERROR]"; color = "\033[31m"; break;
        }
        std::cout << color << prefix << " [zenith] " << msg << "\033[0m\n";
    }

    static LogLevel& get_min_level() {
        static LogLevel level = LogLevel::Info;
        return level;
    }
};

#define ZENITH_LOG_DEBUG(msg) ::zenith::Logger::log(::zenith::LogLevel::Debug, (msg))
#define ZENITH_LOG_INFO(msg)  ::zenith::Logger::log(::zenith::LogLevel::Info,  (msg))
#define ZENITH_LOG_WARN(msg)  ::zenith::Logger::log(::zenith::LogLevel::Warn,  (msg))
#define ZENITH_LOG_ERROR(msg) ::zenith::Logger::log(::zenith::LogLevel::Error, (msg))

inline std::string trim(std::string_view str) {
    const auto first = str.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return "";
    const auto last = str.find_last_not_of(" \t\r\n");
    return std::string(str.substr(first, last - first + 1));
}

inline std::vector<std::string> split(std::string_view str, char delim) {
    std::vector<std::string> tokens;
    size_t start = 0;
    while (start < str.size()) {
        auto end = str.find(delim, start);
        if (end == std::string_view::npos) end = str.size();
        tokens.emplace_back(str.substr(start, end - start));
        start = end + 1;
    }
    return tokens;
}

inline std::optional<std::string> read_file_string(const fs::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) return std::nullopt;
    std::stringstream ss;
    ss << file.rdbuf();
    return trim(ss.str());
}

inline bool write_file_string(const fs::path& path, std::string_view content) {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << content;
    return file.good();
}

} // namespace zenith
