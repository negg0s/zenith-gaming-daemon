#include "zenith/safe_mode.hpp"
#include <cassert>
#include <iostream>

namespace fs = std::filesystem;

void test_safe_mode_crash_loop() {
    fs::path test_dir = fs::current_path() / "test_run_safemode";
    std::error_code ec;
    fs::remove_all(test_dir, ec);
    fs::create_directories(test_dir, ec);

    fs::path history_file = test_dir / "crash_history.json";

    zenith::SafeModeManager mgr(history_file, 10, 3);
    assert(!mgr.evaluate_and_record_boot());
    assert(!mgr.is_safe_mode_active());

    // Record crash 1
    mgr.record_crash("Crash SIGSEGV 1");
    assert(!mgr.evaluate_and_record_boot());

    // Record crash 2
    mgr.record_crash("Crash SIGABRT 2");
    assert(!mgr.evaluate_and_record_boot());

    // Record crash 3 -> Threshold reached!
    mgr.record_crash("Crash SIGBUS 3");
    bool safe_engaged = mgr.evaluate_and_record_boot();
    assert(safe_engaged);
    assert(mgr.is_safe_mode_active());
    assert(mgr.get_recent_crash_count() == 3);

    // Operator reset
    mgr.reset();
    assert(!mgr.evaluate_and_record_boot());
    assert(!mgr.is_safe_mode_active());

    fs::remove_all(test_dir, ec);
    std::cout << "[PASS] test_safe_mode_crash_loop\n";
}

int main() {
    test_safe_mode_crash_loop();
    return 0;
}
