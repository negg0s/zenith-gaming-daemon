// SPDX-License-Identifier: MIT
#include "zenith/sysctl_tuner.hpp"

namespace zenith {

SysctlTuner::SysctlTuner(StateJournal* journal)
    : journal_(journal) {}

SysctlTuner::~SysctlTuner() {
    rollback();
}

bool SysctlTuner::set_param(const fs::path& path, std::string_view value, MutationType type, int fault_point) {
    if (!fs::exists(path)) return false;

    auto current = read_file_string(path);
    if (!current) return false;

    // Trigger fault injection point (simulating crash right before mutation)
    if (fault_point > 0) {
        FaultInjector::trigger_if_hit(fault_point);
    }

    uint64_t j_id = 0;
    if (journal_) {
        j_id = journal_->record_pending(type, path.string(), *current, value);
    }

    // Trigger fault injection point (simulating crash between WAL pending and write)
    if (fault_point > 0) {
        FaultInjector::trigger_if_hit(fault_point + 100);
    }

    SysctlEntry entry;
    entry.path = path;
    entry.original_value = *current;
    entry.tuned_value = std::string(value);
    entry.journal_id = j_id;

    if (write_file_string(path, value)) {
        entry.modified = true;
        entries_.push_back(entry);

        if (journal_ && j_id > 0) {
            journal_->mark_applied(j_id);
        }

        // Trigger fault injection point (simulating crash right after mutation applied)
        if (fault_point > 0) {
            FaultInjector::trigger_if_hit(fault_point + 200);
        }

        ZENITH_LOG_DEBUG("Sysctl " + path.string() + " = " + std::string(value) + " (was " + *current + ")");
        return true;
    } else {
        if (journal_ && j_id > 0) {
            journal_->mark_failed(j_id);
        }
    }
    return false;
}

bool SysctlTuner::apply(bool disable_split_lock, bool enable_thp, int swappiness, int dirty_ratio, int compaction) {
    entries_.clear();

    if (disable_split_lock) {
        set_param("/proc/sys/kernel/split_lock_mitigate", "0", MutationType::SysctlString, 1);
    }

    if (enable_thp) {
        set_param("/sys/kernel/mm/transparent_hugepage/enabled", "always", MutationType::SysfsToken, 2);
        set_param("/sys/kernel/mm/transparent_hugepage/defrag", "defer+madvise", MutationType::SysfsToken, 3);
    }

    set_param("/proc/sys/vm/swappiness", std::to_string(swappiness), MutationType::SysctlInt, 4);
    set_param("/proc/sys/vm/dirty_ratio", std::to_string(dirty_ratio), MutationType::SysctlInt, 5);
    set_param("/proc/sys/vm/compaction_proactiveness", std::to_string(compaction), MutationType::SysctlInt, 6);
    set_param("/proc/sys/kernel/sched_autogroup_enabled", "0", MutationType::SysctlInt, 7);

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
            if (journal_ && it->journal_id > 0) {
                journal_->mark_reverted(it->journal_id);
            }
            ZENITH_LOG_DEBUG("Restored " + it->path.string() + " to " + it->original_value);
        }
    }
    entries_.clear();
    active_ = false;
}

} // namespace zenith
