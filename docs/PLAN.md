# Zenith Gaming Daemon: Master Implementation Plan

> **Project**: `zenith-gaming-daemon` (C++20, Linux, `D:\zenith-gaming-daemon`, GitHub: `NEGG0/zenith-gaming-daemon`)  
> **Status**: Active Development  
> **Protocol**: Autonomous Senior Engineer State Protocol (Strict Quality Gates)

---

## Quality Gate Checklist (Every Task)
- [ ] Build clean on GCC (0 warnings with `-Wall -Wextra -Wpedantic`)
- [ ] Build clean on Clang (0 warnings)
- [ ] All unit/integration tests pass
- [ ] Clean under ASan (AddressSanitizer) and UBSan (UndefinedBehaviorSanitizer)
- [ ] `clang-tidy` checks pass cleanly
- [ ] Documentation updated (`README.md`, manpages, or inline API)
- [ ] Log entry in `docs/PROGRESS.md` with verifiable terminal output
- [ ] Conventional Commit pushed to repository

---

## 12-Stage Roadmap

### [ ] Stage 1: Subsystem Architecture & Mutation Protocol (P0)
- [ ] Define universal `ISubsystem` interface (`probe()`, `apply()`, `revert()`, `describe()`)
- [ ] Implement Subsystem Registry and RAII Rollback Stack (atomic rollback on partial failures)
- [ ] Implement mock sysfs/procfs test fixture framework for rootless and cross-platform verification
- [ ] Write comprehensive unit tests for rollback mechanics under simulated failures

### [ ] Stage 2: Hardware Discovery & CPU Topology Engine (P0)
- [ ] Sysfs parser for L1/L2/L3 cache sizes, NUMA domains, physical packages, core IDs
- [ ] Reliable AMD 3D V-Cache CCD identification via cache asymmetry heuristic & sysfs
- [ ] Intel Hybrid (P-core vs E-core) identification via max frequency & core type
- [ ] Test fixtures with recorded topology dumps (Ryzen 7950X3D, Core i9-13900K, uniform 8-core)

### [ ] Stage 3: cgroups v2 Dynamic Resource Controller (P0)
- [ ] cgroups v2 unified hierarchy detection and controller capability probe (`cpuset`)
- [ ] Background housekeeping slice lifecycle (`/sys/fs/cgroup/zenith_background`)
- [ ] Safe thread migration and rollback mechanics
- [ ] Unit tests using temporary cgroup mock directory structures

### [ ] Stage 4: Kernel Latency & VMM Parameter Tuner (P0)
- [ ] Split-lock mitigation controller (`/proc/sys/kernel/split_lock_mitigate`)
- [ ] Transparent HugePages (THP) mode & defrag orchestrator
- [ ] VMM latency parameters (`compaction_proactiveness`, `dirty_ratio`, `swappiness`)
- [ ] Safe probe/apply/revert tests with rollback verification

### [ ] Stage 5: GPU Power & Clock Domain Controller (P1)
- [ ] AMD Radeon sysfs DPM performance level controller (`power_dpm_force_performance_level`)
- [ ] NVIDIA GeForce persistent clocks & boost control abstraction
- [ ] Intel Xe / Arc sysfs frequency lock probe
- [ ] Hardware mock tests for AMD sysfs and NVIDIA CLI interfaces

### [ ] Stage 6: Audio & Real-time PipeWire Tuning (P1)
- [ ] PipeWire metadata quantum controller (`pw-metadata` / libpipewire interface)
- [ ] Environment variable propagator for child game processes (`PIPEWIRE_LATENCY`)
- [ ] Verification tests for quantum calculations and environment export

### [ ] Stage 7: Network QoS & Bufferbloat Mitigation (P1)
- [ ] Queueing discipline tuner (`cake` / `fq_codel` via sysctl and `tc`)
- [ ] TCP buffer optimization probes
- [ ] Non-root fallback and status reporting

### [ ] Stage 8: Game Detection & Process Lifecycle Watcher (P0)
- [ ] `/proc` monitor with configurable polling and netlink process connector
- [ ] Steam (`reaper`, `steamapps/common`), Proton/Wine (`wineserver`), Lutris, Heroic, Bottles detection
- [ ] Filtering of non-game binaries (e.g., `steamwebhelper`, overlay injectors)
- [ ] Simulated game launch / exit lifecycle test fixtures

### [ ] Stage 9: Process Scheduling & Priority Elevator (P0)
- [ ] CPU core affinity assignment via `sched_setaffinity`
- [ ] Real-time scheduling policy elevation (`SCHED_RR` / `SCHED_FIFO` with priority clamp)
- [ ] Dynamic thread discovery (`/proc/<pid>/task`) for child render/audio threads
- [ ] I/O priority tuning (`ioprio_set` Realtime class)

### [ ] Stage 10: Client-Server IPC & CLI Interface (P1)
- [ ] UNIX domain socket IPC between CLI client (`zenith`) and background daemon
- [ ] CLI commands: `daemon`, `run`, `tune`, `revert`, `status`, `profile`
- [ ] Formatted status output with human-readable hardware and active game telemetry
- [ ] Integration test for IPC communication and command dispatch

### [ ] Stage 11: Systemd Service & Arch/CachyOS Packaging (P1)
- [ ] Hardened systemd unit file with fine-grained capabilities (`CAP_SYS_NICE`, `CAP_SYS_ADMIN`, `CAP_SYS_RESOURCE`)
- [ ] Production PKGBUILD for Arch Linux / CachyOS with validation
- [ ] Desktop integration / polkit rules for unprivileged client control

### [ ] Stage 12: End-to-End Stress Testing & Quality Gate Audit (P0)
- [ ] Comprehensive integration test suite simulating full game session lifecycle
- [ ] Memory leak audit under Valgrind / ASan during prolonged daemon execution
- [ ] Clang-tidy static analysis audit
- [ ] Documentation and CHANGELOG finalization for v1.1.0 release
