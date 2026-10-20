#include "zenith/config.hpp"
#include <cassert>
#include <iostream>

void test_default_config() {
    auto cfg = zenith::Config::default_config();
    assert(cfg.cgroup_isolation_enabled == true);
    assert(cfg.split_lock_mitigation_disabled == true);
    assert(cfg.pipewire_latency_numerator == 64);
    assert(cfg.pipewire_latency_denominator == 48000);
    assert(cfg.swappiness == 10);
    std::cout << "[PASS] test_default_config\n";
}

int main() {
    test_default_config();
    return 0;
}
