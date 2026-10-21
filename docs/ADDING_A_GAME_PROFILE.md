# Contributor Guide: Adding a Game Profile

Zenith maintains a public database of game profiles located in `profiles/*.toml`. These profiles specify specialized optimizations for specific Steam AppIDs, Windows executables running under Proton, or native Linux games.

---

## 1. Directory & Naming Convention

Profile files are placed in the `profiles/` directory and should follow the format:

```text
profiles/<appid>_<short_name>.toml
```

Examples:
* `profiles/1091500_cyberpunk2077.toml`
* `profiles/730_cs2.toml`
* `profiles/570_dota2.toml`
* `profiles/1245620_elden_ring.toml`

For non-Steam games without an AppID, use the executable name:
* `profiles/osu_lazer.toml`
* `profiles/minecraft.toml`

---

## 2. TOML Schema Specification

Every profile file supports the following schema:

```toml
# General metadata
name = "Cyberpunk 2077"
appid = 1091500                          # Steam AppID (integer)
executables = ["Cyberpunk2077.exe"]      # Executable names or patterns

# Profile tier override
profile = "aggressive"                   # safe | balanced | aggressive

# CPU Topology & Cache Preferences
ccd_preference = "vcache"                # vcache | frequency | auto
pin_render_threads = true                # Pin main threads to CCD0 or P-cores

# Process Scheduling
realtime_priority = true                 # Elevate game threads (nice -15 / SCHED_RR)
io_class = "realtime"                    # realtime | best-effort | idle

# Hardware Specifics
gpu_performance = "high"                 # high | auto | standard
pipewire_quantum = 64                    # 32 | 64 | 128
network_cake = true                      # Enable CAKE qdisc for anti-bufferbloat
```

---

## 3. Key Descriptions

| Field | Type | Default | Options / Description |
| :--- | :--- | :--- | :--- |
| `name` | string | *required* | Human-readable game title. |
| `appid` | integer | `0` | Steam Application ID (from Steam store URL). |
| `executables` | list of strings | `[]` | Binary or process names matched via `/proc/PID/comm` or `cmdline`. |
| `profile` | string | `"balanced"` | Base profile to engage (`"safe"`, `"balanced"`, `"aggressive"`). |
| `ccd_preference` | string | `"auto"` | `"vcache"` (prioritize 3D V-Cache CCD0), `"frequency"` (prioritize high-clock CCD1), or `"auto"`. |
| `gpu_performance` | string | `"auto"` | `"high"` locks GPU clocks for zero latency dip; `"auto"` leaves driver to scale. |
| `pipewire_quantum` | integer | `64` | Audio buffer quantum (samples at 48kHz). `64` = 1.33ms. |
| `network_cake` | boolean | `false` | Enables CAKE bufferbloat mitigation on the primary network interface. |

---

## 4. Validating Your Profile Locally

Before submitting a Pull Request, validate your profile with the official CI validation script:

```bash
python3 scripts/validate_game_profiles.py
```

Expected output:
```text
[✓] Validated profiles/1091500_cyberpunk2077.toml (AppID: 1091500)
[✓] Validated profiles/730_cs2.toml (AppID: 730)
[✓] All 7 game profiles passed schema validation!
```

---

## 5. Submitting via Pull Request

1. Fork the repository on GitHub.
2. Create a new branch: `git checkout -b profile/my-game`.
3. Add your `.toml` file to `profiles/`.
4. Run `python3 scripts/validate_game_profiles.py` to ensure all fields are valid.
5. Commit and open a PR with the title: `feat(profile): add profile for <Game Name>`.
