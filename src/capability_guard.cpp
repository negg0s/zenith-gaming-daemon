// SPDX-License-Identifier: MIT
#include "zenith/capability_guard.hpp"

#ifdef __linux__
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <linux/capability.h>

#ifndef PR_SET_NO_NEW_PRIVS
#define PR_SET_NO_NEW_PRIVS 38
#endif
#ifndef PR_GET_NO_NEW_PRIVS
#define PR_GET_NO_NEW_PRIVS 39
#endif
#ifndef PR_CAPBSET_DROP
#define PR_CAPBSET_DROP 24
#endif
#endif

namespace zenith {

bool CapabilityGuard::drop_unneeded_capabilities() {
#ifdef __linux__
    // Prevent child processes or exec calls from obtaining new privileges
    if (::prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        ZENITH_LOG_WARN("Failed to set PR_SET_NO_NEW_PRIVS");
        return false;
    }

    // Best-effort: drop dangerous capabilities from the bounding set when running as root.
    const int caps_to_drop[] = {
#ifdef CAP_SYS_MODULE
        CAP_SYS_MODULE,
#endif
#ifdef CAP_SYS_RAWIO
        CAP_SYS_RAWIO,
#endif
#ifdef CAP_SYS_PTRACE
        CAP_SYS_PTRACE,
#endif
#ifdef CAP_SYS_BOOT
        CAP_SYS_BOOT,
#endif
#ifdef CAP_SYS_TIME
        CAP_SYS_TIME,
#endif
#ifdef CAP_MKNOD
        CAP_MKNOD,
#endif
    };
    for (int cap : caps_to_drop) {
        ::prctl(PR_CAPBSET_DROP, cap, 0, 0, 0);
    }

    ZENITH_LOG_INFO("Security sandbox: PR_SET_NO_NEW_PRIVS enabled.");
    return true;
#else
    return true;
#endif
}

bool CapabilityGuard::is_hardened() {
#ifdef __linux__
    int ret = ::prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0);
    return ret == 1;
#else
    return true;
#endif
}

} // namespace zenith
