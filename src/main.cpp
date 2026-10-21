// SPDX-License-Identifier: MIT
#include "zenith/engine.hpp"
#include "zenith/tweaks/tweak_registry.hpp"
#include "zenith/conflict_detector.hpp"
#include "zenith/per_game_profile.hpp"
#include <iostream>
#include "zenith/version.hpp"

namespace fs = std::filesystem;

void print_help() {
    std::cout <<
"  ______           _ _   _     \n"
" |___  /          (_) | | |    \n"
"    / / ___ _ __   _| |_| |__  \n"
"   / / / _ \\ '_ \\ | | __| '_ \\ \n"
"  / /_|  __/ | | || | |_| | | |\n"
" /_____\\___|_| |_||_|\\__|_| |_| Gaming Daemon (v" << zenith::ZENITH_VERSION << ")\n\n"
<< "Ultra-low latency Linux gaming optimizer daemon for Arch / CachyOS & SteamOS\n\n"
"Usage:\n"
"  zenith daemon                     Run as a background daemon (systemd service)\n"
"  zenith run <command> [args...]    Launch a game under Zenith wrapper (Steam %command%)\n"
"  zenith tune                       Manually apply gaming optimizations immediately\n"
"  zenith revert                     Manually restore original system settings\n"
"  zenith status                     Display CPU topology, cgroups, and current mode\n"
"  zenith --rollback                 Emergency crash recovery from state journal\n"
"  zenith --reset-safemode           Reset crash history and exit safe mode\n"
"  zenith --dry-run [profile]        Simulate tweak application with colorized diff without mutations\n"
"  zenith --explain                  Explain all kernel and subsystem tweaks in plain language\n"
"  zenith --conflicts                Detect competing services (gamemode, ananicy, tuned, etc.)\n"
"  zenith --help, -h                 Show this help menu\n\n"
"Examples:\n"
"  zenith daemon\n"
"  zenith run steam -applaunch 730\n"
"  zenith --rollback\n";
}

int main(int argc, char* argv[]) {
    zenith::Logger::init(zenith::LogLevel::Info);

    if (argc < 2) {
        print_help();
        return 1;
    }

    std::string action = argv[1];

    if (action == "--help" || action == "-h" || action == "help") {
        print_help();
        return 0;
    }

    if (action == "--rollback") {
        return zenith::ZenithEngine::perform_emergency_rollback() ? 0 : 1;
    }

    if (action == "--reset-safemode") {
        return zenith::ZenithEngine::reset_safe_mode() ? 0 : 1;
    }

    if (action == "--explain") {
        zenith::TweakRegistry::instance().register_defaults();
        zenith::TweakRegistry::instance().print_explain();
        return 0;
    }

    if (action == "--conflicts") {
        auto conflicts = zenith::ConflictDetector::probe_system_conflicts();
        zenith::ConflictDetector::print_conflict_report(conflicts);
        return 0;
    }

    if (action == "--dry-run") {
        zenith::Profile prof = zenith::Profile::Balanced;
        if (argc >= 3) {
            auto parsed = zenith::string_to_profile(argv[2]);
            if (parsed) prof = *parsed;
        }
        zenith::TweakRegistry::instance().register_defaults();
        zenith::TweakRegistry::instance().print_dry_run(prof);
        return 0;
    }

    auto config = zenith::Config::default_config();
    fs::path config_path = "/etc/zenith/zenith.conf";
    if (fs::exists(config_path)) {
        config = zenith::Config::load_from_file(config_path);
    }

    zenith::ZenithEngine engine(config);

    if (action == "daemon") {
        engine.run_daemon();
    } else if (action == "run") {
        if (argc < 3) {
            std::cerr << "Error: 'run' requires a command to execute.\n";
            std::cerr << "Usage: zenith run <executable> [args...]\n";
            return 1;
        }
        std::vector<std::string> cmd_args;
        for (int i = 2; i < argc; ++i) {
            cmd_args.emplace_back(argv[i]);
        }
        return engine.run_wrapped_command(cmd_args) ? 0 : 1;
    } else if (action == "tune") {
        if (!engine.initialize()) return 1;
        zenith::DetectedGame manual_game;
        manual_game.pid = 1;
        manual_game.name = "Manual Gaming Session";
        engine.activate(manual_game);
        std::cout << "Zenith optimizations applied. Run 'zenith revert' to restore.\n";
    } else if (action == "revert") {
        if (!engine.initialize()) return 1;
        engine.deactivate();
        std::cout << "System settings restored to default.\n";
    } else if (action == "status") {
        if (!engine.initialize()) return 1;
        engine.print_status();
    } else {
        std::cerr << "Unknown command: " << action << "\n";
        print_help();
        return 1;
    }

    return 0;
}
