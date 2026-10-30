// SPDX-License-Identifier: MIT
#include "zenith/sched_ext_tuner.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <thread>

#ifdef __linux__
#include <csignal>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace zenith {
namespace {

// Scheduler binaries live in the system directories. The name is already
// restricted by the config parser; this is the second line of defence so a
// hand-written config cannot point the tuner at an arbitrary path.
bool is_safe_scheduler_path(const std::string& path) {
    for (const char* root : {"/usr/bin/", "/usr/local/bin/"}) {
        const std::string prefix(root);
        if (path.rfind(prefix, 0) != 0) continue;
        const std::string name = path.substr(prefix.size());
        if (name.empty() || name.find('/') != std::string::npos) continue;
        for (char c : name) {
            const unsigned char uc = static_cast<unsigned char>(c);
            if (!(std::isalnum(uc) || c == '_' || c == '-')) return false;
        }
        return true;
    }
    return false;
}

// Compares the kernel release against major.minor without fighting with the
// suffix. Kernel "6.3" is older than "6.12", which a plain stod() gets wrong.
bool kernel_at_least(const std::string& release, int req_major, int req_minor) {
    auto read_number = [&release](size_t& pos) -> int {
        while (pos < release.size() && !std::isdigit(static_cast<unsigned char>(release[pos]))) {
            if (release[pos] == '.') break;
            ++pos;
        }
        int value = 0;
        bool any = false;
        while (pos < release.size() && std::isdigit(static_cast<unsigned char>(release[pos]))) {
            value = value * 10 + (release[pos] - '0');
            any = true;
            ++pos;
        }
        return any ? value : -1;
    };

    size_t pos = 0;
    const int major = read_number(pos);
    if (major < 0) return false;
    if (pos < release.size() && release[pos] == '.') ++pos;
    const int minor = read_number(pos);
    if (minor < 0) return major > req_major;
    return major > req_major || (major == req_major && minor >= req_minor);
}

#ifdef __linux__
std::pair<bool, std::string> exec_and_wait(const std::string& path,
                                           const std::vector<std::string>& args,
                                           int timeout_ms) {
    const pid_t pid = ::fork();
    if (pid < 0) {
        return {false, "fork failed"};
    }

    if (pid == 0) {
        const int devnull = ::open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            ::dup2(devnull, STDIN_FILENO);
            ::dup2(devnull, STDOUT_FILENO);
            ::dup2(devnull, STDERR_FILENO);
            if (devnull > STDERR_FILENO) ::close(devnull);
        }

        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(path.c_str()));
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        char* const envp[] = {
            const_cast<char*>("PATH=/usr/bin:/bin"),
            const_cast<char*>("LC_ALL=C"),
            nullptr
        };

        ::execve(path.c_str(), argv.data(), envp);
        ::_exit(127);
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    int status = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        const pid_t res = ::waitpid(pid, &status, WNOHANG);
        if (res == pid) {
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return {true, {}};
            if (WIFEXITED(status)) return {false, "exit code " + std::to_string(WEXITSTATUS(status))};
            if (WIFSIGNALED(status)) return {false, "killed by signal " + std::to_string(WTERMSIG(status))};
            return {false, "unknown exit status"};
        }
        if (res < 0) {
            return {false, "waitpid failed"};
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    ::kill(pid, SIGKILL);
    ::waitpid(pid, &status, 0);
    return {false, "timed out after " + std::to_string(timeout_ms) + " ms"};
}

// Sends a signal to every process whose /proc/<pid>/comm matches the name.
// Returns the number of processes signalled, or -1 when /proc is unreadable.
int signal_processes_by_name(const std::string& name, int sig) {
    DIR* proc = ::opendir("/proc");
    if (!proc) return -1;

    int signalled = 0;
    while (dirent* entry = ::readdir(proc)) {
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9') continue;

        std::ifstream comm(std::string("/proc/") + entry->d_name + "/comm");
        std::string line;
        if (!std::getline(comm, line)) continue;
        if (trim(line) != name) continue;

        const pid_t pid = static_cast<pid_t>(std::atoi(entry->d_name));
        if (pid > 1 && ::kill(pid, sig) == 0) {
            ++signalled;
        }
    }

    ::closedir(proc);
    return signalled;
}
#endif

} // namespace

SchedExtTuner::SchedExtTuner(ISchedExtFs* fs)
    : fs_(fs ? fs : &RealSchedExtFs::instance()) {}

SchedExtTuner::~SchedExtTuner() {
    if (auto_stop_) {
        (void)stop_scheduler();
    }
}

std::pair<bool, std::string> SchedExtTuner::run_cmd(const std::string& bin,
                                                    const std::vector<std::string>& args) {
    if (cmd_executor_) {
        return cmd_executor_(bin, args);
    }

    if (!is_safe_scheduler_path(bin)) {
        return {false, "refusing to execute a binary outside /usr/bin or /usr/local/bin: " + bin};
    }

#ifdef __linux__
    return exec_and_wait(bin, args, 5000);
#else
    (void)args;
    return {false, "sched_ext is only available on Linux"};
#endif
}

bool SchedExtTuner::is_supported() const {
    if (fs_->exists("/sys/kernel/sched_ext/state")) {
        return true;
    }
    // sched_ext is part of the mainline kernel since 6.12.
    if (auto rel = fs_->read_file("/proc/sys/kernel/osrelease")) {
        return kernel_at_least(*rel, 6, 12);
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
        const std::string path = std::string(dir) + "/" + scheduler_name;
        if (fs_->exists(path)) {
            return path;
        }
    }
    return {};
}

std::vector<std::string> SchedExtTuner::get_available_schedulers() const {
    std::vector<std::string> schedulers;

    for (const char* dir : {"/usr/bin", "/usr/local/bin"}) {
        for (const auto& name : fs_->list_dir(dir)) {
            if (name.rfind("scx_", 0) != 0) continue;
            if (name.find('.') != std::string::npos) continue;
            schedulers.push_back(name);
        }
    }

    if (schedulers.empty()) {
        // Filesystems that cannot list directories, or an unusual install
        // prefix: fall back to the schedulers we know by name.
        for (const char* candidate : {"scx_lavd", "scx_rusty", "scx_bpfland", "scx_flash", "scx_userland"}) {
            if (!find_scheduler_binary(candidate).empty()) {
                schedulers.push_back(candidate);
            }
        }
    }

    std::sort(schedulers.begin(), schedulers.end());
    schedulers.erase(std::unique(schedulers.begin(), schedulers.end()), schedulers.end());
    return schedulers;
}

bool SchedExtTuner::start_scheduler(const std::string& scheduler_name) {
    if (running_) return true;

    if (scheduler_name.empty() || scheduler_name.find('/') != std::string::npos) {
        ZENITH_LOG_WARN("Invalid sched_ext scheduler name: '" + scheduler_name + "'");
        return false;
    }

    if (!is_supported()) {
        ZENITH_LOG_WARN("sched_ext is not supported by the current kernel (requires Linux 6.12+).");
        return false;
    }

    if (is_active()) {
        ZENITH_LOG_INFO("sched_ext is already active (" + get_active_scheduler() +
                        "); leaving the system scheduler untouched.");
        return false;
    }

    const std::string bin = find_scheduler_binary(scheduler_name);
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

bool SchedExtTuner::stop_scheduler(const std::string& scheduler_name) {
    const std::string target = scheduler_name.empty() ? current_scheduler_ : scheduler_name;

    // Nothing to do when this instance never started a scheduler and no
    // explicit name was given. This keeps the daemon from killing a scheduler
    // managed by scx_loader.
    if (target.empty()) return true;
    if (!running_ && scheduler_name.empty()) return true;

    bool stopped = false;

    if (cmd_executor_) {
        // Test hook: the real path scans /proc below.
        auto [ok, out] = run_cmd("/usr/bin/pkill", {"-INT", "-x", target});
        (void)out;
        stopped = ok;
    } else {
#ifdef __linux__
        const int signalled = signal_processes_by_name(target, SIGINT);
        if (signalled > 0) {
            // Give the scheduler a moment to detach before the caller restores
            // other kernel state.
            for (int i = 0; i < 25; ++i) {
                if (signal_processes_by_name(target, 0) == 0) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(80));
            }
            stopped = true;
        } else if (signalled == 0) {
            // Process already gone; treat as success.
            stopped = true;
        }
#endif
    }

    if (stopped) {
        ZENITH_LOG_INFO("Stopped sched_ext BPF scheduler: " + target);
        if (target == current_scheduler_) {
            current_scheduler_.clear();
            running_ = false;
        }
        return true;
    }

    ZENITH_LOG_WARN("Could not stop sched_ext scheduler " + target +
                    "; it may still be running. Check with: zenithctl scx status");
    return false;
}

} // namespace zenith
