#include "zenith/sysctl_tuner.hpp"

namespace zenith {

SysctlTuner::~SysctlTuner() {
    rollback();
}

bool SysctlTuner::set_param(const fs::path& path, std::string_view value) {
    if (!fs::exists(path)) return false;

    auto current = read_file_string(path);
    if (!current) return false;

    // Record entry for rollback
    SysctlEntry entry;
    entry.path = path;
    entry.original_value = *current;
    entry.tuned_value = std::string(value);

    if (write_file_string(path, value)) {
        entry.modified = true;
        entries_.push_back(entry);
        ZENITH_LOG_DEBUG("Sysctl " + path.string() + " = " + std::string(value) + " (was " + *current + ")");
        return true;
    }
    return false;
}

bool SysctlTuner::apply(bool disable_split_lock, bool enable_thp, int swappiness, int dirty_ratio, int compaction) {
    entries_.clear();

    if (disable_split_lock) {
        set_param("/proc/sys/kernel/split_lock_mitigate", "0");
    }

    if (enable_thp) {
        set_param("/sys/kernel/mm/transparent_hugepage/enabled", "always");
        set_param("/sys/kernel/mm/transparent_hugepage/defrag", "defer+madvise");
    }

    set_param("/proc/sys/vm/swappiness", std::to_string(swappiness));
    set_param("/proc/sys/vm/dirty_ratio", std::to_string(dirty_ratio));
    set_param("/proc/sys/vm/compaction_proactiveness", std::to_string(compaction));
    set_param("/proc/sys/kernel/sched_autogroup_enabled", "0");

    active_ = !entries_.empty();
    if (active_) {
        ZENITH_LOG_INFO("Applied kernel latency and memory optimizations (split_lock=0, THP, compaction=0).");
    }
    return active_;
}

void SysctlTuner::rollback() {
    if (!active_) return;
    ZENITH_LOG_INFO("Restoring original kernel sysctl settings...");
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        if (it->modified) {
            write_file_string(it->path, it->original_value);
            ZENITH_LOG_DEBUG("Restored " + it->path.string() + " to " + it->original_value);
        }
    }
    entries_.clear();
    active_ = false;
}

} // namespace zenith
