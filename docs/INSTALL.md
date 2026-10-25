# Installation Guide

Zenith Gaming Daemon provides native pre-compiled packages for all major Linux distributions, reproducible source archives, and an AUR package.

---

## 1. Quick Install Matrix

| Distribution | Package Manager / Method | Command | Status |
| :--- | :--- | :--- | :--- |
| **Arch Linux / CachyOS** | AUR (paru / yay) | `paru -S zenith-gaming-daemon` | Tier 1 (Recommended) |
| **Debian / Ubuntu / Pop!_OS** | Native `.deb` | `sudo dpkg -i zenith-gaming-daemon-*.deb` | Tier 1 (Validated LTS) |
| **Fedora / RHEL / Bazzite** | Native `.rpm` | `sudo rpm -ivh zenith-gaming-daemon-*.rpm` | Tier 1 (Validated F40+) |
| **NixOS** | Nix Flake | `nix profile install github:negg0s/zenith-gaming-daemon` | Tier 1 (Flake Module) |
| **Any Linux (Kernel ≥ 6.6)** | Build from Source | `cmake -B build && cmake --build build` | Supported |

---

## 2. Arch Linux & CachyOS (AUR)

### Using an AUR Helper (Recommended)
```bash
# Using paru
paru -S zenith-gaming-daemon

# Or using yay
yay -S zenith-gaming-daemon
```

### Manual AUR Build
```bash
git clone https://aur.archlinux.org/zenith-gaming-daemon.git
cd zenith-gaming-daemon
makepkg -si
```

### Development Snapshot (`-git`)
If you want the bleeding edge master branch:
```bash
paru -S zenith-gaming-daemon-git
```

---

## 3. Debian, Ubuntu & Pop!_OS (`.deb`)

Download the latest `.deb` release package from [GitHub Releases](https://github.com/negg0s/zenith-gaming-daemon/releases):

```bash
# Download latest release
wget https://github.com/negg0s/zenith-gaming-daemon/releases/download/v1.1.0/zenith-gaming-daemon-1.1.0-Linux.deb

# Verify checksum
wget https://github.com/negg0s/zenith-gaming-daemon/releases/download/v1.1.0/SHA256SUMS
sha256sum --ignore-missing -c SHA256SUMS

# Install package and resolve dependencies
sudo dpkg -i zenith-gaming-daemon-1.1.0-Linux.deb || sudo apt-get install -f -y
```

---

## 4. Fedora, RHEL & Bazzite (`.rpm`)

Download the latest `.rpm` release package:

```bash
# Download latest release
wget https://github.com/negg0s/zenith-gaming-daemon/releases/download/v1.1.0/zenith-gaming-daemon-1.1.0-Linux.rpm

# Verify checksum
wget https://github.com/negg0s/zenith-gaming-daemon/releases/download/v1.1.0/SHA256SUMS
sha256sum --ignore-missing -c SHA256SUMS

# Install using dnf or rpm
sudo dnf install ./zenith-gaming-daemon-1.1.0-Linux.rpm
# or on rpm-ostree systems (Bazzite / Silverblue):
rpm-ostree install ./zenith-gaming-daemon-1.1.0-Linux.rpm
```

---

## 5. NixOS / Nix

Zenith provides a first-class Nix Flake with reproducible builds and a complete NixOS module.

### Declarative NixOS Configuration (`configuration.nix`)
Add Zenith to your flake inputs:

```nix
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    zenith.url = "github:negg0s/zenith-gaming-daemon";
  };

  outputs = { self, nixpkgs, zenith, ... }: {
    nixosConfigurations.myhostname = nixpkgs.lib.nixosSystem {
      system = "x86_64-linux";
      modules = [
        zenith.nixosModules.zenith
        {
          services.zenith-gaming-daemon = {
            enable = true;
            profile = "balanced"; # safe | balanced | aggressive
            vcachePinning = true;
            cgroupIsolation = true;
          };
        }
      ];
    };
  };
}
```

### Ad-hoc CLI Run
```bash
nix run github:negg0s/zenith-gaming-daemon -- status
```

---

## 6. Building from Source

### Prerequisites
* **C++ Compiler**: GCC ≥ 12 or Clang ≥ 16 (strict C++20 support required).
* **Build System**: CMake ≥ 3.20, Ninja or Make.
* **Libraries**: `libpipewire-0.3-dev` (optional, for real-time PipeWire quantum synchronization).
* **Kernel**: Linux kernel ≥ 6.6 with cgroups v2 enabled (`/sys/fs/cgroup`).

### Build & Installation Steps
```bash
# Clone the repository
git clone https://github.com/negg0s/zenith-gaming-daemon.git
cd zenith-gaming-daemon

# Configure with CMake (Release mode)
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr

# Compile on all CPU threads
cmake --build build --parallel $(nproc)

# Execute test suite to verify kernel interfaces and fakes
ctest --test-dir build --output-on-failure

# Install system binaries, configs, and man pages
sudo cmake --install build
```

---

## 7. Post-Installation Setup & Service Activation

### 1. Enable Systemd Service
Zenith runs as a system daemon managed by systemd:

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now zenith.service
```

### 2. Verify Health
Run `zenithctl doctor` to ensure cgroups v2, kernel drivers, and capability permissions are in place:

```bash
zenithctl doctor
```

Expected output:
```text
[✓] Kernel Version: 6.13.2-cachyos (Pass: >= 6.6 LTS)
[✓] cgroups v2 Hierarchy: /sys/fs/cgroup (Pass: active and writeable)
[✓] AMD 3D V-Cache Driver: amd_x3d_vcache loaded (Pass)
[✓] Pressure Stall Information (PSI): /proc/pressure active (Pass)
[✓] Daemon Status: active (running)
[✓] Overall Health: All 8 checks passed. System ready for ultra-low latency gaming.
```

### 3. User Group Permissions
To allow non-root users to execute `zenithctl` without `sudo`, ensure your user belongs to the `wheel` or `zenith` group:

```bash
sudo groupadd -f zenith
sudo usermod -aG zenith $USER
```
*(Log out and log back in for group changes to take effect).*
