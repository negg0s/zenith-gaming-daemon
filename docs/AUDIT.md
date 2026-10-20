# Zenith Gaming Daemon: System Mutation & Rollback Audit

> **Document Version**: 1.0.0  
> **Target Platform**: Linux (Arch / CachyOS / SteamOS)  
> **Security Domain**: Root / `CAP_SYS_ADMIN`, `CAP_SYS_NICE`, `CAP_SYS_RESOURCE`

This document details every hardware, kernel, process, and driver mutation performed by `zenith-gaming-daemon`, its baseline origin, its atomic rollback procedure, and the failure consequence if the daemon crashes mid-operation.

---

## 1. System Mutation Matrix

| # | Subsystem & Target Path | Baseline Origin | Mutation Applied | Rollback Method | Crash Consequence (Without Journal) | Journal Record Type |
|---|-------------------------|-----------------|------------------|-----------------|--------------------------------------|---------------------|
| 1 | `/proc/sys/kernel/split_lock_mitigate` | Read string (`1` or `0`) | Write `0` | Write original string | Split-lock mitigation stays disabled until reboot | `SYSCTL_STRING` |
| 2 | `/sys/kernel/mm/transparent_hugepage/enabled` | Extract token inside `[...]` from read file | Write `always` | Write original token back | THP remains in `always` mode (increased memory footprint) | `SYSFS_TOKEN` |
| 3 | `/sys/kernel/mm/transparent_hugepage/defrag` | Extract token inside `[...]` from read file | Write `defer+madvise` | Write original token back | Defrag remains `defer+madvise` | `SYSFS_TOKEN` |
| 4 | `/proc/sys/vm/compaction_proactiveness` | Read integer string (e.g. `20`) | Write `0` | Write original string | Proactive memory compaction remains disabled | `SYSCTL_INT` |
| 5 | `/proc/sys/vm/dirty_ratio` | Read integer string (e.g. `20`) | Write `5` | Write original string | Background writeback threshold remains 5% | `SYSCTL_INT` |
| 6 | `/proc/sys/vm/swappiness` | Read integer string (e.g. `60` or `100`) | Write `10` | Write original string | Swap aggressiveness remains 10 | `SYSCTL_INT` |
| 7 | `/proc/sys/kernel/sched_autogroup_enabled` | Read integer string (`1` or `0`) | Write `0` | Write original string | Task autogrouping remains disabled | `SYSCTL_INT` |
| 8 | `/proc/sys/net/core/default_qdisc` | Read string (e.g. `fq_codel`) | Write `cake` | Write original string | Default network queue discipline remains `cake` | `SYSCTL_STRING` |
| 9 | `/sys/class/drm/card*/device/power_dpm_force_performance_level` | Read string (usually `auto`) | Write `high` | Write original string (or `auto`) | AMD GPU VRAM stays at high clock, consuming ~15W idle power | `GPU_AMD_DPM` |
| 10 | NVIDIA GPU Persistence & Clocks (`nvidia-smi`) | Query persistence mode (`Enabled`/`Disabled`) | Exec `nvidia-smi -pm 1`, lock boost | Exec `nvidia-smi -rgc`, `-pm 0` | NVIDIA GPU remains in persistence mode | `GPU_NVIDIA_SMI` |
| 11 | cgroups v2 background slice (`/sys/fs/cgroup/zenith_background`) | Query original cgroup of target PIDs | Create slice, set `cpuset.cpus`, move PIDs | Migrate PIDs back to parent cgroup, `rmdir` slice | Background apps (Discord, Chrome) remain restricted to core 0 | `CGROUP_SLICE` |
| 12 | Process Scheduling Policy (`sched_setscheduler`) | Query policy & priority via `sched_getscheduler` | `SCHED_RR` / `SCHED_FIFO` (pri 50) | `sched_setscheduler(pid, SCHED_OTHER, &param)` | Game threads retain RT priority until process exit | `PROC_SCHED` |
| 13 | Process Nice Value (`setpriority`) | `getpriority(PRIO_PROCESS, pid)` | `setpriority(PRIO_PROCESS, pid, -20)` | `setpriority(PRIO_PROCESS, pid, orig)` | Game retains nice -20 until process exit | `PROC_NICE` |
| 14 | Process I/O Priority (`ioprio_set`) | Query via `syscall(SYS_ioprio_get, ...)` | `IOPRIO_CLASS_RT` (level 4) | Restore original IOPRIO word | Game retains RT disk priority until process exit | `PROC_IOPRIO` |
| 15 | Process CPU Affinity (`sched_setaffinity`) | Query via `sched_getaffinity` | Mask of gaming cores (`0,1,2,3...`) | Restore original cpu_set_t | Game retains restricted CPU affinity until process exit | `PROC_AFFINITY` |
| 16 | PipeWire Quantum (`pw-metadata`) | Query `pw-metadata -n settings 0 clock.force-quantum` | Write quantum `64` | Write `0` (reset to dynamic) | PipeWire stays at low latency quantum | `PIPEWIRE_QUANTUM` |
| 17 | IRQ Affinity (`/proc/irq/<id>/smp_affinity_list`) | Read original CPU list from procfs | Steer to housekeeping cores | Write original CPU list back | Hardware IRQs stay steered to housekeeping cores | `IRQ_AFFINITY` |

---

## 2. Crash-Only Architecture & Write-Ahead Logging (WAL)

To prevent system state corruption during abrupt power loss, `SIGKILL`, kernel OOM-killer invocations, or unhandled exceptions:

```
                  ┌──────────────────────────────────────────────┐
                  │ 1. Read Current Baseline Value from System   │
                  └──────────────────────┬───────────────────────┘
                                         ▼
                  ┌──────────────────────────────────────────────┐
                  │ 2. WAL: Append PENDING entry to Journal      │
                  │    Atomic Write: tmp -> fsync -> rename      │
                  └──────────────────────┬───────────────────────┘
                                         ▼
                  ┌──────────────────────────────────────────────┐
                  │ 3. Apply System Mutation (sysfs/sysctl/etc.) │
                  └──────────────────────┬───────────────────────┘
                                         ▼
                  ┌──────────────────────────────────────────────┐
                  │ 4. WAL: Mark Entry as APPLIED in Journal     │
                  │    Atomic Write: tmp -> fsync -> rename      │
                  └──────────────────────────────────────────────┘
```

### Invariant Guarantees:
1. **Crash before Mutation**: The journal contains a `PENDING` entry. On next boot or restart, the recovery engine inspects the target. If target value does not match tuned value, no action needed; if it matches, original value is restored.
2. **Crash during Mutation**: Partial write either fails or succeeds. On startup, recovery engine restores the recorded baseline value.
3. **Crash after Mutation**: Entry is `APPLIED`. Recovery engine restores original baseline value and marks entry `REVERTED`.
4. **Clean Exit**: Regular daemon shutdown invokes the exact same recovery/rollback engine that processes journal entries.

---

## 3. Safe Mode Protection (Crash Loop Prevention)

If the daemon crashes $\ge 3$ times within a sliding window of $N = 10$ minutes:
- The daemon detects the crash storm via `/run/zenith/crash_history.json`.
- The daemon **refuses to apply aggressive kernel/GPU mutations**.
- The daemon starts in `SAFE_MODE` (read-only monitoring, logging warnings to systemd journal).
- Operators can reset Safe Mode via `zenith --reset-safemode`.

---

## 4. Single-Instance Protection (`flock`)

`/run/zenith/zenith.lock` is acquired at startup via `flock(fd, LOCK_EX | LOCK_NB)`. If locked:
- Secondary instance logs fatal error and exits immediately with code `2`.
- Prevents conflicting mutations or corrupted rollback sequences.
