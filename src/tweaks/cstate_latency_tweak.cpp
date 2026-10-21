// SPDX-License-Identifier: MIT
#include "zenith/tweaks/cstate_latency_tweak.hpp"

#include <fcntl.h>
#include <sys/stat.h>

#ifdef __linux__
#include <unistd.h>
#endif

namespace zenith {

CstateLatencyTweak::CstateLatencyTweak(fs::path dev_path)
    : dev_path_(std::move(dev_path)) {}

CstateLatencyTweak::~CstateLatencyTweak() {
    revert();
}

bool CstateLatencyTweak::probe() {
    std::error_code ec;
    return fs::exists(dev_path_, ec);
}

bool CstateLatencyTweak::apply(Profile profile) {
    if (fd_ >= 0) {
        revert();
    }

    target_latency_us_ = (profile == Profile::Aggressive) ? 0 : 50;

#ifdef __linux__
    int fd = ::open(dev_path_.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }

    int32_t val = target_latency_us_;
    ssize_t bytes_written = ::write(fd, &val, sizeof(val));
    if (bytes_written != static_cast<ssize_t>(sizeof(val))) {
        ::close(fd);
        return false;
    }

    fd_ = fd;
    return true;
#else
    fd_ = 9999;
    return true;
#endif
}

bool CstateLatencyTweak::revert() {
    if (fd_ >= 0) {
#ifdef __linux__
        ::close(fd_);
#endif
        fd_ = -1;
        target_latency_us_ = -1;
        return true;
    }
    return false;
}

std::string CstateLatencyTweak::describe() const {
    return "Sets maximum allowable CPU DMA latency to prevent deep C-state wake-up penalties";
}

std::string CstateLatencyTweak::explain() const {
    return "Writing a target microsecond constraint to /dev/cpu_dma_latency holds a PM QoS request "
           "preventing the kernel cpuidle governor from placing CPU cores into deep sleep states (C6/C8). "
           "Deep sleep exit latency can reach 50 to 150 microseconds, creating noticeable frame-time jitter "
           "when render threads resume execution. Closing the file descriptor immediately releases the request.";
}

std::string CstateLatencyTweak::get_current_value() const {
    if (fd_ >= 0) {
        return std::to_string(target_latency_us_) + " us (active constraint)";
    }
    return "unconstrained (idle defaults)";
}

std::string CstateLatencyTweak::get_target_value(Profile profile) const {
    if (profile == Profile::Aggressive) return "0 us (lock C0/C1)";
    if (profile == Profile::Balanced) return "50 us (limit C6/C8)";
    return "unconstrained";
}

} // namespace zenith
