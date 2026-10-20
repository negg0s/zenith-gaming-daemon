#include "zenith/result.hpp"
#include "zenith/interfaces/isysfs.hpp"
#include "zenith/interfaces/iprocess_source.hpp"
#include "zenith/interfaces/icommand_runner.hpp"
#include "zenith/interfaces/iclock.hpp"
#include "zenith/interfaces/inetlink.hpp"
#include "zenith/raii.hpp"
#include "zenith/structured_logger.hpp"
#include "zenith/event_loop.hpp"
#include "zenith/tweaks/tweak_registry.hpp"
#include "zenith/tweaks/tweak_interface.hpp"

#include <cassert>
#include <cerrno>
#include <iostream>
#include <sstream>
#include <thread>
#include <atomic>

using namespace zenith;

// Test mock tweak for plugin-like registration
class DummyPluginTweak : public ITweak {
public:
    std::string name() const override { return "dummy_plugin_tweak"; }
    std::string description() const override { return "Test dummy tweak for plugin architecture"; }
    uint32_t supported_profiles() const override { return ProfileMask::All; }

    bool probe() override { return true; }
    bool apply(Profile) override { applied_ = true; return true; }
    bool revert() override { applied_ = false; return true; }
    std::string describe() const override { return "dummy: " + std::string(applied_ ? "1" : "0"); }
    std::string explain() const override { return "Explanation for dummy tweak"; }

    std::string get_current_value() const override { return applied_ ? "1" : "0"; }
    std::string get_target_value(Profile) const override { return "1"; }

private:
    bool applied_{false};
};

// Register via macro to prove plugin registration works
ZENITH_REGISTER_TWEAK(DummyPluginTweak)

void test_result_monadic_operations() {
    // 1. Success case
    Result<int> r1 = Ok(42);
    assert(r1.is_ok());
    assert(r1.has_value());
    assert(!r1.is_err());
    assert(r1.value() == 42);
    assert(*r1 == 42);

    // .map()
    auto mapped = r1.map([](int x) { return std::to_string(x * 2); });
    assert(mapped.is_ok());
    assert(mapped.value() == "84");

    // .and_then()
    auto chained = r1.and_then([](int x) -> Result<double> {
        if (x > 0) return Ok(static_cast<double>(x) / 2.0);
        return Result<double>::err(Error(ErrorCode::InvalidArgument, "Must be positive"));
    });
    assert(chained.is_ok());
    assert(chained.value() == 21.0);

    // 2. Error case
    Result<int> err_res = Result<int>::err(Error(ErrorCode::NotFound, "Value not found", ENOENT));
    assert(err_res.is_err());
    assert(!err_res.has_value());
    assert(err_res.value_or(100) == 100);
    assert(err_res.error().code == ErrorCode::NotFound);
    assert(err_res.error().system_errno == ENOENT);
    assert(err_res.error().to_string().find("Value not found") != std::string::npos);

    // .or_else()
    auto recovered = err_res.or_else([](const Error&) -> Result<int> {
        return Ok(999);
    });
    assert(recovered.is_ok());
    assert(recovered.value() == 999);

    // 3. Result<void>
    Result<void> void_ok = Ok();
    assert(void_ok.is_ok());
    assert(!void_ok.is_err());

    Result<void> void_err = Result<void>::err(Error(ErrorCode::PermissionDenied, "Access denied"));
    assert(void_err.is_err());
    assert(void_err.error().code == ErrorCode::PermissionDenied);

    std::cout << "[PASS] test_result_monadic_operations" << std::endl;
}

void test_interfaces_and_fakes() {
    // 1. ISysfs via MockSysfs
    MockSysfs mock_sysfs;
    mock_sysfs.set_file("/sys/devices/system/cpu/isolated", "2,3,4");
    assert(mock_sysfs.exists("/sys/devices/system/cpu/isolated"));
    auto read_res = mock_sysfs.read_string("/sys/devices/system/cpu/isolated");
    assert(read_res.is_ok());
    assert(read_res.value() == "2,3,4");

    auto write_res = mock_sysfs.write_string("/sys/devices/system/cpu/isolated", "0-3");
    assert(write_res.is_ok());
    assert(mock_sysfs.read_string("/sys/devices/system/cpu/isolated").value() == "0-3");

    // Test failure simulation
    mock_sysfs.set_fail_writes(true);
    assert(mock_sysfs.write_string("/test", "val").is_err());

    // 2. IProcessSource via MockProcessSource
    MockProcessSource mock_ps;
    mock_ps.add_process(1234, {"game.x86_64", "game.x86_64 -fullscreen", "SteamAppId=1091500", {1234, 1235, 1236}});
    auto pids = mock_ps.get_running_pids().value();
    assert(pids.size() == 1);
    assert(pids[0] == 1234);
    assert(mock_ps.get_process_comm(1234).value() == "game.x86_64");
    assert(mock_ps.get_process_tasks(1234).value().size() == 3);

    // 3. ICommandRunner via MockCommandRunner
    MockCommandRunner mock_cmd;
    mock_cmd.set_canned_output("/usr/bin/nvidia-smi", {0, "GPU Clocks: 2100 MHz", ""});
    auto cmd_res = mock_cmd.run("/usr/bin/nvidia-smi", {"-q"}).value();
    assert(cmd_res.exit_code == 0);
    assert(cmd_res.stdout_str.find("2100 MHz") != std::string::npos);
    assert(mock_cmd.history().size() == 1);

    // 4. IClock via MockClock
    MockClock mock_clock;
    auto t0 = mock_clock.now_steady();
    mock_clock.advance(std::chrono::milliseconds(500));
    auto t1 = mock_clock.now_steady();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    assert(elapsed == 500);

    // 5. INetlink via MockNetlink
    MockNetlink mock_netlink;
    assert(!mock_netlink.is_connected());
    assert(mock_netlink.open().is_ok());
    assert(mock_netlink.is_connected());

    mock_netlink.inject_event({NetlinkEventType::Exec, 4321, 4321, 0});
    auto poll_res = mock_netlink.poll_event(std::chrono::milliseconds(0)).value();
    assert(poll_res.has_value());
    assert(poll_res->type == NetlinkEventType::Exec);
    assert(poll_res->pid == 4321);

    std::cout << "[PASS] test_interfaces_and_fakes" << std::endl;
}

void test_raii_guards() {
    // 1. UniqueFd move and close semantics
    {
        UniqueFd fd1;
        assert(!fd1.is_valid());
        assert(fd1.get() == -1);

        // Dummy fd using standard stream
        UniqueFd fd2(100);
        assert(fd2.is_valid());
        assert(fd2.get() == 100);

        UniqueFd fd3 = std::move(fd2);
        assert(fd3.is_valid());
        assert(fd3.get() == 100);
        assert(!fd2.is_valid());
        assert(fd2.get() == -1);

        (void)fd3.release();
        assert(!fd3.is_valid());
    }

    // 2. ScopeExitGuard
    bool executed = false;
    {
        auto guard = make_scope_exit([&]() { executed = true; });
        assert(!executed);
    }
    assert(executed);

    // Dismiss test
    bool dismissed_executed = false;
    {
        auto guard = make_scope_exit([&]() { dismissed_executed = true; });
        guard.dismiss();
    }
    assert(!dismissed_executed);

    std::cout << "[PASS] test_raii_guards" << std::endl;
}

void test_structured_logger_and_rate_limiter() {
    auto& logger = StructuredLogger::instance();
    logger.set_min_level(StructuredLogLevel::Trace);

    std::stringstream ss;
    logger.set_custom_sink(&ss);

    // 1. Test Text Format
    logger.set_format_mode(LogFormatMode::Text);
    logger.log(StructuredLogLevel::Info, "test_core", "Initialization complete", {{"version", "1.1.0"}});
    std::string text_out = ss.str();
    assert(text_out.find("Initialization complete") != std::string::npos);
    assert(text_out.find("version=1.1.0") != std::string::npos);

    // 2. Test JSON Format
    ss.str("");
    ss.clear();
    logger.set_format_mode(LogFormatMode::Json);
    logger.log(StructuredLogLevel::Warn, "latency", "PSI memory spike detected", {{"avg10", "35.2"}});
    std::string json_out = ss.str();
    assert(json_out.find("\"level\":\"WARN\"") != std::string::npos);
    assert(json_out.find("\"subsystem\":\"latency\"") != std::string::npos);
    assert(json_out.find("\"avg10\":\"35.2\"") != std::string::npos);

    // 3. Test Journald Format
    ss.str("");
    ss.clear();
    logger.set_format_mode(LogFormatMode::Journald);
    logger.log(StructuredLogLevel::Error, "gpu", "Hardware throttle event", {{"temp", "92"}});
    std::string journald_out = ss.str();
    assert(journald_out.find("<3>[gpu]") != std::string::npos);

    // 4. Test Rate Limiter
    RateLimiter limiter(3); // Max 3 per second
    uint64_t suppressed = 0;
    assert(limiter.check("key1", suppressed)); // 1
    assert(limiter.check("key1", suppressed)); // 2
    assert(limiter.check("key1", suppressed)); // 3
    assert(!limiter.check("key1", suppressed)); // 4 (suppressed!)
    assert(!limiter.check("key1", suppressed)); // 5 (suppressed!)

    // Reset sink to stdout
    logger.set_custom_sink(nullptr);
    logger.set_format_mode(LogFormatMode::Text);

    std::cout << "[PASS] test_structured_logger_and_rate_limiter" << std::endl;
}

void test_event_loop_and_concurrency() {
    auto mock_poller = std::make_shared<MockPoller>();
    EventLoop loop(mock_poller);

    std::atomic<int> callback_hits{0};
    int dummy_fd = 50;

    auto add_res = loop.add_fd(dummy_fd, PollEvent::Read, [&](int fd, uint32_t events) {
        assert(fd == dummy_fd);
        assert(events & PollEvent::Read);
        callback_hits++;
    });
    assert(add_res.is_ok());

    // Post background task from another thread
    std::atomic<bool> task_executed{false};
    std::thread worker([&]() {
        loop.post([&]() {
            task_executed = true;
        });
    });
    worker.join();

    // Trigger FD event in mock poller
    mock_poller->trigger_event(dummy_fd, PollEvent::Read);

    // Single step
    loop.run_one();
    assert(task_executed.load());
    assert(callback_hits.load() == 1);

    // Verify remove
    assert(loop.remove_fd(dummy_fd).is_ok());
    mock_poller->trigger_event(dummy_fd, PollEvent::Read);
    loop.run_one();
    assert(callback_hits.load() == 1); // Unchanged because removed

    std::cout << "[PASS] test_event_loop_and_concurrency" << std::endl;
}

void test_plugin_tweak_registration() {
    auto& registry = TweakRegistry::instance();
    bool found_dummy = false;
    for (const auto& tweak : registry.get_all_tweaks()) {
        if (tweak->name() == "dummy_plugin_tweak") {
            found_dummy = true;
            break;
        }
    }
    assert(found_dummy);
    std::cout << "[PASS] test_plugin_tweak_registration" << std::endl;
}

int main() {
    std::cout << "Starting Stage 8 Reference C++20 Architecture Tests...\n";
    test_result_monadic_operations();
    test_interfaces_and_fakes();
    test_raii_guards();
    test_structured_logger_and_rate_limiter();
    test_event_loop_and_concurrency();
    test_plugin_tweak_registration();
    std::cout << "ALL 6 STAGE 8 ARCHITECTURE & EXCELLENCE TESTS PASSED!\n";
    return 0;
}
