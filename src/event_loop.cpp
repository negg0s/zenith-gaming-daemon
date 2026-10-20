#include "zenith/event_loop.hpp"
#include <system_error>
#include <cerrno>

#if defined(__linux__)
#include <sys/epoll.h>
#include <unistd.h>
#endif

namespace zenith {

#if defined(__linux__)
EpollPoller::EpollPoller() {
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
}

EpollPoller::~EpollPoller() {
    if (epoll_fd_ >= 0) {
        ::close(epoll_fd_);
    }
}

Result<void> EpollPoller::add_fd(int fd, uint32_t events, FdCallback cb) {
    if (epoll_fd_ < 0) {
        return Result<void>::err(Error(ErrorCode::InternalError, "Epoll instance not initialized"));
    }
    if (contexts_.find(fd) != contexts_.end()) {
        return Result<void>::err(Error(ErrorCode::AlreadyExists, "FD already registered in epoll"));
    }

    auto ctx = std::make_unique<FdContext>();
    ctx->fd = fd;
    ctx->events = events;
    ctx->callback = std::move(cb);

    struct epoll_event ev{};
    if (events & PollEvent::Read) ev.events |= EPOLLIN;
    if (events & PollEvent::Write) ev.events |= EPOLLOUT;
    if (events & PollEvent::Error) ev.events |= EPOLLERR;
    if (events & PollEvent::HangUp) ev.events |= EPOLLHUP;
    ev.data.ptr = ctx.get();

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
        return Result<void>::err(Error(ErrorCode::SystemError, "epoll_ctl ADD failed", errno));
    }

    contexts_[fd] = std::move(ctx);
    return Ok();
}

Result<void> EpollPoller::modify_fd(int fd, uint32_t events) {
    auto it = contexts_.find(fd);
    if (it == contexts_.end()) {
        return Result<void>::err(Error(ErrorCode::NotFound, "FD not registered in epoll"));
    }

    it->second->events = events;
    struct epoll_event ev{};
    if (events & PollEvent::Read) ev.events |= EPOLLIN;
    if (events & PollEvent::Write) ev.events |= EPOLLOUT;
    if (events & PollEvent::Error) ev.events |= EPOLLERR;
    if (events & PollEvent::HangUp) ev.events |= EPOLLHUP;
    ev.data.ptr = it->second.get();

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
        return Result<void>::err(Error(ErrorCode::SystemError, "epoll_ctl MOD failed", errno));
    }
    return Ok();
}

Result<void> EpollPoller::remove_fd(int fd) {
    auto it = contexts_.find(fd);
    if (it == contexts_.end()) {
        return Result<void>::err(Error(ErrorCode::NotFound, "FD not registered in epoll"));
    }

    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    contexts_.erase(it);
    return Ok();
}

Result<int> EpollPoller::poll(std::chrono::milliseconds timeout) {
    if (epoll_fd_ < 0) {
        return Result<int>::err(Error(ErrorCode::InternalError, "Epoll instance not initialized"));
    }

    constexpr int kMaxEvents = 64;
    struct epoll_event events[kMaxEvents];

    int nfds = epoll_wait(epoll_fd_, events, kMaxEvents, static_cast<int>(timeout.count()));
    if (nfds < 0) {
        if (errno == EINTR) {
            return Ok(0);
        }
        return Result<int>::err(Error(ErrorCode::SystemError, "epoll_wait failed", errno));
    }

    for (int i = 0; i < nfds; ++i) {
        auto* ctx = static_cast<FdContext*>(events[i].data.ptr);
        if (ctx && ctx->callback) {
            uint32_t flags = 0;
            if (events[i].events & EPOLLIN) flags |= PollEvent::Read;
            if (events[i].events & EPOLLOUT) flags |= PollEvent::Write;
            if (events[i].events & EPOLLERR) flags |= PollEvent::Error;
            if (events[i].events & EPOLLHUP) flags |= PollEvent::HangUp;
            ctx->callback(ctx->fd, flags);
        }
    }

    return Ok(nfds);
}
#endif

EventLoop::EventLoop(std::shared_ptr<IPoller> poller)
    : poller_(std::move(poller)) {
    if (!poller_) {
#if defined(__linux__)
        poller_ = std::make_shared<EpollPoller>();
#else
        poller_ = std::make_shared<MockPoller>();
#endif
    }
}

EventLoop::~EventLoop() {
    stop();
}

void EventLoop::post(TaskCallback task) {
    std::lock_guard<std::mutex> lock(task_mutex_);
    tasks_.push_back(std::move(task));
}

void EventLoop::run() {
    running_.store(true, std::memory_order_relaxed);
    while (running_.load(std::memory_order_relaxed)) {
        run_one(std::chrono::milliseconds(20));
    }
}

void EventLoop::run_one(std::chrono::milliseconds timeout) {
    // 1. Execute queued posted tasks
    std::vector<TaskCallback> local_tasks;
    {
        std::lock_guard<std::mutex> lock(task_mutex_);
        if (!tasks_.empty()) {
            local_tasks.swap(tasks_);
        }
    }
    for (auto& task : local_tasks) {
        if (task) {
            task();
        }
    }

    // 2. Poll I/O descriptors
    if (poller_) {
        poller_->poll(timeout);
    }
}

void EventLoop::stop() {
    running_.store(false, std::memory_order_relaxed);
}

} // namespace zenith
