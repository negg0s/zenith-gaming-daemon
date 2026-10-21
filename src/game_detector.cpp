// SPDX-License-Identifier: MIT
#include "zenith/game_detector.hpp"
#include <algorithm>
#include <cctype>

namespace zenith {

namespace {

std::string to_lower_str(std::string_view sv) {
    std::string s(sv);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

} // namespace

GameDetector::GameDetector(std::vector<std::string> custom_patterns, IProcFs* fs)
    : patterns_(std::move(custom_patterns)),
      fs_(fs ? fs : &RealProcFs::instance()) {}

bool GameDetector::matches_custom_pattern(std::string_view name, std::string_view cmdline) const {
    std::string lower_name = to_lower_str(name);
    std::string lower_cmd = to_lower_str(cmdline);

    for (const auto& pattern : patterns_) {
        std::string lower_pat = to_lower_str(pattern);
        if (lower_name.find(lower_pat) != std::string::npos ||
            lower_cmd.find(lower_pat) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool GameDetector::is_known_emulator(std::string_view comm, std::string_view cmdline) const {
    static const std::vector<std::string> emulators = {
        "rpcs3", "yuzu", "ryujinx", "dolphin-emu", "pcsx2", "cemu",
        "duckstation", "ppsspp", "retroarch", "mame", "melonds", "citra"
    };
    std::string lower_comm = to_lower_str(comm);
    std::string lower_cmd = to_lower_str(cmdline);

    for (const auto& emu : emulators) {
        if (lower_comm.find(emu) != std::string::npos || lower_cmd.find(emu) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool GameDetector::is_non_game_binary(std::string_view comm, std::string_view cmdline) const {
    std::string lower_comm = to_lower_str(comm);
    std::string lower_cmd = to_lower_str(cmdline);

    // Filter daemon itself and system components
    if (lower_comm.find("zenith") != std::string::npos ||
        lower_comm.find("systemd") != std::string::npos ||
        lower_comm.find("dbus") != std::string::npos ||
        lower_comm.find("pipewire") != std::string::npos ||
        lower_comm.find("wireplumber") != std::string::npos ||
        lower_comm.find("polkit") != std::string::npos ||
        lower_comm.find("sshd") != std::string::npos ||
        lower_comm.find("kwin") != std::string::npos ||
        lower_comm.find("mutter") != std::string::npos ||
        lower_comm.find("sway") != std::string::npos ||
        lower_comm.find("hyprland") != std::string::npos ||
        lower_comm.find("xorg") != std::string::npos ||
        lower_comm.find("xwayland") != std::string::npos) {
        return true;
    }

    // Filter known steam auxiliary tools
    if (lower_comm.find("steamwebhelper") != std::string::npos ||
        lower_comm.find("steamerrorreporter") != std::string::npos ||
        (lower_comm == "steam" && lower_cmd.find("steamwebhelper") != std::string::npos)) {
        return true;
    }

    // Filter standalone launcher UI processes (when without game payload)
    if (lower_comm == "epicgameslauncher" ||
        lower_comm == "origin.exe" ||
        lower_comm == "eadesktop.exe" ||
        lower_comm == "ubisoftconnect.exe" ||
        lower_comm == "battle.net.exe" ||
        lower_comm == "riotclientservices.exe" ||
        lower_comm == "gog-galaxy.exe") {
        return true;
    }

    // Filter bare/idle wineserver (only considered game if active wine client exists)
    if (lower_comm == "wineserver") {
        return true;
    }

    return false;
}

std::string GameDetector::extract_steam_appid(int pid) const {
    // Check environ
    auto env = fs_->read_file("/proc/" + std::to_string(pid) + "/environ");
    if (env) {
        // Environ is null-separated or newline-separated
        std::string_view env_sv(*env);
        size_t pos = env_sv.find("SteamAppId=");
        if (pos != std::string_view::npos) {
            size_t val_start = pos + 11;
            size_t val_end = val_start;
            while (val_end < env_sv.size() && env_sv[val_end] != '\0' &&
                   env_sv[val_end] != '\n' && env_sv[val_end] != '\r' && env_sv[val_end] != ';') {
                val_end++;
            }
            std::string appid(env_sv.substr(val_start, val_end - val_start));
            if (!appid.empty() && std::all_of(appid.begin(), appid.end(), ::isdigit) && appid != "0") {
                return appid;
            }
        }
    }

    // Check cmdline
    auto cmd = fs_->read_file("/proc/" + std::to_string(pid) + "/cmdline");
    if (cmd) {
        std::string_view cmd_sv(*cmd);
        size_t pos = cmd_sv.find("AppId=");
        if (pos != std::string_view::npos) {
            size_t val_start = pos + 6;
            size_t val_end = val_start;
            while (val_end < cmd_sv.size() && cmd_sv[val_end] != ' ' && cmd_sv[val_end] != '\0' &&
                   cmd_sv[val_end] != '\n' && cmd_sv[val_end] != '\r') {
                val_end++;
            }
            std::string appid(cmd_sv.substr(val_start, val_end - val_start));
            if (!appid.empty() && std::all_of(appid.begin(), appid.end(), ::isdigit) && appid != "0") {
                return appid;
            }
        }
    }

    return "";
}

bool GameDetector::has_gpu_context(int pid) const {
    auto fds = fs_->list_open_fds(pid);
    for (const auto& fd : fds) {
        if (fd.find("/dev/dri/renderD") != std::string::npos ||
            fd.find("/dev/nvidia") != std::string::npos) {
            return true;
        }
    }

    // Check memory maps
    auto maps = fs_->read_file("/proc/" + std::to_string(pid) + "/maps");
    if (maps) {
        if (maps->find("libvulkan.so") != std::string::npos ||
            maps->find("libGL.so") != std::string::npos ||
            maps->find("dxvk") != std::string::npos ||
            maps->find("vkd3d") != std::string::npos) {
            return true;
        }
    }

    return false;
}

bool GameDetector::is_wine_or_proton_game(int pid, std::string_view comm, std::string_view cmdline) const {
    std::string lower_comm = to_lower_str(comm);
    std::string lower_cmd = to_lower_str(cmdline);

    if (lower_comm.find("wine64-preloader") != std::string::npos ||
        lower_comm.find("wine-preloader") != std::string::npos ||
        lower_comm.find("proton") != std::string::npos) {
        return true;
    }

    if (lower_cmd.find("wine") != std::string::npos || lower_cmd.find("proton") != std::string::npos) {
        if (lower_cmd.find(".exe") != std::string::npos) {
            return true;
        }
    }

    // Windows executable executing on Linux
    if (lower_cmd.find(".exe") != std::string::npos || lower_comm.find(".exe") != std::string::npos) {
        return true;
    }

    // Check environ for Proton / Wine environment variables
    auto env = fs_->read_file("/proc/" + std::to_string(pid) + "/environ");
    if (env) {
        if (env->find("STEAM_COMPAT_DATA_PATH") != std::string::npos ||
            env->find("WINEPREFIX") != std::string::npos ||
            env->find("PROTON_") != std::string::npos) {
            return true;
        }
    }

    return false;
}

std::optional<DetectedGame> GameDetector::inspect_process(int pid) const {
    if (pid <= 1) return std::nullopt;

    auto comm = fs_->read_file("/proc/" + std::to_string(pid) + "/comm");
    if (!comm || comm->empty()) return std::nullopt;

    std::string clean_comm = trim(*comm);
    auto raw_cmd = fs_->read_file("/proc/" + std::to_string(pid) + "/cmdline");
    std::string clean_cmd;
    if (raw_cmd) {
        for (char c : *raw_cmd) {
            clean_cmd += (c == '\0') ? ' ' : c;
        }
        clean_cmd = trim(clean_cmd);
    }

    if (is_non_game_binary(clean_comm, clean_cmd)) {
        return std::nullopt;
    }

    DetectedGame game;
    game.pid = pid;
    game.name = clean_comm;
    game.cmdline = clean_cmd;
    game.detected_at = std::chrono::steady_clock::now();

    // 1. Steam detection
    std::string appid = extract_steam_appid(pid);
    if (!appid.empty()) {
        game.steam_appid = appid;
        game.runner_type = "steam";
        game.is_wine_or_proton = is_wine_or_proton_game(pid, clean_comm, clean_cmd);
        if (game.is_wine_or_proton) {
            game.runner_type = "proton";
        }
        game.has_gpu_render_node = has_gpu_context(pid);
        return game;
    }

    // 2. Wine / Proton standalone or runner
    if (is_wine_or_proton_game(pid, clean_comm, clean_cmd)) {
        game.is_wine_or_proton = true;
        game.runner_type = "proton";
        game.has_gpu_render_node = has_gpu_context(pid);
        return game;
    }

    // 3. Known Emulators
    if (is_known_emulator(clean_comm, clean_cmd)) {
        game.is_emulator = true;
        game.runner_type = "emulator";
        game.has_gpu_render_node = has_gpu_context(pid);
        return game;
    }

    // 4. Gamescope session
    std::string lower_cmd = to_lower_str(clean_cmd);
    if (lower_cmd.find("gamescope") != std::string::npos && clean_comm != "gamescope") {
        game.runner_type = "gamescope";
        game.has_gpu_render_node = has_gpu_context(pid);
        return game;
    }

    // 5. Lutris / Heroic / Bottles wrappers
    if (lower_cmd.find("lutris-wrapper") != std::string::npos ||
        lower_cmd.find("heroic") != std::string::npos ||
        lower_cmd.find("bottles") != std::string::npos) {
        game.runner_type = "runner";
        game.has_gpu_render_node = has_gpu_context(pid);
        return game;
    }

    // 6. Custom patterns or steamapps/common path
    if (lower_cmd.find("steamapps/common") != std::string::npos || matches_custom_pattern(clean_comm, clean_cmd)) {
        game.runner_type = "native";
        game.has_gpu_render_node = has_gpu_context(pid);
        return game;
    }

    // 7. GPU render context heuristic for unknown heavy binaries
    if (has_gpu_context(pid)) {
        // If it's holding a DRM render node and has graphic maps, classify as native game
        game.runner_type = "native";
        game.has_gpu_render_node = true;
        return game;
    }

    return std::nullopt;
}

bool GameDetector::is_game(int pid) const {
    return inspect_process(pid).has_value();
}

std::vector<DetectedGame> GameDetector::scan_all_games() const {
    std::vector<DetectedGame> games;
    auto pids = fs_->list_pids();

    for (int pid : pids) {
        auto maybe_game = inspect_process(pid);
        if (maybe_game) {
            games.push_back(*maybe_game);
        }
    }
    return games;
}

std::optional<DetectedGame> GameDetector::scan_for_game() const {
    if (!active_games_.empty()) {
        return active_games_.begin()->second;
    }
    auto games = scan_all_games();
    if (!games.empty()) {
        return games.front();
    }
    return std::nullopt;
}

bool GameDetector::handle_process_exec(int pid) {
    auto maybe_game = inspect_process(pid);
    if (maybe_game) {
        active_games_[pid] = *maybe_game;
        pending_exits_.erase(pid);
        return true;
    }
    return false;
}

bool GameDetector::handle_process_exit(int pid) {
    auto it = active_games_.find(pid);
    if (it != active_games_.end()) {
        active_games_.erase(it);
        pending_exits_[pid] = std::chrono::steady_clock::now();
        return true;
    }
    return false;
}

bool GameDetector::has_active_game() const {
    return !active_games_.empty();
}

bool GameDetector::is_pid_alive(int pid, IProcFs* fs) {
    if (pid <= 0) return false;
    IProcFs* effective_fs = fs ? fs : &RealProcFs::instance();
    return effective_fs->exists("/proc/" + std::to_string(pid));
}

std::string GameDetector::get_process_name(int pid, IProcFs* fs) {
    IProcFs* effective_fs = fs ? fs : &RealProcFs::instance();
    auto comm = effective_fs->read_file("/proc/" + std::to_string(pid) + "/comm");
    return comm ? trim(*comm) : "";
}

std::string GameDetector::get_process_cmdline(int pid, IProcFs* fs) {
    IProcFs* effective_fs = fs ? fs : &RealProcFs::instance();
    auto cmd = effective_fs->read_file("/proc/" + std::to_string(pid) + "/cmdline");
    if (!cmd) return "";
    std::string res;
    for (char c : *cmd) {
        res += (c == '\0') ? ' ' : c;
    }
    return trim(res);
}

std::vector<int> GameDetector::find_pids_by_name(std::string_view name, IProcFs* fs) {
    std::vector<int> result;
    IProcFs* effective_fs = fs ? fs : &RealProcFs::instance();
    std::string target = to_lower_str(name);

    auto pids = effective_fs->list_pids();
    for (int pid : pids) {
        auto comm = effective_fs->read_file("/proc/" + std::to_string(pid) + "/comm");
        if (comm && to_lower_str(trim(*comm)) == target) {
            result.push_back(pid);
        }
    }
    return result;
}

} // namespace zenith
