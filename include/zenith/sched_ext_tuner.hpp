// SPDX-License-Identifier: MIT
#pragma once

#include "zenith/common.hpp"
#include <string>
#include <vector>
#include <optional>
#include <functional>

namespace zenith {

class ISchedExtFs {
public:
    virtual ~ISchedExtFs() = default;
    [[nodiscard]] virtual bool exists(const fs::path& path) const = 0;
    [[nodiscard]] virtual std::optional<std::string> read_file(const fs::path& path) const = 0;
};

class RealSchedExtFs final : public ISchedExtFs {
public:
    static RealSchedExtFs& instance() {
        static RealSchedExtFs inst;
        return inst;
    }

    [[nodiscard]] bool exists(const fs::path& path) const override {
        std::error_code ec;
        return fs::exists(path, ec);
    }

    [[nodiscard]] std::optional<std::string> read_file(const fs::path& path) const override {
        return read_file_string(path);
    }
};

class MockSchedExtFs final : public ISchedExtFs {
public:
    std::unordered_map<std::string, std::string> files;

    [[nodiscard]] bool exists(const fs::path& path) const override {
        return files.find(path.generic_string()) != files.end();
    }

    [[nodiscard]] std::optional<std::string> read_file(const fs::path& path) const override {
        auto it = files.find(path.generic_string());
        if (it != files.end()) return it->second;
        return std::nullopt;
    }
};

class SchedExtTuner {
public:
    explicit SchedExtTuner(ISchedExtFs* fs = nullptr);
    ~SchedExtTuner();

    [[nodiscard]] bool is_supported() const;
    [[nodiscard]] bool is_active() const;
    [[nodiscard]] std::string get_active_scheduler() const;
    [[nodiscard]] std::vector<std::string> get_available_schedulers() const;

    bool start_scheduler(const std::string& scheduler_name = "scx_lavd");
    bool stop_scheduler();

    [[nodiscard]] bool is_running() const { return running_; }
    [[nodiscard]] const std::string& current_scheduler() const { return current_scheduler_; }

    // The engine owns the scheduler for the game session. The CLI can leave it
    // running after the process exits.
    void set_auto_stop(bool enabled) { auto_stop_ = enabled; }

    // Mock command runner hook
    using CommandExecutor = std::function<std::pair<bool, std::string>(const std::string&, const std::vector<std::string>&)>;
    void set_command_executor(CommandExecutor executor) { cmd_executor_ = std::move(executor); }

private:
    ISchedExtFs* fs_;
    bool running_ = false;
    bool auto_stop_ = true;
    std::string current_scheduler_;
    CommandExecutor cmd_executor_;

    [[nodiscard]] std::pair<bool, std::string> run_cmd(const std::string& bin, const std::vector<std::string>& args);
    [[nodiscard]] std::string find_scheduler_binary(const std::string& scheduler_name) const;
};

} // namespace zenith
