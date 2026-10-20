#include "zenith/irq_tuner.hpp"
#include "zenith/secure_process.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace zenith {

namespace {

std::string to_lower_str(std::string_view sv) {
    std::string s(sv);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

std::string format_cpulist(const std::vector<int>& cores) {
    if (cores.empty()) return "0";
    std::string res;
    for (size_t i = 0; i < cores.size(); ++i) {
        if (i > 0) res += ",";
        res += std::to_string(cores[i]);
    }
    return res;
}

} // namespace

IrqTuner::IrqTuner(IIrqFs* fs)
    : fs_(fs ? fs : &RealIrqFs::instance()) {}

IrqTuner::~IrqTuner() {
    revert();
}

bool IrqTuner::probe() {
    return fs_->exists("/proc/interrupts");
}

void IrqTuner::discover_irqs() {
    discovered_irqs_.clear();
    auto content = fs_->read_file("/proc/interrupts");
    if (!content) return;

    std::istringstream stream(*content);
    std::string line;
    while (std::getline(stream, line)) {
        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) continue;

        std::string irq_str = trim(line.substr(0, colon_pos));
        if (irq_str.empty() || !std::all_of(irq_str.begin(), irq_str.end(), ::isdigit)) {
            continue; // Skip NMI, LOC, CAL, etc.
        }

        int irq = -1;
        try {
            irq = std::stoi(irq_str);
        } catch (...) {
            continue;
        }

        std::string rest = line.substr(colon_pos + 1);
        std::string lrest = to_lower_str(rest);

        IrqTarget target;
        bool matched = false;

        // Check GPU
        if (lrest.find("amdgpu") != std::string::npos ||
            lrest.find("nvidia") != std::string::npos ||
            lrest.find("i915") != std::string::npos ||
            lrest.find("xe") != std::string::npos ||
            lrest.find("nouveau") != std::string::npos) {
            target = IrqTarget::Gaming;
            matched = true;
        }
        // Check Network
        else if (lrest.find("eth") != std::string::npos ||
                 lrest.find("enp") != std::string::npos ||
                 lrest.find("wlan") != std::string::npos ||
                 lrest.find("wlp") != std::string::npos ||
                 lrest.find("e1000") != std::string::npos ||
                 lrest.find("r8169") != std::string::npos ||
                 lrest.find("igb") != std::string::npos ||
                 lrest.find("mlx") != std::string::npos) {
            target = IrqTarget::Background;
            matched = true;
        }
        // Check USB
        else if (lrest.find("xhci") != std::string::npos ||
                 lrest.find("ehci") != std::string::npos) {
            target = IrqTarget::Background;
            matched = true;
        }
        // Check NVMe
        else if (lrest.find("nvme") != std::string::npos) {
            target = IrqTarget::Background;
            matched = true;
        }

        if (matched) {
            DiscoveredIrq item;
            item.irq = irq;
            item.name = trim(rest);
            item.target = target;

            fs::path aff_path = "/proc/irq/" + std::to_string(irq) + "/smp_affinity_list";
            if (auto orig = fs_->read_file(aff_path)) {
                item.original_affinity = trim(*orig);
            } else {
                fs::path aff_mask_path = "/proc/irq/" + std::to_string(irq) + "/smp_affinity";
                if (auto orig_mask = fs_->read_file(aff_mask_path)) {
                    item.original_affinity = trim(*orig_mask);
                }
            }
            discovered_irqs_.push_back(item);
        }
    }
}

bool IrqTuner::apply(const std::vector<int>& gaming_cores, const std::vector<int>& housekeeping_cores) {
    if (applied_) return true;

    discover_irqs();
    if (discovered_irqs_.empty()) {
        ZENITH_LOG_INFO("No high-throughput IRQs discovered for steering.");
        return true;
    }

    // Suspend irqbalance if active to prevent affinity override fighting
    if (irqbalance_present_ || fs::exists("/usr/bin/irqbalance")) {
        auto res = SecureProcess::execute("/usr/bin/systemctl", {"is-active", "--quiet", "irqbalance"});
        if (res.executed && res.exit_code == 0) {
            auto stop_res = SecureProcess::execute("/usr/bin/systemctl", {"stop", "irqbalance"});
            if (stop_res.executed && stop_res.exit_code == 0) {
                irqbalance_stopped_ = true;
                ZENITH_LOG_INFO("Suspended irqbalance service for game session.");
            }
        }
    }

    std::string gaming_cpulist = format_cpulist(gaming_cores.empty() ? std::vector<int>{0} : gaming_cores);
    std::string bg_cpulist = format_cpulist(housekeeping_cores.empty() ? gaming_cores : housekeeping_cores);

    for (const auto& item : discovered_irqs_) {
        std::string target_list = (item.target == IrqTarget::Gaming) ? gaming_cpulist : bg_cpulist;
        fs::path aff_path = "/proc/irq/" + std::to_string(item.irq) + "/smp_affinity_list";
        fs_->write_file(aff_path, target_list + "\n");
        ZENITH_LOG_DEBUG("Steered IRQ " + std::to_string(item.irq) + " (" +
                         (item.target == IrqTarget::Gaming ? "Gaming" : "Background") +
                         ") to CPU list: " + target_list);
    }

    applied_ = true;
    return true;
}

bool IrqTuner::revert() {
    if (!applied_) return true;

    for (const auto& item : discovered_irqs_) {
        if (!item.original_affinity.empty()) {
            fs::path aff_path = "/proc/irq/" + std::to_string(item.irq) + "/smp_affinity_list";
            fs_->write_file(aff_path, item.original_affinity + "\n");
        }
    }

    if (irqbalance_stopped_) {
        SecureProcess::execute("/usr/bin/systemctl", {"start", "irqbalance"});
        irqbalance_stopped_ = false;
        ZENITH_LOG_INFO("Restored irqbalance service.");
    }

    applied_ = false;
    return true;
}

std::string IrqTuner::describe() const {
    return "IRQ Affinity Steering (Network/USB/NVMe -> Background, GPU -> Gaming CCD)";
}

} // namespace zenith
