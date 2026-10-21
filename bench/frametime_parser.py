#!/usr/bin/env python3
"""
bench/frametime_parser.py
Parser and statistical analyzer for MangoHud and CapFrameX frame time CSV logs.
Calculates Mean FPS, 1% Low FPS, 0.1% Low FPS, Frame Time Variance, and Stutter Counts.
"""

import sys
import os
import csv
import math
from typing import Dict, List, Optional, Tuple

class FrameTimeStats:
    def __init__(self, frametimes_ms: List[float]):
        if not frametimes_ms:
            raise ValueError("frametimes_ms cannot be empty")
        self.frametimes = frametimes_ms
        self.count = len(frametimes_ms)
        self.total_time_ms = sum(frametimes_ms)
        self.avg_fps = (self.count / (self.total_time_ms / 1000.0)) if self.total_time_ms > 0 else 0.0

        sorted_ft = sorted(frametimes_ms)
        self.median_ft_ms = sorted_ft[self.count // 2]
        self.mean_ft_ms = self.total_time_ms / self.count

        # Variance & Standard Deviation
        variance = sum((ft - self.mean_ft_ms) ** 2 for ft in frametimes_ms) / self.count
        self.stddev_ft_ms = math.sqrt(variance)

        # 1% Low and 0.1% Low calculations
        # 1% low is the worst 1% of frame times (highest frame times)
        idx_1pct = max(0, int(math.ceil(self.count * 0.99)) - 1)
        idx_01pct = max(0, int(math.ceil(self.count * 0.999)) - 1)
        worst_1pct_ft = sorted_ft[idx_1pct]
        worst_01pct_ft = sorted_ft[idx_01pct]

        self.low_1pct_fps = 1000.0 / worst_1pct_ft if worst_1pct_ft > 0 else 0.0
        self.low_01pct_fps = 1000.0 / worst_01pct_ft if worst_01pct_ft > 0 else 0.0

        # Stutter count: frames whose duration exceeds 2.5x mean or >50ms
        stutter_threshold = max(2.5 * self.mean_ft_ms, 50.0)
        self.stutter_count = sum(1 for ft in frametimes_ms if ft >= stutter_threshold)

    def to_dict(self) -> Dict[str, float]:
        return {
            "total_frames": self.count,
            "duration_s": self.total_time_ms / 1000.0,
            "avg_fps": round(self.avg_fps, 2),
            "1pct_low_fps": round(self.low_1pct_fps, 2),
            "01pct_low_fps": round(self.low_01pct_fps, 2),
            "mean_frametime_ms": round(self.mean_ft_ms, 3),
            "median_frametime_ms": round(self.median_ft_ms, 3),
            "stddev_frametime_ms": round(self.stddev_ft_ms, 3),
            "stutter_count": self.stutter_count,
        }

def parse_mangohud_csv(filepath: str) -> List[float]:
    """Parses MangoHud CSV log (looking for 'Frametime' or 'Frame Time (ms)' column)."""
    frametimes: List[float] = []
    with open(filepath, "r", encoding="utf-8") as f:
        reader = csv.reader(f)
        header = None
        for row in reader:
            if not row or row[0].startswith("#"):
                continue
            if header is None:
                header = [col.strip().lower() for col in row]
                continue
            # Try to locate frametime column index
            ft_idx = -1
            for candidate in ["frametime", "frame time (ms)", "frametime (ms)", "frame_time"]:
                if candidate in header:
                    ft_idx = header.index(candidate)
                    break
            if ft_idx >= 0 and ft_idx < len(row):
                try:
                    val = float(row[ft_idx])
                    # If stored in microseconds, convert to ms
                    if val > 500.0:
                        val /= 1000.0
                    frametimes.append(val)
                except ValueError:
                    continue
            elif len(row) == 1:
                try:
                    frametimes.append(float(row[0]))
                except ValueError:
                    continue
    return frametimes

def parse_capframex_csv(filepath: str) -> List[float]:
    """Parses CapFrameX CSV file."""
    frametimes: List[float] = []
    with open(filepath, "r", encoding="utf-8") as f:
        reader = csv.reader(f)
        found_data = False
        for row in reader:
            if not row:
                continue
            if "FrameTime" in row or "msBetweenPresents" in row:
                found_data = True
                continue
            if found_data and len(row) >= 1:
                try:
                    frametimes.append(float(row[0]))
                except ValueError:
                    continue
    return frametimes

def generate_synthetic_log(filepath: str, frames: int = 1000, base_ms: float = 16.66, jitter_ms: float = 1.0, stutters: int = 5):
    """Generates synthetic MangoHud CSV log for reproducible testing and CI pipelines."""
    import random
    os.makedirs(os.path.dirname(os.path.abspath(filepath)), exist_ok=True)
    with open(filepath, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["# MangoHud Synthetic Log"])
        writer.writerow(["FrameTime", "FPS", "CPU", "GPU"])
        
        stutter_indices = set(random.sample(range(50, frames - 50), stutters)) if stutters > 0 else set()
        for i in range(frames):
            if i in stutter_indices:
                ft = base_ms + random.uniform(40.0, 75.0) # microstutter
            else:
                ft = max(1.0, random.gauss(base_ms, jitter_ms))
            fps = 1000.0 / ft
            writer.writerow([f"{ft:.3f}", f"{fps:.1f}", "35", "90"])

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: frametime_parser.py <log.csv> [--capframex]")
        sys.exit(1)

    path = sys.argv[1]
    is_capframex = "--capframex" in sys.argv
    if is_capframex:
        fts = parse_capframex_csv(path)
    else:
        fts = parse_mangohud_csv(path)

    if not fts:
        print(f"Error: No frametimes parsed from {path}")
        sys.exit(1)

    stats = FrameTimeStats(fts)
    import json
    print(json.dumps(stats.to_dict(), indent=2))
