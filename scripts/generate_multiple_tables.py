#!/usr/bin/env python3
import subprocess
import os
import platform
from latex_table import generate_latex_table

# Configuration
benchmark_executable = './build/benchmarks/benchmark'
latex_script = './scripts/latex_table.py'
output_dir = './outputs'
input_files = [
    'data/canada.txt',
    'data/mesh.txt',
]
models = [
    'uniform_01',
    'uniform_all',
    'integer_uniform',
    'centered',
    'non_centered',
]
runs_r = 1_000
volume_v = 1_000_000
flag_combinations = [
    [],
    ['-F6'],
    ['-s'],
    ['-F6', '-s'],
]


def get_cpu_model():
    if platform.system() == "Windows":
        return platform.processor()
    elif platform.system() == "Darwin":
        os.environ['PATH'] = os.environ['PATH'] + os.pathsep + '/usr/sbin'
        command = "sysctl -n machdep.cpu.brand_string"
        return subprocess.check_output(command).strip()
    elif platform.system() == "Linux":
        command = "cat /proc/cpuinfo"
        output = subprocess.check_output(command, shell=True).decode().strip()
        for line in output.split("\n"):
            if line.startswith("model name"):
                return line.split(':', 1)[1].strip()
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
    print(f"Running: {' '.join(cmd)}")
    output = run_cmd(cmd)

    # Build output file name
    flag_label = ''.join([f.strip('-') for f in flags]) or 'none'
    safe_label = label.replace('.', '_')
    filename = f"{CPUModel}_{safe_label}_{flag_label}.tex"
    out_path = os.path.join(output_dir, filename)

    # Write to file
    tex_content = generate_latex_table(output)
    with open(out_path, 'w') as f:
        f.write(tex_content)
    print(f"Written: {out_path}\n")


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
