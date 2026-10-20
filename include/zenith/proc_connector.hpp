#pragma once

#include "zenith/common.hpp"
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>

namespace zenith {

enum class ProcEventType {
    Fork,
    Exec,
    Exit,
    Unknown
};

struct ProcEvent {
    ProcEventType type = ProcEventType::Unknown;
    int pid = 0;
    int tgid = 0;
    int ppid = 0;
    int exit_code = 0;
    int exit_signal = 0;
};

using ProcEventCallback = std::function<void(const ProcEvent&)>;

class ProcConnector {
public:
    ProcConnector();
    ~ProcConnector();

    ProcConnector(const ProcConnector&) = delete;
    ProcConnector& operator=(const ProcConnector&) = delete;

    [[nodiscard]] bool start(ProcEventCallback callback);
    void stop();

    [[nodiscard]] bool is_running() const { return running_.load(); }
    [[nodiscard]] bool is_netlink_available() const { return netlink_available_; }

    // Test helper to inject simulated events
    void inject_event(const ProcEvent& event);

private:
    void listener_loop();

    std::atomic<bool> running_{false};
    bool netlink_available_{false};
    int socket_fd_{-1};
    ProcEventCallback callback_;
    std::mutex callback_mutex_;
    std::thread listener_thread_;
};

} // namespace zenith
