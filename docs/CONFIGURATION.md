# Configuration Guide (`/etc/zenith/zenith.conf`)

Zenith enforces a strict configuration format: unknown keys, invalid types, or out-of-range integer values result in startup validation rejection to prevent silent misconfigurations.

Default location: `/etc/zenith/zenith.conf`  
Profiles override location: `/etc/zenith/profiles.conf`

---

## Configuration Keys Reference

### 1. `version`
* **Type**: `string`
* **Default**: `"1.1.0"`
* **Allowed Values**: Semantic version string (e.g. `"1.0.0"`, `"1.1.0"`).
* **Description**: Declares the schema version of the configuration file. Unversioned configs are detected as `1.0.0` and can be migrated automatically via `ConfigMigrator`.

---

### 2. `profile` (alias: `active_profile`)
* **Type**: `string`
* **Default**: `balanced`
* **Allowed Values**: `safe`, `balanced`, `aggressive`
* **Description**: The default tuning profile engaged when an active game session is detected.
  - `safe`: Zero-risk optimizations. No sysctl mutations, no GPU clock lock. Ideal for battery power or sensitive enterprise machines.
  - `balanced`: Recommended default. AMD 3D V-Cache / Intel P-core pinning, mild VM tuning, and cgroups v2 background isolation.
  - `aggressive`: Maximum competitive performance. Split-lock disabled, GPU clock lock, SCHED_RR real-time priority, and CAKE network queueing.

---

### 3. `cgroup_isolation_enabled`
* **Type**: `boolean` (`true`, `false`, `1`, `0`)
* **Default**: `true`
* **Participating Profiles**: `safe`, `balanced`, `aggressive`
* **Mechanism**: Creates `/sys/fs/cgroup/zenith_background` with restricted CPU affinity (`cpuset.cpus = 0` or housekeeping cores). Moves known resource-hungry background applications (Discord, Chromium, Steam Web Helper, torrents, OBS preview) away from game render cores.
* **Performance Impact**: Eliminates frame-time jitter caused by browser tabs or voice chat threads preempting the game render loop.
* **Failure Mode**: Non-fatal fallback; if cgroups v2 controllers are missing, Zenith falls back to `sched_setaffinity`.

---

### 4. `vcache_pinning_enabled`
* **Type**: `boolean`
* **Default**: `true`
* **Participating Profiles**: `balanced`, `aggressive`
* **Mechanism**: Integrates with the Linux 6.13+ `amd_x3d_vcache` driver via `/sys/bus/platform/drivers/amd_x3d_vcache/*/amd_x3d_mode` with fallback to kernel CPU cache index3 analysis (`/sys/devices/system/cpu/cpu*/cache/index3/`). Automatically detects 3D V-Cache CCD cores (Ryzen 7900X3D / 7950X3D) and pins game and Proton worker threads to the cache CCD.
* **Performance Impact**: 15-30% improvement in 1% and 0.1% FPS lows in cache-sensitive titles; prevents cross-CCD latency penalties (~75ns inter-core transit).
* **Failure Mode**: Automatically bypassed on monolithic CPUs (e.g. 7800X3D, 5600X, Steam Deck) without error.

---

### 5. `hybrid_pinning_enabled`
* **Type**: `boolean`
* **Default**: `true`
* **Participating Profiles**: `balanced`, `aggressive`
* **Mechanism**: Reads Intel `core_type` and `cpu_capacity` from sysfs to distinguish Performance (P), Efficient (E), and Low-Power Island (LP-E) cores. Pins primary game render threads to P-cores while delegating background tasks to E-cores.
* **Performance Impact**: Eliminates thread migration stalls when Intel Thread Director incorrectly assigns heavy game compute threads to slow E-cores under Linux.

---

### 6. `sysctl_tuning_enabled`
* **Type**: `boolean`
* **Default**: `true`
* **Participating Profiles**: `balanced`, `aggressive`
* **Mechanism**: Master switch governing virtual memory, dirty ratio, and scheduler adjustments in `/proc/sys/`.

---

### 7. `split_lock_mitigation_disabled`
* **Type**: `boolean`
* **Default**: `false` (`true` in `aggressive` profile)
* **Kernel Path**: `/proc/sys/kernel/split_lock_mitigate`
* **Minimum Kernel**: 5.19+
* **Mechanism**: Sets `split_lock_mitigate = 0` during active gameplay. The Linux kernel's default mitigation intentionally slows down processes that execute unaligned atomic instructions (widespread in DirectX 9-11 and Unreal Engine titles under Proton/Wine) by up to 100 milliseconds per event.
* **Performance Impact**: Completely cures 50-100ms micro-stutters in affected Wine/Proton games.
* **Risks**: Malicious user applications on multi-tenant servers could theoretically issue split locks to induce bus lock bus contention. Fully mitigated by reverting to `1` when games exit.

---

### 8. `thp_always_enabled`
* **Type**: `boolean`
* **Default**: `false`
* **Kernel Path**: `/sys/kernel/mm/transparent_hugepage/enabled`
* **Mechanism**: When `false`, Zenith uses safe `madvise` with `defrag = defer+madvise`. When `true`, forces THP `always`.
* **Performance Impact**: Reduces TLB page misses in large memory footprint titles; `madvise` is safest to prevent memory bloat on <16 GB systems.

---

### 9. `gpu_clock_lock_enabled`
* **Type**: `boolean`
* **Default**: `false` (`true` in `aggressive` profile)
* **Participating Profiles**: `aggressive`
* **Mechanism**:
  - **AMD Radeon**: Sets `/sys/class/drm/card*/device/power_dpm_force_performance_level` to `high`.
  - **NVIDIA GeForce**: Calls NVML to set Persistence Mode (`-pm 1`) and locks graphics clock boost tables.
* **Performance Impact**: Prevents GPU memory and compute clock downclocking during menus or lower-complexity scenes, eliminating the latency penalty when action resumes.
* **Risks**: Increases power consumption and heat generation. Governed by Zenith's adaptive controller: automatically downgraded if GPU temperatures cross thermal throttle thresholds (e.g. >88°C).

---

### 10. `realtime_process_priority`
* **Type**: `boolean`
* **Default**: `true`
* **Participating Profiles**: `balanced`, `aggressive`
* **Mechanism**: Renices game main threads to `nice -10` (or `SCHED_RR` with priority ceiling 20 if permitted by `rt_runtime_us`). Background tasks are set to `nice 19` / `idle` class.
* **Safety Watchdog**: Zenith enforces priority caps and active watchdog threads to prevent thread starvation or system hangs.

---

### 11. `pipewire_quantum_tuning`
* **Type**: `boolean`
* **Default**: `true`
* **Participating Profiles**: `balanced`, `aggressive`
* **Mechanism**: Interacts with the PipeWire daemon via `pw-metadata` to set `clock.force-quantum` and `clock.force-rate`.
* **Performance Impact**: Cuts audio latency from default 21.3ms down to 1.33ms (64 samples @ 48kHz), critical for competitive shooters (CS2, Apex, Overwatch).

---

### 12. `pipewire_latency_numerator`
* **Type**: `integer`
* **Default**: `64`
* **Allowed Range**: `16` to `8192`
* **Description**: PipeWire buffer quantum size in audio samples. `64` yields 1.33ms; `32` yields 0.67ms (requires high-end DAC/interface).

---

### 13. `pipewire_latency_denominator`
* **Type**: `integer`
* **Default**: `48000`
* **Allowed Range**: `44100` to `192000`
* **Description**: Target audio sample rate in Hz. Standard is `48000` (48 kHz).

---

### 14. `swappiness`
* **Type**: `integer`
* **Default**: `10`
* **Allowed Range**: `0` to `200`
* **Kernel Path**: `/proc/sys/vm/swappiness`
* **Description**: Controls kernel aggression in swapping anonymous memory vs page cache. Lower values (10) protect active gaming memory from being prematurely paged out.

---

### 15. `dirty_ratio`
* **Type**: `integer`
* **Default**: `5`
* **Allowed Range**: `1` to `90`
* **Kernel Path**: `/proc/sys/vm/dirty_ratio`
* **Description**: Percentage of system RAM that can hold dirty data before background flush starts. A lower dirty ratio prevents bursty multi-gigabyte disk write freezes.

---

### 16. `compaction_proactiveness`
* **Type**: `integer`
* **Default**: `0`
* **Allowed Range**: `0` to `100`
* **Kernel Path**: `/proc/sys/vm/compaction_proactiveness`
* **Description**: Setting to `0` suspends proactive memory defragmentation loops in the background while gaming, eliminating intermittent kernel CPU stalls.

---

### 17. `poll_interval_ms`
* **Type**: `integer`
* **Default**: `500`
* **Allowed Range**: `50` to `10000`
* **Description**: Polling heartbeat interval in milliseconds for process fallback scanning when Netlink proc connector events are unavailable.
