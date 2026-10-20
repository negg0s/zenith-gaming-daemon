#include "zenith/latency_controller.hpp"

namespace zenith {

LatencyController::LatencyController(
    std::unique_ptr<IrqTuner> irq,
    std::unique_ptr<NetworkTuner> network,
    std::unique_ptr<PipewireTuner> pipewire,
    std::unique_ptr<GpuController> gpu
) {
    irq_ = irq ? std::move(irq) : std::make_unique<IrqTuner>();
    network_ = network ? std::move(network) : std::make_unique<NetworkTuner>();
    pipewire_ = pipewire ? std::move(pipewire) : std::make_unique<PipewireTuner>();
    gpu_ = gpu ? std::move(gpu) : std::make_unique<GpuController>();
}

LatencyController::~LatencyController() {
    rollback_all();
}

LatencyReport LatencyController::apply_all(
    const std::vector<int>& gaming_cores,
    const std::vector<int>& housekeeping_cores,
    int audio_quantum
) {
    LatencyReport report;

    // 1. IRQ Steering (Independent)
    try {
        if (irq_->apply(gaming_cores, housekeeping_cores)) {
            report.irq_applied = true;
            report.notices.push_back("IRQ steering active");
        }
    } catch (const std::exception& e) {
        report.notices.push_back(std::string("IRQ steering failed: ") + e.what());
    }

    // 2. Network QoS (Independent)
    try {
        if (network_->apply()) {
            report.network_applied = true;
            report.notices.push_back("Network QoS active");
        }
    } catch (const std::exception& e) {
        report.notices.push_back(std::string("Network tuning failed: ") + e.what());
    }

    // 3. Audio PipeWire (Independent)
    try {
        if (pipewire_->apply(audio_quantum)) {
            report.pipewire_applied = true;
            report.notices.push_back("PipeWire low-latency quantum active");
        }
    } catch (const std::exception& e) {
        report.notices.push_back(std::string("PipeWire tuning failed: ") + e.what());
    }

    // 4. GPU Clocks & DPM (Independent)
    try {
        if (gpu_->lock_clocks()) {
            report.gpu_applied = true;
            report.notices.push_back("GPU clock locking active");
        } else {
            if (gpu_->thermal_throttle_guard_active()) {
                report.notices.push_back("GPU clock locking bypassed: Thermal Guard limit reached (>=85C)");
            }
            if (gpu_->battery_guard_active()) {
                report.notices.push_back("GPU clock locking bypassed: Battery Guard active (on battery power)");
            }
        }
    } catch (const std::exception& e) {
        report.notices.push_back(std::string("GPU tuning failed: ") + e.what());
    }

    return report;
}

void LatencyController::rollback_all() {
    try { if (irq_) irq_->revert(); } catch (...) {}
    try { if (network_) network_->revert(); } catch (...) {}
    try { if (pipewire_) pipewire_->revert(); } catch (...) {}
    try { if (gpu_) gpu_->rollback(); } catch (...) {}
}

} // namespace zenith
