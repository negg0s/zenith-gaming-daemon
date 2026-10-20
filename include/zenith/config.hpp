#pragma once

#include "zenith/common.hpp"
#include "zenith/tweaks/tweak_interface.hpp"

namespace zenith {

struct ConfigResult {
    bool success = false;
    std::string error_message;
};

struct Config {
    // Schema version
    std::string version = "1.1.0";

    // Active Profile: safe, balanced (default), aggressive
    Profile active_profile = Profile::Balanced;

    // Core features
    bool cgroup_isolation_enabled = true;
    bool vcache_pinning_enabled = true;
    bool hybrid_pinning_enabled = true;
    bool sysctl_tuning_enabled = true;
    bool split_lock_mitigation_disabled = true;
    bool thp_always_enabled = true;
    bool gpu_clock_lock_enabled = true;
    bool realtime_process_priority = true;
    bool pipewire_quantum_tuning = true;

    // Advanced latency settings
    int pipewire_latency_numerator = 64;
    int pipewire_latency_denominator = 48000;
    int swappiness = 10;
    int dirty_ratio = 5;
    int compaction_proactiveness = 0;
    int poll_interval_ms = 500;

    // Custom process lists
    std::vector<std::string> custom_game_patterns = {
        "wine", "proton", "steamapps", "lutris", "heroic", "bottles", "gamescope"
    };

    std::vector<std::string> background_process_names = {
        "chrome", "firefox", "brave", "discord", "spotify", "telegram-desktop",
        "steamwebhelper", "transmission", "qbittorrent", "obs"
    };

    static Config default_config();
    static Config load_from_file(const fs::path& path);
    static std::pair<Config, ConfigResult> parse_string(std::string_view content);

    // Max allowed config file size: 64 KB
    static constexpr size_t MAX_CONFIG_SIZE = 64 * 1024;
};

} // namespace zenith
