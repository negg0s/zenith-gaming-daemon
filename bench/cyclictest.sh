#!/usr/bin/env bash
# bench/cyclictest.sh
# Measures CPU thread scheduling latency and jitter using cyclictest (rt-tests).
# Compares standard Linux kernel scheduler latency against Zenith-optimized priorities.

set -euo pipefail

DURATION="${1:-30s}"
THREADS="$(nproc)"
OUTPUT_DIR="bench/results"
mkdir -p "${OUTPUT_DIR}"

echo "========================================================================"
echo "          ZENITH SCHEDULING LATENCY BENCHMARK (cyclictest)              "
echo "========================================================================"
echo "Duration: ${DURATION}"
echo "Threads:  ${THREADS}"

if ! command -v cyclictest &>/dev/null; then
    echo "[NOTICE] 'cyclictest' is not installed."
    echo "Install via: sudo pacman -S rt-tests (Arch/CachyOS) or sudo apt-get install rt-tests (Debian/Ubuntu)"
    echo "Logging placeholder benchmark template to ${OUTPUT_DIR}/cyclictest_results.txt..."
    cat <<EOF > "${OUTPUT_DIR}/cyclictest_results.txt"
# cyclictest benchmark template
# Tool: rt-tests (cyclictest)
# Command: sudo cyclictest -m -Sp90 -i1000 -h100 -q -D ${DURATION} -t ${THREADS}
# Baseline Max Latency: [MEASURE ON BARE METAL] us
# Zenith Max Latency:   [MEASURE ON BARE METAL] us
EOF
    exit 0
fi

echo "Running cyclictest for ${DURATION} with priority 90..."
sudo cyclictest -m -Sp90 -i1000 -h100 -q -D "${DURATION}" -t "${THREADS}" | tee "${OUTPUT_DIR}/cyclictest_results.txt"

echo "Latency results saved to ${OUTPUT_DIR}/cyclictest_results.txt"
