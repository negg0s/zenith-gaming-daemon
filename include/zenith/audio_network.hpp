// SPDX-License-Identifier: MIT
#pragma once

#include "zenith/common.hpp"

namespace zenith {

class AudioNetworkTuner {
public:
    static bool apply_pipewire_quantum(int numerator = 64, int denominator = 48000);
    static bool optimize_network_qdisc();
    static void export_environment_hints(int numerator, int denominator);
};

} // namespace zenith
