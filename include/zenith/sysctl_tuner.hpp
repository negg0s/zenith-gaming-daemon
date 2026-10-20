#pragma once

#include "zenith/common.hpp"
#include "zenith/journal.hpp"
#include "zenith/fault_injector.hpp"

namespace zenith {

struct SysctlEntry {
    fs::path path;
    std::string original_value;
    std::string tuned_value;
    bool modified = false;
    uint64_t journal_id = 0;
};

class SysctlTuner {
public:
    explicit SysctlTuner(StateJournal* journal = nullptr);
    ~SysctlTuner();

    void set_journal(StateJournal* journal) { journal_ = journal; }

    bool apply(bool disable_split_lock, bool enable_thp, int swappiness, int dirty_ratio, int compaction);
    void rollback();

    bool is_active() const { return active_; }
    const std::vector<SysctlEntry>& get_entries() const { return entries_; }

private:
    StateJournal* journal_ = nullptr;
    std::vector<SysctlEntry> entries_;
    bool active_ = false;

    bool set_param(const fs::path& path, std::string_view value, MutationType type = MutationType::SysctlString, int fault_point = 0);
};

} // namespace zenith
