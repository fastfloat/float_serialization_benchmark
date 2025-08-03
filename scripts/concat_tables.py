#!/usr/bin/env python3
"""
Concatenate multiple benchmark result tables into a single comprehensive table.

This script finds and combines related benchmark results from different datasets
(mesh, canada, uniform_01) into a single LaTeX table for easier comparison.
"""
import os
import re
import argparse
import pandas as pd


def parse_tex_table(filepath):
    """Parse a LaTeX table file into a pandas DataFrame."""
    with open(filepath, 'r') as file:
        lines = file.readlines()
    data_start = False
    parsed = []
    for line in lines:
        if "\\midrule" in line:
            data_start = True
            continue
        if "\\bottomrule" in line:
            break
        if data_start and '&' in line:
            row = [x.strip().strip('\\') for x in line.split('&')]
            if len(row) == 4:
                parsed.append({
                    'algorithm': row[0],
                    'ns/f': row[1],
                    'ins/f': row[2],
                    'ins/c': row[3]
                })
    return pd.DataFrame(parsed)


def clean_cpu_name(cpu_name):
    """Clean CPU name for better display in tables."""
    cpu_cleaned = cpu_name.replace("Ryzen9900x", "Ryzen 9900X")
    cpu_cleaned = cpu_cleaned.replace("_Platinum", "")
    cpu_cleaned = re.sub(r"_\d+-Core_Processor", "", cpu_cleaned)
    cpu_cleaned = re.sub(r"_CPU__\d+\.\d+GHz", "", cpu_cleaned)
    cpu_cleaned = re.sub(r"\(R\)", "", cpu_cleaned)
    return cpu_cleaned.replace("_", " ").replace("  ", " ").strip()


def format_latex_table(df, cpu_name, compiler, float_bits, microarch=None,
                       exclude_algos=None):
    """Format the combined data as a LaTeX table."""
    if exclude_algos is None:
        exclude_algos = set()

    cpu_cleaned = clean_cpu_name(cpu_name)
    caption = f"{cpu_cleaned} results ({compiler}, {float_bits}-bit floats"
    if microarch:
        caption += f", {microarch}"
    caption += ")"
    label = f"tab:{re.sub(r'[^a-zA-Z0-9]+', '', cpu_name.lower())}results"
    header = (
        "\\begin{table}\n"
        "  \\centering\n"
        f"  \\caption{{{caption}}}%\n"
        f"  \\label{{{label}}}\n"
        "  \\begin{tabular}{lccccccccc}\n"
        "    \\toprule\n"
        "    \\multirow{1}{*}{Name}   & \\multicolumn{3}{c|}{mesh}  & "
        "\\multicolumn{3}{c|}{canada} & \\multicolumn{3}{c}{unit} \\\\\n"
        "                            & {ns/f} & {ins/f} & {ins/c} & "
        "{ns/f} & {ins/f} & {ins/c}  & {ns/f} & {ins/f} & {ins/c} \\\\ "
        "\\midrule\n"
    )
    body = ""
    for _, row in df.iterrows():
        if row['algorithm'] in exclude_algos:
            continue
        line = (
            f"    {row['algorithm']} & {row['ns/f_mesh']} & "
            f"{row['ins/f_mesh']} & {row['ins/c_mesh']} & "
            f"{row['ns/f_canada']} & {row['ins/f_canada']} & "
            f"{row['ins/c_canada']} & "
            f"{row['ns/f_unit']} & {row['ins/f_unit']} & "
            f"{row['ins/c_unit']} \\\\\n"
        )
        body += line
    footer = (
        "    \\bottomrule\n"
        "  \\end{tabular}\\restartrowcolors\n"
        "\\end{table}\n"
    )
    return header + body + footer


def find_combinations(root, pattern=None):
    """Find all combinations of benchmark result files that can be combined."""
    if pattern is None:
        pattern = re.compile(
            r"(.*?)_(g\+\+|clang\+\+)_(mesh|canada|uniform_01)_(none|s)"
            r"(?:_(x86-64|x86-64-v2|x86-64-v3|x86-64-v4|native))?\.tex"
        )
    # group(1)=cpu, 2=compiler, 3=dataset, 4=variant, 5=microarch (optional)

    combos = []
    for dirpath, _, filenames in os.walk(root):
        tex_files = [f for f in filenames if f.endswith('.tex')]
        table = {}
        for f in tex_files:
            m = pattern.match(f)
            if m:
                cpu, compiler, dataset, variant, microarch = m.groups()
                key = (dirpath, cpu, compiler, variant, microarch)
                if key not in table:
                    table[key] = {}
                table[key][dataset] = os.path.join(dirpath, f)
        for (dirpath, cpu, compiler, variant, microarch), files in table.items():
            if {"mesh", "canada", "uniform_01"}.issubset(files.keys()):
                combos.append((dirpath, cpu, compiler, variant, microarch, files))
    return combos


def main():
    parser = argparse.ArgumentParser(
        description="Concatenate benchmark tables into comprehensive tables")
    parser.add_argument(
        "--input-dir", "-i", default="./outputs",
        help="Directory containing benchmark .tex files")
    parser.add_argument(
        "--output-dir", "-o",
        help="Output directory for combined tables (defaults to input directory)")
    parser.add_argument(
        "--exclude", "-e", nargs="+",
        default=["netlib", "teju\\_jagua", "yy\\_double", "snprintf", "abseil"],
        help="Algorithms to exclude from the output tables")
    args = parser.parse_args()

    input_dir = args.input_dir
    output_dir = args.output_dir if args.output_dir else input_dir
    exclude_algos = set(args.exclude)

    # Create output directory if it doesn't exist
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    combos = find_combinations(input_dir)
    if not combos:
        print(f"No matching benchmark files found in {input_dir}")
        return

    print(f"Found {len(combos)} combinations to process")

    for dirpath, cpu, compiler, variant, microarch, paths in combos:
        df_mesh = parse_tex_table(paths['mesh'])
        df_canada = parse_tex_table(paths['canada'])
        df_unit = parse_tex_table(paths['uniform_01'])
        df_merged = df_mesh.merge(
            df_canada, on='algorithm', suffixes=('_mesh', '_canada'))
        df_merged = df_merged.merge(df_unit, on='algorithm')
        df_merged.rename(columns={
            'ns/f': 'ns/f_unit',
            'ins/f': 'ins/f_unit',
            'ins/c': 'ins/c_unit'
        }, inplace=True)

        float_bits = "32" if variant == "s" else "64"
        tex_code = format_latex_table(
            df_merged, cpu, compiler, float_bits, microarch, exclude_algos)

        suffix = f"_{microarch}" if microarch else ""
        out_path = os.path.join(
            output_dir, f"{cpu}_{compiler}_all_{variant}{suffix}.tex")
        with open(out_path, "w") as f:
            f.write(tex_code)
        print(f"[OK] {out_path}")


if __name__ == "__main__":
    main()
