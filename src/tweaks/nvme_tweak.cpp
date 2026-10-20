// SPDX-License-Identifier: MIT
#include "zenith/tweaks/nvme_tweak.hpp"

namespace zenith {

NvmeTweak::NvmeTweak(fs::path block_root)
    : block_root_(std::move(block_root)) {}

bool NvmeTweak::probe() {
    nvme_queues_.clear();
    std::error_code ec;
    if (!fs::exists(block_root_, ec) || !fs::is_directory(block_root_, ec)) {
        return false;
    }

    for (const auto& entry : fs::directory_iterator(block_root_, ec)) {
        std::string name = entry.path().filename().string();
        if (name.starts_with("nvme")) {
            fs::path queue_dir = entry.path() / "queue";
            if (fs::exists(queue_dir, ec)) {
                nvme_queues_.push_back(queue_dir);
            }
        }
    }

    return !nvme_queues_.empty();
}

bool NvmeTweak::apply(Profile profile) {
    if (!probe()) return false;

    std::string target_sched = "none";
    std::string target_readahead = (profile == Profile::Aggressive) ? "256" : "128";

    bool any_applied = false;
    for (const auto& queue : nvme_queues_) {
        // 1. Scheduler -> none
        fs::path sched_path = queue / "scheduler";
        if (fs::exists(sched_path)) {
            auto cur = read_file_string(sched_path);
            if (cur && saved_schedulers_.find(sched_path.string()) == saved_schedulers_.end()) {
                // Extract bracketed token
                auto start = cur->find('[');
                auto end = cur->find(']', start);
                std::string token = (start != std::string::npos && end != std::string::npos)
                                    ? cur->substr(start + 1, end - start - 1) : *cur;
                saved_schedulers_[sched_path.string()] = token;
            }
            if (write_file_string(sched_path, target_sched)) {
                any_applied = true;
            }
        }

        // 2. Read-ahead
        fs::path ra_path = queue / "read_ahead_kb";
        if (fs::exists(ra_path)) {
            auto cur = read_file_string(ra_path);
            if (cur && saved_readahead_.find(ra_path.string()) == saved_readahead_.end()) {
                saved_readahead_[ra_path.string()] = *cur;
            }
            if (write_file_string(ra_path, target_readahead)) {
                any_applied = true;
            }
        }
    }

    return any_applied;
}

bool NvmeTweak::revert() {
    bool all_ok = true;

    for (const auto& [path, original] : saved_schedulers_) {
        if (!write_file_string(path, original)) all_ok = false;
    }
    saved_schedulers_.clear();

    for (const auto& [path, original] : saved_readahead_) {
        if (!write_file_string(path, original)) all_ok = false;
    }
    saved_readahead_.clear();

    return all_ok;
}

std::string NvmeTweak::describe() const {
    return "NVMe blk-mq scheduler and readahead for " + std::to_string(nvme_queues_.size()) + " NVMe devices";
}

std::string NvmeTweak::explain() const {
    return "Sets NVMe block queue scheduler to 'none', bypassing unnecessary software I/O elevator locking overhead. "
           "Allows DirectStorage and game asset streaming to leverage hardware NVMe submission queues directly. "
           "Risks: None on NVMe drives.";
}

std::string NvmeTweak::get_current_value() const {
    if (nvme_queues_.empty()) return "no_nvme_detected";
    std::string sample = "unknown";
    if (auto cur = read_file_string(nvme_queues_.front() / "scheduler")) {
        sample = *cur;
    }
    return "scheduler=" + sample;
}

std::string NvmeTweak::get_target_value(Profile profile) const {
    std::string ra = (profile == Profile::Aggressive) ? "256" : "128";
    return "scheduler=none, read_ahead_kb=" + ra;
}

} // namespace zenith
