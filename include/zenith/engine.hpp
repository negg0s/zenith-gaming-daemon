#pragma once

#include "zenith/config.hpp"
#include "zenith/cpu_topology.hpp"
#include "zenith/cgroup_isolator.hpp"
#include "zenith/sysctl_tuner.hpp"
#include "zenith/gpu_controller.hpp"
#include "zenith/irq_tuner.hpp"
#include "zenith/network_tuner.hpp"
#include "zenith/pipewire_tuner.hpp"
#include "zenith/process_tuner.hpp"
#include "zenith/pressure_monitor.hpp"
#include "zenith/adaptive_controller.hpp"
#include "zenith/local_learner.hpp"
#include "zenith/sched_ext_tuner.hpp"
#include "zenith/audio_network.hpp"
#include "zenith/game_detector.hpp"
#include "zenith/journal.hpp"
#include "zenith/lock_guard.hpp"
#include "zenith/safe_mode.hpp"
#include "zenith/socket_server.hpp"

namespace zenith {

class ZenithEngine {
public:
    explicit ZenithEngine(Config config,
                          fs::path run_dir = "/run/zenith");
    ~ZenithEngine();

    bool initialize();
    bool activate(const DetectedGame& game);
    void deactivate();

    void run_daemon();
    bool run_wrapped_command(const std::vector<std::string>& cmd_args);

    static bool perform_emergency_rollback(const fs::path& run_dir = "/run/zenith");
    static bool reset_safe_mode(const fs::path& run_dir = "/run/zenith");

    void print_status() const;

    bool is_game_active() const { return active_game_.has_value(); }
    bool is_safe_mode() const { return safe_mode_.is_safe_mode_active(); }

    AdaptiveController& adaptive() { return adaptive_controller_; }
    LocalLearner& learner() { return local_learner_; }
    SchedExtTuner& sched_ext() { return sched_ext_; }
    PressureMonitor& pressure() { return pressure_monitor_; }

    StateJournal& get_journal() { return journal_; }

private:
    fs::path run_dir_;
    Config config_;
    StateJournal journal_;
    DaemonLock lock_;
    SafeModeManager safe_mode_;

    CpuTopology topology_;
    CgroupIsolator isolator_;
    SysctlTuner sysctl_;
    GpuController gpu_;
    IrqTuner irq_;
    NetworkTuner network_;
    PipewireTuner pipewire_;
    PressureMonitor pressure_monitor_;
    AdaptiveController adaptive_controller_;
    LocalLearner local_learner_;
    SchedExtTuner sched_ext_;
    GameDetector detector_;
    SocketServer socket_server_;

    std::optional<DetectedGame> active_game_;
    bool initialized_ = false;

    std::string build_status_json() const;
    std::string handle_socket_command(const std::string& cmd, const ClientCredentials& creds);

    static void setup_signals();
    static ZenithEngine*& active_instance();
    static void signal_handler(int signum);
};

} // namespace zenith
