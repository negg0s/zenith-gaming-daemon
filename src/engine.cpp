#include "zenith/engine.hpp"
#include "zenith/capability_guard.hpp"
#include <csignal>
#include <thread>
#include <iostream>

#ifdef __linux__
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace zenith {

ZenithEngine*& ZenithEngine::active_instance() {
    static ZenithEngine* instance = nullptr;
    return instance;
}

void ZenithEngine::signal_handler(int signum) {
    ZENITH_LOG_WARN("Caught signal " + std::to_string(signum) + ", cleanly shutting down...");
    if (auto* inst = active_instance()) {
        inst->deactivate();
    }
    std::exit(signum);
}

void ZenithEngine::setup_signals() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
#ifdef SIGHUP
    std::signal(SIGHUP, signal_handler);
#endif
}

ZenithEngine::ZenithEngine(Config config, fs::path run_dir)
    : run_dir_(std::move(run_dir)),
      config_(std::move(config)),
      journal_(run_dir_ / "journal.json"),
      lock_(run_dir_ / "zenith.lock"),
      safe_mode_(run_dir_ / "crash_history.json"),
      sysctl_(&journal_),
      detector_(config_.custom_game_patterns) {
    active_instance() = this;
    setup_signals();
}

ZenithEngine::~ZenithEngine() {
    deactivate();
    if (active_instance() == this) {
        active_instance() = nullptr;
    }
}

bool ZenithEngine::initialize() {
    ZENITH_LOG_INFO("Initializing Zenith Gaming Daemon...");

    // 1. Single-Instance Protection (flock)
    if (!lock_.acquire()) {
        ZENITH_LOG_ERROR("Another Zenith instance is already running (locked by PID " +
                         std::to_string(lock_.get_locked_pid()) + "). Exiting.");
        return false;
    }

    // 2. Privilege Sandboxing (drop unneeded capabilities)
    CapabilityGuard::drop_unneeded_capabilities();

    // 3. Safe Mode Evaluation (Crash Loop Protection)
    safe_mode_.evaluate_and_record_boot();
    if (safe_mode_.is_safe_mode_active()) {
        ZENITH_LOG_WARN("SAFE MODE ENGAGED: Aggressive mutations disabled due to recent crash loops.");
    }

    // 3. Crash-Only Recovery: Auto-heal any orphaned mutations from previous crash
    auto report = journal_.recover_all();
    if (report.total_entries > 0 && report.successfully_reverted > 0) {
        ZENITH_LOG_INFO("Crash recovery executed: Reverted " +
                        std::to_string(report.successfully_reverted) + " orphaned mutations.");
    }

    // 4. Discover CPU Topology and GPUs
    topology_ = CpuTopology::detect();

    std::string topo_info = "Topology: " + std::to_string(topology_.total_cores()) + " cores";
    if (topology_.has_vcache()) topo_info += " (AMD 3D V-Cache detected!)";
    if (topology_.has_hybrid()) topo_info += " (Intel Hybrid P/E cores detected!)";
    ZENITH_LOG_INFO(topo_info);

    ZENITH_LOG_INFO("Gaming Cores: " + topology_.to_cpuset_string(topology_.get_gaming_cores()));
    ZENITH_LOG_INFO("Housekeeping Cores: " + topology_.to_cpuset_string(topology_.get_background_cores()));

    gpu_.detect_gpus();
    initialized_ = true;
    return true;
}

bool ZenithEngine::activate(const DetectedGame& game) {
    if (safe_mode_.is_safe_mode_active()) {
        ZENITH_LOG_WARN("Cannot activate optimizations: Safe Mode is currently active.");
        return false;
    }

    if (active_game_.has_value()) {
        if (active_game_->pid == game.pid) return true;
        deactivate();
    }

    active_game_ = game;
    std::cout << "\n=======================================================\n";
    ZENITH_LOG_INFO(">>> ACTIVATING HARDCORE GAMING MODE FOR: " + game.name + " (PID: " + std::to_string(game.pid) + ")");
    std::cout << "=======================================================\n";

    // 1. Sysctl & Kernel latency
    if (config_.sysctl_tuning_enabled) {
        sysctl_.apply(config_.split_lock_mitigation_disabled,
                      config_.thp_always_enabled,
                      config_.swappiness,
                      config_.dirty_ratio,
                      config_.compaction_proactiveness);
    }

    // 2. cgroup isolation: isolate background apps to housekeeping cores
    if (config_.cgroup_isolation_enabled) {
        std::string bg_cpus = topology_.to_cpuset_string(topology_.get_background_cores());
        if (isolator_.setup_background_slice(bg_cpus)) {
            isolator_.isolate_background_by_name(config_.background_process_names);
        }
    }

    // 3. GPU Clocks Lock
    if (config_.gpu_clock_lock_enabled) {
        gpu_.lock_clocks();
    }

    // 4. Game Process & Thread Affinity Tuning
    ProcessTuner::tune_entire_process_tree(game.pid, topology_.get_gaming_cores(), config_.realtime_process_priority);

    // 5. Audio & Network low-latency
    if (config_.pipewire_quantum_tuning) {
        AudioNetworkTuner::apply_pipewire_quantum(config_.pipewire_latency_numerator, config_.pipewire_latency_denominator);
    }
    AudioNetworkTuner::optimize_network_qdisc();

    ZENITH_LOG_INFO("Zenith optimizations are ACTIVE. Low input-lag and stutter-free gaming engaged.");
    return true;
}

void ZenithEngine::deactivate() {
    if (!active_game_.has_value()) return;

    std::cout << "\n=======================================================\n";
    ZENITH_LOG_INFO("<<< Game exited (" + active_game_->name + "). Restoring original system state...");
    std::cout << "=======================================================\n";

    sysctl_.rollback();
    gpu_.rollback();
    isolator_.rollback();
    journal_.recover_all();
    active_game_ = std::nullopt;

    ZENITH_LOG_INFO("All system states cleanly restored.");
}

bool ZenithEngine::perform_emergency_rollback(const fs::path& run_dir) {
    ZENITH_LOG_INFO("Performing standalone emergency rollback from journal at " + run_dir.string());
    StateJournal journal(run_dir / "journal.json");
    auto report = journal.recover_all();
    ZENITH_LOG_INFO("Emergency rollback finished: " + std::to_string(report.successfully_reverted) +
                    " entries reverted, " + std::to_string(report.failed_reverts) + " failures.");
    return report.is_clean();
}

bool ZenithEngine::reset_safe_mode(const fs::path& run_dir) {
    SafeModeManager mgr(run_dir / "crash_history.json");
    return mgr.reset();
}

void ZenithEngine::run_daemon() {
    if (!initialize()) {
        return;
    }

    ZENITH_LOG_INFO("Daemon running. Polling for games every " + std::to_string(config_.poll_interval_ms) + "ms...");

    while (true) {
        if (active_game_.has_value()) {
            if (!GameDetector::is_pid_alive(active_game_->pid)) {
                deactivate();
            }
        } else {
            auto game = detector_.scan_for_game();
            if (game.has_value()) {
                activate(*game);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.poll_interval_ms));
    }
}

bool ZenithEngine::run_wrapped_command(const std::vector<std::string>& cmd_args) {
    if (cmd_args.empty()) return false;
    if (!initialize()) return false;

    AudioNetworkTuner::export_environment_hints(config_.pipewire_latency_numerator, config_.pipewire_latency_denominator);

#ifdef __linux__
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        std::vector<char*> argv;
        for (const auto& arg : cmd_args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        std::perror("execvp failed");
        std::exit(1);
    } else if (pid > 0) {
        DetectedGame game;
        game.pid = pid;
        game.name = cmd_args[0];
        activate(game);

        int status = 0;
        waitpid(pid, &status, 0);
        deactivate();
        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
#else
    ZENITH_LOG_WARN("Wrapper execution is native to Linux. On this platform, mock execution completed.");
    DetectedGame game;
    game.pid = 12345;
    game.name = cmd_args[0];
    activate(game);
    deactivate();
    return true;
#endif
    return false;
}

void ZenithEngine::print_status() const {
    std::cout << "\n================ [ Zenith System Status ] ================\n";
    std::cout << "Lock Status       : " << (lock_.is_acquired() ? "Acquired" : "Unheld") << "\n";
    std::cout << "Safe Mode Status  : " << (safe_mode_.is_safe_mode_active() ? "ACTIVE (Throttled)" : "Normal") << "\n";
    std::cout << "Recent Crashes    : " << safe_mode_.get_recent_crash_count() << "\n";
    std::cout << "Total Cores       : " << topology_.total_cores() << "\n";
    std::cout << "AMD 3D V-Cache    : " << (topology_.has_vcache() ? "YES (Optimized)" : "Not detected") << "\n";
    std::cout << "Intel Hybrid P/E  : " << (topology_.has_hybrid() ? "YES (P-cores pinned)" : "Uniform topology") << "\n";
    std::cout << "Gaming Cores      : " << topology_.to_cpuset_string(topology_.get_gaming_cores()) << "\n";
    std::cout << "Background Cores  : " << topology_.to_cpuset_string(topology_.get_background_cores()) << "\n";
    std::cout << "cgroups v2 isol   : " << (isolator_.is_supported() ? "Supported" : "Disabled/Unsupported") << "\n";
    std::cout << "Active Game       : " << (active_game_.has_value() ? active_game_->name + " (PID " + std::to_string(active_game_->pid) + ")" : "None (Idle)") << "\n";
    std::cout << "Journal Entries   : " << journal_.get_entries().size() << "\n";
    std::cout << "==========================================================\n\n";
}

} // namespace zenith
