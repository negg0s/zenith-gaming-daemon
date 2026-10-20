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
