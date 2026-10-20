// SPDX-License-Identifier: MIT
#pragma once

#include "zenith/common.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace zenith {

enum class Profile {
    Safe,
    Balanced,
    Aggressive
};

inline std::string profile_to_string(Profile p) {
    switch (p) {
        case Profile::Safe: return "safe";
        case Profile::Balanced: return "balanced";
        case Profile::Aggressive: return "aggressive";
    }
    return "balanced";
}

inline std::optional<Profile> string_to_profile(std::string_view s) {
    if (s == "safe" || s == "Safe") return Profile::Safe;
    if (s == "balanced" || s == "Balanced") return Profile::Balanced;
    if (s == "aggressive" || s == "Aggressive") return Profile::Aggressive;
    return std::nullopt;
}

namespace ProfileMask {
    constexpr uint32_t Safe = 1 << 0;
    constexpr uint32_t Balanced = 1 << 1;
    constexpr uint32_t Aggressive = 1 << 2;
    constexpr uint32_t All = Safe | Balanced | Aggressive;
    constexpr uint32_t Performance = Balanced | Aggressive;
}

inline uint32_t profile_to_mask(Profile p) {
    switch (p) {
        case Profile::Safe: return ProfileMask::Safe;
        case Profile::Balanced: return ProfileMask::Balanced;
        case Profile::Aggressive: return ProfileMask::Aggressive;
    }
    return ProfileMask::Balanced;
}

class ITweak {
public:
    virtual ~ITweak() = default;

    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
    virtual uint32_t supported_profiles() const = 0;

    bool is_supported_in(Profile p) const {
        return (supported_profiles() & profile_to_mask(p)) != 0;
    }

    virtual bool probe() = 0;
    virtual bool apply(Profile profile) = 0;
    virtual bool revert() = 0;
    virtual std::string describe() const = 0;
    virtual std::string explain() const = 0;

    virtual std::string get_current_value() const = 0;
    virtual std::string get_target_value(Profile profile) const = 0;
};

} // namespace zenith
