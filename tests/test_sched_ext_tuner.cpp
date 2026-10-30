#include "zenith/sched_ext_tuner.hpp"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace zenith;

namespace {

void test_support_and_list() {
    MockSchedExtFs fs;
    fs.files["/proc/sys/kernel/osrelease"] = "6.12.0-arch1-1\n";
    fs.files["/usr/bin/scx_lavd"] = "";

    SchedExtTuner tuner(&fs);
    assert(tuner.is_supported());

    auto available = tuner.get_available_schedulers();
    assert(available.size() == 1);
    assert(available[0] == "scx_lavd");

    std::cout << "[PASS] test_support_and_list\n";
}

void test_kernel_version_compare() {
    {
        MockSchedExtFs fs;
        fs.files["/proc/sys/kernel/osrelease"] = "6.3.0-arch1-1\n";
        SchedExtTuner tuner(&fs);
        // 6.3 is older than 6.12 even though 6.3 > 6.12 as a decimal.
        assert(!tuner.is_supported());
    }
    {
        MockSchedExtFs fs;
        fs.files["/proc/sys/kernel/osrelease"] = "6.12-rc1\n";
        SchedExtTuner tuner(&fs);
        assert(tuner.is_supported());
    }
    {
        MockSchedExtFs fs;
        fs.files["/proc/sys/kernel/osrelease"] = "5.15.0-generic\n";
        SchedExtTuner tuner(&fs);
        assert(!tuner.is_supported());
    }
    {
        MockSchedExtFs fs;
        fs.files["/proc/sys/kernel/osrelease"] = "7.0.0\n";
        SchedExtTuner tuner(&fs);
        assert(tuner.is_supported());
    }

    std::cout << "[PASS] test_kernel_version_compare\n";
}

void test_lists_only_scx_binaries() {
    MockSchedExtFs fs;
    fs.files["/usr/bin/scx_rusty"] = "";
    fs.files["/usr/bin/scx_lavd"] = "";
    fs.files["/usr/bin/scx_lavd.desc"] = "";
    fs.files["/usr/bin/pw-metadata"] = "";
    fs.files["/usr/local/bin/scx_bpfland"] = "";

    SchedExtTuner tuner(&fs);
    auto available = tuner.get_available_schedulers();
    assert(available.size() == 3);
    assert(available[0] == "scx_bpfland");
    assert(available[1] == "scx_lavd");
    assert(available[2] == "scx_rusty");

    std::cout << "[PASS] test_lists_only_scx_binaries\n";
}

void test_start_and_stop() {
    MockSchedExtFs fs;
    fs.files["/proc/sys/kernel/osrelease"] = "6.12.0\n";
    fs.files["/usr/bin/scx_lavd"] = "";

    SchedExtTuner tuner(&fs);
    std::vector<std::string> calls;
    tuner.set_command_executor([&](const std::string& bin, const std::vector<std::string>& args) {
        std::string call = bin;
        for (const auto& arg : args) call += " " + arg;
        calls.push_back(call);
        return std::make_pair(true, std::string());
    });

    assert(tuner.start_scheduler("scx_lavd"));
    assert(tuner.is_running());
    assert(calls.size() == 1);
    assert(calls[0] == "/usr/bin/scx_lavd --background");

    assert(tuner.start_scheduler("scx_lavd"));
    assert(calls.size() == 1);

    assert(tuner.stop_scheduler());
    assert(!tuner.is_running());
    assert(calls.size() == 2);
    assert(calls[1].find("pkill") != std::string::npos);
    assert(calls[1].find("scx_lavd") != std::string::npos);

    std::cout << "[PASS] test_start_and_stop\n";
}

void test_stop_failure_is_reported() {
    MockSchedExtFs fs;
    fs.files["/proc/sys/kernel/osrelease"] = "6.12.0\n";
    fs.files["/usr/bin/scx_lavd"] = "";

    SchedExtTuner tuner(&fs);
    tuner.set_command_executor([&](const std::string& bin, const std::vector<std::string>&) {
        return std::make_pair(bin.find("pkill") == std::string::npos, std::string());
    });

    assert(tuner.start_scheduler("scx_lavd"));
    assert(!tuner.stop_scheduler());
    assert(tuner.is_running());

    std::cout << "[PASS] test_stop_failure_is_reported\n";
}

void test_existing_scheduler_is_left_alone() {
    MockSchedExtFs fs;
    fs.files["/sys/kernel/sched_ext/state"] = "enabled\n";
    fs.files["/sys/kernel/sched_ext/root/ops"] = "scx_bpfland\n";

    SchedExtTuner tuner(&fs);
    int calls = 0;
    tuner.set_command_executor([&](const std::string&, const std::vector<std::string>&) {
        calls++;
        return std::make_pair(true, std::string());
    });

    assert(tuner.is_active());
    assert(tuner.get_active_scheduler() == "scx_bpfland");
    assert(!tuner.start_scheduler("scx_lavd"));
    assert(calls == 0);

    std::cout << "[PASS] test_existing_scheduler_is_left_alone\n";
}

void test_rejects_unknown_binary_and_bad_name() {
    MockSchedExtFs fs;
    fs.files["/proc/sys/kernel/osrelease"] = "6.12.0\n";

    SchedExtTuner tuner(&fs);
    int calls = 0;
    tuner.set_command_executor([&](const std::string&, const std::vector<std::string>&) {
        calls++;
        return std::make_pair(true, std::string());
    });

    assert(!tuner.start_scheduler("scx_missing"));
    assert(!tuner.start_scheduler("../scx_lavd"));
    assert(!tuner.start_scheduler(""));
    assert(calls == 0);

    std::cout << "[PASS] test_rejects_unknown_binary_and_bad_name\n";
}

void test_auto_stop() {
    MockSchedExtFs fs;
    fs.files["/proc/sys/kernel/osrelease"] = "6.12.0\n";
    fs.files["/usr/bin/scx_lavd"] = "";

    int stops = 0;
    {
        SchedExtTuner tuner(&fs);
        tuner.set_command_executor([&](const std::string& bin, const std::vector<std::string>&) {
            if (bin.find("pkill") != std::string::npos) stops++;
            return std::make_pair(true, std::string());
        });
        assert(tuner.start_scheduler("scx_lavd"));
    }
    assert(stops == 1);

    std::cout << "[PASS] test_auto_stop\n";
}

} // namespace

int main() {
    test_support_and_list();
    test_kernel_version_compare();
    test_lists_only_scx_binaries();
    test_start_and_stop();
    test_stop_failure_is_reported();
    test_existing_scheduler_is_left_alone();
    test_rejects_unknown_binary_and_bad_name();
    test_auto_stop();
    return 0;
}
