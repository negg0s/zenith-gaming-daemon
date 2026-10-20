#include "zenith/capability_guard.hpp"

#ifdef __linux__
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef PR_SET_NO_NEW_PRIVS
#define PR_SET_NO_NEW_PRIVS 38
#endif
#ifndef PR_GET_NO_NEW_PRIVS
#define PR_GET_NO_NEW_PRIVS 39
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
