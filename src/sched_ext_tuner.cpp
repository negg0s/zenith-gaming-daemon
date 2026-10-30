// SPDX-License-Identifier: MIT
#include "zenith/sched_ext_tuner.hpp"
#include "zenith/secure_process.hpp"

namespace zenith {

SchedExtTuner::SchedExtTuner(ISchedExtFs* fs)
    : fs_(fs ? fs : &RealSchedExtFs::instance()) {}

SchedExtTuner::~SchedExtTuner() {
    if (auto_stop_) {
        stop_scheduler();
    }
}

std::pair<bool, std::string> SchedExtTuner::run_cmd(const std::string& bin, const std::vector<std::string>& args) {
    if (cmd_executor_) {
        return cmd_executor_(bin, args);
    }
    auto res = SecureProcess::execute(bin, args);
    return {res.executed && res.exit_code == 0, res.stdout_output};
}

bool SchedExtTuner::is_supported() const {
    if (fs_->exists("/sys/kernel/sched_ext/state")) {
        return true;
    }
    // sched_ext is part of the mainline kernel since 6.12.
    if (auto rel = fs_->read_file("/proc/sys/kernel/osrelease")) {
        try {
            double ver = std::stod(*rel);
            if (ver >= 6.12) return true;
        } catch (...) {}
    }
    return false;
}

bool SchedExtTuner::is_active() const {
    if (auto state = fs_->read_file("/sys/kernel/sched_ext/state")) {
        return trim(*state) == "enabled";
    }
    return false;
}

std::string SchedExtTuner::get_active_scheduler() const {
    if (auto ops = fs_->read_file("/sys/kernel/sched_ext/root/ops")) {
        auto name = trim(*ops);
        if (!name.empty()) return name;
    }
    if (auto state = fs_->read_file("/sys/kernel/sched_ext/state")) {
        auto name = trim(*state);
        if (!name.empty()) return name;
    }
    return "disabled";
}

std::string SchedExtTuner::find_scheduler_binary(const std::string& scheduler_name) const {
    for (const char* dir : {"/usr/bin", "/usr/local/bin"}) {
        std::string path = std::string(dir) + "/" + scheduler_name;
        if (fs_->exists(path)) {
            return path;
        }
    }
    return {};
}

std::vector<std::string> SchedExtTuner::get_available_schedulers() const {
    std::vector<std::string> schedulers;
    static const std::vector<std::string> candidates = {
        "scx_lavd", "scx_rusty", "scx_bpfland", "scx_flash", "scx_userland"
    };

    for (const auto& scx : candidates) {
        if (!find_scheduler_binary(scx).empty()) {
            schedulers.push_back(scx);
        }
    }
    return schedulers;
}

bool SchedExtTuner::start_scheduler(const std::string& scheduler_name) {
    if (running_) return true;

    if (!is_supported()) {
        ZENITH_LOG_WARN("sched_ext is not supported by the current kernel (requires Linux 6.12+).");
        return false;
    }

    if (is_active()) {
        ZENITH_LOG_INFO("sched_ext is already active (" + get_active_scheduler() +
                        "); leaving the system scheduler untouched.");
        return false;
    }

    std::string bin = find_scheduler_binary(scheduler_name);
    if (bin.empty()) {
        ZENITH_LOG_WARN("sched_ext binary not found: " + scheduler_name +
                        " (looked in /usr/bin and /usr/local/bin)");
        return false;
    }

    auto [ok, out] = run_cmd(bin, {"--background"});
    if (ok) {
        running_ = true;
        current_scheduler_ = scheduler_name;
        ZENITH_LOG_INFO("Started sched_ext BPF scheduler: " + scheduler_name);
        return true;
    }

    ZENITH_LOG_WARN("Failed to start sched_ext scheduler " + scheduler_name +
                    (out.empty() ? std::string() : ": " + out));
    return false;
}

bool SchedExtTuner::stop_scheduler() {
    if (!running_) return true;

    auto [ok, out] = run_cmd("/usr/bin/killall", {"-SIGINT", current_scheduler_});
    if (!ok) {
        (void)run_cmd("/usr/bin/pkill", {"-INT", "-x", current_scheduler_});
    }
    running_ = false;
    ZENITH_LOG_INFO("Stopped sched_ext BPF scheduler: " + current_scheduler_);
    current_scheduler_.clear();
    return true;
}

} // namespace zenith
