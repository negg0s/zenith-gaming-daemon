#!/usr/bin/env bash
# bench/run_benchmarks.sh
# Universal single-command entrypoint to run Zenith benchmark suite, compute statistics, and render charts.
# Usage: ./bench/run_benchmarks.sh [all|daemon|ablation|quick]

set -euo pipefail

MODE="${1:-quick}"
RESULTS_DIR="bench/results"
mkdir -p "${RESULTS_DIR}"

echo "========================================================================"
echo "          ZENITH GAMING DAEMON: UNIVERSAL BENCHMARK SUITE               "
echo "========================================================================"
echo "Execution Mode: ${MODE}"
echo "Results Dir:    ${RESULTS_DIR}"
echo "Date:           $(date)"
echo "Kernel:         $(uname -r 2>/dev/null || echo 'N/A')"
echo "========================================================================\n"

# Step 1: Run C++20 Daemon Resource & Latency Microbenchmark
echo "[1/4] Executing C++20 Daemon Microbenchmark..."
if [ -f "build/zenith_bench_daemon" ]; then
    ./build/zenith_bench_daemon | tee "${RESULTS_DIR}/daemon_metrics.txt"
    ./build/zenith_bench_daemon --json > "${RESULTS_DIR}/daemon_metrics.json"
elif [ -f "build/Release/zenith_bench_daemon" ]; then
    ./build/Release/zenith_bench_daemon | tee "${RESULTS_DIR}/daemon_metrics.txt"
    ./build/Release/zenith_bench_daemon --json > "${RESULTS_DIR}/daemon_metrics.json"
else
    echo "[INFO] Binary 'zenith_bench_daemon' not built yet. Proceeding with script-based benchmarks."
fi

# Step 2: Run Python A/B and Ablation Sweep
echo "\n[2/4] Running A/B & Ablation Sweep (mode: ${MODE})..."
if command -v python3 &>/dev/null; then
    python3 bench/runner.py --mode "${MODE}" --synthetic --output-dir "${RESULTS_DIR}"
    
    # Step 3: Render Matplotlib Charts
    echo "\n[3/4] Rendering Charts..."
    python3 bench/plot_results.py --output-dir "${RESULTS_DIR}" || true
else
    echo "[WARN] python3 not found. Skipping statistical runner."
fi

# Step 4: Run Subsystem Probes
echo "\n[4/4] Executing Subsystem Latency Probes..."
bash bench/bench_pipewire.sh || true
bash bench/bench_network.sh "1.1.1.1" 5 || true

echo "\n========================================================================"
echo "                  BENCHMARK SUITE COMPLETE                              "
echo "========================================================================"
echo "Results and figures generated in: ${RESULTS_DIR}/"
ls -la "${RESULTS_DIR}" 2>/dev/null || dir "${RESULTS_DIR}"
echo "========================================================================"
