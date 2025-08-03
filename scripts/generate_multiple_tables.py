#!/usr/bin/env python3
"""
Generate multiple benchmark tables with automatic compilation.

This script automates the process of compiling the benchmark code,
running benchmarks with various configurations, and generating LaTeX tables.
"""
import subprocess
import os
import platform
import argparse
import shutil
from latex_table import generate_latex_table

# Configuration
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
    # ['-F6'],
    ['-s'],
    # ['-F6', '-s'],
]


def parse_args():
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(
        description="Compile, run benchmarks, and generate LaTeX tables")
    parser.add_argument(
        "compiler", help="Compiler to use (g++, clang++)")
    parser.add_argument(
        "--build-dir", default="build",
        help="Build directory (default: build)")
    parser.add_argument(
        "--output-dir", default="./outputs",
        help="Output directory for tables (default: ./outputs)")
    parser.add_argument(
        "--clean", action="store_true",
        help="Clean build directory before compilation")
    parser.add_argument(
        "--march", default="native",
        help="Architecture target for -march flag (default: native)")
    return parser.parse_args()


def get_cpu_model():
    """Get the CPU model name for the current system."""
    env = os.environ.copy()
    env["LANG"] = "C"

    system = platform.system()
    if system == "Windows":
        return platform.processor()
    elif system == "Darwin":
        os.environ['PATH'] += os.pathsep + '/usr/sbin'
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


def compile_benchmarks(compiler, build_dir, clean=False, march="native"):
    """Compile the benchmark code with the specified compiler."""
    print(f"Compiling benchmarks with {compiler}...")

    # Clean build directory if requested
    if clean and os.path.exists(build_dir):
        print(f"Cleaning build directory: {build_dir}")
        shutil.rmtree(build_dir)

    # Set environment variables for compiler
    env = os.environ.copy()
    if compiler == "g++":
        env["CC"] = "gcc"
        env["CXX"] = "g++"
    elif compiler == "clang++":
        env["CC"] = "clang"
        env["CXX"] = "clang++"

    # Configure with CMake
    cmake_cmd = [
        "cmake", "-B", build_dir, ".",
        f"-DSIMPLE_FAST_FLOAT_BENCHMARK_MARCH={march}"
    ]
    print(f"Running: {' '.join(cmake_cmd)}")
    subprocess.run(cmake_cmd, env=env, check=True)

    # Build with CMake
    build_cmd = ["cmake", "--build", build_dir]
    print(f"Running: {' '.join(build_cmd)}")
    subprocess.run(build_cmd, env=env, check=True)

    print("Compilation successful!")


# Helper to run a command and return its stdout
def run_cmd(cmd):
    """Run a command and return its stdout."""
    result = subprocess.run(cmd, capture_output=True, text=True)
    result.check_returncode()
    return result.stdout


# Process a single benchmark invocation and generate .tex
def process_job(benchmark_executable, output_dir, cpu_model, compiler_label,
                label, cmd_args, flags):
    """Run a benchmark and generate LaTeX table."""
    # Run the benchmark
    cmd = [benchmark_executable] + cmd_args + flags
    print(f"Running: {' '.join(cmd)}", flush=True)
    output = run_cmd(cmd)

    # Build output file name
    flag_label = ''.join([f.strip('-') for f in flags]) or 'none'
    safe_label = label.replace('.', '_')
    filename_tex = f"{cpu_model}_{compiler_label}_{safe_label}_{flag_label}.tex"
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


def main():
    """Main function."""
    args = parse_args()

    # Compile the benchmarks
    compile_benchmarks(
        compiler=args.compiler,
        build_dir=args.build_dir,
        clean=args.clean,
        march=args.march
    )

    # Set up paths and directories
    benchmark_executable = f'./{args.build_dir}/benchmarks/benchmark'
    output_dir = args.output_dir
    os.makedirs(output_dir, exist_ok=True)

    # Get CPU model and clean it for filenames
    cpu_model = get_cpu_model().replace(' ', '_').replace('/', '-').replace('@', '')

    # Save compiler information
    compiler_info_path = os.path.join(output_dir, f"{args.compiler}.txt")
    try:
        compiler_version = subprocess.check_output(
            [args.compiler, "--version"], text=True)
        with open(compiler_info_path, 'w') as f:
            f.write(compiler_version)
        print(f"Saved compiler info to: {compiler_info_path}")
    except Exception as e:
        print(f"Warning: Could not get compiler version: {e}")

    # File-based benchmarks
    for filepath in input_files:
        file_label = os.path.splitext(os.path.basename(filepath))[0]
        for flags in flag_combinations:
            process_job(
                benchmark_executable=benchmark_executable,
                output_dir=output_dir,
                cpu_model=cpu_model,
                compiler_label=args.compiler,
                label=file_label,
                cmd_args=['-f', filepath, '-r', str(runs_r)],
                flags=flags
            )

    # Model-based benchmarks
    for model in models:
        for flags in flag_combinations:
            process_job(
                benchmark_executable=benchmark_executable,
                output_dir=output_dir,
                cpu_model=cpu_model,
                compiler_label=args.compiler,
                label=model,
                cmd_args=['-m', model, '-v', str(volume_v), '-r', str(runs_r)],
                flags=flags
            )


if __name__ == '__main__':
    main()
