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

### [x] Stage 3: Tweaks Justification, Profiles & Conflict Detection (P0)
- [x] `docs/TWEAKS.md`: Exhaustive registry of every tweak (mechanism, kernel path, min kernel version, pros, risks, harm analysis, default, source, and "[Эффект не измерен]" tag).
- [x] Review & Refactor Controversial Tweaks:
  - `split_lock_mitigate`: default off, only in aggressive;
  - THP mode: `madvise` vs `always`, defrag evaluation;
  - `compaction_proactiveness` & VMM watermark knobs;
  - Memory & Swap: zram-aware `page-cluster=0`, `swappiness`, MGLRU probe (`/sys/kernel/mm/lru_gen/enabled`);
  - GPU clock lock: thermal guardrails, separated by profile;
  - PipeWire quantum: 128 (safe) / 64 (balanced) / 32 (aggressive);
  - RT Scheduling (`SCHED_RR`): strict watchdog, priority ceiling ($\le 50$), runtime throttling protection (`sched_rt_runtime_us`);
  - cgroup isolation: soft throttling (`cpu.weight`, `cpu.idle`, `io.weight`) instead of hard single-core pinning.
- [x] Profiles Architecture (`safe`, `balanced` [default], `aggressive`):
  - Each tweak declares its supported profile mask;
  - Configurable in `zenith.conf` (`profile = balanced`).
- [x] Modular Tweak Subsystems (Probe/Apply/Revert/Describe/Explain):
  - CPU Governor & EPP Tuner (`scaling_governor`, `energy_performance_preference` for `amd_pstate` / `intel_pstate`);
  - cgroup v2 Soft Weights Tuner (`cpu.idle`, `cpu.weight`, `io.weight`, `memory.high`);
  - Scheduler Attributes & uclamp (`uclamp.min` for games, `SCHED_BATCH`/`SCHED_IDLE` for heavy background);
  - Timer Slack & Scheduler Latency (`PR_SET_TIMERSLACK` to 50ns, safe sysctl latency probes);
  - NVMe I/O Scheduler (`none`) and readahead optimization;
  - USB Gaming Device Autosuspend Inhibitor (`power/control -> on`);
  - zram & Swap Tuner (`vm.watermark_boost_factor = 0`, `page-cluster = 0`).
- [x] CLI Introspection:
  - `--dry-run`: ANSI-colored diff of "Before $\to$ After" without system mutation;
  - `--explain`: Detailed plain-language breakdown of all tweaks, mechanisms, and risks.
- [x] Per-Game Configuration & Profiles:
  - Match by Steam AppID or executable name;
  - Profile overrides and tweak blacklist support.
- [x] Ecosystem Conflict Detector:
  - Probe active services: `gamemode`, `ananicy`, `tuned`, `power-profiles-daemon`, `irqbalance`, `scx_*`;
  - Coordination notices and non-destructive coexistence.
- [x] Quality Gates:
  - 100% unit tests pass;
  - Clean build under GCC + Clang with ASan/UBSan;
  - Progress and ADR recorded.

### [x] Stage 4: Hardware Discovery & Advanced CPU Topology Engine (P0)
- [x] Kernel 6.13+ `amd_x3d_vcache` driver integration:
  - Probe `/sys/bus/platform/drivers/amd_x3d_vcache` and `/sys/devices/platform/amd_x3d_vcache/amd_x3d_mode`;
  - Use kernel driver mode (`cache`, `frequency`, `none`) as primary selector when present; fallback to custom L3 affinity pinning.
- [x] Precision Cache & Core Architecture Parsing:
  - Parse `cache/index3/size` + `shared_cpu_list` to isolate distinct L3 clusters and asymmetric 3D V-Cache CCDs;
  - Intel Hybrid identification: parse `core_type` (0x20 Core/P-core, 0x40 Atom/E-core, 0x10 LP-E core) and `cpu_capacity`;
  - System architecture features: SMT status (`/sys/devices/system/cpu/smt/control`), NUMA nodes (`/sys/devices/system/node/`), Multi-socket (`physical_package_id`), Isolated cores (`/sys/devices/system/cpu/isolated`), and VM detection (`/sys/class/dmi/id/product_name`).
- [x] Core Selection Modes (`cache`, `frequency`, `auto`):
  - Game-aware selection: allow choosing V-Cache CCD or high-clock CCD depending on title requirements.
- [x] FileSystem Abstraction & Fixtures Suite (`tests/fixtures/`):
  - `ITopologyFs` interface decoupled from real disk root;
  - 11 comprehensive fixtures: Ryzen 7950X3D, 9950X3D, 7800X3D, 5800X3D, 5600, Core i9-13900K, i7-12700K, Core Ultra 9, Epyc 2xNUMA, VM 4 vCPU, Steam Deck APU.
- [x] CPU Hotplug Monitoring:
  - Monitor `/sys/devices/system/cpu/online` / `offline` and dynamically recompute core sets.
- [x] Form-Factor Awareness (Handheld & Laptop):
  - Probe chassis type (`/sys/class/dmi/id/chassis_type`) and power supply status (`/sys/class/power_supply/`);
  - Automatically engage energy-conscious profile when on battery or handheld device (Steam Deck, ROG Ally, Legion Go).
- [x] Quality Gates:
  - All 11 fixtures pass validation tests;
  - Clean build under GCC and Clang with ASan/UBSan.

### [x] Stage 5: Game Detector & Process Tuner Engine (P0)
- [x] Netlink process connector (`PROC_EVENT_EXEC`/`EXIT`/`FORK`) with polling fallback (<50 ms reaction time)
- [x] Deep Game Recognition:
  - Steam (`SteamAppId` in environ/cmdline, reaper, `steamapps/common`)
  - Proton/Wine (`wine64-preloader`, wineserver filtering, Wine prefixes)
  - Lutris, Heroic Games Launcher, Bottles, Gamescope, Native games, Emulators (RPCS3, Yuzu, Ryujinx, Dolphin, PCSX2)
  - Exclusion filter: `steamwebhelper`, launchers (Epic, Battle.net, Origin, EA), idle `wineserver`, system services
- [x] Real Game Heuristics:
  - Open Vulkan/GL/DRM/NVIDIA render context inspection (`/dev/dri/renderD128`, `/dev/nvidia*`, memory maps)
  - Debounce (100ms) and Hysteresis (500ms) against spurious process flips
- [x] Multi-Game & Lifecycle Management:
  - Concurrent games tracking, game restart handling, pre-existing games discovery on daemon startup
- [x] Background Process Classifier & cgroup Manager:
  - Whitelist "Never Touch": Compositor, PipeWire, audio, network, security, agent terminal
  - Blacklist "Always Background": Web browsers, chat clients, torrents, file indexers
  - Move to `/sys/fs/cgroup/zenith_background` with soft limits (`cpu.weight=50`, `cpu.idle=1`, `io.weight=50`), never kill
- [x] Fine-Grained Thread Classification & Priority:
  - Classify `/proc/<pid>/task/*/comm` into Main, Render, Audio, ShaderCompile, IoWorker, AnticheatOrOverlay
  - Selective priority: Main/Render $\to$ perf cores/uclamp, Audio $\to$ nice -11, ShaderCompile $\to$ throughput cores
  - Anticheat & Overlay protection: NEVER touch or elevate anticheat/overlay threads
- [x] Anti-Cheat Compatibility Architecture:
  - Non-invasive design: zero `ptrace`, zero memory reading, zero DLL injection
  - `docs/ANTI_CHEAT.md`: Detailed anti-cheat compatibility notice (EAC, BattlEye, VAC disclaimer)
- [x] Fake `/proc` Test Suite & Stress Harness:
  - Decoupled `IProcFs` abstraction with `MockProcFs`
  - 1000 processes fork-storm & exit race tests
  - Zero false positives test across 100 system daemons
  - Zero file descriptor leaks and zero zombie process verification

### [x] Stage 6: Latency Optimization Subsystems & Hardware Controllers (P0)
- [x] IRQ Affinity Tuner (`irq_tuner`):
  - Isolate Network, USB, and NVMe interrupts to background housekeeping cores
  - Bind GPU interrupts to gaming CCD / performance cores
  - Temporary suspension and guaranteed rollback of `irqbalance`
  - Idempotent affinity restoration via Journal WAL
- [x] Network Low-Latency & Bufferbloat Tuner (`network_tuner`):
  - Active default route interface discovery via `/proc/net/route`
  - Non-destructive queueing discipline (CAKE / FQ_CODEL) via `tc` with full rollback
  - Wi-Fi Power Save management via `iw` (inhibit during gaming, restore on exit)
- [x] Audio Latency & xrun Guardian (`pipewire_tuner`):
  - PipeWire quantum adjustment via `pw-metadata` and isolated drop-in configuration (`/run/pipewire/pipewire.conf.d/99-zenith.conf`)
  - Real-time priority via rtkit / `CAP_SYS_NICE`
  - Dynamic xrun Guardian: automatic quantum elevation upon audio dropouts
- [x] Multi-Vendor GPU Controller (`gpu_controller`):
  - AMD Radeon: `power_dpm_force_performance_level` with rollback
  - NVIDIA GeForce: Native NVML library dynamic loader (`libnvidia-ml.so.1`) with persistence mode and locked clocks (safe limits), fallback to `nvidia-smi`
  - Intel Arc / iGPU: `gt_min_freq_mhz` and `gt_boost_freq_mhz` sysfs tuning
- [x] Comprehensive Thermal & Battery Safety Guard:
  - Thermal limit enforcement ($\ge 85^\circ\text{C}$ aborts clock locks)
  - Battery discharge detection (prevents frequency lock on battery power)
- [x] Subsystem Independence & Resilience:
  - Total decoupling: failure in one subsystem (e.g. GPU, PipeWire, IRQ, Network) does not crash or block the others
  - Comprehensive unit test suite with mock sysfs, mock NVML, and mock IRQ fixtures (`test_latency_subsystems.cpp`)

### [x] Stage 7: Intelligent Adaptive Controller, PSI Telemetry & Local Self-Learning (P0)
- [x] Pressure Stall Information (PSI) Monitor (`pressure_monitor`):
  - Parse `/proc/pressure/cpu`, `/proc/pressure/memory`, `/proc/pressure/io` (`some` and `full`)
  - Monitor CPU/GPU temperatures, core/package thermal throttle counts
  - Decoupled `IPressureFs` abstraction for mock and host testing
- [x] Adaptive Profile Controller (`adaptive_controller`):
  - Automatic profile degradation upon threshold breach: `Aggressive` $\to$ `Balanced` $\to$ `Safe`
  - Anti-oscillation guardrails: hysteresis cycles count + cooldown window before recovery
  - Comprehensive reason tracking for all state transitions
  - Handheld & Battery profile ceilings
- [x] Offline Local Learning & Profile Suggester (`local_learner`):
  - Parse local MangoHud frametime CSV logs (`avg_fps`, `1%_low`, `0.1%_low`, `frametime_variance`, `stutters`)
  - Game-specific profile recommendation engine (`zenithctl suggest`)
  - Zero telemetry guarantee: strictly local JSON storage in `/var/lib/zenith/learning_db.json`
- [x] Sched_ext (scx) BPF Extensibility Scheduler Controller (`sched_ext_tuner`):
  - Probe Linux 6.12+ `/sys/kernel/sched_ext/state` and scheduler binaries (`scx_lavd`, `scx_rusty`, `scx_bpfland`)
  - Disabled by default; non-destructive launch, supervisor, and clean rollback
- [x] Comprehensive Unit & Simulation Test Suite (`tests/test_adaptive_controller.cpp`):
  - Simulated PSI spikes (CPU, Memory, IO) and thermal throttling transitions
  - Verification of degradation, recovery hysteresis, anti-oscillation, and local profile suggestions

### [x] Stage 8: Reference C++20 Architecture & Code Excellence (P0)
- [x] 8.1 System Architecture Documentation (`docs/ARCHITECTURE.md`):
  - Complete Mermaid diagrams: Subsystem Topology, Reactive Event Data Flows, Game Lifecycle State Machine, Privilege Boundaries.
- [x] 8.2 Monadic Error Handling (`Result<T, Error>`):
  - Zero-exception hot path; monadic chaining (`and_then`, `or_else`, `map`); rich `Error` context with `ErrorCode`, message, errno, and source location.
- [x] 8.3 Core Dependency Interfaces (`include/zenith/interfaces/`):
  - `ISysfs`, `IProcessSource`, `ICommandRunner`, `IClock`, `INetlink` with Production and Fake implementations (root-free testing).
- [x] 8.4 Plugin-like Self-Registering Tweak Subsystem:
  - Single-file tweak additions with declarative metadata and registration (`ZENITH_REGISTER_TWEAK`).
- [x] 8.5 Strict RAII Resource Management:
  - Zero raw `new`/`delete`; `UniqueFd`, `CgroupDirGuard`, `TweakMutationGuard`.
- [x] 8.6 Structured Logging Engine:
  - Levels (TRACE, DEBUG, INFO, WARN, ERROR), Journald integration, JSON streaming format, token-bucket rate limiter.
- [x] 8.7 Single-Threaded Epoll Core Event Loop:
  - Epoll/io_uring-compatible reactor (`IPoller`, `EpollPoller`, `MockPoller`), signalfd, timerfd, socket events; TSan verified.
- [x] 8.8 Tooling & Strict Compiler Quality Gates:
  - `.clang-format`, `.clang-tidy` (bugprone, performance, modernize, cert), `-Wall -Wextra -Wconversion -Wshadow -Werror`.
- [x] 8.9 Coverage & Validation Suite:
  - Comprehensive tests in `tests/test_cpp20_architecture.cpp`; lcov coverage target ($\ge 85\%$).

### [ ] Stage 11: End-to-End Stress Testing & Quality Gate Audit (P0)
- [ ] Comprehensive integration test suite simulating full game session lifecycle
- [ ] Memory leak audit under Valgrind / ASan during prolonged daemon execution
- [ ] Clang-tidy static analysis audit
- [ ] Documentation and CHANGELOG finalization for v1.1.0 release
