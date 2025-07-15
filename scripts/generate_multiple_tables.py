#!/usr/bin/env python3
import subprocess
import os
import platform
import sys
from latex_table import generate_latex_table

# Configuration
benchmark_executable = './build/benchmarks/benchmark'
output_dir = './outputs'
input_files = [
    'data/canada.txt',
    'data/mesh.txt',
]
models = [
    'uniform_01',
    # 'logspace_all',
    # 'integer_uniform',
    # 'centered',
    # 'non_centered',
]
runs_r = 100
volume_v = 100_000
flag_combinations = [
    [],
    ['-F6'],
    ['-s'],
    ['-F6', '-s'],
]

# Get compiler label from command line
if len(sys.argv) < 2:
    print("Usage: ./scripts/generate_multiple_tables.py <compiler_name>")
    sys.exit(1)
CompilerLabel = sys.argv[1]


def get_cpu_model():
    env = os.environ.copy()
    env["LANG"] = "C"

    system = platform.system()
    if system == "Windows":
        return platform.processor()
    elif system == "Darwin":
        os.environ['PATH'] = os.environ['PATH'] + os.pathsep + '/usr/sbin'
        command = ["sysctl", "-n", "machdep.cpu.brand_string"]
        return subprocess.check_output(command, env=env, text=True).strip()
    elif system == "Linux":
        output = subprocess.check_output(["lscpu"], env=env, text=True)
        model_name = None
        architecture = None
        for line in output.splitlines():
            if "Model name:" in line:
                model_name = line.split(":", 1)[1].strip()
            elif "Architecture:" in line:
                architecture = line.split(":", 1)[1].strip()
        # Prefer model_name if available; fallback to architecture
        return model_name or architecture or "unknown_cpu"
    return "unknown_cpu"


CPUModel = get_cpu_model().replace(' ', '_').replace('/', '-').replace('@', '')
os.makedirs(output_dir, exist_ok=True)


# Helper to run a command and return its stdout
def run_cmd(cmd):
    result = subprocess.run(cmd, capture_output=True, text=True)
    result.check_returncode()
    return result.stdout


# Process a single benchmark invocation and generate .tex
def process_job(label, cmd_args, flags):
    # Run the benchmark
    cmd = [benchmark_executable] + cmd_args + flags
    print(f"Running: {' '.join(cmd)}", flush=True)
    output = run_cmd(cmd)

    # Build output file name
    flag_label = ''.join([f.strip('-') for f in flags]) or 'none'
    safe_label = label.replace('.', '_')
    filename_tex = f"{CPUModel}_{CompilerLabel}_{safe_label}_{flag_label}.tex"
    filename_raw = filename_tex[:-4] + '.raw'  # replace .tex with .raw
    out_path_tex = os.path.join(output_dir, filename_tex)
    out_path_raw = os.path.join(output_dir, filename_raw)

    # Write LaTeX table to file
    tex_content = generate_latex_table(output)
    with open(out_path_tex, 'w') as f:
        f.write(tex_content)
    print(f"Written: {out_path_tex}", flush=True)

    # Write raw output to .raw file
    with open(out_path_raw, 'w') as f:
        f.write(output)
    print(f"Written: {out_path_raw}\n", flush=True)


if __name__ == '__main__':
    # File-based benchmarks
    for filepath in input_files:
        file_label = os.path.splitext(os.path.basename(filepath))[0]
        for flags in flag_combinations:
            process_job(
                label=file_label,
                cmd_args=['-f', filepath, '-r', str(runs_r)],
                flags=flags
            )

    # Model-based benchmarks
    for model in models:
        for flags in flag_combinations:
            process_job(
                label=model,
                cmd_args=['-m', model, '-v', str(volume_v), '-r', str(runs_r)],
                flags=flags
            )
