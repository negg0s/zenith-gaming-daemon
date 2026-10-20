// SPDX-License-Identifier: MIT
#pragma once

#include "zenith/common.hpp"
#include <string>
#include <optional>
#include <functional>

namespace zenith {

class INetworkFs {
public:
    virtual ~INetworkFs() = default;
    [[nodiscard]] virtual bool exists(const fs::path& path) const = 0;
    [[nodiscard]] virtual std::optional<std::string> read_file(const fs::path& path) const = 0;
};

class RealNetworkFs final : public INetworkFs {
public:
    static RealNetworkFs& instance() {
        static RealNetworkFs inst;
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

class MockNetworkFs final : public INetworkFs {
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

class NetworkTuner {
public:
    explicit NetworkTuner(INetworkFs* fs = nullptr);
    ~NetworkTuner();

    [[nodiscard]] bool probe();
    bool apply();
    bool revert();
    [[nodiscard]] std::string describe() const;

    [[nodiscard]] std::string get_active_interface() const;
    [[nodiscard]] bool is_applied() const { return applied_; }
    [[nodiscard]] const std::string& original_qdisc() const { return original_qdisc_; }
    [[nodiscard]] bool wifi_power_save_disabled() const { return wifi_power_save_disabled_; }

    // Mock command runner hook for testing without root / tc / iw binaries
    using CommandExecutor = std::function<std::pair<bool, std::string>(const std::string&, const std::vector<std::string>&)>;
    void set_command_executor(CommandExecutor executor) { cmd_executor_ = std::move(executor); }

private:
    INetworkFs* fs_;
    bool applied_ = false;
    std::string active_iface_;
    std::string original_qdisc_;
    bool wifi_power_save_disabled_ = false;
    CommandExecutor cmd_executor_;

    [[nodiscard]] std::pair<bool, std::string> run_cmd(const std::string& bin, const std::vector<std::string>& args);
    void discover_active_interface();
    void query_current_qdisc();
};

} // namespace zenith
