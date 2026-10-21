// SPDX-License-Identifier: MIT
#include "zenith/tweaks/transparent_hugepage_tweak.hpp"

namespace zenith {

namespace {

std::string extract_bracketed_token(std::string_view s) {
    auto start = s.find('[');
    auto end = s.find(']', start);
    if (start != std::string_view::npos && end != std::string_view::npos && end > start) {
        return std::string(s.substr(start + 1, end - start - 1));
    }
    return trim(s);
}

} // namespace

TransparentHugepageTweak::TransparentHugepageTweak(fs::path thp_root)
    : thp_root_(std::move(thp_root)),
      defrag_path_(thp_root_ / "defrag"),
      khugepaged_defrag_path_(thp_root_ / "khugepaged" / "defrag"),
      shmem_enabled_path_(thp_root_ / "shmem_enabled") {}

bool TransparentHugepageTweak::probe() {
    std::error_code ec;
    return fs::exists(defrag_path_, ec);
}

bool TransparentHugepageTweak::apply(Profile profile) {
    if (!probe()) return false;

    // 1. Save original defrag setting
    if (auto cur_defrag = read_file_string(defrag_path_)) {
        if (saved_defrag_.empty()) {
            saved_defrag_ = extract_bracketed_token(*cur_defrag);
        }
    }

    // 2. Save original khugepaged defrag setting
    if (fs::exists(khugepaged_defrag_path_)) {
        if (auto cur_khp = read_file_string(khugepaged_defrag_path_)) {
            if (saved_khugepaged_defrag_.empty()) {
                saved_khugepaged_defrag_ = trim(*cur_khp);
            }
        }
    }

    // 3. Save and tune shmem_enabled for DXVK/VKD3D shared memory
    if (fs::exists(shmem_enabled_path_)) {
        if (auto cur_shmem = read_file_string(shmem_enabled_path_)) {
            if (saved_shmem_enabled_.empty()) {
                saved_shmem_enabled_ = extract_bracketed_token(*cur_shmem);
            }
        }
        std::string target_shmem = (profile == Profile::Ultra) ? "advise" : "advise";
        write_file_string(shmem_enabled_path_, target_shmem);
    }

    // Target values
    std::string target_defrag = (profile == Profile::Aggressive || profile == Profile::Ultra) ? "madvise" : "madvise";
    write_file_string(defrag_path_, target_defrag);

    if (fs::exists(khugepaged_defrag_path_)) {
        write_file_string(khugepaged_defrag_path_, "0");
    }

    applied_ = true;
    return true;
}

bool TransparentHugepageTweak::revert() {
    if (!applied_) return false;

    if (!saved_defrag_.empty() && fs::exists(defrag_path_)) {
        write_file_string(defrag_path_, saved_defrag_);
        saved_defrag_.clear();
    }

    if (!saved_khugepaged_defrag_.empty() && fs::exists(khugepaged_defrag_path_)) {
        write_file_string(khugepaged_defrag_path_, saved_khugepaged_defrag_);
        saved_khugepaged_defrag_.clear();
    }

    if (!saved_shmem_enabled_.empty() && fs::exists(shmem_enabled_path_)) {
        write_file_string(shmem_enabled_path_, saved_shmem_enabled_);
        saved_shmem_enabled_.clear();
    }

    applied_ = false;
    return true;
}

std::string TransparentHugepageTweak::describe() const {
    return "Eliminates synchronous memory stall delays caused by direct THP memory defragmentation";
}

std::string TransparentHugepageTweak::explain() const {
    return "When transparent hugepages are set to 'always', page faults trigger direct compaction "
           "synchronously within the faulting thread when contiguous 2MB memory blocks are unavailable. "
           "This stalls rendering threads for 10 to 80 milliseconds. Setting defrag to 'madvise' restricts "
           "hugepage allocation to processes requesting it explicitly and prevents synchronous compaction stalls. "
           "Setting khugepaged/defrag to 0 prevents background compaction thread wake-ups while gaming.";
}

std::string TransparentHugepageTweak::get_current_value() const {
    if (auto cur = read_file_string(defrag_path_)) {
        return extract_bracketed_token(*cur);
    }
    return "unknown";
}

std::string TransparentHugepageTweak::get_target_value(Profile profile) const {
    if (profile == Profile::Aggressive || profile == Profile::Balanced || profile == Profile::Ultra) {
        return "madvise (khugepaged defrag=0)";
    }
    return "distro default";
}

} // namespace zenith
