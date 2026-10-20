#include "zenith/gpu_controller.hpp"
#include <cstdlib>

namespace zenith {

GpuController::GpuController() {
    detect_gpus();
}

GpuController::~GpuController() {
    rollback();
}

bool GpuController::detect_gpus() {
    gpus_.clear();

    // Check AMD sysfs DRM devices
    fs::path drm_root = "/sys/class/drm";
    if (fs::exists(drm_root)) {
        for (const auto& entry : fs::directory_iterator(drm_root)) {
            std::string name = entry.path().filename().string();
            if (name.starts_with("card") && name.find('-') == std::string::npos) {
                auto dpm_file = entry.path() / "device" / "power_dpm_force_performance_level";
                if (fs::exists(dpm_file)) {
                    GpuDevice dev;
                    dev.vendor = GpuVendor::Amd;
                    dev.name = name;
                    dev.sysfs_card_path = entry.path();
                    if (auto cur = read_file_string(dpm_file)) {
                        dev.original_dpm_level = *cur;
                    }
                    gpus_.push_back(dev);
                    ZENITH_LOG_INFO("Detected AMD GPU: " + name + " (DPM level: " + dev.original_dpm_level + ")");
                }
            }
        }
    }

    // Check NVIDIA via nvidia-smi
    if (std::system("which nvidia-smi >/dev/null 2>&1") == 0) {
        GpuDevice dev;
        dev.vendor = GpuVendor::Nvidia;
        dev.name = "NVIDIA GPU (nvidia-smi)";
        gpus_.push_back(dev);
        ZENITH_LOG_INFO("Detected NVIDIA GPU via driver CLI.");
    }

    return !gpus_.empty();
}

bool GpuController::lock_amd_gpu(GpuDevice& gpu) {
    auto dpm_file = gpu.sysfs_card_path / "device" / "power_dpm_force_performance_level";
    if (fs::exists(dpm_file)) {
        if (write_file_string(dpm_file, "high")) {
            gpu.clock_locked = true;
            ZENITH_LOG_INFO("Locked AMD GPU " + gpu.name + " to performance level: high");
            return true;
        }
    }
    return false;
}

bool GpuController::lock_nvidia_gpu(GpuDevice& gpu) {
    // Set persistence mode on
    int ret1 = std::system("nvidia-smi -pm 1 >/dev/null 2>&1");
    // Lock compute/graphics clocks to max if supported
    int ret2 = std::system("nvidia-smi --auto-boost-permission=0 >/dev/null 2>&1");
    gpu.clock_locked = (ret1 == 0 || ret2 == 0);
    if (gpu.clock_locked) {
        ZENITH_LOG_INFO("Configured NVIDIA GPU persistence mode and boost permissions.");
    }
    return gpu.clock_locked;
}

void GpuController::rollback_amd_gpu(GpuDevice& gpu) {
    if (gpu.clock_locked) {
        auto dpm_file = gpu.sysfs_card_path / "device" / "power_dpm_force_performance_level";
        std::string reset_level = gpu.original_dpm_level.empty() ? "auto" : gpu.original_dpm_level;
        write_file_string(dpm_file, reset_level);
        ZENITH_LOG_DEBUG("Restored AMD GPU " + gpu.name + " DPM level to " + reset_level);
        gpu.clock_locked = false;
    }
}

void GpuController::rollback_nvidia_gpu(GpuDevice& gpu) {
    if (gpu.clock_locked) {
        (void)std::system("nvidia-smi -rgc >/dev/null 2>&1");
        (void)std::system("nvidia-smi -pm 0 >/dev/null 2>&1");
        ZENITH_LOG_DEBUG("Reset NVIDIA GPU clocks.");
        gpu.clock_locked = false;
    }
}

bool GpuController::lock_clocks() {
    bool ok = false;
    for (auto& gpu : gpus_) {
        if (gpu.vendor == GpuVendor::Amd) ok |= lock_amd_gpu(gpu);
        else if (gpu.vendor == GpuVendor::Nvidia) ok |= lock_nvidia_gpu(gpu);
    }
    active_ = ok;
    return active_;
}

void GpuController::rollback() {
    if (!active_) return;
    ZENITH_LOG_INFO("Restoring original GPU power and clock states...");
    for (auto& gpu : gpus_) {
        if (gpu.vendor == GpuVendor::Amd) rollback_amd_gpu(gpu);
        else if (gpu.vendor == GpuVendor::Nvidia) rollback_nvidia_gpu(gpu);
    }
    active_ = false;
}

} // namespace zenith
