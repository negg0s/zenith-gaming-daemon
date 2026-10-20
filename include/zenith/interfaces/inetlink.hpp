#pragma once

#include "zenith/result.hpp"
#include <chrono>
#include <optional>
#include <queue>
#include <mutex>
#include <sys/types.h>

namespace zenith {

enum class NetlinkEventType {
    Fork,
    Exec,
    Exit,
    Unknown
};

struct NetlinkProcessEvent {
    NetlinkEventType type{NetlinkEventType::Unknown};
    pid_t pid{0};
    pid_t tgid{0};
    int exit_code{0};
};

class INetlink {
public:
    virtual ~INetlink() = default;

    virtual Result<void> open() = 0;
    virtual void close() = 0;
    [[nodiscard]] virtual bool is_connected() const = 0;
    virtual Result<std::optional<NetlinkProcessEvent>> poll_event(std::chrono::milliseconds timeout) = 0;
};

class MockNetlink : public INetlink {
public:
    Result<void> open() override {
        std::lock_guard<std::mutex> lock(mutex_);
        connected_ = true;
        return Ok();
    }

    void close() override {
        std::lock_guard<std::mutex> lock(mutex_);
        connected_ = false;
    }

    bool is_connected() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return connected_;
    }

    void inject_event(NetlinkProcessEvent ev) {
        std::lock_guard<std::mutex> lock(mutex_);
        events_.push(ev);
    }

    Result<std::optional<NetlinkProcessEvent>> poll_event(std::chrono::milliseconds) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!connected_) {
            return Result<std::optional<NetlinkProcessEvent>>::err(
                Error(ErrorCode::InternalError, "Netlink not connected"));
        }
        if (events_.empty()) {
            return Ok(std::optional<NetlinkProcessEvent>{std::nullopt});
        }
        auto ev = events_.front();
        events_.pop();
        return Ok(std::optional<NetlinkProcessEvent>{ev});
    }

private:
    std::mutex mutex_;
    bool connected_{false};
    std::queue<NetlinkProcessEvent> events_;
};

} // namespace zenith
