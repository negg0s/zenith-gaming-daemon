#include "zenith/engine.hpp"
#include <cassert>
#include <iostream>

void test_engine_lifecycle() {
    auto cfg = zenith::Config::default_config();
    cfg.poll_interval_ms = 10;
    zenith::ZenithEngine engine(cfg);
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

    std::cout << "[PASS] test_engine_lifecycle\n";
}

int main() {
    test_engine_lifecycle();
    return 0;
}
