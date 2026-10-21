// SPDX-License-Identifier: MIT
#pragma once

#include "zenith/tweaks/tweak_interface.hpp"

namespace zenith {

class CstateLatencyTweak : public ITweak {
public:
    explicit CstateLatencyTweak(fs::path dev_path = "/dev/cpu_dma_latency");
    ~CstateLatencyTweak() override;

    std::string name() const override { return "cstate_latency"; }
    std::string description() const override { return "CPU DMA Latency QoS constraint (/dev/cpu_dma_latency)"; }
    uint32_t supported_profiles() const override {
        return ProfileMask::Performance; // Balanced and Aggressive
    }

    bool probe() override;
    bool apply(Profile profile) override;
    bool revert() override;
    std::string describe() const override;
    std::string explain() const override;

    std::string get_current_value() const override;
    std::string get_target_value(Profile profile) const override;

private:
    fs::path dev_path_;
    int fd_ = -1;
    int32_t target_latency_us_ = -1;
};

} // namespace zenith
