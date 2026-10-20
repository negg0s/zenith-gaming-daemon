// SPDX-License-Identifier: MIT
#pragma once

#include "zenith/config.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace zenith {

struct MigrationResult {
    bool success = false;
    bool needed_migration = false;
    std::string from_version;
    std::string to_version;
    std::string migrated_content;
    std::string message;
};

class ConfigMigrator {
public:
    static constexpr std::string_view CURRENT_VERSION = "1.1.0";

    // Detect version tag from config string
    [[nodiscard]] static std::string detect_version(std::string_view content);

    // Migrate any older configuration string to the CURRENT_VERSION schema
    [[nodiscard]] static MigrationResult migrate_to_latest(std::string_view content);

    // Validates that configuration can be parsed and fulfills all schema constraints
    [[nodiscard]] static bool validate_migrated_config(std::string_view content);

private:
    [[nodiscard]] static std::string migrate_v1_0_0_to_v1_1_0(std::string_view content);
};

} // namespace zenith
