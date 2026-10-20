#include "zenith/audio_network.hpp"
#include <cstdlib>

namespace zenith {

bool AudioNetworkTuner::apply_pipewire_quantum(int numerator, int denominator) {
    std::string quantum = std::to_string(numerator) + "/" + std::to_string(denominator);
    // Export environment for wrapped commands
    export_environment_hints(numerator, denominator);

    // If pw-metadata CLI is available, dynamically request the low quantum
    std::string cmd = "pw-metadata -n settings 0 clock.force-quantum " + std::to_string(numerator) + " >/dev/null 2>&1";
    int ret = std::system(cmd.c_str());
    if (ret == 0) {
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
    // Attempt CAKE or FQ_CODEL qdisc via tc / sysctl
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
