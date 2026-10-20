#include "zenith/irq_tuner.hpp"
#include "zenith/network_tuner.hpp"
#include "zenith/pipewire_tuner.hpp"
#include "zenith/gpu_controller.hpp"
#include "zenith/latency_controller.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#include <string>

using namespace zenith;

// Test 1: IRQ Steering for Network, USB, NVMe and GPU
void test_irq_steering() {
    MockIrqFs mock_fs;

    // Simulated /proc/interrupts
    std::string interrupts_data =
        "            CPU0       CPU1       CPU2       CPU3\n"
        "  45:       1000       2000       3000       4000  PCI-MSI-edge      amdgpu\n"
        "  46:        500        600        700        800  PCI-MSI-edge      nvme0q1\n"
        "  47:        200        250        300        350  PCI-MSI-edge      xhci_hcd\n"
        "  48:        100        150        200        250  PCI-MSI-edge      enp5s0\n";

    mock_fs.files["/proc/interrupts"] = interrupts_data;
    mock_fs.files["/proc/irq/45/smp_affinity_list"] = "0-3";
    mock_fs.files["/proc/irq/46/smp_affinity_list"] = "0-3";
    mock_fs.files["/proc/irq/47/smp_affinity_list"] = "0-3";
    mock_fs.files["/proc/irq/48/smp_affinity_list"] = "0-3";

    IrqTuner tuner(&mock_fs);
    assert(tuner.probe());

    std::vector<int> gaming_cores = {0, 1};
    std::vector<int> housekeeping_cores = {2, 3};

    bool applied = tuner.apply(gaming_cores, housekeeping_cores);
    assert(applied);
    assert(tuner.is_applied());

    // GPU IRQ (45) steered to gaming cores: "0,1"
    assert(mock_fs.files["/proc/irq/45/smp_affinity_list"] == "0,1\n");

    // NVMe (46), USB (47), Network (48) steered to housekeeping cores: "2,3"
    assert(mock_fs.files["/proc/irq/46/smp_affinity_list"] == "2,3\n");
    assert(mock_fs.files["/proc/irq/47/smp_affinity_list"] == "2,3\n");
    assert(mock_fs.files["/proc/irq/48/smp_affinity_list"] == "2,3\n");

    // Revert and verify restoration of original affinities
    bool reverted = tuner.revert();
    assert(reverted);
    assert(!tuner.is_applied());
    assert(mock_fs.files["/proc/irq/45/smp_affinity_list"] == "0-3\n");
    assert(mock_fs.files["/proc/irq/46/smp_affinity_list"] == "0-3\n");

    std::cout << "[PASS] test_irq_steering" << std::endl;
}

void test_irqbalance_suspension() {
    MockIrqFs mock_fs;
    mock_fs.files["/proc/interrupts"] = "  10: 100 200 PCI-MSI amdgpu\n";
    mock_fs.files["/proc/irq/10/smp_affinity_list"] = "0-3";
    IrqTuner tuner(&mock_fs);
    tuner.set_irqbalance_present(true);
    assert(tuner.probe());
    std::cout << "[PASS] test_irqbalance_suspension" << std::endl;
}

// Test 2: Network Active Interface Discovery & CAKE / Wi-Fi Power Save
void test_network_active_interface_and_qdisc() {
    MockNetworkFs mock_fs;

    // Simulated /proc/net/route with default route on wlan0
    std::string route_data =
        "Iface\tDestination\tGateway \tFlags\tRefCnt\tUse\tMetric\tMask\t\tMTU\tWindow\tIRTT\n"
        "wlan0\t00000000\t0101A8C0\t0003\t0\t0\t600\t00000000\t0\t0\t0\n"
        "eth0\t0001A8C0\t00000000\t0001\t0\t0\t100\t00FFFFFF\t0\t0\t0\n";

    mock_fs.files["/proc/net/route"] = route_data;
    mock_fs.files["/sys/class/net/wlan0/wireless"] = "";

    NetworkTuner tuner(&mock_fs);
    assert(tuner.probe());
    assert(tuner.get_active_interface() == "wlan0");

    std::vector<std::string> executed_commands;
    tuner.set_command_executor([&](const std::string& bin, const std::vector<std::string>& args) {
        std::string cmd = bin;
        for (const auto& a : args) cmd += " " + a;
        executed_commands.push_back(cmd);

        if (bin.find("tc") != std::string::npos && args[0] == "qdisc" && args[1] == "show") {
            return std::make_pair(true, "qdisc fq_codel 0: root refcnt 2 limit 10240p");
        }
        if (bin.find("iw") != std::string::npos && args.size() >= 3 && args[2] == "get") {
            return std::make_pair(true, "Power save: on");
        }
        return std::make_pair(true, "");
    });

    bool ok = tuner.apply();
    assert(ok);
    assert(tuner.is_applied());
    assert(tuner.original_qdisc() == "fq_codel");
    assert(tuner.wifi_power_save_disabled());

    // Verify CAKE replacement command was issued
    bool found_cake = false;
    bool found_wifi_off = false;
    for (const auto& cmd : executed_commands) {
        if (cmd.find("tc qdisc replace dev wlan0 root cake") != std::string::npos) found_cake = true;
        if (cmd.find("iw dev wlan0 set power_save off") != std::string::npos) found_wifi_off = true;
    }
    assert(found_cake);
    assert(found_wifi_off);

    // Revert and verify restoration commands
    executed_commands.clear();
    bool rev_ok = tuner.revert();
    assert(rev_ok);
    assert(!tuner.is_applied());

    bool found_fq_restore = false;
    bool found_wifi_on = false;
    for (const auto& cmd : executed_commands) {
        if (cmd.find("tc qdisc replace dev wlan0 root fq_codel") != std::string::npos) found_fq_restore = true;
        if (cmd.find("iw dev wlan0 set power_save on") != std::string::npos) found_wifi_on = true;
    }
    assert(found_fq_restore);
    assert(found_wifi_on);

    std::cout << "[PASS] test_network_active_interface_and_qdisc" << std::endl;
}

// Test 3: PipeWire Quantum Tuner & Dynamic xrun Guardian
void test_pipewire_and_xrun_guardian() {
    MockPipewireFs mock_fs;
    PipewireTuner tuner(&mock_fs);

    std::vector<std::string> pw_metadata_calls;
    tuner.set_command_executor([&](const std::string& bin, const std::vector<std::string>& args) {
        std::string cmd = bin;
        for (const auto& a : args) cmd += " " + a;
        pw_metadata_calls.push_back(cmd);
        return std::make_pair(true, "");
    });

    // 1. Initial application (quantum = 64)
    bool ok = tuner.apply(64, 48000);
    assert(ok);
    assert(tuner.is_applied());
    assert(tuner.current_quantum() == 64);
    assert(mock_fs.exists(std::string(PipewireTuner::DROPIN_PATH)));

    // 2. xrun audio dropout occurs (xrun count increases to 1)
    bool mitigated1 = tuner.handle_xrun_detected(1);
    assert(mitigated1);
    assert(tuner.current_quantum() == 128); // Bumped to 128
    assert(tuner.quantum_elevations_count() == 1);

    // 3. Second xrun burst (xrun count increases to 3)
    bool mitigated2 = tuner.handle_xrun_detected(3);
    assert(mitigated2);
    assert(tuner.current_quantum() == 256); // Bumped to 256
    assert(tuner.quantum_elevations_count() == 2);

    // 4. Stale/duplicate xrun count check (should not bump)
    bool mitigated3 = tuner.handle_xrun_detected(3);
    assert(!mitigated3);
    assert(tuner.current_quantum() == 256);

    // 5. Revert
    bool rev_ok = tuner.revert();
    assert(rev_ok);
    assert(!tuner.is_applied());
    assert(!mock_fs.exists(std::string(PipewireTuner::DROPIN_PATH))); // Drop-in config deleted

    // Check pw-metadata called with 0 to reset
    assert(!pw_metadata_calls.empty());
    assert(pw_metadata_calls.back().find("clock.force-quantum 0") != std::string::npos);

    std::cout << "[PASS] test_pipewire_and_xrun_guardian" << std::endl;
}

// Test 4: AMD Radeon DPM Performance Level Locking & Rollback
void test_gpu_amd_dpm_and_rollback() {
    MockGpuFs mock_fs;
    mock_fs.dirs["/sys/class/drm"] = {fs::path("/sys/class/drm/card0")};
    mock_fs.dirs["/sys/class/drm/card0/device/hwmon"] = {fs::path("/sys/class/drm/card0/device/hwmon/hwmon0")};
    mock_fs.files["/sys/class/drm/card0/device/hwmon/hwmon0/temp1_input"] = "55000"; // 55 C
    mock_fs.files["/sys/class/drm/card0/device/power_dpm_force_performance_level"] = "auto";

    auto mock_nvml = std::make_shared<MockNvmlDriver>();
    mock_nvml->available = false; // No NVIDIA

    GpuController controller(&mock_fs, mock_nvml);
    assert(controller.get_gpus().size() == 1);
    assert(controller.get_gpus()[0].vendor == GpuVendor::Amd);
    assert(controller.get_gpus()[0].original_dpm_level == "auto");

    // Lock AMD clocks
    bool locked = controller.lock_clocks();
    assert(locked);
    assert(controller.is_active());
    assert(mock_fs.files["/sys/class/drm/card0/device/power_dpm_force_performance_level"] == "high\n");

    // Rollback
    controller.rollback();
    assert(!controller.is_active());
    assert(mock_fs.files["/sys/class/drm/card0/device/power_dpm_force_performance_level"] == "auto\n");

    std::cout << "[PASS] test_gpu_amd_dpm_and_rollback" << std::endl;
}

// Test 5: NVIDIA Native NVML Persistence Mode, Clock Lock & Rollback
void test_gpu_nvidia_nvml_and_rollback() {
    MockGpuFs mock_fs;
    auto mock_nvml = std::make_shared<MockNvmlDriver>();
    mock_nvml->available = true;
    mock_nvml->simulated_temp = 58;

    GpuController controller(&mock_fs, mock_nvml);
    assert(controller.get_gpus().size() == 1);
    assert(controller.get_gpus()[0].vendor == GpuVendor::Nvidia);

    bool locked = controller.lock_clocks();
    assert(locked);
    assert(mock_nvml->persistence_mode == true);
    assert(mock_nvml->clocks_locked == true);
    assert(mock_nvml->locked_min == GpuController::SAFE_NVIDIA_MIN_MHZ);
    assert(mock_nvml->locked_max == GpuController::SAFE_NVIDIA_MAX_MHZ);

    controller.rollback();
    assert(mock_nvml->clocks_locked == false);
    assert(mock_nvml->persistence_mode == false);

    std::cout << "[PASS] test_gpu_nvidia_nvml_and_rollback" << std::endl;
}

// Test 6: Intel Arc / iGPU Boost Lock & Rollback
void test_gpu_intel_arc_and_rollback() {
    MockGpuFs mock_fs;
    mock_fs.dirs["/sys/class/drm"] = {fs::path("/sys/class/drm/card1")};
    mock_fs.dirs["/sys/class/drm/card1/device/hwmon"] = {fs::path("/sys/class/drm/card1/device/hwmon/hwmon1")};
    mock_fs.files["/sys/class/drm/card1/device/hwmon/hwmon1/temp1_input"] = "52000"; // 52 C
    mock_fs.files["/sys/class/drm/card1/gt_min_freq_mhz"] = "300";
    mock_fs.files["/sys/class/drm/card1/gt_boost_freq_mhz"] = "2100";

    auto mock_nvml = std::make_shared<MockNvmlDriver>();
    mock_nvml->available = false;

    GpuController controller(&mock_fs, mock_nvml);
    assert(controller.get_gpus().size() == 1);
    assert(controller.get_gpus()[0].vendor == GpuVendor::Intel);
    assert(controller.get_gpus()[0].original_intel_min_freq == 300);

    bool locked = controller.lock_clocks();
    assert(locked);
    assert(mock_fs.files["/sys/class/drm/card1/gt_min_freq_mhz"] == "2100");

    controller.rollback();
    assert(mock_fs.files["/sys/class/drm/card1/gt_min_freq_mhz"] == "300\n");

    std::cout << "[PASS] test_gpu_intel_arc_and_rollback" << std::endl;
}

// Test 7: Thermal & Battery Safety Guardrails
void test_thermal_and_battery_guardrails() {
    // 1. Overheating GPU (90 C >= 85 C limit)
    {
        MockGpuFs mock_fs;
        mock_fs.dirs["/sys/class/drm"] = {fs::path("/sys/class/drm/card0")};
        mock_fs.dirs["/sys/class/drm/card0/device/hwmon"] = {fs::path("/sys/class/drm/card0/device/hwmon/hwmon0")};
        mock_fs.files["/sys/class/drm/card0/device/hwmon/hwmon0/temp1_input"] = "90000"; // 90 C OVERHEAT!
        mock_fs.files["/sys/class/drm/card0/device/power_dpm_force_performance_level"] = "auto";

        auto mock_nvml = std::make_shared<MockNvmlDriver>();
        mock_nvml->available = false;

        GpuController controller(&mock_fs, mock_nvml);
        bool locked = controller.lock_clocks();
        assert(!locked); // Refused to lock!
        assert(controller.thermal_throttle_guard_active());
        assert(mock_fs.files["/sys/class/drm/card0/device/power_dpm_force_performance_level"] == "auto");
    }

    // 2. Battery Discharging Protection
    {
        MockGpuFs mock_fs;
        mock_fs.dirs["/sys/class/drm"] = {fs::path("/sys/class/drm/card0")};
        mock_fs.dirs["/sys/class/drm/card0/device/hwmon"] = {fs::path("/sys/class/drm/card0/device/hwmon/hwmon0")};
        mock_fs.files["/sys/class/drm/card0/device/hwmon/hwmon0/temp1_input"] = "50000"; // 50 C
        mock_fs.files["/sys/class/drm/card0/device/power_dpm_force_performance_level"] = "auto";

        // Power supply is discharging battery
        mock_fs.dirs["/sys/class/power_supply"] = {fs::path("/sys/class/power_supply/BAT0")};
        mock_fs.files["/sys/class/power_supply/BAT0/status"] = "Discharging";
        mock_fs.files["/sys/class/power_supply/BAT0/type"] = "Battery";

        auto mock_nvml = std::make_shared<MockNvmlDriver>();
        mock_nvml->available = false;

        GpuController controller(&mock_fs, mock_nvml);
        bool locked = controller.lock_clocks();
        assert(!locked); // Refused to lock to protect battery!
        assert(controller.battery_guard_active());
        assert(mock_fs.files["/sys/class/drm/card0/device/power_dpm_force_performance_level"] == "auto");
    }

    std::cout << "[PASS] test_thermal_and_battery_guardrails" << std::endl;
}

// Test 8: Subsystem Independence (Failure of one subsystem does not block others)
void test_subsystem_independence() {
    // 1. Working IRQ tuner
    MockIrqFs mock_irq_fs;
    mock_irq_fs.files["/proc/interrupts"] = "  10: 100 200 PCI-MSI amdgpu\n";
    mock_irq_fs.files["/proc/irq/10/smp_affinity_list"] = "0-3";
    auto irq = std::make_unique<IrqTuner>(&mock_irq_fs);

    // 2. Empty/Failing Network tuner (no default route)
    MockNetworkFs mock_net_fs;
    auto network = std::make_unique<NetworkTuner>(&mock_net_fs);

    // 3. Working PipeWire tuner
    MockPipewireFs mock_pw_fs;
    auto pipewire = std::make_unique<PipewireTuner>(&mock_pw_fs);

    // 4. Overheated GPU (aborts clock lock)
    MockGpuFs mock_gpu_fs;
    mock_gpu_fs.dirs["/sys/class/drm"] = {fs::path("/sys/class/drm/card0")};
    mock_gpu_fs.dirs["/sys/class/drm/card0/device/hwmon"] = {fs::path("/sys/class/drm/card0/device/hwmon/hwmon0")};
    mock_gpu_fs.files["/sys/class/drm/card0/device/hwmon/hwmon0/temp1_input"] = "92000"; // 92 C Overheat
    mock_gpu_fs.files["/sys/class/drm/card0/device/power_dpm_force_performance_level"] = "auto";
    auto mock_nvml = std::make_shared<MockNvmlDriver>();
    mock_nvml->available = false;
    auto gpu = std::make_unique<GpuController>(&mock_gpu_fs, mock_nvml);

    LatencyController controller(std::move(irq), std::move(network), std::move(pipewire), std::move(gpu));

    auto report = controller.apply_all({0, 1}, {2, 3}, 64);

    // IRQ and PipeWire applied successfully despite GPU overheating and empty network route!
    assert(report.irq_applied == true);
    assert(report.pipewire_applied == true);
    assert(report.gpu_applied == false); // Safely bypassed!

    // Rollback all
    controller.rollback_all();
    assert(!controller.irq().is_applied());
    assert(!controller.pipewire().is_applied());

    std::cout << "[PASS] test_subsystem_independence" << std::endl;
}

int main() {
    std::cout << "Starting Stage 6 Latency Subsystems & Hardware Controllers Tests..." << std::endl;

    test_irq_steering();
    test_irqbalance_suspension();
    test_network_active_interface_and_qdisc();
    test_pipewire_and_xrun_guardian();
    test_gpu_amd_dpm_and_rollback();
    test_gpu_nvidia_nvml_and_rollback();
    test_gpu_intel_arc_and_rollback();
    test_thermal_and_battery_guardrails();
    test_subsystem_independence();

    std::cout << "ALL 9 STAGE 6 LATENCY SUBSYSTEMS & HARDWARE TESTS PASSED!" << std::endl;
    return 0;
}
