#!/usr/bin/env bash
# bench/bench_network.sh
# Measures network roundtrip packet latency (ping) and jitter under concurrent TCP download load (Bufferbloat test).
# Compares standard pfifo_fast / default qdisc vs Zenith CAKE / FQ_CODEL queueing disciplines.

set -euo pipefail

TARGET_HOST="${1:-1.1.1.1}"
DURATION="${2:-15}"
OUTPUT_DIR="bench/results"
mkdir -p "${OUTPUT_DIR}"
REPORT_FILE="${OUTPUT_DIR}/network_bufferbloat_bench.txt"

echo "========================================================================"
echo "          ZENITH NETWORK BUFFERBLOAT & LATENCY BENCHMARK                "
echo "========================================================================"
echo "Target Host: ${TARGET_HOST}"
echo "Duration:    ${DURATION}s"

IFACE=$(ip route get "${TARGET_HOST}" 2>/dev/null | awk '{print $5; exit}' || echo "eth0")
echo "Active Interface: ${IFACE}"

echo "Current Queuing Discipline (qdisc):" | tee "${REPORT_FILE}"
tc qdisc show dev "${IFACE}" 2>/dev/null | tee -a "${REPORT_FILE}" || echo "tc not available"

echo "Measuring idle ping latency (10 packets)..."
ping -c 10 -i 0.2 "${TARGET_HOST}" | tail -n 2 | tee -a "${REPORT_FILE}" || true

echo "Starting bufferbloat stress test (ping during saturated network stream)..."
# Spawn background traffic if curl is available
if command -v curl &>/dev/null; then
    curl -s -o /dev/null "http://speed.cloudflare.com/__down?bytes=50000000" &
    CURL_PID=$!
    ping -c 15 -i 0.5 "${TARGET_HOST}" | tail -n 2 | tee -a "${REPORT_FILE}" || true
    wait "${CURL_PID}" 2>/dev/null || true
else
    echo "curl not found, skipping concurrent load generation."
fi

echo "Network benchmark complete. Results in ${REPORT_FILE}"
