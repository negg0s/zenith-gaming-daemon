#pragma once

#include <string>
#include <vector>

namespace zenith {

struct TuiSnapshot {
    std::string daemon_version = "1.0.0";
    std::string profile = "Balanced";
    bool is_game_active = false;
    int game_pid = 0;
    std::string game_name = "None (Idle)";
    std::string game_type = "N/A";

    std::vector<int> gaming_cores;
    std::vector<int> housekeeping_cores;

    double cpu_pressure_avg10 = 0.0;
    double mem_pressure_avg10 = 0.0;
    double io_pressure_avg10 = 0.0;

    double cpu_temp_c = 0.0;
    double gpu_temp_c = 0.0;

    int active_tweaks_count = 0;
    bool wal_synced = true;
    std::vector<std::string> recent_events;
};

class TuiRenderer {
public:
    static std::string render_dashboard(const TuiSnapshot& snapshot);
    static void run_watch_loop(int interval_ms = 1000, int max_iterations = 0);
};

} // namespace zenith
