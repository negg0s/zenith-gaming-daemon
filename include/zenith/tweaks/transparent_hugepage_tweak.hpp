// SPDX-License-Identifier: MIT
#pragma once

#include "zenith/tweaks/tweak_interface.hpp"

namespace zenith {

class TransparentHugepageTweak : public ITweak {
public:
    explicit TransparentHugepageTweak(
        fs::path thp_root = "/sys/kernel/mm/transparent_hugepage"
    );

    std::string name() const override { return "transparent_hugepage"; }
    std::string description() const override { return "Transparent Hugepage (THP) latency tuning and compaction guard"; }
    uint32_t supported_profiles() const override {
        return ProfileMask::Performance; // Balanced and Aggressive
    }

    bool probe() override;
    bool apply(Profile profile) override;
    bool revert() override;
    std::string describe() const override;
    std::string explain() const override;

    std::string get_current_value() const override;
    std::string get_target_value(Profile profile) const override;

private:
    fs::path thp_root_;
    fs::path defrag_path_;
    fs::path khugepaged_defrag_path_;
    fs::path shmem_enabled_path_;

    std::string saved_defrag_;
    std::string saved_khugepaged_defrag_;
    std::string saved_shmem_enabled_;
    bool applied_ = false;
};

} // namespace zenith
