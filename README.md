# Zenith Gaming Daemon

[![CI](https://github.com/negg0s/zenith-gaming-daemon/actions/workflows/ci.yml/badge.svg)](https://github.com/negg0s/zenith-gaming-daemon/actions/workflows/ci.yml)
[![CodeQL Analysis](https://github.com/negg0s/zenith-gaming-daemon/actions/workflows/codeql.yml/badge.svg)](https://github.com/negg0s/zenith-gaming-daemon/actions/workflows/codeql.yml)
[![Release](https://img.shields.io/github/v/release/negg0s/zenith-gaming-daemon?include_prereleases&color=blue)](https://github.com/negg0s/zenith-gaming-daemon/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

Linux game session optimizer for Arch, CachyOS, Fedora, Ubuntu and SteamOS. It detects a running game, applies a profile, and restores the previous system state when the game exits.

The current source version is 1.6.0. Packages are published on the [Releases](https://github.com/negg0s/zenith-gaming-daemon/releases) page.

## What it does

- Detects game launches through the kernel Netlink process connector.
- Moves selected background processes into a cgroups v2 housekeeping slice.
- Pins game threads to physical cores, AMD 3D V-Cache CCDs or Intel P-cores when the topology is known.
- Applies kernel settings such as split-lock mitigation, THP and compaction proactiveness for the session.
- Can lock GPU clocks through the AMDGPU sysfs interface or NVIDIA NVML.
- Lowers the PipeWire quantum and adjusts the network qdisc for lower audio and packet latency.
- Writes a rollback journal before changing system state and restores it when the game exits or the daemon restarts.

Every change is session-scoped. The daemon does not write persistent sysctl or security configuration files.

## What it does not do

- It does not guarantee higher average FPS. A game that is limited by the GPU will usually see no gain.
- It does not disable CPU security mitigations.
- It does not inject libraries, read game memory, trace game threads or modify game files.
- It does not guarantee anti-cheat compatibility. The daemon uses normal kernel and OS interfaces, but anti-cheat vendors make their own decisions. Check the rules for each game.

## Benchmarks

Benchmark notes and methodology are documented in [docs/BENCHMARKS.md](docs/BENCHMARKS.md). Results vary by game, hardware, kernel and background load. Run your own before and after captures with MangoHud, PresentMon or an equivalent frame-time logger.

## Requirements

| Component | Minimum | Notes |
| --- | --- | --- |
| Linux kernel | 6.6 | 6.12 or newer recommended |
| cgroups | v2 unified | Required for background isolation |
| CPU | AMD Ryzen 3000+ or Intel 8th gen+ | 3D V-Cache and hybrid core pinning need newer hardware |
| GPU | AMDGPU or NVIDIA proprietary driver | GPU clock lock is optional |
| Audio | PipeWire 0.3.50+ | Quantum tuning uses `pw-metadata` |
| Init | systemd | The package installs `zenith.service` |

Steam Deck, ROG Ally and other monolithic APUs skip dual-CCD pinning.

## Install

### Arch Linux and CachyOS

```bash
paru -S zenith-gaming-daemon
# or
yay -S zenith-gaming-daemon
```

### Debian and Ubuntu

Download the `.deb` from the latest release, then:

```bash
sudo dpkg -i zenith-gaming-daemon-*.deb
sudo apt-get install -f -y
```

### Fedora and Bazzite

Download the `.rpm` from the latest release, then:

```bash
sudo dnf install ./zenith-gaming-daemon-*.rpm
```

On an rpm-ostree system:

```bash
sudo rpm-ostree install ./zenith-gaming-daemon-*.rpm
```

### NixOS

Use the flake in the repository. The module name and options are documented in `flake.nix`.

### Build from source

```bash
git clone https://github.com/negg0s/zenith-gaming-daemon.git
cd zenith-gaming-daemon
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j"$(nproc)"
sudo cmake --install build
```

## Quick start

```bash
sudo systemctl enable --now zenith.service
zenithctl doctor
zenithctl status --watch
```

`doctor` checks cgroups v2, kernel features, drivers and conflicting services. `status --watch` shows the active session, CPU topology, pressure stall information, GPU state and recent events.

Start a game in Steam or another launcher. The daemon detects the process and applies the configured profile without a `%command%` wrapper.

## Profiles

The built-in profiles are `safe`, `balanced`, `aggressive` and `ultra`.

```bash
zenithctl profile set balanced
zenithctl profile show
```

| Profile | Intended use |
| --- | --- |
| `safe` | Minimal system changes and no GPU clock lock. |
| `balanced` | Default. CPU affinity, background isolation and audio tuning. |
| `aggressive` | Adds kernel and GPU latency tuning. Higher power draw. |
| `ultra` | Zero-contention mode. Freezes non-essential background processes (cgroup.freeze) and applies maximum GPU power headroom. |

Per-game overrides live in `profiles/*.toml`. See [docs/PROFILES.md](docs/PROFILES.md).

## Steam Launch Options (`zenith-run`)

While the daemon auto-detects games via Netlink, you can also wrap commands explicitly or set the profile per-title in Steam:

```bash
# In Steam Game Properties -> Launch Options:
zenith-run %command%

# Force Ultra profile with background freeze for competitive play:
zenith-run --ultra %command%
```

## CLI

```bash
# Show the active session and topology
zenithctl status --watch

# List detected games and their processes
zenithctl game list

# Switch profile
zenithctl profile set aggressive

# Restore all session changes now
zenithctl rollback

# Explain a tweak and its default
zenithctl explain split_lock_mitigate

# Create a redacted diagnostic report
zenithctl report --redact > report.txt
```

## Configuration

The main configuration file is `/etc/zenith/zenith.conf`. The full key reference is in [docs/CONFIGURATION.md](docs/CONFIGURATION.md). The tweak catalog, including kernel paths and risks, is in [docs/TWEAKS.md](docs/TWEAKS.md).

## Rollback and recovery

Before a session starts, the daemon writes the previous values to `/run/zenith/journal.json`. When the game exits, it restores the journal. If the daemon crashes, the next start reads the journal and restores the recorded values.

`/run` is cleared at reboot, so a journal does not survive a power loss. In that case the affected kernel settings return to their boot defaults.

## Risks

- The `aggressive` profile locks GPU clocks and can increase power draw and heat.
- On a laptop, use `safe` or `balanced` on battery.
- The daemon runs as root. Review the configuration and the installed service before enabling it.
- Background isolation can make Discord, browsers or downloads slower while a game is running.

## Documentation

- [Installation](docs/INSTALL.md)
- [Configuration](docs/CONFIGURATION.md)
- [Profiles](docs/PROFILES.md)
- [Tweaks](docs/TWEAKS.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Anti-cheat notes](docs/ANTI_CHEAT.md)
- [Benchmarks](docs/BENCHMARKS.md)
- [Threat model](docs/THREAT_MODEL.md)
- [Troubleshooting](docs/TROUBLESHOOTING.md)
- [FAQ](docs/FAQ.md)

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) and [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md). Game profiles can be submitted in `profiles/*.toml`. Benchmark reports are welcome in GitHub Discussions.

## License

MIT. See [LICENSE](LICENSE) and [NOTICE](NOTICE).
