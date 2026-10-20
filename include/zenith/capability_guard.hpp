// SPDX-License-Identifier: MIT
#pragma once

#include "zenith/common.hpp"

namespace zenith {

class CapabilityGuard {
public:
    // Enforces prctl(PR_SET_NO_NEW_PRIVS) and drops all unneeded capabilities
    static bool drop_unneeded_capabilities();
    static bool is_hardened();
};

} // namespace zenith
