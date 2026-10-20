#pragma once

#include "zenith/common.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <chrono>

namespace zenith {

class IPressureFs {
public:
    virtual ~IPressureFs() = default;
    [[nodiscard]] virtual bool exists(const fs::path& path) const = 0;
    [[nodiscard]] virtual std::optional<std::string> read_file(const fs::path& path) const = 0;
    [[nodiscard]] virtual std::vector<fs::path> list_directory(const fs::path& path) const = 0;
};

class RealPressureFs final : public IPressureFs {
public:
    static RealPressureFs& instance() {
        static RealPressureFs inst;
        return inst;
    }

    [[nodiscard]] bool exists(const fs::path& path) const override {
        std::error_code ec;
        return fs::exists(path, ec);
    }

    [[nodiscard]] std::optional<std::string> read_file(const fs::path& path) const override {
        return read_file_string(path);
    }

    [[nodiscard]] virtual std::vector<fs::path> list_directory(const fs::path& path) const override {
        std::vector<fs::path> res;
        std::error_code ec;
        if (!fs::exists(path, ec)) return res;
        for (const auto& entry : fs::directory_iterator(path, ec)) {
            res.push_back(entry.path());
        }
        return res;
    }
};

class MockPressureFs final : public IPressureFs {
public:
    std::unordered_map<std::string, std::string> files;
    std::unordered_map<std::string, std::vector<fs::path>> dirs;

    [[nodiscard]] bool exists(const fs::path& path) const override {
        std::string p = path.generic_string();
        return files.find(p) != files.end() || dirs.find(p) != dirs.end();
    }

    [[nodiscard]] std::optional<std::string> read_file(const fs::path& path) const override {
        auto it = files.find(path.generic_string());
        if (it != files.end()) return it->second;
        return std::nullopt;
    }

    [[nodiscard]] std::vector<fs::path> list_directory(const fs::path& path) const override {
        auto it = dirs.find(path.generic_string());
        if (it != dirs.end()) return it->second;
        return {};
    }
};

struct PsiMetric {
    double avg10 = 0.0;
    double avg60 = 0.0;
    double avg300 = 0.0;
    uint64_t total_us = 0;
};

struct SystemPressure {
    PsiMetric cpu_some;
    PsiMetric memory_some;
    PsiMetric memory_full;
    PsiMetric io_some;
    PsiMetric io_full;

    uint32_t cpu_max_temp_c = 0;
    uint32_t gpu_max_temp_c = 0;
    uint64_t thermal_throttle_count = 0;
    bool is_thermal_throttling = false;
    std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

class PressureMonitor {
public:
    explicit PressureMonitor(IPressureFs* fs = nullptr);

    [[nodiscard]] bool is_psi_supported() const;
    [[nodiscard]] SystemPressure sample();

    // Helper static parser for PSI file content
    static void parse_psi_file(
        std::string_view content,
        PsiMetric& out_some,
        std::optional<std::reference_wrapper<PsiMetric>> out_full = std::nullopt
    );

private:
    IPressureFs* fs_;
    uint64_t last_throttle_count_ = 0;
    bool baseline_initialized_ = false;

    void sample_psi(SystemPressure& sp) const;
    void sample_thermals(SystemPressure& sp);
};

} // namespace zenith
