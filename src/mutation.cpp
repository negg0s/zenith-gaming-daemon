#include "zenith/mutation.hpp"

namespace zenith {

FileStringMutation::FileStringMutation(fs::path path, std::string target_value, MutationType type)
    : path_(std::move(path)),
      target_value_(std::move(target_value)),
      type_(type) {}

bool FileStringMutation::probe() {
    if (!fs::exists(path_)) return false;
    auto cur = read_file_string(path_);
    if (!cur) return false;
    original_value_ = *cur;
    return true;
}

bool FileStringMutation::apply(StateJournal* journal) {
    if (!probe()) return false;

    // 1. Write-Ahead Log: Record as Pending before modifying state
    if (journal) {
        journal_id_ = journal->record_pending(type_, path_.string(), original_value_, target_value_);
    }

    // 2. Perform system mutation
    if (!write_file_string(path_, target_value_)) {
        if (journal && journal_id_ > 0) {
            journal->mark_failed(journal_id_);
        }
        return false;
    }

    // 3. Mark Applied in WAL
    if (journal && journal_id_ > 0) {
        journal->mark_applied(journal_id_);
    }

    applied_ = true;
    ZENITH_LOG_DEBUG("Applied mutation: " + describe());
    return true;
}

bool FileStringMutation::revert(StateJournal* journal) {
    if (!applied_) return true;

    bool ok = write_file_string(path_, original_value_);
    if (ok) {
        applied_ = false;
        if (journal && journal_id_ > 0) {
            journal->mark_reverted(journal_id_);
        }
        ZENITH_LOG_DEBUG("Reverted mutation: " + describe());
    } else {
        ZENITH_LOG_ERROR("Failed to revert mutation: " + describe());
    }
    return ok;
}

std::string FileStringMutation::describe() const {
    return path_.string() + " = " + target_value_ + " [original: " + original_value_ + "]";
}

TokenReplaceMutation::TokenReplaceMutation(fs::path path, std::string target_token)
    : path_(std::move(path)),
      target_token_(std::move(target_token)) {}

std::string TokenReplaceMutation::extract_active_token(std::string_view line) {
    auto start = line.find('[');
    if (start == std::string_view::npos) return "";
    auto end = line.find(']', start);
    if (end == std::string_view::npos) return "";
    return std::string(line.substr(start + 1, end - start - 1));
}

bool TokenReplaceMutation::probe() {
    if (!fs::exists(path_)) return false;
    auto cur = read_file_string(path_);
    if (!cur) return false;
    original_token_ = extract_active_token(*cur);
    if (original_token_.empty()) {
        original_token_ = *cur;
    }
    return true;
}

bool TokenReplaceMutation::apply(StateJournal* journal) {
    if (!probe()) return false;

    if (journal) {
        journal_id_ = journal->record_pending(MutationType::SysfsToken, path_.string(), original_token_, target_token_);
    }

    if (!write_file_string(path_, target_token_)) {
        if (journal && journal_id_ > 0) {
            journal->mark_failed(journal_id_);
        }
        return false;
    }

    if (journal && journal_id_ > 0) {
        journal->mark_applied(journal_id_);
    }

    applied_ = true;
    ZENITH_LOG_DEBUG("Applied token mutation: " + describe());
    return true;
}

bool TokenReplaceMutation::revert(StateJournal* journal) {
    if (!applied_) return true;

    bool ok = write_file_string(path_, original_token_);
    if (ok) {
        applied_ = false;
        if (journal && journal_id_ > 0) {
            journal->mark_reverted(journal_id_);
        }
        ZENITH_LOG_DEBUG("Reverted token mutation: " + describe());
    }
    return ok;
}

std::string TokenReplaceMutation::describe() const {
    return path_.string() + " -> [" + target_token_ + "] [original: [" + original_token_ + "]]";
}

} // namespace zenith
