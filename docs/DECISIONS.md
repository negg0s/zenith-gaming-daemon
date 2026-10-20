# Zenith Gaming Daemon: Architectural Decision Records (ADRs)

---

## ADR-0001: Modern C++20 for Linux Daemon Implementation

### Context
Low-latency game optimization requires microsecond-level process manipulation, direct sysfs/procfs filesystem interaction, cgroups v2 tree management, and robust POSIX system call execution without garbage collection pauses or runtime bloat.

### Options Considered
1. **Python / Bash Scripting**: Quick to write, but introduces interpreter overhead, lack of static type safety, high memory footprint, and fragile error handling during atomic rollbacks.
2. **Rust**: Strong memory safety, but increases build friction in minimal CachyOS/Arch environments without cargo installed.
3. **C++20**: Zero-overhead abstraction, native `std::filesystem`, `std::format`, RAII resource lifecycle management, clean header/source modularity, and native integration with GCC/Clang on Arch Linux.

### Decision
Adopt **C++20** with strict compiler flags (`-Wall -Wextra -Wpedantic -Werror` / `/W4 /permissive-`).

### Status
Accepted and verified in CI.

---

## ADR-0002: RAII Subsystem Mutation & Rollback Pattern

### Context
A gaming daemon fundamentally alters critical kernel parameters (split-lock mitigations, CPU core affinities, GPU clocks, cgroup structures). If a game crashes or the daemon receives `SIGTERM`/`SIGINT`, the system must never be left in an unstable, locked, or power-draining state.

### Decision
Every system modifying component must implement an explicit 4-phase contract:
1. `probe()`: Check if hardware/kernel interfaces exist and are writable without throwing.
2. `apply()`: Atomically save previous state and apply the optimized value.
3. `revert()`: Guaranteed restoration of the saved original state (called in destructor/signal handler).
4. `describe()`: Human-readable diagnostic description of the parameter and active delta.

### Status
Accepted. Universal interface adoption scheduled for Stage 1.

---

## ADR-0003: Strict Workspace and Storage Isolation

### Context
To maintain disk hygiene and comply with user system requirements, work must strictly avoid modifying or occupying space on system drive `C:`.

### Decision
All builds, cloned repositories, logs, fixtures, and source files must reside exclusively on partition `D:` (`D:\zenith-gaming-daemon`). Any temporary build caches must be cleaned after execution.

### Status
Accepted and strictly enforced.

---

## ADR-0004: Defense-in-Depth Security & Hardening Architecture

### Context
Zenith operates with elevated privileges (`CAP_SYS_ADMIN`, `CAP_SYS_NICE`, `CAP_SYS_RESOURCE`) to adjust kernel parameters, process priorities, and GPU power profiles. This creates potential attack surfaces: command injection via malicious process names, symlink traversal in virtual filesystems, socket spoofing by local unprivileged users, configuration corruption, and memory safety vulnerabilities.

### Options Considered
1. **Unchecked Shell / System Invocations**: Use `std::system()` or `popen()` with formatted strings. Highly susceptible to command injection via crafted game titles or executable paths.
2. **Minimal Hardening**: Standard file I/O and unauthenticated UNIX domain sockets. Vulnerable to symlink races (`/proc` pointing outside) and unprivileged local users sending commands to alter hardware clock states.
3. **Defense-in-Depth Hardening**:
   - Zero Shell Policy: Total elimination of `std::system()` and `popen()`. All external tools executed via `fork()` + `execve()` with whitelisted binaries (`/usr/bin/nvidia-smi`, `/usr/bin/pw-metadata`) and sanitized environment variables.
   - Filesystem Sandboxing: `openat2` with `RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS` via trusted base dirfds to prevent symlink traversal attacks.
   - IPC Authorization: UNIX domain socket at `/run/zenith/zenith.sock` with `0660` permissions, group ownership restricted to `wheel`/`gamemode`, kernel-level `SO_PEERCRED` validation, and 4 KB payload limit.
   - Strict Configuration Validation: Config files capped at 64 KB; any unknown key triggers parse failure; strict type and numerical range enforcement.
   - Privilege Containment: `PR_SET_NO_NEW_PRIVS` set on startup; systemd service hardened with `ProtectSystem=strict`, `ProtectHome=yes`, `PrivateTmp=yes`, and `ProtectKernelModules=yes`.
   - Toolchain Hardening: PIE, Full RELRO (`-Wl,-z,relro,-z,now`), `-fstack-protector-strong`, `-fstack-clash-protection`, `-fcf-protection=full`, `_FORTIFY_SOURCE=3`, and `-Wl,-z,noexecstack`.

### Decision
Adopt **Option 3: Defense-in-Depth Hardening**.

### Status
Accepted and implemented for Stage 2.

---

## ADR-0005: Tweak Justification, Modular Subsystems, and Three-Tier Profiles Architecture

### Context
A gaming daemon that mutates system state without fine-grained scoping risks system instability, thermal throttling, battery drain on laptops, or audio/IPC stutter on non-game background processes. Each optimization must be technically justified, guarded by kernel compatibility probes, categorized into operational profiles, and audited against conflicting daemons in the Linux ecosystem.

### Options Considered
1. **Monolithic Global Switches**: All tweaks enabled simultaneously. Increases the risk of regressions on unstable hardware or battery-powered devices.
2. **Ad-Hoc Manual Flags**: Every knob exposed independently with no profile grouping. Overwhelms users and complicates safe defaults.
3. **Modular Subsystems with Three-Tier Profiles**:
   - Universal `ITweak` Interface: Every tweak implements `probe()`, `apply(profile)`, `revert()`, `describe()`, `explain()`, `get_current_value()`, `get_target_value(profile)`.
   - Three Operational Profiles:
     - `safe`: Near-zero risk; no hardware clock overrides; soft cgroup prioritization; PipeWire quantum 128.
     - `balanced` (Default): THP `madvise`, NVMe `none`, EPP `performance`, soft cgroups, `uclamp.min = 512`, USB autosuspend disabled.
     - `aggressive`: Split-lock mitigation disabled (`0`), THP `always`, GPU clock lock, `SCHED_RR` priority with 3-second watchdog, PipeWire quantum 32.
   - Non-Destructive CLI Introspection:
     - `--dry-run`: Colorized diff of "Before $\to$ After" values without applying mutations.
     - `--explain`: Human-readable technical breakdown of kernel mechanisms, pros, and risks.
     - `--conflicts`: Proactive detection and coordination with `gamemode`, `ananicy`, `tuned`, `power-profiles-daemon`, `irqbalance`, and `scx`.
   - Per-Game Profile Overrides: Match by Steam AppID (e.g. `[730]`) or executable pattern, with tweak blacklisting support.
   - Empirical Rigor: All tweaks documented in `docs/TWEAKS.md` and explicitly labeled `[Эффект не измерен]` until empirical benchmark verification in Stage 9.

### Decision
Adopt **Option 3: Modular Subsystems with Three-Tier Profiles**.

### Status
Accepted and implemented for Stage 3.

---

## ADR-0006: Hardware Topology Engine with amd_x3d_vcache Integration and Multi-Architecture Fixtures

### Context
Modern gaming systems feature diverse hardware topologies: AMD asymmetric dual-CCD 3D V-Cache CPUs (7950X3D, 9950X3D), single-CCD V-Cache (7800X3D, 5800X3D), Intel Hybrid architectures (12th-14th Gen P/E cores, Core Ultra P/E/LP-E cores), multi-socket NUMA servers, virtual machines, and handheld APUs (Steam Deck). Statically assuming core capabilities or attempting thread pinning without knowing cache topology leads to performance regressions.

### Options Considered
1. **Frequency-Only Heuristic**: Guess P vs E cores by comparing max frequencies. Fails on architectures where E-cores clock high or when thermal throttling occurs. Fails on AMD 3D V-Cache where core types are identical except for L3 cache attachment.
2. **Hardcoded CPU Model Tables**: Maintain a hardcoded database of CPU models by name string. Fragile, requires constant updates, breaks on unreleased or custom CPUs.
3. **Hardware Discovery via Kernel Virtual Filesystem & Driver Abstraction**:
   - `amd_x3d_vcache` Driver (Linux 6.13+): Primary driver probe via `/sys/devices/platform/amd_x3d_vcache/amd_x3d_mode` (`cache`, `frequency`, `none`). Fallback to L3 cache asymmetry if driver not present.
   - Cache Parsing: Parse `cache/index3/size` and `cache/index3/shared_cpu_list` to identify distinct L3 clusters.
   - Intel Core Types: Parse `topology/core_type` (`0x20` P-core, `0x40` E-core, `0x10` LP-E core) and `cpu_capacity`.
   - Core Selection Modes: `cache` (prioritize V-Cache), `frequency` (prioritize high clock CCD), `auto`.
   - Virtualization & Platform Awareness: Detect VMs (`QEMU`, `KVM`, `VMware`), Handhelds (Steam Deck, ROG Ally), and laptops on battery via `chassis_type` and `power_supply`.
   - FileSystem Abstraction (`ITopologyFs`): Decouple topology detection from the live host filesystem, enabling full unit test validation across 11 synthetic architecture fixtures.

### Decision
Adopt **Option 3: Hardware Discovery via Kernel Virtual Filesystem & Driver Abstraction**.

### Status
Accepted and implemented for Stage 4.

---

## ADR-0007: Event-Driven Game Detection, Thread Classification, and Non-Invasive Anti-Cheat Compatibility

### Context
Detecting game launches and process lifecycles via high-frequency `/proc` polling consumes unnecessary CPU cycles and incurs 500-1000ms latency. At the same time, aggressive or naive process manipulation can trigger game anti-cheat bans (Easy Anti-Cheat, BattlEye, VAC) if tools attach debuggers (`ptrace`), inspect memory, inject dynamic libraries, or modify anti-cheat service threads. Furthermore, naive background killing or hard CPU pinning disrupts user workflows (compositors, browsers, audio daemons).

### Options Considered
1. **Periodic High-Frequency Polling**: Poll `/proc` every 50ms, reading cmdline and stat for all PIDs. Inefficient, causes constant cache line thrashing and energy drain, especially on handhelds/laptops.
2. **Ptrace / In-Memory Inspection / DLL Injection**: Attach to game processes with `ptrace`, hook Vulkan/OpenGL symbols with `LD_PRELOAD`. Strictly prohibited: triggers immediate anti-cheat bans, causes game crashes, and violates zero-overhead design.
3. **Event-Driven Kernel Connector + Non-Invasive OS-Standard Tuning**:
   - **Netlink Process Connector (`AF_NETLINK`, `NETLINK_CONNECTOR`, `CN_IDX_PROC`)**: Kernel sends instant asynchronous events (`PROC_EVENT_EXEC`, `PROC_EVENT_EXIT`, `PROC_EVENT_FORK`) directly to Zenith with sub-millisecond delivery and zero CPU polling overhead. Graceful fallback to timer-based polling if unprivileged or unsupported.
   - **Deep Recognition & False Positive Immunity**: Detect Steam (`SteamAppId` in environ/cmdline, reaper), Proton/Wine prefixes and `.exe` binaries, Lutris, Heroic, Bottles, Gamescope, native games, and emulators (RPCS3, Yuzu, etc.). Specifically filter out `steamwebhelper`, launchers (Epic, Battle.net), and idle `wineserver`.
   - **Real Game Verification & Hysteresis**: Inspect open file descriptors for GPU render nodes (`/dev/dri/renderD128`, `/dev/nvidia*`) and mapped graphics drivers (`libvulkan`, `libGL`). 100ms debounce on launch and 500ms hysteresis on exit prevent fluttering.
   - **Background Soft Throttling (Never Kill)**: Whitelist essential system services (compositor, PipeWire, audio, network, security, agent terminal). Blacklist heavy apps (browsers, Discord, torrents) and throttle them non-destructively via cgroup v2 soft weights (`cpu.weight=50`, `cpu.idle=1`, `io.weight=50`).
   - **Selective Thread-Level Tuning**: Scan `/proc/<pid>/task/*/comm` to classify threads: Main and Render threads receive performance core pinning and `uclamp.min`; Audio threads receive low-latency scheduling; Shader compilation runs on throughput cores. Anticheat and third-party overlay threads are explicitly excluded from priority elevation.
   - **Anti-Cheat Safety Contract**: 100% non-invasive. Zero `ptrace`, zero memory reading (`/proc/<pid>/mem`), zero code injection. Tuning strictly employs POSIX standard OS scheduler calls (`sched_setaffinity`, `setpriority`, `sched_setscheduler`) and cgroups. Explicit documentation in `docs/ANTI_CHEAT.md` with transparent VAC/anti-cheat limitations.
   - **Filesystem Abstraction (`IProcFs`)**: Decouple procfs operations for unit and stress testing (1000-process fork/exit races, crash injection, zero fd leaks).

### Decision
Adopt **Option 3: Event-Driven Kernel Connector + Non-Invasive OS-Standard Tuning**.

### Status
Accepted and implemented for Stage 5.

---

## ADR-0008: Modular Latency Subsystems, Dynamic NVML Loading, and Hardware Safety Guardrails

### Context
Low latency in competitive gaming requires orchestrating multiple independent hardware and kernel domains: IRQ interrupt steering, network queueing disciplines, audio server buffering, and GPU clock domains. However, systems exhibit wide hardware heterogeneity: AMD vs NVIDIA vs Intel graphics, diverse network adapters (Ethernet, Wi-Fi), and varying audio daemons. Crucially, a failure or absence of one component (e.g. no NVIDIA driver, or an unmanaged network interface) must never cause a cascading failure in other subsystems. Furthermore, aggressive GPU clock forcing without thermal and power guardrails can cause thermal throttling or battery exhaustion.

### Options Considered
1. **Monolithic Procedural Tuning**: Combine all tuning steps into a single sequence. Fragile: if any command fails (e.g. `tc` fails on a virtual bridge, or `nvidia-smi` is missing), the entire optimization pipeline aborts.
2. **External Shell Scripting**: Call unvetted bash scripts for `tc`, `iw`, `nvidia-smi`, `pw-metadata`. Violates the Zero Shell Policy and makes atomic rollback impossible.
3. **Modular Subsystems with Safety Guardrails & Dynamic NVML Loading**:
   - **Independent Subsystems**: Each subsystem (`IrqTuner`, `NetworkTuner`, `PipewireTuner`, `GpuController`) implements an independent RAII contract with isolated `probe()`, `apply()`, `revert()`, and `describe()` methods. Failure in one subsystem is non-fatal and does not block others.
   - **IRQ Steering**: Move network, USB, and NVMe interrupts to housekeeping cores while binding GPU interrupts to gaming CCD cores. Temporarily pause `irqbalance` during gaming sessions and restore it on exit.
   - **Targeted Network QoS & Wi-Fi Power Save**: Discover active default gateway route from `/proc/net/route` and apply CAKE/FQ_CODEL strictly to the active interface, preserving the original qdisc for clean restoration. Disable Wi-Fi power save via `iw` during gaming.
   - **Audio Latency & xrun Guardian**: Configure PipeWire quantum via `pw-metadata` or ephemeral drop-in configuration (`/run/pipewire/pipewire.conf.d/99-zenith.conf`), avoiding modification of `/etc` or user configuration. Monitor xrun counters; dynamically elevate quantum upon audio dropouts.
   - **Multi-Vendor GPU & Dynamic NVML**:
     - NVIDIA: Dynamically load `libnvidia-ml.so.1` via `dlopen`/`dlsym` for direct in-process NVML queries and clock locking without fork/exec overhead. Graceful fallback to `nvidia-smi` binary.
     - AMD: Manage `power_dpm_force_performance_level` via sysfs with rollback.
     - Intel: Tune `gt_min_freq_mhz` and `gt_boost_freq_mhz` via sysfs.
   - **Thermal & Battery Safety Guard**:
     - Thermal Guard: Check GPU temperature via hwmon/NVML. If $T \ge 85^\circ\text{C}$, refuse to lock maximum clocks.
     - Battery Guard: Probe power supplies. If discharging on battery, refuse to engage aggressive GPU performance profiles.

### Decision
Adopt **Option 3: Modular Subsystems with Safety Guardrails & Dynamic NVML Loading**.

### Status
Accepted and implemented for Stage 6.

---

## ADR-0009: Adaptive State Machine, PSI Pressure Monitoring, and Offline Local Learning

### Context
Static system tuning is inherently fragile: applying aggressive sysctl tweaks, uclamp allocations, or locked clocks on thermally constrained hardware or memory-starved systems can cause latency spikes, severe thrashing, and thermal throttling. Furthermore, blindly forcing aggressive settings regardless of whether a game benefits from them can degrade performance.

### Options Considered
1. **Static Blind Tuning**: Apply configured profile once and never adapt during runtime. Fails under dynamic workload spikes, thermal buildup, or memory exhaustion.
2. **Cloud Telemetry / Remote Optimization Profiles**: Collect user frametimes and upload them to a remote server for profile generation. Violates user privacy and adds remote network dependencies.
3. **Local Closed-Loop Adaptive Controller & Offline Learning Engine**:
   - **Pressure Stall Information (PSI)**: Continuously sample `/proc/pressure/{cpu,memory,io}` (`some` and `full` avg10/avg60/avg300) along with CPU/GPU hwmon temperatures and hardware throttle counts (`core_throttle_count`).
   - **Adaptive Degradation & Hysteresis**:
     - When thresholds are breached (e.g. `memory.some.avg10 > 25%`, `io.some.avg10 > 30%`, $T \ge 85^\circ\text{C}$, or thermal throttling events increment), automatically degrade profile: `Aggressive` $\to$ `Balanced` $\to$ `Safe`, logging the exact metric breach.
     - Recovery requires remaining healthy for $N=5$ consecutive sampling cycles and exceeding a cooldown window (10s) to guarantee zero oscillation.
   - **Offline Local Learning (`zenithctl suggest`)**:
     - Parses local MangoHud CSV frametime logs (`avg_fps`, `1%_low`, `0.1%_low`, `frametime_variance`).
     - Recommends the optimal profile per game based on historical variance and thermal headroom.
     - 100% offline: all profiles stored in local JSON (`/var/lib/zenith/learning_db.json`), zero external telemetry.
   - **Sched_ext Extensibility**:
     - Probe Linux 6.12+ `/sys/kernel/sched_ext/state`.
     - Support optional user-configured BPF schedulers (`scx_lavd`, `scx_rusty`, `scx_bpfland`) with non-destructive lifecycle management and rollback.
   - **Platform Clamps**:
     - Automatically enforce battery and handheld ceilings (clamping profile to Balanced or Safe).

### Decision
Adopt **Option 3: Local Closed-Loop Adaptive Controller & Offline Learning Engine**.

### Status
Accepted and implemented for Stage 7.

---

## ADR-0010: C++20 Architectural Paradigm: Monadic Result, Dependency Inversion, RAII Containment, and Single-Threaded Reactor Event Loop

### Context
Zenith is an ultra-low latency system daemon operating with root privileges. As the codebase scales across subsystems (cgroups, CPU topology, process detection, latency tuners, telemetry, BPF), maintaining zero runtime overhead, testability without root privileges, deterministic resource release, and thread safety is paramount.
Traditional C++ patterns using exceptions on hot paths, raw pointers, direct system calls embedded in business logic, and ad-hoc multithreading introduce latency jitter, data races, and make testing dependent on physical Linux root access.

### Options Considered
1. **Traditional Exceptions & Multithreaded Locks**: Throw `std::runtime_error` on failures and use mutexes across multiple worker threads. Incurs non-deterministic stack unwinding penalties, risk of deadlocks, and complexity under ThreadSanitizer (TSan).
2. **Pure C-style Return Codes**: Return integer status codes (`int err`) with output parameters (`int func(T* out)`). Lacks composability, lacks type safety, and separates error metadata from return values.
3. **Reference C++20 Clean Architecture**:
   - **Monadic `Result<T, Error>`**: Zero-allocation error handling with functional chaining (`.map()`, `.and_then()`, `.or_else()`). Rich `Error` structs with `ErrorCode`, message, errno, and source location. Zero exceptions on hot latency paths.
   - **Dependency Inversion Interfaces**: Core operating system interactions abstracted behind clean C++ interfaces (`ISysfs`, `IProcessSource`, `ICommandRunner`, `IClock`, `INetlink`) with production and mock implementations, enabling 100% test coverage without root permissions.
   - **Plugin-like Tweak Registry**: Self-contained tweak definition with auto-registration macros (`ZENITH_REGISTER_TWEAK`), allowing additions with zero boilerplate.
   - **Strict RAII**: Move-only `UniqueFd`, `CgroupDirGuard`, and `ScopeExitGuard`. Absolutely zero raw `new`/`delete`.
   - **Single-Threaded Epoll Core Reactor**: One event loop thread for all signalfd, timerfd, netlink, and socket events, with thread-safe worker posting, proving zero data races under TSan.
   - **Structured High-Performance Logger**: Sinks for ANSI console, journald, and JSON lines with per-key rate limiting to prevent logging overhead.

### Decision
Adopt **Option 3: Reference C++20 Clean Architecture**.

### Status
Accepted and implemented for Stage 8.

---

## ADR-0011: Empirical Benchmarking, Statistical Ablation & Tweak Decision Framework

### Context
System optimization daemons frequently suffer from "snake oil" syndrome: declaring hundreds of sysctl or scheduler tweaks without verifiable statistical evidence, or making unproven marketing assertions ("eliminates 100% of stutters", "applies in 5 ms"). Furthermore, complex configurations can introduce regressions where a tweak benefits one hardware topology while degrading another. Zenith requires a rigorous empirical framework where every tweak is measured across $\ge 10$ runs with 95% Confidence Intervals, and unproven tweaks are disabled by default.

### Options Considered
1. **Ad-hoc Manual Testing**: Developers run informal manual tests without warm-up, statistical confidence intervals, or automated baseline comparisons. Highly subjective, unreproducible, and prone to placebo bias.
2. **Third-Party Game Benchmarking Exclusively**: Relying solely on built-in game benchmarks. Impractical for automated headless CI environments and lacks isolation of individual kernel knobs.
3. **Reproducible Empirical Benchmarking & Ablation Suite (`bench/`)**:
   - **Multi-Iteration Statistical Rigor**: $\ge 10$ runs per scenario preceded by 2 unrecorded warmup runs. Calculation of Median, Mean, Interquartile Range (IQR), Standard Deviation, and 95% Confidence Intervals.
   - **Condition Pinning & Safety Guards**: Thermal stabilization verification, CPU scaling governor verification, and automatic flagging of runs experiencing thermal throttling ($\ge 85^\circ\text{C}$).
   - **A/B & Ablation Architecture (`bench/runner.py`)**: Tests Baseline vs complete Profiles (Safe, Balanced, Aggressive) vs Individual Isolated Tweaks (e.g. `split_lock`, `thp`, `vcache`, `cgroups`, `epp`, `pipewire`, `nvme`) to measure the discrete delta of each intervention.
   - **Dual Execution Engine**:
     - *Bare-Metal Mode*: Real cyclictest, pw-top, ping load, and MangoHud/CapFrameX game logs.
     - *Synthetic/Simulation Mode*: Deterministic, mathematically modeled frame time generator enabling full test execution in cloud CI without GPU or bare-metal hardware.
   - **Publication-Ready Visualizations (`bench/plot_results.py`)**: Automated rendering of Matplotlib bar charts with 95% CI error bars and frametime jitter distributions.
   - **Nanosecond-Precision Daemon Footprint Probe (`zenith_bench_daemon`)**: Real empirical measurements of startup latency, memory RSS, and mutation apply/revert duration.
   - **Empirical Tweak Decision Table**: Every tweak must declare its measured effect; tweaks without proven benefit are disabled by default.
   - **Documentation Integrity**: Purging all unverified claims from `README.md` and providing a single reproduction command (`./bench/run_benchmarks.sh`).

### Decision
Adopt **Option 3: Reproducible Empirical Benchmarking & Ablation Suite**.

### Status
Accepted and implemented for Stage 9.

---

## ADR-0012: UX Architecture, Unix Socket Protocol & Doctor Diagnostics

### Context
Zenith operates primarily as a background system daemon, but end-users interact with it through CLI tools, Steam launch options, and shell prompts. A subpar user experience (cryptic error codes, failure without actionable guidance, difficulty diagnosing system incompatibilities, or complex multi-step setups) leads to user frustration and misconfigurations.
A first-class UX is required such that any user can install Zenith, run a diagnostic health check, inspect live core topologies, and launch games seamlessly within 2 minutes.

### Options Considered
1. **Direct Configuration File Editing Exclusively**: Require users to manually modify `/etc/zenith/zenith.conf` and restart systemd. High friction, error-prone, and provides zero real-time feedback during gaming.
2. **Heavy Desktop Electron / Web GUI**: Package a complete web/electron desktop application. Violates Zenith's ultra-low latency philosophy by consuming hundreds of megabytes of RAM and background CPU cycles.
3. **Multi-Tiered CLI, Doctor Engine, and Lightweight Socket Protocol**:
   - **Unified Control CLI (`zenithctl`)**: Intuitive verb-based command interface (`status`, `doctor`, `profile`, `game`, `dry-run`, `explain`, `rollback`, `suggest`, `logs`, `report`, `completions`).
   - **Versioned UNIX Domain Socket Protocol (`/run/zenith/zenith.sock`)**: JSON-based request/response protocol (`STATUS`, `GET_PROFILES`, `SET_PROFILE`, `ROLLBACK`) with fallback to local inspection when the daemon is offline.
   - **Comprehensive Health Doctor (`zenithctl doctor`)**: Automated verification of cgroups v2, kernel version, privileges, competing daemons (gamemode/ananicy/tuned), AMD X3D driver, Intel hybrid cores, PSI, and PipeWire, outputting formatted colored results with **exact actionable bash remediation commands**.
   - **Live Terminal Dashboard (`zenithctl status --watch`)**: Real-time ASCII/ANSI TUI monitor displaying active game, core topology map (CCD0 V-Cache vs CCD1 background cores), PSI pressure gauges, temperatures, and state journal status.
   - **Steam Launch Options Wrapper (`zenith-run %command%`)**: Transparent wrapper supporting `gamescope`, forwarding exit codes without shell expansion, and dispatching D-Bus desktop notifications (`notify-send`) upon game start and exit.
   - **Bilingual Internationalization (`zenith::i18n`)**: English and Russian localization with standardized error codes catalog (`docs/ERRORS.md`) linked directly in output.
   - **Anonymized Issue Reporter (`zenithctl report`)**: Generates a GitHub-ready diagnostics markdown snippet with redacting of usernames, home paths, and non-loopback IP addresses.

### Decision
Adopt **Option 3: Multi-Tiered CLI, Doctor Engine, and Lightweight Socket Protocol**.

### Status
Accepted and implemented for Stage 10.



