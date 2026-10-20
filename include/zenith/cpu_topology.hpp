// SPDX-License-Identifier: MIT
#pragma once

#include "zenith/common.hpp"
#include "zenith/topology_fs.hpp"
#include <vector>
#include <string>
#include <memory>
#include <unordered_set>

namespace zenith {

enum class CoreType {
    Standard,  // Uniform x86 core
    P_Core,    // Intel Performance Core (0x20)
    E_Core,    // Intel Efficient Core (0x40)
    LPE_Core   // Intel Low Power Efficient Core (0x10)
};

enum class CacheSelectionMode {
    Auto,
    Cache,
    Frequency
};

enum class FormFactor {
    Desktop,
    Laptop,
    Handheld,
    VirtualMachine
};

struct CpuCore {
    int id = -1;
    int package_id = 0;
    int core_id = 0;
    int numa_node = 0;
    size_t l3_cache_bytes = 0;
    int l3_cache_id = -1;
    std::vector<int> l3_shared_cpus;
    uint64_t max_freq_khz = 0;
    CoreType type = CoreType::Standard;
    bool is_vcache_ccd = false;
    bool is_isolated = false;
    bool is_online = true;
};

class CpuTopology {
public:
    static CpuTopology detect(const ITopologyFs* fs = nullptr);

    // Core lists
    const std::vector<CpuCore>& get_cores() const { return cores_; }
    std::vector<int> get_gaming_cores(CacheSelectionMode mode = CacheSelectionMode::Auto) const;
    const std::vector<int>& get_background_cores() const { return background_cores_; }
    const std::vector<int>& get_isolated_cores() const { return isolated_cores_; }

    // Architecture identification
    bool has_vcache() const { return has_vcache_; }
    bool has_hybrid() const { return has_hybrid_; }
    bool has_amd_x3d_driver() const { return has_amd_x3d_driver_; }
    const fs::path& get_amd_x3d_mode_path() const { return amd_x3d_mode_path_; }

    bool is_smt_active() const { return smt_active_; }
    size_t num_numa_nodes() const { return numa_nodes_count_; }
    size_t num_sockets() const { return sockets_count_; }
    bool is_vm() const { return is_vm_; }

    // Form factor & battery
    FormFactor form_factor() const { return form_factor_; }
    bool is_on_battery() const { return on_battery_; }
    bool is_energy_conscious() const {
        return (form_factor_ == FormFactor::Handheld || (form_factor_ == FormFactor::Laptop && on_battery_));
    }

    size_t total_cores() const { return cores_.size(); }
    size_t online_cores_count() const;

    // Hotplug dynamic update
    bool recompute_hotplug(const ITopologyFs* fs = nullptr);

    std::string to_cpuset_string(const std::vector<int>& core_ids) const;

    // Helper to parse Linux cpulist format: "0-3,7,12-15"
    static std::vector<int> parse_cpu_list(std::string_view list_str);

private:
    std::vector<CpuCore> cores_;
    std::vector<int> vcache_cores_;
    std::vector<int> frequency_cores_;
    std::vector<int> background_cores_;
    std::vector<int> isolated_cores_;

    bool has_vcache_ = false;
    bool has_hybrid_ = false;
    bool has_amd_x3d_driver_ = false;
    fs::path amd_x3d_mode_path_;

    bool smt_active_ = true;
    size_t numa_nodes_count_ = 1;
    size_t sockets_count_ = 1;
    bool is_vm_ = false;

    FormFactor form_factor_ = FormFactor::Desktop;
    bool on_battery_ = false;

    void classify_cores();
    void probe_platform(const ITopologyFs& fs);
};

} // namespace zenith
