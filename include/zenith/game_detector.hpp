#pragma once

#include "zenith/common.hpp"
#include "zenith/proc_fs.hpp"
#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <string_view>

namespace zenith {

struct DetectedGame {
    int pid = 0;
    std::string name;
    std::string exe_path;
    std::string cmdline;
    std::string steam_appid;
    std::string runner_type; // "steam", "proton", "lutris", "heroic", "bottles", "gamescope", "emulator", "native"
    bool is_wine_or_proton = false;
    bool is_emulator = false;
    bool has_gpu_render_node = false;
    std::chrono::steady_clock::time_point detected_at = std::chrono::steady_clock::now();
};

class GameDetector {
public:
    explicit GameDetector(std::vector<std::string> custom_patterns = {}, IProcFs* fs = nullptr);

    // Initial sweep across procfs to discover any already running games
    [[nodiscard]] std::vector<DetectedGame> scan_all_games() const;

    // Scan for single active game (backward-compatible API)
    [[nodiscard]] std::optional<DetectedGame> scan_for_game() const;

    // Process event handling with debounce & hysteresis
    bool handle_process_exec(int pid);
    bool handle_process_exit(int pid);

    // Deep inspection of a specific PID
    [[nodiscard]] std::optional<DetectedGame> inspect_process(int pid) const;

    // Classification helpers
    [[nodiscard]] bool is_game(int pid) const;
    [[nodiscard]] bool is_non_game_binary(std::string_view comm, std::string_view cmdline) const;
    [[nodiscard]] std::string extract_steam_appid(int pid) const;
    [[nodiscard]] bool has_gpu_context(int pid) const;

    // Multi-game state
    [[nodiscard]] bool has_active_game() const;
    [[nodiscard]] const std::unordered_map<int, DetectedGame>& active_games() const { return active_games_; }
    void clear_active_games() { active_games_.clear(); }

    // Static process utility methods
    [[nodiscard]] static bool is_pid_alive(int pid, IProcFs* fs = nullptr);
    [[nodiscard]] static std::string get_process_name(int pid, IProcFs* fs = nullptr);
    [[nodiscard]] static std::string get_process_cmdline(int pid, IProcFs* fs = nullptr);
    [[nodiscard]] static std::vector<int> find_pids_by_name(std::string_view name, IProcFs* fs = nullptr);

    void set_debounce_ms(uint64_t ms) { debounce_ms_ = ms; }
    void set_hysteresis_ms(uint64_t ms) { hysteresis_ms_ = ms; }
    [[nodiscard]] uint64_t debounce_ms() const { return debounce_ms_; }
    [[nodiscard]] uint64_t hysteresis_ms() const { return hysteresis_ms_; }

private:
    std::vector<std::string> patterns_;
    IProcFs* fs_;
    uint64_t debounce_ms_{100};
    uint64_t hysteresis_ms_{500};

    std::unordered_map<int, DetectedGame> active_games_;
    std::unordered_map<int, std::chrono::steady_clock::time_point> pending_exits_;

    [[nodiscard]] bool matches_custom_pattern(std::string_view name, std::string_view cmdline) const;
    [[nodiscard]] bool is_known_emulator(std::string_view comm, std::string_view cmdline) const;
    [[nodiscard]] bool is_wine_or_proton_game(int pid, std::string_view comm, std::string_view cmdline) const;
};

} // namespace zenith
