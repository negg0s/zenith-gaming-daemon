#include "zenith/cpu_topology.hpp"
#include <map>

namespace zenith {

CpuTopology CpuTopology::detect(const fs::path& sysfs_cpu_root) {
    CpuTopology topo;

    if (!fs::exists(sysfs_cpu_root)) {
        // Fallback for non-Linux or simulated test environment: create a mock 8-core topology
        for (int i = 0; i < 8; ++i) {
            CpuCore core;
            core.id = i;
            core.package_id = 0;
            core.core_id = i;
            core.numa_node = 0;
            core.l3_cache_bytes = (i < 4) ? 96 * 1024 * 1024 : 32 * 1024 * 1024; // Simulated V-Cache on 0-3
            core.max_freq_khz = 5000000;
            core.is_vcache_ccd = (i < 4);
            core.is_p_core = true;
            topo.cores_.push_back(core);
        }
        topo.classify_cores();
        return topo;
    }

    for (const auto& entry : fs::directory_iterator(sysfs_cpu_root)) {
        std::string filename = entry.path().filename().string();
        if (!filename.starts_with("cpu") || filename.size() <= 3) continue;
        if (!std::isdigit(filename[3])) continue;

        int core_id = -1;
        try {
            core_id = std::stoi(filename.substr(3));
        } catch (...) {
            continue;
        }

        CpuCore core;
        core.id = core_id;

        // Topology files
        auto topo_dir = entry.path() / "topology";
        if (auto pkg = read_file_string(topo_dir / "physical_package_id")) {
            try { core.package_id = std::stoi(*pkg); } catch (...) {}
        }
        if (auto cid = read_file_string(topo_dir / "core_id")) {
            try { core.core_id = std::stoi(*cid); } catch (...) {}
        }

        // L3 Cache index
        auto cache_dir = entry.path() / "cache";
        if (fs::exists(cache_dir)) {
            for (const auto& c_entry : fs::directory_iterator(cache_dir)) {
                auto level_str = read_file_string(c_entry.path() / "level");
                if (level_str && *level_str == "3") {
                    if (auto sz_str = read_file_string(c_entry.path() / "size")) {
                        size_t multiplier = 1024;
                        if (sz_str->ends_with('M')) multiplier = 1024 * 1024;
                        else if (sz_str->ends_with('K')) multiplier = 1024;
                        try {
                            core.l3_cache_bytes = std::stoull(*sz_str) * multiplier;
                        } catch (...) {}
                    }
                }
            }
        }

        // Frequency (for Intel P/E core identification)
        auto cpufreq_dir = entry.path() / "cpufreq";
        if (auto freq_str = read_file_string(cpufreq_dir / "cpuinfo_max_freq")) {
            try { core.max_freq_khz = std::stoull(*freq_str); } catch (...) {}
        }

        topo.cores_.push_back(core);
    }

    std::sort(topo.cores_.begin(), topo.cores_.end(), [](const CpuCore& a, const CpuCore& b) {
        return a.id < b.id;
    });

    topo.classify_cores();
    return topo;
}

void CpuTopology::classify_cores() {
    if (cores_.empty()) return;

    // Detect AMD 3D V-Cache asymmetry: find max L3 cache size across cores
    size_t max_l3 = 0;
    size_t min_l3 = std::numeric_limits<size_t>::max();
    for (const auto& c : cores_) {
        if (c.l3_cache_bytes > max_l3) max_l3 = c.l3_cache_bytes;
        if (c.l3_cache_bytes > 0 && c.l3_cache_bytes < min_l3) min_l3 = c.l3_cache_bytes;
    }

    // If there is significant cache disparity (e.g. 96MB CCD vs 32MB CCD on 7900X3D/7950X3D)
    if (max_l3 > 0 && min_l3 > 0 && max_l3 >= (min_l3 * 2)) {
        has_vcache_ = true;
        for (auto& c : cores_) {
            c.is_vcache_ccd = (c.l3_cache_bytes == max_l3);
        }
    }

    // Detect Intel Hybrid (P-cores vs E-cores) by max frequency disparity
    uint64_t max_freq = 0;
    uint64_t min_freq = std::numeric_limits<uint64_t>::max();
    for (const auto& c : cores_) {
        if (c.max_freq_khz > max_freq) max_freq = c.max_freq_khz;
        if (c.max_freq_khz > 0 && c.max_freq_khz < min_freq) min_freq = c.max_freq_khz;
    }

    if (max_freq > 0 && min_freq > 0 && (max_freq - min_freq) > 800000) { // > 800 MHz delta
        has_hybrid_ = true;
        for (auto& c : cores_) {
            c.is_p_core = (c.max_freq_khz >= (max_freq - 300000));
        }
    }

    // Partition into gaming and background cores
    gaming_cores_.clear();
    background_cores_.clear();

    if (has_vcache_) {
        for (const auto& c : cores_) {
            if (c.is_vcache_ccd) gaming_cores_.push_back(c.id);
            else background_cores_.push_back(c.id);
        }
    } else if (has_hybrid_) {
        for (const auto& c : cores_) {
            if (c.is_p_core) gaming_cores_.push_back(c.id);
            else background_cores_.push_back(c.id);
        }
    } else {
        // Uniform topology: reserve core 0 for system/housekeeping, remainder for gaming
        if (cores_.size() > 4) {
            background_cores_.push_back(cores_[0].id);
            for (size_t i = 1; i < cores_.size(); ++i) {
                gaming_cores_.push_back(cores_[i].id);
            }
        } else {
            for (const auto& c : cores_) {
                gaming_cores_.push_back(c.id);
            }
            background_cores_.push_back(cores_[0].id);
        }
    }
}

std::string CpuTopology::to_cpuset_string(const std::vector<int>& core_ids) const {
    if (core_ids.empty()) return "0";
    std::string res;
    for (size_t i = 0; i < core_ids.size(); ++i) {
        if (i > 0) res += ",";
        res += std::to_string(core_ids[i]);
    }
    return res;
}

} // namespace zenith
