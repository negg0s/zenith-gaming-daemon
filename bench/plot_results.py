#!/usr/bin/env python3
"""
bench/plot_results.py
Generates publication-quality Matplotlib figures from Zenith benchmark sweep JSON/CSV data:
1. Profiles Comparison (Median Avg FPS, 1% Lows, 0.1% Lows with 95% CI error bars)
2. Ablation Analysis (Isolated impact of each individual tweak vs Baseline)
3. Frame Time Jitter & Microstutter distribution
"""

import argparse
import glob
import json
import os
import sys

def plot_benchmark_results(json_path: str, output_dir: str):
    try:
        import matplotlib
        matplotlib.use("Agg") # Headless backend
        import matplotlib.pyplot as plt
    except ImportError:
        print("[WARN] matplotlib not installed. Skipping chart generation. (Install via 'pip install matplotlib')")
        return

    os.makedirs(output_dir, exist_ok=True)
    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    scenarios = data.get("scenarios", {})
    if not scenarios:
        print("[WARN] No scenarios found in JSON.")
        return

    names = list(scenarios.keys())

    # 1. Figure: FPS Metrics Comparison (Avg, 1% Low, 0.1% Low with error bars)
    fig, ax = plt.subplots(figsize=(12, 6))
    x = range(len(names))
    width = 0.25

    avg_medians = [scenarios[n]["avg_fps"]["median"] for n in names]
    avg_ci_low = [scenarios[n]["avg_fps"]["median"] - scenarios[n]["avg_fps"]["ci95_lower"] for n in names]
    avg_ci_high = [scenarios[n]["avg_fps"]["ci95_upper"] - scenarios[n]["avg_fps"]["median"] for n in names]

    low1_medians = [scenarios[n]["low_1pct_fps"]["median"] for n in names]
    low1_ci_low = [scenarios[n]["low_1pct_fps"]["median"] - scenarios[n]["low_1pct_fps"]["ci95_lower"] for n in names]
    low1_ci_high = [scenarios[n]["low_1pct_fps"]["ci95_upper"] - scenarios[n]["low_1pct_fps"]["median"] for n in names]

    low01_medians = [scenarios[n]["low_01pct_fps"]["median"] for n in names]
    low01_ci_low = [scenarios[n]["low_01pct_fps"]["median"] - scenarios[n]["low_01pct_fps"]["ci95_lower"] for n in names]
    low01_ci_high = [scenarios[n]["low_01pct_fps"]["ci95_upper"] - scenarios[n]["low_01pct_fps"]["median"] for n in names]

    ax.bar([i - width for i in x], avg_medians, width, yerr=[avg_ci_low, avg_ci_high], capsize=4, label="Average FPS", color="#2b5c8f")
    ax.bar(x, low1_medians, width, yerr=[low1_ci_low, low1_ci_high], capsize=4, label="1% Low FPS", color="#3b8b68")
    ax.bar([i + width for i in x], low01_medians, width, yerr=[low01_ci_low, low01_ci_high], capsize=4, label="0.1% Low FPS", color="#d95f02")

    ax.set_ylabel("Frames Per Second (FPS)")
    ax.set_title("Zenith A/B & Ablation Benchmark: Frame Rate Stability (95% Confidence Intervals)")
    ax.set_xticks(list(x))
    ax.set_xticklabels([n.replace("ablation_", "").replace("profile_", "") for n in names], rotation=40, ha="right")
    ax.legend()
    ax.grid(True, linestyle="--", alpha=0.5)

    plt.tight_layout()
    chart1_path = os.path.join(output_dir, "benchmark_fps_comparison.png")
    plt.savefig(chart1_path, dpi=180)
    plt.close()
    print(f"[Chart] Saved: {chart1_path}")

    # 2. Figure: Jitter and Microstutters
    fig, ax1 = plt.subplots(figsize=(10, 5))
    jitter_medians = [scenarios[n]["jitter_frametime_ms"]["median"] for n in names]
    stutters_medians = [scenarios[n]["stutters"]["median"] for n in names]

    color = "#7570b3"
    ax1.set_xlabel("Scenario")
    ax1.set_ylabel("Frame Time Jitter (StdDev ms)", color=color)
    bars1 = ax1.bar([i - 0.15 for i in x], jitter_medians, 0.3, color=color, label="Frametime Jitter (ms)")
    ax1.tick_params(axis="y", labelcolor=color)
    ax1.set_xticks(list(x))
    ax1.set_xticklabels([n.replace("ablation_", "").replace("profile_", "") for n in names], rotation=40, ha="right")

    ax2 = ax1.twinx()
    color = "#e7298a"
    ax2.set_ylabel("Microstutter Count (events)", color=color)
    bars2 = ax2.bar([i + 0.15 for i in x], stutters_medians, 0.3, color=color, label="Stutters Count")
    ax2.tick_params(axis="y", labelcolor=color)

    plt.title("Frametime Jitter and Microstutter Frequency per Scenario")
    plt.tight_layout()
    chart2_path = os.path.join(output_dir, "benchmark_jitter_stutters.png")
    plt.savefig(chart2_path, dpi=180)
    plt.close()
    print(f"[Chart] Saved: {chart2_path}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Plot Zenith benchmark results")
    parser.add_argument("json_file", nargs="?", help="Path to sweep JSON file (default: latest in bench/results/)")
    parser.add_argument("--output-dir", default="bench/results", help="Directory to save charts")
    args = parser.parse_args()

    target_json = args.json_file
    if not target_json:
        files = glob.glob("bench/results/sweep_*.json")
        if files:
            target_json = max(files, key=os.path.getmtime)
        else:
            print("Error: No sweep JSON files found. Run runner.py first.")
            sys.exit(1)

    print(f"Plotting results from: {target_json}")
    plot_benchmark_results(target_json, args.output_dir)
