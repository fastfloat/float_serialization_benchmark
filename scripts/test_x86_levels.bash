#!/bin/bash

CPU=$1
OutputDir="outputs/${CPU}"
Algorithms="schubfach,dragonbox"

if [ -z "$1" ]; then
  echo "Usage: $0 <CPU>"
  exit 1
fi

mkdir -p ${OutputDir}
for v in x86-64 x86-64-v2 x86-64-v3 x86-64-v4 native; do
  cmake -B build-${v} -DSIMPLE_FAST_FLOAT_BENCHMARK_MARCH=${v}
  cmake --build build-${v}

  echo "Running benchmarks for ${v} on ${CPU}..."
  ./build-${v}/benchmarks/benchmark -f data/canada.txt -a ${Algorithms} > ${OutputDir}/${CPU}_g++_canada_none_${v}.raw
  ./build-${v}/benchmarks/benchmark -f data/mesh.txt -a ${Algorithms} > ${OutputDir}/${CPU}_g++_mesh_none_${v}.raw
  ./build-${v}/benchmarks/benchmark -a ${Algorithms} > ${OutputDir}/${CPU}_g++_uniform_01_none_${v}.raw
done

for f in ${OutputDir}/*.raw; do
  python3 scripts/latex_table.py "$f" > "${f%.raw}.tex"
  echo "Converted $f to ${f%.raw}.tex"
done
