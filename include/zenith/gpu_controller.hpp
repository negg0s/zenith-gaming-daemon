#pragma once

#include "zenith/common.hpp"

namespace zenith {

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
    bool clock_locked = false;
};

class GpuController {
public:
    GpuController();
    ~GpuController();

    bool detect_gpus();
    bool lock_clocks();
    void rollback();

    const std::vector<GpuDevice>& get_gpus() const { return gpus_; }

private:
    std::vector<GpuDevice> gpus_;
    bool active_ = false;

    bool lock_amd_gpu(GpuDevice& gpu);
    bool lock_nvidia_gpu(GpuDevice& gpu);
    void rollback_amd_gpu(GpuDevice& gpu);
    void rollback_nvidia_gpu(GpuDevice& gpu);
};

} // namespace zenith
