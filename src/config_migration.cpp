// SPDX-License-Identifier: MIT
#include "zenith/config_migration.hpp"
#include <algorithm>
#include <sstream>

namespace zenith {

std::string ConfigMigrator::detect_version(std::string_view content) {
    std::istringstream stream{std::string(content)};
    std::string line;

    while (std::getline(stream, line)) {
        // Strip leading whitespace
        auto first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#' || line[first] == ';') {
            continue;
        }

        auto eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = line.substr(first, eq_pos - first);
            // Trim key
            auto last = key.find_last_not_of(" \t");
            if (last != std::string::npos) key = key.substr(0, last + 1);

            if (key == "version" || key == "config_version") {
                std::string val = line.substr(eq_pos + 1);
                auto val_first = val.find_first_not_of(" \t");
                auto val_last = val.find_last_not_of(" \t\r\n");
                if (val_first != std::string::npos && val_last != std::string::npos) {
                    return val.substr(val_first, val_last - val_first + 1);
                }
            }
        }
    }

    // Default legacy baseline version if tag is absent
    return "1.0.0";
}

std::string ConfigMigrator::migrate_v1_0_0_to_v1_1_0(std::string_view content) {
    std::istringstream stream{std::string(content)};
    std::ostringstream out;
    std::string line;
    bool has_version_header = false;

    while (std::getline(stream, line)) {
        auto first = line.find_first_not_of(" \t");
        if (first != std::string::npos && line.substr(first, 7) == "version") {
            out << "version = " << CURRENT_VERSION << " # Automatically migrated from v1.0.0\n";
            has_version_header = true;
            continue;
        }
        out << line << "\n";
    }

    if (!has_version_header) {
        std::string res = "version = " + std::string(CURRENT_VERSION) + " # Automatically migrated from v1.0.0\n" + out.str();
        return res;
    }

    return out.str();
}

MigrationResult ConfigMigrator::migrate_to_latest(std::string_view content) {
    MigrationResult result;
    result.from_version = detect_version(content);
    result.to_version = std::string(CURRENT_VERSION);

    if (result.from_version == CURRENT_VERSION) {
        result.success = true;
        result.needed_migration = false;
        result.migrated_content = std::string(content);
        result.message = "Configuration already at target version " + std::string(CURRENT_VERSION);
        return result;
    }

    std::string migrated = std::string(content);
    if (result.from_version == "1.0.0") {
        migrated = migrate_v1_0_0_to_v1_1_0(migrated);
    }

    // Validate that the migrated content is parseable
    if (!validate_migrated_config(migrated)) {
        result.success = false;
        result.needed_migration = true;
        result.message = "Validation failed on migrated configuration schema";
        return result;
    }

    result.success = true;
    result.needed_migration = true;
    result.migrated_content = migrated;
    result.message = "Successfully migrated configuration from " + result.from_version + " to " + result.to_version;
    return result;
}

bool ConfigMigrator::validate_migrated_config(std::string_view content) {
    auto [cfg, err] = Config::parse_string(content);
    return err.success;
}

} // namespace zenith
