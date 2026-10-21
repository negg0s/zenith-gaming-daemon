// SPDX-License-Identifier: MIT
#include "zenith/pipewire_tuner.hpp"
#include "zenith/secure_process.hpp"
#include <cstdlib>

namespace zenith {

PipewireTuner::PipewireTuner(IPipewireFs* fs)
    : fs_(fs ? fs : &RealPipewireFs::instance()) {}

PipewireTuner::~PipewireTuner() {
    revert();
}

std::pair<bool, std::string> PipewireTuner::run_cmd(const std::string& bin, const std::vector<std::string>& args) {
    if (cmd_executor_) {
        return cmd_executor_(bin, args);
    }
    auto res = SecureProcess::execute(bin, args);
    return {res.executed && res.exit_code == 0, res.stdout_output};
}

bool PipewireTuner::probe() {
    return fs::exists("/usr/bin/pw-metadata") || fs::exists("/run/pipewire");
}

void PipewireTuner::write_dropin_config(int quantum) {
    std::string config_content =
        "# Generated automatically by Zenith Gaming Daemon\n"
        "context.properties = {\n"
        "    default.clock.quantum = " + std::to_string(quantum) + "\n"
        "    default.clock.min-quantum = " + std::to_string(quantum) + "\n"
        "}\n";

    fs::path dropin_dir = fs::path(DROPIN_PATH).parent_path();
    std::error_code ec;
    fs::create_directories(dropin_dir, ec);

    fs_->write_file(DROPIN_PATH, config_content);
}

bool PipewireTuner::apply(int quantum, int sample_rate) {
    if (applied_) return true;

    current_quantum_ = quantum;
    sample_rate_ = sample_rate;

    // 1. Write drop-in config in ephemeral /run/pipewire/pipewire.conf.d
    write_dropin_config(quantum);

    // 2. Query/set live metadata via pw-metadata
    (void)run_cmd("/usr/bin/pw-metadata", {"-n", "settings", "0", "clock.force-quantum", std::to_string(quantum)});

    // 3. Export process environment variable hint
    std::string env_val = std::to_string(quantum) + "/" + std::to_string(sample_rate);
#ifdef __linux__
    setenv("PIPEWIRE_LATENCY", env_val.c_str(), 1);
#endif

    ZENITH_LOG_INFO("Configured PipeWire real-time audio quantum: " + env_val);
    applied_ = true;
    return true;
}

bool PipewireTuner::handle_xrun_detected(uint64_t new_xrun_count) {
    if (new_xrun_count <= xrun_count_) {
        return false;
    }

    xrun_count_ = new_xrun_count;
    quantum_elevations_count_++;

    int elevated_quantum = current_quantum_ * 2;
    if (elevated_quantum > 1024) {
        elevated_quantum = 1024;
    }

    ZENITH_LOG_WARN("PipeWire xrun audio dropouts detected (total: " + std::to_string(new_xrun_count) +
                    "). Automatically elevating quantum from " + std::to_string(current_quantum_) +
                    " to " + std::to_string(elevated_quantum) + " to eliminate audio stutter.");

    current_quantum_ = elevated_quantum;
    write_dropin_config(elevated_quantum);
    (void)run_cmd("/usr/bin/pw-metadata", {"-n", "settings", "0", "clock.force-quantum", std::to_string(elevated_quantum)});

    std::string env_val = std::to_string(elevated_quantum) + "/" + std::to_string(sample_rate_);
#ifdef __linux__
    setenv("PIPEWIRE_LATENCY", env_val.c_str(), 1);
#endif

    return true;
}

bool PipewireTuner::revert() {
    if (!applied_) return true;

    // Remove drop-in config
    fs_->remove_file(DROPIN_PATH);

    // Reset force-quantum to 0 (dynamic/unconstrained)
    (void)run_cmd("/usr/bin/pw-metadata", {"-n", "settings", "0", "clock.force-quantum", "0"});

#ifdef __linux__
    unsetenv("PIPEWIRE_LATENCY");
#endif

    applied_ = false;
    ZENITH_LOG_DEBUG("Reverted PipeWire quantum to default dynamic clock.");
    return true;
}

std::string PipewireTuner::describe() const {
    return "PipeWire Real-Time Quantum Tuner & Dynamic xrun Guardian";
}

} // namespace zenith
