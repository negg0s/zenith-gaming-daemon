# Zenith Gaming Daemon: Threat Model & Security Architecture

> **Document Version**: 2.0.0  
> **Classification**: Public Engineering Security Specification  
> **Methodology**: STRIDE & Privilege Boundary Threat Modeling

---

## 1. Security Context & Assets

`zenith-gaming-daemon` operates as an ambient root service on Linux systems (Arch Linux, CachyOS, SteamOS). Its express purpose is dynamic, low-latency reconfiguration of kernel parameters, CPU affinity masks, GPU hardware clocks, and cgroups v2 slices during gameplay.

### Critical Assets:
1. **Operating System Integrity**: Kernel sysctl stability, file system consistency, prevention of arbitrary file overwrites.
2. **CPU & Scheduler Fairness**: Preventing untrusted local users from monopolizing all CPU cores or starving system-critical daemons (`dbus`, `pipewire`, `systemd-journald`).
3. **Hardware & Thermal Safety**: Preventing malicious, unbounded GPU/VRAM clock or voltage overclocks that exceed manufacturer TDP.
4. **Process Boundary Integrity**: Ensuring background applications (browsers, Discord, password managers) cannot be manipulated through improper IPC or cgroup leaks.
5. **State Journal & Lock File**: `/run/zenith/journal.json` and `/run/zenith/zenith.lock` must remain tamper-proof.

---

## 2. Trust Boundaries & Attacker Profiles

```
  [ Untrusted User Space ]
  (Steam Client / Games / Browser / Discord / Local User)
          │
  ───────┼─────────────────────────────────────────────────────────────
          │ Trust Boundary 1: UNIX Domain Socket (/run/zenith/zenith.sock)
          │ Checked via SO_PEERCRED (UID/GID verification)
          ▼
  [ Zenith CLI Client (Unprivileged) ]
          │
  ───────┼─────────────────────────────────────────────────────────────
          │ Trust Boundary 2: Config Files (/etc/zenith/zenith.conf)
          │ Strict bounds, size limits, unknown key rejection
          ▼
  [ Zenith Gaming Daemon (Privileged Core) ]
          │ Restricted Capabilities: CAP_SYS_NICE, CAP_SYS_ADMIN, CAP_SYS_RESOURCE
          │
  ───────┼─────────────────────────────────────────────────────────────
          │ Trust Boundary 3: Virtual Filesystems (/proc, /sys, /sys/fs/cgroup)
          │ Sandboxed via openat2 (RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS)
          ▼
  [ Linux Kernel & Hardware Controllers ]
  (CPU Topology / Sysctl / AMD DPM / NVIDIA SMI / PipeWire)
```

### Threat Actors:
- **Local Unprivileged User**: An unprivileged user or compromised desktop process attempting privilege escalation to root.
- **Malicious Application / Binary Name**: A malicious executable with crafted `/proc/<pid>/cmdline` or path traversal strings (`../../etc/shadow`).
- **Malicious / Tampered Config**: Maliciously edited `/etc/zenith/zenith.conf` attempting buffer overflows, integer wraparounds, or resource exhaustion.
- **Socket Impersonator**: An unauthorized local process attempting to spoof IPC messages to alter hardware states.

---

## 3. STRIDE Threat Analysis & Defense Strategy

| Threat Category | Attack Vector | Potential Impact | Zenith Mitigation Architecture |
| :--- | :--- | :--- | :--- |
| **Spoofing** | Unprivileged client connects to `/run/zenith/zenith.sock` to trigger system commands | Unauthorized root actions | **Kernel-enforced `SO_PEERCRED`**: Daemon queries peer credentials directly from the kernel. Non-root / non-wheel users receive immediate `EPERM`. |
| **Tampering** | Symlink attack replacing target file in `/proc` or `/sys` with `/etc/passwd` | Arbitrary file write / Root takeover | **`openat2` with `RESOLVE_BENEATH \| RESOLVE_NO_SYMLINKS`**: Daemon traverses virtual filesystems strictly through verified dirfds with symlink following disabled. |
| **Tampering** | Command injection via crafted game executable names | Shell code execution | **Zero Shell Policy**: Total elimination of `std::system()` and `popen()`. All external utilities executed via `execve` using strict absolute-path whitelists (`/usr/bin/nvidia-smi`, `/usr/bin/pw-metadata`). |
| **Tampering** | Arbitrarily large config file or hostile tokens | Buffer overflow / Memory exhaustion | **Strict Config Guard**: Maximum file size cap (64 KB). Any unknown key triggers immediate parsing abortion. Numerical values strictly range-checked. |
| **Repudiation** | Daemon crashes leaving hardware in high-power or unstable state | Denial of Service / Overheating | **Crash-Only Write-Ahead State Journal**: Mutations written to `/run/zenith/journal.json` before applying. Unconditional rollback on boot. Systemd `ExecStopPost=/usr/bin/zenith --rollback`. |
| **Information Disclosure** | Malicious reading of IPC status dumps | Leaking process details | **Permissions 0660**: Socket and journal directory restricted strictly to `root:wheel` / `root:gamemode`. |
| **Denial of Service** | Rapid crash loop due to unstable kernel driver | System reboot loop | **Safe Mode Manager**: Detects $\ge 3$ crashes within a 10-minute window and disables all aggressive mutations. |
| **Elevation of Privilege** | Exploiting memory safety flaw in daemon binary | Root shell acquisition | **Compiler & Binary Hardening**: PIE, Full RELRO (`-z,relro,-z,now`), stack protector (`-fstack-protector-strong`), `_FORTIFY_SOURCE=3`, CET (`-fcf-protection=full`), and continuous `libFuzzer` regression tests. |

---

## 4. Residual Risk Assessment

- **Kernel Vulnerabilities in `sysfs`**: If the underlying kernel driver for AMD DRM or Intel CoreFreq contains a zero-day panic when writing `high` to `power_dpm_force_performance_level`, Zenith cannot mitigate kernel-level bugs. Safe Mode limits exposure by halting repeated attempts.
- **Physical Power Cuts**: Handled via atomic Journal persistence (`tmp` -> `fsync` -> `rename` -> `dir fsync`). On reboot, the journal is detected and evaluated.
