#include "zenith/sysctl_tuner.hpp"
#include <cassert>
#include <iostream>

void test_sysctl_dummy() {
    zenith::SysctlTuner tuner;
    // On a non-root or non-existent path it gracefully handles errors
    bool res = tuner.apply(true, true, 10, 5, 0);
    // Even if no sysctl files were writable, it shouldn't crash
    tuner.rollback();
    std::cout << "[PASS] test_sysctl_dummy\n";
}

int main() {
    test_sysctl_dummy();
    return 0;
}
