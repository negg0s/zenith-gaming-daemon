#!/usr/bin/env python3
"""
scripts/validate_game_profiles.py
Validates all game profiles in the profiles/ directory against the Zenith Game Profile schema.
Can run under Python 3.9+ standard library (using tomllib on 3.11+ or simple fallback parser).
"""

import sys
import os
import glob

# Try modern tomllib (Python 3.11+), fallback to basic parser if absent
try:
    import tomllib
except ImportError:
    try:
        import tomli as tomllib
    except ImportError:
        tomllib = None

VALID_PROFILES = {"safe", "balanced", "aggressive"}
VALID_CCD_PREFS = {"vcache", "frequency", "auto"}
VALID_GPU_PERF = {"high", "auto", "standard"}
VALID_IO_CLASSES = {"realtime", "best-effort", "idle"}

def parse_simple_toml(content: str) -> dict:
    """Fallback parser for basic key-value TOML when tomllib is absent."""
    result = {}
    for line in content.splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        if '#' in line:
            line = line.split('#')[0].strip()
        if '=' not in line:
            continue
        key, val = line.split('=', 1)
        key = key.strip()
        val = val.strip()

        # Parse string
        if val.startswith('"') and val.endswith('"'):
            result[key] = val[1:-1]
        # Parse list
        elif val.startswith('[') and val.endswith(']'):
            items = val[1:-1].split(',')
            result[key] = [item.strip().strip('"') for item in items if item.strip()]
        # Parse boolean
        elif val.lower() == 'true':
            result[key] = True
        elif val.lower() == 'false':
            result[key] = False
        # Parse integer
        else:
            try:
                result[key] = int(val)
            except ValueError:
                result[key] = val
    return result

def validate_profile(filepath: str) -> bool:
    print(f"[*] Validating {filepath}...")
    with open(filepath, "r", encoding="utf-8") as f:
        content = f.read()

    try:
        if tomllib:
            data = tomllib.loads(content)
        else:
            data = parse_simple_toml(content)
    except Exception as e:
        print(f"[!] Syntax Error in {filepath}: {e}", file=sys.stderr)
        return False

    # Check required fields
    if "name" not in data or not isinstance(data["name"], str) or not data["name"].strip():
        print(f"[!] Error in {filepath}: Missing or invalid 'name' field", file=sys.stderr)
        return False

    # Check appid
    if "appid" in data:
        if not isinstance(data["appid"], int) or data["appid"] < 0:
            print(f"[!] Error in {filepath}: 'appid' must be a non-negative integer", file=sys.stderr)
            return False

    # Check profile
    profile = data.get("profile", "balanced")
    if profile not in VALID_PROFILES:
        print(f"[!] Error in {filepath}: Invalid profile '{profile}'. Allowed: {VALID_PROFILES}", file=sys.stderr)
        return False

    # Check ccd_preference
    ccd = data.get("ccd_preference", "auto")
    if ccd not in VALID_CCD_PREFS:
        print(f"[!] Error in {filepath}: Invalid ccd_preference '{ccd}'. Allowed: {VALID_CCD_PREFS}", file=sys.stderr)
        return False

    # Check gpu_performance
    if "gpu_performance" in data and data["gpu_performance"] not in VALID_GPU_PERF:
        print(f"[!] Error in {filepath}: Invalid gpu_performance '{data['gpu_performance']}'", file=sys.stderr)
        return False

    # Check executables
    if "executables" in data:
        if not isinstance(data["executables"], list):
            print(f"[!] Error in {filepath}: 'executables' must be a list of strings", file=sys.stderr)
            return False

    # Check pipewire_quantum
    if "pipewire_quantum" in data:
        q = data["pipewire_quantum"]
        if not isinstance(q, int) or q < 16 or q > 8192:
            print(f"[!] Error in {filepath}: 'pipewire_quantum' must be integer between 16 and 8192", file=sys.stderr)
            return False

    print(f"  [OK] PASS: {data['name']} (Profile: {profile}, CCD: {ccd})")
    return True

def main():
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    profiles_dir = os.path.join(repo_root, "profiles")
    if not os.path.exists(profiles_dir):
        print(f"[!] Error: profiles directory not found at {profiles_dir}", file=sys.stderr)
        sys.exit(1)

    profile_files = sorted(glob.glob(os.path.join(profiles_dir, "*.toml")))
    if not profile_files:
        print("[!] Error: No .toml profile files found!", file=sys.stderr)
        sys.exit(1)

    all_passed = True
    for p in profile_files:
        if not validate_profile(p):
            all_passed = False

    if not all_passed:
        print("\n[FAIL] Validation failed on one or more profiles!", file=sys.stderr)
        sys.exit(1)

    print(f"\n[OK] All {len(profile_files)} game profiles successfully validated!")
    sys.exit(0)

if __name__ == "__main__":
    main()
