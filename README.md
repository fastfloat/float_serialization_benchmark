# float_serialization_benchmark

This repository contains benchmarking code for 


If you have a recent version of CMake (3.15 or better) under linux,  you can simply
go in the directory and type the following commands:

```
cmake -B build .
cmake --build build
./build/benchmarks/benchmark 
```



You may use docker to run these benchmarks easily on a variety of platforms: see https://github.com/lemire/docker_programming_station


## Windows

Usage under Windows is similar. After installing cmake and Visual Studio 2019, one might type in the appropriate shell:

```
cmake -B build .
cmake --build build --config Release
.\build\benchmarks\Release\benchmark.exe
```

## Advanced Usage

Serialize the strings (one per line) included in a text file:

```
./build/benchmarks/benchmark -f data/canada.txt
```

Serialize strings generated from floats in (0,1):


```
./build/benchmarks/benchmark
```

## References

- [dtoa Benchmark](https://github.com/miloyip/dtoa-benchmark)
- [Benchmark different approaches to parsing scientific datafiles](https://github.com/alugowski/parse-bench)