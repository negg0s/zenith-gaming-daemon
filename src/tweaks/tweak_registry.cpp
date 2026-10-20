#include "zenith/tweaks/tweak_registry.hpp"
#include "zenith/tweaks/cpu_governor_tweak.hpp"
#include "zenith/tweaks/cgroup_weight_tweak.hpp"
#include "zenith/tweaks/uclamp_tweak.hpp"
#include "zenith/tweaks/timer_slack_tweak.hpp"
#include "zenith/tweaks/nvme_tweak.hpp"
#include "zenith/tweaks/usb_autosuspend_tweak.hpp"
#include "zenith/tweaks/zram_swap_tweak.hpp"
#include "zenith/tweaks/sysctl_tweak.hpp"
#include <iostream>
#include <iomanip>

namespace zenith {

TweakRegistry& TweakRegistry::instance() {
    static TweakRegistry reg;
    return reg;
}

void TweakRegistry::register_tweak(std::shared_ptr<ITweak> tweak) {
    tweaks_.push_back(std::move(tweak));
}

void TweakRegistry::register_defaults() {
    tweaks_.clear();
    register_tweak(std::make_shared<CpuGovernorTweak>());
    register_tweak(std::make_shared<CgroupWeightTweak>());
    register_tweak(std::make_shared<UclampTweak>());
    register_tweak(std::make_shared<TimerSlackTweak>());
    register_tweak(std::make_shared<NvmeTweak>());
    register_tweak(std::make_shared<UsbAutosuspendTweak>());
    register_tweak(std::make_shared<ZramSwapTweak>());
    register_tweak(std::make_shared<SysctlTweak>());
}

bool TweakRegistry::apply_all(Profile profile, const std::unordered_set<std::string>& blacklist) {
    bool all_ok = true;
    for (const auto& tweak : tweaks_) {
        if (blacklist.find(tweak->name()) != blacklist.end()) {
            ZENITH_LOG_INFO("Tweak '" + tweak->name() + "' skipped (blacklisted).");
            continue;
        }

        if (tweak->is_supported_in(profile)) {
            if (tweak->probe()) {
                if (tweak->apply(profile)) {
                    applied_tweaks_.push_back(tweak);
                    ZENITH_LOG_DEBUG("Applied tweak: " + tweak->name());
                } else {
                    all_ok = false;
                    ZENITH_LOG_WARN("Failed to apply tweak: " + tweak->name());
                }
            }
        }
    }
    return all_ok;
}

bool TweakRegistry::revert_all() {
    bool all_ok = true;
    for (auto it = applied_tweaks_.rbegin(); it != applied_tweaks_.rend(); ++it) {
        if (!(*it)->revert()) {
            all_ok = false;
            ZENITH_LOG_WARN("Failed to revert tweak: " + (*it)->name());
        }
    }
    applied_tweaks_.clear();
    return all_ok;
}

std::vector<DryRunDiffEntry> TweakRegistry::compute_dry_run(Profile profile,
                                                           const std::unordered_set<std::string>& blacklist) const {
    std::vector<DryRunDiffEntry> diffs;
    for (const auto& tweak : tweaks_) {
        if (blacklist.find(tweak->name()) != blacklist.end()) continue;
        if (!tweak->is_supported_in(profile)) continue;

        DryRunDiffEntry entry;
        entry.tweak_name = tweak->name();
        entry.description = tweak->description();
        entry.current_value = tweak->get_current_value();
        entry.target_value = tweak->get_target_value(profile);
        entry.would_modify = (entry.current_value != entry.target_value);
        diffs.push_back(entry);
    }
    return diffs;
}

void TweakRegistry::print_dry_run(Profile profile, const std::unordered_set<std::string>& blacklist) const {
    std::cout << "\n\033[1;36m=========================================================================================\033[0m\n";
    std::cout << "\033[1;36m                     ZENITH GAMING DAEMON: DRY-RUN SIMULATION (" 
              << profile_to_string(profile) << ")\033[0m\n";
    std::cout << "\033[1;36m=========================================================================================\033[0m\n";

    auto diffs = compute_dry_run(profile, blacklist);
    for (const auto& d : diffs) {
        std::cout << "  * \033[1m" << std::left << std::setw(30) << d.tweak_name << "\033[0m: ";
        std::cout << "\033[31m[Current: " << d.current_value << "]\033[0m -> ";
        std::cout << "\033[32m[Target: " << d.target_value << "]\033[0m\n";
        std::cout << "    \033[90m(" << d.description << ")\033[0m\n\n";
    }
    std::cout << "\033[1;36m=========================================================================================\033[0m\n";
    std::cout << "Total candidate tweaks for " << profile_to_string(profile) << ": " << diffs.size() << " (Zero mutations applied)\n\n";
}

void TweakRegistry::print_explain() const {
    std::cout << "\n\033[1;32m=========================================================================================\033[0m\n";
    std::cout << "\033[1;32m                      ZENITH SUBSYSTEM TWEAKS EXPLANATION REGISTRY                       \033[0m\n";
    std::cout << "\033[1;32m=========================================================================================\033[0m\n\n";

    for (const auto& tweak : tweaks_) {
        std::cout << "\033[1m[" << tweak->name() << "]\033[0m - " << tweak->description() << "\n";
        std::cout << "  \033[33mSupported Profiles\033[0m: ";
        if (tweak->is_supported_in(Profile::Safe)) std::cout << "safe ";
        if (tweak->is_supported_in(Profile::Balanced)) std::cout << "balanced ";
        if (tweak->is_supported_in(Profile::Aggressive)) std::cout << "aggressive ";
        std::cout << "\n";

        std::cout << "  \033[36mMechanism & Details\033[0m: " << tweak->explain() << "\n";
        std::cout << "  \033[35mBenchmark Status\033[0m   : [Эффект не измерен]\n\n";
    }
    std::cout << "\033[1;32m=========================================================================================\033[0m\n\n";
}

} // namespace zenith
