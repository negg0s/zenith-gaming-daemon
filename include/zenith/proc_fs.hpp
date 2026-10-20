#pragma once

#include "zenith/common.hpp"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace zenith {

class IProcFs {
public:
    virtual ~IProcFs() = default;
    [[nodiscard]] virtual bool exists(const fs::path& path) const = 0;
    [[nodiscard]] virtual std::optional<std::string> read_file(const fs::path& path) const = 0;
    [[nodiscard]] virtual std::vector<int> list_pids() const = 0;
    [[nodiscard]] virtual std::vector<int> list_tasks(int pid) const = 0;
    [[nodiscard]] virtual std::vector<std::string> list_open_fds(int pid) const = 0;
};

class RealProcFs final : public IProcFs {
public:
    static RealProcFs& instance() {
        static RealProcFs inst;
        return inst;
    }

    [[nodiscard]] bool exists(const fs::path& path) const override {
        std::error_code ec;
        return fs::exists(path, ec);
    }

    [[nodiscard]] std::optional<std::string> read_file(const fs::path& path) const override {
        return read_file_string(path);
    }

    [[nodiscard]] std::vector<int> list_pids() const override {
        std::vector<int> pids;
        std::error_code ec;
        fs::path proc_dir = "/proc";
        if (!fs::exists(proc_dir, ec)) return pids;

        for (const auto& entry : fs::directory_iterator(proc_dir, ec)) {
            if (!entry.is_directory(ec)) continue;
            std::string dirname = entry.path().filename().string();
            if (dirname.empty() || !std::isdigit(static_cast<unsigned char>(dirname[0]))) continue;
            try {
                pids.push_back(std::stoi(dirname));
            } catch (...) {}
        }
        return pids;
    }

    [[nodiscard]] std::vector<int> list_tasks(int pid) const override {
        std::vector<int> tasks;
        std::error_code ec;
        fs::path task_dir = "/proc/" + std::to_string(pid) + "/task";
        if (!fs::exists(task_dir, ec)) return tasks;

        for (const auto& entry : fs::directory_iterator(task_dir, ec)) {
            try {
                tasks.push_back(std::stoi(entry.path().filename().string()));
            } catch (...) {}
        }
        return tasks;
    }

    [[nodiscard]] std::vector<std::string> list_open_fds(int pid) const override {
        std::vector<std::string> fds;
        std::error_code ec;
        fs::path fd_dir = "/proc/" + std::to_string(pid) + "/fd";
        if (!fs::exists(fd_dir, ec)) return fds;

        for (const auto& entry : fs::directory_iterator(fd_dir, ec)) {
            auto target = fs::read_symlink(entry.path(), ec);
            if (!ec) {
                fds.push_back(target.string());
            }
        }
        return fds;
    }
};

class MockProcFs final : public IProcFs {
public:
    struct MockProcess {
        int pid = 0;
        std::string comm;
        std::string cmdline;
        std::string environ;
        std::string maps;
        std::vector<int> tasks;
        std::unordered_map<int, std::string> task_comms;
        std::vector<std::string> open_fds;
    };

    void add_process(MockProcess proc) {
        if (proc.tasks.empty()) {
            proc.tasks.push_back(proc.pid);
            proc.task_comms[proc.pid] = proc.comm;
        }
        pids_.push_back(proc.pid);
        processes_[proc.pid] = std::move(proc);
    }

    void remove_process(int pid) {
        processes_.erase(pid);
        std::erase(pids_, pid);
    }

    void clear() {
        processes_.clear();
        pids_.clear();
    }

    [[nodiscard]] bool exists(const fs::path& path) const override {
        std::string p = path.generic_string();
        if (p == "/proc" || p == "/proc/") return true;

        for (const auto& [pid, proc] : processes_) {
            std::string prefix = "/proc/" + std::to_string(pid);
            if (p == prefix) return true;
            if (p == prefix + "/comm") return true;
            if (p == prefix + "/cmdline") return true;
            if (p == prefix + "/environ") return true;
            if (p == prefix + "/maps") return true;
            if (p == prefix + "/task") return true;
            if (p == prefix + "/fd") return true;

            for (int tid : proc.tasks) {
                if (p == prefix + "/task/" + std::to_string(tid)) return true;
                if (p == prefix + "/task/" + std::to_string(tid) + "/comm") return true;
            }
        }
        return false;
    }

    [[nodiscard]] std::optional<std::string> read_file(const fs::path& path) const override {
        std::string p = path.generic_string();

        for (const auto& [pid, proc] : processes_) {
            std::string prefix = "/proc/" + std::to_string(pid);
            if (p == prefix + "/comm") return proc.comm;
            if (p == prefix + "/cmdline") return proc.cmdline;
            if (p == prefix + "/environ") return proc.environ;
            if (p == prefix + "/maps") return proc.maps;

            for (const auto& [tid, comm] : proc.task_comms) {
                if (p == prefix + "/task/" + std::to_string(tid) + "/comm") {
                    return comm;
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::vector<int> list_pids() const override {
        return pids_;
    }

    [[nodiscard]] std::vector<int> list_tasks(int pid) const override {
        auto it = processes_.find(pid);
        if (it != processes_.end()) {
            return it->second.tasks;
        }
        return {};
    }

    [[nodiscard]] std::vector<std::string> list_open_fds(int pid) const override {
        auto it = processes_.find(pid);
        if (it != processes_.end()) {
            return it->second.open_fds;
        }
        return {};
    }

private:
    std::vector<int> pids_;
    std::unordered_map<int, MockProcess> processes_;
};

} // namespace zenith
