<h1 align="center">⚡ Zenith Gaming Daemon</h1>

<p align="center">
  <b>Ultra-low latency Linux gaming optimizer daemon for Arch / CachyOS & SteamOS</b><br/>
  <sub>Dynamic cgroups v2 core isolation · AMD 3D V-Cache CCX pinning · GPU clock lock · split-lock mitigation · sub-ms PipeWire quantum</sub>
</p>

<p align="center">
  <a href="https://github.com/NEGG0/zenith-gaming-daemon/actions/workflows/ci.yml"><img src="https://github.com/NEGG0/zenith-gaming-daemon/actions/workflows/ci.yml/badge.svg?branch=main" alt="CI Status"/></a>
  <img src="https://img.shields.io/badge/Language-C%2B%2B20-blue.svg" alt="C++20"/>
  <img src="https://img.shields.io/badge/License-MIT-green.svg" alt="MIT License"/>
  <img src="https://img.shields.io/badge/Platform-Arch%20%7C%20CachyOS%20%7C%20SteamOS-orange.svg" alt="Platform"/>
</p>

---

## 🎯 The Problem

Modern Linux gaming (Proton/Wine, DXVK, VKD3D) achieves stellar average frame rates, but competitive play and stutter-free gaming are frequently undermined by **frame-time inconsistency and 0.1% / 1% FPS drops**:

1. **Split-Lock Penalties**: Linux kernel default `kernel.split_lock_mitigate=1` artificially throttles processes triggering unaligned atomic instructions (widespread in DirectX 9–11 and older game engines), inducing severe 50–100ms micro-stutters.
2. **CPU Core Bouncing & L3 Cache Misses**: On multi-CCD processors (especially AMD Ryzen 9 7900X3D / 7950X3D with single-CCD 3D V-Cache or Intel hybrid architectures), thread schedulers bounce game threads across CCD boundaries, causing catastrophic L3 cache thrashing and memory latency spikes.
3. **Background Process Interference**: Discord, Chromium web browsers, background indexing, and Steam client processes preempt time-sensitive game render loops.
4. **GPU Clock Downclocking**: GPU drivers downclock memory (VRAM) and compute clocks during low-complexity scenes or menus, inducing latency spikes during sudden camera rotations or action.
5. **Audio & Network Bufferbloat**: High-latency PipeWire quantum defaults and standard FIFO network queuing increase input-to-sound and packet jitter.

**Zenith** orchestrates kernel and hardware parameters dynamically to mitigate these bottlenecks without compromising system stability.

---

## 🏗️ Architecture

```
                                 [ Game Launch ]
                       (Steam / Proton / Lutris / Wine / Native)
                                        │
                                        ▼
                            ┌───────────────────────┐
                            │  Zenith Game Detector │
                            │     (/proc scan)      │
                            └───────────┬───────────┘
                                        │ (Game PID detected)
                                        ▼
                            ┌───────────────────────┐
                            │     Zenith Engine     │
                            └───────────┬───────────┘
                                        │
        ┌───────────────────┬───────────┼───────────────────┬───────────────────┐
        ▼                   ▼           ▼                   ▼                   ▼
┌───────────────┐   ┌───────────────┐ ┌───────────────┐ ┌───────────────┐ ┌───────────────┐
│ CPU Topology  │   │  cgroups v2   │ │ Kernel Latency│ │ GPU Clock Lock│ │ Audio/Network │
│  & V-Cache    │   │   Isolation   │ │ (Sysctl/VMM)  │ │   (DPM/NVML)  │ │  (PipeWire)   │
├───────────────┤   ├───────────────┤ ├───────────────┤ ├───────────────┤ ├───────────────┤
│ AMD 3D-Cache  │   │ Move Discord, │ │ split_lock=0  │ │ AMD: dpm=high │ │ Quantum=64/48k│
│ CCD0 pinning  │   │ Chrome, Steam │ │ THP=madvise   │ │ Lock VRAM clk │ │ sub-ms audio  │
│ Intel P-Cores │   │ to core 0     │ │ compact_pro=0 │ │ NVIDIA: -pm 1 │ │ CAKE qdisc    │
│ SCHED_RR rt   │   │ cpuset.cpus   │ │ swappiness=10 │ │ max boost clk │ │ low jitter    │
└───────────────┘   └───────────────┘ └───────────────┘ └───────────────┘ └───────────────┘
                                        │
                                        ▼
                             [ Active Gaming Session ]
                           (Smooth 0.1% Lows & Frame Pacing)
                                        │
                                 [ Game Exits ]
                                        │
                                        ▼
                            ┌───────────────────────┐
                            │    RAII State Revert  │
                            │  (Atomic Clean Reset) │
                            └───────────────────────┘
```

---

## ⚡ Core Features

### 1. Dynamic cgroups v2 Core Isolation
- Detects cgroups v2 controller hierarchy (`/sys/fs/cgroup`).
- Instantiates a high-priority background slice (`/sys/fs/cgroup/zenith_background`).
- Restricts non-gaming processes (Discord, browsers, torrent clients, media players) to dedicated housekeeping cores (`cpuset.cpus = 0`).
- Prioritizes primary gaming cores for game render threads while soft-throttling background workers.

### 2. AMD 3D V-Cache Pinning & Intel P-Core Locking
- Integrates with Linux 6.13+ `amd_x3d_vcache` driver (`amd_x3d_mode`) with fallback to L3 cluster topology analysis.
- Automatically identifies CCD0 with 96MB 3D V-Cache (Ryzen 7900X3D / 7950X3D) vs standard CCD1.
- Automatically pins game and Proton worker threads to 3D V-Cache cores, minimizing inter-CCD latency penalties.
- For Intel Hybrid architectures (12th/13th/14th Gen, Core Ultra), binds game loops strictly to high-frequency Performance cores (P-cores).

### 3. Kernel Latency & VMM Tuning
- **Split-Lock Mitigation**: Configurable via profiles (disabled in `aggressive` to prevent kernel-imposed throttling on Wine/DirectX atomic calls).
- **Transparent HugePages (THP)**: Sets `transparent_hugepage = madvise` with `defrag = defer+madvise`, reducing page table walks in DXVK memory allocations without risking memory bloat.
- **Compaction Proactiveness**: Sets `vm.compaction_proactiveness = 0` to prevent background memory defragmentation stalls while gaming.
- **Autogroup Disabled**: Disables `sched_autogroup_enabled` so custom real-time and nice priority boosts are respected.

### 4. GPU Power & Frequency Lock
- **AMD Radeon**: Configures `/sys/class/drm/card*/device/power_dpm_force_performance_level` to `high` in aggressive profile, preventing VRAM memory clock downclocking.
- **NVIDIA GeForce**: Activates driver persistence mode (`-pm 1`) and locks boost permissions to avoid throttle spikes under appropriate cooling.

### 5. Low-Latency Audio & Network
- Sets PipeWire real-time metadata to quantum `64/48000` (1.33ms buffer latency) with `SCHED_FIFO` priority.
- Configures `CAKE` / `FQ_CODEL` packet scheduling to mitigate bufferbloat and ping spikes during active downloads.

### 6. Atomic RAII State Rollback & Crash Recovery
- Original kernel sysctl values, GPU performance levels, and cgroup configurations are saved to a versioned Write-Ahead Log (`/run/zenith/journal.json`) before modification.
- When the game process exits (or upon `SIGINT`/`SIGTERM`/crash), Zenith restores every original setting safely via idempotent rollback.

---

## 🚀 Quick Start

### Building from Source (Arch / CachyOS / SteamOS)

```bash
# Clone the repository
git clone https://github.com/NEGG0/zenith-gaming-daemon.git
cd zenith-gaming-daemon

# Configure & build with CMake (C++20)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run test suite
ctest --test-dir build --output-on-failure

# Install system-wide
sudo cmake --install build
```

### Installation via Arch Linux PKGBUILD

```bash
cd aur
makepkg -si
```

---

## 🎮 Usage

### 1. Daemon Mode (Recommended)
Enable and start the Zenith systemd service:

```bash
sudo systemctl enable --now zenith.service
```
Zenith now operates silently in the background, consuming negligible CPU cycles when idle (<0.01% via single-threaded epoll reactor, <6 MB RSS). Whenever you launch any game (Steam, Lutris, Heroic, Wine, Bottles, Native), optimizations engage automatically.

### 2. Steam Launch Options (Wrapper Mode)
Alternatively, you can launch specific games directly through Zenith:

Right-click any game in Steam -> **Properties** -> **Launch Options**:
```bash
zenith run %command%
```

### 3. Manual CLI Commands

```bash
# Check CPU topology, cache detection, and active game state
zenith status

# Manually trigger gaming optimizations for a standalone session
sudo zenith tune

# Restore system to default power-saving state
sudo zenith revert
```

---

## ⚙️ Configuration (`/etc/zenith/zenith.conf`)

```ini
# Dynamic cgroup v2 core isolation
cgroup_isolation_enabled = true

# Prioritize AMD 3D V-Cache CCD cores
vcache_pinning_enabled = true

# Prioritize Intel Performance (P) cores over E-cores
hybrid_pinning_enabled = true

# Disable split lock mitigation to prevent Wine/DirectX stutter
split_lock_mitigation_disabled = false

# Transparent HugePages (madvise for safety)
thp_always_enabled = false

# Lock GPU performance levels (prevents memory clock downclocking)
gpu_clock_lock_enabled = false

# PipeWire real-time audio quantum
pipewire_quantum_tuning = true
pipewire_latency_numerator = 64
pipewire_latency_denominator = 48000

# Virtual memory latency tuning
swappiness = 10
dirty_ratio = 5
compaction_proactiveness = 0
```

---

## 📊 Empirical Benchmarks & Reproducibility

Zenith enforces a strict engineering principle: **"Measure, don't believe."** All tweaks are evaluated through an A/B benchmark runner across $\ge 10$ iterations with 95% Confidence Intervals. Any tweak without proven benefit is disabled by default.

### Reproduce Benchmarks with One Command

```bash
# Run quick benchmark sweep and generate statistical summary + Matplotlib charts:
./bench/run_benchmarks.sh quick

# Run full A/B and ablation sweep across all profiles:
./bench/run_benchmarks.sh all
```

Output reports and charts are generated in `bench/results/`:
- `sweep_<timestamp>.csv` & `.json`: 95% Confidence Intervals, Medians, and Stutter distributions.
- `benchmark_fps_comparison.png`: Comparison of Average FPS, 1% Lows, and 0.1% Lows.
- `benchmark_jitter_stutters.png`: Frametime variance and microstutter event frequency.
- `daemon_metrics.txt`: Microsecond-precision startup and memory footprint metrics.

For the comprehensive benchmarking methodology, hardware tier matrix, and empirical tweak decision table, see [docs/BENCHMARKS.md](docs/BENCHMARKS.md).

---

## 📜 License

Distributed under the [MIT License](LICENSE).
Copyright (c) 2026 Emir ([NEGG0](https://github.com/NEGG0)).
