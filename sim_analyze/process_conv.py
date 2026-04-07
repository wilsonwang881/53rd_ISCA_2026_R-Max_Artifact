#!/usr/bin/env python3

import argparse
import csv
import math
import os
import re
import sys
from typing import Dict, List, Optional, Tuple

try:
    import plotly.graph_objects as go
except ImportError:
    print("Error: plotly is required. Install it with: pip install plotly", file=sys.stderr)
    sys.exit(1)


MAX_N = 12

# Example target line:
# Simulation complete ... cumulative IPC: 1.2345
IPC_PATTERN = re.compile(
    r"Simulation complete.*?cumulative IPC:\s*([0-9]*\.?[0-9]+(?:[eE][-+]?\d+)?)"
)


def extract_ipc(file_path: str) -> Optional[float]:
    """
    Return the IPC from the line containing both:
      - 'Simulation complete'
      - 'cumulative IPC:'
    If not found or invalid, return None.
    """
    try:
        with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
            for line in f:
                match = IPC_PATTERN.search(line)
                if match:
                    try:
                        return float(match.group(1))
                    except ValueError:
                        return None
    except OSError:
        return None

    return None


def geometric_mean(values: List[float]) -> Optional[float]:
    """
    Geometric mean of positive values only.
    Returns None if values is empty or contains non-positive values.
    """
    if not values:
        return None
    if any(v <= 0 for v in values):
        return None

    log_sum = sum(math.log(v) for v in values)
    return math.exp(log_sum / len(values))


def mean(values: List[float]) -> Optional[float]:
    if not values:
        return None
    return sum(values) / len(values)


def variance_population(values: List[float]) -> Optional[float]:
    """
    Population variance.
    """
    if not values:
        return None
    mu = mean(values)
    return sum((x - mu) ** 2 for x in values) / len(values)


def stddev_population(values: List[float]) -> Optional[float]:
    var = variance_population(values)
    if var is None:
        return None
    return math.sqrt(var)


def find_trace_dirs(parent_dir: str) -> List[str]:
    """
    Return immediate child directories only.
    """
    trace_dirs = []
    try:
        for entry in sorted(os.listdir(parent_dir)):
            full_path = os.path.join(parent_dir, entry)
            if os.path.isdir(full_path):
                trace_dirs.append(full_path)
    except OSError as e:
        print(f"Error reading directory '{parent_dir}': {e}", file=sys.stderr)
        
    return trace_dirs


def collect_trace_ratios(trace_dir: str) -> Tuple[Optional[Dict[int, float]], Optional[str]]:
    """
    For one trace directory:
      - Require all log_1.txt ... log_12.txt to exist.
      - Compute prefix ratio up to n against global 12 best.
    """
    ipc_by_n: Dict[int, Optional[float]] = {}

    for n in range(1, MAX_N + 1):
        file_path = os.path.join(trace_dir, f"log_{n}.txt")
        if not os.path.isfile(file_path):
            return None, f"missing log_{n}.txt"
        ipc_by_n[n] = extract_ipc(file_path)

    valid_all = [ipc for ipc in ipc_by_n.values() if ipc is not None]

    if not valid_all:
        return None, "no valid IPC found in any log_n.txt"

    global_best = max(valid_all)
    if global_best <= 0:
        return None, "global best IPC is non-positive"

    ratios: Dict[int, float] = {}
    for n in range(1, MAX_N + 1):
        valid_prefix = [ipc_by_n[k] for k in range(1, n + 1) if ipc_by_n[k] is not None]

        if not valid_prefix:
            continue

        prefix_best = max(valid_prefix)
        ratios[n] = prefix_best / global_best

    if not ratios:
        return None, "no valid prefix ratio could be computed"

    return ratios, None


def write_summary_csv(output_csv: str, summary_rows: List[Dict[str, object]]) -> None:
    fieldnames = ["config", "n", "geomean_ratio", "variance", "stddev", "count"]

    with open(output_csv, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for row in summary_rows:
            writer.writerow(row)


def make_html_plot(output_html: str, plot_data: dict) -> None:
    """
    Generates a Plotly HTML graph where each ChampSim config is a distinct line.
    """
    fig = go.Figure()

    for config, data in plot_data.items():
        fig.add_trace(
            go.Scatter(
                x=data["ns"],
                y=data["geomeans"],
                mode="lines+markers",
                name=config,
                error_y=dict(
                    type="data",
                    array=data["stddevs"],
                    visible=True,
                ),
                customdata=data["counts"],
                hovertemplate=(
                    f"<b>Config: {config}</b><br>"
                    "n=%{x}<br>"
                    "Geomean Ratio=%{y:.6f}<br>"
                    "Stddev=%{error_y.array:.6f}<br>"
                    "Traces Evaluated=%{customdata}<extra></extra>"
                ),
            )
        )

    fig.update_layout(
        title="R-Max: Best Prefix IPC Ratio vs n (Across Configurations)",
        xaxis_title="n (using log_1.txt ... log_n.txt)",
        yaxis_title="Ratio = best IPC (first n runs) / best IPC (all 12 runs)",
        template="plotly_white",
        legend_title="ChampSim Configuration"
    )

    fig.write_html(output_html, include_plotlyjs="cdn")


def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "Scan explicit ChampSim configurations, extract IPC from log_1.txt ... log_12.txt, "
            "compute prefix-best/global-best ratios, and output an aggregated CSV + HTML plot."
        )
    )
    parser.add_argument(
        "config_dirs",
        nargs="+",  # Accepts one or more directory paths
        help="One or more ChampSim configuration directories to analyze (e.g., config1 config2)",
    )
    parser.add_argument(
        "--summary-csv",
        default="ipc_ratio_summary.csv",
        help="Output CSV with config, n, geomean_ratio, variance, stddev, count",
    )
    parser.add_argument(
        "--html",
        default="ipc_ratio_plot.html",
        help="Output HTML plot file",
    )

    args = parser.parse_args()

    all_summary_rows = []
    plot_data = {}
    total_skipped = 0
    total_valid = 0
    valid_configs_parsed = 0

    # Process each provided configuration directory directly
    for config_dir in args.config_dirs:
        config_path = os.path.abspath(config_dir)
        config_name = os.path.basename(config_path) # Use folder name as config name
        results_path = os.path.join(config_path, "results")

        if not os.path.isdir(config_path):
            print(f"Warning: Directory '{config_dir}' does not exist. Skipping.", file=sys.stderr)
            continue

        if not os.path.isdir(results_path):
            print(f"Warning: No 'results' folder found in '{config_dir}'. Skipping.", file=sys.stderr)
            continue

        trace_dirs = find_trace_dirs(results_path)
        
        per_trace_ratios: Dict[str, Dict[int, float]] = {}
        for trace_dir in trace_dirs:
            ratios, reason = collect_trace_ratios(trace_dir)
            if ratios is None:
                total_skipped += 1
                continue
            per_trace_ratios[trace_dir] = ratios
            total_valid += 1

        if not per_trace_ratios:
            print(f"Warning: No valid trace data found in '{config_dir}'.", file=sys.stderr)
            continue

        valid_configs_parsed += 1

        # Compute Geomeans and StdDevs for this specific config
        ns_for_plot, geomeans_for_plot, stddevs_for_plot, counts_for_plot = [], [], [], []
        
        for n in range(1, MAX_N + 1):
            ratios_at_n = [
                ratio_map[n]
                for ratio_map in per_trace_ratios.values()
                if n in ratio_map
            ]

            if not ratios_at_n:
                continue

            gm = geometric_mean(ratios_at_n)
            var = variance_population(ratios_at_n)
            sd = stddev_population(ratios_at_n)

            if gm is None or var is None or sd is None:
                continue

            all_summary_rows.append({
                "config": config_name,
                "n": n,
                "geomean_ratio": gm,
                "variance": var,
                "stddev": sd,
                "count": len(ratios_at_n)
            })

            ns_for_plot.append(n)
            geomeans_for_plot.append(gm)
            stddevs_for_plot.append(sd)
            counts_for_plot.append(len(ratios_at_n))

        if ns_for_plot:
            plot_data[config_name] = {
                "ns": ns_for_plot,
                "geomeans": geomeans_for_plot,
                "stddevs": stddevs_for_plot,
                "counts": counts_for_plot
            }

    # Output the generated data
    if not all_summary_rows:
        print("Error: No valid data could be computed across any provided configuration.", file=sys.stderr)
        sys.exit(1)

    write_summary_csv(args.summary_csv, all_summary_rows)
    make_html_plot(args.html, plot_data)

    print(f"Configurations successfully parsed: {valid_configs_parsed}")
    print(f"Valid trace directories evaluated: {total_valid}")
    print(f"Skipped trace directories (missing files/data): {total_skipped}")
    print(f"Summary CSV written to: {args.summary_csv}")
    print(f"HTML plot written to: {args.html}")

if __name__ == "__main__":
    main()