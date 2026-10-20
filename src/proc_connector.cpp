#include "zenith/proc_connector.hpp"

#ifdef __linux__
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/connector.h>
#include <linux/cn_proc.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>
#endif

namespace zenith {

ProcConnector::ProcConnector() = default;

ProcConnector::~ProcConnector() {
    stop();
}

bool ProcConnector::start(ProcEventCallback callback) {
    if (running_.load()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callback_ = std::move(callback);
    }

#ifdef __linux__
    socket_fd_ = socket(PF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, NETLINK_CONNECTOR);
    if (socket_fd_ >= 0) {
        struct sockaddr_nl sa;
        std::memset(&sa, 0, sizeof(sa));
        sa.nl_family = AF_NETLINK;
        sa.nl_groups = CN_IDX_PROC;
        sa.nl_pid = static_cast<__u32>(getpid());

        if (bind(socket_fd_, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)) == 0) {
            // Standard Netlink buffer payload to avoid flexible array struct warnings
            char buf[NLMSG_SPACE(sizeof(struct cn_msg) + sizeof(enum proc_cn_mcast_op))];
            std::memset(buf, 0, sizeof(buf));

            auto* nl_hdr = reinterpret_cast<struct nlmsghdr*>(buf);
            nl_hdr->nlmsg_len = sizeof(buf);
            nl_hdr->nlmsg_pid = static_cast<__u32>(getpid());
            nl_hdr->nlmsg_type = NLMSG_DONE;

            auto* cn_hdr = reinterpret_cast<struct cn_msg*>(NLMSG_DATA(nl_hdr));
            cn_hdr->id.idx = CN_IDX_PROC;
            cn_hdr->id.val = CN_VAL_PROC;
            cn_hdr->len = sizeof(enum proc_cn_mcast_op);

            auto* op = reinterpret_cast<enum proc_cn_mcast_op*>(cn_hdr->data);
            *op = PROC_CN_MCAST_LISTEN;

            if (send(socket_fd_, buf, sizeof(buf), 0) >= 0) {
                netlink_available_ = true;
            }
        }

        if (!netlink_available_) {
            close(socket_fd_);
            socket_fd_ = -1;
        }
    }
#endif

    running_.store(true);

    if (netlink_available_) {
        listener_thread_ = std::thread(&ProcConnector::listener_loop, this);
    }

    return true;
}

void ProcConnector::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    if (listener_thread_.joinable()) {
        listener_thread_.join();
    }

#ifdef __linux__
    if (socket_fd_ >= 0) {
        // Standard Netlink unsubscribe packet
        char buf[NLMSG_SPACE(sizeof(struct cn_msg) + sizeof(enum proc_cn_mcast_op))];
        std::memset(buf, 0, sizeof(buf));

        auto* nl_hdr = reinterpret_cast<struct nlmsghdr*>(buf);
        nl_hdr->nlmsg_len = sizeof(buf);
        nl_hdr->nlmsg_pid = static_cast<__u32>(getpid());
        nl_hdr->nlmsg_type = NLMSG_DONE;

        auto* cn_hdr = reinterpret_cast<struct cn_msg*>(NLMSG_DATA(nl_hdr));
        cn_hdr->id.idx = CN_IDX_PROC;
        cn_hdr->id.val = CN_VAL_PROC;
        cn_hdr->len = sizeof(enum proc_cn_mcast_op);

        auto* op = reinterpret_cast<enum proc_cn_mcast_op*>(cn_hdr->data);
        *op = PROC_CN_MCAST_IGNORE;

        (void)send(socket_fd_, buf, sizeof(buf), 0);

        close(socket_fd_);
        socket_fd_ = -1;
    }
#endif

    netlink_available_ = false;
}

void ProcConnector::inject_event(const ProcEvent& event) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (callback_) {
        callback_(event);
    }
}

void ProcConnector::listener_loop() {
#ifdef __linux__
    char buffer[4096];
    struct pollfd pfd;
    pfd.fd = socket_fd_;
    pfd.events = POLLIN;

    while (running_.load()) {
        int ret = poll(&pfd, 1, 50); // 50ms poll timeout
        if (ret <= 0) {
            continue;
        }

        if (!(pfd.revents & POLLIN)) {
            continue;
        }

        ssize_t len = recv(socket_fd_, buffer, sizeof(buffer), 0);
        if (len <= 0) {
            continue;
        }

        auto* nlhdr = reinterpret_cast<struct nlmsghdr*>(buffer);
        while (NLMSG_OK(nlhdr, static_cast<unsigned int>(len))) {
            if (nlhdr->nlmsg_type == NLMSG_ERROR || nlhdr->nlmsg_type == NLMSG_NOOP) {
                nlhdr = NLMSG_NEXT(nlhdr, len);
                continue;
            }

            auto* cn_hdr = reinterpret_cast<struct cn_msg*>(NLMSG_DATA(nlhdr));
            if (cn_hdr->id.idx == CN_IDX_PROC && cn_hdr->id.val == CN_VAL_PROC) {
                auto* proc_ev = reinterpret_cast<struct proc_event*>(cn_hdr->data);
                ProcEvent ev;

                switch (proc_ev->what) {
                    case PROC_EVENT_FORK:
                        ev.type = ProcEventType::Fork;
                        ev.pid = static_cast<int>(proc_ev->event_data.fork.child_pid);
                        ev.tgid = static_cast<int>(proc_ev->event_data.fork.child_tgid);
                        ev.ppid = static_cast<int>(proc_ev->event_data.fork.parent_pid);
                        break;
                    case PROC_EVENT_EXEC:
                        ev.type = ProcEventType::Exec;
                        ev.pid = static_cast<int>(proc_ev->event_data.exec.process_pid);
                        ev.tgid = static_cast<int>(proc_ev->event_data.exec.process_tgid);
                        break;
                    case PROC_EVENT_EXIT:
                        ev.type = ProcEventType::Exit;
                        ev.pid = static_cast<int>(proc_ev->event_data.exit.process_pid);
                        ev.tgid = static_cast<int>(proc_ev->event_data.exit.process_tgid);
                        ev.exit_code = static_cast<int>(proc_ev->event_data.exit.exit_code);
                        ev.exit_signal = static_cast<int>(proc_ev->event_data.exit.exit_signal);
                        break;
                    default:
                        ev.type = ProcEventType::Unknown;
                        break;
                }

                if (ev.type != ProcEventType::Unknown) {
                    std::lock_guard<std::mutex> lock(callback_mutex_);
                    if (callback_) {
                        callback_(ev);
                    }
                }
            }

            nlhdr = NLMSG_NEXT(nlhdr, len);
        }
    }
#endif
}

} // namespace zenith
