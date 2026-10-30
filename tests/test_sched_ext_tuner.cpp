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

void test_start_and_stop() {
    MockSchedExtFs fs;
    fs.files["/proc/sys/kernel/osrelease"] = "6.12.0\n";
    fs.files["/usr/bin/scx_lavd"] = "";

    SchedExtTuner tuner(&fs);
    std::vector<std::string> calls;
    tuner.set_command_executor([&](const std::string& bin, const std::vector<std::string>& args) {
        calls.push_back(bin + " " + (args.empty() ? std::string() : args[0]));
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
    assert(calls[1].find("killall") != std::string::npos);

    std::cout << "[PASS] test_start_and_stop\n";
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

void test_auto_stop() {
    MockSchedExtFs fs;
    fs.files["/proc/sys/kernel/osrelease"] = "6.12.0\n";
    fs.files["/usr/bin/scx_lavd"] = "";

    int stops = 0;
    {
        SchedExtTuner tuner(&fs);
        tuner.set_command_executor([&](const std::string& bin, const std::vector<std::string>&) {
            if (bin.find("killall") != std::string::npos) stops++;
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
    test_start_and_stop();
    test_existing_scheduler_is_left_alone();
    test_auto_stop();
    return 0;
}
