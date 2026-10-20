#pragma once

#include "zenith/common.hpp"

namespace zenith {

struct CpuCore {
    int id = -1;
    int package_id = 0;
    int core_id = 0;
    int numa_node = 0;
    size_t l3_cache_bytes = 0;
    uint64_t max_freq_khz = 0;
    bool is_vcache_ccd = false;
    bool is_p_core = true;
};

class CpuTopology {
public:
    static CpuTopology detect(const fs::path& sysfs_cpu_root = "/sys/devices/system/cpu");

    const std::vector<CpuCore>& get_cores() const { return cores_; }
    const std::vector<int>& get_gaming_cores() const { return gaming_cores_; }
    const std::vector<int>& get_background_cores() const { return background_cores_; }

    bool has_vcache() const { return has_vcache_; }
    bool has_hybrid() const { return has_hybrid_; }
    size_t total_cores() const { return cores_.size(); }

    std::string to_cpuset_string(const std::vector<int>& core_ids) const;

private:
    std::vector<CpuCore> cores_;
    std::vector<int> gaming_cores_;
    std::vector<int> background_cores_;
    bool has_vcache_ = false;
    bool has_hybrid_ = false;

    void classify_cores();
};

} // namespace zenith
