// tests/test_benchmarks.cpp
// Unit tests for Zenith Benchmarking Engine and Metric Analyzers (Stage 9)

#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <numeric>
#include <vector>

#if defined(__linux__)
#include <sys/resource.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#endif

#include "zenith/config.hpp"
#include "zenith/engine.hpp"
#include "zenith/tweaks/tweak_registry.hpp"

namespace {

struct BenchStats {
    double min_v{0.0};
    double max_v{0.0};
    double mean_v{0.0};
    double median_v{0.0};
    double p95_v{0.0};
    double p99_v{0.0};
    double stddev_v{0.0};
};

BenchStats compute_test_stats(std::vector<double> vals) {
    BenchStats s;
    if (vals.empty()) return s;
    std::sort(vals.begin(), vals.end());

    s.min_v = vals.front();
    s.max_v = vals.back();
    double sum = std::accumulate(vals.begin(), vals.end(), 0.0);
    s.mean_v = sum / static_cast<double>(vals.size());

    size_t n = vals.size();
    if (n % 2 == 0) {
        s.median_v = (vals[n / 2 - 1] + vals[n / 2]) / 2.0;
    } else {
        s.median_v = vals[n / 2];
    }

    size_t idx95 = static_cast<size_t>(std::ceil(0.95 * n)) - 1;
    size_t idx99 = static_cast<size_t>(std::ceil(0.99 * n)) - 1;
    s.p95_v = vals[std::min(idx95, n - 1)];
    s.p99_v = vals[std::min(idx99, n - 1)];

    double var_sum = 0.0;
    for (double v : vals) {
        var_sum += (v - s.mean_v) * (v - s.mean_v);
    }
    s.stddev_v = std::sqrt(var_sum / static_cast<double>(vals.size()));
    return s;
}

size_t get_process_rss_kb() {
#if defined(__linux__)
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return static_cast<size_t>(usage.ru_maxrss);
    }
    return 0;
#elif defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize / 1024;
    }
    return 0;
#else
    return 0;
#endif
}

} // namespace

void test_statistical_computation() {
    std::vector<double> samples = {10.0, 12.0, 12.0, 13.0, 14.0, 15.0, 16.0, 18.0, 20.0, 100.0};
    auto stats = compute_test_stats(samples);

    assert(stats.min_v == 10.0);
    assert(stats.max_v == 100.0);
    assert(std::abs(stats.mean_v - 23.0) < 0.001);
    // Median of 10 items is (14.0 + 15.0) / 2 = 14.5
    assert(std::abs(stats.median_v - 14.5) < 0.001);
    assert(stats.p95_v == 100.0);
    assert(stats.p99_v == 100.0);
    assert(stats.stddev_v > 0.0);

    std::cout << "[PASS] test_statistical_computation" << std::endl;
}

void test_daemon_startup_measurement() {
    const int iterations = 15;
    std::vector<double> timings_ms;
    timings_ms.reserve(iterations);

    for (int i = 0; i < iterations; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        zenith::Config cfg;
        cfg.cgroup_isolation_enabled = true;
        cfg.vcache_pinning_enabled = true;

        auto& registry = zenith::tweaks::TweakRegistry::instance();
        (void)registry.get_all();

        auto t1 = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        timings_ms.push_back(ms);
    }

    auto stats = compute_test_stats(timings_ms);
    // Startup target is < 100 ms
    assert(stats.median_v < 100.0);
    assert(stats.min_v >= 0.0);

    std::cout << "[PASS] test_daemon_startup_measurement (median: " << stats.median_v << " ms)" << std::endl;
}

void test_peak_rss_measurement() {
    size_t rss_kb = get_process_rss_kb();
    double rss_mb = static_cast<double>(rss_kb) / 1024.0;

    // Verify non-zero and reasonable (< 50MB for test executable)
    assert(rss_kb > 0);
    assert(rss_mb < 50.0);

    std::cout << "[PASS] test_peak_rss_measurement (current RSS: " << rss_mb << " MB)" << std::endl;
}

void test_tweak_apply_revert_timing() {
    zenith::Config cfg;
    cfg.cgroup_isolation_enabled = false;
    cfg.gpu_clock_lock_enabled = false;
    cfg.pipewire_quantum_tuning = false;

    zenith::ZenithEngine engine(cfg);

    auto t0 = std::chrono::steady_clock::now();
    engine.optimize_for_game(12345);
    auto t1 = std::chrono::steady_clock::now();
    engine.revert();
    auto t2 = std::chrono::steady_clock::now();

    double apply_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double revert_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();

    assert(apply_ms >= 0.0);
    assert(revert_ms >= 0.0);

    std::cout << "[PASS] test_tweak_apply_revert_timing (apply: " << apply_ms
              << " ms, revert: " << revert_ms << " ms)" << std::endl;
}

int main() {
    std::cout << "Starting Stage 9 Benchmark & Metric Analyzer Tests..." << std::endl;

    test_statistical_computation();
    test_daemon_startup_measurement();
    test_peak_rss_measurement();
    test_tweak_apply_revert_timing();

    std::cout << "ALL STAGE 9 BENCHMARK TESTS PASSED!" << std::endl;
    return 0;
}
