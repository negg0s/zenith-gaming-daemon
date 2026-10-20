#include "zenith/game_detector.hpp"
#include "zenith/process_tuner.hpp"
#include "zenith/background_manager.hpp"
#include "zenith/proc_connector.hpp"
#include "zenith/proc_fs.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

using namespace zenith;

// Test 1: Steam Game Detection via Environ & Cmdline
void test_steam_detection() {
    MockProcFs mock_fs;

    // Steam Game 1: Cyberpunk 2077 via Proton (SteamAppId in environ)
    MockProcFs::MockProcess cyberpunk;
    cyberpunk.pid = 4001;
    cyberpunk.comm = "Cyberpunk2077.e";
    cyberpunk.cmdline = "Z:\\SteamLibrary\\steamapps\\common\\Cyberpunk 2077\\bin\\x64\\Cyberpunk2077.exe";
    cyberpunk.environ = "SteamAppId=1091500\0STEAM_COMPAT_DATA_PATH=/home/user/.steam/compat\0";
    cyberpunk.open_fds = {"/dev/dri/renderD128", "/dev/null"};
    cyberpunk.tasks = {4001, 4002, 4003};
    cyberpunk.task_comms[4001] = "Cyberpunk2077.e";
    cyberpunk.task_comms[4002] = "vk_render";
    cyberpunk.task_comms[4003] = "audio_out";
    mock_fs.add_process(cyberpunk);

    // Steam Game 2: Counter-Strike 2 Native (AppId in cmdline)
    MockProcFs::MockProcess cs2;
    cs2.pid = 4010;
    cs2.comm = "cs2";
    cs2.cmdline = "/home/user/.steam/steamapps/common/Counter-Strike Global Offensive/game/bin/linuxsteamrt64/cs2 -steam AppId=730";
    cs2.environ = "SteamAppId=730\0";
    cs2.open_fds = {"/dev/dri/renderD128"};
    mock_fs.add_process(cs2);

    GameDetector detector({}, &mock_fs);
    auto game1 = detector.inspect_process(4001);
    assert(game1.has_value());
    assert(game1->pid == 4001);
    assert(game1->steam_appid == "1091500");
    assert(game1->runner_type == "proton");
    assert(game1->is_wine_or_proton);
    assert(game1->has_gpu_render_node);

    auto game2 = detector.inspect_process(4010);
    assert(game2.has_value());
    assert(game2->pid == 4010);
    assert(game2->steam_appid == "730");
    assert(!game2->is_wine_or_proton);

    std::cout << "[PASS] test_steam_detection" << std::endl;
}

// Test 2: Wine / Proton & Standalone Wineserver Filtering
void test_wine_and_wineserver_filtering() {
    MockProcFs mock_fs;

    // Bare/idle wineserver should NEVER be recognized as a game
    MockProcFs::MockProcess wineserver;
    wineserver.pid = 3001;
    wineserver.comm = "wineserver";
    wineserver.cmdline = "/usr/bin/wineserver -p";
    mock_fs.add_process(wineserver);

    // Standalone Wine game
    MockProcFs::MockProcess wine_game;
    wine_game.pid = 3005;
    wine_game.comm = "wine64-preloader";
    wine_game.cmdline = "wine64 /games/Witcher3/bin/x64/witcher3.exe";
    wine_game.open_fds = {"/dev/dri/renderD128"};
    mock_fs.add_process(wine_game);

    GameDetector detector({}, &mock_fs);
    assert(!detector.is_game(3001)); // wineserver filtered out!
    assert(detector.is_game(3005));   // wine game detected!

    auto detected = detector.inspect_process(3005);
    assert(detected.has_value());
    assert(detected->is_wine_or_proton);
    assert(detected->runner_type == "proton");

    std::cout << "[PASS] test_wine_and_wineserver_filtering" << std::endl;
}

// Test 3: Emulators & Launchers Filtering
void test_emulators_and_launchers() {
    MockProcFs mock_fs;

    // Known emulators
    MockProcFs::MockProcess rpcs3;
    rpcs3.pid = 5001;
    rpcs3.comm = "rpcs3";
    rpcs3.cmdline = "/usr/bin/rpcs3 /games/ps3/game.iso";
    rpcs3.open_fds = {"/dev/dri/renderD128"};
    mock_fs.add_process(rpcs3);

    MockProcFs::MockProcess yuzu;
    yuzu.pid = 5002;
    yuzu.comm = "yuzu";
    yuzu.cmdline = "/usr/bin/yuzu";
    mock_fs.add_process(yuzu);

    // Launchers without game (Must NOT be detected as games)
    MockProcFs::MockProcess steamwebhelper;
    steamwebhelper.pid = 6001;
    steamwebhelper.comm = "steamwebhelper";
    steamwebhelper.cmdline = "/home/user/.steam/ubuntu12_64/steamwebhelper";
    mock_fs.add_process(steamwebhelper);

    MockProcFs::MockProcess epic;
    epic.pid = 6002;
    epic.comm = "EpicGamesLauncher";
    epic.cmdline = "EpicGamesLauncher.exe";
    mock_fs.add_process(epic);

    MockProcFs::MockProcess bnet;
    bnet.pid = 6003;
    bnet.comm = "Battle.net.exe";
    bnet.cmdline = "Battle.net.exe --type=renderer";
    mock_fs.add_process(bnet);

    GameDetector detector({}, &mock_fs);
    // Emulators detected
    assert(detector.is_game(5001));
    auto emu1 = detector.inspect_process(5001);
    assert(emu1 && emu1->is_emulator && emu1->runner_type == "emulator");

    assert(detector.is_game(5002));

    // Launchers NOT detected
    assert(!detector.is_game(6001));
    assert(!detector.is_game(6002));
    assert(!detector.is_game(6003));

    std::cout << "[PASS] test_emulators_and_launchers" << std::endl;
}

// Test 4: Real Game Heuristics (DRM Render Node & Vulkan/GL)
void test_real_game_heuristics() {
    MockProcFs mock_fs;

    // Process holding GPU render node
    MockProcFs::MockProcess custom_game;
    custom_game.pid = 7001;
    custom_game.comm = "indie_game_x86_";
    custom_game.cmdline = "./indie_game_x86_64";
    custom_game.open_fds = {"/dev/dri/renderD128", "/tmp/game.lock"};
    mock_fs.add_process(custom_game);

    // Process without GPU node and not a game
    MockProcFs::MockProcess regular_app;
    regular_app.pid = 7002;
    regular_app.comm = "python3";
    regular_app.cmdline = "python3 script.py";
    regular_app.open_fds = {"/dev/null", "/home/user/file.txt"};
    mock_fs.add_process(regular_app);

    GameDetector detector({}, &mock_fs);
    assert(detector.is_game(7001));
    assert(!detector.is_game(7002));

    std::cout << "[PASS] test_real_game_heuristics" << std::endl;
}

// Test 5: Concurrent Games & Lifecycle (Exec, Exit, Restart)
void test_concurrent_lifecycle() {
    MockProcFs mock_fs;

    MockProcFs::MockProcess gameA;
    gameA.pid = 8001;
    gameA.comm = "dota2";
    gameA.cmdline = "./dota2";
    gameA.environ = "SteamAppId=570\0";
    mock_fs.add_process(gameA);

    MockProcFs::MockProcess gameB;
    gameB.pid = 8002;
    gameB.comm = "factorio";
    gameB.cmdline = "./factorio";
    gameB.environ = "SteamAppId=427520\0";
    mock_fs.add_process(gameB);

    GameDetector detector({}, &mock_fs);
    detector.set_debounce_ms(150);
    detector.set_hysteresis_ms(600);
    assert(detector.debounce_ms() == 150);
    assert(detector.hysteresis_ms() == 600);

    auto all_games = detector.scan_all_games();
    assert(all_games.size() == 2);

    // Test exec handling
    assert(detector.handle_process_exec(8001));
    assert(detector.handle_process_exec(8002));
    assert(detector.has_active_game());
    assert(detector.active_games().size() == 2);

    // Test gameA exit
    assert(detector.handle_process_exit(8001));
    assert(detector.has_active_game()); // gameB still running
    assert(detector.active_games().size() == 1);

    // Test gameA restart with new PID 8003
    MockProcFs::MockProcess gameA_restarted = gameA;
    gameA_restarted.pid = 8003;
    mock_fs.add_process(gameA_restarted);
    assert(detector.handle_process_exec(8003));
    assert(detector.active_games().size() == 2);

    // Test both exit
    assert(detector.handle_process_exit(8002));
    assert(detector.handle_process_exit(8003));
    assert(!detector.has_active_game());

    std::cout << "[PASS] test_concurrent_lifecycle" << std::endl;
}

// Test 6: Thread Classification & Anti-Cheat Exclusion
void test_thread_classification_and_anticheat() {
    // 1. Thread role classification
    assert(ProcessTuner::classify_thread(100, 100, "game_main") == ThreadRole::Main);
    assert(ProcessTuner::classify_thread(101, 100, "RenderThread") == ThreadRole::Render);
    assert(ProcessTuner::classify_thread(102, 100, "VkQueue-0") == ThreadRole::Render);
    assert(ProcessTuner::classify_thread(103, 100, "AudioMixer") == ThreadRole::Audio);
    assert(ProcessTuner::classify_thread(104, 100, "ShaderCompile-1") == ThreadRole::ShaderCompile);
    assert(ProcessTuner::classify_thread(105, 100, "AsyncIoPool-0") == ThreadRole::IoWorker);

    // Anti-cheat and overlay threads MUST be identified and protected
    assert(ProcessTuner::classify_thread(106, 100, "EasyAntiCheat") == ThreadRole::AnticheatOrOverlay);
    assert(ProcessTuner::classify_thread(107, 100, "BEService_thread") == ThreadRole::AnticheatOrOverlay);
    assert(ProcessTuner::classify_thread(108, 100, "DiscordHook") == ThreadRole::AnticheatOrOverlay);
    assert(ProcessTuner::classify_thread(109, 100, "OBS_capture") == ThreadRole::AnticheatOrOverlay);

    // 2. Mock process thread inspection and fine-grained tuning
    MockProcFs mock_fs;
    MockProcFs::MockProcess game;
    game.pid = 9000;
    game.comm = "EldenRing.exe";
    game.cmdline = "eldenring.exe";
    game.tasks = {9000, 9001, 9002, 9003, 9004};
    game.task_comms[9000] = "EldenRing.exe";
    game.task_comms[9001] = "D3D12Render";
    game.task_comms[9002] = "AudioThread";
    game.task_comms[9003] = "ShaderCompiler";
    game.task_comms[9004] = "EasyAntiCheat_x";
    mock_fs.add_process(game);

    auto threads = ProcessTuner::inspect_threads(9000, &mock_fs);
    assert(threads.size() == 5);
    assert(threads[0].role == ThreadRole::Main);
    assert(threads[1].role == ThreadRole::Render);
    assert(threads[2].role == ThreadRole::Audio);
    assert(threads[3].role == ThreadRole::ShaderCompile);
    assert(threads[4].role == ThreadRole::AnticheatOrOverlay);

    // Fine-grained tuning call
    std::vector<int> perf_cores = {0, 1, 2, 3};
    std::vector<int> all_cores = {0, 1, 2, 3, 4, 5, 6, 7};
    bool ok = ProcessTuner::tune_process_fine_grained(9000, perf_cores, all_cores, &mock_fs);
    assert(ok);

    std::cout << "[PASS] test_thread_classification_and_anticheat" << std::endl;
}

// Test 7: Background Process Manager (Whitelist & Blacklist)
void test_background_manager() {
    BackgroundManager bg;

    // Whitelist tests (Never touch)
    assert(bg.is_whitelisted("kwin_wayland", "/usr/bin/kwin_wayland"));
    assert(bg.is_whitelisted("pipewire", "/usr/bin/pipewire"));
    assert(bg.is_whitelisted("wireplumber", "/usr/bin/wireplumber"));
    assert(bg.is_whitelisted("systemd-journald", "/usr/lib/systemd/systemd-journald"));
    assert(bg.is_whitelisted("bash", "/bin/bash"));
    assert(bg.is_whitelisted("tmux", "tmux"));
    assert(bg.is_whitelisted("alacritty", "alacritty"));
    assert(bg.classify_process("kwin_wayland", "") == ProcessCategory::Whitelisted);

    // Blacklist tests (Throttle)
    assert(bg.is_blacklisted("chrome", "/opt/google/chrome/chrome"));
    assert(bg.is_blacklisted("firefox", "/usr/lib/firefox/firefox"));
    assert(bg.is_blacklisted("discord", "/opt/discord/Discord"));
    assert(bg.is_blacklisted("spotify", "/usr/share/spotify/spotify"));
    assert(bg.is_blacklisted("qbittorrent", "qbittorrent"));
    assert(bg.classify_process("discord", "") == ProcessCategory::Blacklisted);

    // Neutral
    assert(bg.classify_process("my_custom_script", "./run.sh") == ProcessCategory::Neutral);

    std::cout << "[PASS] test_background_manager" << std::endl;
}

// Test 8: Fake /proc Stress Test: 1000 Processes, Fork Storm, Zero False Positives
void test_fake_proc_stress_and_false_positives() {
    MockProcFs mock_fs;

    // Add 100 standard system daemons & tools
    std::vector<std::string> daemons = {
        "systemd", "systemd-udevd", "systemd-resolved", "dbus-broker",
        "sshd", "polkitd", "NetworkManager", "cron", "rsyslogd",
        "pipewire", "wireplumber", "kwin_wayland", "Xwayland", "bash",
        "zsh", "tmux", "htop", "glslc", "gcc", "clang", "cmake", "ninja"
    };

    int pid = 100;
    for (const auto& d : daemons) {
        MockProcFs::MockProcess p;
        p.pid = pid++;
        p.comm = d;
        p.cmdline = "/usr/bin/" + d;
        mock_fs.add_process(p);
    }

    // Add 2 real games in between
    MockProcFs::MockProcess g1;
    g1.pid = pid++;
    g1.comm = "witcher3.exe";
    g1.cmdline = "wine64 witcher3.exe";
    g1.environ = "SteamAppId=292030\0";
    g1.open_fds = {"/dev/dri/renderD128"};
    mock_fs.add_process(g1);

    MockProcFs::MockProcess g2;
    g2.pid = pid++;
    g2.comm = "Hades2";
    g2.cmdline = "./Hades2";
    g2.environ = "SteamAppId=1145350\0";
    g2.open_fds = {"/dev/dri/renderD128"};
    mock_fs.add_process(g2);

    // Add 1000 background processes (simulating heavy desktop load: browser workers, node apps, etc.)
    for (int i = 0; i < 1000; ++i) {
        MockProcFs::MockProcess bg_proc;
        bg_proc.pid = pid++;
        bg_proc.comm = (i % 2 == 0) ? "chrome" : "node";
        bg_proc.cmdline = (i % 2 == 0) ? "/opt/google/chrome/chrome --type=renderer" : "/usr/bin/node server.js";
        mock_fs.add_process(bg_proc);
    }

    assert(mock_fs.list_pids().size() >= 1000);

    GameDetector detector({}, &mock_fs);

    // Initial sweep
    auto detected = detector.scan_all_games();
    // Exactly the 2 real games must be detected! Zero false positives across 1000+ processes!
    assert(detected.size() == 2);
    assert(detected[0].steam_appid == "292030" || detected[0].steam_appid == "1145350");
    assert(detected[1].steam_appid == "292030" || detected[1].steam_appid == "1145350");

    // Rapid fork/exit storm simulation
    for (int i = 0; i < 500; ++i) {
        int temp_pid = 20000 + i;
        MockProcFs::MockProcess fork_proc;
        fork_proc.pid = temp_pid;
        fork_proc.comm = "worker_fork";
        fork_proc.cmdline = "./fork_child";
        mock_fs.add_process(fork_proc);

        // Notify detector
        detector.handle_process_exec(temp_pid);

        // Immediately terminate
        mock_fs.remove_process(temp_pid);
        detector.handle_process_exit(temp_pid);
    }

    // State must remain strictly clean
    assert(detector.scan_all_games().size() == 2);

    std::cout << "[PASS] test_fake_proc_stress_and_false_positives" << std::endl;
}

// Test 9: Netlink Proc Connector Mock Event Injection
void test_proc_connector_injection() {
    ProcConnector connector;
    std::vector<ProcEvent> received_events;

    bool started = connector.start([&](const ProcEvent& ev) {
        received_events.push_back(ev);
    });
    assert(started);
    assert(connector.is_running());

    // Inject synthetic events
    ProcEvent fork_ev;
    fork_ev.type = ProcEventType::Fork;
    fork_ev.pid = 1234;
    fork_ev.ppid = 1000;
    connector.inject_event(fork_ev);

    ProcEvent exec_ev;
    exec_ev.type = ProcEventType::Exec;
    exec_ev.pid = 1234;
    connector.inject_event(exec_ev);

    ProcEvent exit_ev;
    exit_ev.type = ProcEventType::Exit;
    exit_ev.pid = 1234;
    exit_ev.exit_code = 0;
    connector.inject_event(exit_ev);

    assert(received_events.size() == 3);
    assert(received_events[0].type == ProcEventType::Fork && received_events[0].pid == 1234);
    assert(received_events[1].type == ProcEventType::Exec && received_events[1].pid == 1234);
    assert(received_events[2].type == ProcEventType::Exit && received_events[2].pid == 1234);

    connector.stop();
    assert(!connector.is_running());

    std::cout << "[PASS] test_proc_connector_injection" << std::endl;
}

int main() {
    std::cout << "Starting Stage 5 Game Detector & Process Tuner Unit Tests..." << std::endl;

    test_steam_detection();
    test_wine_and_wineserver_filtering();
    test_emulators_and_launchers();
    test_real_game_heuristics();
    test_concurrent_lifecycle();
    test_thread_classification_and_anticheat();
    test_background_manager();
    test_fake_proc_stress_and_false_positives();
    test_proc_connector_injection();

    std::cout << "ALL 9 STAGE 5 GAME DETECTOR & PROCESS TUNER TESTS PASSED!" << std::endl;
    return 0;
}
