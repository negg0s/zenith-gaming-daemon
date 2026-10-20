#include "zenith/cgroup_isolator.hpp"
#include "zenith/game_detector.hpp"

namespace zenith {

CgroupIsolator::CgroupIsolator(fs::path cgroup_root)
    : cgroup_root_(std::move(cgroup_root)) {
    background_slice_ = cgroup_root_ / "zenith_background";
}

CgroupIsolator::~CgroupIsolator() {
    rollback();
}

bool CgroupIsolator::is_supported() const {
    return fs::exists(cgroup_root_ / "cgroup.controllers");
}

bool CgroupIsolator::setup_background_slice(const std::string& background_cpuset) {
    if (!is_supported()) {
        ZENITH_LOG_WARN("cgroups v2 not mounted or unsupported.");
        return false;
    }

    try {
        if (!fs::exists(background_slice_)) {
            fs::create_directories(background_slice_);
        }

        // Enable cpuset in root controllers if needed
        auto root_subtree = cgroup_root_ / "cgroup.subtree_control";
        if (auto current_sub = read_file_string(root_subtree)) {
            if (current_sub->find("cpuset") == std::string::npos) {
                write_file_string(root_subtree, "+cpuset");
            }
        }

        // Set cpuset.cpus on the background slice
        write_file_string(background_slice_ / "cpuset.cpus", background_cpuset);
        ZENITH_LOG_INFO("Configured background cgroup cpuset: " + background_cpuset);
        active_ = true;
        return true;
    } catch (const std::exception& e) {
        ZENITH_LOG_ERROR(std::string("Failed to setup background cgroup slice: ") + e.what());
        return false;
    }
}

bool CgroupIsolator::isolate_background_pids(const std::vector<int>& pids) {
    if (!active_ || !fs::exists(background_slice_)) return false;

    auto procs_file = background_slice_ / "cgroup.procs";
    int migrated = 0;
    for (int pid : pids) {
        if (write_file_string(procs_file, std::to_string(pid))) {
            migrated++;
        }
    }
    ZENITH_LOG_DEBUG("Migrated " + std::to_string(migrated) + " processes to background slice.");
    return migrated > 0;
}

bool CgroupIsolator::isolate_background_by_name(const std::vector<std::string>& process_names) {
    if (!active_) return false;
    std::vector<int> all_pids;
    for (const auto& name : process_names) {
        auto pids = GameDetector::find_pids_by_name(name);
        all_pids.insert(all_pids.end(), pids.begin(), pids.end());
    }
    return isolate_background_pids(all_pids);
}

void CgroupIsolator::rollback() {
    if (!active_) return;
    ZENITH_LOG_INFO("Rolling back background cgroup slice.");
    try {
        if (fs::exists(background_slice_ / "cgroup.procs")) {
            // Move processes back to root cgroup
            if (auto procs = read_file_string(background_slice_ / "cgroup.procs")) {
                auto pids = split(*procs, '\n');
                auto root_procs = cgroup_root_ / "cgroup.procs";
                for (const auto& p : pids) {
                    if (!p.empty()) write_file_string(root_procs, p);
                }
            }
        }
        fs::remove(background_slice_);
    } catch (...) {}
    active_ = false;
}

} // namespace zenith
