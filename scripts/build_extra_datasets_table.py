#!/usr/bin/env python3
"""
Generate a compact LaTeX table summarizing performance on real-world datasets
across multiple CPUs and compilers.

Outputs a table of the form:

Algorithm | Dataset | Ryzen(...) | Apple M4(...)

Rules:
- Dataset name extracted automatically from filenames (no hardcoded list)
- Floatbits determined from filename (_s = float32, else float64)
- Only the *native* floatbits for each dataset are kept (e.g., mobilenet=32, gaia=64)
- All CPUs detected automatically
- Filter algorithms via --algos, or by setting ALGO_FILTER

Directory structure must match your outputs/, e.g.:
    outputs/
        AMD_Ryzen9_9900X/...
        apple_m4/...
"""

import os
import re
import argparse
import pandas as pd

# Regex for parsing rows: algorithm & ns/f & ins/f & ins/c
ROW_RE = re.compile(
    r"^\s*(?P<algo>[A-Za-z0-9_:\\]+)\s*&\s*(?P<nsf>[\d\.]+)\s*&\s*(?P<insf>[\d\.]+)\s*&\s*(?P<insc>[\d\.]+)"
)

NATIVE_FLOATBITS = {
    "canada": 64,
    "marine_ik": 32,
    "mesh": 64,
    "bitcoin": 64,
    "numbers": 64,
    "mobilenetv3_large": 32,
    "gaia": 64,
    "noaa_global_hourly_2023": 32,
    "noaa_gfs_1p00": 32,
    "hellfloat64": 64,
}


def detect_dataset(filename):
    """
    Automatically extract dataset name from filename.

    Filename convention:
        CPU_COMPILER_DATASET_VARIANT(.tex/.raw)
    e.g.:
        Ryzen9900x_g++_mobilenetv3_s.tex
        Apple_M4_Max_clang++_noaa_global_hourly_2023_float64.tex

    Dataset = all parts after compiler (g++/clang++) and before last 1 part.
    """
    base = os.path.splitext(filename)[0]
    parts = base.split("_")

    # Find compiler index
    comp_idx = None
    for i, p in enumerate(parts):
        if p in ("g++", "clang++"):
            comp_idx = i
            break
    if comp_idx is None:
        return None  # cannot detect dataset

    # dataset parts = everything after compiler, except last element (variant)
    dataset_parts = parts[comp_idx + 1:-1]
    if not dataset_parts:
        return None

    return "_".join(dataset_parts)


def detect_float_bits(filename):
    """
    Detect float bits:
    - filenames ending in *_s.* → float32
    - otherwise → float64
    """
    base = os.path.splitext(filename)[0]
    if base.endswith("_s"):
        return 32
    return 64


def clean_cpu(cpu_dir):
    """
    Clean CPU directory name for nicer LaTeX display.
    """
    cpu = cpu_dir.replace("_", " ").strip()

    # Ryzen name cleanup
    cpu = cpu.replace("Ryzen9", "Ryzen 9")

    # Apple cleanup
    cpu = cpu.replace("apple m4", "Apple M4 Max")
    cpu = cpu.replace("apple", "Apple")

    return cpu


def parse_tex_result(path, cpu, compiler):
    """
    Parse a 4-column LaTeX table row:
        algo, ns/f, ins/f, ins/c
    Return dict rows: algorithm, metrics, cpu, dataset, floatbits.
    """
    fname = os.path.basename(path)
    dataset = detect_dataset(fname)
    if dataset is None:
        return []

    floatbits = detect_float_bits(fname)
    rows = []

    with open(path, "r") as f:
        for line in f:
            m = ROW_RE.match(line)
            if not m:
                continue
            rows.append({
                "algorithm": m.group("algo"),
                "dataset": dataset,
                "cpu": cpu,
                "compiler": compiler,
                "floatbits": floatbits,
                "ns/f": float(m.group("nsf")),
                "ins/f": float(m.group("insf")),
                "ins/c": float(m.group("insc")),
            })

    return rows


def scan_outputs(root):
    """
    Recursively scan subdirectories for .tex result files.
    """
    all_rows = []

    for cpu_dir in sorted(os.listdir(root)):
        cpu_path = os.path.join(root, cpu_dir)
        if not os.path.isdir(cpu_path):
            continue

        cpu_clean = clean_cpu(cpu_dir)
        for fname in sorted(os.listdir(cpu_path)):
            if not fname.endswith(".tex"):
                continue

            full = os.path.join(cpu_path, fname)

            # determine compiler
            if "_g++" in fname:
                compiler = "g++"
            elif "_clang++" in fname:
                compiler = "clang++"
            else:
                compiler = "unknown"

            # parse
            rows = parse_tex_result(full, cpu_clean, compiler)
            all_rows.extend(rows)

    return all_rows


def select_native_floatbits(df):
    """
    Keep only the native floatbits for each dataset.
    Native floatbits are defined explicitly in NATIVE_FLOATBITS.
    """
    cleaned = []
    for i, row in df.iterrows():
        ds = row["dataset"]
        if ds not in NATIVE_FLOATBITS:
            continue

        if row["floatbits"] == NATIVE_FLOATBITS[ds]:
            cleaned.append(True)
        else:
            cleaned.append(False)

    return df[cleaned]


def latex_table(df, algo_filter=None):
    """
    Build the final compact LaTeX table.
    Columns:
        Dataset | Algorithm | CPU1(ns/f,ins/f,ins/c) | CPU2(...) | ...
    """
    if algo_filter:
        df = df[df["algorithm"].isin(algo_filter)]

    cpus = sorted(df["cpu"].unique())
    datasets = sorted(df["dataset"].unique())

    tex = []
    tex.append("\\begin{table*}[htbp]")
    tex.append("  \\centering")
    tex.append(
        "  \\caption{Performance on additional real-world datasets across CPUs.}"
    )
    tex.append("  \\label{tab:additional_summary}")
    tex.append("")
    tex.append("  \\begin{tabular}{ll" + "r" * (3 * len(cpus)) + "}")
    tex.append("    \\toprule")

    # CPU header row — minimal change
    header = ["Dataset", "Algorithm"]
    for cpu in cpus:
        header.append(f"\\multicolumn{{3}}{{c}}{{{cpu}}}")
    tex.append("    " + " & ".join(header) + " \\\\")

    for i in range(len(cpus)):
        left = 3 + 3 * i
        right = left + 2
        tex.append(f"    \\cmidrule(lr){{{left}-{right}}}")

    # Metric header — unchanged except for swapped order
    sub = ["", ""]
    sub.extend(["ns/f", "ins/f", "ins/c"] * len(cpus))
    tex.append("    " + " & ".join(sub) + " \\\\")
    tex.append("    \\midrule")

    # Rows grouped by dataset — minimal change applied
    prev_ds = None
    for ds in datasets:

        # midrule between dataset blocks
        if prev_ds is not None:
            tex.append("    \\midrule")
        prev_ds = ds

        # dataset label
        display_ds = f"{ds} (f{NATIVE_FLOATBITS[ds]})".replace("_", "\\_")

        first_line = True
        for algo in sorted(df["algorithm"].unique()):

            # dataset only on first line of group
            if first_line:
                row = [display_ds, algo]
                first_line = False
            else:
                row = ["", algo]

            # Collect values per CPU as triples
            cpu_values = []
            for cpu in cpus:
                subdf = df[(df["algorithm"] == algo) & (df["dataset"] == ds) &
                           (df["cpu"] == cpu)]
                if subdf.empty:
                    cpu_values.append(["--", "--", "--"])
                else:
                    r = subdf.iloc[0]
                    cpu_values.append([
                        f"{r['ns/f']:.1f}",
                        f"{r['ins/f']:.0f}",
                        f"{r['ins/c']:.1f}",
                    ])

            flat = [v for triple in cpu_values for v in triple]

            if all(v == "--" for v in flat):
                continue

            for triple in cpu_values:
                row.extend(triple)

            tex.append("    " + " & ".join(row) + " \\\\")

    tex.append("    \\bottomrule")
    tex.append("  \\end{tabular}")
    tex.append("\\end{table*}\n")
    return "\n".join(tex)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-i",
                    "--input-dir",
                    default="./outputs",
                    help="Directory containing CPU output folders.")
    ap.add_argument("-o",
                    "--output",
                    default="./outputs/extra_datasets_table.tex",
                    help="Output LaTeX filename.")
    ap.add_argument("--algos",
                    nargs="+",
                    default=None,
                    help="Filter: keep only these algorithms.")
    args = ap.parse_args()

    rows = scan_outputs(args.input_dir)
    if not rows:
        print("No results found.")
        return

    df = pd.DataFrame(rows)
    if df.empty:
        print("No valid rows parsed.")
        return

    df = select_native_floatbits(df)
    tex = latex_table(df, args.algos)

    with open(args.output, "w") as f:
        f.write(tex)

    print(f"[OK] wrote {args.output}")


if __name__ == "__main__":
    main()
