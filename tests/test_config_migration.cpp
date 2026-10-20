// tests/test_config_migration.cpp
// Unit tests for configuration schema versioning and seamless migration

#include "zenith/config_migration.hpp"
#include <cassert>
#include <iostream>

void test_version_detection() {
    std::string v1_cfg = "version = 1.0.0\nactive_profile = balanced\n";
    assert(zenith::ConfigMigrator::detect_version(v1_cfg) == "1.0.0");

    std::string unversioned_cfg = "active_profile = aggressive\nswappiness = 10\n";
    assert(zenith::ConfigMigrator::detect_version(unversioned_cfg) == "1.0.0");

    std::string latest_cfg = "version = 1.1.0\nactive_profile = safe\n";
    assert(zenith::ConfigMigrator::detect_version(latest_cfg) == "1.1.0");

    std::cout << "[PASS] test_version_detection" << std::endl;
}

void test_v1_0_0_to_latest_migration() {
    std::string legacy =
        "# Zenith v1.0.0 Legacy Configuration\n"
        "version = 1.0.0\n"
        "active_profile = aggressive\n"
        "swappiness = 15\n"
        "dirty_ratio = 8\n"
        "cgroup_isolation_enabled = true\n";

    auto res = zenith::ConfigMigrator::migrate_to_latest(legacy);
    assert(res.success);
    assert(res.needed_migration);
    assert(res.from_version == "1.0.0");
    assert(res.to_version == "1.1.0");
    assert(res.migrated_content.find("version = 1.1.0") != std::string::npos);
    assert(res.migrated_content.find("active_profile = aggressive") != std::string::npos);
    assert(res.migrated_content.find("swappiness = 15") != std::string::npos);

    // Verify it parses cleanly
    auto [cfg, parse_res] = zenith::Config::parse_string(res.migrated_content);
    assert(parse_res.success);
    assert(cfg.active_profile == zenith::Profile::Aggressive);
    assert(cfg.swappiness == 15);
    assert(cfg.dirty_ratio == 8);

    std::cout << "[PASS] test_v1_0_0_to_latest_migration" << std::endl;
}

void test_migration_idempotence() {
    std::string current =
        "version = 1.1.0\n"
        "active_profile = balanced\n"
        "swappiness = 10\n";

    auto res = zenith::ConfigMigrator::migrate_to_latest(current);
    assert(res.success);
    assert(!res.needed_migration);
    assert(res.from_version == "1.1.0");
    assert(res.to_version == "1.1.0");
    assert(res.migrated_content == current);

    std::cout << "[PASS] test_migration_idempotence" << std::endl;
}

void test_invalid_schema_rejection() {
    std::string corrupted =
        "version = 1.0.0\n"
        "swappiness = 9999\n"; // Invalid range (0..100)

    auto res = zenith::ConfigMigrator::migrate_to_latest(corrupted);
    assert(!res.success);
    assert(res.needed_migration);

    std::cout << "[PASS] test_invalid_schema_rejection" << std::endl;
}

int main() {
    std::cout << "Starting Config Migration Test Suite..." << std::endl;

    test_version_detection();
    test_v1_0_0_to_latest_migration();
    test_migration_idempotence();
    test_invalid_schema_rejection();

    std::cout << "ALL CONFIG MIGRATION TESTS PASSED!" << std::endl;
    return 0;
}
