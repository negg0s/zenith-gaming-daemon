#include "zenith/pressure_monitor.hpp"
#include "zenith/adaptive_controller.hpp"
#include "zenith/local_learner.hpp"
#include "zenith/sched_ext_tuner.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#include <string>

using namespace zenith;

// Test 1: PSI Parsing and Thermal Sampling
void test_pressure_monitor_psi_parsing() {
    MockPressureFs mock_fs;

    mock_fs.files["/proc/pressure/cpu"] =
        "some avg10=4.50 avg60=2.30 avg300=1.10 total=4567890\n";
    mock_fs.files["/proc/pressure/memory"] =
        "some avg10=12.50 avg60=5.00 avg300=2.00 total=123456\n"
        "full avg10=2.10 avg60=0.50 avg300=0.10 total=4567\n";
    mock_fs.files["/proc/pressure/io"] =
        "some avg10=8.20 avg60=4.10 avg300=1.50 total=98765\n"
        "full avg10=1.00 avg60=0.20 avg300=0.05 total=1234\n";

    mock_fs.dirs["/sys/class/hwmon"] = {fs::path("/sys/class/hwmon/hwmon0")};
    mock_fs.files["/sys/class/hwmon/hwmon0/temp1_input"] = "62000"; // 62 C

    PressureMonitor monitor(&mock_fs);
    assert(monitor.is_psi_supported());

    auto sp = monitor.sample();
    assert(sp.cpu_some.avg10 == 4.50);
    assert(sp.cpu_some.avg60 == 2.30);
    assert(sp.cpu_some.total_us == 4567890);

    assert(sp.memory_some.avg10 == 12.50);
    assert(sp.memory_full.avg10 == 2.10);

    assert(sp.io_some.avg10 == 8.20);
    assert(sp.io_full.avg10 == 1.00);

    assert(sp.cpu_max_temp_c == 62);
    assert(!sp.is_thermal_throttling);

    std::cout << "[PASS] test_pressure_monitor_psi_parsing" << std::endl;
}

// Test 2: Adaptive Profile Degradation on PSI Spikes
void test_adaptive_degradation_from_pressure() {
    AdaptiveController controller(Profile::Aggressive);
    assert(controller.active_profile() == Profile::Aggressive);

    std::vector<std::string> transition_reasons;
    controller.set_profile_change_callback([&](Profile, Profile, const std::string& reason) {
        transition_reasons.push_back(reason);
    });

    // Healthy sample
    SystemPressure healthy_sp;
    healthy_sp.cpu_max_temp_c = 60;
    healthy_sp.memory_some.avg10 = 5.0;
    healthy_sp.io_some.avg10 = 5.0;
    bool changed1 = controller.evaluate(healthy_sp);
    assert(!changed1);
    assert(controller.active_profile() == Profile::Aggressive);

    // Memory pressure spike (28.5% > 25.0% threshold)
    SystemPressure mem_spike_sp = healthy_sp;
    mem_spike_sp.memory_some.avg10 = 28.5;

    bool changed2 = controller.evaluate(mem_spike_sp);
    assert(changed2);
    assert(controller.active_profile() == Profile::Balanced); // Degraded to Balanced
    assert(!transition_reasons.empty());
    assert(transition_reasons.back().find("Memory pressure") != std::string::npos);

    // Severe I/O stall on Balanced (io_full > 15.0)
    SystemPressure io_severe_sp = healthy_sp;
    io_severe_sp.io_some.avg10 = 48.0;
    io_severe_sp.io_full.avg10 = 18.5;

    bool changed3 = controller.evaluate(io_severe_sp);
    assert(changed3);
    assert(controller.active_profile() == Profile::Safe); // Degraded to Safe

    assert(controller.history().size() == 2);
    assert(controller.history()[0].from_profile == Profile::Aggressive);
    assert(controller.history()[0].to_profile == Profile::Balanced);
    assert(controller.history()[1].from_profile == Profile::Balanced);
    assert(controller.history()[1].to_profile == Profile::Safe);

    std::cout << "[PASS] test_adaptive_degradation_from_pressure" << std::endl;
}

// Test 3: Adaptive Degradation on Thermal Throttling
void test_adaptive_thermal_throttling_degradation() {
    AdaptiveController controller(Profile::Aggressive);

    // Thermal throttling spike (temp >= 85 C and is_thermal_throttling flag)
    SystemPressure thermal_sp;
    thermal_sp.cpu_max_temp_c = 89;
    thermal_sp.is_thermal_throttling = true;

    bool changed = controller.evaluate(thermal_sp);
    assert(changed);
    assert(controller.active_profile() == Profile::Balanced);
    assert(!controller.history().empty());
    assert(controller.history().back().reason.find("temperature") != std::string::npos);

    std::cout << "[PASS] test_adaptive_thermal_throttling_degradation" << std::endl;
}

// Test 4: Recovery Hysteresis and Anti-Oscillation
void test_adaptive_recovery_hysteresis_and_anti_oscillation() {
    AdaptiveController controller(Profile::Aggressive);
    controller.set_cooldown_seconds(0); // 0s cooldown for instant test execution
    controller.set_hysteresis_cycles(3); // 3 consecutive healthy cycles required

    // Force degrade to Safe
    SystemPressure severe_sp;
    severe_sp.memory_some.avg10 = 40.0;
    severe_sp.memory_full.avg10 = 15.0;
    controller.evaluate(severe_sp); // Aggressive -> Balanced
    controller.evaluate(severe_sp); // Balanced -> Safe
    assert(controller.active_profile() == Profile::Safe);

    SystemPressure healthy_sp;
    healthy_sp.cpu_max_temp_c = 55;
    healthy_sp.memory_some.avg10 = 4.0;
    healthy_sp.io_some.avg10 = 3.0;

    // Cycle 1: Stays Safe
    assert(!controller.evaluate(healthy_sp));
    assert(controller.active_profile() == Profile::Safe);
    assert(controller.healthy_cycles_count() == 1);

    // Cycle 2: Stays Safe
    assert(!controller.evaluate(healthy_sp));
    assert(controller.active_profile() == Profile::Safe);
    assert(controller.healthy_cycles_count() == 2);

    // Sudden transient spike: resets healthy count!
    SystemPressure glitch_sp = healthy_sp;
    glitch_sp.cpu_max_temp_c = 86;
    assert(!controller.evaluate(glitch_sp));
    assert(controller.healthy_cycles_count() == 0); // Reset!
    assert(controller.active_profile() == Profile::Safe);

    // Now 3 consecutive healthy cycles:
    assert(!controller.evaluate(healthy_sp)); // Cycle 1
    assert(!controller.evaluate(healthy_sp)); // Cycle 2
    assert(controller.evaluate(healthy_sp));  // Cycle 3 -> Promotes Safe -> Balanced!
    assert(controller.active_profile() == Profile::Balanced);

    // Another 3 consecutive healthy cycles -> Promotes Balanced -> Aggressive!
    assert(!controller.evaluate(healthy_sp)); // Cycle 1
    assert(!controller.evaluate(healthy_sp)); // Cycle 2
    assert(controller.evaluate(healthy_sp));  // Cycle 3 -> Promotes Balanced -> Aggressive!
    assert(controller.active_profile() == Profile::Aggressive);

    std::cout << "[PASS] test_adaptive_recovery_hysteresis_and_anti_oscillation" << std::endl;
}

// Test 5: Battery and Handheld Clamping
void test_platform_clamping() {
    AdaptiveController controller(Profile::Aggressive);
    assert(controller.active_profile() == Profile::Aggressive);

    // Engage battery mode
    controller.set_battery_mode(true);
    assert(controller.active_profile() == Profile::Balanced); // Clamped immediately!
    assert(controller.is_battery_mode());

    // Pristine healthy metrics should NEVER promote to Aggressive on battery
    SystemPressure pristine_sp;
    pristine_sp.cpu_max_temp_c = 45;
    pristine_sp.memory_some.avg10 = 1.0;
    pristine_sp.io_some.avg10 = 1.0;
    controller.set_cooldown_seconds(0);
    controller.set_hysteresis_cycles(1);

    controller.evaluate(pristine_sp);
    assert(controller.active_profile() == Profile::Balanced); // Still clamped!

    // Disengage battery mode
    controller.set_battery_mode(false);
    controller.evaluate(pristine_sp);
    assert(controller.active_profile() == Profile::Aggressive); // Promoted back!

    // Handheld form factor clamp
    controller.set_handheld_mode(true);
    assert(controller.active_profile() == Profile::Balanced); // Clamped to Balanced!

    std::cout << "[PASS] test_platform_clamping" << std::endl;
}

// Test 6: MangoHud CSV Log Parsing
void test_mangohud_csv_parsing() {
    std::string mock_csv =
        "fps,frametime,cpu_temp,gpu_temp\n"
        "60.0,16.66,55,60\n"
        "60.2,16.61,55,60\n"
        "59.8,16.72,56,60\n"
        "30.0,33.33,56,61\n" // 1 stutter
        "60.0,16.66,56,61\n";

    auto bm = LocalLearner::parse_mangohud_csv(mock_csv);
    assert(bm.sample_count == 5);
    assert(bm.avg_fps > 45.0 && bm.avg_fps < 65.0);
    assert(bm.p1_low_fps > 0.0);
    assert(bm.stutter_count == 1);
    assert(bm.frametime_variance_ms > 0.0);

    std::cout << "[PASS] test_mangohud_csv_parsing" << std::endl;
}

// Test 7: Local Learner Profile Suggestion Engine
void test_local_learner_profile_suggestion() {
    LocalLearner learner;

    // Session 1: Aggressive profile triggered thermal throttling
    GameSessionRecord sess1;
    sess1.game_id = "1091500"; // Cyberpunk 2077
    sess1.profile_used = Profile::Aggressive;
    sess1.benchmark.avg_fps = 75.0;
    sess1.benchmark.p1_low_fps = 35.0;
    sess1.benchmark.frametime_variance_ms = 8.5;
    sess1.benchmark.stutter_count = 14;
    sess1.max_cpu_temp = 92;
    sess1.thermal_throttled = true;
    learner.record_session(sess1);

    // Session 2: Balanced profile smooth frame pacing without throttling
    GameSessionRecord sess2;
    sess2.game_id = "1091500";
    sess2.profile_used = Profile::Balanced;
    sess2.benchmark.avg_fps = 72.0;
    sess2.benchmark.p1_low_fps = 48.0; // Higher 1% low!
    sess2.benchmark.frametime_variance_ms = 3.2; // Much lower variance!
    sess2.benchmark.stutter_count = 1;
    sess2.max_cpu_temp = 74;
    sess2.thermal_throttled = false;
    learner.record_session(sess2);

    // Suggestion for Cyberpunk 2077
    auto rec = learner.suggest_profile("1091500");
    assert(rec.suggested_profile == Profile::Balanced);
    assert(rec.explanation.find("throttling") != std::string::npos || rec.explanation.find("variance") != std::string::npos);

    // Suggestion on battery power
    auto battery_rec = learner.suggest_profile("1091500", true, false);
    assert(battery_rec.suggested_profile == Profile::Balanced);
    assert(battery_rec.explanation.find("battery") != std::string::npos);

    // Format output text check
    std::string text = learner.format_suggestion_text(rec);
    assert(text.find("100% Offline") != std::string::npos);

    std::cout << "[PASS] test_local_learner_profile_suggestion" << std::endl;
}

// Test 8: Sched_ext (scx) Probing & Lifecycle
void test_sched_ext_probing_and_lifecycle() {
    MockSchedExtFs mock_fs;
    mock_fs.files["/sys/kernel/sched_ext/state"] = "disabled\n";
    mock_fs.files["/proc/sys/kernel/osrelease"] = "6.12.5-cachyos\n";
    mock_fs.files["/usr/bin/scx_lavd"] = "";

    SchedExtTuner tuner(&mock_fs);
    assert(tuner.is_supported());
    assert(tuner.get_active_scheduler() == "disabled");

    auto available = tuner.get_available_schedulers();
    assert(!available.empty());
    assert(available[0] == "scx_lavd");

    std::vector<std::string> commands_run;
    tuner.set_command_executor([&](const std::string& bin, const std::vector<std::string>& args) {
        std::string cmd = bin;
        for (const auto& a : args) cmd += " " + a;
        commands_run.push_back(cmd);
        return std::make_pair(true, "");
    });

    bool started = tuner.start_scheduler("scx_lavd");
    assert(started);
    assert(tuner.is_running());
    assert(tuner.current_scheduler() == "scx_lavd");
    assert(!commands_run.empty());

    bool stopped = tuner.stop_scheduler();
    assert(stopped);
    assert(!tuner.is_running());

    std::cout << "[PASS] test_sched_ext_probing_and_lifecycle" << std::endl;
}

int main() {
    std::cout << "Starting Stage 7 Adaptive Controller, PSI Telemetry & Local Learner Tests..." << std::endl;

    test_pressure_monitor_psi_parsing();
    test_adaptive_degradation_from_pressure();
    test_adaptive_thermal_throttling_degradation();
    test_adaptive_recovery_hysteresis_and_anti_oscillation();
    test_platform_clamping();
    test_mangohud_csv_parsing();
    test_local_learner_profile_suggestion();
    test_sched_ext_probing_and_lifecycle();

    std::cout << "ALL 8 STAGE 7 ADAPTIVE CONTROLLER & LEARNER TESTS PASSED!" << std::endl;
    return 0;
}
