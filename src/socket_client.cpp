#include "zenith/socket_client.hpp"
#include "zenith/i18n.hpp"

#include <cstring>

#if defined(__linux__)
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>
#endif

namespace zenith {

SocketClient::SocketClient(fs::path socket_path)
    : socket_path_(std::move(socket_path)) {}

bool SocketClient::is_daemon_reachable() {
#if defined(__linux__)
    int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return false;

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

    bool connected = (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0);
    ::close(fd);
    return connected;
#else
    return false;
#endif
}

Result<std::string> SocketClient::send_command(const std::string& command, int timeout_ms) {
#if defined(__linux__)
    int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        return Err(ErrorCode::SocketError, "Failed to create UNIX domain socket", errno);
    }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        int err = errno;
        ::close(fd);
        return Err(ErrorCode::SocketError,
                   i18n::format_error("E101", "Could not connect to Zenith daemon at " + socket_path_.string()),
                   err);
    }

    std::string payload = command;
    if (!payload.ends_with('\n')) payload += '\n';

    ssize_t written = ::write(fd, payload.data(), payload.size());
    if (written < 0) {
        int err = errno;
        ::close(fd);
        return Err(ErrorCode::SocketError, "Failed to write command to socket", err);
    }

    // Wait for response with poll
    struct pollfd pfd{};
    pfd.fd = fd;
    pfd.events = POLLIN;

    int poll_res = ::poll(&pfd, 1, timeout_ms);
    if (poll_res <= 0) {
        ::close(fd);
        return Err(ErrorCode::Timeout, "Socket read timed out waiting for daemon response");
    }

    char buffer[4096];
    ssize_t bytes_read = ::read(fd, buffer, sizeof(buffer) - 1);
    ::close(fd);

    if (bytes_read < 0) {
        return Err(ErrorCode::SocketError, "Failed to read response from socket", errno);
    }

    buffer[bytes_read] = '\0';
    return Ok(std::string(buffer));
#else
    (void)command;
    (void)timeout_ms;
    return Err(ErrorCode::NotSupported, "UNIX domain sockets are native to Linux");
#endif
}

} // namespace zenith
