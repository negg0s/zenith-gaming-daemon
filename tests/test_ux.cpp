// tests/test_ux.cpp
// Unit tests for Stage 10 UX: i18n, Doctor diagnostics, TUI dashboard, Shell completions, and Sanitized Report generator

#include "zenith/completions.hpp"
#include "zenith/doctor.hpp"
#include "zenith/i18n.hpp"
#include "zenith/report.hpp"
#include "zenith/socket_client.hpp"
#include "zenith/tui.hpp"

#include <cassert>
#include "zenith/version.hpp"
#include <iostream>
#include <string>

void test_i18n_and_errors() {
    using namespace zenith::i18n;

    set_language(Language::English);
    assert(get_language() == Language::English);
    assert(tr("doctor_title") == "Zenith Doctor System Diagnostics");

    set_language(Language::Russian);
    assert(get_language() == Language::Russian);
    assert(tr("doctor_title") == "Диагностика системы Zenith Doctor");

    std::string err = format_error("E101", "Socket connection failed");
    assert(err.find("[ZENITH-E101]") != std::string::npos);
    assert(err.find("docs/ERRORS.md#E101") != std::string::npos);

    // Reset back to English
    set_language(Language::English);
    std::cout << "[PASS] test_i18n_and_errors" << std::endl;
}

void test_doctor_engine() {
    auto items = zenith::Doctor::run_all_checks();
    assert(items.size() >= 8);

    bool found_cgroups = false;
    bool found_kernel = false;
    bool found_psi = false;

    for (const auto& it : items) {
        assert(!it.name.empty());
        assert(!it.message.empty());
        if (it.name == "cgroups_v2") found_cgroups = true;
        if (it.name == "kernel_version") found_kernel = true;
        if (it.name == "pressure_stall_information") found_psi = true;
    }

    assert(found_cgroups);
    assert(found_kernel);
    assert(found_psi);

    std::string json = zenith::Doctor::to_json(items);
    assert(json.front() == '[');
    assert(json.back() == ']');
    assert(json.find("\"name\": \"cgroups_v2\"") != std::string::npos);

    std::cout << "[PASS] test_doctor_engine (" << items.size() << " checks verified)" << std::endl;
}

void test_tui_dashboard_rendering() {
    zenith::TuiSnapshot snap;
    snap.daemon_version = zenith::ZENITH_VERSION;
    snap.profile = "Balanced";
    snap.is_game_active = true;
    snap.game_name = "Cyberpunk 2077";
    snap.game_pid = 4096;
    snap.game_type = "Proton (DXVK)";
    snap.gaming_cores = {0, 1, 2, 3, 4, 5, 6, 7};
    snap.housekeeping_cores = {8, 9, 10, 11, 12, 13, 14, 15};
    snap.cpu_pressure_avg10 = 2.45;
    snap.mem_pressure_avg10 = 0.80;
    snap.io_pressure_avg10 = 0.12;
    snap.cpu_temp_c = 64.0;
    snap.gpu_temp_c = 58.0;
    snap.active_tweaks_count = 14;
    snap.recent_events.push_back("Game Cyberpunk 2077 detected -> Activated Balanced profile");

    std::string rendered = zenith::TuiRenderer::render_dashboard(snap);
    assert(!rendered.empty());
    assert(rendered.find("ZENITH GAMING DAEMON MONITOR") != std::string::npos);
    assert(rendered.find("Cyberpunk 2077") != std::string::npos);
    assert(rendered.find("4096") != std::string::npos);
    assert(rendered.find("2.45%") != std::string::npos);
    assert(rendered.find("64 °C") != std::string::npos);

    std::cout << "[PASS] test_tui_dashboard_rendering" << std::endl;
}

void test_shell_completions() {
    using namespace zenith;
    std::string bash = CompletionGenerator::generate(ShellType::Bash);
    assert(!bash.empty());
    assert(bash.find("_zenithctl()") != std::string::npos);
    assert(bash.find("complete -F _zenithctl zenithctl") != std::string::npos);

    std::string zsh = CompletionGenerator::generate(ShellType::Zsh);
    assert(!zsh.empty());
    assert(zsh.find("#compdef zenithctl") != std::string::npos);

    std::string fish = CompletionGenerator::generate(ShellType::Fish);
    assert(!fish.empty());
    assert(fish.find("complete -c zenithctl") != std::string::npos);

    std::cout << "[PASS] test_shell_completions (bash, zsh, fish)" << std::endl;
}

void test_report_sanitizer() {
    std::string raw = "User emir running on /home/emir/projects connecting to 192.168.1.105 with local 127.0.0.1";
    // Set USER env var for test
#if defined(_WIN32)
    _putenv("USER=emir");
#else
    setenv("USER", "emir", 1);
#endif

    std::string sanitized = zenith::IssueReportGenerator::sanitize_text(raw);
    assert(sanitized.find("/home/$USER") != std::string::npos);
    assert(sanitized.find("192.168.1.105") == std::string::npos);
    assert(sanitized.find("XXX.XXX.XXX.XXX") != std::string::npos);
    // 127.0.0.1 should not be redacted
    assert(sanitized.find("127.0.0.1") != std::string::npos);

    std::string full_report = zenith::IssueReportGenerator::generate_anonymized_report();
    assert(!full_report.empty());
    assert(full_report.find("Zenith System Diagnostics & Issue Report") != std::string::npos);
    assert(full_report.find("System Doctor Results") != std::string::npos);

    std::cout << "[PASS] test_report_sanitizer" << std::endl;
}

int main() {
    std::cout << "Starting Stage 10 UX & CLI Test Suite..." << std::endl;

    test_i18n_and_errors();
    test_doctor_engine();
    test_tui_dashboard_rendering();
    test_shell_completions();
    test_report_sanitizer();

    std::cout << "ALL STAGE 10 UX TESTS PASSED!" << std::endl;
    return 0;
}
