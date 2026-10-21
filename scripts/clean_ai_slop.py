#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
Script to scan and replace AI buzzwords, promotional fluff, and hyperbolic adjectives
with precise, dry, professional engineering prose across all documentation.
"""

from pathlib import Path

REPLACEMENTS = {
    "README.md": [
        ("achieves outstanding average frame rates", "achieves high average frame rates"),
        ("creating catastrophic stutter.", "causing severe stutter and latency spikes."),
        ("causing massive L3 cache thrashing (~75ns inter-CCD latency penalty).", "causing L3 cache thrashing and memory latency penalties (~75ns inter-CCD penalty)."),
        ("with zero user intervention.", "automatically without manual intervention."),
        ("* **Dramatically Improves 1% and 0.1% Lows**:", "* **Improves 1% and 0.1% Lows**:"),
        ('* **No "Double Your FPS" Magic**: If your game is purely GPU-bound at 99% GPU utilization, Zenith will not double your average FPS.', '* **GPU-Bound Limits**: If your game is purely GPU-bound at 99% GPU utilization, Zenith will not increase your average FPS.'),
        ("* **No Snake Oil**: Zenith only implements", "* **Empirical Kernel Interfaces Only**: Zenith only implements"),
    ],
    "docs/FAQ.md": [
        ("Zenith will not magically double your frame rate.", "Zenith will not double your frame rate when GPU-bound."),
        ("CachyOS is a premier Linux gaming distribution featuring specialized kernels", "CachyOS is an optimized Linux gaming distribution featuring specialized kernels"),
    ],
    "docs/PROFILES.md": [
        ("* **Philosophy**: Zero risk, zero elevated power draw, zero battery compromise.", "* **Philosophy**: Conservative baseline. Standard power draw and thermal limits."),
        ("* **Philosophy**: The sweet spot for 95% of desktop gaming systems. Maximizes frame-time consistency while maintaining rock-solid system stability.", "* **Philosophy**: Standard desktop profile. Balances frame pacing with system stability and energy efficiency."),
        ("* **Philosophy**: Uncompromising competitive performance. Prioritizes 0.1% frame pacing above power consumption and thermal conservation.", "* **Philosophy**: Maximum performance profile. Prioritizes 0.1% frame pacing above power consumption and thermal conservation."),
    ],
    "docs/DECISIONS.md": [
        ("and launch games seamlessly within 2 minutes.", "and launch games directly without complex setup."),
        ("- **Showcase README**: Clear visual elevator pitch, real-time TUI mock, badges, hardware compatibility matrix,", "- **Showcase README**: Concise technical overview, real-time TUI mock, badges, hardware compatibility matrix,"),
    ],
    "docs/PLAN.md": [
        ("verifying seamless v1.0.0", "verifying automated v1.0.0"),
        ("Flawless single-command installation path verified.", "Single-command installation path verified."),
    ],
    "docs/PROGRESS.md": [
        ("Seamlessly upgrades legacy v1.0.0 configurations", "Automatically upgrades legacy v1.0.0 configurations"),
    ],
    "docs/ru/index.md": [
        ("обеспечивает превосходный средний FPS", "обеспечивает высокий средний FPS"),
    ],
    "docs/ADDING_A_GAME_PROFILE.md": [
        ("These profiles specify tailored optimizations", "These profiles specify specialized optimizations"),
    ],
    "tests/test_config_migration.cpp": [
        ("seamless migration", "automatic migration"),
    ],
}

def main():
    repo_root = Path(__file__).resolve().parent.parent
    for rel_path, pairs in REPLACEMENTS.items():
        file_path = repo_root / rel_path
        if not file_path.exists():
            print(f"Skipping missing file: {rel_path}")
            continue
        text = file_path.read_text(encoding="utf-8")
        modified = False
        for old, new in pairs:
            if old in text:
                text = text.replace(old, new)
                modified = True
                print(f"[{rel_path}] Replaced: '{old[:40]}...' -> '{new[:40]}...'")
        if modified:
            file_path.write_text(text, encoding="utf-8", newline="\n")
            print(f"Updated {rel_path}")

if __name__ == "__main__":
    main()
