// SPDX-License-Identifier: MIT
#include "zenith/tweaks/sysctl_tweak.hpp"

namespace zenith {

SysctlTweak::SysctlTweak(fs::path proc_root, fs::path sys_root)
    : proc_root_(std::move(proc_root)),
      sys_root_(std::move(sys_root)) {}

bool SysctlTweak::probe() {
    std::error_code ec;
    return fs::exists(proc_root_, ec) || fs::exists(sys_root_, ec);
}

bool SysctlTweak::apply(Profile profile) {
    auto write_saved = [this](const fs::path& p, const std::string& val) {
        if (!fs::exists(p)) return;
        auto cur = read_file_string(p);
        if (cur && saved_values_.find(p.string()) == saved_values_.end()) {
            // If bracketed, extract token
            auto s = cur->find('[');
            auto e = cur->find(']', s);
            std::string token = (s != std::string::npos && e != std::string::npos)
                                ? cur->substr(s + 1, e - s - 1) : *cur;
            saved_values_[p.string()] = token;
        }
        write_file_string(p, val);
    };

    // 1. split_lock_mitigate (Only in Aggressive profile)
    fs::path split_lock = proc_root_ / "sys/kernel/split_lock_mitigate";
    if (profile == Profile::Aggressive || profile == Profile::Ultra) {
        write_saved(split_lock, "0");
    }

    // 2. THP enabled (madvise for safe & balanced, always for aggressive)
    fs::path thp_enabled = sys_root_ / "kernel/mm/transparent_hugepage/enabled";
    std::string thp_mode = (profile == Profile::Aggressive || profile == Profile::Ultra) ? "always" : "madvise";
    write_saved(thp_enabled, thp_mode);

    // 3. THP defrag (defer+madvise for balanced & aggressive)
    fs::path thp_defrag = sys_root_ / "kernel/mm/transparent_hugepage/defrag";
    if (profile != Profile::Safe) {
        write_saved(thp_defrag, "defer+madvise");
    }

    // 4. compaction_proactiveness -> 0 in balanced and aggressive
    fs::path compaction = proc_root_ / "sys/vm/compaction_proactiveness";
    if (profile != Profile::Safe) {
        write_saved(compaction, "0");
    }

    return true;
}

bool SysctlTweak::revert() {
    bool all_ok = true;
    for (const auto& [path, original] : saved_values_) {
        if (!write_file_string(path, original)) all_ok = false;
    }
    saved_values_.clear();
    return all_ok;
}

std::string SysctlTweak::describe() const {
    return "Kernel memory, THP, compaction proactiveness, and split-lock mitigation";
}

std::string SysctlTweak::explain() const {
    return "Configures Transparent HugePages (madvise in balanced, always in aggressive) and sets compaction_proactiveness to 0. "
           "In aggressive mode, disables split_lock_mitigate to prevent kernel penalties on unaligned atomic locks in Windows games under Proton. "
           "Risks: Minor memory fragmentation when THP is set to always.";
}

std::string SysctlTweak::get_current_value() const {
    std::string thp = "unknown";
    std::string sl = "unknown";
    if (auto v = read_file_string(sys_root_ / "kernel/mm/transparent_hugepage/enabled")) thp = *v;
    if (auto v = read_file_string(proc_root_ / "sys/kernel/split_lock_mitigate")) sl = *v;
    return "thp=" + thp + ", split_lock=" + sl;
}

std::string SysctlTweak::get_target_value(Profile profile) const {
    switch (profile) {
        case Profile::Safe: return "thp=madvise, split_lock=(untouched), compaction=(untouched)";
        case Profile::Balanced: return "thp=madvise, defrag=defer+madvise, compaction=0, split_lock=(untouched)";
        case Profile::Aggressive: return "thp=always, defrag=defer+madvise, compaction=0, split_lock=0";
        case Profile::Ultra: return "thp=always, defrag=defer+madvise, compaction=0, split_lock=0, swappiness=0";
    }
    return "thp=madvise";
}

} // namespace zenith
