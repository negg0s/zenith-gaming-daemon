// SPDX-License-Identifier: MIT
// src/zenith-run/main_run.cpp
// Steam Launch Options Wrapper (zenith-run %command%)
// Supports --ultra, --profile <name>, gamescope, Proton/Wine, D-Bus desktop notifications, and transparent exit code forwarding.

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
        // Double-fork so notification never blocks game launch
        if (fork() == 0) {
            execlp("notify-send", "notify-send", "-a", "Zenith", "-i", "input-gaming", summary.c_str(), body.c_str(), nullptr);
            std::exit(0);
        }
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

void apply_graphics_driver_env() {
#if defined(__linux__)
    setenv("MESA_SHADER_CACHE_MAX_SIZE", "10G", 0);
    setenv("RADV_PERFTEST", "aco,sam,dcc", 0);
    setenv("AMD_VULKAN_ICD", "RADV", 0);
    setenv("mesa_glthread", "true", 0);
    setenv("VKD3D_CONFIG", "dxr11,dxr", 0);
    setenv("__GL_THREADED_OPTIMIZATIONS", "1", 0);
    setenv("__GL_SYNC_TO_VBLANK", "0", 0);
#endif
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: zenith-run [--ultra | -u] [--profile <name>] <command> [args...]\n"
                  << "Example (Steam Launch Options): zenith-run %command%\n"
                  << "Example (Ultra zero-contention): zenith-run --ultra %command%\n"
                  << "Example (with gamescope):       zenith-run gamescope -W 2560 -H 1440 -r 144 -- %command%\n";
        return 1;
    }

    std::string requested_profile;
    int arg_start = 1;
    while (arg_start < argc) {
        std::string current_arg = argv[arg_start];
        if (current_arg == "--ultra" || current_arg == "-u") {
            requested_profile = "ultra";
            arg_start++;
        } else if (current_arg == "--profile" || current_arg == "-p") {
            if (arg_start + 1 < argc) {
                requested_profile = argv[arg_start + 1];
                arg_start += 2;
            } else {
                break;
            }
        } else if (current_arg == "--help" || current_arg == "-h") {
            std::cout << "Usage: zenith-run [--ultra | -u] [--profile <name>] <command> [args...]\n"
                      << "Options:\n"
                      << "  --ultra, -u           Launch with Ultra zero-contention profile (cgroup freeze, max clocks)\n"
                      << "  --profile, -p <name>  Specify active optimization profile (safe, balanced, aggressive, ultra)\n"
                      << "  --help, -h            Show this help message\n";
            return 0;
        } else {
            break;
        }
    }

    if (arg_start >= argc) {
        std::cerr << "Error: No command specified to run.\n";
        return 1;
    }

    std::vector<std::string> cmd_args;
    cmd_args.reserve(argc - arg_start);
    for (int i = arg_start; i < argc; ++i) {
        cmd_args.emplace_back(argv[i]);
    }

    std::string game_name = get_game_display_name(cmd_args);
    std::string profile_notice = requested_profile.empty() ? "" : " [" + requested_profile + "]";
    send_desktop_notification("⚡ Zenith Gaming Daemon", "Optimizations engaged for " + game_name + profile_notice);

    apply_graphics_driver_env();

    // Check if background daemon is active and set requested profile
    zenith::SocketClient client;
    bool daemon_running = client.is_daemon_reachable();

    if (daemon_running && !requested_profile.empty()) {
        client.send_command("SET_PROFILE " + requested_profile);
    }

    // Forward execution to child process
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

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 1;
#else
    return 0;
#endif
}
