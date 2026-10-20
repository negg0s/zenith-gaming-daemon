#include "zenith/socket_server.hpp"

#ifdef __linux__
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#endif

namespace zenith {

SocketServer::SocketServer(fs::path socket_path, std::string group_name)
    : socket_path_(std::move(socket_path)),
      group_name_(std::move(group_name)) {}

SocketServer::~SocketServer() {
    stop();
}

bool SocketServer::authenticate_peer(int client_fd, ClientCredentials& creds) const {
#ifdef __linux__
    struct ucred ucred{};
    socklen_t len = sizeof(ucred);
    if (::getsockopt(client_fd, SOL_SOCKET, SO_PEERCRED, &ucred, &len) != 0) {
        ZENITH_LOG_ERROR("SO_PEERCRED extraction failed on client socket");
        return false;
    }

    creds.pid = ucred.pid;
    creds.uid = ucred.uid;
    creds.gid = ucred.gid;

    // Root is unconditionally authorized
    if (ucred.uid == 0) {
        return true;
    }

    // Check if client UID belongs to authorized group (wheel or gamemode)
    struct group* grp = ::getgrnam(group_name_.c_str());
    if (grp) {
        if (static_cast<gid_t>(ucred.gid) == grp->gr_gid) {
            return true;
        }

        // Check secondary groups
        struct passwd* pwd = ::getpwuid(ucred.uid);
        if (pwd) {
            int ngroups = 64;
            gid_t groups[64];
            if (::getgrouplist(pwd->pw_name, pwd->pw_gid, groups, &ngroups) != -1) {
                for (int i = 0; i < ngroups; ++i) {
                    if (groups[i] == grp->gr_gid) {
                        return true;
                    }
                }
            }
        }
    }

    ZENITH_LOG_WARN("Access denied for client UID " + std::to_string(ucred.uid) +
                    ": not in authorized group '" + group_name_ + "'");
    return false;
#else
    (void)client_fd;
    creds.pid = 1000;
    creds.uid = 1000;
    creds.gid = 1000;
    return true;
#endif
}

bool SocketServer::handle_connection(int client_fd) {
    ClientCredentials creds;
    if (!authenticate_peer(client_fd, creds)) {
#ifdef __linux__
        const char* err_msg = "{\"status\":\"error\",\"message\":\"Unauthorized: access denied by SO_PEERCRED policy\"}\n";
        (void)::write(client_fd, err_msg, std::strlen(err_msg));
        ::close(client_fd);
#endif
        return false;
    }

#ifdef __linux__
    char buffer[MAX_MESSAGE_SIZE + 1];
    ssize_t bytes_read = ::read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        ::close(client_fd);
        return false;
    }

    if (static_cast<size_t>(bytes_read) > MAX_MESSAGE_SIZE) {
        ZENITH_LOG_WARN("Payload exceeded MAX_MESSAGE_SIZE (4KB). Dropping connection.");
        ::close(client_fd);
        return false;
    }

    buffer[bytes_read] = '\0';
    std::string command = trim(buffer);

    std::string response = "{\"status\":\"ok\"}\n";
    if (command_handler_) {
        response = command_handler_(command, creds);
        if (!response.ends_with('\n')) response += '\n';
    }

    (void)::write(client_fd, response.data(), response.size());
    ::close(client_fd);
#else
    (void)client_fd;
#endif
    return true;
}

bool SocketServer::start() {
    auto parent = socket_path_.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        fs::create_directories(parent, ec);
    }

    std::error_code ec;
    fs::remove(socket_path_, ec);

#ifdef __linux__
    server_fd_ = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (server_fd_ < 0) {
        ZENITH_LOG_ERROR("Failed to create UNIX domain socket: " + socket_path_.string());
        return false;
    }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socket_path_.string().c_str(), sizeof(addr.sun_path) - 1);

    if (::bind(server_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        ZENITH_LOG_ERROR("Failed to bind UNIX domain socket: " + socket_path_.string());
        ::close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    // Permissions: 0660 (root and group members only)
    ::chmod(socket_path_.string().c_str(), 0660);

    // Set group ownership
    struct group* grp = ::getgrnam(group_name_.c_str());
    if (grp) {
        (void)::chown(socket_path_.string().c_str(), 0, grp->gr_gid);
    }

    if (::listen(server_fd_, 16) != 0) {
        ZENITH_LOG_ERROR("Failed to listen on socket: " + socket_path_.string());
        ::close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    running_ = true;
    ZENITH_LOG_INFO("Socket server listening at " + socket_path_.string() + " (group: " + group_name_ + ")");
    return true;
#else
    running_ = true;
    return true;
#endif
}

void SocketServer::stop() {
#ifdef __linux__
    if (server_fd_ >= 0) {
        ::close(server_fd_);
        server_fd_ = -1;
    }
#endif
    if (running_) {
        std::error_code ec;
        fs::remove(socket_path_, ec);
        running_ = false;
    }
}

} // namespace zenith
