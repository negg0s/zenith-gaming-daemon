#pragma once

#include <string>

namespace zenith {

enum class ShellType {
    Bash,
    Zsh,
    Fish
};

class CompletionGenerator {
public:
    static std::string generate(ShellType shell);
};

} // namespace zenith
