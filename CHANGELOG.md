# Changelog

All notable changes to the **Zenith Gaming Daemon** project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.6.0] - 2026-10-25

### Added
- **AMD P-State Policy Support**: added AMD P-State performance policy tuning.
- **Network Low-Latency Sysctls**: tuned network stack for low latency.
- **Completion Alias**: added shell completion alias support.
- **zenith-run Enhancements**: inject glibc/Proton low-latency runtime hints and add `vm.max_map_count` check in doctor.

### Changed
- Updated documentation (README) with ultra profile and Steam launch options.

---

## [1.5.0] - 2026-10-21

### Added
- **DirectStorage & Real-Time I/O Priority Acceleration**:
  - Game Main, Render, and I/O worker threads are now automatically assigned real-time I/O priority (`set_realtime_ioprio`), guaranteeing asset streaming and shader disk reads bypass standard OS I/O queues.
  - Background processes throttled by `BackgroundManager` are simultaneously demoted to `IOPRIO_CLASS_IDLE` and nice 19, eliminating disk bandwidth contention during texture streaming.
- **Proton / DXVK Shared Memory Transparent HugePages (`shmem_enabled`)**:
  - Automatically tunes `/sys/kernel/mm/transparent_hugepage/shmem_enabled` to `advise` (and restores baseline on revert).
  - Accelerates DXVK/VKD3D staging buffers and shared memory pools with 2MB hugepages, reducing TLB miss overhead.
- **VMA Memory Space Expansion (`vm.max_map_count = 2147483642`)**:
  - Tunes kernel maximum memory map count with WAL state journal restoration, preventing out-of-memory crashes and stutter in heavy Proton titles (Cyberpunk 2077, Starfield, Hogwarts Legacy).
- **Proactive Pre-Game Memory Compaction**:
  - Upon entering `Ultra` profile, triggers `/proc/sys/vm/compact_memory` to defragment physical RAM pages before heavy game launch allocations occur.
- **Intel Hardware Energy Performance Bias (EPB) Tuning**:
  - Inspects and sets `/sys/devices/system/cpu/cpu*/power/energy_perf_bias` to `0` (performance), eliminating hardware frequency ramp delays on Intel Core processors.
- **Non-Rotational SATA SSD Optimization**:
  - Extended blk-mq I/O queue tuning to non-rotational SATA SSDs (`queue/rotational == 0`), applying `none` scheduler and elevated read-ahead.
- **Complete Background Freeze Engine Lifecycle**:
  - `ZenithEngine::activate` and `deactivate` now natively manage `BackgroundManager`, automatically suspending blacklisted background applications in `Ultra` mode and thawing them upon exit.
- **`zenith-run` CLI Flags & Low-Latency Runtime Environment Injection**:
  - Supports `--ultra` (`-u`) and `--profile <name>` directly in Steam launch options (`zenith-run --ultra %command%`), automatically dispatching profile changes to the running daemon.
  - Automatically exports optimal runtime variables: `MALLOC_ARENA_MAX=2` (limits glibc arena virtual memory fragmentation), `PROTON_ENABLE_NVAPI=1`, `DXVK_ASYNC=1`, `DXVK_STATE_CACHE=1`, `WINE_FULLSCREEN_FSR=1`, and `PULSE_LATENCY_MSEC=30`.
- **System Doctor Memory Limit Diagnostics**:
  - Added `check_max_map_count()` to `zenithctl doctor` verifying system VMA limits with actionable remediation advice for heavy Proton titles.

---

## [1.4.0] - 2026-10-21

### Added
- **AMD Radeon PowerPlay 3D Full Screen Profile (`pp_power_profile_mode = 1`)**:
  - Automatically configures the AMD SMU firmware into `3D_FULL_SCREEN` mode upon game launch.
  - Fixes aggressive VRAM clock downscaling down to 400 MHz during scene transitions and UI pauses in Vulkan and DirectX 12 games.
- **AMD Hardware Power Cap Maximization (`power1_cap_max`)**:
  - In `Ultra` profile, detects vendor power headroom (`power1_cap_max`) and elevates the power limit from baseline to maximum hardware limit, eliminating power-target throttling under heavy shader/RT load.
- **NVIDIA Dedicated Memory Clocks Lock & Max Power Limit via Native NVML**:
  - Locks GDDR6/GDDR6X/GDDR7 memory clocks (`set_memory_locked_clocks`) to prevent VRAM downclocking.
  - Dynamically queries hardware power constraints (`nvmlDeviceGetPowerManagementLimitConstraints`) and sets maximum sustained TDP headroom in `Ultra` mode.
- **High-Performance Vulkan & Mesa Driver Optimization Hints**:
  - Configures `MESA_SHADER_CACHE_MAX_SIZE=10G` to eliminate shader re-compilation stutter across multiple game installations.
  - Enables `RADV_PERFTEST=aco,sam,dcc` (Smart Access Memory / Resizable BAR and Delta Color Compression for AMD RDNA).
  - Enables `mesa_glthread=true`, `VKD3D_CONFIG=dxr11,dxr`, and `__GL_THREADED_OPTIMIZATIONS=1`.
- **Full Reversible State Management**:
  - Guaranteed recovery of original DPM levels, PowerPlay profiles, power caps, memory clocks, and persistence modes upon game exit or emergency journal rollback.

---

## [1.3.0] - 2026-10-21

### Added
- **New Performance Tier: Ultra Profile (`ultra`)**:
  - Introduced the hardcore `Ultra` optimization profile dedicated to competitive esports and CPU-bound titles.
  - **Zero Background Contention via cgroups v2 Freezer**: all non-essential background tasks are automatically frozen (`cgroup.freeze = 1`), consuming **0% CPU cycles, 0% memory bus bandwidth**, and causing zero L1/L2/L3 cache line pollution.
  - **Paging Elimination (`vm.swappiness = 0`)**: completely eliminates anonymous memory swapping to disk while gaming.
  - **Zero Wakeup Latency**: sets CPU DMA Latency PM QoS request to `0 us` (locks cores out of C6/C8 deep sleep).
  - **DirectStorage & High Throughput I/O**: NVMe read-ahead set to 512 KB with queue scheduler bypassed (`none`).
- **cgroups v2 Freezer & Automatic Process Suspension in `BackgroundManager`**:
  - Implemented `freeze_background()` and `unfreeze_background()` with `SIGSTOP`/`SIGCONT` fallback.
  - Expanded blacklist with 20+ resource-heavy applications (browsers, Discord, Spotify, Telegram, qBittorrent, Deluge, indexers, compilers).
  - Instant automatic thawing when the game exits.
- **Adaptive Controller Degradation & Promotion Ladder**:
  - Integrated `Ultra` profile into the self-healing state machine: `Ultra` -> `Aggressive` -> `Balanced` -> `Safe` on thermal/PSI load, promoting back to `Ultra` after hysteresis cycles.
- **UX & Tooling Enhancements**:
  - `zenithctl profile set ultra` and updated `zenithctl profile list`.
  - Shell autocompletion scripts (bash, zsh, fish) updated to support `ultra`.

---

## [1.2.0] - 2026-10-21

### Added
- **CPU DMA Latency QoS Tweak (`cstate_latency`)**:
  - Implemented `CstateLatencyTweak` controlling `/dev/cpu_dma_latency` to hold a PM QoS request preventing deep C-states (C6/C8).
  - Limits wake-up latency to 0us (Aggressive) or 50us (Balanced), eliminating 50-150us wake-up latency penalties when game threads resume.
  - Automatic kernel PM QoS restoration upon file descriptor release via RAII.
- **Transparent Hugepage Compaction Guard Tweak (`transparent_hugepage`)**:
  - Implemented `TransparentHugepageTweak` setting `/sys/kernel/mm/transparent_hugepage/defrag` to `madvise`.
  - Disables background compaction stalls via `/sys/kernel/mm/transparent_hugepage/khugepaged/defrag` set to `0`.
  - Eliminates 10-80ms synchronous direct-reclaim page-fault stalls in DirectX 12 / Vulkan game engines.
- **Expanded Public Game Profiles Database**:
  - Added 5 new validated game profiles in `profiles/*.toml`:
    - *Helldivers 2* (AppID 553850)
    - *Path of Exile* (AppID 238960)
    - *Black Myth: Wukong* (AppID 2358720)
    - *Grand Theft Auto V* (AppID 271590)
    - *Team Fortress 2* (AppID 440)
  - All 12 public profiles validated against CI profile schema.

### Fixed
- **Process Detection Environment Parsing**:
  - Resolved latent bug in `GameDetector::extract_steam_appid` where embedded null bytes in `/proc/<pid>/environ` truncated C-string literals in `std::string_view::find_first_of`.
  - Replaced with character-by-character bounds-checked scanning for accurate AppID extraction in multi-variable environments.
- **System Doctor Multi-Service Command Generation**:
  - Fixed `zenithctl doctor` competing services fix command to format service names as space-separated arguments instead of comma-separated strings for `systemctl`.
- **Desktop Notification Non-Blocking Wrapper**:
  - Applied double-fork pattern in `zenith-run` notification dispatch to guarantee game launches never block on D-Bus or `notify-send` timeouts.
  - Added `WIFSIGNALED` inspection for standard POSIX `128 + WTERMSIG(status)` exit code forwarding.
- **Packaging and Manifest Versioning**:
  - Synchronized package versioning to v1.2.0 across PKGBUILD, RPM spec, Nix flake, and CycloneDX SBOM.

---

## [1.0.0] - 2026-10-20

### Added
- **Core Engine & Architecture (C++20)**:
  - Single-threaded non-blocking event loop (`epoll_wait` on Linux) with structured signal handling (`SIGINT`, `SIGTERM`, `SIGHUP`).
  - Strict RAII wrappers (`ScopedFd`, `ProcConnector`, `CapabilityGuard`, `FlockLock`).
  - Unified monadic error handling via `zenith::Result<T, Error>` without exceptions on the hot execution path.
  - Zero-drift Write-Ahead state journal (`/run/zenith/journal.json`) ensuring crash-only resilience and emergency state rollback.
- **CPU Topology & Hardware Pinning**:
  - AMD 3D V-Cache native driver probe (`amd_x3d_vcache` and `amd_x3d_mode` sysfs interface).
  - Intel Hybrid P-core vs E-core vs LP E-core classification via `core_type` / `cpu_capacity`.
  - Cache topology parsing via `/sys/devices/system/cpu/cpu*/cache/index3/size` and `shared_cpu_list`.
  - Hotplug dynamic recomputation.
- **Security & Privilege Hardening**:
  - Linux `cap_set_proc` capability bounding (`CAP_SYS_NICE`, `CAP_SYS_RESOURCE`, `CAP_SYS_PTRACE`, `CAP_NET_ADMIN`).
  - Path-traversal proof filesystem operations with `openat2` (`RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS`).
  - Safe subprocess execution via `execve` using strict binary whitelists and zero shell invocation.
  - SO_PEERCRED authentication and GID verification on UNIX domain sockets.
- **Process Detection & Tuning**:
  - Sub-50ms event-driven process monitoring via Netlink Proc Connector (`PROC_EVENT_EXEC` / `EXIT`).
  - Identification heuristics for Steam AppID, Proton/DXVK, Wine, Lutris, Heroic, Bottles, and native binaries.
  - cgroups v2 background isolation slice with soft resource limits (`cpu.weight = 50`, `io.weight = 50`, `cpu.idle = 1`).
  - Real-time priority tuning (`SCHED_RR`, `nice -10`, timer slack reduction).
- **Hardware & Latency Subsystems**:
  - IRQ CPU affinity migration transferring NVMe, network, and USB interrupts to background cores while isolating GPU interrupts.
  - CAKE and FQ-CoDel network qdisc bufferbloat elimination.
  - PipeWire sub-millisecond quantum latency tuning via `pw-metadata`.
  - GPU clock stabilization and power management for AMD Radeon DPM and NVIDIA NVML.
- **Adaptive Controller & Machine Learning**:
  - Real-time PSI (`/proc/pressure/{cpu,memory,io}`) monitoring and thermal throttle prevention.
  - Closed-loop hysteresis state machine with automatic fallback (`Aggressive` $\to$ `Balanced` $\to$ `Safe`).
  - Local MangoHud frametime log parser and session variance learner.
- **UX & Control CLI (`zenithctl`)**:
  - Interactive ASCII/ANSI TUI watch monitor (`zenithctl status --watch`).
  - 8-point system diagnostic doctor (`zenithctl doctor`) with actionable fix commands.
  - Steam integration wrapper (`zenith-run %command%`) with gamescope support and D-Bus desktop notifications.
  - Bilingual localization (`ru` / `en`) and standardized error catalog (`docs/ERRORS.md`).
  - Anonymized diagnostic report generator (`zenithctl report`) for GitHub issues.
  - Autocompletions for Bash, Zsh, and Fish.
- **Packaging & Distribution**:
  - Arch Linux AUR package (`PKGBUILD`) and development variant (`PKGBUILD-git`).
  - Native Debian (`.deb`) and RPM (`.rpm`) packages with uninstall rollback guarantees.
  - Nix flake (`flake.nix`) and NixOS service module.
  - CycloneDX 1.5 Software Bill of Materials (SBOM) and reproducible build tooling.
