#!/usr/bin/env python3
import sys
import re
import argparse


# Function to format a number to two significant digits
def format_to_two_sig_digits(value):
    if not isinstance(value, (int, float)) or value == 0:
        return "N/A"

    # Handle negative numbers
    is_negative = value < 0
    abs_value = abs(value)

    exponent = 0

    while abs_value >= 100:
        abs_value /= 10
        exponent += 1
    while abs_value < 10:
        abs_value *= 10
        exponent -= 1

    # Round to two significant digits
    abs_value = round(abs_value, 0)

    # Format the number
    if exponent >= 0 and exponent <= 4:
        format = f"{'-' if is_negative else ''}{abs_value * 10 ** exponent}"
        format = format.replace(".0", "")
        return format
    elif exponent < 0 and exponent >= -4:
        return f"{'-' if is_negative else ''}{abs_value * 10 ** exponent:.1f}"
    else:
        return f"{'-' if is_negative else ''}{abs_value:.1f}e{exponent}"


# Function to parse the raw input data
def parse_input(data):
    lines = data.splitlines()
    parsed_data = []
    current_entry = None

    for line in lines:
        line = line.strip()
        # Skip empty lines or comments
        if not line or line.startswith("#"):
            continue

        # Match lines that start a new entry (e.g., "just_string : 1365.92 MB/s ...")
        match_entry = re.match(r"(\S+)\s*:\s*[\d.]+\s*MB/s", line)
        if match_entry:
            current_entry = {"name": match_entry.group(1)}
            parsed_data.append(current_entry)
        if not current_entry:
            continue

        # Match lines with ns/f
        match_ns = re.search(r"([\d.]+)\s*ns/f", line)
        if match_ns and current_entry:
            current_entry["ns_per_float"] = float(match_ns.group(1))

        # Match lines with instructions/float (i/f)
        match_inst_float = re.search(r"([\d.]+)\s*i/f", line)
        if match_inst_float and current_entry:
            current_entry["inst_per_float"] = float(match_inst_float.group(1))

        # Match lines with instructions/cycle (i/c)
        match_inst_cycle = re.search(r"([\d.]+)\s*i/c", line)
        if match_inst_cycle and current_entry:
            current_entry["inst_per_cycle"] = float(match_inst_cycle.group(1))

    # Filter out incomplete entries
    return parsed_data


# Function to generate LaTeX table
def generate_latex_table(raw_input):
    data = parse_input(raw_input)

    latex_table = r"""
\begin{tabular}{lccc}
\toprule
\textbf{Name} & \textbf{ns/f} & \textbf{instructions/float} & \textbf{instructions/cycle} \\
\midrule
"""
    for entry in data:
        name = entry["name"].replace("_", "\\_")  # Escape underscores for LaTeX
        ns_per_float = format_to_two_sig_digits(entry['ns_per_float']) if 'ns_per_float' in entry else 'N/A'
        inst_per_float = format_to_two_sig_digits(entry['inst_per_float']) if 'inst_per_float' in entry else 'N/A'
        inst_per_cycle = format_to_two_sig_digits(entry['inst_per_cycle']) if 'inst_per_cycle' in entry else 'N/A'
        latex_table += f"{name} & {ns_per_float} & {inst_per_float} & {inst_per_cycle} \\\\ \n"
    latex_table += r"""\bottomrule
\end{tabular}
"""
    return latex_table


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate LaTeX table from performance data")
    parser.add_argument("file", nargs="?", help="Optional input file name (if not provided, reads from stdin)")
    args = parser.parse_args()

    # Read input data
    if args.file:
        try:
            with open(args.file, "r") as f:
                raw_input = f.read()
        except FileNotFoundError:
            print(f"Error: File '{args.file}' not found.", file=sys.stderr)
            sys.exit(1)
        except IOError as e:
            print(f"Error reading file '{args.file}': {e}", file=sys.stderr)
            sys.exit(1)
    else:
        raw_input = sys.stdin.read()

    latex_output = generate_latex_table(raw_input)
    print(latex_output)
