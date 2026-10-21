# Zenith Gaming Daemon: Anti-Cheat Compatibility & Safety Contract

> **Document Status**: Production Standard  
> **Scope**: Linux Game Anti-Cheat Systems (Easy Anti-Cheat, BattlEye, Valve Anti-Cheat, Vanguard, Ricochet, FairFight)  
> **Target Audience**: System Administrators, Competitive Gamers, Game Developers

---

## 1. Executive Summary & Core Principles

Zenith operates under a **Strict Non-Invasive Process Policy**. The daemon is designed from the ground up to optimize Linux gaming workloads without ever violating game integrity, inspecting game memory, or triggering heuristic anti-cheat detection vectors.

### Core Non-Invasive Rules
1. **Zero Memory Reading or Modification**: Zenith **never** accesses `/proc/<pid>/mem`, never calls `process_vm_readv()` or `process_vm_writev()`, and never inspects private application virtual address spaces.
2. **Zero Debugging / Tracing Hooks**: Zenith **never** invokes `ptrace()`, `PTRACE_ATTACH`, `PTRACE_SEIZE`, or any hardware debug registers.
3. **Zero Code or Shared Object Injection**: Zenith **never** uses `LD_PRELOAD`, `dlopen()`, `libmem` hooks, remote thread creation, or function detour trampolines.
4. **Standard Kernel-Level Resource Attribution Only**: Zenith interacts with games exclusively through official Linux kernel system calls provided by the POSIX standard and cgroups v2:
  - `sched_setaffinity(2)`: Pins threads to selected performance cores or V-Cache CCDs.
  - `setpriority(2)` / `nice`: Adjusts standard process nice levels (-20 to +19).
  - `sched_setscheduler(2)` / `sched_setattr(2)`: Requests standard scheduling classes (`SCHED_OTHER`, `SCHED_BATCH`, `SCHED_ISO`, `SCHED_RR`).
  - `sched_setattr(2)` with `uclamp.min`: Informs the kernel CFS/EEVDF scheduler of expected minimum compute capacity.
  - `cgroups v2` (`cgroup.procs`): Moves non-game background tasks into dedicated throttled cgroup slices (`cpu.weight=50`, `cpu.idle=1`).

---

## 2. Anti-Cheat Engine Analysis & Compatibility Matrix

| Anti-Cheat Solution | Common Linux Titles | Zenith Compatibility Mode | Detection Vectors Analyzed | Risk Level |
| :--- | :--- | :--- | :--- | :--- |
| **Easy Anti-Cheat (EAC)** (Proton/Native) | *Apex Legends*, *War Thunder*, *Elden Ring*, *Rust* | Full Compatibility | Memory integrity checks, debugger presence (`TracerPid`), module injection | **Zero Conflict** |
| **BattlEye** (Proton) | *Rainbow Six Siege* (Proton enabled), *DayZ*, *Destiny 2* (if enabled), *ARK* | Full Compatibility | Kernel driver hooks, unauthorized thread injection, memory scanning | **Zero Conflict** |
| **Valve Anti-Cheat (VAC / VACnet)** | *Counter-Strike 2*, *Dota 2*, *Team Fortress 2* | Full Compatibility | Process memory checksums, DLL hashes, open handle inspection | **Low (See Disclaimer)** |
| **Riot Vanguard** | *Valorant*, *League of Legends* | N/A (Windows Kernel Driver) | Does not run on Linux/Proton. Titles requiring Vanguard are unplayable on Linux. | N/A |
| **Call of Duty Ricochet** | *Call of Duty: Warzone* | N/A (Windows Kernel Driver) | Does not run on Linux/Proton. | N/A |

---

## 3. Specific Protection Mechanisms in Zenith

### 3.1. Exclusion of Anti-Cheat and Overlay Threads
When Zenith classifies child threads within an active game process tree, it inspects thread communication names (`/proc/<pid>/task/<tid>/comm`):
- Any thread matching known anti-cheat or overlay identifiers is **explicitly blacklisted from elevation or modification**:
  - `*eac*`, `*easyanticheat*`
  - `*battleye*`, `*beservice*`
  - `*vgk*`
  - `*discord*`, `*obs*`, `*geforce*`
- These threads remain on their default system scheduler attributes, core masks, and priority levels. This ensures that anti-cheat watchdog timers and heartbeat threads are never desynchronized or starved of CPU resources.

### 3.2. Preservation of `TracerPid: 0`
Anti-cheat engines continuously query `/proc/self/status` and verify that `TracerPid == 0`. If a debugger or monitoring tool attaches via `ptrace`, `TracerPid` becomes non-zero, resulting in an immediate game disconnection or security violation. Zenith **never** traces games, preserving `TracerPid: 0` at all times.

### 3.3. File Descriptor and Open Handle Hygiene
Zenith only inspects read-only virtual files in `/proc/<pid>/` (`comm`, `cmdline`, `environ`, `status`, `task/`). It opens these files with `O_RDONLY | O_CLOEXEC`, reads the minimum necessary bytes (e.g. SteamAppId), and closes file descriptors immediately. Zenith does not hold open handles to game binaries or files.

---

## 4. Transparent Anti-Cheat & VAC Disclaimer

> [!WARNING]
> **No Unconditional "VAC Safe" or Anti-Cheat Guarantee**:  
> While Zenith strictly adheres to standard Linux kernel scheduling interfaces and does not hook, inject, or read process memory, **no third-party software can guarantee 100% unconditional immunity** from future proprietary anti-cheat algorithm changes, server-side heuristic updates, or automated policy enforcement.
>
> Valve Corporation, Epic Games, and other publishers reserve the right to flag any third-party background activity, regardless of its innocence. Zenith provides a "Safe" profile (`profile = safe` in `/etc/zenith/zenith.conf`) that minimizes scheduler adjustments to basic affinity pinning and cgroup isolation for maximum caution in competitive esports titles.
