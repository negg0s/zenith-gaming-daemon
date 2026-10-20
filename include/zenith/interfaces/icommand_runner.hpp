#pragma once

#include "zenith/result.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace zenith {

struct CommandOutput {
    int exit_code{0};
    std::string stdout_str;
    std::string stderr_str;
};

class ICommandRunner {
public:
    virtual ~ICommandRunner() = default;

    virtual Result<CommandOutput> run(const std::string& binary,
                                     const std::vector<std::string>& args) = 0;
};

class MockCommandRunner : public ICommandRunner {
public:
    struct Invocation {
        std::string binary;
        std::vector<std::string> args;
    };

    void set_canned_output(const std::string& binary, CommandOutput output) {
        std::lock_guard<std::mutex> lock(mutex_);
        canned_outputs_[binary] = std::move(output);
    }

    Result<CommandOutput> run(const std::string& binary,
                              const std::vector<std::string>& args) override {
        std::lock_guard<std::mutex> lock(mutex_);
        history_.push_back({binary, args});

        auto it = canned_outputs_.find(binary);
        if (it != canned_outputs_.end()) {
            return Ok(it->second);
        }
        return Ok(CommandOutput{0, "", ""});
    }

    const std::vector<Invocation>& history() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return history_;
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, CommandOutput> canned_outputs_;
    std::vector<Invocation> history_;
};

} // namespace zenith
