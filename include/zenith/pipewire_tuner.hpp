#pragma once

#include "zenith/common.hpp"
#include <string>
#include <functional>
#include <optional>

namespace zenith {

class IPipewireFs {
public:
    virtual ~IPipewireFs() = default;
    [[nodiscard]] virtual bool exists(const fs::path& path) const = 0;
    virtual bool write_file(const fs::path& path, const std::string& content) = 0;
    virtual bool remove_file(const fs::path& path) = 0;
};

class RealPipewireFs final : public IPipewireFs {
public:
    static RealPipewireFs& instance() {
        static RealPipewireFs inst;
        return inst;
    }

    [[nodiscard]] bool exists(const fs::path& path) const override {
        std::error_code ec;
        return fs::exists(path, ec);
    }

    bool write_file(const fs::path& path, const std::string& content) override {
        return write_file_string(path, content);
    }

    bool remove_file(const fs::path& path) override {
        std::error_code ec;
        return fs::remove(path, ec);
    }
};

class MockPipewireFs final : public IPipewireFs {
public:
    std::unordered_map<std::string, std::string> files;

    [[nodiscard]] bool exists(const fs::path& path) const override {
        return files.find(path.generic_string()) != files.end();
    }

    bool write_file(const fs::path& path, const std::string& content) override {
        files[path.generic_string()] = content;
        return true;
    }

    bool remove_file(const fs::path& path) override {
        return files.erase(path.generic_string()) > 0;
    }
};

class PipewireTuner {
public:
    explicit PipewireTuner(IPipewireFs* fs = nullptr);
    ~PipewireTuner();

    [[nodiscard]] bool probe();
    bool apply(int quantum = 64, int sample_rate = 48000);
    bool revert();
    [[nodiscard]] std::string describe() const;

    // xrun Guardian: report and auto-mitigate audio dropouts
    bool handle_xrun_detected(uint64_t xrun_count);

    [[nodiscard]] bool is_applied() const { return applied_; }
    [[nodiscard]] int current_quantum() const { return current_quantum_; }
    [[nodiscard]] int original_quantum() const { return original_quantum_; }
    [[nodiscard]] uint64_t xrun_count() const { return xrun_count_; }
    [[nodiscard]] uint64_t quantum_elevations_count() const { return quantum_elevations_count_; }

    // Mock command runner hook
    using CommandExecutor = std::function<std::pair<bool, std::string>(const std::string&, const std::vector<std::string>&)>;
    void set_command_executor(CommandExecutor executor) { cmd_executor_ = std::move(executor); }

    static constexpr std::string_view DROPIN_PATH = "/run/pipewire/pipewire.conf.d/99-zenith.conf";

private:
    IPipewireFs* fs_;
    bool applied_ = false;
    int current_quantum_ = 64;
    int original_quantum_ = 0;
    int sample_rate_ = 48000;
    uint64_t xrun_count_ = 0;
    uint64_t quantum_elevations_count_ = 0;
    CommandExecutor cmd_executor_;

    [[nodiscard]] std::pair<bool, std::string> run_cmd(const std::string& bin, const std::vector<std::string>& args);
    void write_dropin_config(int quantum);
};

} // namespace zenith
