// SPDX-License-Identifier: MIT
// src/zenith-run/main_run.cpp
// Steam Launch Options Wrapper (zenith-run %command%)
// Supports gamescope, Proton/Wine, D-Bus desktop notifications, and transparent exit code forwarding.

#include "zenith/config.hpp"
#include "zenith/engine.hpp"
#include "zenith/socket_client.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#if defined(__linux__)
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

void send_desktop_notification(const std::string& summary, const std::string& body) {
#if defined(__linux__)
    pid_t pid = fork();
    if (pid == 0) {
        // Child: send notification without blocking game launch
        execlp("notify-send", "notify-send", "-a", "Zenith", "-i", "input-gaming", summary.c_str(), body.c_str(), nullptr);
        std::exit(0);
    } else if (pid > 0) {
        waitpid(pid, nullptr, 0);
    }
#else
    (void)summary;
    (void)body;
#endif
}

std::string get_game_display_name(const std::vector<std::string>& cmd_args) {
    if (cmd_args.empty()) return "Unknown Game";

    const char* appid = std::getenv("SteamAppId");
    if (!appid) appid = std::getenv("SteamGameId");

    std::string bin = cmd_args[0];
    size_t last_slash = bin.find_last_of('/');
    if (last_slash != std::string::npos) {
        bin = bin.substr(last_slash + 1);
    }

    if (appid && *appid) {
        return bin + " (Steam AppID " + std::string(appid) + ")";
    }
    return bin;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: zenith-run <command> [args...]\n"
                  << "Example (Steam Launch Options): zenith-run %command%\n"
                  << "Example (with gamescope):       zenith-run gamescope -W 2560 -H 1440 -r 144 -- %command%\n";
        return 1;
    }

    std::vector<std::string> cmd_args;
    cmd_args.reserve(argc - 1);
    for (int i = 1; i < argc; ++i) {
        cmd_args.emplace_back(argv[i]);
    }

    std::string game_name = get_game_display_name(cmd_args);
    send_desktop_notification("⚡ Zenith Gaming Daemon", "Optimizations engaged for " + game_name);

    // Check if background daemon is active
    zenith::SocketClient client;
    bool daemon_running = client.is_daemon_reachable();

    if (daemon_running) {
        // Daemon is active, it will auto-detect process via netlink proc connector or gamescope
        // Forward execution to child
#if defined(__linux__)
        std::vector<char*> exec_args;
        for (const auto& arg : cmd_args) {
            exec_args.push_back(const_cast<char*>(arg.c_str()));
        }
        exec_args.push_back(nullptr);

        pid_t child = fork();
        if (child == 0) {
            execvp(exec_args[0], exec_args.data());
            std::perror("zenith-run: execvp failed");
            std::exit(127);
        }

        int status = 0;
        waitpid(child, &status, 0);

        send_desktop_notification("⚡ Zenith Gaming Daemon", "Game exited. System restored to baseline.");
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
        return 1;
#else
        return 0;
#endif
    } else {
        // Standalone wrapper mode: apply optimizations locally via engine
        auto cfg = zenith::Config::default_config();
        zenith::ZenithEngine engine(cfg);
        bool ok = engine.run_wrapped_command(cmd_args);
        send_desktop_notification("⚡ Zenith Gaming Daemon", "Game exited. Standalone session restored.");
        return ok ? 0 : 1;
    }
}
