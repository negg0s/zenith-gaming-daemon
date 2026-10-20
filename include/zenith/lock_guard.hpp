#pragma once

#include "zenith/common.hpp"

namespace zenith {

class DaemonLock {
public:
    explicit DaemonLock(fs::path lock_path = "/run/zenith/zenith.lock");
    ~DaemonLock();

    DaemonLock(const DaemonLock&) = delete;
    DaemonLock& operator=(const DaemonLock&) = delete;

    DaemonLock(DaemonLock&& other) noexcept;
    DaemonLock& operator=(DaemonLock&& other) noexcept;

    bool acquire();
    void release();

    bool is_acquired() const { return acquired_; }
    int get_locked_pid() const { return locked_pid_; }
    const fs::path& get_path() const { return lock_path_; }

private:
    fs::path lock_path_;
    int fd_ = -1;
    bool acquired_ = false;
    int locked_pid_ = -1;
};

} // namespace zenith
