#pragma once

#include "zenith/tweaks/tweak_interface.hpp"
#include <unordered_map>

namespace zenith {

class NvmeTweak : public ITweak {
public:
    explicit NvmeTweak(fs::path block_root = "/sys/block");

    std::string name() const override { return "nvme_io_scheduler_readahead"; }
    std::string description() const override { return "NVMe blk-mq scheduler bypass (none) and read-ahead tuning"; }
    uint32_t supported_profiles() const override { return ProfileMask::All; }

    bool probe() override;
    bool apply(Profile profile) override;
    bool revert() override;
    std::string describe() const override;
    std::string explain() const override;

    std::string get_current_value() const override;
    std::string get_target_value(Profile profile) const override;

private:
    fs::path block_root_;
    std::vector<fs::path> nvme_queues_;
    std::unordered_map<std::string, std::string> saved_schedulers_;
    std::unordered_map<std::string, std::string> saved_readahead_;
};

} // namespace zenith
