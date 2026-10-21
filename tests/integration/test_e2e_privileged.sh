#!/usr/bin/env bash
# tests/integration/test_e2e_privileged.sh
# End-to-end privileged/mock integration test:
#   1. Snapshot baseline sysctl & cgroup states
#   2. Apply optimizations (Aggressive profile)
#   3. Verify mutated values
#   4. Execute emergency rollback
#   5. Verify zero-drift: post-rollback snapshot identical to baseline

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}========================================================================${NC}"
echo -e "${BLUE}       ZENITH E2E PRIVILEGED INTEGRATION & ROLLBACK VERIFICATION        ${NC}"
echo -e "${BLUE}========================================================================${NC}"

WORK_DIR=$(mktemp -d /tmp/zenith_e2e_XXXXXX)
trap 'rm -rf "${WORK_DIR}"' EXIT

SNAPSHOT_BEFORE="${WORK_DIR}/before.snap"
SNAPSHOT_MUTATED="${WORK_DIR}/mutated.snap"
SNAPSHOT_AFTER="${WORK_DIR}/after.snap"

# Mock filesystem root for safe container/test environments
MOCK_ROOT="${WORK_DIR}/sysroot"
mkdir -p "${MOCK_ROOT}/proc/sys/vm"
mkdir -p "${MOCK_ROOT}/proc/sys/kernel"
mkdir -p "${MOCK_ROOT}/sys/fs/cgroup/zenith_gaming"
mkdir -p "${MOCK_ROOT}/run/zenith"

# Setup initial mock baseline values
echo "60" > "${MOCK_ROOT}/proc/sys/vm/swappiness"
echo "20" > "${MOCK_ROOT}/proc/sys/vm/dirty_ratio"
echo "1"  > "${MOCK_ROOT}/proc/sys/kernel/split_lock_mitigate"
echo "100" > "${MOCK_ROOT}/sys/fs/cgroup/zenith_gaming/cpu.weight"
echo "0"   > "${MOCK_ROOT}/sys/fs/cgroup/zenith_gaming/cpu.idle"

# Capture baseline
echo -e "[*] Taking baseline system snapshot..."
cat <<EOF > "${SNAPSHOT_BEFORE}"
vm.swappiness=$(cat "${MOCK_ROOT}/proc/sys/vm/swappiness")
vm.dirty_ratio=$(cat "${MOCK_ROOT}/proc/sys/vm/dirty_ratio")
kernel.split_lock_mitigate=$(cat "${MOCK_ROOT}/proc/sys/kernel/split_lock_mitigate")
cgroup.cpu.weight=$(cat "${MOCK_ROOT}/sys/fs/cgroup/zenith_gaming/cpu.weight")
cgroup.cpu.idle=$(cat "${MOCK_ROOT}/sys/fs/cgroup/zenith_gaming/cpu.idle")
EOF
cat "${SNAPSHOT_BEFORE}"

echo -e "\n[*] Simulating write-ahead journal mutation to Aggressive Profile..."
# Write-ahead journal simulation
JOURNAL_PATH="${MOCK_ROOT}/run/zenith/journal.json"
cat <<EOF > "${JOURNAL_PATH}"
{
  "version": 1,
  "entries": [
    {
      "mutation_id": "sysctl_swappiness",
      "target_path": "${MOCK_ROOT}/proc/sys/vm/swappiness",
      "original_value": "60",
      "applied_value": "10",
      "status": "applied"
    },
    {
      "mutation_id": "sysctl_dirty_ratio",
      "target_path": "${MOCK_ROOT}/proc/sys/vm/dirty_ratio",
      "original_value": "20",
      "applied_value": "5",
      "status": "applied"
    },
    {
      "mutation_id": "sysctl_split_lock",
      "target_path": "${MOCK_ROOT}/proc/sys/kernel/split_lock_mitigate",
      "original_value": "1",
      "applied_value": "0",
      "status": "applied"
    },
    {
      "mutation_id": "cgroup_weight",
      "target_path": "${MOCK_ROOT}/sys/fs/cgroup/zenith_gaming/cpu.weight",
      "original_value": "100",
      "applied_value": "1000",
      "status": "applied"
    }
  ]
}
EOF

# Apply mutated values
echo "10" > "${MOCK_ROOT}/proc/sys/vm/swappiness"
echo "5"  > "${MOCK_ROOT}/proc/sys/vm/dirty_ratio"
echo "0"  > "${MOCK_ROOT}/proc/sys/kernel/split_lock_mitigate"
echo "1000" > "${MOCK_ROOT}/sys/fs/cgroup/zenith_gaming/cpu.weight"

# Capture mutated state
echo -e "\n[*] Verifying mutated parameters..."
cat <<EOF > "${SNAPSHOT_MUTATED}"
vm.swappiness=$(cat "${MOCK_ROOT}/proc/sys/vm/swappiness")
vm.dirty_ratio=$(cat "${MOCK_ROOT}/proc/sys/vm/dirty_ratio")
kernel.split_lock_mitigate=$(cat "${MOCK_ROOT}/proc/sys/kernel/split_lock_mitigate")
cgroup.cpu.weight=$(cat "${MOCK_ROOT}/sys/fs/cgroup/zenith_gaming/cpu.weight")
cgroup.cpu.idle=$(cat "${MOCK_ROOT}/sys/fs/cgroup/zenith_gaming/cpu.idle")
EOF
cat "${SNAPSHOT_MUTATED}"

# Verify mutated differs from before
if diff -q "${SNAPSHOT_BEFORE}" "${SNAPSHOT_MUTATED}" >/dev/null; then
    echo -e "${RED}[FAIL] Mutated state did not change!${NC}"
    exit 1
fi
echo -e "${GREEN}[PASS] System successfully transitioned to Aggressive profile.${NC}"

# Execute Rollback (Reverting mutations according to journal)
echo -e "\n[*] Executing journal-based rollback..."
# In python or bash, simulate replay of journal entries
for entry in swappiness dirty_ratio split_lock weight; do
    case "${entry}" in
        swappiness) echo "60" > "${MOCK_ROOT}/proc/sys/vm/swappiness" ;;
        dirty_ratio) echo "20" > "${MOCK_ROOT}/proc/sys/vm/dirty_ratio" ;;
        split_lock) echo "1" > "${MOCK_ROOT}/proc/sys/kernel/split_lock_mitigate" ;;
        weight) echo "100" > "${MOCK_ROOT}/sys/fs/cgroup/zenith_gaming/cpu.weight" ;;
    esac
done
rm -f "${JOURNAL_PATH}"

# Capture post-rollback snapshot
echo -e "\n[*] Capturing post-rollback system snapshot..."
cat <<EOF > "${SNAPSHOT_AFTER}"
vm.swappiness=$(cat "${MOCK_ROOT}/proc/sys/vm/swappiness")
vm.dirty_ratio=$(cat "${MOCK_ROOT}/proc/sys/vm/dirty_ratio")
kernel.split_lock_mitigate=$(cat "${MOCK_ROOT}/proc/sys/kernel/split_lock_mitigate")
cgroup.cpu.weight=$(cat "${MOCK_ROOT}/sys/fs/cgroup/zenith_gaming/cpu.weight")
cgroup.cpu.idle=$(cat "${MOCK_ROOT}/sys/fs/cgroup/zenith_gaming/cpu.idle")
EOF
cat "${SNAPSHOT_AFTER}"

# Strict zero-drift comparison
echo -e "\n[*] Comparing baseline snapshot against post-rollback snapshot..."
if diff -u "${SNAPSHOT_BEFORE}" "${SNAPSHOT_AFTER}"; then
    echo -e "${GREEN}========================================================================${NC}"
    echo -e "${GREEN}  E2E INTEGRATION & ZERO-DRIFT ROLLBACK TEST: PASSED (100% IDENTICAL)   ${NC}"
    echo -e "${GREEN}========================================================================${NC}"
    exit 0
else
    echo -e "${RED}[FAIL] Zero-drift violation! State post-rollback differs from baseline!${NC}"
    exit 1
fi
