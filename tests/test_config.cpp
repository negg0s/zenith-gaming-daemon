#include "zenith/config.hpp"
#include <cassert>
#include <iostream>

void test_default_config() {
    auto cfg = zenith::Config::default_config();
    assert(cfg.active_profile == zenith::Profile::Balanced);
    assert(cfg.cgroup_isolation_enabled == true);
    assert(cfg.split_lock_mitigation_disabled == true);
    assert(cfg.pipewire_latency_numerator == 64);
    assert(cfg.pipewire_latency_denominator == 48000);
    assert(cfg.swappiness == 10);
    assert(cfg.sched_ext_enabled == false);
    assert(cfg.sched_ext_scheduler == "scx_lavd");
    std::cout << "[PASS] test_default_config\n";
}

void test_sched_ext_config() {
    auto [cfg, res] = zenith::Config::parse_string(
        "sched_ext_enabled = true\n"
        "sched_ext_scheduler = scx_bpfland\n");
    assert(res.success);
    assert(cfg.sched_ext_enabled == true);
    assert(cfg.sched_ext_scheduler == "scx_bpfland");

    auto [bad, bad_res] = zenith::Config::parse_string(
        "sched_ext_scheduler = ../scx_lavd\n");
    assert(!bad_res.success);

    std::cout << "[PASS] test_sched_ext_config\n";
}

int main() {
    test_default_config();
    test_sched_ext_config();
    return 0;
}
