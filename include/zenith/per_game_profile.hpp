#pragma once

#include "zenith/tweaks/tweak_interface.hpp"
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

namespace zenith {

struct GameProfileRule {
    std::string identifier; // AppID (e.g. "730") or exe pattern (e.g. "cs2", "cyberpunk2077")
    std::optional<Profile> profile_override;
    std::unordered_set<std::string> blacklisted_tweaks;
    std::unordered_map<std::string, std::string> custom_overrides;
};

class PerGameProfileManager {
public:
    PerGameProfileManager() = default;

    static PerGameProfileManager load_from_string(std::string_view content);
    static PerGameProfileManager load_from_file(const fs::path& path);

    void add_rule(GameProfileRule rule);

    GameProfileRule resolve(std::string_view game_name, int steam_appid = 0,
                            Profile default_profile = Profile::Balanced) const;

    const std::vector<GameProfileRule>& get_rules() const { return rules_; }

private:
    std::vector<GameProfileRule> rules_;
};

} // namespace zenith
