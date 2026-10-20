#include "zenith/completions.hpp"

namespace zenith {

namespace {

const char* bash_completion = R"(# Bash completion for zenithctl
_zenithctl() {
    local cur prev words cword
    _init_completion || return

    local commands="status enable disable profile game dry-run explain rollback doctor suggest logs version completions report"
    local profiles="safe balanced aggressive"

    case "${prev}" in
        zenithctl)
            COMPREPLY=( $(compgen -W "${commands}" -- "${cur}") )
            return 0
            ;;
        profile)
            COMPREPLY=( $(compgen -W "list set" -- "${cur}") )
            return 0
            ;;
        set)
            COMPREPLY=( $(compgen -W "${profiles}" -- "${cur}") )
            return 0
            ;;
        game)
            COMPREPLY=( $(compgen -W "list add" -- "${cur}") )
            return 0
            ;;
        completions)
            COMPREPLY=( $(compgen -W "bash zsh fish" -- "${cur}") )
            return 0
            ;;
        status)
            COMPREPLY=( $(compgen -W "--watch -w --json" -- "${cur}") )
            return 0
            ;;
        doctor)
            COMPREPLY=( $(compgen -W "--json" -- "${cur}") )
            return 0
            ;;
    esac
}
complete -F _zenithctl zenithctl
)";

const char* zsh_completion = R"(#compdef zenithctl

_zenithctl() {
    local -a commands
    commands=(
        'status:Display current daemon and gaming state'
        'enable:Enable dynamic gaming optimizations'
        'disable:Disable dynamic optimizations'
        'profile:Inspect or set active performance profile'
        'game:Manage per-game configurations'
        'dry-run:Simulate tweak application without modifying system'
        'explain:Show plain-language explanations of all tweaks'
        'rollback:Emergency recovery of system mutations'
        'doctor:Run complete diagnostic audit with repair advice'
        'suggest:MangoHud frametime-based profile recommender'
        'logs:View active daemon journald log stream'
        'version:Show version and build information'
        'completions:Generate shell autocompletion script'
        'report:Generate anonymized diagnostic report for GitHub issues'
    )

    _arguments -C \
        '1: :->command' \
        '*:: :->args'

    case $state in
        command)
            _describe -t commands 'zenithctl command' commands
            ;;
        args)
            case $words[1] in
                profile)
                    _values 'action' 'list[List available profiles]' 'set[Switch active profile]'
                    ;;
                completions)
                    _values 'shell' 'bash' 'zsh' 'fish'
                    ;;
                status)
                    _arguments \
                        '(-w --watch)'{-w,--watch}'[Live dashboard watch mode]' \
                        '--json[Output raw JSON]'
                    ;;
            esac
            ;;
    esac
}

_zenithctl "$@"
)";

const char* fish_completion = R"(# Fish completion for zenithctl
complete -c zenithctl -f

# Main commands
complete -c zenithctl -n "__fish_use_subcommand" -a status -d "Display current daemon state"
complete -c zenithctl -n "__fish_use_subcommand" -a enable -d "Enable dynamic gaming optimizations"
complete -c zenithctl -n "__fish_use_subcommand" -a disable -d "Disable dynamic optimizations"
complete -c zenithctl -n "__fish_use_subcommand" -a profile -d "Inspect or set active performance profile"
complete -c zenithctl -n "__fish_use_subcommand" -a game -d "Manage per-game configurations"
complete -c zenithctl -n "__fish_use_subcommand" -a dry-run -d "Simulate tweak application with diff"
complete -c zenithctl -n "__fish_use_subcommand" -a explain -d "Show plain-language explanations of tweaks"
complete -c zenithctl -n "__fish_use_subcommand" -a rollback -d "Emergency recovery of system mutations"
complete -c zenithctl -n "__fish_use_subcommand" -a doctor -d "Run diagnostic audit with fix advice"
complete -c zenithctl -n "__fish_use_subcommand" -a suggest -d "Recommend profile from MangoHud logs"
complete -c zenithctl -n "__fish_use_subcommand" -a logs -d "View daemon log stream"
complete -c zenithctl -n "__fish_use_subcommand" -a version -d "Show version and build information"
complete -c zenithctl -n "__fish_use_subcommand" -a completions -d "Generate shell completion script"
complete -c zenithctl -n "__fish_use_subcommand" -a report -d "Generate anonymized issue report"

# Subcommands
complete -c zenithctl -n "__fish_seen_subcommand_from profile" -a "list set"
complete -c zenithctl -n "__fish_seen_subcommand_from profile; and __fish_seen_subcommand_from set" -a "safe balanced aggressive"
complete -c zenithctl -n "__fish_seen_subcommand_from completions" -a "bash zsh fish"
complete -c zenithctl -n "__fish_seen_subcommand_from status" -s w -l watch -d "Live watch dashboard"
complete -c zenithctl -n "__fish_seen_subcommand_from status" -l json -d "Output raw JSON"
complete -c zenithctl -n "__fish_seen_subcommand_from doctor" -l json -d "Output raw JSON"
)";

} // namespace

std::string CompletionGenerator::generate(ShellType shell) {
    switch (shell) {
        case ShellType::Bash: return bash_completion;
        case ShellType::Zsh:  return zsh_completion;
        case ShellType::Fish: return fish_completion;
    }
    return "";
}

} // namespace zenith
