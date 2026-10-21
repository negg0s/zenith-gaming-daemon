# Frequently Asked Questions (FAQ)

---

### 1. How is Zenith different from Feral's GameMode?
Feral GameMode is primarily a D-Bus client-server script launcher: games or launchers must explicitly link against `libgamemode` or be prefixed with `gamemoderun`.

**Zenith is an autonomous, kernel-integrated daemon:**
* **Zero Wrapper Overhead**: Zenith uses the Linux kernel's **Netlink proc connector** to detect games instantaneously (<50ms) without needing any wrapper or `%command%` edits in Steam.
* **Hardware & Cache Aware**: Zenith automatically parses CPU cache hierarchies and supports AMD 3D V-Cache dual-CCD pinning (`amd_x3d_vcache`) and Intel Hybrid P/E/LP-E core isolation. GameMode has no concept of cache topologies or dual-CCDs.
* **Safety & Zero-Drift**: Zenith uses an ACID Write-Ahead Log (`/run/zenith/journal.json`). If the system crashes or loses power, Zenith unconditionally heals the kernel state on boot.
* **cgroups v2 Background Isolation**: Zenith automatically cages Discord, browsers, and background utilities into a background slice.
* **Adaptive Controller**: Actively monitors PSI memory/IO pressure and thermals to prevent thermal throttling.

---

### 2. Is Zenith safe with Anti-Cheat (EAC, BattlEye, Ricochet, VAC)?
**Yes, 100% safe.**

Zenith is completely external to the game process:
- It **never** injects shared libraries (`.so` / `.dll`) into the game address space.
- It **never** reads or writes game memory (`process_vm_readv`, `/proc/$pid/mem`).
- It **never** uses debugging or tracing syscalls (`ptrace`).
- It **never** intercepts game graphics calls or modifies DirectX/Vulkan binaries.

Zenith interacts exclusively with the Linux kernel using standard POSIX and sysfs system calls (`sched_setaffinity`, `setpriority`, `openat2`). Anti-cheat engines running under Proton or natively view Zenith as standard OS scheduler activity.

For a full security and anti-cheat compliance analysis, see [docs/ANTI_CHEAT.md](ANTI_CHEAT.md).

---

### 3. Does Zenith work on Steam Deck / SteamOS?
**Yes.**
On the Steam Deck (monolithic AMD Van Gogh APU with single quad-core CCX):
* Zenith detects that the CPU is monolithic (single L3 cache cluster) and automatically bypasses dual-CCD pinning without errors.
* Zenith applies cgroups v2 background isolation to keep background downloads and SteamOS client tasks from causing frame pacing micro-stutters during 30/40/60 FPS capped gameplay.
* The `safe` or `balanced` profile is recommended on battery to preserve battery life.

---

### 4. Will Zenith increase my average FPS?
Zenith is engineered to fix **frame-time consistency and 0.1% / 1% FPS lows**, not to artificially inflate average FPS when a game is purely GPU-bound:
* If your GPU is running at 99% utilization, Zenith will not double your frame rate when GPU-bound.
* What Zenith **does** do is eliminate the sudden 50-100ms micro-stutters, shader-compile hiccups, L3 cache-bounce spikes, and split-lock penalties that ruin competitive gaming.
* In CPU-bound and cache-sensitive titles (e.g. *Counter-Strike 2*, *Dota 2*, *Cyberpunk 2077*, *Baldur's Gate 3*), users typically see a **15% to 35% improvement in 1% and 0.1% lows**.

---

### 5. Does Zenith support NVIDIA and Intel Arc GPUs?
**Yes.**
* **NVIDIA**: Zenith uses NVML (NVIDIA Management Library) to enable Driver Persistence Mode (`-pm 1`) and locks graphics clock boost tables to prevent downclocking latency spikes.
* **AMD**: Zenith writes directly to `power_dpm_force_performance_level` and `pp_dpm_sclk` in sysfs.
* **Intel Arc / Xe**: Zenith interfaces with `/sys/class/drm/card*/device/` frequency controls.

---

### 6. Can I still use `zenith run %command%` if I want to?
**Yes.**
Zenith includes `zenith-run` as a lightweight companion wrapper. If you prefer to optimize only specific titles or run games without the background daemon enabled, you can add `zenith-run %command%` to your Steam Launch Options.

---

### 7. How does Zenith integrate with CachyOS kernels?
CachyOS is an optimized Linux gaming distribution featuring specialized kernels (BORE, EEVDF, sched-ext). Zenith detects CachyOS environments automatically:
* If the `amd_x3d_vcache` driver is present, Zenith prioritizes hardware-level frequency/cache switching.
* If `sched_ext` (e.g. `scx_bpfland`, `scx_lavd`) is running, Zenith cooperates with the BPF scheduler and applies targeted nice/uclamp hints.
