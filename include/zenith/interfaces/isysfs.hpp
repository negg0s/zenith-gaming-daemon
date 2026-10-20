// SPDX-License-Identifier: MIT
#pragma once

#include "zenith/result.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <cerrno>

namespace zenith {

class ISysfs {
public:
    virtual ~ISysfs() = default;

    [[nodiscard]] virtual Result<std::string> read_string(const std::string& path) const = 0;
    virtual Result<void> write_string(const std::string& path, const std::string& content) = 0;
    [[nodiscard]] virtual bool exists(const std::string& path) const = 0;
    [[nodiscard]] virtual Result<std::vector<std::string>> list_directory(const std::string& path) const = 0;
};

class RealSysfs : public ISysfs {
public:
    Result<std::string> read_string(const std::string& path) const override {
        std::ifstream file(path);
        if (!file.is_open()) {
            return Result<std::string>(Error(ErrorCode::NotFound, "Failed to open sysfs file for read: " + path, errno));
        }
        std::string content((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
        // Trim trailing newline
        if (!content.empty() && content.back() == '\n') {
            content.pop_back();
        }
        return Ok(std::move(content));
    }

    Result<void> write_string(const std::string& path, const std::string& content) override {
        std::ofstream file(path);
        if (!file.is_open()) {
            return Result<void>::err(Error(ErrorCode::PermissionDenied, "Failed to open sysfs file for write: " + path, errno));
        }
        file << content;
        if (!file.good()) {
            return Result<void>::err(Error(ErrorCode::IoError, "Write failed on sysfs path: " + path, errno));
        }
        return Ok();
    }

    bool exists(const std::string& path) const override {
        std::error_code ec;
        return std::filesystem::exists(path, ec);
    }

    Result<std::vector<std::string>> list_directory(const std::string& path) const override {
        std::error_code ec;
        if (!std::filesystem::is_directory(path, ec)) {
            return Result<std::vector<std::string>>(Error(ErrorCode::NotFound, "Path is not a directory: " + path));
        }
        std::vector<std::string> entries;
        for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
            entries.push_back(entry.path().filename().string());
        }
        if (ec) {
            return Result<std::vector<std::string>>(Error(ErrorCode::IoError, "Directory iteration error: " + ec.message()));
        }
        return Ok(std::move(entries));
    }
};

class MockSysfs : public ISysfs {
public:
    void set_file(const std::string& path, std::string content) {
        std::lock_guard<std::mutex> lock(mutex_);
        files_[path] = std::move(content);
    }

    void remove_file(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        files_.erase(path);
    }

    Result<std::string> read_string(const std::string& path) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = files_.find(path);
        if (it == files_.end()) {
            return Result<std::string>(Error(ErrorCode::NotFound, "MockSysfs file not found: " + path));
        }
        return Ok(it->second);
    }

    Result<void> write_string(const std::string& path, const std::string& content) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (fail_all_writes_) {
            return Result<void>::err(Error(ErrorCode::PermissionDenied, "MockSysfs simulated write failure"));
        }
        files_[path] = content;
        written_history_.push_back({path, content});
        return Ok();
    }

    bool exists(const std::string& path) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return files_.find(path) != files_.end();
    }

    Result<std::vector<std::string>> list_directory(const std::string& path) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> entries;
        std::string prefix = path;
        if (!prefix.empty() && prefix.back() != '/') {
            prefix += '/';
        }
        for (const auto& [filepath, _] : files_) {
            if (filepath.rfind(prefix, 0) == 0) {
                std::string rest = filepath.substr(prefix.size());
                auto slash = rest.find('/');
                std::string entry = (slash == std::string::npos) ? rest : rest.substr(0, slash);
                if (!entry.empty()) {
                    entries.push_back(entry);
                }
            }
        }
        return Ok(std::move(entries));
    }

    void set_fail_writes(bool fail) {
        std::lock_guard<std::mutex> lock(mutex_);
        fail_all_writes_ = fail;
    }

    const std::vector<std::pair<std::string, std::string>>& written_history() const {
        return written_history_;
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::string> files_;
    std::vector<std::pair<std::string, std::string>> written_history_;
    bool fail_all_writes_{false};
};

} // namespace zenith
