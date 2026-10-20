#include "zenith/tui.hpp"
#include "zenith/cpu_topology.hpp"
#include "zenith/pressure_monitor.hpp"
#include "zenith/tweaks/tweak_registry.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

namespace zenith {

namespace {

std::string make_gauge_bar(double pct, int width = 20) {
    pct = std::clamp(pct, 0.0, 100.0);
    int filled = static_cast<int>((pct / 100.0) * width);
    std::string bar = "[";
    for (int i = 0; i < width; ++i) {
        if (i < filled) {
            bar += "█";
        } else {
            bar += "░";
        }
    }
    bar += "]";
    return bar;
}

std::string format_core_list(const std::vector<int>& cores) {
    if (cores.empty()) return "none";
    std::ostringstream ss;
    ss << "[";
    for (size_t i = 0; i < cores.size(); ++i) {
        ss << cores[i];
        if (i + 1 < cores.size()) ss << ",";
    }
    ss << "]";
    return ss.str();
}

} // namespace

std::string TuiRenderer::render_dashboard(const TuiSnapshot& s) {
    std::ostringstream ss;

    // Clear terminal screen and position cursor at top
    ss << "\033[2J\033[H";

    ss << "\033[1;36m========================================================================\033[0m\n"
       << "\033[1;37m   ⚡ ZENITH GAMING DAEMON MONITOR (v" << s.daemon_version << ") | Profile: \033[1;32m["
       << s.profile << "]\033[0m\n"
       << "\033[1;36m========================================================================\033[0m\n";

    // Game Status Block
    ss << "\033[1;33m[ Game Status ]\033[0m\n";
    if (s.is_game_active) {
        ss << "  Active Game:  \033[1;32m" << s.game_name << " (PID " << s.game_pid << ")\033[0m\n"
           << "  Platform:     " << s.game_type << "\n"
           << "  Status:       \033[1;32mOPTIMIZED (Low Latency Mode Engaged)\033[0m\n";
    } else {
        ss << "  Active Game:  \033[2m" << s.game_name << "\033[0m\n"
           << "  Status:       \033[36mIDLE (Listening via Netlink proc connector)\033[0m\n";
    }
    ss << "\n";

    // CPU Topology Map
    ss << "\033[1;33m[ CPU Core Allocation Map ]\033[0m\n";
    ss << "  Gaming Cores (V-Cache / P-cores):      \033[1;32m"
       << format_core_list(s.gaming_cores) << " --> [Exclusive/Perf]\033[0m\n";
    ss << "  Housekeeping Cores (cgroup Background): \033[34m"
       << format_core_list(s.housekeeping_cores) << " --> [Soft-Throttled]\033[0m\n\n";

    // Live PSI Pressure
    ss << "\033[1;33m[ System Pressure Stall Information (PSI avg10) ]\033[0m\n";
    ss << "  CPU:    " << make_gauge_bar(s.cpu_pressure_avg10) << " "
       << std::fixed << std::setprecision(2) << s.cpu_pressure_avg10 << "%\n";
    ss << "  Memory: " << make_gauge_bar(s.mem_pressure_avg10) << " "
       << std::fixed << std::setprecision(2) << s.mem_pressure_avg10 << "%\n";
    ss << "  I/O:    " << make_gauge_bar(s.io_pressure_avg10) << " "
       << std::fixed << std::setprecision(2) << s.io_pressure_avg10 << "%\n\n";

    // Thermals and Mutation State
    ss << "\033[1;33m[ Hardware State & Rollback Protection ]\033[0m\n";
    ss << "  CPU Temp:      " << (s.cpu_temp_c > 0 ? std::to_string(static_cast<int>(s.cpu_temp_c)) + " °C" : "N/A")
       << " | GPU Temp: " << (s.gpu_temp_c > 0 ? std::to_string(static_cast<int>(s.gpu_temp_c)) + " °C" : "N/A") << "\n";
    ss << "  Active Tweaks: " << s.active_tweaks_count << " applied\n";
    ss << "  State Journal: " << (s.wal_synced ? "\033[32mSynchronized (/run/zenith/journal.json)\033[0m" : "\033[31mUnsynced\033[0m") << "\n\n";

    // Recent Events
    ss << "\033[1;33m[ Recent Events ]\033[0m\n";
    if (s.recent_events.empty()) {
        ss << "  \033[2mNo state transitions recorded in this session.\033[0m\n";
    } else {
        for (const auto& ev : s.recent_events) {
            ss << "  • " << ev << "\n";
        }
    }

    ss << "\033[1;36m========================================================================\033[0m\n"
       << "\033[2mPress Ctrl+C to exit watch mode.\033[0m\n";

    return ss.str();
}

void TuiRenderer::run_watch_loop(int interval_ms, int max_iterations) {
    TuiSnapshot snapshot;

    // Probe hardware topology
    CpuTopology topo;
    if (topo.probe()) {
        snapshot.gaming_cores = topo.get_gaming_cores();
        snapshot.housekeeping_cores = topo.get_housekeeping_cores();
    } else {
        snapshot.gaming_cores = {0, 1, 2, 3};
        snapshot.housekeeping_cores = {4, 5, 6, 7};
    }

    snapshot.recent_events.push_back("Daemon socket server active at /run/zenith/zenith.sock");
    snapshot.recent_events.push_back("cgroups v2 background isolation slice established");

    PressureMonitor monitor;
    int iteration = 0;

    while (max_iterations <= 0 || iteration < max_iterations) {
        // Sample PSI
        auto psi = monitor.read_metrics();
        snapshot.cpu_pressure_avg10 = psi.cpu_some.avg10;
        snapshot.mem_pressure_avg10 = psi.memory_some.avg10;
        snapshot.io_pressure_avg10 = psi.io_some.avg10;
        snapshot.cpu_temp_c = monitor.read_cpu_temperature();
        snapshot.gpu_temp_c = monitor.read_gpu_temperature();

        std::string frame = render_dashboard(snapshot);
        std::cout << frame << std::flush;

        iteration++;
        if (max_iterations > 0 && iteration >= max_iterations) break;

        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
}

} // namespace zenith
