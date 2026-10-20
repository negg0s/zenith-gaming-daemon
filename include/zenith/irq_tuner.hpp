// SPDX-License-Identifier: MIT
#pragma once

#include "zenith/common.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>

namespace zenith {

class IIrqFs {
public:
    virtual ~IIrqFs() = default;
    [[nodiscard]] virtual bool exists(const fs::path& path) const = 0;
    [[nodiscard]] virtual std::optional<std::string> read_file(const fs::path& path) const = 0;
    virtual bool write_file(const fs::path& path, const std::string& content) = 0;
};

class RealIrqFs final : public IIrqFs {
public:
    static RealIrqFs& instance() {
        static RealIrqFs inst;
        return inst;
    }

    [[nodiscard]] bool exists(const fs::path& path) const override {
        std::error_code ec;
        return fs::exists(path, ec);
    }

    [[nodiscard]] std::optional<std::string> read_file(const fs::path& path) const override {
        return read_file_string(path);
    }

    bool write_file(const fs::path& path, const std::string& content) override {
        return write_file_string(path, content);
    }
};

class MockIrqFs final : public IIrqFs {
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

    bool write_file(const fs::path& path, const std::string& content) override {
        files[path.generic_string()] = content;
        return true;
    }
};

enum class IrqTarget {
    Background, // Housekeeping cores: Network, USB, NVMe
    Gaming      // Gaming CCD: GPU
};

struct DiscoveredIrq {
    int irq = -1;
    std::string name;
    IrqTarget target = IrqTarget::Background;
    std::string original_affinity;
};

class IrqTuner {
public:
    explicit IrqTuner(IIrqFs* fs = nullptr);
    ~IrqTuner();

    [[nodiscard]] bool probe();
    bool apply(const std::vector<int>& gaming_cores, const std::vector<int>& housekeeping_cores);
    bool revert();
    [[nodiscard]] std::string describe() const;

    [[nodiscard]] const std::vector<DiscoveredIrq>& discovered_irqs() const { return discovered_irqs_; }
    [[nodiscard]] bool is_applied() const { return applied_; }
    [[nodiscard]] bool was_irqbalance_stopped() const { return irqbalance_stopped_; }

    // Mock hook for irqbalance command execution in tests
    void set_irqbalance_present(bool present) { irqbalance_present_ = present; }

private:
    IIrqFs* fs_;
    bool applied_ = false;
    bool irqbalance_present_ = false;
    bool irqbalance_stopped_ = false;
    std::vector<DiscoveredIrq> discovered_irqs_;

    void discover_irqs();
};

} // namespace zenith
