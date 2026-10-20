#pragma once

#include "zenith/result.hpp"
#include <filesystem>
#include <string>

namespace zenith {

namespace fs = std::filesystem;

class SocketClient {
public:
    explicit SocketClient(fs::path socket_path = "/run/zenith/zenith.sock");

    // Sends command and reads reply string. Returns error if socket is unreachable.
    Result<std::string> send_command(const std::string& command, int timeout_ms = 1000);

    // Checks if daemon socket is active and accepting connections
    bool is_daemon_reachable();

    const fs::path& get_path() const { return socket_path_; }

private:
    fs::path socket_path_;
};

} // namespace zenith
