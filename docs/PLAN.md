# Zenith Gaming Daemon: Master Implementation Plan

> **Project**: `zenith-gaming-daemon` (C++20, Linux, `D:\zenith-gaming-daemon`, GitHub: `NEGG0/zenith-gaming-daemon`)  
> **Status**: Active Development  
> **Protocol**: Autonomous Senior Engineer State Protocol (Strict Quality Gates)

---

## Quality Gate Checklist (Every Task)
- [x] Build clean on GCC (0 warnings with `-Wall -Wextra -Wpedantic`)
- [x] Build clean on Clang (0 warnings)
- [x] All unit/integration tests pass
- [x] Clean under ASan (AddressSanitizer) and UBSan (UndefinedBehaviorSanitizer)
- [x] Documentation updated (`README.md`, `docs/AUDIT.md`)
- [x] Log entry in `docs/PROGRESS.md` with verifiable terminal output
- [x] Conventional Commit pushed to repository

---

## 12-Stage Roadmap

### [x] Stage 1: Subsystem Architecture, State Journal & Rollback Reliability (P0)
- [x] `docs/AUDIT.md`: Matrix of ALL system mutations, baseline origins, rollback procedures, and crash consequences.
- [x] Write-Ahead Logging State Journal (`/run/zenith/journal.json`): Atomic tmp -> fsync -> rename -> dir fsync persistence.
- [x] Crash-only design: Daemon startup unconditionally recovers orphaned mutations; clean shutdown shares the identical rollback pipeline.
- [x] Single-instance protection via `flock` on `/run/zenith/zenith.lock`.
- [x] Resilient, idempotent rollback with structured `RollbackReport`.
- [x] Safe Mode / crash loop protection: detects $\ge 3$ crashes within sliding 10-minute window and disables aggressive mutations.
- [x] Systemd hardening: `Type=notify`, `WatchdogSec=15`, `Restart=on-failure`, `ExecStopPost=/usr/bin/zenith --rollback`.
- [x] Fault Injection Matrix test (`test_fault_injection`): 21 crash points tested across pre-mutation, mid-WAL, and post-applied phases. 100% green verification.

### [x] Stage 2: Security & Hardening Architecture (P0)
- [x] `docs/THREAT_MODEL.md`: Assets, trust boundaries, attacker vectors (local user, malicious config, process spoofing).
- [x] Zero Shell Policy: Total elimination of `std::system()` and `popen()`; all executions through `SecureProcess` using `fork()` + `execve()` with approved whitelist (`/usr/bin/nvidia-smi`, `/usr/bin/pw-metadata`).
- [x] Virtual FS Sandboxing (`SecureIo`): `openat2` with `RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS` via trusted base dirfds.
- [x] Strict configuration validation: 64 KB cap, immediate error on unknown keys, strict boolean/integer ranges.
- [x] Control socket hardening (`SocketServer`): 0660 permissions, `SO_PEERCRED` kernel authorization (root/wheel/gamemode), 4 KB payload limit.
- [x] Capabilities dropping & privilege containment (`CapabilityGuard`): `PR_SET_NO_NEW_PRIVS`, ambient capabilities dropped down to `CAP_SYS_NICE`, `CAP_SYS_ADMIN`, `CAP_SYS_RESOURCE`.
- [x] Systemd hardening (`systemd/zenith.service`): `NoNewPrivileges=yes`, `ProtectSystem=strict`, `ProtectHome=yes`, `PrivateTmp=yes`, `ProtectKernelModules=yes`, `RestrictAddressFamilies=AF_UNIX AF_NETLINK`, `SystemCallFilter=@system-service`, minimal `ReadWritePaths`.
- [x] Compiler and Linker Hardening: PIE, Full RELRO (`-Wl,-z,relro,-z,now`), `-fstack-protector-strong`, `-fstack-clash-protection`, `-fcf-protection=full`, `-D_FORTIFY_SOURCE=3`, `-Wl,-z,noexecstack`.
- [x] Fuzzing and adversarial regression suite: `fuzz_one_config`, `fuzz_one_sysfs`, `fuzz_one_socket`, `fuzz_one_proc` with seed corpus in `fuzz/corpus/` tested via `test_fuzz_corpus`.
- [x] `SECURITY.md`: Responsible disclosure policy with encrypted contact and response SLAs.

### [ ] Stage 3: Tweaks Justification, Profiles & Conflict Detection (P0)
- [ ] `docs/TWEAKS.md`: Exhaustive registry of every tweak (mechanism, kernel path, min kernel version, pros, risks, harm analysis, default, source, and "[Эффект не измерен]" tag).
- [ ] Review & Refactor Controversial Tweaks:
  - `split_lock_mitigate`: default off, only in aggressive;
  - THP mode: `madvise` vs `always`, defrag evaluation;
  - `compaction_proactiveness` & VMM watermark knobs;
  - Memory & Swap: zram-aware `page-cluster=0`, `swappiness`, MGLRU probe (`/sys/kernel/mm/lru_gen/enabled`);
  - GPU clock lock: thermal guardrails, separated by profile;
  - PipeWire quantum: 128 (safe) / 64 (balanced) / 32 (aggressive);
  - RT Scheduling (`SCHED_RR`): strict watchdog, priority ceiling ($\le 50$), runtime throttling protection (`sched_rt_runtime_us`);
  - cgroup isolation: soft throttling (`cpu.weight`, `cpu.idle`, `io.weight`) instead of hard single-core pinning.
- [ ] Profiles Architecture (`safe`, `balanced` [default], `aggressive`):
  - Each tweak declares its supported profile mask;
  - Configurable in `zenith.conf` (`profile = balanced`).
- [ ] Modular Tweak Subsystems (Probe/Apply/Revert/Describe/Explain):
  - CPU Governor & EPP Tuner (`scaling_governor`, `energy_performance_preference` for `amd_pstate` / `intel_pstate`);
  - cgroup v2 Soft Weights Tuner (`cpu.idle`, `cpu.weight`, `io.weight`, `memory.high`);
  - Scheduler Attributes & uclamp (`uclamp.min` for games, `SCHED_BATCH`/`SCHED_IDLE` for heavy background);
  - Timer Slack & Scheduler Latency (`PR_SET_TIMERSLACK` to 50ns, safe sysctl latency probes);
  - NVMe I/O Scheduler (`none`) and readahead optimization;
  - USB Gaming Device Autosuspend Inhibitor (`power/control -> on`);
  - zram & Swap Tuner (`vm.watermark_boost_factor = 0`, `page-cluster = 0`).
- [ ] CLI Introspection:
  - `--dry-run`: ANSI-colored diff of "Before $\to$ After" without system mutation;
  - `--explain`: Detailed plain-language breakdown of all tweaks, mechanisms, and risks.
- [ ] Per-Game Configuration & Profiles:
  - Match by Steam AppID or executable name;
  - Profile overrides and tweak blacklist support.
- [ ] Ecosystem Conflict Detector:
  - Probe active services: `gamemode`, `ananicy`, `tuned`, `power-profiles-daemon`, `irqbalance`, `scx_*`;
  - Coordination notices and non-destructive coexistence.
- [ ] Quality Gates:
  - 100% unit tests pass;
  - Clean build under GCC + Clang with ASan/UBSan;
  - Progress and ADR recorded.

### [ ] Stage 4: Hardware Discovery & CPU Topology Engine (P0)
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
