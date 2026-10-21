# Optimization Profiles & Adaptive Controller

Zenith organizes its performance tweaks into three distinct, carefully calibrated tiers: **Safe**, **Balanced** (default), and **Aggressive**. 

Additionally, Zenith features an **Adaptive Controller with Hysteresis** that continuously monitors system pressure (PSI) and temperatures to prevent thermal throttling or memory starvation.

---

## Profile Tiers Overview

| Feature / Tweak | Safe | Balanced (Default) | Aggressive |
| :--- | :---: | :---: | :---: |
| **cgroups v2 Background Isolation** | ✓ (Light) | ✓ (Dedicated Slice) | ✓ (Hard Throttled) |
| **AMD 3D V-Cache CCD Pinning** | ✓ | ✓ | ✓ |
| **Intel P-Core / E-Core Affinity** | ✓ | ✓ | ✓ |
| **Process Nice Priority** | `nice -5` | `nice -10` | `nice -15` / `SCHED_RR` |
| **Background Nice Demotion** | `nice 10` | `nice 19` | `nice 19` + `idle` class |
| **VM Swappiness Tuning** | ✗ (Untouched) | `vm.swappiness=10` | `vm.swappiness=5` |
| **VM Compaction Proactiveness** | ✗ (Untouched) | `0` (Paused) | `0` (Paused) |
| **Dirty Ratio Flushing** | ✗ (Untouched) | `5` | `3` |
| **Split-Lock Mitigation** | ✗ (Enabled: 1) | ✗ (Enabled: 1) | **Disabled (0)** |
| **Transparent HugePages (THP)** | ✗ (Untouched) | `madvise` | `always` |
| **GPU Clock Lock (NVML / DPM)** | ✗ (Dynamic) | ✗ (Dynamic) | **Locked High** |
| **PipeWire Real-Time Quantum** | ✗ (System Default)| `64/48000` (1.33ms) | `32/48000` (0.67ms) |
| **Network Queue Scheduling** | ✗ (Untouched) | `CAKE` / `FQ_CODEL` | `CAKE` + No Power-Save |
| **IRQ Affinity Re-routing** | ✗ (Untouched) | ✓ (GPU isolated) | ✓ (Network/NVMe moved) |

---

## 1. Safe Profile (`safe`)
* **Philosophy**: Conservative baseline. Standard power draw and thermal limits.
* **Intended Audience**: Handhelds on battery power (Steam Deck, ROG Ally), laptops, enterprise developer workstations, systems with marginal cooling.
* **Mechanism**: Binds game threads strictly through CPU affinity (CCD0 or P-cores) and moves browser/Discord background processes to core 0 via cgroups v2. Leaves all `sysctl`, memory, and GPU power domains completely untouched.

---

## 2. Balanced Profile (`balanced` - Default)
* **Philosophy**: Standard desktop profile. Balances frame pacing with system stability and energy efficiency.
* **Intended Audience**: Modern desktop gaming rigs (AMD Ryzen 5000/7000/9000, Intel 12th-14th Gen, Core Ultra).
* **Mechanism**: 
  - Engages full 3D V-Cache / P-Core affinity pinning.
  - Modulates virtual memory parameters (`vm.swappiness=10`, `vm.dirty_ratio=5`, `vm.compaction_proactiveness=0`) to eliminate background IO hitching.
  - Lowers PipeWire quantum to 64 samples (1.33ms latency).
  - Isolates GPU interrupt lines (IRQs) away from background housekeeping cores.

---

## 3. Aggressive Profile (`aggressive`)
* **Philosophy**: Maximum performance profile. Prioritizes 0.1% frame pacing above power consumption and thermal conservation.
* **Intended Audience**: Esports players, competitive titles (CS2, Apex Legends, Overwatch), high-refresh displays (165Hz-360Hz+), systems with robust cooling.
* **Mechanism**:
  - Disables split-lock mitigation (`kernel.split_lock_mitigate=0`), eliminating atomic instruction stalls in DirectX/Wine.
  - Locks GPU memory and compute clocks to peak performance states, eliminating downclock latency.
  - Sets game threads to `SCHED_RR` with watchdog limits and nice `-15`.
  - Configures CAKE bufferbloat mitigation and disables Wi-Fi power saving.

---

## 4. The Adaptive Controller: Auto-Downgrade & Self-Healing

Zenith avoids "fire-and-forget" instability by running an **Adaptive Monitoring Engine** sampling kernel Pressure Stall Information (PSI) and hardware thermal telemetry every 2 seconds:

```
                          ┌───────────────────────────┐
                          │   Current Profile Mode    │
                          │       (Aggressive)        │
                          └─────────────┬─────────────┘
                                        │
                         [ Every 2s Telemetry Check ]
                        - /proc/pressure/memory
                        - /proc/pressure/io
                        - GPU & CPU Core Temperatures
                                        │
           ┌────────────────────────────┴────────────────────────────┐
           ▼                                                         ▼
[ Spike: Memory PSI avg10 > 25%                           [ Metrics Nominal: ]
  OR GPU Temp > 88°C ]                                   - Memory PSI < 5%
           │                                             - Temperatures Safe
           ▼                                                         │
┌───────────────────────────┐                                        ▼
│     DYNAMIC DOWNGRADE     │                             ┌────────────────────┐
│  Aggressive ──> Balanced  │                             │ HYSTERESIS COOLDOWN│
│ (Reason logged in journal)│                             │    (60 Seconds)    │
└───────────────────────────┘                             └──────────┬─────────┘
                                                                     │
                                                          [ Cooldown Expired ]
                                                                     ▼
                                                          ┌────────────────────┐
                                                          │  RESTORE PROFILE   │
                                                          │ Balanced ──> Aggr. │
                                                          └────────────────────┘
```

### Safety Thresholds:
1. **Memory Pressure (PSI)**: If `/proc/pressure/memory` `some avg10` exceeds `25.0`, THP is downgraded and background processes are restored to standard swappiness.
2. **IO Pressure (PSI)**: If `/proc/pressure/io` `some avg10` exceeds `30.0`, dirty ratio flushing is relaxed.
3. **Thermal Throttling**: If CPU temperature exceeds 90°C or GPU exceeds 88°C, GPU clock locks are immediately released and the daemon falls back to `balanced` or `safe`.
4. **Hysteresis**: Zenith requires 60 consecutive seconds of healthy telemetry before restoring a higher profile tier, preventing rapid flapping.

---

## 5. Switching Profiles

### Via `zenithctl`
```bash
# Check available profiles and active status
zenithctl profile list

# Switch to aggressive
zenithctl profile set aggressive

# Switch to safe
zenithctl profile set safe
```

### Via Per-Game TOML Profiles
Games can specify individual profiles in `profiles/<name>.toml`:
```toml
name = "Counter-Strike 2"
appid = 730
profile = "aggressive"
ccd_preference = "frequency"
```
Zenith will automatically load the game-specific profile when the game starts, and restore the default daemon profile when it exits.
