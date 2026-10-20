#pragma once

#include "zenith/config.hpp"
#include "zenith/mutation.hpp"
#include "zenith/game_detector.hpp"
#include "zenith/socket_server.hpp"
#include <cstdint>
#include <cstddef>
#include <string_view>

namespace zenith::fuzz {

inline void fuzz_one_config(const uint8_t* data, size_t size) {
    if (!data || size == 0) return;
    std::string_view input(reinterpret_cast<const char*>(data), size);
    auto [cfg, res] = Config::parse_string(input);
    (void)cfg;
    (void)res;
}

inline void fuzz_one_sysfs(const uint8_t* data, size_t size) {
    if (!data || size == 0) return;
    std::string_view input(reinterpret_cast<const char*>(data), size);
    // 1. Bracketed token extraction
    auto start = input.find('[');
    if (start != std::string_view::npos) {
        auto end = input.find(']', start);
        if (end != std::string_view::npos) {
            std::string token(input.substr(start + 1, end - start - 1));
            (void)token;
        }
    }

    // 2. Size multiplier parsing (e.g., 96M, 32K, 1024)
    if (!input.empty() && input.size() < 32) {
        std::string s(input);
        size_t mult = 1024;
        if (s.ends_with('M') || s.ends_with('m')) mult = 1024 * 1024;
        else if (s.ends_with('K') || s.ends_with('k')) mult = 1024;
        try {
            uint64_t val = std::stoull(s) * mult;
            (void)val;
        } catch (...) {}
    }
}

inline void fuzz_one_socket(const uint8_t* data, size_t size) {
    if (!data || size == 0 || size > SocketServer::MAX_MESSAGE_SIZE) return;
    std::string_view input(reinterpret_cast<const char*>(data), size);
    std::string cmd = trim(input);

    ClientCredentials creds{ 1234, 1000, 1000 };
    std::string response;

    if (cmd == "STATUS") {
        response = "{\"status\":\"ok\"}";
    } else if (cmd == "PING") {
        response = "PONG";
    } else if (cmd.starts_with("TUNE") || cmd.starts_with("REVERT")) {
        response = "{\"status\":\"ok\"}";
    } else {
        response = "{\"status\":\"error\",\"message\":\"unknown command\"}";
    }
    (void)response;
}

inline void fuzz_one_proc(const uint8_t* data, size_t size) {
    if (!data || size == 0 || size > 8192) return;
    std::string_view input(reinterpret_cast<const char*>(data), size);

    // 1. Parse cmdline with null delimiters
    std::string clean_cmdline;
    for (char c : input) {
        clean_cmdline += (c == '\0') ? ' ' : c;
    }
    (void)clean_cmdline;

    // 2. Parse Linux /proc/<pid>/stat parenthesized comm safely:
    // e.g. "1234 (steam (child)) S 100 100 ..."
    auto first_paren = input.find('(');
    auto last_paren = input.rfind(')');
    if (first_paren != std::string_view::npos && last_paren != std::string_view::npos && last_paren > first_paren) {
        std::string comm(input.substr(first_paren + 1, last_paren - first_paren - 1));
        std::string_view rest = input.substr(last_paren + 1);
        auto tokens = split(rest, ' ');
        (void)comm;
        (void)tokens;
    }
}

} // namespace zenith::fuzz
