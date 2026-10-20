# ⚡ Zenith Gaming Daemon

<p align="center">
  <b>Ultra-low latency Linux gaming optimizer daemon for Arch / CachyOS, Fedora, Ubuntu & SteamOS</b><br/>
  <sub>Dynamic cgroups v2 core isolation · AMD 3D V-Cache CCX pinning · GPU clock lock · split-lock mitigation · sub-ms PipeWire quantum</sub>
</p>

---

## 🎯 What is Zenith?

**Zenith** is a modern C++20 background daemon engineered for Linux gamers. While tools like DXVK, VKD3D, and Proton have brought Linux gaming to performance parity with Windows in terms of raw average FPS, competitive play is frequently hindered by **frame-time inconsistency, micro-stutters, and 0.1% / 1% low frame rate drops**.

Zenith operates autonomously at the kernel and hardware boundary. It intercepts process events via the Linux Netlink proc connector, identifies active gaming workloads, and orchestrates CPU cache topologies, GPU clock states, memory management, and audio buffer scheduling to deliver a tear-free, stutter-free gaming experience.

---

## ⚡ Core Philosophy & Guarantees

* **Crash-Only Architecture & Zero-Drift**: All system modifications are recorded in a Write-Ahead Log (`/run/zenith/journal.json`). When your game exits (or even if your system loses power), Zenith unconditionally heals all kernel knobs back to pristine distro defaults.
* **Non-Intrusive & Anti-Cheat Safe**: Zenith interacts solely through documented Linux kernel interfaces (`sched_setaffinity`, `cgroups v2`, sysfs). It never touches game memory, never uses `ptrace`, and never injects code into game binaries. 100% compliant with EAC, BattlEye, VAC, and Ricochet.
* **Empirical Validation**: Zenith adheres to a strict guideline: *"Measure, don't believe"*. Every optimization is backed by A/B benchmarking with 95% Confidence Intervals. If a tweak does not demonstrably improve frame pacing, it is disabled by default.

---

## 🚀 Quick Navigation

* [Installation Guide](INSTALL.md) — Pre-compiled packages for Arch AUR, Debian/Ubuntu `.deb`, Fedora `.rpm`, Nix Flake, and Source.
* [Configuration Guide](CONFIGURATION.md) — Detailed explanation of every key in `/etc/zenith/zenith.conf`.
* [Optimization Profiles](PROFILES.md) — Safe, Balanced, and Aggressive tiers with adaptive PSI/thermal auto-downgrade.
* [Tweaks Reference](TWEAKS.md) — Kernel paths, mechanisms, risks, and benchmarked benefits.
* [Architecture & Design](ARCHITECTURE.md) — Complete module architecture with Mermaid lifecycle diagrams.
* [Anti-Cheat Compliance](ANTI_CHEAT.md) — Why Zenith is safe with modern anti-cheat engines.
* [Empirical Benchmarks](BENCHMARKS.md) — Benchmark data, frametime variance analysis, and hardware tier results.
* [Troubleshooting & Diagnostics](TROUBLESHOOTING.md) — `zenithctl doctor` guide and conflicting daemon resolution.
* [Frequently Asked Questions (FAQ)](FAQ.md) — Common questions, Steam Deck, CachyOS, and NVIDIA tuning.
* [Документация на русском (Russian)](ru/index.md) — Полное руководство на русском языке.
