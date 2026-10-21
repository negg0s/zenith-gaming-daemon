// SPDX-License-Identifier: MIT
#include "zenith/background_manager.hpp"
#include <algorithm>
#include <cctype>
#include <csignal>

namespace zenith {

namespace {

std::string to_lower_str(std::string_view sv) {
    std::string s(sv);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

} // namespace

BackgroundManager::BackgroundManager(IProcFs* fs)
    : fs_(fs ? fs : &RealProcFs::instance()) {
    // Default system whitelist
    whitelist_rules_ = {
        // Compositors & Display servers
        "kwin", "kwin_wayland", "mutter", "sway", "hyprland", "weston", "wayfire", "xorg", "xwayland",
        // Audio servers & session managers
        "pipewire", "wireplumber", "pipewire-pulse", "pulseaudio", "jackd",
        // System & Security daemons
        "systemd", "systemd-journald", "systemd-udevd", "systemd-resolved", "systemd-logind",
        "dbus-daemon", "dbus-broker", "networkmanager", "iwd", "wpa_supplicant", "sshd", "polkitd", "seatd",
        // Shells & Agent terminals (Never throttle developer / user shells)
        "bash", "zsh", "fish", "pwsh", "tmux", "screen",
        "alacritty", "kitty", "foot", "konsole", "gnome-terminal", "wezterm", "ptyxis"
    };

    // Default background heavy apps blacklist (browsers, discord, torrents, sync daemons, compilers)
    blacklist_rules_ = {
        // Web browsers
        "chrome", "firefox", "chromium", "brave", "edge", "opera", "vivaldi",
        // Communication & media
        "discord", "vesktop", "spotify", "telegram-desktop", "slack", "teams", "skype", "element",
        // Downloaders & Indexers & Torrent
        "qbittorrent", "transmission", "transmission-qt", "deluge", "baloo_file", "tracker-miner-fs", "tracker-extract",
        // Background sync & cloud
        "dropbox", "nextcloud", "syncthing", "insync", "mega",
        // Heavy background dev / build tools
        "cc1plus", "rustc", "clangd", "cargo", "ninja"
    };
}

ProcessCategory BackgroundManager::classify_process(std::string_view comm, std::string_view cmdline) const {
    if (is_whitelisted(comm, cmdline)) {
        return ProcessCategory::Whitelisted;
    }
    if (is_blacklisted(comm, cmdline)) {
        return ProcessCategory::Blacklisted;
    }
    return ProcessCategory::Neutral;
}

bool BackgroundManager::is_whitelisted(std::string_view comm, std::string_view cmdline) const {
    std::string lcomm = to_lower_str(comm);
    std::string lcmd = to_lower_str(cmdline);

    for (const auto& rule : whitelist_rules_) {
        std::string lrule = to_lower_str(rule);
        if (lcomm == lrule || lcomm.find(lrule) != std::string::npos || lcmd.find(lrule) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool BackgroundManager::is_blacklisted(std::string_view comm, std::string_view cmdline) const {
    std::string lcomm = to_lower_str(comm);
    std::string lcmd = to_lower_str(cmdline);

    for (const auto& rule : blacklist_rules_) {
        std::string lrule = to_lower_str(rule);
        if (lcomm == lrule || lcomm.find(lrule) != std::string::npos || lcmd.find(lrule) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void BackgroundManager::setup_cgroup() const {
    fs::path bg_path(CGROUP_BACKGROUND_DIR);
    std::error_code ec;
    if (!fs::exists(bg_path, ec)) {
        fs::create_directories(bg_path, ec);
    }
    if (fs::exists(bg_path, ec)) {
        // Set soft weight and idle throttling
        write_file_string(bg_path / "cpu.weight", "50\n");
        write_file_string(bg_path / "cpu.idle", "1\n");
        write_file_string(bg_path / "io.weight", "50\n");
    }
}

bool BackgroundManager::throttle_pid(int pid) {
    if (pid <= 1) return false;

    // Verify process is not whitelisted before moving
    auto comm = fs_->read_file("/proc/" + std::to_string(pid) + "/comm").value_or("");
    auto cmdline = fs_->read_file("/proc/" + std::to_string(pid) + "/cmdline").value_or("");
    if (is_whitelisted(comm, cmdline)) {
        return false;
    }

    setup_cgroup();

    fs::path procs_file = fs::path(CGROUP_BACKGROUND_DIR) / "cgroup.procs";
    if (write_file_string(procs_file, std::to_string(pid) + "\n")) {
        throttled_pids_.insert(pid);
        ZENITH_LOG_DEBUG("Demoted PID " + std::to_string(pid) + " (" + comm + ") to zenith_background cgroup");
        return true;
    }

    return false;
}

size_t BackgroundManager::throttle_blacklisted_processes() {
    size_t count = 0;
    auto pids = fs_->list_pids();
    for (int pid : pids) {
        if (pid <= 1) continue;
        auto comm = fs_->read_file("/proc/" + std::to_string(pid) + "/comm").value_or("");
        auto cmdline = fs_->read_file("/proc/" + std::to_string(pid) + "/cmdline").value_or("");

        if (is_whitelisted(comm, cmdline)) continue;

        if (is_blacklisted(comm, cmdline)) {
            if (throttle_pid(pid)) {
                count++;
            }
        }
    }
    return count;
}

bool BackgroundManager::freeze_background() {
    setup_cgroup();
    fs::path freeze_file = fs::path(CGROUP_BACKGROUND_DIR) / "cgroup.freeze";
    if (write_file_string(freeze_file, "1\n")) {
        is_frozen_ = true;
        ZENITH_LOG_INFO("Frozen background cgroup: 0% CPU, zero cache contention for gaming");
        return true;
    }

#ifdef __linux__
    for (int pid : throttled_pids_) {
        if (pid > 1) {
            ::kill(pid, SIGSTOP);
        }
    }
    is_frozen_ = true;
    ZENITH_LOG_INFO("Suspended background processes via SIGSTOP fallback");
    return true;
#else
    is_frozen_ = true;
    return true;
#endif
}

bool BackgroundManager::unfreeze_background() {
    if (!is_frozen_) return true;

    fs::path freeze_file = fs::path(CGROUP_BACKGROUND_DIR) / "cgroup.freeze";
    if (write_file_string(freeze_file, "0\n")) {
        is_frozen_ = false;
        ZENITH_LOG_INFO("Thawed background cgroup: resumed normal background execution");
        return true;
    }

#ifdef __linux__
    for (int pid : throttled_pids_) {
        if (pid > 1) {
            ::kill(pid, SIGCONT);
        }
    }
    is_frozen_ = false;
    ZENITH_LOG_INFO("Resumed background processes via SIGCONT fallback");
    return true;
#else
    is_frozen_ = false;
    return true;
#endif
}

size_t BackgroundManager::restore_all_throttled() {
    if (is_frozen_) {
        unfreeze_background();
    }

    size_t count = 0;
    fs::path root_procs = "/sys/fs/cgroup/cgroup.procs";

    for (int pid : throttled_pids_) {
        if (write_file_string(root_procs, std::to_string(pid) + "\n")) {
            count++;
        }
    }
    throttled_pids_.clear();
    return count;
}

void BackgroundManager::add_whitelist_rule(std::string pattern) {
    whitelist_rules_.push_back(std::move(pattern));
}

void BackgroundManager::add_blacklist_rule(std::string pattern) {
    blacklist_rules_.push_back(std::move(pattern));
}

} // namespace zenith
