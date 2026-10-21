#include "zenith/tweaks/tweak_registry.hpp"
#include "zenith/tweaks/cpu_governor_tweak.hpp"
#include "zenith/tweaks/cgroup_weight_tweak.hpp"
#include "zenith/tweaks/uclamp_tweak.hpp"
#include "zenith/tweaks/timer_slack_tweak.hpp"
#include "zenith/tweaks/nvme_tweak.hpp"
#include "zenith/tweaks/usb_autosuspend_tweak.hpp"
#include "zenith/tweaks/zram_swap_tweak.hpp"
#include "zenith/tweaks/sysctl_tweak.hpp"
#include "zenith/tweaks/cstate_latency_tweak.hpp"
#include "zenith/tweaks/transparent_hugepage_tweak.hpp"
#include "zenith/conflict_detector.hpp"
#include "zenith/per_game_profile.hpp"
#include <cassert>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

void test_profiles() {
    using namespace zenith;

    assert(string_to_profile("safe") == Profile::Safe);
    assert(string_to_profile("balanced") == Profile::Balanced);
    assert(string_to_profile("aggressive") == Profile::Aggressive);
    assert(string_to_profile("ultra") == Profile::Ultra);
    assert(string_to_profile("invalid") == std::nullopt);

    assert(profile_to_string(Profile::Safe) == "safe");
    assert(profile_to_string(Profile::Balanced) == "balanced");
    assert(profile_to_string(Profile::Aggressive) == "aggressive");
    assert(profile_to_string(Profile::Ultra) == "ultra");

    assert(profile_to_mask(Profile::Safe) == ProfileMask::Safe);
    assert(profile_to_mask(Profile::Balanced) == ProfileMask::Balanced);
    assert(profile_to_mask(Profile::Aggressive) == ProfileMask::Aggressive);
    assert(profile_to_mask(Profile::Ultra) == ProfileMask::Ultra);

    std::cout << "[PASS] test_profiles\n";
}

void test_cpu_governor_mock() {
    using namespace zenith;

    fs::path mock_dir = fs::current_path() / "test_mock_cpu_gov";
    std::error_code ec;
    fs::remove_all(mock_dir, ec);

    fs::path cpu0 = mock_dir / "cpu0" / "cpufreq";
    fs::path cpu1 = mock_dir / "cpu1" / "cpufreq";
    fs::create_directories(cpu0, ec);
    fs::create_directories(cpu1, ec);
    fs::create_directories(mock_dir / "cpu0" / "power", ec);
    fs::create_directories(mock_dir / "cpu1" / "power", ec);

    write_file_string(cpu0 / "scaling_governor", "powersave");
    write_file_string(cpu0 / "energy_performance_preference", "balance_power");
    write_file_string(mock_dir / "cpu0" / "power" / "energy_perf_bias", "6");
    write_file_string(cpu1 / "scaling_governor", "powersave");
    write_file_string(cpu1 / "energy_performance_preference", "balance_power");
    write_file_string(mock_dir / "cpu1" / "power" / "energy_perf_bias", "6");

    CpuGovernorTweak tweak(mock_dir);
    assert(tweak.probe() == true);

    // 1. Apply Balanced (only changes EPP to performance)
    bool ok_bal = tweak.apply(Profile::Balanced);
    assert(ok_bal == true);
    assert(*read_file_string(cpu0 / "energy_performance_preference") == "performance");
    assert(*read_file_string(cpu0 / "scaling_governor") == "powersave");
    assert(*read_file_string(mock_dir / "cpu0" / "power" / "energy_perf_bias") == "0");

    // 2. Apply Aggressive (changes both governor and EPP to performance)
    bool ok_agg = tweak.apply(Profile::Aggressive);
    assert(ok_agg == true);
    assert(*read_file_string(cpu0 / "scaling_governor") == "performance");
    assert(*read_file_string(cpu0 / "energy_performance_preference") == "performance");

    // 3. Revert
    bool reverted = tweak.revert();
    assert(reverted == true);
    assert(*read_file_string(cpu0 / "scaling_governor") == "powersave");
    assert(*read_file_string(cpu0 / "energy_performance_preference") == "balance_power");
    assert(*read_file_string(mock_dir / "cpu0" / "power" / "energy_perf_bias") == "6");

    fs::remove_all(mock_dir, ec);
    std::cout << "[PASS] test_cpu_governor_mock\n";
}

void test_cpu_governor_policy_mock() {
    using namespace zenith;

    fs::path mock_dir = fs::current_path() / "test_mock_cpu_policy";
    std::error_code ec;
    fs::remove_all(mock_dir, ec);
    fs::create_directories(mock_dir, ec);

    fs::path policy0 = mock_dir / "cpufreq" / "policy0";
    fs::path policy1 = mock_dir / "cpufreq" / "policy1";
    fs::create_directories(policy0, ec);
    fs::create_directories(policy1, ec);

    write_file_string(policy0 / "scaling_governor", "powersave");
    write_file_string(policy0 / "energy_performance_preference", "balance_power");
    write_file_string(policy1 / "scaling_governor", "powersave");
    write_file_string(policy1 / "energy_performance_preference", "balance_power");

    CpuGovernorTweak tweak(mock_dir);
    assert(tweak.probe() == true);

    // Apply Balanced
    bool ok_bal = tweak.apply(Profile::Balanced);
    assert(ok_bal == true);
    assert(*read_file_string(policy0 / "energy_performance_preference") == "performance");
    assert(*read_file_string(policy0 / "scaling_governor") == "powersave");

    // Apply Aggressive
    bool ok_agg = tweak.apply(Profile::Aggressive);
    assert(ok_agg == true);
    assert(*read_file_string(policy0 / "scaling_governor") == "performance");
    assert(*read_file_string(policy0 / "energy_performance_preference") == "performance");

    // Revert
    bool reverted = tweak.revert();
    assert(reverted == true);
    assert(*read_file_string(policy0 / "scaling_governor") == "powersave");
    assert(*read_file_string(policy0 / "energy_performance_preference") == "balance_power");

    fs::remove_all(mock_dir, ec);
    std::cout << "[PASS] test_cpu_governor_policy_mock\n";
}

void test_cgroup_weight_mock() {
    using namespace zenith;

    fs::path mock_dir = fs::current_path() / "test_mock_cgroup_weights";
    std::error_code ec;
    fs::remove_all(mock_dir, ec);
    fs::create_directories(mock_dir, ec);

    fs::path bg_slice = mock_dir / "zenith_background";
    fs::create_directories(bg_slice, ec);

    write_file_string(bg_slice / "cpu.weight", "100");
    write_file_string(bg_slice / "cpu.idle", "0");
    write_file_string(bg_slice / "io.weight", "100");

    CgroupWeightTweak tweak(mock_dir);
    assert(tweak.probe() == true);

    // Apply balanced
    assert(tweak.apply(Profile::Balanced) == true);
    assert(*read_file_string(bg_slice / "cpu.weight") == "50");
    assert(*read_file_string(bg_slice / "cpu.idle") == "1");
    assert(*read_file_string(bg_slice / "io.weight") == "50");

    // Revert
    assert(tweak.revert() == true);
    assert(*read_file_string(bg_slice / "cpu.weight") == "100");
    assert(*read_file_string(bg_slice / "cpu.idle") == "0");
    assert(*read_file_string(bg_slice / "io.weight") == "100");

    fs::remove_all(mock_dir, ec);
    std::cout << "[PASS] test_cgroup_weight_mock\n";
}

void test_nvme_mock() {
    using namespace zenith;

    fs::path mock_dir = fs::current_path() / "test_mock_nvme";
    std::error_code ec;
    fs::remove_all(mock_dir, ec);

    fs::path queue_dir = mock_dir / "nvme0n1" / "queue";
    fs::create_directories(queue_dir, ec);

    write_file_string(queue_dir / "scheduler", "[mq-deadline] none bfq");
    write_file_string(queue_dir / "read_ahead_kb", "128");

    // Also test SATA SSD with rotational == 0
    fs::path sda_queue = mock_dir / "sda" / "queue";
    fs::create_directories(sda_queue, ec);
    write_file_string(sda_queue / "rotational", "0\n");
    write_file_string(sda_queue / "scheduler", "[bfq] none mq-deadline");
    write_file_string(sda_queue / "read_ahead_kb", "128");

    NvmeTweak tweak(mock_dir);
    assert(tweak.probe() == true);

    // Apply
    assert(tweak.apply(Profile::Balanced) == true);
    assert(*read_file_string(queue_dir / "scheduler") == "none");
    assert(*read_file_string(sda_queue / "scheduler") == "none");

    // Revert
    assert(tweak.revert() == true);
    assert(*read_file_string(queue_dir / "scheduler") == "mq-deadline");
    assert(*read_file_string(sda_queue / "scheduler") == "bfq");

    fs::remove_all(mock_dir, ec);
    std::cout << "[PASS] test_nvme_mock\n";
}

void test_usb_mock() {
    using namespace zenith;

    fs::path mock_dir = fs::current_path() / "test_mock_usb";
    std::error_code ec;
    fs::remove_all(mock_dir, ec);

    fs::path usb_dev = mock_dir / "1-1" / "power";
    fs::create_directories(usb_dev, ec);
    write_file_string(usb_dev / "control", "auto");

    UsbAutosuspendTweak tweak(mock_dir);
    assert(tweak.probe() == true);

    // Apply
    assert(tweak.apply(Profile::Balanced) == true);
    assert(*read_file_string(usb_dev / "control") == "on");

    // Revert
    assert(tweak.revert() == true);
    assert(*read_file_string(usb_dev / "control") == "auto");

    fs::remove_all(mock_dir, ec);
    std::cout << "[PASS] test_usb_mock\n";
}

void test_zram_swap_mock() {
    using namespace zenith;

    fs::path mock_vm = fs::current_path() / "test_mock_vm";
    fs::path mock_block = fs::current_path() / "test_mock_block";
    std::error_code ec;
    fs::remove_all(mock_vm, ec);
    fs::remove_all(mock_block, ec);

    fs::create_directories(mock_vm, ec);
    fs::create_directories(mock_block / "zram0", ec); // simulate zram presence

    write_file_string(mock_vm / "page-cluster", "3");
    write_file_string(mock_vm / "watermark_boost_factor", "15000");

    ZramSwapTweak tweak(mock_vm, mock_block);
    assert(tweak.probe() == true);
    assert(tweak.is_zram_detected() == true);

    // Apply
    assert(tweak.apply(Profile::Balanced) == true);
    assert(*read_file_string(mock_vm / "page-cluster") == "0");
    assert(*read_file_string(mock_vm / "watermark_boost_factor") == "0");

    // Revert
    assert(tweak.revert() == true);
    assert(*read_file_string(mock_vm / "page-cluster") == "3");
    assert(*read_file_string(mock_vm / "watermark_boost_factor") == "15000");

    fs::remove_all(mock_vm, ec);
    fs::remove_all(mock_block, ec);
    std::cout << "[PASS] test_zram_swap_mock\n";
}

void test_sysctl_tweak_mock() {
    using namespace zenith;

    fs::path mock_proc = fs::current_path() / "test_mock_proc_sysctl";
    fs::path mock_sys = fs::current_path() / "test_mock_sys_sysctl";
    std::error_code ec;
    fs::remove_all(mock_proc, ec);
    fs::remove_all(mock_sys, ec);

    fs::path split_lock = mock_proc / "sys/kernel/split_lock_mitigate";
    fs::path thp_enabled = mock_sys / "kernel/mm/transparent_hugepage/enabled";
    fs::path compaction = mock_proc / "sys/vm/compaction_proactiveness";

    fs::create_directories(split_lock.parent_path(), ec);
    fs::create_directories(thp_enabled.parent_path(), ec);
    fs::create_directories(compaction.parent_path(), ec);

    write_file_string(split_lock, "1");
    write_file_string(thp_enabled, "always [madvise] never");
    write_file_string(compaction, "20");

    SysctlTweak tweak(mock_proc, mock_sys);
    assert(tweak.probe() == true);

    // Balanced: split_lock MUST stay untouched (1), thp = madvise, compaction = 0
    assert(tweak.apply(Profile::Balanced) == true);
    assert(*read_file_string(split_lock) == "1");
    assert(*read_file_string(thp_enabled) == "madvise");
    assert(*read_file_string(compaction) == "0");

    // Aggressive: split_lock disabled (0), thp = always
    assert(tweak.apply(Profile::Aggressive) == true);
    assert(*read_file_string(split_lock) == "0");
    assert(*read_file_string(thp_enabled) == "always");

    // Revert
    assert(tweak.revert() == true);
    assert(*read_file_string(split_lock) == "1");
    assert(*read_file_string(thp_enabled) == "madvise");
    assert(*read_file_string(compaction) == "20");

    fs::remove_all(mock_proc, ec);
    fs::remove_all(mock_sys, ec);
    std::cout << "[PASS] test_sysctl_tweak_mock\n";
}

void test_dry_run_and_registry() {
    using namespace zenith;

    auto& reg = TweakRegistry::instance();
    reg.register_defaults();

    auto diffs = reg.compute_dry_run(Profile::Balanced);
    assert(!diffs.empty());

    // Test blacklist
    std::unordered_set<std::string> blacklist = { "nvme_io_scheduler_readahead" };
    auto diffs_filtered = reg.compute_dry_run(Profile::Balanced, blacklist);
    assert(diffs_filtered.size() == diffs.size() - 1);

    for (const auto& d : diffs_filtered) {
        assert(d.tweak_name != "nvme_io_scheduler_readahead");
    }

    std::cout << "[PASS] test_dry_run_and_registry\n";
}

void test_per_game_profile() {
    using namespace zenith;

    std::string config_content = 
        "# Sample profiles\n"
        "[730]\n"
        "profile = aggressive\n"
        "pipewire_quantum = 32\n\n"
        "[cyberpunk2077]\n"
        "profile = balanced\n"
        "blacklist = split_lock_mitigate, nvme_io_scheduler_readahead\n";

    auto mgr = PerGameProfileManager::load_from_string(config_content);
    assert(mgr.get_rules().size() == 2);

    // 1. Resolve by AppID
    auto r1 = mgr.resolve("cs2", 730);
    assert(r1.identifier == "730");
    assert(r1.profile_override == Profile::Aggressive);
    assert(r1.custom_overrides["pipewire_quantum"] == "32");

    // 2. Resolve by exe substring
    auto r2 = mgr.resolve("/opt/games/cyberpunk2077/bin/Cyberpunk2077.exe", 0);
    assert(r2.identifier == "cyberpunk2077");
    assert(r2.profile_override == Profile::Balanced);
    assert(r2.blacklisted_tweaks.count("split_lock_mitigate") == 1);
    assert(r2.blacklisted_tweaks.count("nvme_io_scheduler_readahead") == 1);

    // 3. Fallback for unknown game
    auto r3 = mgr.resolve("unknown_indie_game", 0, Profile::Balanced);
    assert(r3.profile_override == Profile::Balanced);
    assert(r3.blacklisted_tweaks.empty());

    std::cout << "[PASS] test_per_game_profile\n";
}


void test_cstate_latency_mock() {
    using namespace zenith;

    fs::path mock_cstate = fs::current_path() / "test_mock_cstate";
    std::error_code ec;
    fs::remove(mock_cstate, ec);
    write_file_string(mock_cstate, "999");

    CstateLatencyTweak tweak(mock_cstate);
    assert(tweak.probe() == true);
    assert(tweak.name() == "cstate_latency");
    assert(tweak.is_supported_in(Profile::Balanced) == true);
    assert(tweak.is_supported_in(Profile::Aggressive) == true);
    assert(tweak.is_supported_in(Profile::Safe) == false);

    bool ok_apply = tweak.apply(Profile::Balanced);
    assert(ok_apply == true);
    assert(tweak.get_current_value().find("50 us") != std::string::npos);

    bool ok_revert = tweak.revert();
    assert(ok_revert == true);
    assert(tweak.get_current_value().find("unconstrained") != std::string::npos);

    fs::remove(mock_cstate, ec);
    std::cout << "[PASS] test_cstate_latency_mock\n";
}

void test_thp_tweak_mock() {
    using namespace zenith;

    fs::path mock_thp = fs::current_path() / "test_mock_thp";
    std::error_code ec;
    fs::remove_all(mock_thp, ec);
    fs::create_directories(mock_thp / "khugepaged", ec);

    fs::path defrag_p = mock_thp / "defrag";
    fs::path khp_defrag_p = mock_thp / "khugepaged" / "defrag";
    fs::path shmem_p = mock_thp / "shmem_enabled";

    write_file_string(defrag_p, "[always] madvise never");
    write_file_string(khp_defrag_p, "1");
    write_file_string(shmem_p, "[never] always advise");

    TransparentHugepageTweak tweak(mock_thp);
    assert(tweak.probe() == true);
    assert(tweak.name() == "transparent_hugepage");
    assert(tweak.get_current_value() == "always");

    // Apply
    bool ok_apply = tweak.apply(Profile::Aggressive);
    assert(ok_apply == true);
    assert(*read_file_string(defrag_p) == "madvise");
    assert(*read_file_string(khp_defrag_p) == "0");
    assert(*read_file_string(shmem_p) == "advise");

    // Revert
    bool ok_revert = tweak.revert();
    assert(ok_revert == true);
    assert(*read_file_string(defrag_p) == "always");
    assert(*read_file_string(khp_defrag_p) == "1");
    assert(*read_file_string(shmem_p) == "never");

    fs::remove_all(mock_thp, ec);
    std::cout << "[PASS] test_thp_tweak_mock\n";
}

void test_conflict_detector() {
    using namespace zenith;

    auto conflicts = ConflictDetector::probe_system_conflicts();
    (void)conflicts; // Simply verify it executes safely without throwing
    std::cout << "[PASS] test_conflict_detector\n";
}

void test_uclamp_and_timer_slack() {
    using namespace zenith;

    UclampTweak uclamp;
    (void)uclamp.probe();
    assert(uclamp.name() == "sched_uclamp_min");
    assert(uclamp.is_supported_in(Profile::Balanced) == true);
    assert(uclamp.is_supported_in(Profile::Aggressive) == true);
    assert(uclamp.is_supported_in(Profile::Safe) == false);

    TimerSlackTweak timer;
    (void)timer.probe();
    assert(timer.name() == "timer_slack_and_sched_latency");
    assert(timer.is_supported_in(Profile::Balanced) == true);
    assert(timer.is_supported_in(Profile::Safe) == false);

    std::cout << "[PASS] test_uclamp_and_timer_slack\n";
}

int main() {
    std::cout << "Running Stage 3 Subsystem Tweaks, Profiles & Conflict Tests...\n";
    test_profiles();
    test_cpu_governor_mock();
    test_cpu_governor_policy_mock();
    test_cgroup_weight_mock();
    test_nvme_mock();
    test_usb_mock();
    test_zram_swap_mock();
    test_sysctl_tweak_mock();
    test_uclamp_and_timer_slack();
    test_cstate_latency_mock();
    test_thp_tweak_mock();
    test_dry_run_and_registry();
    test_per_game_profile();
    test_conflict_detector();
    std::cout << "All Stage 3 Tests Passed Successfully!\n";
    return 0;
}
