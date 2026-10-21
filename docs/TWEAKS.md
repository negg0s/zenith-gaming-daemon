# Zenith Gaming Daemon: Kernel & Subsystem Tweaks Registry

> **Document Version**: 1.0.0  
> **Status**: Living Architectural Specification  
> **Rule**: Every tweak must be justified, documented with kernel paths, evaluated for risks, scoped to profiles, and tagged `[Эффект не измерен]` until empirical benchmark verification in Stage 9.

---

## 1. Profiles Architecture

Zenith supports three operational profiles that dictate which tweaks are active and their tuning bounds:

| Profile | Philosophy | Risk Profile | Target Systems |
| :--- | :--- | :--- | :--- |
| **`safe`** | Near-zero risk; zero hardware clock overrides; soft cgroup prioritization; latency-tolerant defaults. | Near-zero risk of crashes, overheating, or starvation. | Battery devices, laptops, daily workstations, unstable hardware. |
| **`balanced`** (Default) | High-performance latency optimization without destructive overrides; soft cgroups; EPP `balance_performance`; THP `madvise`; NVMe `none`. | Low risk; tested across standard desktop environments. | Primary desktop gaming rigs running Arch Linux, CachyOS, or SteamOS. |
| **`aggressive`** | Maximum throughput and minimum frame latency; split-lock penalty disabled; GPU clock lock; `SCHED_RR` priority with hardware watchdog; aggressive PipeWire quantum (32/64). | Elevated thermals, power consumption, potential stutter if misconfigured. | Dedicated competitive gaming desktop PCs with adequate cooling. |

---

## 2. Exhaustive Tweaks Registry

### Tweak 01: `split_lock_mitigate`
- **Kernel Path**: `/proc/sys/kernel/split_lock_mitigate`
- **Min Kernel**: Linux 5.19+ (introduced via x86 bus lock detection patchsets)
- **Mechanism**: The Linux kernel intentionally penalizes (slows down via sleeping/signals) threads that execute unaligned atomic operations crossing cache line boundaries ("split locks"), which would otherwise lock the entire memory bus. Setting this to `0` disables kernel-induced latency penalties.
- **Pros**: Eliminates micro-stutters and frame drops in legacy Windows games running under Proton/Wine that execute split locks.
- **Risks**: An application executing split locks at scale can saturate memory bus bandwidth, hurting overall system throughput.
- **Where it hurts**: Multi-threaded production servers or systems running adversarial code.
- **Default**: `0` in `aggressive`, `1` (untouched) in `safe` and `balanced`.
- **Profiles**: `aggressive` only.
- **Source**: Linux Kernel Documentation (`Documentation/admin-guide/sysctl/kernel.rst`).
- **Benchmark Status**: `[Эффект не измерен]`

---

### Tweak 02: `transparent_hugepage_mode`
- **Kernel Path**: `/sys/kernel/mm/transparent_hugepage/enabled`
- **Min Kernel**: Linux 2.6.38+
- **Mechanism**: Controls whether the kernel allocates 2MB memory pages instead of standard 4KB pages. Values: `always`, `madvise`, `never`.
- **Pros**: Drastically reduces TLB (Translation Lookaside Buffer) misses for memory-intensive modern game engines (Unreal Engine 5, Frostbite, Cyberpunk 2077).
- **Risks**: `always` can cause memory bloating (internal fragmentation) and latency spikes during page compaction if free contiguous 2MB blocks are scarce.
- **Where it hurts**: Systems with limited RAM (<16 GB), apps that frequently allocate small scattered buffers.
- **Default**: `madvise` in `safe` and `balanced`; `always` in `aggressive`.
- **Profiles**: `safe` (`madvise`), `balanced` (`madvise`), `aggressive` (`always`).
- **Source**: Linux Kernel Documentation (`Documentation/admin-guide/mm/transhuge.rst`).
- **Benchmark Status**: `[Эффект не измерен]`

---

### Tweak 03: `transparent_hugepage_defrag`
- **Kernel Path**: `/sys/kernel/mm/transparent_hugepage/defrag`
- **Min Kernel**: Linux 2.6.38+
- **Mechanism**: Controls whether memory allocation synchronously stalls to compact memory when a huge page is requested but unavailable. Values: `always`, `defer`, `defer+madvise`, `madvise`, `never`.
- **Pros**: Setting to `defer+madvise` prevents memory allocations in the critical game rendering loop from blocking on direct compaction, offloading compaction to `kcompactd` in the background.
- **Risks**: If background compaction cannot keep up, allocations fall back to 4KB pages.
- **Where it hurts**: None in gaming; strictly superior to synchronous `always`.
- **Default**: `defer+madvise` in `balanced` and `aggressive`; untouched in `safe`.
- **Profiles**: `balanced`, `aggressive`.
- **Source**: Red Hat Performance Tuning Guide / Kernel transhuge docs.
- **Benchmark Status**: `[Эффект не измерен]`

---

### Tweak 04: `compaction_proactiveness`
- **Kernel Path**: `/proc/sys/vm/compaction_proactiveness`
- **Min Kernel**: Linux 5.9+
- **Mechanism**: Controls how aggressively the kernel proactive compaction daemon (`kcompactd`) defragments memory before allocations happen (0 to 100).
- **Pros**: Setting to `0` disables proactive background compaction during gaming, eliminating periodic background CPU thread wakeups that cause frametime jitter.
- **Risks**: When THP is set to `always`, setting compaction proactiveness to 0 can increase the probability that future 2MB allocations fall back to 4KB.
- **Where it hurts**: Long-running background processes requiring hugepages over days.
- **Default**: `0` in `balanced` and `aggressive`; untouched in `safe`.
- **Profiles**: `balanced`, `aggressive`.
- **Source**: Linux Kernel commit `c89511ab2f8f` ("mm: proactive compaction").
- **Benchmark Status**: `[Эффект не измерен]`

---

### Tweak 05: `watermark_boost_factor`
- **Kernel Path**: `/proc/sys/vm/watermark_boost_factor`
- **Min Kernel**: Linux 5.0+
- **Mechanism**: Directs `kswapd` to boost memory watermarks when memory fragmentation is detected, causing aggressive memory reclaiming. Setting to `0` disables memory watermark boosting.
- **Pros**: Eliminates erratic `kswapd` spikes and memory reclaiming jitter during gameplay when loading new assets or textures into RAM.
- **Risks**: Under severe memory pressure, direct reclaim may occur earlier.
- **Where it hurts**: Systems with less than 8 GB RAM experiencing extreme memory pressure.
- **Default**: `0` in `balanced` and `aggressive`; untouched in `safe`.
- **Profiles**: `balanced`, `aggressive`.
- **Source**: Linux Kernel Documentation (`Documentation/admin-guide/sysctl/vm.rst`).
- **Benchmark Status**: `[Эффект не измерен]`

---

### Tweak 06: `swappiness_and_page_cluster`
- **Kernel Path**: `/proc/sys/vm/swappiness`, `/proc/sys/vm/page-cluster`
- **Min Kernel**: Linux 2.6+
- **Mechanism**: `swappiness` controls relative preference for swapping anonymous memory vs reclaiming page cache. `page-cluster` controls the number of pages read/written in a single swap I/O chunk ($2^{\text{page\_cluster}}$). For zram (compressed in-RAM swap), sequential disk clusters are counterproductive.
- **Pros**: For zram systems, `page-cluster = 0` (1 page per swap read/write) avoids decompressing unneeded adjacent pages, dramatically reducing zram CPU latency. Lowering `swappiness` to 10 on disk swap prevents paging active game working sets.
- **Risks**: Setting `page-cluster = 0` on spinning mechanical hard disks (HDD) causes severe performance degradation due to lack of sequential read clustering. Zenith probes whether swap is zram before applying.
- **Where it hurts**: Mechanical hard disk swap partitions.
- **Default**: `swappiness = 10`, `page-cluster = 0` (only if zram detected).
- **Profiles**: `safe`, `balanced`, `aggressive`.
- **Source**: CachyOS Kernel Documentation & zram Linux documentation.
- **Benchmark Status**: `[Эффект не измерен]`

---

### Tweak 07: `cpu_energy_performance_preference` (EPP)
- **Kernel Path**: `/sys/devices/system/cpu/cpu*/cpufreq/energy_performance_preference`
- **Min Kernel**: Linux 5.17+ (AMD P-State / Intel P-State)
- **Mechanism**: Hardware autonomous performance hinting register (MSR / CPPC). Informs CPU core firmware whether to ramp clocks aggressively at the first hint of load.
- **Pros**: Setting to `performance` or `balance_performance` eliminates the 5-15ms clock ramp-up latency when frame rendering threads experience brief bursty spikes.
- **Risks**: Higher idle power draw and higher operating temperatures.
- **Where it hurts**: Battery life on laptops.
- **Default**: `balance_performance` in `safe`, `performance` in `balanced` and `aggressive`.
- **Profiles**: `safe` (`balance_performance`), `balanced` (`performance`), `aggressive` (`performance`).
- **Source**: AMD P-State EPP Kernel Documentation.
- **Benchmark Status**: `[Эффект не измерен]`

---

### Tweak 08: `cgroup_soft_prioritization`
- **Kernel Path**: `/sys/fs/cgroup/zenith_background/cpu.weight`, `cpu.idle`, `io.weight`, `memory.high`
- **Min Kernel**: Linux 5.10+ (cgroups v2 unified hierarchy)
- **Mechanism**: Instead of strictly hard-pinning background apps (Chrome, Discord, Spotify) to 1 CPU core (which causes audio crackle and IPC freezes), background apps are assigned `cpu.weight = 50` (vs default 100), `cpu.idle = 1` (SCHED_IDLE semantics), and `io.weight = 50`.
- **Pros**: Gives 100% CPU priority to games when they need it, while allowing background apps to run smoothly when game threads are idle or waiting on GPU/V-Sync.
- **Risks**: None. Eliminates the audio-stutter risks associated with hard cpuset eviction.
- **Where it hurts**: Heavy background compilation running simultaneously with games.
- **Default**: Enabled across all profiles.
- **Profiles**: `safe`, `balanced`, `aggressive`.
- **Source**: Linux cgroups v2 Kernel Documentation (`Documentation/admin-guide/cgroup-v2.rst`).
- **Benchmark Status**: `[Эффект не измерен]`

---

### Tweak 09: `sched_uclamp_min`
- **Kernel Path**: Syscall `sched_setattr` with `SCHED_FLAG_UTIL_CLAMP_MIN`
- **Min Kernel**: Linux 5.3+ (CONFIG_UCLAMP_TASK)
- **Mechanism**: Utilization clamping (uclamp) informs the kernel scheduler (CFS / EEVDF) of the minimum compute capacity that should be allocated to a thread, preventing the frequency governor from downclocking cores during frame delivery.
- **Pros**: Guarantees game render threads operate above a floor frequency without locking CPU frequency globally to maximum.
- **Risks**: Uncalibrated uclamp values on low-tier CPUs can cause thermal throttling.
- **Where it hurts**: Thermal-constrained laptops.
- **Default**: `uclamp.min = 512` (50%) in `balanced`, `1024` (100%) in `aggressive`; untouched in `safe`.
- **Profiles**: `balanced`, `aggressive`.
- **Source**: Linux Scheduler Utilization Clamping Architecture (`Documentation/scheduler/sched-util-clamping.rst`).
- **Benchmark Status**: `[Эффект не измерен]`

---

### Tweak 10: `timer_slack_ns`
- **Kernel Path**: Syscall `prctl(PR_SET_TIMERSLACK, ...)`
- **Min Kernel**: Linux 2.6.28+
- **Mechanism**: Configures the allowable delay the kernel may group timers together to save power. Default is 50,000 ns (50 µs).
- **Pros**: Setting timer slack to `50 ns` for game processes ensures microsecond-accurate sleeps (`nanosleep`, `epoll_wait`, SDL frame pacers), minimizing frame pacing jitter.
- **Risks**: Marginally higher timer interrupt rate.
- **Where it hurts**: Power consumption on battery.
- **Default**: `50 ns` in `balanced` and `aggressive`; untouched in `safe`.
- **Profiles**: `balanced`, `aggressive`.
- **Source**: `prctl(2)` man page.
- **Benchmark Status**: `[Эффект не измерен]`

---

### Tweak 11: `nvme_io_scheduler_and_readahead`
- **Kernel Path**: `/sys/block/nvme*/queue/scheduler`, `/sys/block/nvme*/queue/read_ahead_kb`
- **Min Kernel**: Linux 4.12+ (blk-mq)
- **Mechanism**: NVMe devices have massive hardware command queues (64K queues with 64K commands). Software elevator schedulers (`mq-deadline`, `bfq`) introduce CPU locking overhead. Setting scheduler to `none` allows direct hardware queue submission.
- **Pros**: Reduces I/O latency by up to 20% during game asset streaming (DirectStorage / fast load times).
- **Risks**: None on modern NVMe drives.
- **Where it hurts**: Mechanical spinning hard disks or slow SATA drives (where BFQ is beneficial). Zenith strictly targets `nvme*` block devices.
- **Default**: `none` for NVMe block queues; `read_ahead_kb = 128` (or 256).
- **Profiles**: `safe`, `balanced`, `aggressive`.
- **Source**: Linux Block MQ Documentation (`Documentation/block/blk-mq.rst`).
- **Benchmark Status**: `[Эффект не измерен]`

---

### Tweak 12: `usb_autosuspend_inhibitor`
- **Kernel Path**: `/sys/bus/usb/devices/*/power/control`
- **Min Kernel**: Linux 2.6.35+
- **Mechanism**: Linux power management autosuspends idle USB devices. Setting `power/control` to `on` disables power autosuspend for USB Human Interface Devices (HID mice, keyboards, controllers).
- **Pros**: Eliminates "first movement" input latency and mouse wake-up lag caused by USB power sleep transitions.
- **Risks**: Very minor increase in idle power consumption (milliwatts).
- **Where it hurts**: Battery life when computer is left idle for hours.
- **Default**: Enabled for USB HID devices in `balanced` and `aggressive`.
- **Profiles**: `balanced`, `aggressive`.
- **Source**: Linux USB Power Management (`Documentation/driver-api/usb/power-management.rst`).
- **Benchmark Status**: `[Эффект не измерен]`

---

### Tweak 13: `rt_priority_and_watchdog`
- **Kernel Path**: Syscall `sched_setscheduler(pid, SCHED_RR, &param)`
- **Min Kernel**: Linux 2.6+
- **Mechanism**: Elevates main rendering thread to Round-Robin Real-Time scheduling (`SCHED_RR`).
- **Safety Safeguards**:
  1. Respects `kernel.sched_rt_runtime_us` (default: 950,000 µs out of 1,000,000 µs reserved for RT, guaranteeing 5% CPU for system processes to prevent total machine freeze).
  2. Priority ceiling: capped strictly at `rt_priority = 20` (never max 99, preventing starvation of kernel RT threads like PipeWire audio server or display compositor).
  3. Built-in Watchdog: Zenith monitors the RT thread; if the thread does not yield within 3 seconds, Zenith demotes it back to `SCHED_OTHER` with `nice = -20`.
- **Pros**: Guarantees the frame delivery thread is never preempted by background desktop processes.
- **Risks**: Misbehaved non-yielding game loops could cause audio or input dropouts without our watchdog.
- **Where it hurts**: Single-core CPUs or CPU-starved environments.
- **Default**: Disabled in `safe` and `balanced` (uses `nice = -10`); enabled in `aggressive` with watchdog.
- **Profiles**: `aggressive` only.
- **Source**: Linux Real-Time Scheduling Documentation.
- **Benchmark Status**: `[Эффект не измерен]`

---

### Tweak 14: `pipewire_quantum_tuning`
- **Kernel Path**: `/usr/bin/pw-metadata -n settings 0 clock.quantum <quantum>`
- **Min Kernel / App**: PipeWire 0.3+
- **Mechanism**: Adjusts the audio buffer quantum size (samples per buffer).
- **Pros**: Drastically reduces sound effect latency (gunshots, footsteps) in competitive FPS titles.
- **Risks**: Setting quantum too low (e.g. 32 on USB audio interfaces without low-latency DACs) causes audible buffer underruns ("xruns"), clicks, and popping.
- **Where it hurts**: High-latency USB DACs, complex audio routing chains with DSP plugins.
- **Default**: `128` in `safe`, `64` in `balanced`, `32` in `aggressive`.
- **Profiles**: `safe` (128), `balanced` (64), `aggressive` (32).
- **Source**: PipeWire Documentation (`https://gitlab.freedesktop.org/pipewire/pipewire/-/wikis/Config-PipeWire`).
- **Benchmark Status**: `[Эффект не измерен]`

---

### Tweak 15: `gpu_performance_mode`
- **Kernel Path**: `/sys/class/drm/card*/device/power_dpm_force_performance_level`, `nvidia-smi -pm 1 -lgc`
- **Min Kernel**: Linux 4.10+ (AMD DRM) / Proprietary NVIDIA Driver
- **Mechanism**: Locks GPU core and memory clocks to maximum performance states to prevent downclocking during light scenes.
- **Pros**: Eliminates stutter when transitioning from low-complexity scenes (menus, indoor) to high-complexity outdoor rendering.
- **Risks**: High GPU power consumption, increased fan noise, elevated temperatures.
- **Where it hurts**: Laptops, thermal-limited SFF (Small Form Factor) cases, energy bills.
- **Default**: Disabled in `safe`, dynamic in `balanced`, locked in `aggressive`.
- **Profiles**: `aggressive` (locked), `balanced` (`auto` or high without lock), `safe` (`auto`).
- **Source**: AMDGPU Driver Documentation (`Documentation/gpu/amdgpu/thermal.rst`).
- **Benchmark Status**: `[Эффект не измерен]`

---

### Tweak 16: `network_qdisc_cake`
- **Kernel Path**: `net.core.default_qdisc = cake`, `tc qdisc replace dev <iface> root cake`
- **Min Kernel**: Linux 4.19+ (SCHED_CAKE)
- **Mechanism**: Common Applications Kept Enhanced (CAKE) replaces FIFO/pfifo_fast with smart deficit round-robin and active queue management (AQM), preventing bufferbloat.
- **Pros**: Reduces ping spikes when background downloads or updates occur concurrently with online multiplayer gaming.
- **Risks**: Incompatible with certain enterprise traffic shaping setups.
- **Where it hurts**: Virtual interfaces, VPN tap adapters.
- **Default**: `cake` if supported, fallback to `fq_codel`.
- **Profiles**: `safe`, `balanced`, `aggressive`.
- **Source**: Bufferbloat.net / CAKE Linux documentation.
- **Benchmark Status**: `[Эффект не измерен]`
