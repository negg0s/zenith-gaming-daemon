#pragma once

#include "zenith/config.hpp"
#include "zenith/cpu_topology.hpp"
#include "zenith/cgroup_isolator.hpp"
#include "zenith/sysctl_tuner.hpp"
#include "zenith/gpu_controller.hpp"
#include "zenith/process_tuner.hpp"
#include "zenith/audio_network.hpp"
#include "zenith/game_detector.hpp"
#include "zenith/journal.hpp"
#include "zenith/lock_guard.hpp"
#include "zenith/safe_mode.hpp"

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
    GameDetector detector_;

    std::optional<DetectedGame> active_game_;
    bool initialized_ = false;

    static void setup_signals();
    static ZenithEngine*& active_instance();
    static void signal_handler(int signum);
};

} // namespace zenith
