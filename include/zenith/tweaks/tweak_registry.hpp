#pragma once

#include "zenith/tweaks/tweak_interface.hpp"
#include <memory>
#include <vector>
#include <string>
#include <unordered_set>

namespace zenith {

struct DryRunDiffEntry {
    std::string tweak_name;
    std::string current_value;
    std::string target_value;
    bool would_modify = false;
    std::string description;
};

class TweakRegistry {
public:
    static TweakRegistry& instance();

    void register_tweak(std::shared_ptr<ITweak> tweak);
    void register_defaults();

    const std::vector<std::shared_ptr<ITweak>>& get_all_tweaks() const {
        return tweaks_;
    }

    // Applies all active tweaks for the given profile, respecting blacklist
    bool apply_all(Profile profile, const std::unordered_set<std::string>& blacklist = {});

    // Reverts all applied tweaks
    bool revert_all();

    // Dry-run inspection
    std::vector<DryRunDiffEntry> compute_dry_run(Profile profile, const std::unordered_set<std::string>& blacklist = {}) const;
    void print_dry_run(Profile profile, const std::unordered_set<std::string>& blacklist = {}) const;

    // Plain-language explanations
    void print_explain() const;

private:
    std::vector<std::shared_ptr<ITweak>> tweaks_;
    std::vector<std::shared_ptr<ITweak>> applied_tweaks_;
};

// Plugin-like auto-registration helper
template <typename TweakType>
class TweakRegistrar {
public:
    TweakRegistrar() {
        TweakRegistry::instance().register_tweak(std::make_shared<TweakType>());
    }
};

#define ZENITH_REGISTER_TWEAK(TweakClass) \
    namespace { \
        static const ::zenith::TweakRegistrar<TweakClass> _registrar_##TweakClass; \
    }

} // namespace zenith
