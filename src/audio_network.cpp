// SPDX-License-Identifier: MIT
#include "zenith/audio_network.hpp"
#include "zenith/secure_process.hpp"

namespace zenith {

bool AudioNetworkTuner::apply_pipewire_quantum(int numerator, int denominator) {
    std::string quantum = std::to_string(numerator) + "/" + std::to_string(denominator);
    // Export environment for wrapped commands
    export_environment_hints(numerator, denominator);

    // Call whitelisted pw-metadata binary directly via fork+execve without shell
    auto res = SecureProcess::execute("/usr/bin/pw-metadata",
                                      {"-n", "settings", "0", "clock.force-quantum", std::to_string(numerator)});
    if (res.executed && res.exit_code == 0) {
        ZENITH_LOG_INFO("Configured PipeWire real-time audio quantum: " + quantum);
        return true;
    }
    return false;
}

void AudioNetworkTuner::export_environment_hints(int numerator, int denominator) {
    std::string val = std::to_string(numerator) + "/" + std::to_string(denominator);
#ifdef __linux__
    setenv("PIPEWIRE_LATENCY", val.c_str(), 1);
    setenv("MESA_VK_WSI_PRESENT_MODE", "mailbox", 1);
    setenv("DXVK_ASYNC", "1", 1);
    setenv("RADV_PERFTEST", "aco,sam,dcc,nggc", 1);
#endif
}

bool AudioNetworkTuner::optimize_network_qdisc() {
    auto default_qdisc = read_file_string("/proc/sys/net/core/default_qdisc");
    if (default_qdisc && *default_qdisc != "cake") {
        if (write_file_string("/proc/sys/net/core/default_qdisc", "cake")) {
            ZENITH_LOG_INFO("Configured CAKE network queueing discipline (anti-bufferbloat).");
            return true;
        }
    }
    return false;
}

} // namespace zenith
