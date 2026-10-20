#pragma once

#include "zenith/common.hpp"

namespace zenith {

struct SysctlEntry {
    fs::path path;
    std::string original_value;
    std::string tuned_value;
    bool modified = false;
};

class SysctlTuner {
public:
    SysctlTuner() = default;
    ~SysctlTuner();

    bool apply(bool disable_split_lock, bool enable_thp, int swappiness, int dirty_ratio, int compaction);
    void rollback();

    bool is_active() const { return active_; }

private:
    std::vector<SysctlEntry> entries_;
    bool active_ = false;

    bool set_param(const fs::path& path, std::string_view value);
};

} // namespace zenith
