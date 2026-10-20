#include "zenith/gpu_controller.hpp"
#include "zenith/secure_process.hpp"

#ifdef __linux__
#include <dlfcn.h>
#endif

namespace zenith {

// ================= RealNvmlDriver Implementation =================

RealNvmlDriver::RealNvmlDriver() {
#ifdef __linux__
    lib_handle_ = dlopen("libnvidia-ml.so.1", RTLD_NOW);
    if (!lib_handle_) {
        lib_handle_ = dlopen("libnvidia-ml.so", RTLD_NOW);
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
        nvmlDeviceSetPersistenceMode_ = reinterpret_cast<nvmlDeviceSetPersistenceMode_t>(dlsym(lib_handle_, "nvmlDeviceSetPersistenceMode"));
        nvmlDeviceSetGpuLockedClocks_ = reinterpret_cast<nvmlDeviceSetGpuLockedClocks_t>(dlsym(lib_handle_, "nvmlDeviceSetGpuLockedClocks"));
        nvmlDeviceResetGpuLockedClocks_ = reinterpret_cast<nvmlDeviceResetGpuLockedClocks_t>(dlsym(lib_handle_, "nvmlDeviceResetGpuLockedClocks"));

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

// ================= GpuController Implementation =================

GpuController::GpuController(IGpuFs* fs, std::shared_ptr<INvmlDriver> nvml)
    : fs_(fs ? fs : &RealGpuFs::instance()),
      nvml_(nvml ? std::move(nvml) : std::make_shared<RealNvmlDriver>()) {
    detect_gpus();
}

GpuController::~GpuController() {
    rollback();
}

bool GpuController::is_running_on_battery() const {
    fs::path pwr_dir = "/sys/class/power_supply";
    auto entries = fs_->list_directory(pwr_dir);
    for (const auto& entry : entries) {
        auto status = fs_->read_file(entry / "status");
        if (status && trim(*status) == "Discharging") {
            return true;
        }
        auto type = fs_->read_file(entry / "type");
        if (type && trim(*type) == "Battery") {
            auto online = fs_->read_file(entry / "online");
            if (online && trim(*online) == "0") {
                return true;
            }
        }
    }
    return false;
}

uint32_t GpuController::read_sysfs_temperature(const fs::path& card_path) const {
    fs::path hwmon_dir = card_path / "device" / "hwmon";
    auto hwmons = fs_->list_directory(hwmon_dir);
    for (const auto& hm : hwmons) {
        auto temp_str = fs_->read_file(hm / "temp1_input");
        if (temp_str) {
            try {
                uint32_t millideg = std::stoul(trim(*temp_str));
                return millideg / 1000;
            } catch (...) {}
        }
    }
    return 0;
}

bool GpuController::detect_gpus() {
    gpus_.clear();

    // 1. AMD & Intel sysfs DRM devices
    fs::path drm_root = "/sys/class/drm";
    auto entries = fs_->list_directory(drm_root);
    for (const auto& card_path : entries) {
        std::string name = card_path.filename().string();
        if (name.starts_with("card") && name.find('-') == std::string::npos) {
            // Check AMD DPM
            auto dpm_file = card_path / "device" / "power_dpm_force_performance_level";
            if (fs_->exists(dpm_file)) {
                GpuDevice dev;
                dev.vendor = GpuVendor::Amd;
                dev.name = name;
                dev.sysfs_card_path = card_path;
                if (auto cur = fs_->read_file(dpm_file)) {
                    dev.original_dpm_level = trim(*cur);
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

bool GpuController::lock_amd_gpu(GpuDevice& gpu) {
    uint32_t temp = read_sysfs_temperature(gpu.sysfs_card_path);
    if (temp >= MAX_SAFE_TEMP_C) {
        thermal_guard_active_ = true;
        ZENITH_LOG_WARN("AMD GPU " + gpu.name + " temperature (" + std::to_string(temp) +
                        "C) exceeds safety limit (" + std::to_string(MAX_SAFE_TEMP_C) +
                        "C). Aborting clock lock to prevent thermal throttling.");
        return false;
    }

    auto dpm_file = gpu.sysfs_card_path / "device" / "power_dpm_force_performance_level";
    if (fs_->exists(dpm_file)) {
        if (fs_->write_file(dpm_file, "high\n")) {
            gpu.clock_locked = true;
            ZENITH_LOG_INFO("Locked AMD GPU " + gpu.name + " to performance level: high");
            return true;
        }
    }
    return false;
}

bool GpuController::lock_nvidia_gpu(GpuDevice& gpu) {
    if (nvml_->is_available()) {
        auto handle = nvml_->get_device_handle(0);
        uint32_t temp = nvml_->get_temperature(handle);
        if (temp >= MAX_SAFE_TEMP_C) {
            thermal_guard_active_ = true;
            ZENITH_LOG_WARN("NVIDIA GPU temperature (" + std::to_string(temp) +
                            "C) exceeds safety limit (" + std::to_string(MAX_SAFE_TEMP_C) +
                            "C). Aborting clock lock.");
            return false;
        }

        nvml_->set_persistence_mode(handle, true);
        gpu.persistence_mode_enabled = true;

        if (nvml_->set_gpu_locked_clocks(handle, SAFE_NVIDIA_MIN_MHZ, SAFE_NVIDIA_MAX_MHZ)) {
            gpu.clock_locked = true;
            ZENITH_LOG_INFO("Locked NVIDIA GPU clocks via NVML (Min: " + std::to_string(SAFE_NVIDIA_MIN_MHZ) +
                            "MHz, Max: " + std::to_string(SAFE_NVIDIA_MAX_MHZ) + "MHz)");
            return true;
        }
    }

    // Fallback via SecureProcess
    auto res1 = SecureProcess::execute("/usr/bin/nvidia-smi", {"-pm", "1"});
    auto res2 = SecureProcess::execute("/usr/bin/nvidia-smi", {"--auto-boost-permission=0"});
    gpu.clock_locked = (res1.executed && res1.exit_code == 0) || (res2.executed && res2.exit_code == 0);
    return gpu.clock_locked;
}

bool GpuController::lock_intel_gpu(GpuDevice& gpu) {
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

void GpuController::rollback_amd_gpu(GpuDevice& gpu) {
    if (gpu.clock_locked) {
        auto dpm_file = gpu.sysfs_card_path / "device" / "power_dpm_force_performance_level";
        std::string reset_level = gpu.original_dpm_level.empty() ? "auto" : gpu.original_dpm_level;
        fs_->write_file(dpm_file, reset_level + "\n");
        ZENITH_LOG_DEBUG("Restored AMD GPU " + gpu.name + " DPM level to " + reset_level);
        gpu.clock_locked = false;
    }
}

void GpuController::rollback_nvidia_gpu(GpuDevice& gpu) {
    if (gpu.clock_locked) {
        if (nvml_->is_available()) {
            auto handle = nvml_->get_device_handle(0);
            nvml_->reset_gpu_locked_clocks(handle);
            if (gpu.persistence_mode_enabled) {
                nvml_->set_persistence_mode(handle, false);
            }
        } else {
            SecureProcess::execute("/usr/bin/nvidia-smi", {"-rgc"});
            SecureProcess::execute("/usr/bin/nvidia-smi", {"-pm", "0"});
        }
        ZENITH_LOG_DEBUG("Reset NVIDIA GPU clocks and persistence mode.");
        gpu.clock_locked = false;
        gpu.persistence_mode_enabled = false;
    }
}

void GpuController::rollback_intel_gpu(GpuDevice& gpu) {
    if (gpu.clock_locked && gpu.original_intel_min_freq > 0) {
        fs::path min_file = gpu.sysfs_card_path / "device" / "drm" / gpu.name / "gt_min_freq_mhz";
        if (!fs_->exists(min_file)) min_file = gpu.sysfs_card_path / "gt_min_freq_mhz";
        if (fs_->exists(min_file)) {
            fs_->write_file(min_file, std::to_string(gpu.original_intel_min_freq) + "\n");
            ZENITH_LOG_DEBUG("Restored Intel GPU min freq to " + std::to_string(gpu.original_intel_min_freq) + "MHz");
            gpu.clock_locked = false;
        }
    }
}

bool GpuController::lock_clocks() {
    thermal_guard_active_ = false;
    battery_guard_active_ = false;

    // Battery Guardrail
    if (is_running_on_battery()) {
        battery_guard_active_ = true;
        ZENITH_LOG_WARN("System is running on BATTERY power. Aborting GPU clock locking to protect battery health and prevent sudden shutdown.");
        return false;
    }

    bool ok = false;
    for (auto& gpu : gpus_) {
        if (gpu.vendor == GpuVendor::Amd) ok |= lock_amd_gpu(gpu);
        else if (gpu.vendor == GpuVendor::Nvidia) ok |= lock_nvidia_gpu(gpu);
        else if (gpu.vendor == GpuVendor::Intel) ok |= lock_intel_gpu(gpu);
    }
    active_ = ok;
    return active_;
}

void GpuController::rollback() {
    for (auto& gpu : gpus_) {
        if (gpu.vendor == GpuVendor::Amd) rollback_amd_gpu(gpu);
        else if (gpu.vendor == GpuVendor::Nvidia) rollback_nvidia_gpu(gpu);
        else if (gpu.vendor == GpuVendor::Intel) rollback_intel_gpu(gpu);
    }
    active_ = false;
}

} // namespace zenith
