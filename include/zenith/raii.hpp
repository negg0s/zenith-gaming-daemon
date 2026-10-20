// SPDX-License-Identifier: MIT
#pragma once

#if defined(_WIN32)
#include <io.h>
#define close _close
#else
#include <unistd.h>
#include <fcntl.h>
#endif

#include <string>
#include <utility>
#include <filesystem>
#include <functional>

namespace zenith {

// RAII File Descriptor Wrapper
class UniqueFd {
public:
    constexpr UniqueFd() noexcept : fd_(-1) {}
    constexpr explicit UniqueFd(int fd) noexcept : fd_(fd) {}

    ~UniqueFd() noexcept {
        reset();
    }

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    UniqueFd(UniqueFd&& other) noexcept : fd_(other.release()) {}

    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    [[nodiscard]] constexpr int get() const noexcept { return fd_; }
    [[nodiscard]] constexpr bool is_valid() const noexcept { return fd_ >= 0; }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return is_valid(); }

    [[nodiscard]] int release() noexcept {
        int old_fd = fd_;
        fd_ = -1;
        return old_fd;
    }

    void reset(int new_fd = -1) noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
        }
        fd_ = new_fd;
    }

private:
    int fd_{-1};
};

// RAII cgroup Directory Guard
// Automatically creates a cgroup v2 directory and removes it on destruction (if requested)
class CgroupDirGuard {
public:
    explicit CgroupDirGuard(std::string path, bool remove_on_destroy = true)
        : path_(std::move(path)), remove_on_destroy_(remove_on_destroy) {
        if (!path_.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(path_, ec);
            created_ = !ec;
        }
    }

    ~CgroupDirGuard() noexcept {
        if (remove_on_destroy_ && created_ && !path_.empty()) {
            std::error_code ec;
            std::filesystem::remove(path_, ec);
        }
    }

    CgroupDirGuard(const CgroupDirGuard&) = delete;
    CgroupDirGuard& operator=(const CgroupDirGuard&) = delete;

    CgroupDirGuard(CgroupDirGuard&& other) noexcept
        : path_(std::move(other.path_)),
          remove_on_destroy_(other.remove_on_destroy_),
          created_(other.created_) {
        other.created_ = false;
    }

    CgroupDirGuard& operator=(CgroupDirGuard&& other) noexcept {
        if (this != &other) {
            if (remove_on_destroy_ && created_ && !path_.empty()) {
                std::error_code ec;
                std::filesystem::remove(path_, ec);
            }
            path_ = std::move(other.path_);
            remove_on_destroy_ = other.remove_on_destroy_;
            created_ = other.created_;
            other.created_ = false;
        }
        return *this;
    }

    [[nodiscard]] const std::string& path() const noexcept { return path_; }
    [[nodiscard]] bool is_created() const noexcept { return created_; }

    void dismiss() noexcept {
        remove_on_destroy_ = false;
    }

private:
    std::string path_;
    bool remove_on_destroy_{true};
    bool created_{false};
};

// RAII Scope Exit Guard for rollback logic
template <typename Callback>
class ScopeExitGuard {
public:
    explicit ScopeExitGuard(Callback&& cb)
        : callback_(std::forward<Callback>(cb)), active_(true) {}

    ~ScopeExitGuard() noexcept {
        if (active_) {
            try {
                callback_();
            } catch (...) {}
        }
    }

    ScopeExitGuard(const ScopeExitGuard&) = delete;
    ScopeExitGuard& operator=(const ScopeExitGuard&) = delete;

    ScopeExitGuard(ScopeExitGuard&& other) noexcept
        : callback_(std::move(other.callback_)), active_(other.active_) {
        other.active_ = false;
    }

    void dismiss() noexcept {
        active_ = false;
    }

private:
    Callback callback_;
    bool active_{true};
};

template <typename Callback>
[[nodiscard]] inline auto make_scope_exit(Callback&& cb) {
    return ScopeExitGuard<std::decay_t<Callback>>(std::forward<Callback>(cb));
}

} // namespace zenith
