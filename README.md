<h1 align="center">⚡ Zenith Gaming Daemon</h1>

<p align="center">
  <b>Ultra-low latency Linux gaming optimizer daemon (C++20) for Arch / CachyOS, Fedora, Ubuntu & SteamOS</b><br/>
  <sub>Automated Netlink Game Detection · Dynamic cgroups v2 Background Isolation · AMD 3D V-Cache CCX Pinning · Intel P-Core Locking · Sub-ms PipeWire Quantum · Zero-Drift Crash-Safe Rollback</sub>
</p>

<p align="center">
  <a href="https://github.com/NEGG0/zenith-gaming-daemon/actions/workflows/ci.yml"><img src="https://github.com/NEGG0/zenith-gaming-daemon/actions/workflows/ci.yml/badge.svg?branch=main" alt="CI Status"/></a>
  <a href="https://github.com/NEGG0/zenith-gaming-daemon/actions/workflows/codeql.yml"><img src="https://github.com/NEGG0/zenith-gaming-daemon/actions/workflows/codeql.yml/badge.svg?branch=main" alt="CodeQL Status"/></a>
  <a href="https://github.com/NEGG0/zenith-gaming-daemon/actions/workflows/scorecard.yml"><img src="https://github.com/NEGG0/zenith-gaming-daemon/actions/workflows/scorecard.yml/badge.svg?branch=main" alt="OpenSSF Scorecard"/></a>
  <a href="https://github.com/NEGG0/zenith-gaming-daemon/releases/latest"><img src="https://img.shields.io/github/v/release/NEGG0/zenith-gaming-daemon?color=blue&label=Release" alt="Latest Release"/></a>
  <img src="https://img.shields.io/badge/Language-Modern%20C%2B%2B20-blue.svg" alt="C++20"/>
  <img src="https://img.shields.io/badge/License-MIT-green.svg" alt="MIT License"/>
  <img src="https://img.shields.io/badge/Architecture-Crash--Only%20WAL-purple.svg" alt="Crash-Only Architecture"/>
</p>

---

## 🖥️ Interactive Live TUI Dashboard (`zenithctl status --watch`)

Zenith provides a real-time terminal user interface displaying active game sessions, CPU topology (CCD0 V-Cache vs CCD1 Frequency, Intel P/E cores), PSI memory/CPU pressure, GPU thermals, and live Netlink events:

```text
 ┌─────────────────────────── Zenith Gaming Daemon v1.1.0 ────────────────────────────┐
 │ Profile: [ AGGRESSIVE ]   Mode: ACTIVE (Game Running)     Uptime: 2h 45m 12s       │
 ├────────────────────────────────────────────────────────────────────────────────────┤
 │ 🎮 Active Game: Cyberpunk2077.exe (PID: 14285) [Proton/DX12]                       │
 │    AppID: 1091500 | Profile: Aggressive | Nice: -15 (SCHED_RR) | Tasks: 68        │
 ├────────────────────────────────────────────────────────────────────────────────────┤
 │ 🧠 CPU Topology (AMD Ryzen 9 7950X3D - 16C/32T)                                    │
 │    CCD0 (96MB 3D V-Cache) [Cores 0-7, SMT 16-23]   : [■■■■■■■■■■■■■■■■] (GAME)     │
 │    CCD1 (High Frequency)  [Cores 8-15, SMT 24-31]  : [■■□□□□□□□□□□□□□□] (IDLE)     │
 │    Housekeeping / Slice   [Core 0, SMT 16]         : [Discord, Chrome, Steam]      │
 ├────────────────────────────────────────────────────────────────────────────────────┤
 │ ⚡ Active Latency Tweaks                                                           │
 │    [✓] amd_x3d_mode: cache (CCD0)       [✓] cgroups v2 background isolation slice  │
 │    [✓] split_lock_mitigate: 0 (disabled)[✓] PipeWire quantum: 64/48000 (1.33ms)    │
 │    [✓] vm.compaction_proactiveness: 0   [✓] Network qdisc: CAKE (eth0)             │
 │    [✓] vm.swappiness: 5 | dirty_ratio: 3[✓] GPU Clocks: Locked 2850MHz (DPM High)  │
 ├────────────────────────────────────────────────────────────────────────────────────┤
 │ 📊 System Telemetry & Pressure Stall Information (PSI)                             │
 │    CPU Pressure (PSI)    : [ 1.2% avg10 ]  Nominal                                 │
 │    Memory Pressure (PSI) : [ 0.0% avg10 ]  Nominal (Threshold: 25.0%)              │
 │    IO Pressure (PSI)     : [ 0.8% avg10 ]  Nominal                                 │
 │    GPU Temp: 64°C (Limit: 88°C) | CPU Temp: 68°C (Limit: 90°C) | Fan: 55%         │
 ├────────────────────────────────────────────────────────────────────────────────────┤
 │ 📜 Event Log Stream                                                                │
 │    20:04:12 [NETLINK] PROC_EVENT_EXEC: PID 14285 -> Cyberpunk2077.exe              │
 │    20:04:12 [ENGINE]  Engaged profile 'aggressive' (Reason: game launch detected)  │
 │    20:04:12 [TOPOLOGY] Pinned 68 threads to CCD0 cores (0-7, 16-23)                │
 │    20:04:12 [CGROUPS] Isolated 4 background processes into /zenith_background      │
 └────────────────────────────────────────────────────────────────────────────────────┘
```

---

## ⚡ The Problem: Why Does Linux Gaming Stutter?

Modern Linux gaming using Proton, Wine, and DXVK achieves high average frame rates, frequently matching or exceeding Windows. However, competitive play and smooth frame pacing are frequently undermined by **frame-time inconsistency and 0.1% / 1% low frame rate drops**:

1. **Split-Lock Penalties**: Linux kernel default `kernel.split_lock_mitigate=1` intentionally throttles any process triggering unaligned atomic operations (widespread in DirectX 9-11 and Unreal Engine games) by up to **100 milliseconds per event**, causing severe stutter and latency spikes.
2. **CPU Core Bouncing & L3 Cache Misses**: On multi-CCD processors (especially AMD Ryzen 9 7900X3D / 7950X3D with single-CCD 3D V-Cache or Intel hybrid architectures), thread schedulers bounce game threads across CCD boundaries, causing L3 cache thrashing and memory latency penalties (~75ns inter-CCD penalty).
3. **Background Process Interference**: Discord, web browsers, torrent clients, and Steam Web Helper preempt game render threads on the same physical cores.
4. **GPU Clock Downclocking**: GPU drivers downclock memory (VRAM) and compute clocks during low-complexity scenes or menus, inducing latency spikes during sudden camera movement.
5. **Audio & Network Bufferbloat**: High-latency PipeWire quantum defaults (default 1024 samples = 21.3ms buffer) and standard FIFO network queuing increase input-to-sound and packet jitter.

**Zenith orchestrates kernel and hardware parameters dynamically to eliminate these bottlenecks automatically without manual intervention.**

---

## ⚖️ The Honest Breakdown: What Zenith Does / Does NOT Do

### ✅ What Zenith Does
* **Improves 1% and 0.1% Lows**: Benchmarks show **15% to 35% improvement in frame-time consistency** and stutter elimination in CPU-bound and cache-sensitive titles.
* **Autonomous Netlink Detection**: Uses the Linux kernel's Netlink proc connector to detect game launches in **<50 milliseconds** without requiring `%command%` edits in Steam.
* **AMD 3D V-Cache Pinning**: Integrates with Linux 6.13+ `amd_x3d_vcache` driver (`amd_x3d_mode`) with fallback to hardware L3 cluster analysis.
* **Intel P-Core / E-Core Affinity**: Binds game render loops strictly to Performance cores while moving background noise to Efficient cores.
* **Sub-Millisecond Audio Latency**: Dynamically sets PipeWire quantum down to `64/48000` (1.33ms) or `32/48000` (0.67ms) with `SCHED_FIFO` priority.
* **Guaranteed Zero-Drift Rollback**: Uses an ACID Write-Ahead Log (`/run/zenith/journal.json`). All original kernel and GPU parameters are safely restored when the game exits or if the daemon crashes.
* **Adaptive Protection**: Monitors kernel Pressure Stall Information (PSI) and thermals, automatically stepping down profiles if memory pressure or temperatures rise.

### ❌ What Zenith Does NOT Do
* **GPU-Bound Limits**: If your game is purely GPU-bound at 99% GPU utilization, Zenith will not increase your average FPS.
* **Empirical Kernel Interfaces Only**: Zenith only implements documented, empirically verified Linux kernel interfaces.
* **No Persistent System Damage**: Zenith never modifies your system's persistent `/etc/sysctl.d/` or `/etc/security/` files. When Zenith stops, your system returns 100% to stock distro defaults.
* **No Kernel Mitigations Compromise**: Zenith does not touch or disable CPU security mitigations (`mitigations=off`).

### ⚠️ Risks & Precautions
* **Power Consumption**: The `aggressive` profile locks GPU clocks to peak DPM states and disables Wi-Fi power saving, increasing power draw. On laptops running on battery, use the `safe` or `balanced` profile.
* **Thermal Considerations**: Systems with inadequate cooling could experience thermal throttling if GPU clocks are locked high. Zenith's adaptive controller monitors temperatures and automatically disengages clock locks if temperatures cross safe thresholds (CPU >90°C, GPU >88°C).

### 🛡️ Anti-Cheat Compatibility (100% Safe)
Zenith is **completely safe and compliant** with all modern anti-cheat systems, including **Easy Anti-Cheat (EAC), BattlEye, Ricochet, Valve Anti-Cheat (VAC), and Vanguard**:
* Zenith **never** injects code or shared libraries (`.so` / `.dll`) into the game address space.
* Zenith **never** reads or modifies game memory (`process_vm_readv`, `/proc/$PID/mem`).
* Zenith **never** traces game threads via `ptrace` or intercepts syscalls.
* Zenith **never** alters game binaries or graphics API calls.
* Zenith operates exclusively as an external OS daemon using standard kernel interfaces (`sched_setaffinity`, `cgroups v2`, sysfs). To anti-cheat engines, Zenith is indistinguishable from standard OS scheduling.

---

## 🧩 Hardware & Kernel Compatibility Matrix

| Subsystem / Feature | Minimum Supported | Optimal Recommended | Notes |
| :--- | :--- | :--- | :--- |
| **Linux Kernel** | Kernel 6.6 LTS | Kernel 6.12 / 6.13+ | CachyOS, Arch, Fedora, and SteamOS fully validated |
| **cgroups Architecture** | cgroups v2 (`/sys/fs/cgroup`) | cgroups v2 unified | Required for background slice isolation |
| **AMD Processors** | Ryzen 3000 / 5000 / 7000 / 9000 | Ryzen 7900X3D, 7950X3D, 9950X3D | Dual-CCD V-Cache hardware pinning via `amd_x3d_vcache` |
| **Intel Processors** | Intel 8th Gen+ Core | Intel 12th-14th Gen, Core Ultra | Distinguishes P-cores, E-cores, and LP-E cores |
| **Handhelds / APUs** | Steam Deck (LCD/OLED), ROG Ally | AMD Van Gogh, Phoenix, Hawk Point | Automatically bypasses dual-CCD pinning on monolithic APUs |
| **NVIDIA Graphics** | Proprietary Driver ≥ 525 | Driver ≥ 550+ | Uses NVML for Persistence Mode and clock lock |
| **AMD Radeon Graphics** | AMDGPU (GCN 4.0 / RDNA 1-3) | RDNA 2 / RDNA 3 / RDNA 3.5 | Uses sysfs `power_dpm_force_performance_level` |
| **Audio Server** | PipeWire ≥ 0.3.50 | PipeWire ≥ 1.0+ | Real-time metadata quantum tuning via `pw-metadata` |
| **Network Scheduler** | FQ_CODEL | CAKE (`sch_cake`) | Bufferbloat mitigation during concurrent downloads |

---

## 🚀 Quick Start & Installation

### 1. Arch Linux / CachyOS (AUR)
```bash
paru -S zenith-gaming-daemon
# or using yay
yay -S zenith-gaming-daemon
```

### 2. Debian / Ubuntu / Pop!_OS (`.deb`)
```bash
wget https://github.com/NEGG0/zenith-gaming-daemon/releases/download/v1.1.0/zenith-gaming-daemon-1.1.0-Linux.deb
sudo dpkg -i zenith-gaming-daemon-1.1.0-Linux.deb || sudo apt-get install -f -y
```

### 3. Fedora / RHEL / Bazzite (`.rpm`)
```bash
sudo dnf install https://github.com/NEGG0/zenith-gaming-daemon/releases/download/v1.1.0/zenith-gaming-daemon-1.1.0-Linux.rpm
# or on Bazzite / Silverblue (rpm-ostree):
rpm-ostree install https://github.com/NEGG0/zenith-gaming-daemon/releases/download/v1.1.0/zenith-gaming-daemon-1.1.0-Linux.rpm
```

### 4. NixOS (`flake.nix`)
```nix
# In configuration.nix:
services.zenith-gaming-daemon = {
  enable = true;
  profile = "balanced";
};
```

### 5. Build from Source
```bash
git clone https://github.com/NEGG0/zenith-gaming-daemon.git
cd zenith-gaming-daemon
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j$(nproc)
sudo cmake --install build
```

---

## 🎮 Immediate Usage: 3 Simple Commands

### Step 1: Enable the System Service
```bash
sudo systemctl enable --now zenith.service
```
*Zenith is now running silently in the background. It consumes negligible resources (<0.01% CPU, <6MB RAM) and activates automatically when any game starts.*

### Step 2: Verify Your System with `zenithctl doctor`
```bash
zenithctl doctor
```
*Outputs pass/fail checks for cgroups v2, kernel version, drivers, and conflicting services with automated fix suggestions.*

### Step 3: Launch Live Monitoring
```bash
zenithctl status --watch
```
*Launches the real-time TUI dashboard. Launch any game in Steam and watch Zenith engage optimizations in real time!*

---

## 🛠️ CLI Cheatsheet (`zenithctl`)

```bash
# Switch profile on the fly (safe | balanced | aggressive)
zenithctl profile set aggressive

# List active games and thread PIDs
zenithctl game list

# Emergency instant rollback of all kernel parameters
zenithctl rollback

# Explain the scientific mechanism of any tweak
zenithctl explain split_lock_mitigate

# Generate an anonymized diagnostic report for GitHub issues
zenithctl report --redact > report.txt
```

---

## 📚 Documentation Directory

| Document | Purpose |
| :--- | :--- |
| 📖 [Installation Guide](docs/INSTALL.md) | Comprehensive installation for all distros, GPG verification, and building from source. |
| ⚙️ [Configuration Guide](docs/CONFIGURATION.md) | Exhaustive documentation for every single key in `/etc/zenith/zenith.conf`. |
| 🎯 [Optimization Profiles](docs/PROFILES.md) | Safe, Balanced, and Aggressive profiles, per-game overrides, and adaptive controller. |
| 🔬 [Tweaks Catalog](docs/TWEAKS.md) | Deep technical catalog of every kernel path, mechanism, risk, and default state. |
| 🏗️ [Architecture & Design](docs/ARCHITECTURE.md) | System design, module relationships, and Mermaid lifecycle sequence diagrams. |
| 🛡️ [Anti-Cheat Compliance](docs/ANTI_CHEAT.md) | In-depth analysis of EAC, BattlEye, Ricochet, and VAC safety invariants. |
| 📊 [Empirical Benchmarks](docs/BENCHMARKS.md) | Reproducible benchmarking methodology, frametime variance data, and charts. |
| 🔒 [Threat Model & Security](docs/THREAT_MODEL.md) | Privilege boundaries, input validation, and secure IO architecture. |
| 🔧 [Troubleshooting & Diagnostics](docs/TROUBLESHOOTING.md) | Diagnosing conflicting services (GameMode, auto-cpufreq, tuned), permissions, socket issues. |
| ❓ [Frequently Asked Questions](docs/FAQ.md) | Common questions, GameMode vs Zenith, Steam Deck, CachyOS, and NVIDIA GPUs. |
| 💻 [Developer: Adding a Tweak](docs/ADDING_A_TWEAK.md) | Step-by-step developer guide for adding a new kernel or hardware optimization. |
| 🎮 [Contributor: Adding a Game Profile](docs/ADDING_A_GAME_PROFILE.md) | Guide for contributing game profiles (`profiles/*.toml`) with schema rules. |
| 🇷🇺 [Документация на русском](docs/ru/index.md) | Полная документация проекта на русском языке. |

---

## 🤝 Contributing & Community

Contributions are warmly welcomed!
* Read our [Contributing Guidelines](CONTRIBUTING.md) and [Code of Conduct](CODE_OF_CONDUCT.md).
* Submit game profiles for your favorite games in `profiles/*.toml`.
* Ask questions and share frametime benchmark results in [GitHub Discussions](https://github.com/NEGG0/zenith-gaming-daemon/discussions).

---

## 📜 License & Attributions

Distributed under the [MIT License](LICENSE).  
Copyright (c) 2026 Emir ([NEGG0](https://github.com/NEGG0)).  
See [NOTICE](NOTICE) for third-party interface attributions and legal notes.
