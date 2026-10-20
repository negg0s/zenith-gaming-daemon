# Troubleshooting & Diagnostic Guide

When troubleshooting performance or daemon behavior, always start with `zenithctl doctor`.

---

## 1. Automated Diagnostics: `zenithctl doctor`

Run `zenithctl doctor` from any terminal:

```bash
zenithctl doctor
```

### Interpreting Doctor Results

| Check Item | Status | Meaning & Remediation |
| :--- | :---: | :--- |
| **cgroups v2** | `[✓] PASS` | `/sys/fs/cgroup` is mounted and controllers are delegated. |
| | `[✗] FAIL` | System is on cgroups v1. Add `systemd.unified_cgroup_hierarchy=1` to kernel parameters in GRUB/systemd-boot and reboot. |
| **Kernel Version** | `[✓] PASS` | Kernel is ≥ 6.6 LTS (modern scheduler and split-lock controls supported). |
| | `[!] WARN` | Kernel < 6.6. Update kernel (`pacman -S linux`, `apt install linux-generic`, etc.). |
| **AMD 3D V-Cache Driver** | `[✓] PASS` | `amd_x3d_vcache` driver loaded on Linux ≥ 6.13. Hardware mode switching active. |
| | `[ℹ] INFO` | Not loaded. Zenith automatically falls back to userspace L3 cache topology analysis. Normal on Intel or monolithic AMD CPUs (5600, 7800X3D). |
| **Conflicting Daemons** | `[✓] PASS` | No conflicting power management or tuning services detected. |
| | `[!] WARN` | Conflicting service running (e.g. `gamemode`, `tuned`, `auto-cpufreq`, `system76-power`, `power-profiles-daemon`). See section below. |
| **Socket Permissions** | `[✓] PASS` | Current user has read/write access to `/run/zenith/zenith.sock`. |
| | `[✗] FAIL` | User not in `zenith` or `wheel` group. Run `sudo usermod -aG zenith $USER` and log back in. |
| **Pressure Stall Info (PSI)**| `[✓] PASS` | `/proc/pressure` active for adaptive controller. |
| | `[!] WARN` | PSI disabled in kernel. Add `psi=1` to kernel cmdline if running custom minimal kernel. |

---

## 2. Resolving Conflicting Services

Zenith coordinates multiple kernel subsystems. Third-party power managers can fight Zenith over CPU governors, GPU clocks, or cgroups.

### Feral GameMode (`gamemoded`)
* **Conflict**: GameMode may attempt to modify CPU governors or renice processes simultaneously.
* **Solution**: You do not need GameMode when Zenith is running; Zenith provides a superset of GameMode capabilities with crash-safe WAL journaling and cache affinity.
```bash
sudo systemctl disable --now gamemoded.service
```

### auto-cpufreq / TLP
* **Conflict**: These services dynamically downclock CPU cores to conserve power, undermining latency optimizations.
* **Solution**: Keep TLP for battery on laptops, but disable governor overriding when plugged in. On desktops:
```bash
sudo systemctl disable --now auto-cpufreq
```

### irqbalance
* **Conflict**: Moves GPU interrupts across cores randomly during gameplay.
* **Solution**: Zenith automatically pauses `irqbalance` during active game sessions and restores it on exit. If issues persist:
```bash
sudo systemctl stop irqbalance
```

---

## 3. Daemon & Socket Issues

### "Cannot connect to /run/zenith/zenith.sock"
1. Verify daemon is running:
   ```bash
   sudo systemctl status zenith.service
   ```
2. If service failed to start, inspect systemd journal:
   ```bash
   journalctl -u zenith.service -e --no-pager
   ```
3. Check permissions on the socket:
   ```bash
   ls -la /run/zenith/zenith.sock
   ```
   Should be owned by `root:zenith` with permissions `0660`.

### Daemon Refuses to Start: "Another instance is holding /run/zenith/zenith.lock"
Zenith enforces single-instance execution via `flock`. If an orphaned daemon crashed without cleaning up:
```bash
# Check what PID holds the lock
fuser /run/zenith/zenith.lock

# Kill orphaned PID if any, or remove stale socket
sudo rm -f /run/zenith/zenith.sock
sudo systemctl restart zenith.service
```

---

## 4. Emergency State Recovery (Zero-Drift Rollback)

If your system encountered an unexpected power cut or hard freeze and you want to ensure all kernel parameters are completely restored to distro stock defaults:

### Method 1: Using zenithctl
```bash
zenithctl rollback
```

### Method 2: Standalone CLI invocation
```bash
sudo zenith --rollback
```

This reads `/run/zenith/journal.json`, walks through all recorded mutations in reverse chronological order, restores the pre-game baseline values, and deletes the journal.

---

## 5. Generating an Anonymized Bug Report

When filing an issue on GitHub, please attach the output of `zenithctl report --redact`:

```bash
zenithctl report --redact > zenith_report.txt
```

This command gathers:
- Kernel version, CPU topology, and GPU driver info.
- `zenithctl doctor` results.
- Active cgroup slice configurations.
- Recent structured daemon logs.
- Sanitizes usernames, hostnames, and IP addresses automatically.
