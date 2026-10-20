#pragma once

#include "zenith/common.hpp"
#include <string>
#include <vector>
#include <optional>

namespace zenith {

class ITopologyFs {
public:
    virtual ~ITopologyFs() = default;
    virtual bool exists(const fs::path& p) const = 0;
    virtual bool is_directory(const fs::path& p) const = 0;
    virtual std::optional<std::string> read_file(const fs::path& p) const = 0;
    virtual std::vector<fs::path> list_directory(const fs::path& p) const = 0;
};

class RealTopologyFs : public ITopologyFs {
public:
    bool exists(const fs::path& p) const override {
        std::error_code ec;
        return fs::exists(p, ec);
    }

    bool is_directory(const fs::path& p) const override {
        std::error_code ec;
        return fs::is_directory(p, ec);
    }

    std::optional<std::string> read_file(const fs::path& p) const override {
        return read_file_string(p);
    }

    std::vector<fs::path> list_directory(const fs::path& p) const override {
        std::vector<fs::path> result;
        std::error_code ec;
        if (!fs::exists(p, ec) || !fs::is_directory(p, ec)) return result;

        for (const auto& entry : fs::directory_iterator(p, ec)) {
            result.push_back(entry.path());
        }
        return result;
    }
};

class MockTopologyFs : public ITopologyFs {
public:
    explicit MockTopologyFs(fs::path fixture_root)
        : fixture_root_(std::move(fixture_root)) {}

    fs::path remap(const fs::path& p) const {
        std::string s = p.string();
        while (!s.empty() && (s.front() == '/' || s.front() == '\\')) {
            s.erase(0, 1);
        }
        return fixture_root_ / s;
    }

    bool exists(const fs::path& p) const override {
        std::error_code ec;
        return fs::exists(remap(p), ec);
    }

    bool is_directory(const fs::path& p) const override {
        std::error_code ec;
        return fs::is_directory(remap(p), ec);
    }

    std::optional<std::string> read_file(const fs::path& p) const override {
        return read_file_string(remap(p));
    }

    std::vector<fs::path> list_directory(const fs::path& p) const override {
        std::vector<fs::path> result;
        std::error_code ec;
        fs::path remapped = remap(p);
        if (!fs::exists(remapped, ec) || !fs::is_directory(remapped, ec)) return result;

        for (const auto& entry : fs::directory_iterator(remapped, ec)) {
            // Return virtual path (unmapped) so callers see "/sys/devices/system/cpu/cpu0"
            fs::path sub = p / entry.path().filename();
            result.push_back(sub);
        }
        return result;
    }

private:
    fs::path fixture_root_;
};

} // namespace zenith
