// SPDX-License-Identifier: MIT
#pragma once

#include "zenith/common.hpp"
#include "zenith/proc_fs.hpp"
#include <string>
#include <vector>
#include <unordered_set>
#include <string_view>

namespace zenith {

enum class ProcessCategory {
    Whitelisted,  // Protected system/compositor/audio/agent terminal: DO NOT TOUCH
    Blacklisted,  // Non-critical background app: Throttle via cgroup v2
    Neutral       // Uncategorized
};

class BackgroundManager {
public:
    explicit BackgroundManager(IProcFs* fs = nullptr);

    [[nodiscard]] ProcessCategory classify_process(std::string_view comm, std::string_view cmdline) const;
    [[nodiscard]] bool is_whitelisted(std::string_view comm, std::string_view cmdline) const;
    [[nodiscard]] bool is_blacklisted(std::string_view comm, std::string_view cmdline) const;

    // Throttle all blacklisted processes currently running
    size_t throttle_blacklisted_processes();

    // Throttle a specific PID by moving to zenith_background cgroup
    bool throttle_pid(int pid);

    // Freeze all demoted background processes in cgroup (0% CPU, 0 cache, 0 disk I/O)
    bool freeze_background();

    // Thaw/resume all demoted background processes
    bool unfreeze_background();

    [[nodiscard]] bool is_background_frozen() const { return is_frozen_; }

    // Restore all throttled processes back to default/root cgroup
    size_t restore_all_throttled();

    [[nodiscard]] const std::unordered_set<int>& throttled_pids() const { return throttled_pids_; }

    void add_whitelist_rule(std::string pattern);
    void add_blacklist_rule(std::string pattern);

    static constexpr std::string_view CGROUP_BACKGROUND_DIR = "/sys/fs/cgroup/zenith_background";

private:
    IProcFs* fs_;
    std::vector<std::string> whitelist_rules_;
    std::vector<std::string> blacklist_rules_;
    std::unordered_set<int> throttled_pids_;
    bool is_frozen_ = false;

    void setup_cgroup() const;
};

} // namespace zenith
