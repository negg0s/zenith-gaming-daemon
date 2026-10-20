#pragma once

#include "zenith/result.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <atomic>
#include <cstdint>

namespace zenith {

namespace PollEvent {
    constexpr uint32_t Read  = 1 << 0;
    constexpr uint32_t Write = 1 << 1;
    constexpr uint32_t Error = 1 << 2;
    constexpr uint32_t HangUp = 1 << 3;
}

using FdCallback = std::function<void(int fd, uint32_t events)>;
using TaskCallback = std::function<void()>;

class IPoller {
public:
    virtual ~IPoller() = default;

    virtual Result<void> add_fd(int fd, uint32_t events, FdCallback cb) = 0;
    virtual Result<void> modify_fd(int fd, uint32_t events) = 0;
    virtual Result<void> remove_fd(int fd) = 0;
    virtual Result<int> poll(std::chrono::milliseconds timeout) = 0;
};

class MockPoller : public IPoller {
public:
    struct Registration {
        uint32_t events{0};
        FdCallback callback;
    };

    Result<void> add_fd(int fd, uint32_t events, FdCallback cb) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (registrations_.find(fd) != registrations_.end()) {
            return Result<void>::err(Error(ErrorCode::AlreadyExists, "FD already registered in MockPoller"));
        }
        registrations_[fd] = {events, std::move(cb)};
        return Ok();
    }

    Result<void> modify_fd(int fd, uint32_t events) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registrations_.find(fd);
        if (it == registrations_.end()) {
            return Result<void>::err(Error(ErrorCode::NotFound, "FD not found in MockPoller"));
        }
        it->second.events = events;
        return Ok();
    }

    Result<void> remove_fd(int fd) override {
        std::lock_guard<std::mutex> lock(mutex_);
        registrations_.erase(fd);
        return Ok();
    }

    Result<int> poll(std::chrono::milliseconds) override {
        std::lock_guard<std::mutex> lock(mutex_);
        int handled = 0;
        while (!pending_events_.empty()) {
            auto [fd, ev] = pending_events_.front();
            pending_events_.pop();
            auto it = registrations_.find(fd);
            if (it != registrations_.end() && (it->second.events & ev)) {
                it->second.callback(fd, ev);
                handled++;
            }
        }
        return Ok(handled);
    }

    void trigger_event(int fd, uint32_t events) {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_events_.push({fd, events});
    }

    bool has_fd(int fd) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return registrations_.find(fd) != registrations_.end();
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<int, Registration> registrations_;
    std::queue<std::pair<int, uint32_t>> pending_events_;
};

#if defined(__linux__)
class EpollPoller : public IPoller {
public:
    EpollPoller();
    ~EpollPoller() override;

    Result<void> add_fd(int fd, uint32_t events, FdCallback cb) override;
    Result<void> modify_fd(int fd, uint32_t events) override;
    Result<void> remove_fd(int fd) override;
    Result<int> poll(std::chrono::milliseconds timeout) override;

private:
    int epoll_fd_{-1};
    struct FdContext {
        int fd{-1};
        uint32_t events{0};
        FdCallback callback;
    };
    std::unordered_map<int, std::unique_ptr<FdContext>> contexts_;
};
#endif

class EventLoop {
public:
    explicit EventLoop(std::shared_ptr<IPoller> poller = nullptr);
    ~EventLoop();

    Result<void> add_fd(int fd, uint32_t events, FdCallback cb) {
        return poller_->add_fd(fd, events, std::move(cb));
    }

    Result<void> remove_fd(int fd) {
        return poller_->remove_fd(fd);
    }

    // Thread-safe task posting from worker threads to single reactor thread
    void post(TaskCallback task);

    // Run until stop() is invoked
    void run();

    // Single step iteration
    void run_one(std::chrono::milliseconds timeout = std::chrono::milliseconds(10));

    void stop();

    [[nodiscard]] bool is_running() const noexcept {
        return running_.load(std::memory_order_relaxed);
    }

private:
    std::shared_ptr<IPoller> poller_;
    std::atomic<bool> running_{false};
    std::mutex task_mutex_;
    std::vector<TaskCallback> tasks_;
};

} // namespace zenith
