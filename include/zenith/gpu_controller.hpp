// SPDX-License-Identifier: MIT
#pragma once

#include "zenith/common.hpp"
#include "zenith/tweaks/tweak_interface.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>
#include <functional>

namespace zenith {

class IGpuFs {
public:
    virtual ~IGpuFs() = default;
    [[nodiscard]] virtual bool exists(const fs::path& path) const = 0;
    [[nodiscard]] virtual std::optional<std::string> read_file(const fs::path& path) const = 0;
    virtual bool write_file(const fs::path& path, const std::string& content) = 0;
    [[nodiscard]] virtual std::vector<fs::path> list_directory(const fs::path& path) const = 0;
};

class RealGpuFs final : public IGpuFs {
public:
    static RealGpuFs& instance() {
        static RealGpuFs inst;
        return inst;
    }

    [[nodiscard]] bool exists(const fs::path& path) const override {
        std::error_code ec;
        return fs::exists(path, ec);
    }

    [[nodiscard]] std::optional<std::string> read_file(const fs::path& path) const override {
        return read_file_string(path);
    }

    bool write_file(const fs::path& path, const std::string& content) override {
        return write_file_string(path, content);
    }

    [[nodiscard]] virtual std::vector<fs::path> list_directory(const fs::path& path) const override {
        std::vector<fs::path> res;
        std::error_code ec;
        if (!fs::exists(path, ec)) return res;
        for (const auto& entry : fs::directory_iterator(path, ec)) {
            res.push_back(entry.path());
        }
        return res;
    }
};

class MockGpuFs final : public IGpuFs {
public:
    std::unordered_map<std::string, std::string> files;
    std::unordered_map<std::string, std::vector<fs::path>> dirs;

    [[nodiscard]] bool exists(const fs::path& path) const override {
        std::string p = path.generic_string();
        if (files.find(p) != files.end()) return true;
        if (dirs.find(p) != dirs.end()) return true;
        return false;
    }

    [[nodiscard]] std::optional<std::string> read_file(const fs::path& path) const override {
        auto it = files.find(path.generic_string());
        if (it != files.end()) return it->second;
        return std::nullopt;
    }

    bool write_file(const fs::path& path, const std::string& content) override {
        files[path.generic_string()] = content;
        return true;
    }

    [[nodiscard]] std::vector<fs::path> list_directory(const fs::path& path) const override {
        auto it = dirs.find(path.generic_string());
        if (it != dirs.end()) return it->second;
        return {};
    }
};

enum class GpuVendor {
    Unknown,
    Amd,
    Nvidia,
    Intel
};

struct GpuDevice {
    GpuVendor vendor = GpuVendor::Unknown;
    std::string name;
    fs::path sysfs_card_path;
    std::string original_dpm_level;
    std::string original_power_profile_mode;
    uint32_t original_power_cap = 0;
    uint32_t original_intel_min_freq = 0;
    bool clock_locked = false;
    bool memory_clock_locked = false;
    bool power_profile_active = false;
    bool power_cap_boosted = false;
    bool persistence_mode_enabled = false;
    uint32_t temperature_c = 0;
    uint32_t original_nvidia_power_limit = 0;
};

// Forward declaration of NVML function types
using NvmlDeviceHandle = void*;

class INvmlDriver {
public:
    virtual ~INvmlDriver() = default;
    [[nodiscard]] virtual bool is_available() const = 0;
    [[nodiscard]] virtual unsigned int get_device_count() = 0;
    [[nodiscard]] virtual NvmlDeviceHandle get_device_handle(unsigned int index) = 0;
    [[nodiscard]] virtual unsigned int get_temperature(NvmlDeviceHandle dev) = 0;
    [[nodiscard]] virtual unsigned int get_power_limit(NvmlDeviceHandle dev) = 0;
    [[nodiscard]] virtual unsigned int get_max_power_limit(NvmlDeviceHandle dev) = 0;
    virtual bool set_persistence_mode(NvmlDeviceHandle dev, bool enable) = 0;
    virtual bool set_gpu_locked_clocks(NvmlDeviceHandle dev, unsigned int min_mhz, unsigned int max_mhz) = 0;
    virtual bool reset_gpu_locked_clocks(NvmlDeviceHandle dev) = 0;
    virtual bool set_memory_locked_clocks(NvmlDeviceHandle dev, unsigned int min_mhz, unsigned int max_mhz) = 0;
    virtual bool reset_memory_locked_clocks(NvmlDeviceHandle dev) = 0;
    virtual bool set_power_limit(NvmlDeviceHandle dev, unsigned int milliwatts) = 0;
};

class RealNvmlDriver final : public INvmlDriver {
public:
    RealNvmlDriver();
    ~RealNvmlDriver() override;

    [[nodiscard]] bool is_available() const override { return loaded_; }
    [[nodiscard]] unsigned int get_device_count() override;
    [[nodiscard]] NvmlDeviceHandle get_device_handle(unsigned int index) override;
    [[nodiscard]] unsigned int get_temperature(NvmlDeviceHandle dev) override;
    [[nodiscard]] unsigned int get_power_limit(NvmlDeviceHandle dev) override;
    [[nodiscard]] unsigned int get_max_power_limit(NvmlDeviceHandle dev) override;
    bool set_persistence_mode(NvmlDeviceHandle dev, bool enable) override;
    bool set_gpu_locked_clocks(NvmlDeviceHandle dev, unsigned int min_mhz, unsigned int max_mhz) override;
    bool reset_gpu_locked_clocks(NvmlDeviceHandle dev) override;
    bool set_memory_locked_clocks(NvmlDeviceHandle dev, unsigned int min_mhz, unsigned int max_mhz) override;
    bool reset_memory_locked_clocks(NvmlDeviceHandle dev) override;
    bool set_power_limit(NvmlDeviceHandle dev, unsigned int milliwatts) override;

private:
    void* lib_handle_ = nullptr;
    bool loaded_ = false;

    // Function pointers
    using nvmlInit_t = int (*)();
    using nvmlShutdown_t = int (*)();
    using nvmlDeviceGetCount_t = int (*)(unsigned int*);
    using nvmlDeviceGetHandleByIndex_t = int (*)(unsigned int, NvmlDeviceHandle*);
    using nvmlDeviceGetTemperature_t = int (*)(NvmlDeviceHandle, int, unsigned int*);
    using nvmlDeviceGetPowerManagementLimit_t = int (*)(NvmlDeviceHandle, unsigned int*);
    using nvmlDeviceGetPowerManagementLimitConstraints_t = int (*)(NvmlDeviceHandle, unsigned int*, unsigned int*);
    using nvmlDeviceSetPersistenceMode_t = int (*)(NvmlDeviceHandle, int);
    using nvmlDeviceSetGpuLockedClocks_t = int (*)(NvmlDeviceHandle, unsigned int, unsigned int);
    using nvmlDeviceResetGpuLockedClocks_t = int (*)(NvmlDeviceHandle);
    using nvmlDeviceSetMemoryLockedClocks_t = int (*)(NvmlDeviceHandle, unsigned int, unsigned int);
    using nvmlDeviceResetMemoryLockedClocks_t = int (*)(NvmlDeviceHandle);
    using nvmlDeviceSetPowerManagementLimit_t = int (*)(NvmlDeviceHandle, unsigned int);

    nvmlInit_t nvmlInit_ = nullptr;
    nvmlShutdown_t nvmlShutdown_ = nullptr;
    nvmlDeviceGetCount_t nvmlDeviceGetCount_ = nullptr;
    nvmlDeviceGetHandleByIndex_t nvmlDeviceGetHandleByIndex_ = nullptr;
    nvmlDeviceGetTemperature_t nvmlDeviceGetTemperature_ = nullptr;
    nvmlDeviceGetPowerManagementLimit_t nvmlDeviceGetPowerManagementLimit_ = nullptr;
    nvmlDeviceGetPowerManagementLimitConstraints_t nvmlDeviceGetPowerManagementLimitConstraints_ = nullptr;
    nvmlDeviceSetPersistenceMode_t nvmlDeviceSetPersistenceMode_ = nullptr;
    nvmlDeviceSetGpuLockedClocks_t nvmlDeviceSetGpuLockedClocks_ = nullptr;
    nvmlDeviceResetGpuLockedClocks_t nvmlDeviceResetGpuLockedClocks_ = nullptr;
    nvmlDeviceSetMemoryLockedClocks_t nvmlDeviceSetMemoryLockedClocks_ = nullptr;
    nvmlDeviceResetMemoryLockedClocks_t nvmlDeviceResetMemoryLockedClocks_ = nullptr;
    nvmlDeviceSetPowerManagementLimit_t nvmlDeviceSetPowerManagementLimit_ = nullptr;
};

class MockNvmlDriver final : public INvmlDriver {
public:
    bool available = true;
    unsigned int device_count = 1;
    unsigned int simulated_temp = 55;
    bool persistence_mode = false;
    bool clocks_locked = false;
    bool mem_clocks_locked = false;
    unsigned int locked_min = 0;
    unsigned int locked_max = 0;
    unsigned int mem_locked_min = 0;
    unsigned int mem_locked_max = 0;
    unsigned int current_power_limit = 250000;
    unsigned int max_power_limit = 320000;

    [[nodiscard]] bool is_available() const override { return available; }
    [[nodiscard]] unsigned int get_device_count() override { return device_count; }
    [[nodiscard]] NvmlDeviceHandle get_device_handle(unsigned int) override { return reinterpret_cast<NvmlDeviceHandle>(0x1234); }
    [[nodiscard]] unsigned int get_temperature(NvmlDeviceHandle) override { return simulated_temp; }
    [[nodiscard]] unsigned int get_power_limit(NvmlDeviceHandle) override { return current_power_limit; }
    [[nodiscard]] unsigned int get_max_power_limit(NvmlDeviceHandle) override { return max_power_limit; }
    bool set_persistence_mode(NvmlDeviceHandle, bool enable) override {
        persistence_mode = enable;
        return true;
    }
    bool set_gpu_locked_clocks(NvmlDeviceHandle, unsigned int min_mhz, unsigned int max_mhz) override {
        clocks_locked = true;
        locked_min = min_mhz;
        locked_max = max_mhz;
        return true;
    }
    bool reset_gpu_locked_clocks(NvmlDeviceHandle) override {
        clocks_locked = false;
        locked_min = 0;
        locked_max = 0;
        return true;
    }
    bool set_memory_locked_clocks(NvmlDeviceHandle, unsigned int min_mhz, unsigned int max_mhz) override {
        mem_clocks_locked = true;
        mem_locked_min = min_mhz;
        mem_locked_max = max_mhz;
        return true;
    }
    bool reset_memory_locked_clocks(NvmlDeviceHandle) override {
        mem_clocks_locked = false;
        mem_locked_min = 0;
        mem_locked_max = 0;
        return true;
    }
    bool set_power_limit(NvmlDeviceHandle, unsigned int milliwatts) override {
        current_power_limit = milliwatts;
        return true;
    }
};

class GpuController {
public:
    explicit GpuController(IGpuFs* fs = nullptr, std::shared_ptr<INvmlDriver> nvml = nullptr);
    ~GpuController();

    bool detect_gpus();
    bool lock_clocks(Profile profile = Profile::Balanced);
    void rollback();

    static void apply_driver_environment_hints();

    [[nodiscard]] const std::vector<GpuDevice>& get_gpus() const { return gpus_; }
    [[nodiscard]] bool is_active() const { return active_; }
    [[nodiscard]] bool thermal_throttle_guard_active() const { return thermal_guard_active_; }
    [[nodiscard]] bool battery_guard_active() const { return battery_guard_active_; }

    static constexpr uint32_t MAX_SAFE_TEMP_C = 85;
    static constexpr uint32_t SAFE_NVIDIA_MIN_MHZ = 1200;
    static constexpr uint32_t SAFE_NVIDIA_MAX_MHZ = 2600;
    static constexpr uint32_t SAFE_NVIDIA_MEM_MIN_MHZ = 5000;
    static constexpr uint32_t SAFE_NVIDIA_MEM_MAX_MHZ = 12000;

private:
    IGpuFs* fs_;
    std::shared_ptr<INvmlDriver> nvml_;
    std::vector<GpuDevice> gpus_;
    bool active_ = false;
    bool thermal_guard_active_ = false;
    bool battery_guard_active_ = false;

    [[nodiscard]] bool is_running_on_battery() const;
    [[nodiscard]] uint32_t read_sysfs_temperature(const fs::path& card_path) const;

    bool lock_amd_gpu(GpuDevice& gpu, Profile profile);
    bool lock_nvidia_gpu(GpuDevice& gpu, Profile profile);
    bool lock_intel_gpu(GpuDevice& gpu, Profile profile);

    void rollback_amd_gpu(GpuDevice& gpu);
    void rollback_nvidia_gpu(GpuDevice& gpu);
    void rollback_intel_gpu(GpuDevice& gpu);
};

} // namespace zenith
