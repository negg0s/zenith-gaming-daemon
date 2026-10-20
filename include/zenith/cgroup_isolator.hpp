// SPDX-License-Identifier: MIT
#pragma once

#include "zenith/common.hpp"

namespace zenith {

class CgroupIsolator {
public:
    explicit CgroupIsolator(fs::path cgroup_root = "/sys/fs/cgroup");
    ~CgroupIsolator();

    bool is_supported() const;
    bool setup_background_slice(const std::string& background_cpuset);
    bool isolate_background_pids(const std::vector<int>& pids);
    bool isolate_background_by_name(const std::vector<std::string>& process_names);
    void rollback();

private:
    fs::path cgroup_root_;
    fs::path background_slice_;
    bool active_ = false;
    std::string original_root_cpuset_;
};

} // namespace zenith
