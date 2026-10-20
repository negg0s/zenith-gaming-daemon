#include "zenith/game_detector.hpp"

namespace zenith {

GameDetector::GameDetector(std::vector<std::string> patterns)
    : patterns_(std::move(patterns)) {}

bool GameDetector::matches_game(std::string_view name, std::string_view cmdline) const {
    std::string lower_name(name);
    std::string lower_cmd(cmdline);
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
    std::transform(lower_cmd.begin(), lower_cmd.end(), lower_cmd.begin(), ::tolower);

    // Ignore known system daemons / steamwebhelper
    if (lower_name.find("steamwebhelper") != std::string::npos ||
        lower_name.find("zenith") != std::string::npos ||
        lower_name.find("systemd") != std::string::npos) {
        return false;
    }

    for (const auto& pattern : patterns_) {
        std::string lower_pat(pattern);
        std::transform(lower_pat.begin(), lower_pat.end(), lower_pat.begin(), ::tolower);
        if (lower_name.find(lower_pat) != std::string::npos ||
            lower_cmd.find(lower_pat) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::optional<DetectedGame> GameDetector::scan_for_game() const {
    fs::path proc_dir = "/proc";
    if (!fs::exists(proc_dir)) return std::nullopt;

    for (const auto& entry : fs::directory_iterator(proc_dir)) {
        if (!entry.is_directory()) continue;
        std::string dirname = entry.path().filename().string();
        if (dirname.empty() || !std::isdigit(dirname[0])) continue;

        int pid = 0;
        try {
            pid = std::stoi(dirname);
        } catch (...) {
            continue;
        }

        std::string comm = get_process_name(pid);
        std::string cmd = get_process_cmdline(pid);

        if (matches_game(comm, cmd)) {
            DetectedGame game;
            game.pid = pid;
            game.name = comm;
            game.cmdline = cmd;
            game.is_wine_or_proton = (cmd.find("wine") != std::string::npos ||
                                      cmd.find("proton") != std::string::npos ||
                                      comm.find("wine") != std::string::npos);
            return game;
        }
    }

    return std::nullopt;
}

bool GameDetector::is_pid_alive(int pid) {
    if (pid <= 0) return false;
    return fs::exists("/proc/" + std::to_string(pid));
}

std::string GameDetector::get_process_name(int pid) {
    auto comm = read_file_string("/proc/" + std::to_string(pid) + "/comm");
    return comm.value_or("");
}

std::string GameDetector::get_process_cmdline(int pid) {
    auto cmd = read_file_string("/proc/" + std::to_string(pid) + "/cmdline");
    if (!cmd) return "";
    std::string res;
    for (char c : *cmd) {
        res += (c == '\0') ? ' ' : c;
    }
    return trim(res);
}

std::vector<int> GameDetector::find_pids_by_name(std::string_view name) {
    std::vector<int> pids;
    fs::path proc_dir = "/proc";
    if (!fs::exists(proc_dir)) return pids;

    std::string target(name);
    std::transform(target.begin(), target.end(), target.begin(), ::tolower);

    for (const auto& entry : fs::directory_iterator(proc_dir)) {
        if (!entry.is_directory()) continue;
        std::string dirname = entry.path().filename().string();
        if (dirname.empty() || !std::isdigit(dirname[0])) continue;

        int pid = 0;
        try { pid = std::stoi(dirname); } catch (...) { continue; }

        std::string comm = get_process_name(pid);
        std::string lower_comm(comm);
        std::transform(lower_comm.begin(), lower_comm.end(), lower_comm.begin(), ::tolower);

        if (lower_comm.find(target) != std::string::npos) {
            pids.push_back(pid);
        }
    }
    return pids;
}

} // namespace zenith
