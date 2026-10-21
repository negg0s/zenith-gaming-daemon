#!/usr/bin/env bash
# bench/bench_daemon.sh
# Empirical measurement of Zenith Gaming Daemon resource consumption:
# - Idle CPU utilization (Target: < 0.1% CPU)
# - Peak Resident Set Size (Target: < 15 MB RSS)
# - Event loop poll efficiency (signalfd / epoll)

set -euo pipefail

OUTPUT_DIR="bench/results"
mkdir -p "${OUTPUT_DIR}"
REPORT_FILE="${OUTPUT_DIR}/daemon_footprint.txt"

echo "========================================================================"
echo "          ZENITH DAEMON RUNTIME RESOURCE PROFILER                       "
echo "========================================================================"

DAEMON_PID=$(pgrep -x zenith 2>/dev/null || echo "")

if [ -z "${DAEMON_PID}" ]; then
    echo "[INFO] Running 'zenith_bench_daemon' microbenchmark binary..."
    if [ -f "build/zenith_bench_daemon" ]; then
        ./build/zenith_bench_daemon | tee "${REPORT_FILE}"
        exit 0
    else
        echo "[NOTICE] Neither running 'zenith' daemon nor 'zenith_bench_daemon' found."
        echo "Build first with: cmake -B build && cmake --build build --target zenith_bench_daemon"
        exit 0
    fi
fi

echo "Profiling active Zenith daemon (PID: ${DAEMON_PID}) for 10 seconds..." | tee "${REPORT_FILE}"

# Sample CPU and RSS using ps over 10 seconds
SAMPLES=10
TOTAL_CPU=0
MAX_RSS_KB=0

for i in $(seq 1 "${SAMPLES}"); do
    STATS=$(ps -p "${DAEMON_PID}" -o %cpu,rss --no-headers 2>/dev/null || echo "0.0 0")
    CPU=$(echo "${STATS}" | awk '{print $1}')
    RSS=$(echo "${STATS}" | awk '{print $2}')
    
    TOTAL_CPU=$(echo "${TOTAL_CPU} + ${CPU}" | bc 2>/dev/null || echo "0.0")
    if [ "${RSS}" -gt "${MAX_RSS_KB}" ]; then
        MAX_RSS_KB="${RSS}"
    fi
    sleep 1
done

AVG_CPU=$(echo "scale=3; ${TOTAL_CPU} / ${SAMPLES}" | bc 2>/dev/null || echo "0.0")
MAX_RSS_MB=$(echo "scale=2; ${MAX_RSS_KB} / 1024" | bc 2>/dev/null || echo "0.0")

echo "------------------------------------------------------------------------" | tee -a "${REPORT_FILE}"
echo "Daemon Profiling Results (10s sustained sample):" | tee -a "${REPORT_FILE}"
echo "  Average Idle CPU: ${AVG_CPU}% (Target: < 0.1%)" | tee -a "${REPORT_FILE}"
echo "  Peak Memory RSS:  ${MAX_RSS_MB} MB (${MAX_RSS_KB} KB) (Target: < 15 MB)" | tee -a "${REPORT_FILE}"
echo "------------------------------------------------------------------------" | tee -a "${REPORT_FILE}"
