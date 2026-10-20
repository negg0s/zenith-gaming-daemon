#include "zenith/config.hpp"

namespace zenith {

Config Config::default_config() {
    return Config{};
}

Config Config::load_from_file(const fs::path& path) {
    Config cfg = default_config();
    std::ifstream file(path);
    if (!file.is_open()) {
        ZENITH_LOG_WARN("Config file not found at " + path.string() + ", using defaults.");
        return cfg;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line.starts_with('#') || line.starts_with(';')) continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        if (key == "cgroup_isolation_enabled") cfg.cgroup_isolation_enabled = (val == "true" || val == "1");
        else if (key == "vcache_pinning_enabled") cfg.vcache_pinning_enabled = (val == "true" || val == "1");
        else if (key == "hybrid_pinning_enabled") cfg.hybrid_pinning_enabled = (val == "true" || val == "1");
        else if (key == "sysctl_tuning_enabled") cfg.sysctl_tuning_enabled = (val == "true" || val == "1");
        else if (key == "split_lock_mitigation_disabled") cfg.split_lock_mitigation_disabled = (val == "true" || val == "1");
        else if (key == "thp_always_enabled") cfg.thp_always_enabled = (val == "true" || val == "1");
        else if (key == "gpu_clock_lock_enabled") cfg.gpu_clock_lock_enabled = (val == "true" || val == "1");
        else if (key == "realtime_process_priority") cfg.realtime_process_priority = (val == "true" || val == "1");
        else if (key == "pipewire_quantum_tuning") cfg.pipewire_quantum_tuning = (val == "true" || val == "1");
        else if (key == "pipewire_latency_numerator") cfg.pipewire_latency_numerator = std::stoi(val);
        else if (key == "pipewire_latency_denominator") cfg.pipewire_latency_denominator = std::stoi(val);
        else if (key == "swappiness") cfg.swappiness = std::stoi(val);
        else if (key == "dirty_ratio") cfg.dirty_ratio = std::stoi(val);
        else if (key == "compaction_proactiveness") cfg.compaction_proactiveness = std::stoi(val);
        else if (key == "poll_interval_ms") cfg.poll_interval_ms = std::stoi(val);
    }

    return cfg;
}

} // namespace zenith
