#pragma once

#include "zenith/common.hpp"

namespace zenith {

struct DetectedGame {
    int pid = 0;
    std::string name;
    std::string exe_path;
    std::string cmdline;
    bool is_wine_or_proton = false;
};

class GameDetector {
public:
    explicit GameDetector(std::vector<std::string> patterns);

    std::optional<DetectedGame> scan_for_game() const;
    static bool is_pid_alive(int pid);
    static std::string get_process_name(int pid);
    static std::string get_process_cmdline(int pid);
    static std::vector<int> find_pids_by_name(std::string_view name);

private:
    std::vector<std::string> patterns_;
    bool matches_game(std::string_view name, std::string_view cmdline) const;
};

} // namespace zenith
