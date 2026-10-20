// SPDX-License-Identifier: MIT
#pragma once

#include "zenith/result.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <algorithm>
#include <sys/types.h>

namespace zenith {

class IProcessSource {
public:
    virtual ~IProcessSource() = default;

    [[nodiscard]] virtual Result<std::vector<pid_t>> get_running_pids() const = 0;
    [[nodiscard]] virtual Result<std::string> get_process_comm(pid_t pid) const = 0;
    [[nodiscard]] virtual Result<std::string> get_process_cmdline(pid_t pid) const = 0;
    [[nodiscard]] virtual Result<std::string> get_process_environ(pid_t pid) const = 0;
    [[nodiscard]] virtual Result<std::vector<pid_t>> get_process_tasks(pid_t pid) const = 0;
};

class RealProcessSource : public IProcessSource {
public:
    Result<std::vector<pid_t>> get_running_pids() const override {
        std::vector<pid_t> pids;
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator("/proc", ec)) {
            if (!entry.is_directory()) continue;
            std::string name = entry.path().filename().string();
            if (!name.empty() && std::all_of(name.begin(), name.end(), ::isdigit)) {
                try {
                    pids.push_back(static_cast<pid_t>(std::stoi(name)));
                } catch (...) {}
            }
        }
        return Ok(std::move(pids));
    }

    Result<std::string> get_process_comm(pid_t pid) const override {
        std::string path = "/proc/" + std::to_string(pid) + "/comm";
        std::ifstream file(path);
        if (!file.is_open()) {
            return Result<std::string>(Error(ErrorCode::NotFound, "Process comm not found for PID " + std::to_string(pid)));
        }
        std::string comm;
        std::getline(file, comm);
        return Ok(std::move(comm));
    }

    Result<std::string> get_process_cmdline(pid_t pid) const override {
        std::string path = "/proc/" + std::to_string(pid) + "/cmdline";
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return Result<std::string>(Error(ErrorCode::NotFound, "Process cmdline not found for PID " + std::to_string(pid)));
        }
        std::string cmdline((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
        return Ok(std::move(cmdline));
    }

    Result<std::string> get_process_environ(pid_t pid) const override {
        std::string path = "/proc/" + std::to_string(pid) + "/environ";
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return Result<std::string>(Error(ErrorCode::NotFound, "Process environ not found for PID " + std::to_string(pid)));
        }
        std::string env((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
        return Ok(std::move(env));
    }

    Result<std::vector<pid_t>> get_process_tasks(pid_t pid) const override {
        std::string task_dir = "/proc/" + std::to_string(pid) + "/task";
        std::vector<pid_t> tids;
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(task_dir, ec)) {
            if (!entry.is_directory()) continue;
            std::string name = entry.path().filename().string();
            if (!name.empty() && std::all_of(name.begin(), name.end(), ::isdigit)) {
                try {
                    tids.push_back(static_cast<pid_t>(std::stoi(name)));
                } catch (...) {}
            }
        }
        return Ok(std::move(tids));
    }
};

class MockProcessSource : public IProcessSource {
public:
    struct MockProcessInfo {
        std::string comm;
        std::string cmdline;
        std::string environ;
        std::vector<pid_t> tasks;
    };

    void add_process(pid_t pid, MockProcessInfo info) {
        std::lock_guard<std::mutex> lock(mutex_);
        processes_[pid] = std::move(info);
    }

    void remove_process(pid_t pid) {
        std::lock_guard<std::mutex> lock(mutex_);
        processes_.erase(pid);
    }

    Result<std::vector<pid_t>> get_running_pids() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<pid_t> pids;
        pids.reserve(processes_.size());
        for (const auto& [pid, _] : processes_) {
            pids.push_back(pid);
        }
        return Ok(std::move(pids));
    }

    Result<std::string> get_process_comm(pid_t pid) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = processes_.find(pid);
        if (it == processes_.end()) {
            return Result<std::string>(Error(ErrorCode::NotFound, "MockProcess not found: " + std::to_string(pid)));
        }
        return Ok(it->second.comm);
    }

    Result<std::string> get_process_cmdline(pid_t pid) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = processes_.find(pid);
        if (it == processes_.end()) {
            return Result<std::string>(Error(ErrorCode::NotFound, "MockProcess not found: " + std::to_string(pid)));
        }
        return Ok(it->second.cmdline);
    }

    Result<std::string> get_process_environ(pid_t pid) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = processes_.find(pid);
        if (it == processes_.end()) {
            return Result<std::string>(Error(ErrorCode::NotFound, "MockProcess not found: " + std::to_string(pid)));
        }
        return Ok(it->second.environ);
    }

    Result<std::vector<pid_t>> get_process_tasks(pid_t pid) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = processes_.find(pid);
        if (it == processes_.end()) {
            return Result<std::vector<pid_t>>(Error(ErrorCode::NotFound, "MockProcess not found: " + std::to_string(pid)));
        }
        return Ok(it->second.tasks);
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<pid_t, MockProcessInfo> processes_;
};

} // namespace zenith
