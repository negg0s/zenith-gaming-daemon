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
