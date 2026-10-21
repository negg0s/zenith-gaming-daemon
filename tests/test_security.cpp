#include "zenith/secure_process.hpp"
#include "zenith/secure_io.hpp"
#include "zenith/config.hpp"
#include "zenith/socket_server.hpp"
#include "zenith/capability_guard.hpp"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <fcntl.h>

namespace fs = std::filesystem;

void test_secure_process_whitelist() {
    using namespace zenith;

    // 1. Verify approved binaries
    assert(SecureProcess::is_binary_approved("/usr/bin/nvidia-smi") == true);
    assert(SecureProcess::is_binary_approved("/usr/bin/pw-metadata") == true);

    // 2. Verify rejection of arbitrary binaries and shells
    assert(SecureProcess::is_binary_approved("/bin/bash") == false);
    assert(SecureProcess::is_binary_approved("/bin/sh") == false);
    assert(SecureProcess::is_binary_approved("/usr/bin/python3") == false);
    assert(SecureProcess::is_binary_approved("rm -rf /") == false);

    // 3. Execution attempt on non-whitelisted binary must fail immediately
    auto result = SecureProcess::execute("/bin/sh", { "-c", "echo injected" });
    assert(result.executed == false);
    assert(result.stdout_output.empty());

    std::cout << "[PASS] test_secure_process_whitelist\n";
}

void test_secure_io_sandbox() {
    using namespace zenith;

    fs::path test_dir = fs::current_path() / "test_sec_io_dir";
    std::error_code ec;
    fs::remove_all(test_dir, ec);
    fs::create_directories(test_dir, ec);

    // 1. Safe write and read inside base directory
    bool wrote = SecureIo::write_string(test_dir, "test_file.txt", "zenith_secure_content");
    assert(wrote == true);

    auto read_val = SecureIo::read_string(test_dir, "test_file.txt");
    assert(read_val.has_value());
    assert(*read_val == "zenith_secure_content");

    // 2. Reject path traversal attempt with ".."
    int fd = SecureIo::open_beneath(test_dir, "../escape.txt", 0);
    assert(fd < 0);

    // 3. Path splitting for trusted directories
    auto [base1, sub1] = SecureIo::split_trusted_path("/proc/sys/vm/swappiness");
    assert(base1 == "/proc/sys/vm");
    assert(sub1 == "swappiness");

    auto [base2, sub2] = SecureIo::split_trusted_path("/sys/fs/cgroup/zenith_games/cgroup.procs");
    assert(base2 == "/sys/fs/cgroup");
    assert(sub2 == "zenith_games/cgroup.procs");

    // Traversal path must be rejected by split_trusted_path
    auto [base3, sub3] = SecureIo::split_trusted_path("/sys/fs/cgroup/../../etc/shadow");
    assert(base3.empty());
    assert(sub3.empty());

    fs::remove_all(test_dir, ec);
    std::cout << "[PASS] test_secure_io_sandbox\n";
}

void test_strict_config_validation() {
    using namespace zenith;

    // 1. Oversized config rejection (> 64 KB)
    std::string huge_config(70 * 1024, 'a');
    auto [cfg1, res1] = Config::parse_string(huge_config);
    assert(res1.success == false);
    assert(res1.error_message.find("exceeds maximum allowed size") != std::string::npos);

    // 2. Unknown key rejection
    std::string unknown_key_cfg = "cgroup_isolation_enabled = true\nmalicious_param = 42\n";
    auto [cfg2, res2] = Config::parse_string(unknown_key_cfg);
    assert(res2.success == false);
    assert(res2.error_message.find("Unknown configuration key: 'malicious_param'") != std::string::npos);

    // 3. Invalid boolean rejection
    std::string invalid_bool_cfg = "split_lock_mitigation_disabled = perhaps\n";
    auto [cfg3, res3] = Config::parse_string(invalid_bool_cfg);
    assert(res3.success == false);
    assert(res3.error_message.find("Invalid boolean value") != std::string::npos);

    // 4. Out-of-bounds integer rejection
    std::string oob_int_cfg = "swappiness = 9999\n";
    auto [cfg4, res4] = Config::parse_string(oob_int_cfg);
    assert(res4.success == false);
    assert(res4.error_message.find("Integer out of bounds") != std::string::npos);

    // 5. Valid config successfully parsed
    std::string valid_cfg = 
        "# Valid zenith configuration\n"
        "cgroup_isolation_enabled = true\n"
        "swappiness = 10\n"
        "dirty_ratio = 5\n"
        "poll_interval_ms = 250\n";
    auto [cfg5, res5] = Config::parse_string(valid_cfg);
    assert(res5.success == true);
    assert(cfg5.cgroup_isolation_enabled == true);
    assert(cfg5.swappiness == 10);
    assert(cfg5.dirty_ratio == 5);
    assert(cfg5.poll_interval_ms == 250);

    std::cout << "[PASS] test_strict_config_validation\n";
}

void test_socket_server() {
    using namespace zenith;

    fs::path test_sock_path = fs::temp_directory_path() / "zenith_test_run_sock" / "zenith_test.sock";
    SocketServer server(test_sock_path, "root");

    assert(SocketServer::MAX_MESSAGE_SIZE == 4096);
    server.set_command_handler([](const std::string& cmd, const ClientCredentials& creds) -> std::string {
        if (cmd == "STATUS") return "{\"status\":\"ok\",\"pid\":" + std::to_string(creds.pid) + "}";
        if (cmd == "PING") return "PONG";
        return "{\"status\":\"unknown\"}";
    });

    bool started = server.start();
    assert(started == true);
    assert(server.is_running() == true);
    server.stop();
    assert(server.is_running() == false);

    std::error_code ec;
    fs::remove_all(fs::temp_directory_path() / "zenith_test_run_sock", ec);

    std::cout << "[PASS] test_socket_server\n";
}

void test_capability_guard() {
    using namespace zenith;

    bool dropped = CapabilityGuard::drop_unneeded_capabilities();
    assert(dropped == true);
#ifdef __linux__
    assert(CapabilityGuard::is_hardened() == true);
#endif
    std::cout << "[PASS] test_capability_guard\n";
}

int main() {
    std::cout << "Running Stage 2 Security Tests...\n";
    test_secure_process_whitelist();
    test_secure_io_sandbox();
    test_strict_config_validation();
    test_socket_server();
    test_capability_guard();
    std::cout << "All Stage 2 Security Tests Passed Successfully!\n";
    return 0;
}
