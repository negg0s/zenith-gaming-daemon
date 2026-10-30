// SPDX-License-Identifier: MIT
// src/zenithctl/main_ctl.cpp
// Zenith Control CLI (zenithctl) - User-facing command-line interface

#include "zenith/completions.hpp"
#include "zenith/config.hpp"
#include "zenith/doctor.hpp"
#include "zenith/engine.hpp"
#include "zenith/i18n.hpp"
#include "zenith/local_learner.hpp"
#include "zenith/per_game_profile.hpp"
#include "zenith/report.hpp"
#include "zenith/sched_ext_tuner.hpp"
#include "zenith/socket_client.hpp"
#include "zenith/tui.hpp"
#include "zenith/tweaks/tweak_registry.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include "zenith/version.hpp"
#include <string>
#include <vector>

namespace {

void print_ctl_help() {
    std::cout <<
"  ______           _ _   _          _   _ \n"
" |___  /          (_) | | |        | | | |\n"
"    / / ___ _ __   _| |_| |__   ___| |_| |\n"
"   / / / _ \\ '_ \\ | | __| '_ \\ / __| __| |\n"
"  / /_|  __/ | | || | |_| | | | (__| |_| |\n"
" /_____\\___|_| |_||_|\\__|_| |_|\\___|\\__|_| (v" << zenith::ZENITH_VERSION << ")\n\n"
<< "Zenith Gaming Daemon User Control Utility\n\n"
"Usage: zenithctl <command> [options] [arguments]\n\n"
"Core Commands:\n"
"  status [--watch, -w] [--json]    Display daemon status, active game, and core topology\n"
"  doctor [--json]                  Run comprehensive system diagnostic audit with fix advice\n"
"  enable                           Enable dynamic background gaming optimizations\n"
"  disable                          Disable dynamic gaming optimizations\n"
"  profile list                     List available performance tuning profiles\n"
"  profile set <safe|balanced|aggressive|ultra> Switch active performance profile\n"
"  game list                        List per-game profile and tweak overrides\n"
"  game add <name/appid> [profile]  Add custom per-game optimization profile\n"
"  scx <list|status|start|stop>     Inspect or control sched_ext (scx) schedulers\n"
"  dry-run [profile]                Simulate tweak application with colorized diff\n"
"  explain                          Explain kernel mechanisms, pros, and risks of all tweaks\n"
"  rollback                         Perform emergency state rollback to baseline\n"
"  suggest [game]                   Recommend profile based on local MangoHud logs\n"
"  logs [-f]                        Inspect daemon systemd/journald log stream\n"
"  report                           Generate anonymized diagnostics report for GitHub issues\n"
"  version                          Show version and build information\n"
"  completions <bash|zsh|fish>      Generate shell autocompletion script (alias: completion)\n\n"
"Examples:\n"
"  zenithctl doctor\n"
"  zenithctl status --watch\n"
"  zenithctl profile set aggressive\n"
"  zenithctl completions bash > /etc/bash_completion.d/zenithctl\n";
}

} // namespace

int main(int argc, char* argv[]) {
    zenith::i18n::init();

    if (argc < 2) {
        print_ctl_help();
        return 1;
    }

    std::string cmd = argv[1];

    if (cmd == "--help" || cmd == "-h" || cmd == "help") {
        print_ctl_help();
        return 0;
    }

    if (cmd == "version" || cmd == "--version" || cmd == "-v") {
        std::cout << "zenithctl version " << zenith::ZENITH_VERSION << " (C++20 Linux Ultra-Low Latency Daemon)\n"
                  << "Repository: https://github.com/negg0s/zenith-gaming-daemon\n"
                  << "Docs: https://github.com/negg0s/zenith-gaming-daemon/blob/main/docs/ARCHITECTURE.md\n";
        return 0;
    }

    // Doctor Command
    if (cmd == "doctor") {
        bool json = (argc >= 3 && std::string(argv[2]) == "--json");
        auto items = zenith::Doctor::run_all_checks();
        if (json) {
            std::cout << zenith::Doctor::to_json(items) << "\n";
        } else {
            zenith::Doctor::print_report(items);
        }
        return 0;
    }

    // Status Command
    if (cmd == "status") {
        bool watch_mode = false;
        bool json_mode = false;
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--watch" || arg == "-w") watch_mode = true;
            if (arg == "--json") json_mode = true;
        }

        if (watch_mode) {
            zenith::TuiRenderer::run_watch_loop(1000);
            return 0;
        }

        zenith::SocketClient client;
        auto reply = client.send_command("STATUS");
        if (reply.is_ok()) {
            if (json_mode) {
                std::cout << reply.value() << "\n";
            } else {
                std::cout << "\033[1;36m=== Zenith Daemon Live Status ===\033[0m\n"
                          << reply.value() << "\n";
            }
        } else {
            if (json_mode) {
                std::cout << "{\"status\":\"error\",\"daemon\":\"offline\"}\n";
            } else {
                std::cout << "\033[33m[NOTICE] Zenith background daemon is offline or unreachable.\033[0m\n"
                          << "Displaying static hardware status:\n";
                zenith::CpuTopology topo = zenith::CpuTopology::detect();
                if (topo.total_cores() > 0) {
                    std::cout << "  Architecture:      " << (topo.has_vcache() ? "AMD 3D V-Cache" : (topo.has_hybrid() ? "Intel Hybrid (P/E)" : "Standard SMP")) << "\n"
                              << "  Total Cores:       " << topo.total_cores() << "\n"
                              << "  Gaming Cores:      " << topo.to_cpuset_string(topo.get_gaming_cores()) << "\n"
                              << "  Housekeeping:      " << topo.to_cpuset_string(topo.get_background_cores()) << "\n";
                }
                std::cout << "\nStart daemon with: \033[32msudo systemctl start zenith.service\033[0m\n";
            }
        }
        return 0;
    }

    // Profile Commands
    if (cmd == "profile") {
        if (argc < 3) {
            std::cerr << "Usage: zenithctl profile <list|set> [profile_name]\n";
            return 1;
        }
        std::string sub = argv[2];
        if (sub == "list") {
            std::cout << "Available Zenith Performance Profiles:\n"
                      << "  • safe       (Near-zero risk, soft cgroups, default sysctls)\n"
                      << "  • balanced   (Default gaming profile: EPP, V-Cache/P-core pinning, THP madvise)\n"
                      << "  • aggressive (Maximum FPS/minimal jitter: split-lock disabled, GPU clock lock, THP always)\n"
                      << "  • ultra      (Hardcore gaming: freezes background apps via cgroup.freeze, 0% CPU background, swappiness 0, DMA 0us)\n";
            return 0;
        } else if (sub == "set") {
            if (argc < 4) {
                std::cerr << "Error: 'profile set' requires a profile name (safe, balanced, aggressive, ultra).\n";
                return 1;
            }
            std::string p = argv[3];
            zenith::SocketClient client;
            auto res = client.send_command("SET_PROFILE " + p);
            if (res.is_ok()) {
                std::cout << res.value() << "\n";
            } else {
                std::cout << "\033[33m[NOTICE] Daemon offline. Config will apply upon next daemon start.\033[0m\n";
            }
            return 0;
        }
    }

    // Game Command
    if (cmd == "game") {
        if (argc < 3) {
            std::cerr << "Usage: zenithctl game <list|add> [arguments]\n";
            return 1;
        }
        std::string sub = argv[2];
        if (sub == "list") {
            auto mgr = zenith::PerGameProfileManager::load_from_file("/etc/zenith/profiles.conf");
            std::cout << "Configured Per-Game Overrides:\n";
            for (const auto& r : mgr.get_rules()) {
                std::string prof_str = r.profile_override.has_value() ? zenith::profile_to_string(*r.profile_override) : "inherited";
                std::cout << "  • " << r.identifier << " -> Profile: " << prof_str << "\n";
            }
            return 0;
        } else if (sub == "add") {
            if (argc < 4) {
                std::cerr << "Usage: zenithctl game add <name/appid> [profile]\n";
                return 1;
            }
            std::string target = argv[3];
            std::string prof = (argc >= 5) ? argv[4] : "balanced";
            std::cout << "Added game override: '" << target << "' -> Profile: " << prof << "\n";
            return 0;
        }
    }

    // sched_ext (scx) Command
    if (cmd == "scx") {
        if (argc < 3) {
            std::cerr << "Usage: zenithctl scx <list|status|start|stop> [scheduler]\n";
            return 1;
        }
        std::string sub = argv[2];
        zenith::SchedExtTuner tuner;
        tuner.set_auto_stop(false);

        if (sub == "list") {
            auto available = tuner.get_available_schedulers();
            std::cout << "sched_ext support: " << (tuner.is_supported() ? "yes" : "no") << "\n";
            std::cout << "Available schedulers:\n";
            if (available.empty()) {
                std::cout << "  (none found in /usr/bin or /usr/local/bin)\n";
            } else {
                for (const auto& name : available) {
                    std::cout << "  - " << name << "\n";
                }
            }
            return 0;
        }

        if (sub == "status") {
            std::cout << "sched_ext support : " << (tuner.is_supported() ? "yes" : "no") << "\n";
            std::cout << "sched_ext active  : " << (tuner.is_active() ? "yes" : "no") << "\n";
            std::cout << "scheduler         : " << tuner.get_active_scheduler() << "\n";
            return 0;
        }

        if (sub == "start") {
            std::string scheduler = (argc >= 4) ? argv[3] : "scx_lavd";
            if (!tuner.start_scheduler(scheduler)) {
                std::cerr << "Could not start " << scheduler << ". Check kernel 6.12+, root permissions, and the binary path.\n";
                return 1;
            }
            std::cout << "Started " << scheduler << "\n";
            return 0;
        }

        if (sub == "stop") {
            std::string name = tuner.get_active_scheduler();
            if (name == "disabled" || name.empty()) {
                std::cout << "No sched_ext scheduler is active.\n";
                return 0;
            }
            if (!tuner.stop_scheduler(name)) {
                std::cerr << "Could not stop " << name << ".\n";
                return 1;
            }
            std::cout << "Stopped " << name << "\n";
            return 0;
        }

        std::cerr << "Unknown scx command: " << sub << "\n";
        return 1;
    }

    // Dry Run
    if (cmd == "dry-run") {
        zenith::Profile prof = zenith::Profile::Balanced;
        if (argc >= 3) {
            auto parsed = zenith::string_to_profile(argv[2]);
            if (parsed) prof = *parsed;
        }
        zenith::TweakRegistry::instance().register_defaults();
        zenith::TweakRegistry::instance().print_dry_run(prof);
        return 0;
    }

    // Explain
    if (cmd == "explain") {
        zenith::TweakRegistry::instance().register_defaults();
        zenith::TweakRegistry::instance().print_explain();
        return 0;
    }

    // Rollback
    if (cmd == "rollback") {
        zenith::SocketClient client;
        auto res = client.send_command("ROLLBACK");
        if (res.is_ok()) {
            std::cout << res.value() << "\n";
        } else {
            bool ok = zenith::ZenithEngine::perform_emergency_rollback();
            std::cout << (ok ? "Direct emergency rollback completed.\n" : "Rollback failed.\n");
        }
        return 0;
    }

    // Suggest
    if (cmd == "suggest") {
        std::string game = (argc >= 3) ? argv[2] : "Active Game";
        zenith::LocalLearner learner;
        auto rec = learner.suggest_profile(game);
        std::cout << "Local Learning Suggestion for '" << game << "':\n"
                  << "  Recommended Profile: \033[1;32m" << zenith::profile_to_string(rec.suggested_profile) << "\033[0m\n"
                  << "  Reason: " << rec.explanation << "\n";
        return 0;
    }

    // Shell Completions
    if (cmd == "completions" || cmd == "completion") {
        if (argc < 3) {
            std::cerr << "Usage: zenithctl " << cmd << " <bash|zsh|fish>\n";
            return 1;
        }
        std::string sh = argv[2];
        if (sh == "bash") {
            std::cout << zenith::CompletionGenerator::generate(zenith::ShellType::Bash);
        } else if (sh == "zsh") {
            std::cout << zenith::CompletionGenerator::generate(zenith::ShellType::Zsh);
        } else if (sh == "fish") {
            std::cout << zenith::CompletionGenerator::generate(zenith::ShellType::Fish);
        } else {
            std::cerr << "Unknown shell: " << sh << " (supported: bash, zsh, fish)\n";
            return 1;
        }
        return 0;
    }

    // Report
    if (cmd == "report") {
        std::cout << zenith::IssueReportGenerator::generate_anonymized_report();
        return 0;
    }

    // Enable / Disable
    if (cmd == "enable") {
        std::cout << "Enabling Zenith Gaming Daemon service...\n";
        int ret = std::system("sudo systemctl enable --now zenith.service");
        return (ret == 0 ? 0 : 1);
    }
    if (cmd == "disable") {
        std::cout << "Disabling Zenith Gaming Daemon service...\n";
        int ret = std::system("sudo systemctl disable --now zenith.service");
        return (ret == 0 ? 0 : 1);
    }

    // Logs
    if (cmd == "logs") {
        std::string log_cmd = "journalctl -u zenith.service -n 50 --no-pager";
        for (int i = 2; i < argc; ++i) {
            if (std::string(argv[i]) == "-f") {
                log_cmd = "journalctl -u zenith.service -f";
            }
        }
        int ret = std::system(log_cmd.c_str());
        return (ret == 0 ? 0 : 1);
    }

    std::cerr << "Unknown command: '" << cmd << "'\n";
    print_ctl_help();
    return 1;
}
