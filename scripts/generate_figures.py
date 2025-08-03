#!/usr/bin/env python3
"""
Generate visualization figures from benchmark results.

This script creates heatmaps and relative performance plots from benchmark
results stored in LaTeX tables. It helps visualize performance differences
across algorithms, CPUs, and compilers.
"""
import os
import re
import sys
import argparse
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from collections import defaultdict


def parse_table(filepath, metric_name):
    """
    Parse a LaTeX table file to extract benchmark metrics.

    Args:
        filepath: Path to the LaTeX table file
        metric_name: Metric to extract (nsf, insf, or insc)

    Returns:
        Tuple of (cpu_name, compiler, width, results_dict)
    """
    metrics = {"nsf": [1, 4, 7], "insf": [2, 5, 8], "insc": [3, 6, 9]}

    with open(filepath, encoding="utf-8") as f:
        lines = f.readlines()

    caption_line = next(line for line in lines if '\\caption' in line)
    cpu_caption = re.search(r'\\caption\{(.+?) results', caption_line)
    cpu_name = cpu_caption.group(1).strip() if cpu_caption else "UnknownCPU"

    compiler = "clang++" if "clang++" in os.path.basename(filepath) else "g++"
    width = "64" if filepath.endswith("_all_none.tex") else "32"

    testcases = ["mesh", "canada", "unit"]

    results = defaultdict(dict)
    in_data = False
    for line in lines:
        if '\\midrule' in line:
            in_data = True
            continue
        if '\\bottomrule' in line:
            break
        if in_data:
            row = line.strip()
            if not row or row.startswith('%') or row.startswith('\\'):
                continue
            parts = [x.strip() for x in row.split('&')]
            if len(parts) < 10:
                continue
            algo = parts[0]
            try:
                for i, testcase in zip(metrics[metric_name], testcases):
                    metric = float(parts[i])
                    results[algo][testcase] = metric
            except Exception:
                continue
    return cpu_name, compiler, width, results


def plot_relative_performance(df, dataset, cpus_to_plot=None,
                              outfile="relative_performance.pdf",
                              considered_suffix="-C-64"):
    """
    Create a relative performance plot comparing algorithms.

    Args:
        df: DataFrame with benchmark results
        dataset: Dataset name (mesh, canada, unit)
        cpus_to_plot: List of CPU names to include
        outfile: Output file path
        considered_suffix: Suffix to filter columns
    """
    df = df.copy()
    cols_to_keep = [col for col in df.columns
                    if col.endswith(considered_suffix) and
                    any(cpu in col for cpu in cpus_to_plot)]
    df = df[cols_to_keep]

    # Use a predefined order for algorithms if available
    if hasattr(df, 'reindex') and 'algorithm_order' in globals():
        df = df.reindex(algorithm_order)

    if "dragon4" not in df.index:
        print("Dragon4 not found in DataFrame; can't normalize!")
        return

    df_rel = df.loc["dragon4"] / df
    df_rel = df_rel.drop("dragon4", axis=0)

    # Use display name mapping if available
    if 'algo_display_map' in globals():
        df_rel.index = [algo_display_map.get(algo, algo)
                        for algo in df_rel.index]

    plt.figure(figsize=(10, 4))
    for col in df_rel.columns:
        plt.plot(df_rel.index, df_rel[col], marker='o', label=col)
    plt.ylabel("Rel. speedup (vs. Dragon4)")
    plt.xlabel("Algorithm")
    plt.xticks(rotation=15, ha='right')
    plt.legend(loc='upper left', fontsize=10)
    plt.tight_layout()
    plt.savefig(outfile)
    plt.close()
    print(f"Generated: {outfile}")


def main():
    parser = argparse.ArgumentParser(
        description="Generate visualization figures from benchmark results")
    parser.add_argument(
        "metric_name", choices=["nsf", "insf", "insc"],
        help="Metric to visualize (nsf=nanoseconds/float, insf=instructions/float, "
             "insc=instructions/cycle)")
    parser.add_argument(
        "input_dir", "-i", default="./outputs",
        help="Directory containing benchmark result .tex files")
    parser.add_argument(
        "--output-dir", "-o", default=None,
        help="Directory to save generated figures (default: same as input directory)")
    parser.add_argument(
        "--exclude", "-e", nargs="+", default=[],
        help="Algorithms to exclude from visualization")
    parser.add_argument(
        "--cpus", "-c", nargs="+",
        default=["Ryzen 9900X", "AMD EPYC 7R13", "Intel Xeon 8488C",
                 "Apple M4 Max", "Neoverse-V2"],
        help="CPUs to include in relative performance plots")
    args = parser.parse_args()

    # Set output directory to input directory if not specified
    if args.output_dir is None:
        args.output_dir = args.input_dir

    # Create output directory if it doesn't exist
    if not os.path.exists(args.output_dir):
        os.makedirs(args.output_dir)

    # Configuration
    sns.set_context("paper", font_scale=1.5)
    algorithms_to_exclude = args.exclude
    cpus_to_plot = args.cpus

    # Algorithm display name mapping
    global algo_display_map
    algo_display_map = {
        "ryu": "ryū",
        "double_conversion": "double_conv.",
    }

    # Compiler mapping
    compiler_map = {"clang++": "C", "g++": "G"}

    # Algorithm order for consistent display
    global algorithm_order
    algorithm_order = [
        "dragon4", "netlib", "double_conversion", "fmt_format", "grisu3",
        "swiftDtoa", "grisu_exact", "schubfach", "ryu", "dragonbox"
    ]

    # Find relevant files
    relevant_files = []
    for root, _, files in os.walk(args.input_dir):
        for file in files:
            if file.endswith("_all_none.tex") or file.endswith("_all_s.tex"):
                relevant_files.append(os.path.join(root, file))

    if not relevant_files:
        print(f"No relevant .tex files found in {args.input_dir}!")
        sys.exit(1)

    # Process files and collect results
    all_results = {
        "mesh": defaultdict(dict),
        "canada": defaultdict(dict),
        "unit": defaultdict(dict)
    }

    for filepath in relevant_files:
        cpu_name, compiler, width, table = parse_table(filepath, args.metric_name)
        shortened_compiler = compiler_map[compiler]
        colname = f"{cpu_name}-{shortened_compiler}-{width}"
        for algo, tc_dict in table.items():
            for testcase, metric in tc_dict.items():
                if testcase in all_results:
                    all_results[testcase][algo][colname] = metric

    # Create DataFrames from collected results
    dfs = {}
    for testcase, d in all_results.items():
        df = pd.DataFrame.from_dict(d, orient="index").sort_index(axis=1)
        dfs[testcase] = df

    # Generate plots for each testcase
    for testcase in ["mesh", "canada", "unit"]:
        df = dfs[testcase].copy()

        # Filter algorithms with enough data points
        most_common_algos = df.dropna(thresh=10).index
        df = df.loc[most_common_algos]
        df.index = df.index.str.replace("\\", "", regex=False)
        df = df[~df.index.isin(algorithms_to_exclude)]

        # Generate relative performance plot
        plot_relative_performance(
            df, testcase, cpus_to_plot=cpus_to_plot,
            outfile=os.path.join(args.output_dir,
                                 f"relative_performance_{testcase}.pdf")
        )

        # Generate heatmap
        df.index = [algo_display_map.get(algo, algo) for algo in df.index]
        df_log = df.map(lambda x: None if pd.isna(x) else
                        (float("nan") if x <= 0 else np.log10(x))
                        ).apply(pd.to_numeric, errors="coerce")
        plt.figure(figsize=(18, 7), constrained_layout=True)
        sns.heatmap(df_log, annot=False, cmap="coolwarm", linewidths=0.1,
                    cbar_kws={'label': f'log$_{{10}}$({args.metric_name})'})
        plt.ylabel("Algorithm")
        plt.savefig(os.path.join(args.output_dir, f"heatmap_{testcase}.pdf"))
        plt.close()
        print(f"Generated: heatmap_{testcase}.pdf")

    print("All heatmaps and relative performance plots generated.")


if __name__ == '__main__':
    main()
