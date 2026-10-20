#include "zenith/config.hpp"
#include <unordered_set>

namespace zenith {

Config Config::default_config() {
    return Config{};
}

static bool parse_bool_strict(std::string_view val, bool& out, std::string& err) {
    if (val == "true" || val == "1") {
        out = true;
        return true;
    }
    if (val == "false" || val == "0") {
        out = false;
        return true;
    }
    err = "Invalid boolean value: '" + std::string(val) + "'. Expected 'true', 'false', '1', or '0'.";
    return false;
}

static bool parse_int_range(std::string_view val, int min_val, int max_val, int& out, std::string& err) {
    try {
        size_t idx = 0;
        int parsed = std::stoi(std::string(val), &idx);
        if (idx != val.size()) {
            err = "Trailing characters after integer: " + std::string(val);
            return false;
        }
        if (parsed < min_val || parsed > max_val) {
            err = "Integer out of bounds: " + std::to_string(parsed) +
                  " (allowed: [" + std::to_string(min_val) + ", " + std::to_string(max_val) + "])";
            return false;
        }
        out = parsed;
        return true;
    } catch (...) {
        err = "Failed to parse integer: " + std::string(val);
        return false;
    }
}

std::pair<Config, ConfigResult> Config::parse_string(std::string_view content) {
    Config cfg = default_config();
    ConfigResult res;
    res.success = true;

    if (content.size() > MAX_CONFIG_SIZE) {
        res.success = false;
        res.error_message = "Config file exceeds maximum allowed size (" +
                            std::to_string(MAX_CONFIG_SIZE) + " bytes)";
        return { cfg, res };
    }

    static const std::unordered_set<std::string_view> KNOWN_KEYS = {
        "cgroup_isolation_enabled",
        "vcache_pinning_enabled",
        "hybrid_pinning_enabled",
        "sysctl_tuning_enabled",
        "split_lock_mitigation_disabled",
        "thp_always_enabled",
        "gpu_clock_lock_enabled",
        "realtime_process_priority",
        "pipewire_quantum_tuning",
        "pipewire_latency_numerator",
        "pipewire_latency_denominator",
        "swappiness",
        "dirty_ratio",
        "compaction_proactiveness",
        "poll_interval_ms"
    };

    auto lines = split(content, '\n');
    int line_num = 0;

    for (const auto& raw_line : lines) {
        line_num++;
        std::string line = trim(raw_line);
        if (line.empty() || line.starts_with('#') || line.starts_with(';')) continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) {
            res.success = false;
            res.error_message = "Line " + std::to_string(line_num) + ": Missing '=' separator";
            return { cfg, res };
        }

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        if (KNOWN_KEYS.find(key) == KNOWN_KEYS.end()) {
            res.success = false;
            res.error_message = "Line " + std::to_string(line_num) + ": Unknown configuration key: '" + key + "'";
            return { cfg, res };
        }

        std::string err;
        if (key == "cgroup_isolation_enabled") {
            if (!parse_bool_strict(val, cfg.cgroup_isolation_enabled, err)) { res.success = false; res.error_message = err; return {cfg, res}; }
        } else if (key == "vcache_pinning_enabled") {
            if (!parse_bool_strict(val, cfg.vcache_pinning_enabled, err)) { res.success = false; res.error_message = err; return {cfg, res}; }
        } else if (key == "hybrid_pinning_enabled") {
            if (!parse_bool_strict(val, cfg.hybrid_pinning_enabled, err)) { res.success = false; res.error_message = err; return {cfg, res}; }
        } else if (key == "sysctl_tuning_enabled") {
            if (!parse_bool_strict(val, cfg.sysctl_tuning_enabled, err)) { res.success = false; res.error_message = err; return {cfg, res}; }
        } else if (key == "split_lock_mitigation_disabled") {
            if (!parse_bool_strict(val, cfg.split_lock_mitigation_disabled, err)) { res.success = false; res.error_message = err; return {cfg, res}; }
        } else if (key == "thp_always_enabled") {
            if (!parse_bool_strict(val, cfg.thp_always_enabled, err)) { res.success = false; res.error_message = err; return {cfg, res}; }
        } else if (key == "gpu_clock_lock_enabled") {
            if (!parse_bool_strict(val, cfg.gpu_clock_lock_enabled, err)) { res.success = false; res.error_message = err; return {cfg, res}; }
        } else if (key == "realtime_process_priority") {
            if (!parse_bool_strict(val, cfg.realtime_process_priority, err)) { res.success = false; res.error_message = err; return {cfg, res}; }
        } else if (key == "pipewire_quantum_tuning") {
            if (!parse_bool_strict(val, cfg.pipewire_quantum_tuning, err)) { res.success = false; res.error_message = err; return {cfg, res}; }
        } else if (key == "pipewire_latency_numerator") {
            if (!parse_int_range(val, 16, 8192, cfg.pipewire_latency_numerator, err)) { res.success = false; res.error_message = err; return {cfg, res}; }
        } else if (key == "pipewire_latency_denominator") {
            if (!parse_int_range(val, 44100, 192000, cfg.pipewire_latency_denominator, err)) { res.success = false; res.error_message = err; return {cfg, res}; }
        } else if (key == "swappiness") {
            if (!parse_int_range(val, 0, 200, cfg.swappiness, err)) { res.success = false; res.error_message = err; return {cfg, res}; }
        } else if (key == "dirty_ratio") {
            if (!parse_int_range(val, 1, 90, cfg.dirty_ratio, err)) { res.success = false; res.error_message = err; return {cfg, res}; }
        } else if (key == "compaction_proactiveness") {
            if (!parse_int_range(val, 0, 100, cfg.compaction_proactiveness, err)) { res.success = false; res.error_message = err; return {cfg, res}; }
        } else if (key == "poll_interval_ms") {
            if (!parse_int_range(val, 50, 10000, cfg.poll_interval_ms, err)) { res.success = false; res.error_message = err; return {cfg, res}; }
        }
    }

    return { cfg, res };
}

Config Config::load_from_file(const fs::path& path) {
    if (!fs::exists(path)) {
        ZENITH_LOG_WARN("Config file not found at " + path.string() + ", using defaults.");
        return default_config();
    }

    auto content = read_file_string(path);
    if (!content) {
        ZENITH_LOG_ERROR("Failed to read config file: " + path.string());
        return default_config();
    }

    auto [cfg, res] = parse_string(*content);
    if (!res.success) {
        ZENITH_LOG_ERROR("Configuration parse error in " + path.string() + ": " + res.error_message + ". Using defaults.");
        return default_config();
    }

    return cfg;
}

} // namespace zenith
