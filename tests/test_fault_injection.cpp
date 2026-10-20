#include "zenith/sysctl_tuner.hpp"
#include "zenith/fault_injector.hpp"
#include <cassert>
#include <iostream>
#include <iomanip>

namespace fs = std::filesystem;

struct MockEnvironment {
    fs::path root;
    fs::path split_lock;
    fs::path thp_enabled;
    fs::path thp_defrag;
    fs::path swappiness;
    fs::path dirty_ratio;
    fs::path compaction;
    fs::path autogroup;

    static MockEnvironment setup(const fs::path& base_dir) {
        MockEnvironment env;
        env.root = base_dir;
        env.split_lock = env.root / "proc/sys/kernel/split_lock_mitigate";
        env.thp_enabled = env.root / "sys/kernel/mm/transparent_hugepage/enabled";
        env.thp_defrag = env.root / "sys/kernel/mm/transparent_hugepage/defrag";
        env.swappiness = env.root / "proc/sys/vm/swappiness";
        env.dirty_ratio = env.root / "proc/sys/vm/dirty_ratio";
        env.compaction = env.root / "proc/sys/vm/compaction_proactiveness";
        env.autogroup = env.root / "proc/sys/kernel/sched_autogroup_enabled";

        fs::create_directories(env.split_lock.parent_path());
        fs::create_directories(env.thp_enabled.parent_path());
        fs::create_directories(env.swappiness.parent_path());

        env.reset_to_baseline();
        return env;
    }

    void reset_to_baseline() {
        zenith::write_file_string(split_lock, "1");
        zenith::write_file_string(thp_enabled, "always [madvise] never");
        zenith::write_file_string(thp_defrag, "[defer] madvise never");
        zenith::write_file_string(swappiness, "60");
        zenith::write_file_string(dirty_ratio, "20");
        zenith::write_file_string(compaction, "20");
        zenith::write_file_string(autogroup, "1");
    }

    bool verify_baseline() const {
        return *zenith::read_file_string(split_lock) == "1" &&
               *zenith::read_file_string(thp_enabled) == "always [madvise] never" &&
               *zenith::read_file_string(thp_defrag) == "[defer] madvise never" &&
               *zenith::read_file_string(swappiness) == "60" &&
               *zenith::read_file_string(dirty_ratio) == "20" &&
               *zenith::read_file_string(compaction) == "20" &&
               *zenith::read_file_string(autogroup) == "1";
    }
};

class MockSysctlTuner {
public:
    MockSysctlTuner(zenith::StateJournal* journal, const MockEnvironment& env)
        : journal_(journal), env_(env) {}

    bool apply() {
        set_param(env_.split_lock, "0", zenith::MutationType::SysctlString, 1);
        set_param(env_.thp_enabled, "always", zenith::MutationType::SysfsToken, 2);
        set_param(env_.thp_defrag, "defer+madvise", zenith::MutationType::SysfsToken, 3);
        set_param(env_.swappiness, "10", zenith::MutationType::SysctlInt, 4);
        set_param(env_.dirty_ratio, "5", zenith::MutationType::SysctlInt, 5);
        set_param(env_.compaction, "0", zenith::MutationType::SysctlInt, 6);
        set_param(env_.autogroup, "0", zenith::MutationType::SysctlInt, 7);
        return true;
    }

private:
    zenith::StateJournal* journal_;
    MockEnvironment env_;

    void set_param(const fs::path& path, std::string_view val, zenith::MutationType type, int pt) {
        auto cur = zenith::read_file_string(path).value_or("");
        zenith::FaultInjector::trigger_if_hit(pt);

        uint64_t j_id = journal_->record_pending(type, path.string(), cur, val);
        zenith::FaultInjector::trigger_if_hit(pt + 100);

        zenith::write_file_string(path, val);
        journal_->mark_applied(j_id);
        zenith::FaultInjector::trigger_if_hit(pt + 200);
    }
};

void run_fault_injection_matrix() {
    fs::path base_dir = fs::current_path() / "test_run_fault_inject";
    std::error_code ec;
    fs::remove_all(base_dir, ec);
    fs::create_directories(base_dir, ec);

    auto env = MockEnvironment::setup(base_dir);
    assert(env.verify_baseline());

    std::cout << "\n=========================================================================================\n";
    std::cout << "                      FAULT INJECTION MATRIX: CRASH POINT x RESULT                      \n";
    std::cout << "=========================================================================================\n";
    std::cout << std::left << std::setw(8) << "Step"
              << std::setw(16) << "Crash Phase"
              << std::setw(14) << "Point Code"
              << std::setw(18) << "Crash Triggered"
              << std::setw(20) << "Journal Recovery"
              << std::setw(12) << "Baseline Restored\n";
    std::cout << "-----------------------------------------------------------------------------------------\n";

    // 7 mutation points x 3 crash phases = 21 crash injection scenarios
    int total_tests = 0;
    int passed_tests = 0;

    for (int step = 1; step <= 7; ++step) {
        int phases[] = { step, step + 100, step + 200 };
        const char* phase_names[] = { "Pre-Mutation", "Mid-WAL-Pending", "Post-Applied" };

        for (int p_idx = 0; p_idx < 3; ++p_idx) {
            total_tests++;
            int crash_point = phases[p_idx];
            const char* phase_name = phase_names[p_idx];

            env.reset_to_baseline();
            fs::path journal_path = base_dir / "journal.json";
            fs::remove(journal_path, ec);

            zenith::StateJournal journal(journal_path);
            MockSysctlTuner tuner(&journal, env);

            zenith::FaultInjector::set_target_crash_point(crash_point);
            bool caught = false;
            try {
                tuner.apply();
            } catch (const zenith::FaultInjectionException& e) {
                caught = true;
                assert(e.get_point() == crash_point);
            }
            assert(caught);

            // Crash-only recovery simulation: next startup reads journal and recovers
            zenith::StateJournal recovery_journal(journal_path);
            auto report = recovery_journal.recover_all();

            bool baseline_intact = env.verify_baseline();
            assert(baseline_intact);
            passed_tests++;

            std::cout << std::left << std::setw(8) << step
                      << std::setw(16) << phase_name
                      << std::setw(14) << crash_point
                      << std::setw(18) << "[CAUGHT]"
                      << std::setw(20) << ("[RECOVERED " + std::to_string(report.successfully_reverted) + "]")
                      << std::setw(12) << "[PASS]"
                      << "\n";
        }
    }

    std::cout << "=========================================================================================\n";
    std::cout << "Matrix Result: " << passed_tests << " / " << total_tests << " scenarios PASSED (100% green)\n";
    std::cout << "=========================================================================================\n\n";

    assert(passed_tests == total_tests);
    fs::remove_all(base_dir, ec);
}

int main() {
    run_fault_injection_matrix();
    return 0;
}
