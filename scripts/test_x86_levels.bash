#!/bin/bash
#
# This script benchmarks floating-point serialization performance across different
# x86 microarchitecture levels (x86-64, x86-64-v2, x86-64-v3, x86-64-v4, native).
# It compiles and runs benchmarks for each architecture level, then generates
# LaTeX tables from the results.
#
# Usage: ./test_x86_levels.bash <CPU_NAME>
#
# The script will:
# 1. Create an output directory for the specified CPU
# 2. For each architecture level:
#    - Compile the benchmarks with the appropriate -march flag
#    - Run benchmarks on three datasets (canada, mesh, uniform_01)
#    - Save raw results to output files
# 3. Convert all raw results to LaTeX tables
#
# Results are saved in the outputs/<CPU_NAME> directory.

CPU=$1
OutputDir="outputs/${CPU}"
Algorithms="schubfach,dragonbox"  # comma-separated list

# Check if CPU name was provided
if [ -z "$1" ]; then
  echo "Usage: $0 <CPU>"
  exit 1
fi

# Create output directory
mkdir -p ${OutputDir}

# Test each x86 architecture level
for v in x86-64 x86-64-v2 x86-64-v3 x86-64-v4 native; do
  # Compile with specific architecture target
  cmake -B build-${v} -DSIMPLE_FAST_FLOAT_BENCHMARK_MARCH=${v}
  cmake --build build-${v}

  echo "Running benchmarks for ${v} on ${CPU}..."

  # Run benchmarks on different datasets
  ./build-${v}/benchmarks/benchmark -f data/canada.txt -a ${Algorithms} > ${OutputDir}/${CPU}_g++_canada_none_${v}.raw
  ./build-${v}/benchmarks/benchmark -f data/mesh.txt -a ${Algorithms} > ${OutputDir}/${CPU}_g++_mesh_none_${v}.raw
  ./build-${v}/benchmarks/benchmark -a ${Algorithms} > ${OutputDir}/${CPU}_g++_uniform_01_none_${v}.raw
done

# Convert all raw results to LaTeX tables
for f in ${OutputDir}/*.raw; do
  python3 scripts/latex_table.py "$f" > "${f%.raw}.tex"
  echo "Converted $f to ${f%.raw}.tex"
done
