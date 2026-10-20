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
- Ready for Stage 3 instructions.
