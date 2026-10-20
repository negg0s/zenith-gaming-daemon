// SPDX-License-Identifier: MIT
#pragma once

#include "zenith/irq_tuner.hpp"
#include "zenith/network_tuner.hpp"
#include "zenith/pipewire_tuner.hpp"
#include "zenith/gpu_controller.hpp"

namespace zenith {

struct LatencyReport {
    bool irq_applied = false;
    bool network_applied = false;
    bool pipewire_applied = false;
    bool gpu_applied = false;
    std::vector<std::string> notices;
};

class LatencyController {
public:
    LatencyController(
        std::unique_ptr<IrqTuner> irq = nullptr,
        std::unique_ptr<NetworkTuner> network = nullptr,
        std::unique_ptr<PipewireTuner> pipewire = nullptr,
        std::unique_ptr<GpuController> gpu = nullptr
    );
    ~LatencyController();

    LatencyReport apply_all(
        const std::vector<int>& gaming_cores,
        const std::vector<int>& housekeeping_cores,
        int audio_quantum = 64
    );

    void rollback_all();

    [[nodiscard]] IrqTuner& irq() { return *irq_; }
    [[nodiscard]] NetworkTuner& network() { return *network_; }
    [[nodiscard]] PipewireTuner& pipewire() { return *pipewire_; }
    [[nodiscard]] GpuController& gpu() { return *gpu_; }

private:
    std::unique_ptr<IrqTuner> irq_;
    std::unique_ptr<NetworkTuner> network_;
    std::unique_ptr<PipewireTuner> pipewire_;
    std::unique_ptr<GpuController> gpu_;
};

} // namespace zenith
