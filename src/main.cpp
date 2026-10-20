#include "zenith/engine.hpp"
#include <iostream>

void print_help() {
    std::cout << R"(
  ______           _ _   _     
 |___  /          (_) | | |    
    / / ___ _ __   _| |_| |__  
   / / / _ \ '_ \ | | __| '_ \ 
  / /_|  __/ | | || | |_| | | |
 /_____\___|_| |_||_|\__|_| |_| Gaming Daemon (v1.0.0)

Ultra-low latency Linux gaming optimizer daemon for Arch / CachyOS & SteamOS

Usage:
  zenith daemon                     Run as a background daemon (systemd service)
  zenith run <command> [args...]    Launch a game under Zenith wrapper (Steam %command%)
  zenith tune                       Manually apply gaming optimizations immediately
  zenith revert                     Manually restore original system settings
  zenith status                     Display CPU topology, cgroups, and current mode
  zenith --help, -h                 Show this help menu

Examples:
  zenith daemon
  zenith run steam -applaunch 730
  zenith run ./my_game
)";
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
        engine.initialize();
        zenith::DetectedGame manual_game;
        manual_game.pid = 1;
        manual_game.name = "Manual Gaming Session";
        engine.activate(manual_game);
        std::cout << "Zenith optimizations applied. Run 'zenith revert' to restore.\n";
    } else if (action == "revert") {
        engine.initialize();
        engine.deactivate();
        std::cout << "System settings restored to default.\n";
    } else if (action == "status") {
        engine.initialize();
        engine.print_status();
    } else {
        std::cerr << "Unknown command: " << action << "\n";
        print_help();
        return 1;
    }

    return 0;
}
