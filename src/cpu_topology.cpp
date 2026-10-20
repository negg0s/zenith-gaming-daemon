// SPDX-License-Identifier: MIT
#include "zenith/cpu_topology.hpp"
#include <map>
#include <algorithm>
#include <sstream>

namespace zenith {

std::vector<int> CpuTopology::parse_cpu_list(std::string_view list_str) {
    std::vector<int> cpus;
    std::string s = trim(list_str);
    if (s.empty()) return cpus;

    auto ranges = split(s, ',');
    for (const auto& r : ranges) {
        std::string token = trim(r);
        if (token.empty()) continue;
        auto dash = token.find('-');
        if (dash != std::string::npos) {
            try {
                int start = std::stoi(token.substr(0, dash));
                int end = std::stoi(token.substr(dash + 1));
                if (start <= end) {
                    for (int i = start; i <= end; ++i) cpus.push_back(i);
                }
            } catch (...) {}
        } else {
            try {
                cpus.push_back(std::stoi(token));
            } catch (...) {}
        }
    }
    std::sort(cpus.begin(), cpus.end());
    cpus.erase(std::unique(cpus.begin(), cpus.end()), cpus.end());
    return cpus;
}

std::string CpuTopology::to_cpuset_string(const std::vector<int>& core_ids) const {
    if (core_ids.empty()) return "";
    std::vector<int> sorted = core_ids;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());

    std::stringstream ss;
    size_t i = 0;
    while (i < sorted.size()) {
        int start = sorted[i];
        int end = start;
        while (i + 1 < sorted.size() && sorted[i + 1] == end + 1) {
            end = sorted[i + 1];
            i++;
        }
        if (start == end) {
            ss << start;
        } else {
            ss << start << "-" << end;
        }
        if (i + 1 < sorted.size()) {
            ss << ",";
        }
        i++;
    }
    return ss.str();
}

CpuTopology CpuTopology::detect(const ITopologyFs* fs_ptr) {
    RealTopologyFs real_fs;
    const ITopologyFs& fs = (fs_ptr != nullptr) ? *fs_ptr : real_fs;

    CpuTopology topo;
    fs::path sysfs_cpu_root = "/sys/devices/system/cpu";

    // 1. Probe Platform, SMT, NUMA, Isolated, VM, Handheld/Battery
    topo.probe_platform(fs);

    // 2. Discover CPUs
    auto entries = fs.list_directory(sysfs_cpu_root);
    for (const auto& entry : entries) {
        std::string filename = entry.filename().string();
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
        fs::path topo_dir = entry / "topology";
        if (auto pkg = fs.read_file(topo_dir / "physical_package_id")) {
            try { core.package_id = std::stoi(trim(*pkg)); } catch (...) {}
        }
        if (auto cid = fs.read_file(topo_dir / "core_id")) {
            try { core.core_id = std::stoi(trim(*cid)); } catch (...) {}
        }

        // Intel Core Type (0x20 = P-core, 0x40 = E-core, 0x10 = LP-E core)
        if (auto ct = fs.read_file(topo_dir / "core_type")) {
            std::string ct_str = trim(*ct);
            if (ct_str == "0x20" || ct_str == "32") {
                core.type = CoreType::P_Core;
            } else if (ct_str == "0x40" || ct_str == "64") {
                core.type = CoreType::E_Core;
            } else if (ct_str == "0x10" || ct_str == "16") {
                core.type = CoreType::LPE_Core;
            }
        }

        // ARM / Hybrid cpu_capacity fallback
        if (core.type == CoreType::Standard) {
            if (auto cap = fs.read_file(entry / "cpu_capacity")) {
                try {
                    int c = std::stoi(trim(*cap));
                    if (c >= 1000) core.type = CoreType::P_Core;
                    else if (c > 300) core.type = CoreType::E_Core;
                    else core.type = CoreType::LPE_Core;
                } catch (...) {}
            }
        }

        // Online status
        if (auto on_str = fs.read_file(entry / "online")) {
            core.is_online = (trim(*on_str) == "1");
        } else {
            core.is_online = true; // CPU0 usually has no 'online' file and is always online
        }

        // L3 Cache index3 (size + shared_cpu_list)
        fs::path index3_dir = entry / "cache/index3";
        if (fs.exists(index3_dir)) {
            if (auto sz_str = fs.read_file(index3_dir / "size")) {
                std::string s = trim(*sz_str);
                size_t multiplier = 1024;
                if (s.ends_with('M') || s.ends_with('m')) multiplier = 1024 * 1024;
                else if (s.ends_with('K') || s.ends_with('k')) multiplier = 1024;
                try {
                    core.l3_cache_bytes = std::stoull(s) * multiplier;
                } catch (...) {}
            }

            if (auto shared = fs.read_file(index3_dir / "shared_cpu_list")) {
                core.l3_shared_cpus = parse_cpu_list(*shared);
            }

            if (auto cid = fs.read_file(index3_dir / "id")) {
                try { core.l3_cache_id = std::stoi(trim(*cid)); } catch (...) {}
            }
        }

        // Frequency
        fs::path cpufreq_dir = entry / "cpufreq";
        if (auto freq_str = fs.read_file(cpufreq_dir / "cpuinfo_max_freq")) {
            try { core.max_freq_khz = std::stoull(trim(*freq_str)); } catch (...) {}
        }

        // Isolated check
        for (int iso : topo.isolated_cores_) {
            if (iso == core.id) {
                core.is_isolated = true;
                break;
            }
        }

        topo.cores_.push_back(core);
    }

    std::sort(topo.cores_.begin(), topo.cores_.end(), [](const CpuCore& a, const CpuCore& b) {
        return a.id < b.id;
    });

    topo.classify_cores();
    return topo;
}

void CpuTopology::probe_platform(const ITopologyFs& fs) {
    // 1. AMD 3D V-Cache driver (Linux 6.13+)
    fs::path driver_path = "/sys/bus/platform/drivers/amd_x3d_vcache";
    fs::path mode_file = "/sys/devices/platform/amd_x3d_vcache/amd_x3d_mode";
    if (fs.exists(mode_file) || fs.exists(driver_path)) {
        has_amd_x3d_driver_ = true;
        amd_x3d_mode_path_ = mode_file;
    }

    // 2. SMT status
    fs::path smt_path = "/sys/devices/system/cpu/smt/control";
    if (auto smt_val = fs.read_file(smt_path)) {
        std::string s = trim(*smt_val);
        smt_active_ = (s != "off" && s != "forceoff" && s != "notsupported");
    }

    // 3. Isolated cores (isolcpus / nohz_full)
    fs::path iso_path = "/sys/devices/system/cpu/isolated";
    if (auto iso_val = fs.read_file(iso_path)) {
        isolated_cores_ = parse_cpu_list(*iso_val);
    }

    // 4. NUMA node count
    fs::path node_dir = "/sys/devices/system/node";
    if (fs.exists(node_dir)) {
        auto nodes = fs.list_directory(node_dir);
        size_t n_count = 0;
        for (const auto& n : nodes) {
            std::string fn = n.filename().string();
            if (fn.starts_with("node") && fn.size() > 4 && std::isdigit(fn[4])) {
                n_count++;
            }
        }
        if (n_count > 0) numa_nodes_count_ = n_count;
    }

    // 5. Chassis & DMI product (Laptop / Handheld / VM)
    fs::path chassis_path = "/sys/class/dmi/id/chassis_type";
    if (auto ch = fs.read_file(chassis_path)) {
        try {
            int type = std::stoi(trim(*ch));
            if (type == 31 || type == 32) {
                form_factor_ = FormFactor::Handheld;
            } else if (type == 8 || type == 9 || type == 10 || type == 11 || type == 14) {
                form_factor_ = FormFactor::Laptop;
            }
        } catch (...) {}
    }

    fs::path prod_path = "/sys/class/dmi/id/product_name";
    if (auto prod = fs.read_file(prod_path)) {
        std::string p = trim(*prod);
        if (p == "Jupiter" || p == "Galileo") {
            form_factor_ = FormFactor::Handheld; // Steam Deck
        } else if (p.find("QEMU") != std::string::npos || p.find("VirtualBox") != std::string::npos ||
                   p.find("VMware") != std::string::npos || p.find("KVM") != std::string::npos) {
            form_factor_ = FormFactor::VirtualMachine;
            is_vm_ = true;
        }
    }

    fs::path vendor_path = "/sys/class/dmi/id/sys_vendor";
    if (auto vend = fs.read_file(vendor_path)) {
        std::string v = trim(*vend);
        if (v.find("QEMU") != std::string::npos || v.find("innotek") != std::string::npos ||
            v.find("VMware") != std::string::npos) {
            form_factor_ = FormFactor::VirtualMachine;
            is_vm_ = true;
        }
    }

    // 6. Power supply / battery status
    fs::path pwr_dir = "/sys/class/power_supply";
    if (fs.exists(pwr_dir)) {
        auto supplies = fs.list_directory(pwr_dir);
        for (const auto& sup : supplies) {
            if (auto type = fs.read_file(sup / "type")) {
                if (trim(*type) == "Battery") {
                    if (auto status = fs.read_file(sup / "status")) {
                        if (trim(*status) == "Discharging") {
                            on_battery_ = true;
                        }
                    }
                }
            }
        }
    }
}

void CpuTopology::classify_cores() {
    if (cores_.empty()) return;

    // Detect socket count
    std::unordered_set<int> packages;
    for (const auto& c : cores_) {
        packages.insert(c.package_id);
    }
    sockets_count_ = packages.empty() ? 1 : packages.size();

    // 1. AMD 3D V-Cache Detection via index3/size
    size_t max_l3 = 0;
    size_t min_l3 = std::numeric_limits<size_t>::max();
    for (const auto& c : cores_) {
        if (!c.is_online) continue;
        if (c.l3_cache_bytes > max_l3) max_l3 = c.l3_cache_bytes;
        if (c.l3_cache_bytes > 0 && c.l3_cache_bytes < min_l3) min_l3 = c.l3_cache_bytes;
    }

    // Single-CCD V-Cache (e.g. 7800X3D, 5800X3D with 96M) or Dual-CCD Asymmetry (7950X3D, 9950X3D)
    if (max_l3 >= 64 * 1024 * 1024) {
        has_vcache_ = true;
        bool asymmetric = (min_l3 > 0 && max_l3 >= (min_l3 * 2));

        for (auto& c : cores_) {
            if (!c.is_online) continue;
            if (asymmetric) {
                c.is_vcache_ccd = (c.l3_cache_bytes == max_l3);
            } else {
                c.is_vcache_ccd = true; // All cores on 7800X3D/5800X3D have 3D V-Cache
            }
        }
    }

    // 2. Intel Hybrid Detection via core_type or frequency
    bool found_explicit_hybrid = false;
    for (const auto& c : cores_) {
        if (c.type == CoreType::P_Core || c.type == CoreType::E_Core || c.type == CoreType::LPE_Core) {
            found_explicit_hybrid = true;
            break;
        }
    }

    if (found_explicit_hybrid) {
        has_hybrid_ = true;
    } else {
        // Fallback: frequency disparity (> 800 MHz)
        uint64_t max_freq = 0;
        uint64_t min_freq = std::numeric_limits<uint64_t>::max();
        for (const auto& c : cores_) {
            if (!c.is_online) continue;
            if (c.max_freq_khz > max_freq) max_freq = c.max_freq_khz;
            if (c.max_freq_khz > 0 && c.max_freq_khz < min_freq) min_freq = c.max_freq_khz;
        }

        if (max_freq > 0 && min_freq > 0 && (max_freq - min_freq) > 800000) {
            has_hybrid_ = true;
            for (auto& c : cores_) {
                if (c.max_freq_khz >= (max_freq - 300000)) {
                    c.type = CoreType::P_Core;
                } else {
                    c.type = CoreType::E_Core;
                }
            }
        }
    }

    // 3. Build Core Pools (vcache_cores_, frequency_cores_, background_cores_)
    vcache_cores_.clear();
    frequency_cores_.clear();
    background_cores_.clear();

    for (const auto& c : cores_) {
        if (!c.is_online || c.is_isolated) continue;

        if (has_vcache_) {
            if (c.is_vcache_ccd) {
                vcache_cores_.push_back(c.id);
            } else {
                frequency_cores_.push_back(c.id);
                background_cores_.push_back(c.id);
            }
        } else if (has_hybrid_) {
            if (c.type == CoreType::P_Core) {
                vcache_cores_.push_back(c.id);
                frequency_cores_.push_back(c.id);
            } else {
                background_cores_.push_back(c.id);
            }
        } else {
            // Uniform CPU: all non-isolated cores are available for gaming
            vcache_cores_.push_back(c.id);
            frequency_cores_.push_back(c.id);
        }
    }

    // In uniform topology, reserve core 0 for background if sufficient cores exist
    if (!has_vcache_ && !has_hybrid_) {
        if (cores_.size() > 4) {
            background_cores_.push_back(cores_[0].id);
        } else {
            background_cores_.push_back(cores_[0].id);
        }
    }

    // If single CCD V-Cache (e.g. 7800X3D), frequency_cores_ is identical to vcache_cores_
    if (frequency_cores_.empty()) {
        frequency_cores_ = vcache_cores_;
    }
}

std::vector<int> CpuTopology::get_gaming_cores(CacheSelectionMode mode) const {
    switch (mode) {
        case CacheSelectionMode::Cache:
            return vcache_cores_.empty() ? frequency_cores_ : vcache_cores_;
        case CacheSelectionMode::Frequency:
            return frequency_cores_.empty() ? vcache_cores_ : frequency_cores_;
        case CacheSelectionMode::Auto:
        default:
            return vcache_cores_.empty() ? frequency_cores_ : vcache_cores_;
    }
}

size_t CpuTopology::online_cores_count() const {
    size_t count = 0;
    for (const auto& c : cores_) {
        if (c.is_online) count++;
    }
    return count;
}

bool CpuTopology::recompute_hotplug(const ITopologyFs* fs_ptr) {
    RealTopologyFs real_fs;
    const ITopologyFs& fs = (fs_ptr != nullptr) ? *fs_ptr : real_fs;

    fs::path sysfs_cpu_root = "/sys/devices/system/cpu";
    if (!fs.exists(sysfs_cpu_root)) return false;

    // Refresh online status of all known cores
    for (auto& c : cores_) {
        fs::path p = sysfs_cpu_root / ("cpu" + std::to_string(c.id)) / "online";
        if (auto on_str = fs.read_file(p)) {
            c.is_online = (trim(*on_str) == "1");
        } else {
            c.is_online = true;
        }
    }

    classify_cores();
    return true;
}

} // namespace zenith
