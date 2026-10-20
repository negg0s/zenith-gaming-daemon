#pragma once

#include "zenith/journal.hpp"

namespace zenith {

class IMutation {
public:
    virtual ~IMutation() = default;

    virtual bool probe() = 0;
    virtual bool apply(StateJournal* journal = nullptr) = 0;
    virtual bool revert(StateJournal* journal = nullptr) = 0;
    virtual std::string describe() const = 0;

    virtual MutationType get_type() const = 0;
    virtual std::string get_target_path() const = 0;
    virtual std::string get_original_value() const = 0;
    virtual std::string get_target_value() const = 0;

    bool is_applied() const { return applied_; }
    uint64_t get_journal_id() const { return journal_id_; }

protected:
    bool applied_ = false;
    uint64_t journal_id_ = 0;
};

class FileStringMutation : public IMutation {
public:
    FileStringMutation(fs::path path, std::string target_value, MutationType type = MutationType::SysctlString);

    bool probe() override;
    bool apply(StateJournal* journal = nullptr) override;
    bool revert(StateJournal* journal = nullptr) override;
    std::string describe() const override;

    MutationType get_type() const override { return type_; }
    std::string get_target_path() const override { return path_.string(); }
    std::string get_original_value() const override { return original_value_; }
    std::string get_target_value() const override { return target_value_; }

private:
    fs::path path_;
    std::string target_value_;
    std::string original_value_;
    MutationType type_;
};

class TokenReplaceMutation : public IMutation {
public:
    TokenReplaceMutation(fs::path path, std::string target_token);

    bool probe() override;
    bool apply(StateJournal* journal = nullptr) override;
    bool revert(StateJournal* journal = nullptr) override;
    std::string describe() const override;

    MutationType get_type() const override { return MutationType::SysfsToken; }
    std::string get_target_path() const override { return path_.string(); }
    std::string get_original_value() const override { return original_token_; }
    std::string get_target_value() const override { return target_token_; }

private:
    fs::path path_;
    std::string target_token_;
    std::string original_token_;

    static std::string extract_active_token(std::string_view line);
};

} // namespace zenith
