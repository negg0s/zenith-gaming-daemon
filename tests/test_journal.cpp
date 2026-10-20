#include "zenith/journal.hpp"
#include <cassert>
#include <iostream>

void test_atomic_journal_wal() {
    fs::path test_dir = fs::current_path() / "test_run_journal";
    std::error_code ec;
    fs::remove_all(test_dir, ec);
    fs::create_directories(test_dir, ec);

    fs::path journal_file = test_dir / "journal.json";

    {
        zenith::StateJournal journal(journal_file);
        uint64_t id1 = journal.record_pending(zenith::MutationType::SysctlString,
                                              "/proc/sys/kernel/split_lock_mitigate",
                                              "1", "0");
        assert(id1 == 1);
        assert(journal.get_entries().size() == 1);
        assert(journal.get_entries()[0].state == zenith::EntryState::Pending);

        bool app_ok = journal.mark_applied(id1);
        assert(app_ok);
        assert(journal.get_entries()[0].state == zenith::EntryState::Applied);

        uint64_t id2 = journal.record_pending(zenith::MutationType::SysctlInt,
                                              "/proc/sys/vm/swappiness",
                                              "60", "10");
        assert(id2 == 2);
    }

    // Verify file exists on disk and loads back accurately
    assert(fs::exists(journal_file));

    {
        zenith::StateJournal loaded_journal(journal_file);
        assert(loaded_journal.get_entries().size() == 2);
        assert(loaded_journal.get_entries()[0].state == zenith::EntryState::Applied);
        assert(loaded_journal.get_entries()[1].state == zenith::EntryState::Pending);

        // Perform recovery
        int custom_reverts = 0;
        auto report = loaded_journal.recover_all([&](const zenith::JournalEntry& entry) {
            (void)entry;
            custom_reverts++;
            return true;
        });

        assert(report.total_entries == 2);
        assert(report.successfully_reverted == 2);
        assert(custom_reverts == 2);
        assert(report.is_clean());
    }

    // Re-verify that journal now records all entries as Reverted
    {
        zenith::StateJournal reloaded(journal_file);
        for (const auto& entry : reloaded.get_entries()) {
            assert(entry.state == zenith::EntryState::Reverted);
        }
    }

    fs::remove_all(test_dir, ec);
    std::cout << "[PASS] test_atomic_journal_wal\n";
}

int main() {
    test_atomic_journal_wal();
    return 0;
}
