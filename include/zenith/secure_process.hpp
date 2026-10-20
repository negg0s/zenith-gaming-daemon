#pragma once

#include "zenith/common.hpp"

namespace zenith {

struct SubprocessResult {
    bool executed = false;
    int exit_code = -1;
    bool timed_out = false;
    std::string stdout_output;
};

class SecureProcess {
public:
    // Approved binary whitelist
    static inline const std::vector<std::string> APPROVED_BINARIES = {
        "/usr/bin/nvidia-smi",
        "/usr/bin/pw-metadata"
    };

    static bool is_binary_approved(std::string_view path);

    // Executes whitelisted binary via direct fork() + execve() with zero shell involvement
    static SubprocessResult execute(const std::string& binary_path,
                                    const std::vector<std::string>& args,
                                    int timeout_ms = 3000,
                                    bool capture_stdout = false);
};

} // namespace zenith
