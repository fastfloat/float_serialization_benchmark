#!/usr/bin/env python3
"""
Extract and summarize benchmark metrics from raw result files.

This script analyzes benchmark raw output files to extract performance metrics
and provide statistical summaries. It can analyze metrics across different
CPU types (dedicated vs. cloud) and identify outliers.
"""
import os
import re
import statistics
import argparse
from collections import defaultdict


def get_cpu_type(filename, dedicated_cpus=None):
    """
    Determine if a benchmark was run on a dedicated or cloud CPU.

    Args:
        filename: Path to the benchmark result file
        dedicated_cpus: Set of CPU folder names considered dedicated

    Returns:
        String: "dedicated" or "cloud"
    """
    if dedicated_cpus is None:
        dedicated_cpus = {"apple_m4", "AMD_Ryzen9_9900X"}

    parts = os.path.normpath(filename).split(os.sep)
    if len(parts) < 2:
        return "unknown"
    cpu_folder = parts[-2]
    if cpu_folder in dedicated_cpus:
        return "dedicated"
    return "cloud"


def extract_metrics_from_file(filename, percent_metrics=None,
                              raw_metrics=None):
    """
    Extract performance metrics from a benchmark result file.

    Args:
        filename: Path to the benchmark result file
        percent_metrics: List of metrics reported with percent variation
        raw_metrics: List of raw metrics to extract

    Returns:
        Tuple of (percent_values, percent_sources, raw_values)
    """
    if percent_metrics is None:
        percent_metrics = ["MB/s", "c/f", "i/f"]
    if raw_metrics is None:
        raw_metrics = ["i/c"]

    percent_values = defaultdict(list)
    percent_sources = defaultdict(list)
    raw_values = defaultdict(list)
    algo = None

    with open(filename, "r") as f:
        for line in f:
            m = re.match(r"([a-zA-Z0-9_]+)\s*:", line)
            if m:
                algo = m.group(1)
            if algo is None:
                continue

            for metric in percent_metrics:
                regex = rf"{re.escape(metric)}\s*\(\+/-\s*([-+]?\d+\.\d+)\s*%\)"
                pmatch = re.search(regex, line)
                if pmatch:
                    val = float(pmatch.group(1))
                    percent_values[(algo, metric)].append(val)
                    percent_sources[(algo, metric)].append((filename, val))

            for metric in raw_metrics + percent_metrics:
                regex = rf"([-\d\.eE]+)\s+{re.escape(metric)}\b"
                match = re.search(regex, line)
                if match:
                    value = float(match.group(1))
                    raw_values[(algo, metric)].append(value)
    return percent_values, percent_sources, raw_values


def collect_all_stats(root=".", cpu_filter=None, dedicated_cpus=None):
    """
    Collect statistics from all benchmark result files.

    Args:
        root: Root directory to search for benchmark files
        cpu_filter: Optional CPU folder name to filter results
        dedicated_cpus: Set of CPU folder names considered dedicated

    Returns:
        Dictionary of collected statistics
    """
    all_data = {
        "dedicated": {
            "percent": defaultdict(list),
            "percent_src": defaultdict(list),
            "raw": defaultdict(list)
        },
        "cloud": {
            "percent": defaultdict(list),
            "percent_src": defaultdict(list),
            "raw": defaultdict(list)
        },
        "global": {
            "percent": defaultdict(list),
            "percent_src": defaultdict(list),
            "raw": defaultdict(list)
        },
    }

    for dirpath, _, filenames in os.walk(root):
        if cpu_filter is not None:
            if os.path.basename(dirpath) != cpu_filter:
                continue
        for fname in filenames:
            if fname.endswith(".raw"):
                fullpath = os.path.join(dirpath, fname)
                cpu_type = get_cpu_type(fullpath, dedicated_cpus)
                percent_vals, percent_sources, raw_vals = extract_metrics_from_file(fullpath)
                for key, vals in percent_vals.items():
                    all_data[cpu_type]["percent"][key].extend(vals)
                    all_data["global"]["percent"][key].extend(vals)
                for key, vals in percent_sources.items():
                    all_data[cpu_type]["percent_src"][key].extend(vals)
                    all_data["global"]["percent_src"][key].extend(vals)
                for key, vals in raw_vals.items():
                    all_data[cpu_type]["raw"][key].extend(vals)
                    all_data["global"]["raw"][key].extend(vals)
    return all_data


def print_stats_block(label, stats, outlier_threshold=5.0,
                      percent_metrics=None, raw_metrics=None):
    """
    Print a formatted block of statistics.

    Args:
        label: Block label
        stats: Statistics dictionary
        outlier_threshold: Threshold for reporting outliers
        percent_metrics: List of metrics reported with percent variation
        raw_metrics: List of raw metrics to report
    """
    if percent_metrics is None:
        percent_metrics = ["MB/s", "c/f", "i/f"]
    if raw_metrics is None:
        raw_metrics = ["i/c"]

    print(f"\n=== {label.upper()} ===")
    percent_stats = stats["percent"]
    percent_sources = stats["percent_src"]
    raw_stats = stats["raw"]

    for metric in percent_metrics:
        print(f"\nMetric: {metric}")
        algos = sorted(set(a for (a, m) in percent_stats if m == metric))
        for algo in algos:
            vals = percent_stats[(algo, metric)]
            if vals:
                mean = statistics.mean(vals)
                median = statistics.median(vals)
                print(
                    f"  Algorithm: {algo:15s} [%] "
                    f"min={min(vals):.2f}%, max={max(vals):.2f}%, "
                    f"mean={mean:.2f}%, median={median:.2f}% (n={len(vals)})"
                )
        all_vals = [v for ((a, m), vs) in percent_stats.items()
                    if m == metric for v in vs]
        if all_vals:
            mean = statistics.mean(all_vals)
            median = statistics.median(all_vals)
            print(
                f"  [Global][%]   min={min(all_vals):.2f}%, "
                f"max={max(all_vals):.2f}%, mean={mean:.2f}%, "
                f"median={median:.2f}% (n={len(all_vals)})"
            )
            outlier_vals = []
            for (algo, m), vs in percent_sources.items():
                if m == metric:
                    for fname, v in vs:
                        if v > outlier_threshold:
                            outlier_vals.append((v, algo, fname))
            if outlier_vals:
                print(f"    Outliers above {outlier_threshold:.1f}%:")
                for v, algo, fname in sorted(outlier_vals, reverse=True):
                    print(f"      {v:.2f}% : {algo} [{fname}]")

    for metric in raw_metrics:
        print(f"\nMetric: {metric}")
        algos = sorted(set(a for (a, m) in raw_stats if m == metric))
        for algo in algos:
            vals = raw_stats[(algo, metric)]
            if vals:
                mean = statistics.mean(vals)
                median = statistics.median(vals)
                print(
                    f"  Algorithm: {algo:15s} [raw] "
                    f"min={min(vals):.4g}, max={max(vals):.4g}, "
                    f"mean={mean:.4g}, median={median:.4g} (n={len(vals)})"
                )
        all_vals = [v for ((a, m), vs) in raw_stats.items()
                    if m == metric for v in vs]
        if all_vals:
            mean = statistics.mean(all_vals)
            median = statistics.median(all_vals)
            print(
                f"  [Global][raw] min={min(all_vals):.4g}, "
                f"max={max(all_vals):.4g}, mean={mean:.4g}, "
                f"median={median:.4g} (n={len(all_vals)})"
            )


def main():
    parser = argparse.ArgumentParser(
        description="Summarize metrics from benchmark .raw files")
    parser.add_argument(
        "--cpu", type=str,
        help="CPU folder name to restrict analysis (e.g. 'apple_m4')")
    parser.add_argument(
        "--input-dir", "-i", default="./outputs",
        help="Directory containing benchmark .raw files")
    parser.add_argument(
        "--outlier-threshold", "-t", type=float, default=5.0,
        help="Threshold for reporting outliers (default: 5.0%%)")
    parser.add_argument(
        "--dedicated-cpus", "-d", nargs="+",
        default=["apple_m4", "AMD_Ryzen9_9900X"],
        help="CPU folder names considered dedicated (non-cloud)")
    args = parser.parse_args()

    dedicated_cpus = set(args.dedicated_cpus)

    if args.cpu:
        print(f"\nFiltering for CPU: {args.cpu}\n")
        all_data = collect_all_stats(
            args.input_dir, cpu_filter=args.cpu, dedicated_cpus=dedicated_cpus)
        # Only print "global" block in this mode for clarity
        print_stats_block(
            args.cpu, all_data["global"], outlier_threshold=args.outlier_threshold)
    else:
        all_data = collect_all_stats(
            args.input_dir, dedicated_cpus=dedicated_cpus)
        print_stats_block(
            "dedicated", all_data["dedicated"],
            outlier_threshold=args.outlier_threshold)
        print_stats_block(
            "cloud", all_data["cloud"],
            outlier_threshold=args.outlier_threshold)
        print_stats_block(
            "global", all_data["global"],
            outlier_threshold=args.outlier_threshold)


if __name__ == "__main__":
    main()
