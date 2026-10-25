// SPDX-License-Identifier: MIT
#include "zenith/secure_io.hpp"

#include <fcntl.h>
#include <sys/stat.h>

#ifdef __linux__
#include <unistd.h>
#include <sys/syscall.h>

#ifndef RESOLVE_BENEATH
#define RESOLVE_BENEATH 0x01
#endif
#ifndef RESOLVE_NO_MAGICLINKS
#define RESOLVE_NO_MAGICLINKS 0x02
#endif
#ifndef RESOLVE_NO_SYMLINKS
#define RESOLVE_NO_SYMLINKS 0x04
#endif

#ifndef SYS_openat2
#if defined(__x86_64__)
#define SYS_openat2 437
#elif defined(__aarch64__)
#define SYS_openat2 437
#endif
#endif

struct zenith_open_how {
    uint64_t flags;
    uint64_t mode;
    uint64_t resolve;
};
#endif

namespace zenith {

std::pair<fs::path, std::string> SecureIo::split_trusted_path(const fs::path& absolute_path) {
    static const std::vector<fs::path> TRUSTED_BASES = {
        "/sys/kernel/mm/transparent_hugepage",
        "/proc/sys/kernel",
        "/proc/sys/vm",
        "/proc/sys/net/core",
        "/sys/fs/cgroup",
        "/sys/class/drm",
        "/run/zenith"
    };

    std::string raw_str = absolute_path.string();
    if (raw_str.find('\0') != std::string::npos || raw_str.find("..") != std::string::npos) {
        return { "", "" };
    }

    std::string abs_str = absolute_path.lexically_normal().string();
    if (abs_str.find('\0') != std::string::npos || abs_str.find("..") != std::string::npos) {
        return { "", "" };
    }

    for (const auto& base : TRUSTED_BASES) {
        std::string base_str = base.string();
        if (abs_str == base_str || abs_str.starts_with(base_str + "/")) {
            std::string sub = abs_str.substr(base_str.size());
            while (!sub.empty() && (sub.front() == '/' || sub.front() == '\\')) {
                sub.erase(0, 1);
            }
            return { base, sub };
        }
    }

    // Reject untrusted paths instead of falling back to an arbitrary base.
    return { "", "" };
}

int SecureIo::open_beneath(const fs::path& base_dir,
                           const std::string& relative_path,
                           int flags,
                           mode_t mode) {
    if (relative_path.empty() || relative_path.find('\0') != std::string::npos ||
        relative_path.find("..") != std::string::npos ||
        relative_path.front() == '/' || relative_path.front() == '\\') {
        ZENITH_LOG_ERROR("Security violation: unsafe relative path rejected: " + relative_path);
        return -1;
    }

#ifdef __linux__
    int dirfd = ::open(base_dir.string().c_str(), O_PATH | O_DIRECTORY | O_CLOEXEC);
    if (dirfd < 0) {
        dirfd = ::open(base_dir.string().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (dirfd < 0) {
            return -1;
        }
    }

    int target_fd = -1;

#ifdef SYS_openat2
    zenith_open_how how{};
    how.flags = static_cast<uint64_t>(flags | O_CLOEXEC);
    how.mode = static_cast<uint64_t>(mode);
    how.resolve = RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS;

    target_fd = static_cast<int>(::syscall(SYS_openat2, dirfd, relative_path.c_str(), &how, sizeof(how)));
#endif

    if (target_fd < 0) {
        // Fallback for kernels without openat2 (< 5.6)
        target_fd = ::openat(dirfd, relative_path.c_str(), flags | O_NOFOLLOW | O_CLOEXEC, mode);
    }

    ::close(dirfd);
    return target_fd;
#else
    fs::path full = base_dir / relative_path;
    int fd = -1;
    if (flags & O_WRONLY) {
        fd = ::open(full.string().c_str(), flags, mode);
    } else {
        fd = ::open(full.string().c_str(), O_RDONLY);
    }
    return fd;
#endif
}

bool SecureIo::write_string(const fs::path& base_dir,
                            const std::string& relative_path,
                            std::string_view content) {
#ifdef __linux__
    int fd = open_beneath(base_dir, relative_path, O_WRONLY | O_TRUNC);
    if (fd < 0) {
        fd = open_beneath(base_dir, relative_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) return false;
    }

    ssize_t written = ::write(fd, content.data(), content.size());
    ::close(fd);
    return written == static_cast<ssize_t>(content.size());
#else
    return write_file_string(base_dir / relative_path, content);
#endif
}

std::optional<std::string> SecureIo::read_string(const fs::path& base_dir,
                                                 const std::string& relative_path) {
#ifdef __linux__
    int fd = open_beneath(base_dir, relative_path, O_RDONLY);
    if (fd < 0) return std::nullopt;

    std::string result;
    char buf[1024];
    ssize_t bytes_read = 0;
    while ((bytes_read = ::read(fd, buf, sizeof(buf))) > 0) {
        result.append(buf, bytes_read);
    }
    ::close(fd);
    return trim(result);
#else
    return read_file_string(base_dir / relative_path);
#endif
}

bool SecureIo::safe_write_file(const fs::path& path, std::string_view content) {
    auto [base, sub] = split_trusted_path(path);
    if (base.empty() || sub.empty()) return false;
    return write_string(base, sub, content);
}

std::optional<std::string> SecureIo::safe_read_file(const fs::path& path) {
    auto [base, sub] = split_trusted_path(path);
    if (base.empty() || sub.empty()) return std::nullopt;
    return read_string(base, sub);
}

} // namespace zenith
