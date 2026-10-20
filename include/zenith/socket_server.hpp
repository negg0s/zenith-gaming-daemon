#pragma once

#include "zenith/common.hpp"

namespace zenith {

struct ClientCredentials {
    int pid = -1;
    int uid = -1;
    int gid = -1;
};

class SocketServer {
public:
    explicit SocketServer(fs::path socket_path = "/run/zenith/zenith.sock",
                          std::string group_name = "wheel");
    ~SocketServer();

    bool start();
    void stop();

    // Max allowed message payload size: 4 KB
    static constexpr size_t MAX_MESSAGE_SIZE = 4096;

    bool is_running() const { return running_; }
    const fs::path& get_path() const { return socket_path_; }

    void set_command_handler(std::function<std::string(const std::string&, const ClientCredentials&)> handler) {
        command_handler_ = std::move(handler);
    }

    // Handles a single client connection with SO_PEERCRED check
    bool handle_connection(int client_fd);

private:
    fs::path socket_path_;
    std::string group_name_;
    int server_fd_ = -1;
    bool running_ = false;

    std::function<std::string(const std::string&, const ClientCredentials&)> command_handler_;

    bool authenticate_peer(int client_fd, ClientCredentials& creds) const;
};

} // namespace zenith
