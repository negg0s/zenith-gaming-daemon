#include "zenith/per_game_profile.hpp"

namespace zenith {

void PerGameProfileManager::add_rule(GameProfileRule rule) {
    rules_.push_back(std::move(rule));
}

PerGameProfileManager PerGameProfileManager::load_from_string(std::string_view content) {
    PerGameProfileManager mgr;
    auto lines = split(content, '\n');

    GameProfileRule current_rule;
    bool in_section = false;

    for (const auto& raw_line : lines) {
        std::string line = trim(raw_line);
        if (line.empty() || line.starts_with('#') || line.starts_with(';')) continue;

        if (line.starts_with('[') && line.ends_with(']')) {
            if (in_section && !current_rule.identifier.empty()) {
                mgr.add_rule(current_rule);
            }
            current_rule = GameProfileRule{};
            current_rule.identifier = line.substr(1, line.size() - 2);
            in_section = true;
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        if (key == "profile") {
            current_rule.profile_override = string_to_profile(val);
        } else if (key == "blacklist") {
            auto tokens = split(val, ',');
            for (const auto& t : tokens) {
                std::string item = trim(t);
                if (!item.empty()) current_rule.blacklisted_tweaks.insert(item);
            }
        } else {
            current_rule.custom_overrides[key] = val;
        }
    }

    if (in_section && !current_rule.identifier.empty()) {
        mgr.add_rule(current_rule);
    }

    return mgr;
}

PerGameProfileManager PerGameProfileManager::load_from_file(const fs::path& path) {
    if (!fs::exists(path)) {
        return PerGameProfileManager{};
    }
    auto content = read_file_string(path);
    if (!content) return PerGameProfileManager{};
    return load_from_string(*content);
}

GameProfileRule PerGameProfileManager::resolve(std::string_view game_name, int steam_appid,
                                              Profile default_profile) const {
    std::string appid_str = (steam_appid > 0) ? std::to_string(steam_appid) : "";

    // 1. Check exact Steam AppID match
    if (!appid_str.empty()) {
        for (const auto& r : rules_) {
            if (r.identifier == appid_str) {
                return r;
            }
        }
    }

    // 2. Check substring / exe pattern match
    for (const auto& r : rules_) {
        if (game_name.find(r.identifier) != std::string_view::npos) {
            return r;
        }
    }

    // 3. Default fallback
    GameProfileRule fallback;
    fallback.identifier = std::string(game_name);
    fallback.profile_override = default_profile;
    return fallback;
}

} // namespace zenith
