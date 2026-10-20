#pragma once

#include "zenith/config.hpp"
#include "zenith/cpu_topology.hpp"
#include "zenith/cgroup_isolator.hpp"
#include "zenith/sysctl_tuner.hpp"
#include "zenith/gpu_controller.hpp"
#include "zenith/process_tuner.hpp"
#include "zenith/audio_network.hpp"
#include "zenith/game_detector.hpp"

namespace zenith {

class ZenithEngine {
public:
    explicit ZenithEngine(Config config);
    ~ZenithEngine();

    bool initialize();
    bool activate(const DetectedGame& game);
    void deactivate();

    void run_daemon();
    bool run_wrapped_command(const std::vector<std::string>& cmd_args);

    void print_status() const;

    bool is_game_active() const { return active_game_.has_value(); }

private:
    Config config_;
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
