// SPDX-License-Identifier: MIT
#include "zenith/tweaks/zram_swap_tweak.hpp"

namespace zenith {

ZramSwapTweak::ZramSwapTweak(fs::path proc_sys_vm, fs::path zram_probe_dir)
    : vm_dir_(std::move(proc_sys_vm)),
      zram_probe_dir_(std::move(zram_probe_dir)) {}

bool ZramSwapTweak::probe() {
    std::error_code ec;
    zram_detected_ = false;

    if (fs::exists(zram_probe_dir_ / "zram0", ec)) {
        zram_detected_ = true;
    } else if (fs::exists("/proc/swaps", ec)) {
        if (auto swaps = read_file_string("/proc/swaps")) {
            if (swaps->find("zram") != std::string::npos) {
                zram_detected_ = true;
            }
        }
    }

    return fs::exists(vm_dir_, ec);
}

bool ZramSwapTweak::apply(Profile profile) {
    if (!probe()) return false;

    auto set_knob = [this](const std::string& knob, const std::string& val) {
        fs::path p = vm_dir_ / knob;
        if (fs::exists(p)) {
            auto cur = read_file_string(p);
            if (cur && saved_vm_values_.find(p.string()) == saved_vm_values_.end()) {
                saved_vm_values_[p.string()] = *cur;
            }
            write_file_string(p, val);
        }
    };

    // 1. Watermark boost factor -> 0 (prevents kswapd spikes during memory allocation)
    if (profile != Profile::Safe) {
        set_knob("watermark_boost_factor", "0");
    }

    // 2. zram optimizations vs Disk swap
    if (zram_detected_) {
        // page-cluster = 0 eliminates reading adjacent compressed pages in RAM
        set_knob("page-cluster", "0");
    } else {
        // Disk swap: keep swappiness low (10)
        set_knob("swappiness", "10");
    }

    // 3. MGLRU probe (/sys/kernel/mm/lru_gen/enabled)
    fs::path mglru_path = "/sys/kernel/mm/lru_gen/enabled";
    if (fs::exists(mglru_path)) {
        auto cur = read_file_string(mglru_path);
        if (cur && cur->find("0x0000") != std::string::npos) {
            if (saved_vm_values_.find(mglru_path.string()) == saved_vm_values_.end()) {
                saved_vm_values_[mglru_path.string()] = *cur;
            }
            // Enable MGLRU tiering
            write_file_string(mglru_path, "0x0007");
        }
    }

    return true;
}

bool ZramSwapTweak::revert() {
    bool all_ok = true;
    for (const auto& [path, original] : saved_vm_values_) {
        if (!write_file_string(path, original)) all_ok = false;
    }
    saved_vm_values_.clear();
    return all_ok;
}

std::string ZramSwapTweak::describe() const {
    return zram_detected_ ? "zram page-cluster=0 and watermark_boost_factor=0"
                          : "disk swap swappiness=10 and watermark_boost_factor=0";
}

std::string ZramSwapTweak::explain() const {
    return "Optimizes Linux swap subsystem based on whether RAM swap (zram) or disk swap is active. "
           "If zram: sets page-cluster=0 to prevent sequential cluster decompressions. "
           "Disables watermark_boost_factor to stop kswapd background reclaiming jitter. "
           "Safely enables Multi-Gen LRU (MGLRU) if available. "
           "Risks: None. Reverts fully.";
}

std::string ZramSwapTweak::get_current_value() const {
    std::string pc = "unknown";
    std::string wbf = "unknown";
    if (auto v = read_file_string(vm_dir_ / "page-cluster")) pc = *v;
    if (auto v = read_file_string(vm_dir_ / "watermark_boost_factor")) wbf = *v;
    return "zram=" + std::string(zram_detected_ ? "yes" : "no") + ", page-cluster=" + pc + ", watermark_boost=" + wbf;
}

std::string ZramSwapTweak::get_target_value(Profile profile) const {
    if (profile == Profile::Safe) {
        return zram_detected_ ? "page-cluster=0" : "page-cluster=(untouched)";
    }
    return zram_detected_ ? "page-cluster=0, watermark_boost=0"
                          : "swappiness=10, watermark_boost=0";
}

} // namespace zenith
