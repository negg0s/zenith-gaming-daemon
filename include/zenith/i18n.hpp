// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace zenith::i18n {

enum class Language {
    English,
    Russian
};

// Initializes language from environment variables (LC_ALL, LC_MESSAGES, LANG)
void init();

// Explicitly set language
void set_language(Language lang);
Language get_language();

// Translate a key
std::string tr(std::string_view key);

// Formats an error with error code and documentation link
// e.g. format_error("E101", "Socket connection failed") -> "[ZENITH-E101] Socket connection failed (docs/ERRORS.md#E101)"
std::string format_error(std::string_view code, std::string_view message);

} // namespace zenith::i18n
