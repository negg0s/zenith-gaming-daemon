// SPDX-License-Identifier: MIT
#include "zenith/lock_guard.hpp"

#include <fcntl.h>
#include <sys/stat.h>

#ifdef __linux__
#include <sys/file.h>
#include <unistd.h>
#else
#include <io.h>
#include <sys/locking.h>
#endif

namespace zenith {

DaemonLock::DaemonLock(fs::path lock_path)
    : lock_path_(std::move(lock_path)) {}

DaemonLock::~DaemonLock() {
    release();
}

DaemonLock::DaemonLock(DaemonLock&& other) noexcept
    : lock_path_(std::move(other.lock_path_)),
      fd_(other.fd_),
      acquired_(other.acquired_),
      locked_pid_(other.locked_pid_) {
    other.fd_ = -1;
    other.acquired_ = false;
    other.locked_pid_ = -1;
}

DaemonLock& DaemonLock::operator=(DaemonLock&& other) noexcept {
    if (this != &other) {
        release();
        lock_path_ = std::move(other.lock_path_);
        fd_ = other.fd_;
        acquired_ = other.acquired_;
        locked_pid_ = other.locked_pid_;
        other.fd_ = -1;
        other.acquired_ = false;
        other.locked_pid_ = -1;
    }
    return *this;
}

bool DaemonLock::acquire() {
    if (acquired_) return true;

    auto parent = lock_path_.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        fs::create_directories(parent, ec);
    }

#ifdef __linux__
    fd_ = ::open(lock_path_.string().c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd_ < 0) {
        ZENITH_LOG_ERROR("Unable to open lock file: " + lock_path_.string());
        return false;
    }

    if (::flock(fd_, LOCK_EX | LOCK_NB) != 0) {
        // Lock already held by another process
        auto content = read_file_string(lock_path_);
        if (content) {
            try { locked_pid_ = std::stoi(*content); } catch (...) {}
        }
        ::close(fd_);
        fd_ = -1;
        acquired_ = false;
        return false;
    }

    // Write current PID into lock file
    if (::ftruncate(fd_, 0) == 0) {
        std::string pid_str = std::to_string(::getpid()) + "\n";
        if (::write(fd_, pid_str.data(), pid_str.size()) > 0) {
            (void)::fsync(fd_);
        }
    }
#else
    fd_ = ::_open(lock_path_.string().c_str(), _O_RDWR | _O_CREAT | _O_BINARY, _S_IREAD | _S_IWRITE);
    if (fd_ < 0) return false;

    if (::_locking(fd_, _LK_NBLCK, 1) != 0) {
        ::_close(fd_);
        fd_ = -1;
        return false;
    }
#endif

    acquired_ = true;
    return true;
}

void DaemonLock::release() {
    if (acquired_ && fd_ >= 0) {
#ifdef __linux__
        (void)::flock(fd_, LOCK_UN);
        ::close(fd_);
#else
        (void)::_locking(fd_, _LK_UNLCK, 1);
        ::_close(fd_);
#endif
        std::error_code ec;
        fs::remove(lock_path_, ec);
        fd_ = -1;
        acquired_ = false;
    }
}

} // namespace zenith
