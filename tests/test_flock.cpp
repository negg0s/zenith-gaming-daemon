#include "zenith/lock_guard.hpp"
#include <cassert>
#include <iostream>

void test_flock_concurrency() {
    fs::path test_dir = fs::current_path() / "test_run_flock";
    std::error_code ec;
    fs::remove_all(test_dir, ec);
    fs::create_directories(test_dir, ec);

    fs::path lock_file = test_dir / "zenith.lock";

    zenith::DaemonLock lock1(lock_file);
    bool ok1 = lock1.acquire();
    assert(ok1);
    assert(lock1.is_acquired());

    // Second lock attempt on same file must fail
    zenith::DaemonLock lock2(lock_file);
    bool ok2 = lock2.acquire();
    assert(!ok2);
    assert(!lock2.is_acquired());

    // Release first lock
    lock1.release();
    assert(!lock1.is_acquired());

    // Now second lock attempt must succeed
    bool ok3 = lock2.acquire();
    assert(ok3);
    assert(lock2.is_acquired());
    lock2.release();

    fs::remove_all(test_dir, ec);
    std::cout << "[PASS] test_flock_concurrency\n";
}

int main() {
    test_flock_concurrency();
    return 0;
}
