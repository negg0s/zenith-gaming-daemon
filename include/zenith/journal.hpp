// SPDX-License-Identifier: MIT
#pragma once

#include "zenith/common.hpp"
#include <chrono>

namespace zenith {

enum class MutationType {
    SysctlString,
    SysfsToken,
    SysctlInt,
    GpuAmdDpm,
    GpuNvidiaSmi,
    CgroupSlice,
    ProcSched,
    ProcNice,
    ProcIoPrio,
    ProcAffinity,
    PipewireQuantum,
    IrqAffinity,
    GenericFile
};

enum class EntryState {
    Pending,
    Applied,
    Reverted,
    Failed
};

std::string mutation_type_to_string(MutationType type);
MutationType mutation_type_from_string(std::string_view str);

std::string entry_state_to_string(EntryState state);
EntryState entry_state_from_string(std::string_view str);

struct JournalEntry {
    uint64_t id = 0;
    MutationType type = MutationType::GenericFile;
    std::string target_path;
    std::string original_value;
    std::string target_value;
    EntryState state = EntryState::Pending;
    std::string timestamp;
};

struct RollbackReport {
    size_t total_entries = 0;
    size_t successfully_reverted = 0;
    size_t failed_reverts = 0;
    size_t skipped_entries = 0;
    std::vector<std::string> error_messages;

    bool is_clean() const {
        return failed_reverts == 0;
    }
};

class StateJournal {
public:
    explicit StateJournal(fs::path journal_path = "/run/zenith/journal.json");
    ~StateJournal() = default;

    // Schema version
    static constexpr int SCHEMA_VERSION = 1;

    // Atomic persistence: write to tmp -> fsync -> rename -> fsync directory
    bool atomic_save();
    bool load();

    // Write-Ahead Logging (WAL)
    uint64_t record_pending(MutationType type,
                            std::string_view target_path,
                            std::string_view original_value,
                            std::string_view target_value);

    bool mark_applied(uint64_t entry_id);
    bool mark_reverted(uint64_t entry_id);
    bool mark_failed(uint64_t entry_id);

    // Crash-only recovery: idempotent rollback of all un-reverted entries
    RollbackReport recover_all(std::function<bool(const JournalEntry&)> custom_handler = nullptr);

    // Rollback execution
    static bool execute_entry_revert(const JournalEntry& entry);

    const std::vector<JournalEntry>& get_entries() const { return entries_; }
    const fs::path& get_path() const { return journal_path_; }
    void clear();

private:
    fs::path journal_path_;
    std::vector<JournalEntry> entries_;
    uint64_t next_id_ = 1;

    std::string serialize_json() const;
    bool deserialize_json(std::string_view json_str);
    static bool sync_directory(const fs::path& dir_path);
};

} // namespace zenith
