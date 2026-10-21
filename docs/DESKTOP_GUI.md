# Zenith Gaming Daemon: Desktop Tray & GUI Integration Specification

> **Document Version**: 1.0.0  
> **Package**: `zenith-tray` (Optional Companion GUI)  
> **Architecture**: Lightweight Standalone StatusNotifierItem / AppIndicator Client

---

## 1. Overview & Philosophy

The core Zenith gaming daemon (`zenith`) is strictly headless and designed to run with zero GUI dependencies, consuming $< 0.1\%$ CPU and $< 6\text{ MB}$ RSS. 

For desktop users on Arch Linux, CachyOS, and SteamOS who desire visual feedback, the companion package **`zenith-tray`** provides a desktop system tray applet adhering to the FreeDesktop `org.kde.StatusNotifierItem` / AppIndicator protocol.

---

## 2. Communication Architecture

`zenith-tray` communicates exclusively via the UNIX domain socket (`/run/zenith/zenith.sock`):

```
┌─────────────────────────────────┐
│     Zenith Daemon (Root)        │
│    /run/zenith/zenith.sock      │
└────────────────┬────────────────┘
                 │ UNIX Socket JSON API (0660 / wheel)
                 ▼
┌─────────────────────────────────┐
│   zenith-tray (User Session)    │
│  (Qt6 / GTK4 StatusNotifier)    │
└────────────────┬────────────────┘
                 │ D-Bus org.freedesktop.Notifications
                 ▼
┌─────────────────────────────────┐
│     Desktop System Tray         │
│   (KDE Plasma / GNOME / Waybar) │
└─────────────────────────────────┘
```

---

## 3. Tray Applet Features

1. **Dynamic Tray Icon State**:
  - ⚡ **Green Flame / Lightning**: Game active, optimizations engaged.
  - 💤 **Dim Shield / Z**: Daemon idle, monitoring background processes.
  - ⚠️ **Orange Alert**: Safe Mode engaged (crash loop prevention).
  - ❌ **Red Cross**: Daemon offline.
2. **Context Menu Actions**:
  - **Active Game**: Title, PID, elapsed time, current profile.
  - **Quick Profile Switcher**: Radio selector for `Safe`, `Balanced`, and `Aggressive`.
  - **Live PSI Pressure**: CPU, Memory, and IO pressure percentages.
  - **Launch Dashboard**: Spawns terminal running `zenithctl status --watch`.
  - **Emergency Revert**: Immediate trigger for `zenithctl rollback`.
  - **Doctor Diagnostics**: Runs `zenithctl doctor` in a floating terminal window.

---

## 4. D-Bus Desktop Notifications

Zenith triggers desktop notifications via `org.freedesktop.Notifications` (`notify-send`):
- **Game Started**: `"⚡ Zenith Gaming Daemon: Optimizations engaged for <Game Name>"`
- **Profile Shift**: `"⚠️ Zenith: Memory pressure exceeded threshold, degraded to Balanced profile"`
- **Game Finished**: `"⚡ Zenith: <Game Name> exited. System restored to baseline"`
