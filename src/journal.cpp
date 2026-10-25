// SPDX-License-Identifier: MIT
#include "zenith/journal.hpp"
#include "zenith/secure_process.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <ctime>
#include <cstdlib>

#ifdef __linux__
#include <unistd.h>
#else
#include <io.h>
#define fsync _commit
#endif

namespace zenith {

std::string mutation_type_to_string(MutationType type) {
    switch (type) {
        case MutationType::SysctlString:   return "SYSCTL_STRING";
        case MutationType::SysfsToken:      return "SYSFS_TOKEN";
        case MutationType::SysctlInt:       return "SYSCTL_INT";
        case MutationType::GpuAmdDpm:       return "GPU_AMD_DPM";
        case MutationType::GpuNvidiaSmi:    return "GPU_NVIDIA_SMI";
        case MutationType::CgroupSlice:     return "CGROUP_SLICE";
        case MutationType::ProcSched:       return "PROC_SCHED";
        case MutationType::ProcNice:        return "PROC_NICE";
        case MutationType::ProcIoPrio:      return "PROC_IOPRIO";
        case MutationType::ProcAffinity:    return "PROC_AFFINITY";
        case MutationType::PipewireQuantum: return "PIPEWIRE_QUANTUM";
        case MutationType::IrqAffinity:     return "IRQ_AFFINITY";
        case MutationType::GenericFile:     return "GENERIC_FILE";
    }
    return "UNKNOWN";
}

MutationType mutation_type_from_string(std::string_view str) {
    if (str == "SYSCTL_STRING")   return MutationType::SysctlString;
    if (str == "SYSFS_TOKEN")      return MutationType::SysfsToken;
    if (str == "SYSCTL_INT")       return MutationType::SysctlInt;
    if (str == "GPU_AMD_DPM")      return MutationType::GpuAmdDpm;
    if (str == "GPU_NVIDIA_SMI")   return MutationType::GpuNvidiaSmi;
    if (str == "CGROUP_SLICE")     return MutationType::CgroupSlice;
    if (str == "PROC_SCHED")       return MutationType::ProcSched;
    if (str == "PROC_NICE")        return MutationType::ProcNice;
    if (str == "PROC_IOPRIO")      return MutationType::ProcIoPrio;
    if (str == "PROC_AFFINITY")    return MutationType::ProcAffinity;
    if (str == "PIPEWIRE_QUANTUM") return MutationType::PipewireQuantum;
    if (str == "IRQ_AFFINITY")     return MutationType::IrqAffinity;
    return MutationType::GenericFile;
}

std::string entry_state_to_string(EntryState state) {
    switch (state) {
        case EntryState::Pending:  return "PENDING";
        case EntryState::Applied:  return "APPLIED";
        case EntryState::Reverted: return "REVERTED";
        case EntryState::Failed:   return "FAILED";
    }
    return "PENDING";
}

EntryState entry_state_from_string(std::string_view str) {
    if (str == "APPLIED")  return EntryState::Applied;
    if (str == "REVERTED") return EntryState::Reverted;
    if (str == "FAILED")   return EntryState::Failed;
    return EntryState::Pending;
}

static std::string current_iso_timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    gmtime_s(&tm_buf, &tt);
#else
    gmtime_r(&tt, &tm_buf);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    return std::string(buf);
}

StateJournal::StateJournal(fs::path journal_path)
    : journal_path_(std::move(journal_path)) {
    load();
}

void StateJournal::clear() {
    entries_.clear();
    next_id_ = 1;
    atomic_save();
}

bool StateJournal::sync_directory(const fs::path& dir_path) {
#ifdef __linux__
    int fd = ::open(dir_path.string().c_str(), O_RDONLY | O_DIRECTORY);
    if (fd >= 0) {
        ::fsync(fd);
        ::close(fd);
        return true;
    }
    return false;
#else
    (void)dir_path;
    return true;
#endif
}

std::string StateJournal::serialize_json() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"schema_version\": " << SCHEMA_VERSION << ",\n";
    ss << "  \"entries\": [\n";
    for (size_t i = 0; i < entries_.size(); ++i) {
        const auto& e = entries_[i];
        ss << "    {\n";
        ss << "      \"id\": " << e.id << ",\n";
        ss << "      \"type\": \"" << mutation_type_to_string(e.type) << "\",\n";
        auto escape = [](const std::string& s) {
            std::string out;
            for (char c : s) {
                if (c == '"') out += "\\\"";
                else if (c == '\\') out += "\\\\";
                else if (c == '\n') out += "\\n";
                else if (c == '\r') out += "\\r";
                else if (c == '\t') out += "\\t";
                else if (static_cast<unsigned char>(c) < 0x20) out += '?';
                else out += c;
            }
            return out;
        };
        ss << "      \"target_path\": \"" << escape(e.target_path) << "\",\n";
        ss << "      \"original_value\": \"" << escape(e.original_value) << "\",\n";
        ss << "      \"target_value\": \"" << escape(e.target_value) << "\",\n";
        ss << "      \"state\": \"" << entry_state_to_string(e.state) << "\",\n";
        ss << "      \"timestamp\": \"" << e.timestamp << "\"\n";
        ss << "    }" << (i + 1 < entries_.size() ? "," : "") << "\n";
    }
    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

static std::string extract_json_field(std::string_view obj, std::string_view key) {
    std::string needle = "\"" + std::string(key) + "\":";
    auto pos = obj.find(needle);
    if (pos == std::string_view::npos) return "";

    pos += needle.size();
    while (pos < obj.size() && (obj[pos] == ' ' || obj[pos] == '\t' || obj[pos] == '\r' || obj[pos] == '\n')) pos++;

    if (pos < obj.size() && obj[pos] == '"') {
        pos++;
        auto end = obj.find('"', pos);
        if (end == std::string_view::npos) return "";
        return std::string(obj.substr(pos, end - pos));
    } else {
        auto end = obj.find_first_of(",}\n\r", pos);
        if (end == std::string_view::npos) end = obj.size();
        return trim(obj.substr(pos, end - pos));
    }
}

bool StateJournal::deserialize_json(std::string_view json_str) {
    entries_.clear();
    next_id_ = 1;

    auto entries_pos = json_str.find("\"entries\":");
    if (entries_pos == std::string_view::npos) return false;

    size_t cur = json_str.find('[', entries_pos);
    if (cur == std::string_view::npos) return false;

    while (true) {
        auto obj_start = json_str.find('{', cur);
        if (obj_start == std::string_view::npos) break;
        auto obj_end = json_str.find('}', obj_start);
        if (obj_end == std::string_view::npos) break;

        std::string_view block = json_str.substr(obj_start, obj_end - obj_start + 1);

        JournalEntry entry;
        try {
            auto id_str = extract_json_field(block, "id");
            if (!id_str.empty()) entry.id = std::stoull(id_str);
        } catch (...) {}

        entry.type = mutation_type_from_string(extract_json_field(block, "type"));
        entry.target_path = extract_json_field(block, "target_path");
        if (entry.target_path.find("..") != std::string::npos ||
            entry.target_path.find('\0') != std::string::npos) {
            // Reject tampered journal entries with traversal attempts.
            continue;
        }
        entry.original_value = extract_json_field(block, "original_value");
        entry.target_value = extract_json_field(block, "target_value");
        entry.state = entry_state_from_string(extract_json_field(block, "state"));
        entry.timestamp = extract_json_field(block, "timestamp");

        if (entry.id >= next_id_) {
            next_id_ = entry.id + 1;
        }

        entries_.push_back(entry);
        cur = obj_end + 1;
    }

    return true;
}

bool StateJournal::atomic_save() {
    auto parent_dir = journal_path_.parent_path();
    if (!parent_dir.empty()) {
        std::error_code ec;
        fs::create_directories(parent_dir, ec);
    }

    auto now_epoch = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    fs::path tmp_path = journal_path_.string() + ".tmp." + std::to_string(now_epoch);

    std::string payload = serialize_json();

    // 1. Write payload to tmp file and fsync
#ifdef _WIN32
    int fd = ::_open(tmp_path.string().c_str(), _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, _S_IREAD | _S_IWRITE);
#else
    int fd = ::open(tmp_path.string().c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
#endif

    if (fd < 0) {
        ZENITH_LOG_ERROR("Failed to open journal tmp file for writing: " + tmp_path.string());
        return false;
    }

    auto written = ::write(fd, payload.data(), static_cast<unsigned int>(payload.size()));
    if (written < 0 || static_cast<size_t>(written) != payload.size()) {
        ::close(fd);
        std::error_code ec;
        fs::remove(tmp_path, ec);
        ZENITH_LOG_ERROR("Partial write failure on journal: " + tmp_path.string());
        return false;
    }

    // 2. fsync file contents
    if (::fsync(fd) != 0) {
        ::close(fd);
        std::error_code ec;
        fs::remove(tmp_path, ec);
        ZENITH_LOG_ERROR("fsync failed on journal: " + tmp_path.string());
        return false;
    }
    ::close(fd);

    // 3. Atomic rename tmp -> journal_path_
    std::error_code rename_ec;
    fs::rename(tmp_path, journal_path_, rename_ec);
    if (rename_ec) {
        fs::remove(tmp_path);
        ZENITH_LOG_ERROR("Atomic rename failed on journal: " + rename_ec.message());
        return false;
    }

    // 4. fsync parent directory to guarantee metadata persistence
    if (!parent_dir.empty()) {
        sync_directory(parent_dir);
    }

    return true;
}

bool StateJournal::load() {
    if (!fs::exists(journal_path_)) {
        return false;
    }
    auto content = read_file_string(journal_path_);
    if (!content) return false;
    return deserialize_json(*content);
}

uint64_t StateJournal::record_pending(MutationType type,
                                      std::string_view target_path,
                                      std::string_view original_value,
                                      std::string_view target_value) {
    JournalEntry entry;
    entry.id = next_id_++;
    entry.type = type;
    entry.target_path = std::string(target_path);
    entry.original_value = std::string(original_value);
    entry.target_value = std::string(target_value);
    entry.state = EntryState::Pending;
    entry.timestamp = current_iso_timestamp();

    entries_.push_back(entry);
    atomic_save();
    return entry.id;
}

bool StateJournal::mark_applied(uint64_t entry_id) {
    for (auto& e : entries_) {
        if (e.id == entry_id) {
            e.state = EntryState::Applied;
            return atomic_save();
        }
    }
    return false;
}

bool StateJournal::mark_reverted(uint64_t entry_id) {
    for (auto& e : entries_) {
        if (e.id == entry_id) {
            e.state = EntryState::Reverted;
            return atomic_save();
        }
    }
    return false;
}

bool StateJournal::mark_failed(uint64_t entry_id) {
    for (auto& e : entries_) {
        if (e.id == entry_id) {
            e.state = EntryState::Failed;
            return atomic_save();
        }
    }
    return false;
}

bool StateJournal::execute_entry_revert(const JournalEntry& entry) {
    switch (entry.type) {
        case MutationType::SysctlString:
        case MutationType::SysctlInt:
        case MutationType::SysfsToken:
        case MutationType::GenericFile: {
            if (!entry.target_path.empty() && fs::exists(entry.target_path)) {
                return write_file_string(entry.target_path, entry.original_value);
            }
            return true; // Target no longer present, safely considered reverted
        }
        case MutationType::GpuAmdDpm: {
            if (!entry.target_path.empty() && fs::exists(entry.target_path)) {
                std::string level = entry.original_value.empty() ? "auto" : entry.original_value;
                return write_file_string(entry.target_path, level);
            }
            return true;
        }
        case MutationType::GpuNvidiaSmi: {
            SecureProcess::execute("/usr/bin/nvidia-smi", {"-rgc"});
            SecureProcess::execute("/usr/bin/nvidia-smi", {"-pm", "0"});
            return true;
        }
        case MutationType::CgroupSlice: {
            fs::path slice = entry.target_path;
            if (fs::exists(slice / "cgroup.procs")) {
                auto procs = read_file_string(slice / "cgroup.procs");
                if (procs) {
                    auto pids = split(*procs, '\n');
                    auto root_procs = slice.parent_path() / "cgroup.procs";
                    for (const auto& p : pids) {
                        if (!p.empty()) write_file_string(root_procs, p);
                    }
                }
            }
            std::error_code ec;
            fs::remove(slice, ec);
            return true;
        }
        case MutationType::PipewireQuantum: {
            // Revert quantum to dynamic default 0
            SecureProcess::execute("/usr/bin/pw-metadata", {"-n", "settings", "0", "clock.force-quantum", "0"});
            return true;
        }
        default:
            return true;
    }
}

RollbackReport StateJournal::recover_all(std::function<bool(const JournalEntry&)> custom_handler) {
    RollbackReport report;
    report.total_entries = entries_.size();

    // Iterate backwards through mutations to safely unwind state in reverse order of application
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        auto& entry = *it;
        if (entry.state == EntryState::Pending || entry.state == EntryState::Applied) {
            bool ok = false;
            if (custom_handler) {
                ok = custom_handler(entry);
            } else {
                ok = execute_entry_revert(entry);
            }

            if (ok) {
                entry.state = EntryState::Reverted;
                report.successfully_reverted++;
                ZENITH_LOG_INFO("Recovered entry #" + std::to_string(entry.id) + " (" +
                                mutation_type_to_string(entry.type) + " -> " + entry.target_path + ")");
            } else {
                entry.state = EntryState::Failed;
                report.failed_reverts++;
                report.error_messages.push_back("Failed to revert entry #" + std::to_string(entry.id) +
                                                ": " + entry.target_path);
                ZENITH_LOG_ERROR("Rollback failed for entry #" + std::to_string(entry.id));
            }
        } else {
            report.skipped_entries++;
        }
    }

    atomic_save();
    return report;
}

} // namespace zenith
