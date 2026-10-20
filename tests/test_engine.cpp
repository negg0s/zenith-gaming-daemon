#include "zenith/engine.hpp"
#include <cassert>
#include <iostream>

namespace fs = std::filesystem;

void test_engine_lifecycle() {
    fs::path test_run = fs::current_path() / "test_run_engine";
    std::error_code ec;
    fs::remove_all(test_run, ec);
    fs::create_directories(test_run, ec);

    auto cfg = zenith::Config::default_config();
    cfg.poll_interval_ms = 10;
    zenith::ZenithEngine engine(cfg, test_run);
    bool init_ok = engine.initialize();
    assert(init_ok);

    zenith::DetectedGame mock_game;
    mock_game.pid = 999999;
    mock_game.name = "TestBenchmarkGame";

    bool act_ok = engine.activate(mock_game);
    assert(act_ok);
    assert(engine.is_game_active());

    engine.deactivate();
    assert(!engine.is_game_active());

    fs::remove_all(test_run, ec);
    std::cout << "[PASS] test_engine_lifecycle\n";
}

int main() {
    test_engine_lifecycle();
    return 0;
}
