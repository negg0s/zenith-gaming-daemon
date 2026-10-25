// SPDX-License-Identifier: MIT
#include "zenith/gpu_controller.hpp"
#include "zenith/secure_process.hpp"

#ifdef __linux__
#include <dlfcn.h>
#include <cstdlib>
#endif

namespace zenith {

// ================= RealNvmlDriver Implementation =================

RealNvmlDriver::RealNvmlDriver() {
#ifdef __linux__
    const char* nvml_paths[] = {
        "/usr/lib/x86_64-linux-gnu/libnvidia-ml.so.1",
        "/usr/lib64/libnvidia-ml.so.1",
        "/usr/lib/libnvidia-ml.so.1",
        "/usr/lib/x86_64-linux-gnu/libnvidia-ml.so",
        "/usr/lib64/libnvidia-ml.so",
        "/usr/lib/libnvidia-ml.so"
    };
    for (const char* path : nvml_paths) {
        lib_handle_ = dlopen(path, RTLD_NOW | RTLD_NOLOAD);
        if (!lib_handle_) lib_handle_ = dlopen(path, RTLD_NOW);
        if (lib_handle_) break;
    }
    if (!lib_handle_) {
        // Fall back to the system loader only if no absolute path matched.
        lib_handle_ = dlopen("libnvidia-ml.so.1", RTLD_NOW);
    }

    if (lib_handle_) {
        nvmlInit_ = reinterpret_cast<nvmlInit_t>(dlsym(lib_handle_, "nvmlInit_v2"));
        if (!nvmlInit_) nvmlInit_ = reinterpret_cast<nvmlInit_t>(dlsym(lib_handle_, "nvmlInit"));

        nvmlShutdown_ = reinterpret_cast<nvmlShutdown_t>(dlsym(lib_handle_, "nvmlShutdown"));
        nvmlDeviceGetCount_ = reinterpret_cast<nvmlDeviceGetCount_t>(dlsym(lib_handle_, "nvmlDeviceGetCount_v2"));
        if (!nvmlDeviceGetCount_) nvmlDeviceGetCount_ = reinterpret_cast<nvmlDeviceGetCount_t>(dlsym(lib_handle_, "nvmlDeviceGetCount"));

        nvmlDeviceGetHandleByIndex_ = reinterpret_cast<nvmlDeviceGetHandleByIndex_t>(dlsym(lib_handle_, "nvmlDeviceGetHandleByIndex_v2"));
        if (!nvmlDeviceGetHandleByIndex_) nvmlDeviceGetHandleByIndex_ = reinterpret_cast<nvmlDeviceGetHandleByIndex_t>(dlsym(lib_handle_, "nvmlDeviceGetHandleByIndex"));

        nvmlDeviceGetTemperature_ = reinterpret_cast<nvmlDeviceGetTemperature_t>(dlsym(lib_handle_, "nvmlDeviceGetTemperature"));
        nvmlDeviceGetPowerManagementLimit_ = reinterpret_cast<nvmlDeviceGetPowerManagementLimit_t>(dlsym(lib_handle_, "nvmlDeviceGetPowerManagementLimit"));
        nvmlDeviceGetPowerManagementLimitConstraints_ = reinterpret_cast<nvmlDeviceGetPowerManagementLimitConstraints_t>(dlsym(lib_handle_, "nvmlDeviceGetPowerManagementLimitConstraints"));
        nvmlDeviceSetPersistenceMode_ = reinterpret_cast<nvmlDeviceSetPersistenceMode_t>(dlsym(lib_handle_, "nvmlDeviceSetPersistenceMode"));
        nvmlDeviceSetGpuLockedClocks_ = reinterpret_cast<nvmlDeviceSetGpuLockedClocks_t>(dlsym(lib_handle_, "nvmlDeviceSetGpuLockedClocks"));
        nvmlDeviceResetGpuLockedClocks_ = reinterpret_cast<nvmlDeviceResetGpuLockedClocks_t>(dlsym(lib_handle_, "nvmlDeviceResetGpuLockedClocks"));
        nvmlDeviceSetMemoryLockedClocks_ = reinterpret_cast<nvmlDeviceSetMemoryLockedClocks_t>(dlsym(lib_handle_, "nvmlDeviceSetMemoryLockedClocks"));
        nvmlDeviceResetMemoryLockedClocks_ = reinterpret_cast<nvmlDeviceResetMemoryLockedClocks_t>(dlsym(lib_handle_, "nvmlDeviceResetMemoryLockedClocks"));
        nvmlDeviceSetPowerManagementLimit_ = reinterpret_cast<nvmlDeviceSetPowerManagementLimit_t>(dlsym(lib_handle_, "nvmlDeviceSetPowerManagementLimit"));

        if (nvmlInit_ && nvmlInit_() == 0) {
            loaded_ = true;
            ZENITH_LOG_INFO("Successfully initialized native NVML driver library.");
        }
    }
#endif
}

RealNvmlDriver::~RealNvmlDriver() {
#ifdef __linux__
    if (loaded_ && nvmlShutdown_) {
        nvmlShutdown_();
    }
    if (lib_handle_) {
        dlclose(lib_handle_);
    }
#endif
}

unsigned int RealNvmlDriver::get_device_count() {
    if (!loaded_ || !nvmlDeviceGetCount_) return 0;
    unsigned int count = 0;
    if (nvmlDeviceGetCount_(&count) == 0) return count;
    return 0;
}

NvmlDeviceHandle RealNvmlDriver::get_device_handle(unsigned int index) {
    if (!loaded_ || !nvmlDeviceGetHandleByIndex_) return nullptr;
    NvmlDeviceHandle handle = nullptr;
    if (nvmlDeviceGetHandleByIndex_(index, &handle) == 0) return handle;
    return nullptr;
}

unsigned int RealNvmlDriver::get_temperature(NvmlDeviceHandle dev) {
    if (!loaded_ || !nvmlDeviceGetTemperature_ || !dev) return 0;
    unsigned int temp = 0;
    if (nvmlDeviceGetTemperature_(dev, 0 /* NVML_TEMPERATURE_GPU */, &temp) == 0) {
        return temp;
    }
    return 0;
}

unsigned int RealNvmlDriver::get_power_limit(NvmlDeviceHandle dev) {
    if (!loaded_ || !nvmlDeviceGetPowerManagementLimit_ || !dev) return 0;
    unsigned int limit = 0;
    if (nvmlDeviceGetPowerManagementLimit_(dev, &limit) == 0) {
        return limit;
    }
    return 0;
}

unsigned int RealNvmlDriver::get_max_power_limit(NvmlDeviceHandle dev) {
    if (!loaded_ || !nvmlDeviceGetPowerManagementLimitConstraints_ || !dev) return 0;
    unsigned int min_limit = 0;
    unsigned int max_limit = 0;
    if (nvmlDeviceGetPowerManagementLimitConstraints_(dev, &min_limit, &max_limit) == 0) {
        return max_limit;
    }
    return 0;
}

bool RealNvmlDriver::set_persistence_mode(NvmlDeviceHandle dev, bool enable) {
    if (!loaded_ || !nvmlDeviceSetPersistenceMode_ || !dev) return false;
    return nvmlDeviceSetPersistenceMode_(dev, enable ? 1 : 0) == 0;
}

bool RealNvmlDriver::set_gpu_locked_clocks(NvmlDeviceHandle dev, unsigned int min_mhz, unsigned int max_mhz) {
    if (!loaded_ || !nvmlDeviceSetGpuLockedClocks_ || !dev) return false;
    return nvmlDeviceSetGpuLockedClocks_(dev, min_mhz, max_mhz) == 0;
}

bool RealNvmlDriver::reset_gpu_locked_clocks(NvmlDeviceHandle dev) {
    if (!loaded_ || !nvmlDeviceResetGpuLockedClocks_ || !dev) return false;
    return nvmlDeviceResetGpuLockedClocks_(dev) == 0;
}

bool RealNvmlDriver::set_memory_locked_clocks(NvmlDeviceHandle dev, unsigned int min_mhz, unsigned int max_mhz) {
    if (!loaded_ || !nvmlDeviceSetMemoryLockedClocks_ || !dev) return false;
    return nvmlDeviceSetMemoryLockedClocks_(dev, min_mhz, max_mhz) == 0;
}

bool RealNvmlDriver::reset_memory_locked_clocks(NvmlDeviceHandle dev) {
    if (!loaded_ || !nvmlDeviceResetMemoryLockedClocks_ || !dev) return false;
    return nvmlDeviceResetMemoryLockedClocks_(dev) == 0;
}

bool RealNvmlDriver::set_power_limit(NvmlDeviceHandle dev, unsigned int milliwatts) {
    if (!loaded_ || !nvmlDeviceSetPowerManagementLimit_ || !dev) return false;
    return nvmlDeviceSetPowerManagementLimit_(dev, milliwatts) == 0;
}

// ================= GpuController Implementation =================

GpuController::GpuController(IGpuFs* fs, std::shared_ptr<INvmlDriver> nvml)
    : fs_(fs ? fs : &RealGpuFs::instance()),
      nvml_(nvml ? std::move(nvml) : std::make_shared<RealNvmlDriver>()) {
    detect_gpus();
}

GpuController::~GpuController() {
    rollback();
}

void GpuController::apply_driver_environment_hints() {
#ifdef __linux__
    ::setenv("MESA_SHADER_CACHE_MAX_SIZE", "10G", 0);
    ::setenv("RADV_PERFTEST", "aco,sam,dcc", 0);
    ::setenv("AMD_VULKAN_ICD", "RADV", 0);
    ::setenv("mesa_glthread", "true", 0);
    ::setenv("VKD3D_CONFIG", "dxr11,dxr", 0);
    ::setenv("__GL_THREADED_OPTIMIZATIONS", "1", 0);
    ::setenv("__GL_SYNC_TO_VBLANK", "0", 0);
#endif
    ZENITH_LOG_INFO("Applied high-performance Vulkan/Mesa graphics driver environment hints");
}

bool GpuController::is_running_on_battery() const {
    fs::path power_supply_dir("/sys/class/power_supply");
    if (!fs_->exists(power_supply_dir)) return false;

    auto entries = fs_->list_directory(power_supply_dir);
    for (const auto& entry : entries) {
        auto type_file = entry / "type";
        auto status_file = entry / "status";

        auto type = fs_->read_file(type_file);
        auto status = fs_->read_file(status_file);

        if (type && trim(*type) == "Battery") {
            if (status && trim(*status) == "Discharging") {
                return true;
            }
        }
    }
    return false;
}

uint32_t GpuController::read_sysfs_temperature(const fs::path& card_path) const {
    fs::path hwmon_dir = card_path / "device" / "hwmon";
    if (!fs_->exists(hwmon_dir)) {
        hwmon_dir = card_path / "hwmon";
    }

    if (fs_->exists(hwmon_dir)) {
        auto hwmons = fs_->list_directory(hwmon_dir);
        for (const auto& h : hwmons) {
            auto temp_file = h / "temp1_input";
            if (fs_->exists(temp_file)) {
                if (auto val = fs_->read_file(temp_file)) {
                    try {
                        uint32_t millideg = std::stoul(trim(*val));
                        return millideg / 1000;
                    } catch (...) {}
                }
            }
        }
    }
    return 0;
}

bool GpuController::detect_gpus() {
    gpus_.clear();

    // 1. Scan sysfs DRM cards
    fs::path drm_dir("/sys/class/drm");
    if (fs_->exists(drm_dir)) {
        auto entries = fs_->list_directory(drm_dir);
        for (const auto& card_path : entries) {
            std::string name = card_path.filename().string();
            if (!name.starts_with("card") || name.find('-') != std::string::npos) {
                continue;
            }

            // Check AMDGPU
            auto dpm_file = card_path / "device" / "power_dpm_force_performance_level";
            if (fs_->exists(dpm_file)) {
                GpuDevice dev;
                dev.vendor = GpuVendor::Amd;
                dev.name = name;
                dev.sysfs_card_path = card_path;
                if (auto cur = fs_->read_file(dpm_file)) {
                    dev.original_dpm_level = trim(*cur);
                }

                auto pp_file = card_path / "device" / "pp_power_profile_mode";
                if (fs_->exists(pp_file)) {
                    if (auto cur = fs_->read_file(pp_file)) {
                        dev.original_power_profile_mode = trim(*cur);
                    }
                }

                dev.temperature_c = read_sysfs_temperature(card_path);
                gpus_.push_back(dev);
                ZENITH_LOG_INFO("Detected AMD GPU: " + name + " (DPM: " + dev.original_dpm_level +
                                ", Temp: " + std::to_string(dev.temperature_c) + "C)");
                continue;
            }

            // Check Intel Arc / iGPU
            auto intel_freq_file = card_path / "device" / "drm" / name / "gt_min_freq_mhz";
            if (!fs_->exists(intel_freq_file)) {
                intel_freq_file = card_path / "gt_min_freq_mhz";
            }
            if (fs_->exists(intel_freq_file)) {
                GpuDevice dev;
                dev.vendor = GpuVendor::Intel;
                dev.name = name;
                dev.sysfs_card_path = card_path;
                if (auto cur = fs_->read_file(intel_freq_file)) {
                    try { dev.original_intel_min_freq = std::stoul(trim(*cur)); } catch (...) {}
                }
                dev.temperature_c = read_sysfs_temperature(card_path);
                gpus_.push_back(dev);
                ZENITH_LOG_INFO("Detected Intel GPU: " + name + " (Min Freq: " +
                                std::to_string(dev.original_intel_min_freq) + "MHz)");
            }
        }
    }

    // 2. Check NVIDIA via NVML or fallback binary
    if (nvml_->is_available() && nvml_->get_device_count() > 0) {
        GpuDevice dev;
        dev.vendor = GpuVendor::Nvidia;
        dev.name = "NVIDIA GPU (Native NVML)";
        auto handle = nvml_->get_device_handle(0);
        dev.temperature_c = nvml_->get_temperature(handle);
        dev.original_nvidia_power_limit = nvml_->get_power_limit(handle);
        gpus_.push_back(dev);
        ZENITH_LOG_INFO("Detected NVIDIA GPU via native NVML (Temp: " + std::to_string(dev.temperature_c) + "C).");
    } else if (fs::exists("/usr/bin/nvidia-smi")) {
        GpuDevice dev;
        dev.vendor = GpuVendor::Nvidia;
        dev.name = "NVIDIA GPU (/usr/bin/nvidia-smi)";
        gpus_.push_back(dev);
        ZENITH_LOG_INFO("Detected NVIDIA GPU via fallback CLI binary.");
    }

    return !gpus_.empty();
}

bool GpuController::lock_amd_gpu(GpuDevice& gpu, Profile profile) {
    uint32_t temp = read_sysfs_temperature(gpu.sysfs_card_path);
    if (temp >= MAX_SAFE_TEMP_C) {
        thermal_guard_active_ = true;
        ZENITH_LOG_WARN("AMD GPU " + gpu.name + " temperature (" + std::to_string(temp) +
                        "C) exceeds safety limit. Aborting clock lock.");
        return false;
    }

    // 1. Force performance level high
    auto dpm_file = gpu.sysfs_card_path / "device" / "power_dpm_force_performance_level";
    if (fs_->exists(dpm_file)) {
        if (fs_->write_file(dpm_file, "high\n")) {
            gpu.clock_locked = true;
            ZENITH_LOG_INFO("Locked AMD GPU " + gpu.name + " to performance level: high");
        }
    }

    // 2. AMD PowerPlay 3D Full Screen Profile mode (mode 1)
    auto pp_file = gpu.sysfs_card_path / "device" / "pp_power_profile_mode";
    if (fs_->exists(pp_file)) {
        if (fs_->write_file(pp_file, "1\n")) {
            gpu.power_profile_active = true;
            ZENITH_LOG_INFO("Configured AMD PowerPlay profile: 3D_FULL_SCREEN (mode 1)");
        }
    }

    // 3. In Ultra profile: boost power cap to power1_cap_max if supported
    if (profile == Profile::Ultra) {
        fs::path hwmon_dir = gpu.sysfs_card_path / "device" / "hwmon";
        if (!fs_->exists(hwmon_dir)) hwmon_dir = gpu.sysfs_card_path / "hwmon";
        if (fs_->exists(hwmon_dir)) {
            for (const auto& h : fs_->list_directory(hwmon_dir)) {
                auto cap_file = h / "power1_cap";
                auto max_file = h / "power1_cap_max";
                if (fs_->exists(cap_file) && fs_->exists(max_file)) {
                    auto cap_str = fs_->read_file(cap_file);
                    auto max_str = fs_->read_file(max_file);
                    if (cap_str && max_str) {
                        try {
                            uint32_t cur = std::stoul(trim(*cap_str));
                            uint32_t max_p = std::stoul(trim(*max_str));
                            if (max_p > cur) {
                                gpu.original_power_cap = cur;
                                fs_->write_file(cap_file, std::to_string(max_p) + "\n");
                                gpu.power_cap_boosted = true;
                                ZENITH_LOG_INFO("Boosted AMD GPU power cap to max limit (" + std::to_string(max_p / 1000000) + "W)");
                            }
                        } catch (...) {}
                    }
                }
            }
        }
    }

    return gpu.clock_locked;
}

bool GpuController::lock_nvidia_gpu(GpuDevice& gpu, Profile profile) {
    if (nvml_->is_available()) {
        auto handle = nvml_->get_device_handle(0);
        uint32_t temp = nvml_->get_temperature(handle);
        if (temp >= MAX_SAFE_TEMP_C) {
            thermal_guard_active_ = true;
            ZENITH_LOG_WARN("NVIDIA GPU temperature (" + std::to_string(temp) +
                            "C) exceeds safety limit. Aborting clock lock.");
            return false;
        }

        nvml_->set_persistence_mode(handle, true);
        gpu.persistence_mode_enabled = true;

        if (nvml_->set_gpu_locked_clocks(handle, SAFE_NVIDIA_MIN_MHZ, SAFE_NVIDIA_MAX_MHZ)) {
            gpu.clock_locked = true;
            ZENITH_LOG_INFO("Locked NVIDIA GPU core clocks via NVML (" + std::to_string(SAFE_NVIDIA_MIN_MHZ) +
                            "-" + std::to_string(SAFE_NVIDIA_MAX_MHZ) + "MHz)");
        }

        if (profile == Profile::Aggressive || profile == Profile::Ultra) {
            if (nvml_->set_memory_locked_clocks(handle, SAFE_NVIDIA_MEM_MIN_MHZ, SAFE_NVIDIA_MEM_MAX_MHZ)) {
                gpu.memory_clock_locked = true;
                ZENITH_LOG_INFO("Locked NVIDIA GPU memory clocks via NVML (" + std::to_string(SAFE_NVIDIA_MEM_MIN_MHZ) +
                                "-" + std::to_string(SAFE_NVIDIA_MEM_MAX_MHZ) + "MHz)");
            }
        }

        if (profile == Profile::Ultra) {
            unsigned int max_power = nvml_->get_max_power_limit(handle);
            if (max_power > 0) {
                nvml_->set_power_limit(handle, max_power);
                gpu.power_cap_boosted = true;
                ZENITH_LOG_INFO("Boosted NVIDIA power limit to maximum (" + std::to_string(max_power / 1000) + "W)");
            }
        }

        return gpu.clock_locked;
    }

    // Fallback via SecureProcess
    auto res1 = SecureProcess::execute("/usr/bin/nvidia-smi", {"-pm", "1"});
    auto res2 = SecureProcess::execute("/usr/bin/nvidia-smi", {"--auto-boost-permission=0"});
    gpu.clock_locked = (res1.executed && res1.exit_code == 0) || (res2.executed && res2.exit_code == 0);
    return gpu.clock_locked;
}

bool GpuController::lock_intel_gpu(GpuDevice& gpu, Profile) {
    uint32_t temp = read_sysfs_temperature(gpu.sysfs_card_path);
    if (temp >= MAX_SAFE_TEMP_C) {
        thermal_guard_active_ = true;
        ZENITH_LOG_WARN("Intel GPU " + gpu.name + " temperature (" + std::to_string(temp) +
                        "C) exceeds safety limit. Aborting clock lock.");
        return false;
    }

    fs::path boost_file = gpu.sysfs_card_path / "device" / "drm" / gpu.name / "gt_boost_freq_mhz";
    if (!fs_->exists(boost_file)) boost_file = gpu.sysfs_card_path / "gt_boost_freq_mhz";

    fs::path min_file = gpu.sysfs_card_path / "device" / "drm" / gpu.name / "gt_min_freq_mhz";
    if (!fs_->exists(min_file)) min_file = gpu.sysfs_card_path / "gt_min_freq_mhz";

    if (fs_->exists(boost_file) && fs_->exists(min_file)) {
        auto boost_val = fs_->read_file(boost_file);
        if (boost_val) {
            if (fs_->write_file(min_file, *boost_val)) {
                gpu.clock_locked = true;
                ZENITH_LOG_INFO("Locked Intel GPU " + gpu.name + " min freq to boost (" + trim(*boost_val) + "MHz)");
                return true;
            }
        }
    }
    return false;
}

bool GpuController::lock_clocks(Profile profile) {
    if (is_running_on_battery()) {
        battery_guard_active_ = true;
        ZENITH_LOG_WARN("System is running on battery power. Aborting GPU clock lock to preserve power delivery.");
        return false;
    }

    battery_guard_active_ = false;
    thermal_guard_active_ = false;

    apply_driver_environment_hints();

    bool any_locked = false;
    for (auto& gpu : gpus_) {
        switch (gpu.vendor) {
            case GpuVendor::Amd:
                any_locked |= lock_amd_gpu(gpu, profile);
                break;
            case GpuVendor::Nvidia:
                any_locked |= lock_nvidia_gpu(gpu, profile);
                break;
            case GpuVendor::Intel:
                any_locked |= lock_intel_gpu(gpu, profile);
                break;
            case GpuVendor::Unknown:
                break;
        }
    }

    active_ = any_locked;
    return active_;
}

void GpuController::rollback_amd_gpu(GpuDevice& gpu) {
    if (gpu.clock_locked) {
        auto dpm_file = gpu.sysfs_card_path / "device" / "power_dpm_force_performance_level";
        std::string orig = gpu.original_dpm_level.empty() ? "auto" : gpu.original_dpm_level;
        fs_->write_file(dpm_file, orig + "\n");
        gpu.clock_locked = false;
        ZENITH_LOG_INFO("Restored AMD GPU DPM level to: " + orig);
    }

    if (gpu.power_profile_active) {
        auto pp_file = gpu.sysfs_card_path / "device" / "pp_power_profile_mode";
        std::string orig = gpu.original_power_profile_mode.empty() ? "0" : gpu.original_power_profile_mode;
        fs_->write_file(pp_file, orig + "\n");
        gpu.power_profile_active = false;
        ZENITH_LOG_INFO("Restored AMD PowerPlay profile to: " + orig);
    }

    if (gpu.power_cap_boosted && gpu.original_power_cap > 0) {
        fs::path hwmon_dir = gpu.sysfs_card_path / "device" / "hwmon";
        if (!fs_->exists(hwmon_dir)) hwmon_dir = gpu.sysfs_card_path / "hwmon";
        if (fs_->exists(hwmon_dir)) {
            for (const auto& h : fs_->list_directory(hwmon_dir)) {
                auto cap_file = h / "power1_cap";
                if (fs_->exists(cap_file)) {
                    fs_->write_file(cap_file, std::to_string(gpu.original_power_cap) + "\n");
                    ZENITH_LOG_INFO("Restored AMD GPU power cap to: " + std::to_string(gpu.original_power_cap / 1000000) + "W");
                }
            }
        }
        gpu.power_cap_boosted = false;
    }
}

void GpuController::rollback_nvidia_gpu(GpuDevice& gpu) {
    if (nvml_->is_available()) {
        auto handle = nvml_->get_device_handle(0);
        if (gpu.clock_locked) {
            nvml_->reset_gpu_locked_clocks(handle);
            gpu.clock_locked = false;
            ZENITH_LOG_INFO("Reset NVIDIA locked GPU clocks.");
        }
        if (gpu.memory_clock_locked) {
            nvml_->reset_memory_locked_clocks(handle);
            gpu.memory_clock_locked = false;
            ZENITH_LOG_INFO("Reset NVIDIA locked memory clocks.");
        }
        if (gpu.power_cap_boosted && gpu.original_nvidia_power_limit > 0) {
            nvml_->set_power_limit(handle, gpu.original_nvidia_power_limit);
            gpu.power_cap_boosted = false;
            ZENITH_LOG_INFO("Restored NVIDIA power limit.");
        }
        if (gpu.persistence_mode_enabled) {
            nvml_->set_persistence_mode(handle, false);
            gpu.persistence_mode_enabled = false;
            ZENITH_LOG_INFO("Disabled NVIDIA persistence mode.");
        }
        return;
    }

    if (gpu.clock_locked) {
        SecureProcess::execute("/usr/bin/nvidia-smi", {"-pm", "0"});
        gpu.clock_locked = false;
        ZENITH_LOG_INFO("Reset NVIDIA settings via fallback CLI.");
    }
}

void GpuController::rollback_intel_gpu(GpuDevice& gpu) {
    if (gpu.clock_locked) {
        fs::path min_file = gpu.sysfs_card_path / "device" / "drm" / gpu.name / "gt_min_freq_mhz";
        if (!fs_->exists(min_file)) min_file = gpu.sysfs_card_path / "gt_min_freq_mhz";

        if (fs_->exists(min_file)) {
            fs_->write_file(min_file, std::to_string(gpu.original_intel_min_freq) + "\n");
            gpu.clock_locked = false;
            ZENITH_LOG_INFO("Restored Intel GPU min freq to: " + std::to_string(gpu.original_intel_min_freq) + "MHz");
        }
    }
}

void GpuController::rollback() {
    if (!active_) return;

    for (auto& gpu : gpus_) {
        switch (gpu.vendor) {
            case GpuVendor::Amd:
                rollback_amd_gpu(gpu);
                break;
            case GpuVendor::Nvidia:
                rollback_nvidia_gpu(gpu);
                break;
            case GpuVendor::Intel:
                rollback_intel_gpu(gpu);
                break;
            case GpuVendor::Unknown:
                break;
        }
    }

    active_ = false;
    ZENITH_LOG_INFO("All GPU clock locks and power states cleanly restored.");
}

} // namespace zenith
