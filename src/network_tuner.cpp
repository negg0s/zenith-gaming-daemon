// SPDX-License-Identifier: MIT
#include "zenith/network_tuner.hpp"
#include "zenith/secure_process.hpp"
#include <sstream>
#include <algorithm>

namespace zenith {

NetworkTuner::NetworkTuner(INetworkFs* fs)
    : fs_(fs ? fs : &RealNetworkFs::instance()) {}

NetworkTuner::~NetworkTuner() {
    revert();
}

std::pair<bool, std::string> NetworkTuner::run_cmd(const std::string& bin, const std::vector<std::string>& args) {
    if (cmd_executor_) {
        return cmd_executor_(bin, args);
    }
    auto res = SecureProcess::execute(bin, args);
    return {res.executed && res.exit_code == 0, res.stdout_output};
}

void NetworkTuner::discover_active_interface() {
    active_iface_.clear();
    auto route_data = fs_->read_file("/proc/net/route");
    if (!route_data) return;

    std::istringstream stream(*route_data);
    std::string line;
    // Skip header line
    std::getline(stream, line);

    while (std::getline(stream, line)) {
        std::istringstream line_stream(line);
        std::string iface, dest, gateway;
        int flags = 0;
        if (line_stream >> iface >> dest >> gateway >> flags) {
            // Default route has destination 00000000
            if (dest == "00000000") {
                active_iface_ = iface;
                break;
            }
        }
    }
}

std::string NetworkTuner::get_active_interface() const {
    if (!active_iface_.empty()) return active_iface_;
    // Const version of discovery
    auto route_data = fs_->read_file("/proc/net/route");
    if (!route_data) return "";
    std::istringstream stream(*route_data);
    std::string line;
    std::getline(stream, line);
    while (std::getline(stream, line)) {
        std::istringstream line_stream(line);
        std::string iface, dest;
        if (line_stream >> iface >> dest) {
            if (dest == "00000000") return iface;
        }
    }
    return "";
}

void NetworkTuner::query_current_qdisc() {
    original_qdisc_.clear();
    if (active_iface_.empty()) return;

    auto [ok, out] = run_cmd("/usr/bin/tc", {"qdisc", "show", "dev", active_iface_});
    if (ok && !out.empty()) {
        std::istringstream stream(out);
        std::string token1, token2;
        if (stream >> token1 >> token2) {
            if (token1 == "qdisc") {
                original_qdisc_ = token2;
            }
        }
    }
}

bool NetworkTuner::probe() {
    return fs_->exists("/proc/net/route");
}

bool NetworkTuner::apply() {
    if (applied_) return true;

    discover_active_interface();
    if (active_iface_.empty()) {
        ZENITH_LOG_INFO("No active default route interface found; skipping network tuning.");
        return true;
    }

    query_current_qdisc();

    // 1. Apply CAKE (fallback to FQ_CODEL if CAKE unsupported)
    auto [cake_ok, cake_out] = run_cmd("/usr/bin/tc", {"qdisc", "replace", "dev", active_iface_, "root", "cake"});
    (void)cake_out;
    if (cake_ok) {
        ZENITH_LOG_INFO("Configured CAKE queueing discipline on active interface: " + active_iface_);
    } else {
        auto [fq_ok, fq_out] = run_cmd("/usr/bin/tc", {"qdisc", "replace", "dev", active_iface_, "root", "fq_codel"});
        (void)fq_out;
        if (fq_ok) {
            ZENITH_LOG_INFO("Configured FQ_CODEL queueing discipline on active interface: " + active_iface_);
        }
    }

    // 2. Wi-Fi Power Save check & disable
    bool is_wireless = fs_->exists("/sys/class/net/" + active_iface_ + "/wireless") ||
                       fs_->exists("/sys/class/net/" + active_iface_ + "/phy80211");
    if (is_wireless || active_iface_.starts_with("wl")) {
        auto [iw_ok, iw_out] = run_cmd("/usr/bin/iw", {"dev", active_iface_, "get", "power_save"});
        if (iw_ok && iw_out.find("Power save: on") != std::string::npos) {
            auto [dis_ok, dis_out] = run_cmd("/usr/bin/iw", {"dev", active_iface_, "set", "power_save", "off"});
            (void)dis_out;
            if (dis_ok) {
                wifi_power_save_disabled_ = true;
                ZENITH_LOG_INFO("Disabled Wi-Fi power save on " + active_iface_ + " to prevent packet jitter.");
            }
        }
    }

    applied_ = true;
    return true;
}

bool NetworkTuner::revert() {
    if (!applied_) return true;

    if (!active_iface_.empty()) {
        // Restore original qdisc if we had recorded one
        if (!original_qdisc_.empty()) {
            (void)run_cmd("/usr/bin/tc", {"qdisc", "replace", "dev", active_iface_, "root", original_qdisc_});
            ZENITH_LOG_DEBUG("Restored original qdisc (" + original_qdisc_ + ") on " + active_iface_);
        }

        // Restore Wi-Fi power save if we disabled it
        if (wifi_power_save_disabled_) {
            (void)run_cmd("/usr/bin/iw", {"dev", active_iface_, "set", "power_save", "on"});
            ZENITH_LOG_DEBUG("Restored Wi-Fi power save on " + active_iface_);
            wifi_power_save_disabled_ = false;
        }
    }

    applied_ = false;
    return true;
}

std::string NetworkTuner::describe() const {
    return "Network QoS & Bufferbloat Mitigation (CAKE/FQ_CODEL & Wi-Fi Power Save)";
}

} // namespace zenith
