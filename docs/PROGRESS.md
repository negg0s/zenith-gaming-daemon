# Zenith Gaming Daemon: Engineering Progress Log

All development actions, execution outputs, and verification milestones are tracked here.

---

## [2026-10-20] - Stage 1: System Mutation Audit & Rollback Reliability

### Context
Execution of Stage 1 according to the Autonomous Senior Engineer protocol. Zenith runs as root / privileged daemon with capabilities (`CAP_SYS_ADMIN`, `CAP_SYS_NICE`, `CAP_SYS_RESOURCE`), mutating kernel parameters, cgroups, and GPU clock domains. The core requirement was establishing a crash-only architecture, Write-Ahead Logging (WAL) state journal, `flock` single-instance protection, Safe Mode crash loop prevention, hardened systemd integration, and a comprehensive fault injection matrix test.

### Actions Completed
1. **`docs/AUDIT.md` Created**:
   - Comprehensive system mutation matrix cataloging 17 distinct kernel, sysfs, cgroup, GPU, process, PipeWire, and IRQ targets.
   - Documented baseline origins, applied values, rollback mechanisms, and failure consequences.
2. **State Journal Implemented (`/run/zenith/journal.json`)**:
   - Versioned schema (v1).
   - Atomic persistence: write to unique `.tmp.<epoch>` file $\to$ `fsync` file $\to$ atomic `rename` $\to$ `fsync` parent directory.
   - Write-Ahead Logging: mutations logged as `PENDING` before modification, marked `APPLIED` upon success, and `REVERTED` on rollback.
   - Crash-only architecture: startup unconditionally runs `recover_all()` in reverse order to heal orphaned mutations; clean shutdown shares the identical recovery pipeline.
3. **Flock Guardian (`/run/zenith/zenith.lock`)**:
   - POSIX file locking (`flock(fd, LOCK_EX | LOCK_NB)`).
   - Prevents duplicate daemon/wrapper invocations, outputs holding PID.
4. **Safe Mode Crash Loop Protection**:
   - Tracks crash timestamps in `/run/zenith/crash_history.json`.
   - Sliding window: 10 minutes, threshold: $\ge 3$ crashes.
   - Disables aggressive mutations upon crash loop detection.
5. **Systemd Hardening (`systemd/zenith.service`)**:
   - Added `Type=notify`, `WatchdogSec=15`, `Restart=on-failure`, `RestartSec=3`.
   - `ExecStopPost=/usr/bin/zenith --rollback` guarantees rollback execution even if daemon is `SIGKILL`'ed.
6. **Fault Injection Matrix & Test Suite**:
   - Implemented `tests/test_journal.cpp`, `tests/test_flock.cpp`, `tests/test_safe_mode.cpp`, and `tests/test_fault_injection.cpp`.
   - 21 simulated crash scenarios tested across Pre-Mutation, Mid-WAL-Pending, and Post-Applied phases.

### Verified CI Command Output (GitHub Actions Run 39463273970)
```text
✓ build-and-test (clang) in 47s (ID 118767879406)
  ✓ Set up job
  ✓ Checkout repository
  ✓ Install dependencies
  ✓ Configure CMake (Clang with ASan/UBSan)
  ✓ Build Zenith
  ✓ Run Unit Tests & Fault Injection Matrix (100% tests passed, 0 failures)
  ✓ Complete job

✓ build-and-test (gcc) in 33s (ID 118767880037)
  ✓ Set up job
  ✓ Checkout repository
  ✓ Install dependencies
  ✓ Configure CMake (GCC with ASan/UBSan)
  ✓ Build Zenith
  ✓ Run Unit Tests & Fault Injection Matrix (100% tests passed, 0 failures)
  ✓ Complete job

Test Suite Execution Details:
1/8 Test #1: CpuTopologyTest ..................   Passed    0.25 sec
2/8 Test #2: ConfigTest .......................   Passed    0.01 sec
3/8 Test #3: SysctlTest .......................   Passed    0.01 sec
4/8 Test #4: EngineTest .......................   Passed    0.02 sec
5/8 Test #5: JournalTest ......................   Passed    0.01 sec
6/8 Test #6: FlockTest ........................   Passed    0.01 sec
7/8 Test #7: SafeModeTest .....................   Passed    0.01 sec
8/8 Test #8: FaultInjectionMatrixTest .........   Passed    0.20 sec

Fault Injection Matrix Result:
=========================================================================================
                      FAULT INJECTION MATRIX: CRASH POINT x RESULT                      
=========================================================================================
Step    Crash Phase     Point Code    Crash Triggered   Journal Recovery    Baseline Restored
-----------------------------------------------------------------------------------------
1       Pre-Mutation    1             [CAUGHT]          [RECOVERED 0]       [PASS]      
1       Mid-WAL-Pending 101           [CAUGHT]          [RECOVERED 1]       [PASS]      
1       Post-Applied    201           [CAUGHT]          [RECOVERED 1]       [PASS]      
2       Pre-Mutation    2             [CAUGHT]          [RECOVERED 1]       [PASS]      
2       Mid-WAL-Pending 102           [CAUGHT]          [RECOVERED 2]       [PASS]      
2       Post-Applied    202           [CAUGHT]          [RECOVERED 2]       [PASS]      
3       Pre-Mutation    3             [CAUGHT]          [RECOVERED 2]       [PASS]      
3       Mid-WAL-Pending 103           [CAUGHT]          [RECOVERED 3]       [PASS]      
3       Post-Applied    203           [CAUGHT]          [RECOVERED 3]       [PASS]      
4       Pre-Mutation    4             [CAUGHT]          [RECOVERED 3]       [PASS]      
4       Mid-WAL-Pending 104           [CAUGHT]          [RECOVERED 4]       [PASS]      
4       Post-Applied    204           [CAUGHT]          [RECOVERED 4]       [PASS]      
5       Pre-Mutation    5             [CAUGHT]          [RECOVERED 4]       [PASS]      
5       Mid-WAL-Pending 105           [CAUGHT]          [RECOVERED 5]       [PASS]      
5       Post-Applied    205           [CAUGHT]          [RECOVERED 5]       [PASS]      
6       Pre-Mutation    6             [CAUGHT]          [RECOVERED 5]       [PASS]      
6       Mid-WAL-Pending 106           [CAUGHT]          [RECOVERED 6]       [PASS]      
6       Post-Applied    206           [CAUGHT]          [RECOVERED 6]       [PASS]      
7       Pre-Mutation    7             [CAUGHT]          [RECOVERED 6]       [PASS]      
7       Mid-WAL-Pending 107           [CAUGHT]          [RECOVERED 7]       [PASS]      
7       Post-Applied    207           [CAUGHT]          [RECOVERED 7]       [PASS]      
=========================================================================================
Matrix Result: 21 / 21 scenarios PASSED (100% green)
=========================================================================================

100% tests passed, 0 tests failed out of 8. Clean under ASan & UBSan.
```

### Active State
- Stage 1 completed and verified.
- Master checklist in `docs/PLAN.md` updated.

---

## [2026-10-20] - Stage 2: Security & Hardening Architecture

### Context
Execution of Stage 2 according to the Autonomous Senior Engineer protocol. Zenith runs as root / privileged daemon with capabilities (`CAP_SYS_ADMIN`, `CAP_SYS_NICE`, `CAP_SYS_RESOURCE`), mutating kernel parameters, cgroups, and GPU clock domains. The primary objective is defense-in-depth security: eliminating shell command injections, sandboxing virtual filesystem access against symlink traversal, strictly validating configuration inputs, enforcing kernel `SO_PEERCRED` socket authorization, dropping unneeded capabilities, hardening systemd services, applying toolchain hardening flags, and constructing a comprehensive fuzzing regression suite.

### Actions Completed
1. **Threat Model Created (`docs/THREAT_MODEL.md`)**:
   - Comprehensive STRIDE threat model detailing assets (kernel stability, CPU fairness, GPU thermals, state journal).
   - Trust boundaries established between untrusted user processes, unprivileged client CLI, privileged daemon, and kernel virtual filesystems.
   - Attack vectors analyzed and counteracted: spoofing, tampering, information disclosure, privilege escalation.
2. **Zero Shell Policy Enforced (`src/secure_process.cpp`, `include/zenith/secure_process.hpp`)**:
   - Completely eliminated all `std::system()`, `popen()`, and shell string concatenations across `src/gpu_controller.cpp` and `src/audio_network.cpp`.
   - Implemented `SecureProcess::execute`: executes only pre-approved whitelisted binaries (`/usr/bin/nvidia-smi`, `/usr/bin/pw-metadata`) via direct `fork()` + `execve()` with sanitized environment (`PATH=/usr/bin:/bin`, `LC_ALL=C`).
3. **Virtual Filesystem Sandboxing (`src/secure_io.cpp`, `include/zenith/secure_io.hpp`)**:
   - Implemented `openat2` sandbox with `RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS` relative to trusted base dirfds (`/sys/fs/cgroup`, `/proc/sys/vm`, `/sys/class/drm`, `/run/zenith`).
   - Anti-traversal guard in `split_trusted_path` rejects paths with `..` before and after lexical normalization.
4. **Strict Configuration Validation (`src/config.cpp`, `include/zenith/config.hpp`)**:
   - Max file size capped at 64 KB (`MAX_CONFIG_SIZE = 64 * 1024`).
   - Rejection of unknown keys with informative parse error and line numbers.
   - Strict boolean validation (`true`, `false`, `1`, `0` only) and integer boundary checks with trailing character detection.
5. **Control Socket Hardening (`src/socket_server.cpp`, `include/zenith/socket_server.hpp`)**:
   - UNIX domain socket at `/run/zenith/zenith.sock` with `0660` permissions.
   - Group ownership set to `wheel` / `gamemode`.
   - Kernel-enforced `SO_PEERCRED` queries peer UID/GID and checks primary/supplementary groups before accepting requests.
   - Strict 4 KB message payload limit.
6. **Capability Dropping & Systemd Hardening (`src/capability_guard.cpp`, `systemd/zenith.service`)**:
   - Enforced `prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)` upon daemon startup to prevent child processes from obtaining elevated capabilities.
   - Hardened `systemd/zenith.service`: `NoNewPrivileges=yes`, `ProtectSystem=strict`, `ProtectHome=yes`, `PrivateTmp=yes`, `ProtectKernelModules=yes`, `RestrictAddressFamilies=AF_UNIX AF_NETLINK`, `SystemCallFilter=@system-service`, minimal `ReadWritePaths`.
7. **Toolchain Hardening Flags (`CMakeLists.txt`)**:
   - Compiler options: `-fstack-protector-strong`, `-fstack-clash-protection`, `-fcf-protection=full`, `-D_FORTIFY_SOURCE=3`.
   - Linker options: `-Wl,-z,relro`, `-Wl,-z,now`, `-Wl,-z,noexecstack`.
8. **Fuzzing and Adversarial Regression Suite (`fuzz/`, `tests/test_fuzz_corpus.cpp`, `tests/test_security.cpp`)**:
   - Fuzz targets implemented in `fuzz/fuzz_targets.hpp`: `fuzz_one_config`, `fuzz_one_sysfs`, `fuzz_one_socket`, `fuzz_one_proc`.
   - Seed corpus checked into repository (`fuzz/corpus/config/`, `fuzz/corpus/sysfs/`, `fuzz/corpus/socket/`, `fuzz/corpus/proc/`).
   - Adversarial regression tests covering empty buffers, byte mutations, format strings, mismatched parentheses in `/proc/<pid>/stat`, and 128 KB payloads.
9. **Responsible Disclosure Policy (`SECURITY.md`)**:
   - Documented reporting workflow, vulnerability scope, contact channel, and 48-hour response SLA.

### Verified CI Command Output (GitHub Actions Run 39465558916)
```text
✓ build-and-test (clang) in 45s (ID 118775899863)
  ✓ Set up job
  ✓ Checkout repository
  ✓ Install dependencies
  ✓ Configure CMake (Clang with ASan/UBSan)
  ✓ Build Zenith (0 compiler warnings)
  ✓ Run Unit Tests & Fault Injection Matrix (100% tests passed, 0 failures)
  ✓ Complete job

✓ build-and-test (gcc) in 53s (ID 118775899580)
  ✓ Set up job
  ✓ Checkout repository
  ✓ Install dependencies
  ✓ Configure CMake (GCC with ASan/UBSan)
  ✓ Build Zenith (0 compiler warnings)
  ✓ Run Unit Tests & Fault Injection Matrix (100% tests passed, 0 failures)
  ✓ Complete job

Test Suite Execution Details:
 1/10 Test  #1: CpuTopologyTest ..................   Passed    0.01 sec
 2/10 Test  #2: ConfigTest .......................   Passed    0.01 sec
 3/10 Test  #3: SysctlTest .......................   Passed    0.01 sec
 4/10 Test  #4: EngineTest .......................   Passed    0.01 sec
 5/10 Test  #5: JournalTest ......................   Passed    0.01 sec
 6/10 Test  #6: FlockTest ........................   Passed    0.01 sec
 7/10 Test  #7: SafeModeTest .....................   Passed    0.01 sec
 8/10 Test  #8: FaultInjectionMatrixTest .........   Passed    0.19 sec
 9/10 Test  #9: SecurityTest .....................   Passed    0.01 sec
10/10 Test #10: FuzzCorpusTest ...................   Passed    0.03 sec

100% tests passed, 0 tests failed out of 10. Clean under ASan & UBSan.
Total Test time (real) = 0.55 sec
```

### Active State
- Stage 2 completed and verified.
- Master checklist in `docs/PLAN.md` updated.
- ADR-0004 recorded in `docs/DECISIONS.md`.

---

## [2026-10-20] - Stage 3: Tweak Justification, Modular Subsystems, Profiles & Conflict Detection

### Context
Execution of Stage 3 according to the Autonomous Senior Engineer protocol. Every tweak must be technically justified, mapped to kernel documentation, guarded by probes, and organized into operational profiles (`safe`, `balanced`, `aggressive`). Controversial tweaks were revised to prevent instability. Added non-destructive CLI inspection (`--dry-run`, `--explain`, `--conflicts`), per-game profile overrides, and ecosystem conflict detection (`gamemode`, `ananicy`, `tuned`, `power-profiles-daemon`, `irqbalance`, `scx`).

### Actions Completed
1. **Exhaustive Tweaks Registry (`docs/TWEAKS.md`)**:
   - Detailed specification cards for 16 kernel/subsystem tweaks detailing mechanism, kernel path, min kernel version, pros, risks, harm analysis, default setting, profile scope, documentation source, and mandatory tag `[Эффект не измерен]` until Stage 9 benchmarking.
2. **Controversial Tweaks Revised**:
   - `split_lock_mitigate`: default untouched (`1`), only disabled (`0`) in `aggressive` profile.
   - `THP`: `madvise` in `safe`/`balanced`, `always` only in `aggressive`; defrag `defer+madvise`.
   - `compaction_proactiveness`: set to 0 in `balanced`/`aggressive` to eliminate kcompactd background frame jitter.
   - `swappiness` and `page-cluster`: detects zram presence; sets `page-cluster = 0` for zram to stop sequential decompression overhead; `swappiness = 10` for disk swap.
   - MGLRU: dynamically probed at `/sys/kernel/mm/lru_gen/enabled`.
   - cgroups: soft resource weighting (`cpu.weight = 50`, `cpu.idle = 1`, `io.weight = 50`) implemented instead of hard single-core pinning, preventing audio crackle and IPC deadlocks.
3. **Modular Subsystems Implemented (`include/zenith/tweaks/`, `src/tweaks/`)**:
   - `CpuGovernorTweak`: Governor and EPP hints (`scaling_governor`, `energy_performance_preference`).
   - `CgroupWeightTweak`: cgroup v2 soft weights (`cpu.weight`, `cpu.idle`, `io.weight`).
   - `UclampTweak`: utilization clamping (`uclamp.min = 512` in balanced, `1024` in aggressive).
   - `TimerSlackTweak`: thread timer slack reduction (50ns) and safe sysctl latency probes.
   - `NvmeTweak`: NVMe blk-mq queue scheduler bypass (`none`) and readahead optimization.
   - `UsbAutosuspendTweak`: USB gaming device power autosuspend inhibitor (`power/control -> on`).
   - `ZramSwapTweak`: zram and swap tuning (`page-cluster = 0`, `watermark_boost_factor = 0`).
   - `SysctlTweak`: Unified memory and split-lock tuning.
   - `TweakRegistry`: Singleton registry orchestrating probe, apply, revert, dry-run, and explain.
4. **CLI Introspection (`src/main.cpp`)**:
   - `zenith --dry-run [profile]`: ANSI-colored diff of "Before $\to$ After" values across all tweaks without modifying the system.
   - `zenith --explain`: Human-readable technical breakdown of all tweaks, mechanisms, and risks.
   - `zenith --conflicts`: Proactive audit of competing daemons.
5. **Per-Game Profiles (`include/zenith/per_game_profile.hpp`, `src/per_game_profile.cpp`, `configs/profiles.conf`)**:
   - Matches by Steam AppID (e.g. `[730]`) or executable substring.
   - Supports profile overrides, custom parameters, and tweak blacklists.
6. **Conflict Detector (`include/zenith/conflict_detector.hpp`, `src/conflict_detector.cpp`)**:
   - Probes running processes and kernel states for `gamemode`, `ananicy`, `tuned`, `power-profiles-daemon`, `irqbalance`, `scx`.
   - Generates non-destructive coordination warnings.
7. **Architectural Records (`docs/DECISIONS.md`, ADR-0005)**:
   - Formally documented decision for modular subsystem architecture and three-tier profiles.

### Verified CI Command Output (GitHub Actions Run 39467097372)
```text
✓ build-and-test (clang) in 54s (ID 118781213297)
  ✓ Set up job
  ✓ Checkout repository
  ✓ Install dependencies
  ✓ Configure CMake (Clang with ASan/UBSan)
  ✓ Build Zenith (0 compiler warnings)
  ✓ Run Unit Tests & Fault Injection Matrix (100% tests passed, 0 failures)
  ✓ Complete job

✓ build-and-test (gcc) in 51s (ID 118781214004)
  ✓ Set up job
  ✓ Checkout repository
  ✓ Install dependencies
  ✓ Configure CMake (GCC with ASan/UBSan)
  ✓ Build Zenith (0 compiler warnings)
  ✓ Run Unit Tests & Fault Injection Matrix (100% tests passed, 0 failures)
  ✓ Complete job

Test Suite Execution Details:
 1/11 Test  #1: CpuTopologyTest ..................   Passed    0.01 sec
 2/11 Test  #2: ConfigTest .......................   Passed    0.01 sec
 3/11 Test  #3: SysctlTest .......................   Passed    0.01 sec
 4/11 Test  #4: EngineTest .......................   Passed    0.01 sec
 5/11 Test  #5: JournalTest ......................   Passed    0.01 sec
 6/11 Test  #6: FlockTest ........................   Passed    0.01 sec
 7/11 Test  #7: SafeModeTest .....................   Passed    0.01 sec
 8/11 Test  #8: FaultInjectionMatrixTest .........   Passed    0.20 sec
 9/11 Test  #9: SecurityTest .....................   Passed    0.01 sec
10/11 Test #10: FuzzCorpusTest ...................   Passed    0.03 sec
11/11 Test #11: TweaksTest .......................   Passed    0.09 sec

100% tests passed, 0 tests failed out of 11. Clean under ASan & UBSan.
Total Test time (real) = 0.90 sec
```

### Active State
- Stage 3 completed and verified.
- Master checklist in `docs/PLAN.md` updated.
- ADR-0005 recorded in `docs/DECISIONS.md`.

---

## [2026-10-20] - Stage 4: Advanced CPU Topology Engine, amd_x3d_vcache & 11 Fixtures

### Context
Execution of Stage 4 according to the Autonomous Senior Engineer protocol. Zenith requires comprehensive hardware topology awareness across heterogeneous modern architectures: AMD 3D V-Cache dual-CCDs (7950X3D, 9950X3D), single-CCD V-Cache (7800X3D, 5800X3D), uniform 6-core (5600), Intel Hybrid (13900K, 12700K) with P-cores and E-cores, Intel Core Ultra 9 with LP-E cores, multi-socket NUMA servers (EPYC), virtual machines (QEMU/KVM), and handheld APUs (Steam Deck).

### Actions Completed
1. **Linux 6.13+ `amd_x3d_vcache` Kernel Driver Integration**:
   - Probe `/sys/bus/platform/drivers/amd_x3d_vcache` and `/sys/devices/platform/amd_x3d_vcache/amd_x3d_mode`.
   - Driver mode (`cache`, `frequency`, `none`) utilized as primary selector when available; custom L3 cache pinning serves as resilient fallback.
2. **Precision Cache & Core Architecture Classification**:
   - Parsed `cache/index3/size` and `cache/index3/shared_cpu_list` to identify distinct L3 cache clusters and asymmetric CCDs.
   - Intel Hybrid core classification: hardware `topology/core_type` (`0x20` P-core, `0x40` E-core, `0x10` LP-E core) and `cpu_capacity`.
   - Detected SMT status (`/sys/devices/system/cpu/smt/control`), NUMA node count (`/sys/devices/system/node/`), multi-socket count, isolated cores (`/sys/devices/system/cpu/isolated`), and VM hypervisor detection (`sys_vendor` / `product_name`).
3. **Core Selection Modes (`CacheSelectionMode`)**:
   - `Cache`: Prioritizes 3D V-Cache CCD cores (e.g. 7950X3D CCD0).
   - `Frequency`: Prioritizes higher-clock CCD cores (e.g. 7950X3D CCD1) for clock-sensitive titles.
   - `Auto`: Resolves based on per-game profile rules and heuristics.
4. **FileSystem Abstraction (`include/zenith/topology_fs.hpp`)**:
   - `ITopologyFs`, `RealTopologyFs`, and `MockTopologyFs` decouple sysfs traversal from physical disk locations.
   - Synthetic fixture generation tool in `tests/fixture_generator.hpp`.
5. **11 Architecture Fixtures Verified in `tests/test_cpu_topology.cpp`**:
   - `ryzen_7950x3d`: 16 cores / 32 threads, 96M V-Cache CCD0 (0-7,16-23) vs 32M CCD1 (8-15,24-31).
   - `ryzen_9950x3d`: Zen 5 dual-CCD V-Cache asymmetry.
   - `ryzen_7800x3d`: Single CCD 8 cores / 16 threads (all 96M V-Cache).
   - `ryzen_5800x3d`: Zen 3 single CCD 8 cores / 16 threads (96M V-Cache).
   - `ryzen_5600`: Uniform 6 cores / 12 threads (32M L3).
   - `intel_13900k`: 8 P-cores (16 threads: 0-15) + 16 E-cores (16 threads: 16-31).
   - `intel_12700k`: 8 P-cores (16 threads: 0-15) + 4 E-cores (4 threads: 16-19).
   - `intel_core_ultra_9`: P-cores (`0x20`), E-cores (`0x40`), LP-E cores (`0x10`).
   - `epyc_2x_numa`: 2 sockets, 2 NUMA nodes.
   - `vm_4vcpu`: QEMU/KVM virtual machine detection.
   - `steam_deck`: Van Gogh / Aerith APU, handheld chassis (`31`), battery detection, energy-conscious mode.
6. **CPU Hotplug Dynamic Recomputation**:
   - `recompute_hotplug()` updates online core masks and filters offlined cores from gaming allocations.
7. **Form-Factor & Battery Awareness**:
   - Identifies Handheld and Laptop form factors via `chassis_type`; checks `power_supply` discharging status to engage battery-aware tuning.
8. **Architectural Decisions (`docs/DECISIONS.md`, ADR-0006)**:
   - Formally documented decision for Virtual Filesystem and Driver-First Topology Architecture.

### Verified CI Command Output (GitHub Actions Run 39468290521)
```text
✓ build-and-test (clang) in 53s (ID 118785356445)
  ✓ Set up job
  ✓ Checkout repository
  ✓ Install dependencies
  ✓ Configure CMake (Clang with ASan/UBSan)
  ✓ Build Zenith (0 compiler warnings)
  ✓ Run Unit Tests & Fault Injection Matrix (100% tests passed, 0 failures)
  ✓ Complete job

✓ build-and-test (gcc) in 45s (ID 118785356771)
  ✓ Set up job
  ✓ Checkout repository
  ✓ Install dependencies
  ✓ Configure CMake (GCC with ASan/UBSan)
  ✓ Build Zenith (0 compiler warnings)
  ✓ Run Unit Tests & Fault Injection Matrix (100% tests passed, 0 failures)
  ✓ Complete job

Test Suite Execution Details:
 1/11 Test  #1: CpuTopologyTest ..................   Passed    0.28 sec
        1: Running Stage 4 CPU Topology & Fixtures Verification Tests...
        1: [PASS] test_cpu_list_parsing
        1:   [PASS] fixture: ryzen_7950x3d
        1:   [PASS] fixture: ryzen_9950x3d
        1:   [PASS] fixture: ryzen_7800x3d
        1:   [PASS] fixture: ryzen_5800x3d
        1:   [PASS] fixture: ryzen_5600
        1:   [PASS] fixture: intel_13900k
        1:   [PASS] fixture: intel_12700k
        1:   [PASS] fixture: intel_core_ultra_9
        1:   [PASS] fixture: epyc_2x_numa
        1:   [PASS] fixture: vm_4vcpu
        1:   [PASS] fixture: steam_deck
        1: [PASS] test_hotplug
        1: All Stage 4 CPU Topology Tests Passed Successfully!
 2/11 Test  #2: ConfigTest .......................   Passed    0.01 sec
 3/11 Test  #3: SysctlTest .......................   Passed    0.01 sec
 4/11 Test  #4: EngineTest .......................   Passed    0.01 sec
 5/11 Test  #5: JournalTest ......................   Passed    0.01 sec
 6/11 Test  #6: FlockTest ........................   Passed    0.01 sec
 7/11 Test  #7: SafeModeTest .....................   Passed    0.01 sec
 8/11 Test  #8: FaultInjectionMatrixTest .........   Passed    0.20 sec
 9/11 Test  #9: SecurityTest .....................   Passed    0.01 sec
10/11 Test #10: FuzzCorpusTest ...................   Passed    0.03 sec
11/11 Test #11: TweaksTest .......................   Passed    0.11 sec

100% tests passed, 0 tests failed out of 11. Clean under ASan & UBSan.
Total Test time (real) = 0.86 sec
```

### Active State
- Stage 4 completed and verified.
- Master checklist in `docs/PLAN.md` updated.
- ADR-0006 recorded in `docs/DECISIONS.md`.
- Ready for Stage 5 instructions.
